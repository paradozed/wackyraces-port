#include "race_weapons.h"
#include <algorithm>
#include <cmath>

namespace wpc {
namespace {
// The types WackyAutoBomba's update knows; others end at once (0x47e264).
bool held(uint16_t t) {
    switch(t) {case 2:case 4:case 8:case 0x10:case 0x40:case 0x80:case 0x100:case 0x200:case 0x400: return true;}
    return false;
}
}
bool step_tint(Tint& t,const std::array<float,3>& spread,int n) {
    const float limit=0.007843138f; // 0x51300c
    const auto small=[&](float k){return k*spread[0]/13<limit && k*spread[1]/13<limit && k*spread[2]/13<limit;};
    switch(t.mode) {
        case 0: {const bool done=small(1-t.f);t.f+=(1-t.f)*0.07692308f;if(done) t.mode=1;return false;}
        case 1: t.f=1;t.hold=0;t.mode=2;return false;
        case 2: if(++t.hold>n) t.mode=3;return false;
        case 3: {const bool done=small(t.f);t.f-=t.f*0.07692308f;if(done) t.mode=4;return false;}
        case 4: t.f=0;t.mode=5;return false;
        default: return true;
    }
}
namespace {
Tint tint_to(int r,int g,int b,float f=0) {Tint t;t.target={{r/255.f,g/255.f,b/255.f}};t.f=f;return t;}
std::array<float,3> spread_of(const BombHit& h) {
    std::array<float,3> s{};
    for(size_t i=0;i<3;++i) s[i]=std::max(std::abs(h.tint.target[i]-h.colour_lo[i]),std::abs(h.tint.target[i]-h.colour_hi[i]));
    return s;
}
}
BombStep update_bomb(BombHit& h) {
    BombStep s;
    if(!h.active()) return s;
    // 0x47e234: the update after +0x36 is set ends the action (0x41ba90 ->
    // vtable +0x14 0x47f690 clears +0x2c) and does nothing else.
    if(h.done) {const BombHit kept=h;h=BombHit{};h.guard=kept.guard;h.colour_lo=kept.colour_lo;h.colour_hi=kept.colour_hi;return s;}
    const int c=h.counter;
    switch(h.type) {
        case 8:case 0x40:case 0x80:
            if(c==0) h.tint=tint_to(10,10,0);
            h.done=step_tint(h.tint,spread_of(h)) && c>=bomb_hold_updates;
            break;
        case 4:
            if(c==0) h.tint=tint_to(20,20,255);
            h.done=step_tint(h.tint,spread_of(h)) && c>=78; // stage 5 at 78
            break;
        case 2:
            if(c==42) h.tint=tint_to(100,100,0,1); // stage 4 writes the colour
            if(c>=50) h.done=step_tint(h.tint,spread_of(h)); // stage 8
            break;
        case 0x400:
            // Stage 0 writes (10, 10, 0), stage 2 (at 27) the saved colours.
            h.tint=tint_to(10,10,0,c<27?1.f:0.f);
            h.done=c>=27;
            break;
        case 0x200:
            // 0x47eea0: +0x66 0.1 toward 1, a spring past +0x28 = 20.
            if(c==0) {h.scale_y=0.1f;h.squash_v=0.2f;h.sub=0;}
            if(h.sub<=23) {
                if(h.sub>20) {
                    const float v=(1-h.scale_y)/((66-float(h.sub))*0.05f+0.3f);
                    h.squash_v=(v+h.squash_v)*0.87f;h.scale_y+=h.squash_v;
                }
                ++h.sub;
            } else h.scale_y=1;
            h.done=c>=25;
            break;
        case 0x10:case 0x100: h.done=c>=0x2e;break;
        default: h.done=true;break;
    }
    // 0x47ef88/0x47ed1c below 0x19, the +0x8db write at 0x19, then the
    // counter increments at 0x47f1e8.
    if(held(h.type)) {
        if(h.counter<bomb_hold_updates) s.hold=true;
        else if(h.counter==bomb_hold_updates) s.boost=true;
    }
    ++h.counter;
    return s;
}
bool start_bomb(BombHit& h,uint16_t type) {
    if(h.active()) return false;
    if(h.guard==8) return false;
    if(h.guard==2) {h.guard=type!=4?1:8;return false;}
    h.type=type;h.counter=0;h.done=false;h.tint=Tint{};h.scale_y=1;h.squash_v=0;h.sub=0;
    return true;
}
Shield start_shield(BombHit& h,int time) {
    Shield s;s.timer=time;h.guard=2;
    return s;
}
void update_shield(Shield& s,BombHit& h) {
    if(s.timer==0 || h.guard==1) s.state=std::max(s.state,1);
    switch(s.state) {
        case 0:
            if(s.timer>30 && h.guard==8) s.timer=30;
            if(s.timer==0) s.state=1;
            else --s.timer;
            break;
        case 1: h.guard=4;s.timer=6;s.state=2;break;
        case 2:
            if(s.timer<2) s.state=3; // the model hides
            else --s.timer;
            break;
        default: h.guard=1;s.done=true;break;
    }
}
namespace {
float length(const Vec3& v) {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
}
bool breath_weapon(int icon,Projectile& out) {
    Projectile p;
    switch(icon) {
        case 25: p.type=8;p.object=0x38c;break;
        case 14: p.type=8;p.object=0x39a;p.p.maneuver=20;p.radius=151;break;
        case 16: p.type=2;p.object=0x3a8;break;
        // WPU_Paintball's WPaintShot 0x396 (class 0x4991c0, not traced):
        // provisionally a WBreath with the paint hit type 4.
        case 13: p.type=4;p.object=0x396;break;
        case 15:
            p.type=0x10;p.object=0x3a1;
            p.p.maneuver=50;p.p.angle=2.1f;p.p.range=10000;p.p.travel=10000;
            break;
        default: return false;
    }
    out=p;
    return true;
}
void aim_projectile(Projectile& p,const Vec3& target) {
    Vec3 d{{target[0]-p.position[0],target[1]-p.position[1],target[2]-p.position[2]}};
    const float l=length(d);
    if(l<=0) return;
    for(size_t i=0;i<3;++i) p.velocity[i]=d[i]/l*p.p.speed;
}
int64_t target_distance(const Vec3& d,float heading,float angle,int range) {
    const float r=float(range);
    if(!(std::abs(d[0])<r && std::abs(d[1])<r && std::abs(d[2])<r)) return -1;
    const int64_t x=int64_t(d[0]),y=int64_t(d[1]),z=int64_t(d[2]);
    const int64_t d2=x*x+y*y+z*z;
    if(d2>int64_t(range)*range) return -1;
    float a=std::atan2(-d[0],d[2])-heading;
    a=std::fmod(a,6.2831855f);
    if(a>3.1415927f) a-=6.2831855f;
    else if(a<-3.1415927f) a+=6.2831855f;
    if(!(a<angle && -angle<a)) return -1;
    return d2;
}
void steer_projectile(Projectile& p,const Vec3* target,bool ground,float ground_y,float reach) {
    bool close=false;
    if(target && p.p.maneuver!=0) {
        Vec3 d{{(*target)[0]-p.position[0],(*target)[1]-p.position[1],(*target)[2]-p.position[2]}};
        const float l=length(d);
        close=l<p.p.aim;
        Vec3 steer{};
        if(l>0) for(size_t i=0;i<3;++i) steer[i]=d[i]/l*p.p.speed-p.velocity[i];
        if(!close) steer[1]=0;
        const float sl=length(steer);
        if(sl>0) for(size_t i=0;i<3;++i) p.velocity[i]+=steer[i]/sl*p.p.maneuver;
    }
    if(!close) {
        // Reach H above the ground over the updates it takes to get there
        // (2 H below and 1 update when no ground is found): lowering scaled
        // by loweringspeed / H, climbing at most climb.
        const float floor=ground?ground_y:p.position[1]-2*p.p.height;
        const float t=ground?std::max(1.f,reach):1.f;
        const float e=floor+p.p.height-p.position[1];
        p.velocity[1]=e<0?std::max(e*p.p.lowering/p.p.height/t,-p.p.lowering):std::min(e/t,p.p.climb);
    }
    // 0x475507: the vertical speed is set aside while the horizontal one is
    // capped at ABSspd, then restored.
    const float h=std::sqrt(p.velocity[0]*p.velocity[0]+p.velocity[2]*p.velocity[2]);
    if(h>p.p.speed) {p.velocity[0]*=p.p.speed/h;p.velocity[2]*=p.p.speed/h;}
}
Tnt throw_tnt(const Vec3& position,const Vec3& forward,int owner,const Vec3* target) {
    Tnt t;t.position=position;t.position[1]+=100;t.owner=owner;
    if(target) {
        Vec3 d{{(*target)[0]-t.position[0],(*target)[1]-t.position[1],(*target)[2]-t.position[2]}};
        const float l=length(d);
        for(size_t i=0;i<3;++i) t.velocity[i]=l>0?d[i]/l*tnt_forward:forward[i]*tnt_forward;
        t.velocity[1]+=tnt_rise;
    } else {
        for(size_t i=0;i<3;++i) t.velocity[i]=forward[i]*tnt_forward;
        t.velocity[1]+=tnt_rise;
    }
    return t;
}
bool bounce_tnt(Tnt& t,const Vec3& hit,const Vec3& n,const Vec3* target) {
    if(++t.bounces==3) return false;
    for(size_t i=0;i<3;++i) t.position[i]=hit[i]+n[i]*10;
    t.speed*=tnt_reducer;
    if(target) {
        Vec3 d{{(*target)[0]-t.position[0],(*target)[1]-t.position[1],(*target)[2]-t.position[2]}};
        const float l=length(d);
        if(l>0) for(size_t i=0;i<3;++i) t.velocity[i]=d[i]/l*t.speed;
        t.velocity[1]+=tnt_rise;
    } else {
        const float dn=t.velocity[0]*n[0]+t.velocity[1]*n[1]+t.velocity[2]*n[2];
        for(size_t i=0;i<3;++i) t.velocity[i]-=2*dn*n[i];
        t.velocity[0]*=tnt_reducer;t.velocity[2]*=tnt_reducer;
    }
    return true;
}
BombKick bomb_kick(uint16_t type) {
    BombKick k;
    k.hop=(type&0xda)!=0;
    if(type&0x3fc) k.spin=12.566371f;
    if(type&0x1000) k.spin=-3.1415927f;
    if(type&0x800) k.spin=3.1415927f;
    return k;
}
void request_spin(Spin& s,float v) {
    const float total=s.left+s.done;
    if((v<0 && total>v) || (v>0 && total<v)) s.left=v-s.done;
}
void decay_spin(Spin& s) {
    if(s.left==0) {s=Spin{};return;} // 0x472680
    s.step=std::abs(s.left)<0.05235988f?s.left:s.left*0.11111111f;
    s.left-=s.step;s.done+=s.step;
}
Landmine drop_detour(const Vec3& position,float yaw,int owner,bool left) {
    Landmine m;
    m.position=position;
    m.position[0]+=std::sin(yaw)*270;m.position[2]-=std::cos(yaw)*270;
    m.yaw=yaw;m.owner=owner;m.life=m.lifetime=150;m.gravity=100;m.trap=150;
    m.type=left?0x1000:0x800;m.detour=true;
    return m;
}
Barrier drop_barrier(const Vec3& position,float yaw,int owner,bool tree) {
    Barrier b;
    b.position=position;
    b.position[0]-=std::sin(yaw)*2000;b.position[2]+=std::cos(yaw)*2000;b.position[1]+=1000;
    b.yaw=yaw;b.owner=owner;b.tree=tree;b.scale_num=tree?5:3;
    return b;
}
Landmine drop_oil(const Vec3& position,float yaw,int owner) {
    Landmine m;
    m.position=position;m.position[1]+=10;
    m.yaw=yaw;m.owner=owner;m.oil=true;
    return m;
}
Landmine drop_hive(const Vec3& position,float yaw,int owner) {
    Landmine m=drop_oil(position,yaw,owner);
    m.oil=false;m.hive=true;
    return m;
}
Landmine drop_landmine(const Vec3& position,float yaw,int owner) {
    Landmine m;
    m.position=position;m.position[1]+=10;
    m.yaw=yaw;m.owner=owner;
    return m;
}
}
