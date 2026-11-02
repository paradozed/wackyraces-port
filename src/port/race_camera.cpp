#include "race_camera.h"
#include <cmath>
#include <cstdint>

namespace wpc {
namespace {
using V=std::array<float,3>;
V add(V a,const V& b) {for(size_t i=0;i<3;++i) a[i]+=b[i];return a;}
V sub(V a,const V& b) {for(size_t i=0;i<3;++i) a[i]-=b[i];return a;}
V mul(V a,float k) {for(float& f:a) f*=k;return a;}
float length(const V& a) {return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
// 0x452e20 divides by the length without a zero test; a zero vector keeps
// its value here.
V unit(const V& a) {const float l=length(a);return l>0?mul(a,1/l):a;}
// 0x452ed0: out = a x b.
V cross(const V& a,const V& b) {return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
V axis(const Matrix4& m,size_t c) {return {{m[c*4],m[c*4+1],m[c*4+2]}};}
// 0x43b880: a point of the frame; 0x43b980: a direction.
V point(const Matrix4& m,const V& p) {return add(add(add(axis(m,3),mul(axis(m,0),p[0])),mul(axis(m,1),p[1])),mul(axis(m,2),p[2]));}
V rotate(const Matrix4& m,const V& p) {return add(add(mul(axis(m,0),p[0]),mul(axis(m,1),p[1])),mul(axis(m,2),p[2]));}
// __ftol truncates; the distance tests then shift right by 4.
int32_t coarse(float f) {return int32_t(f)>>4;}
int32_t coarse_square(const V& d) {int32_t s=0;for(float f:d) {const int32_t c=coarse(f);s+=c*c;}return s;}
Matrix4 basis(const V& x,const V& y,const V& z,const V& p) {
    Matrix4 out=identity_matrix();
    for(size_t r=0;r<3;++r) {out[r]=x[r];out[4+r]=y[r];out[8+r]=z[r];out[12+r]=p[r];}
    return out;
}
}
FollowCameraMembers follow_camera_members(const Node& action) {
    FollowCameraMembers m;
    m.speed_rot_z=action.real(0x14a,m.speed_rot_z);m.near_radius=action.real(0x14d,m.near_radius);m.inertia=action.real(0x14e,m.inertia);
    if(!std::isfinite(m.speed_rot_z) || !std::isfinite(m.near_radius) || !std::isfinite(m.inertia) ||
       m.speed_rot_z<0 || m.speed_rot_z>1 || m.near_radius<0 || m.near_radius>100000 || std::abs(m.inertia)>10)
        throw std::runtime_error("implausible WFollowCamera_2 members");
    return m;
}
FollowCameraMembers race_camera_members(const Node& root) {
    const auto find=[](const auto& self,const Node& n)->const Node* {
        for(const auto& a:n.actions) if(a.class_name=="WFollowCamera_2") return &a;
        for(const auto& c:n.children) if(const Node* f=self(self,c)) return f;
        return nullptr;
    };
    const Node* action=find(find,root);
    if(!action) throw std::runtime_error("general scene has no WFollowCamera_2");
    return follow_camera_members(*action);
}
void FollowCamera::select(int view) {
    offset=offset_[size_t(view)];target_y=look_[size_t(view)][0];target_z=look_[size_t(view)][1];
}
void FollowCamera::init(const FollowCameraMembers& members,bool two,const Matrix4& frame) {
    m_=members;
    // 0x477ea0's writes (the look heights are halved, 0x51026c).
    offset_[0]={{0,two?100.f:300.f,two?-800.f:-550.f}};look_[0]={{(two?150.f:110.f)*0.5f,200}};
    offset_[1]={{0,two?350.f:400.f,-900}};look_[1]={{(two?150.f:300.f)*0.5f,0}};
    offset_[2]={{0,100,100}};look_[2]={{100*0.5f,200}};
    dist_speed_=0.2f;s_=0.6f;flags_=0;near_updates_=0;
    pose(frame);
}
// 0x477b60: view _2 behind the frame, no motion, and the camera's matrix.
void FollowCamera::pose(const Matrix4& frame) {
    prev=axis(frame,3);
    select(1);flags_|=1;y=target_y;z=target_z;
    near_updates_=0;cooldown_=0;
    eye=point(frame,offset);vel={};
    V up_axis=axis(frame,1);up_axis[1]+=1;
    const V target=add(add(prev,mul(up_axis,y)),mul(axis(frame,2),z));
    const V dir=unit(sub(target,eye));
    up=axis(frame,1);
    const V right=unit(cross(up,dir));
    matrix_=basis(right,unit(cross(dir,right)),dir,eye);
}
void FollowCamera::carry(const Matrix4& m) {
    eye=point(m,eye);prev=point(m,prev);up=rotate(m,up);vel=rotate(m,vel);
    matrix_=multiply_matrix(m,matrix_);
}
void FollowCamera::update(const Matrix4& frame,const FollowCameraState& st,const CameraCast& cast) {
    const V old_eye=eye;
    if(st.reset) pose(frame);
    // The rear view while 0x52aad4's bit is held; 0x52aad0 toggles the two
    // views with a 3-update wait (0x478109..0x4781e2). The 0x6e457f gate is
    // not identified (taken as 0).
    if(st.finished) {
        // 0x4781e4: a car the player no longer drives keeps view _2; the
        // test compares the whole flags with 1, so it rewrites the offset
        // every update unless they are exactly 1.
        flags_&=~0x10u;
        if(flags_!=1) {flags_|=1;offset=offset_[1];target_y=look_[1][0];target_z=look_[1][1];}
    } else {
        if(st.look_back) flags_|=0x10; else flags_&=~0x10u;
        if(cooldown_==0) {
            if(st.view) {
                if(flags_&1) {flags_&=~1u;select(0);} else {flags_|=1;select(1);}
                cooldown_=3;
            }
        } else --cooldown_;
    }
    const V pos=axis(frame,3),right_axis=axis(frame,0);
    if(!(flags_&0x100)) {
        near_updates_=0;
        // 0x478233: the eye within m_rNearRadius of the car's last position.
        const int32_t r=coarse(m_.near_radius);
        if(coarse_square(sub(eye,prev))<r*r && !st.near_locked) {
            flags_|=0x100;
            offset=offset_[2];offset[2]-=200;target_y=look_[2][0];target_z=look_[2][1];
        }
    } else {
        // 0x478396: two rays beside the line from near the car to the view's
        // offset (+200); both clear, or one hitting beyond the radius, ends
        // the near mode after 5 updates.
        ++near_updates_;
        const V wanted=point(frame,offset_[(flags_&1)?1:0]);
        const V mid=add(pos,mul(sub(wanted,pos),0.1f));
        const V far=add(wanted,mul(unit(sub(wanted,mid)),200));
        V side=mul(right_axis,40);
        int count=0;
        for(int i=0;i<2;++i) {
            if(i) side=mul(side,-1);
            const V a=add(mid,side),b=add(far,mul(side,2.7f));
            const CameraHit h=cast(a,b);
            const int32_t rr=coarse(m_.near_radius);
            if(!h.hit) count+=1;
            else if(coarse_square(sub(h.point,a))>rr*rr+0x30d4) count+=10;
        }
        // 0x4787a0: not while the car stands still with the same yaw.
        if(count && st.speed<0.1f && st.yaw==last_yaw_) count=0;
        last_yaw_=st.yaw;
        if((count==2 || count>10) && near_updates_>5) {
            eye=add(prev,mul(unit(sub(far,mid)),m_.near_radius+1));
            vel={};flags_&=~0x100u;
            select((flags_&1)?1:0);
        }
    }
    // States 1/0xc (0x478a0a): the first 40 updates swing the offset round
    // the car from 1800 away and 600 up; past 40 the stored offset stays.
    if(st.intro>=0) {
        if(st.intro==0) intro_base=offset;
        if(st.intro>40) offset=intro_base;
        else {
            const float k=40.f-float(st.intro),a=float(st.intro)*0.15707962f+3.1415925f,r=k*20+1000;
            offset[0]=std::sin(a)*r+intro_base[0];
            offset[2]=(std::cos(a)+1)*r+intro_base[2];
            offset[1]=k*15+intro_base[1];
        }
    }
    // 0x478a6f: the eye follows the car's motion x m_rS and closes on the
    // offset point x m_rDistSpeed, less m_rInertia x its last motion.
    const V wanted=point(frame,offset);
    eye=add(add(eye,mul(sub(pos,prev),s_)),mul(sub(wanted,eye),dist_speed_));
    eye=sub(eye,mul(vel,m_.inertia));
    y+=(target_y-y)*dist_speed_;z+=(target_z-z)*dist_speed_;
    V up_axis=axis(frame,1);up_axis[1]+=1;
    const V dir=unit(sub(add(add(pos,mul(up_axis,y)),mul(axis(frame,2),z)),eye));
    if(!(flags_&0x100)) {
        // 0x478b2e: two rays from near the car past the eye, 40 to each side
        // (x 2.7 at the far end); a hit pulls the eye in front of it.
        const bool intro=st.intro>=0;
        const V mid=add(pos,mul(sub(eye,pos),0.1f));
        const V reach=mul(unit(sub(eye,mid)),200);
        V side=mul(right_axis,40);
        int hits=0;
        for(int i=0;i<2;++i) {
            if(i) side=mul(side,-1);
            const V a=add(mid,side),b=add(add(eye,reach),mul(side,2.7f));
            const CameraHit h=cast(a,b);
            if(!h.hit || intro) continue;
            const float frac=length(sub(h.point,a))/length(sub(b,a)),k=frac*1.7f+1;
            const V at=sub(sub(h.point,reach),mul(side,k));
            if(hits==0) {eye=at;y*=frac;z*=frac;vel={};}
            else eye=mul(add(eye,at),0.5f);
            ++hits;
        }
        // 0x478fa0: a ceiling within 100 above holds the eye 100 below it.
        if(!intro) {
            const CameraHit h=cast({{eye[0],eye[1]-100,eye[2]}},{{eye[0],eye[1]+100,eye[2]}});
            if(h.hit) eye[1]+=h.point[1]-(eye[1]+100);
        }
    }
    up=add(up,mul(sub(axis(frame,1),up),m_.speed_rot_z));
    const V right=unit(cross(up,dir)),true_up=unit(cross(dir,right));
    V place=eye,x=right,yv=true_up,zv=dir;
    if(flags_&0x10) {
        // 0x479542: mirrored through the car's up axis (local x and z negated).
        const Matrix4 inv=inverse_affine_matrix(frame);
        const auto flip=[](V v){v[0]=-v[0];v[2]=-v[2];return v;};
        x=rotate(frame,flip(rotate(inv,x)));yv=rotate(frame,flip(rotate(inv,yv)));zv=rotate(frame,flip(rotate(inv,zv)));
        place=point(frame,flip(point(inv,place)));
    }
    matrix_=basis(x,yv,zv,place);
    prev=pos;
    vel=sub(eye,old_eye);
}
}
