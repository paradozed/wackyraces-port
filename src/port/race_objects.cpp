#include "race_objects.h"
#include "game_mesh.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace wpc {
namespace {
constexpr float half_pi=1.5707964f,two_pi=6.2831855f; // 0x513328, 0x512618
constexpr float car_radius=100;                       // WackyAuto PRIM sphere
// 0x52d8cc: {lamp A, lamp B, updates} for the six phases.
struct Phase {uint8_t a,b;uint16_t updates;};
constexpr Phase phases[6]={{1,4,120},{1,2,30},{1,1,30},{4,1,120},{2,1,30},{1,1,30}};
Vec3 point_of(const Matrix4& m,const Vec3& p) {
    return {{m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]}};
}
float axis_length(const Matrix4& m) {return std::sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]);}
// The in-place rotations post-multiply the frame (they turn its local
// axes): 0x43b4a0 about y, 0x43b690 about z, 0x43b2b0 about x.
void turn(Matrix4& m,size_t a,size_t b,float angle) {
    if(angle==0) return;
    const float c=std::cos(angle),s=std::sin(angle);
    for(size_t r=0;r<3;++r) {
        const float u=m[a*4+r],v=m[b*4+r];
        m[a*4+r]=c*u+s*v;m[b*4+r]=c*v-s*u;
    }
}
void turn_y(Matrix4& m,float a) {turn(m,0,2,a);}  // x' = c x + s z, z' = c z - s x
void turn_z(Matrix4& m,float a) {turn(m,1,0,a);}  // y' = c y + s x, x' = c x - s y
void turn_x(Matrix4& m,float a) {turn(m,2,1,a);}  // z' = c z + s y, y' = c y - s z
// 0x452f20: yaw atan2(-x, z) and pitch atan2(y, |xz|).
void angles(const Vec3& v,float& pitch,float& yaw) {yaw=std::atan2(-v[0],v[2]);pitch=std::atan2(v[1],std::sqrt(v[0]*v[0]+v[2]*v[2]));}
// 0x482190 and 0x482310 (as in race_session).
bool spheres_touch(const Vec3& a,const Vec3& b,float radii) {
    for(size_t i=0;i<3;++i) if(std::abs(a[i]-b[i])>radii) return false;
    const int dx=int(a[0]-b[0]),dy=int(a[1]-b[1]),dz=int(a[2]-b[2]),r=int(radii);
    return dx*dx+dy*dy+dz*dz<=r*r;
}
bool sphere_cylinder(const Vec3& s,float rs,const Vec3& base,float rc,float h) {
    const float dx=s[0]-base[0],dz=s[2]-base[2],r=rs+rc;
    if(std::abs(dx)>r || std::abs(dz)>r) return false;
    if(base[1]>s[1]+rs || s[1]-rs>base[1]+h) return false;
    const int ix=int(dx),iz=int(dz),ir=int(r);
    return ix*ix+iz*iz<=ir*ir;
}
// 0x4c6f40: integer box then squared distance within r.
bool within(const Vec3& d,int r) {
    const int x=int(d[0]),y=int(d[1]),z=int(d[2]);
    return std::abs(x)<r && std::abs(y)<r && std::abs(z)<r && x*x+y*y+z*z<r*r;
}
std::vector<Vec3> path_points(const Node& path) {
    const auto found=path.properties.find(0x7c); // GDPath::m_Points
    if(found==path.properties.end() || found->second.size()<4) throw std::runtime_error("GDPath without points");
    const auto& b=found->second;uint32_t n;std::memcpy(&n,b.data(),4);
    if(n<2 || n>4096 || b.size()<4+size_t(n)*12) throw std::runtime_error("truncated GDPath points");
    std::vector<Vec3> out(n);
    for(uint32_t i=0;i<n;++i) std::memcpy(out[i].data(),b.data()+4+12*size_t(i),12);
    return out;
}
// 0x4a71e0 for the bricks' queries: the nearest car within range of a point
// (truncated coordinates) that is shown and not being hit; -1 without one.
int nearest_car(const std::vector<CarProbe>& cars,const Vec3& at,int range) {
    int best=-1;int64_t best_d=-1;
    const float r=float(range);
    for(size_t i=0;i<cars.size();++i) {
        if(cars[i].hidden || cars[i].busy) continue;
        const float dx=cars[i].position[0]-at[0],dy=cars[i].position[1]-at[1],dz=cars[i].position[2]-at[2];
        if(!(std::abs(dx)<r && std::abs(dy)<r && std::abs(dz)<r)) continue;
        const int64_t x=int64_t(dx),y=int64_t(dy),z=int64_t(dz),d=x*x+y*y+z*z;
        if(d>int64_t(range)*range) continue;
        if(best_d<0 || d<=best_d) {best_d=d;best=int(i);}
    }
    return best;
}
BlowUp blow_up_of(const Node& owner) {
    BlowUp b;
    for(const Node& c:owner.children) if(c.class_name=="GDModel") {
        BlowUp::Piece p;p.node=c.id;p.local=node_transform(c);p.home={{p.local[12],p.local[13],p.local[14]}};
        b.pieces.push_back(p);
    }
    return b;
}
Vec3 unit_of(const Vec3& v) {
    const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    return l>0?Vec3{{v[0]/l,v[1]/l,v[2]/l}}:v;
}
Vec3 cross_of(const Vec3& a,const Vec3& b) {return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
const Node* first_model(const Node& n) {for(const auto& c:n.children) if(c.class_name=="GDModel") return &c;return nullptr;}
}
uint8_t TramCrossing::lamp_a() const {return phase<6?phases[phase].a:1;}
uint8_t TramCrossing::lamp_b() const {return phase<6?phases[phase].b:1;}
Matrix4 TramCrossing::tram_frame() const {
    Matrix4 local=tram_local;
    for(size_t i=0;i<3;++i) local[12+i]=from[i]+dir[i]*progress; // 0x43bf90
    return multiply_matrix(path_frame,local);
}
void path_point(const std::vector<Vec3>& p,bool closed,float t,Vec3& at,Vec3& tangent) {
    const int n=int(p.size());
    int i,count;float f;
    if(!closed && t==1) {i=n-1;f=0;count=n;}
    else {
        count=n;const int segments=closed?n:n-1;
        const float s=t*float(segments);const int whole=int(s);   // __ftol
        i=((whole%segments)+segments)%segments;f=s-float(whole);
    }
    int j=i+1;if(j>=count) j=0;
    int k=j+1;if(k>=count) k=0;
    Vec3 d,e;
    for(size_t c=0;c<3;++c) {d[c]=p[size_t(j)][c]-p[size_t(i)][c];e[c]=p[size_t(k)][c]-p[size_t(j)][c];}
    if(!closed) {
        if(j==0) {for(size_t c=0;c<3;++c) {e[c]=d[c];d[c]=p[size_t(i)][c]-p[size_t(i-1)][c];}}
        else if(k==0) e=d;
    }
    for(size_t c=0;c<3;++c) {at[c]=p[size_t(i)][c]+f*d[c];tangent[c]=(1-f)*d[c]+f*e[c];}
}
Matrix4 PathMover::frame() const {
    Vec3 at,tangent;path_point(points,closed,t,at,tangent);
    Matrix4 m=original;
    m[12]=at[0];m[13]=at[1];m[14]=at[2];
    if(rotate) {
        // 0x4d1d6f: turn by the tangent's angles against the path's first
        // segment (+0xb4), less a quarter turn, then the spin +0xd1.
        float pt,yt,pr,yr;angles(tangent,pt,yt);angles(reference,pr,yr);
        turn_y(m,(yt-yr)-half_pi);turn_z(m,spin);turn_x(m,(pt-pr)-half_pi);
    }
    return m;
}
void BlowUp::start() {
    if(on) return;
    on=true;left=200;
    for(Piece& p:pieces) {
        p.local=identity_matrix();p.local[12]=p.home[0];p.local[13]=p.home[1];p.local[14]=p.home[2];
        const float k=0.07692308f; // 0x513010
        p.velocity={{p.home[0]*k,std::max(p.home[1]*k+20,20.f),p.home[2]*k}};
    }
}
void BlowUp::step() {
    if(!on || left<=0) return;
    for(Piece& p:pieces) {
        for(size_t i=0;i<3;++i) {p.local[i]*=0.96f;p.local[4+i]*=0.96f;p.local[8+i]*=0.96f;}
        for(size_t i=0;i<3;++i) p.local[12+i]+=p.velocity[i];
        turn_x(p.local,0.08726646f);
        p.velocity[1]-=2.78f;
    }
    --left;
}
void BlowUp::finish() {
    if(!on) return;
    on=false;
    for(Piece& p:pieces) {p.local=identity_matrix();p.local[12]=p.home[0];p.local[13]=p.home[1];p.local[14]=p.home[2];}
}
bool path_frame(const std::vector<Vec3>& pts,const std::vector<Vec3>& dirs,const std::vector<float>& lens,float chord,float& d,int& seg,uint32_t flags,Matrix4& out) {
    const int n=int(pts.size());
    if(d<0) {
        while(seg>0 && d<0) {--seg;d+=lens[size_t(seg)];}
        if(seg==0 && d<0) return true;
    } else {
        while(seg<n-1 && d>lens[size_t(seg)]) {d-=lens[size_t(seg)];++seg;}
        if(seg>=n-1) return true;
    }
    const Vec3 a=pts[size_t(seg)],da=dirs[size_t(seg)];
    const Vec3 p0{{a[0]+da[0]*d,a[1]+da[1]*d,a[2]+da[2]*d}};
    float e=d+chord;int i=seg;
    while(i<n-1 && e>lens[size_t(i)]) {e-=lens[size_t(i)];++i;}
    if(i>=n-1) return true;
    const Vec3 b=pts[size_t(i)],db=dirs[size_t(i)];
    const Vec3 p1{{b[0]+db[0]*e,b[1]+db[1]*e,b[2]+db[2]*e}};
    const int along=int(flags&3),named=int(flags>>2&3);
    int next=along+1;if(next>3) next=1;
    const Vec3 dir=unit_of(flags&0x10?Vec3{{pts[size_t(n-1)][0]-pts[0][0],pts[size_t(n-1)][1]-pts[0][1],pts[size_t(n-1)][2]-pts[0][2]}}
                                     :Vec3{{p1[0]-p0[0],p1[1]-p0[1],p1[2]-p0[2]}});
    const Vec3 u=unit_of(cross_of(dir,{{-dir[2],dir[1],dir[0]}}));
    const Vec3 v=named==next?cross_of(dir,u):cross_of(u,dir);
    Vec3 axes[3];
    for(int k=1;k<=3;++k) axes[k-1]=k==along?dir:(k==named?u:v);
    out=identity_matrix();
    for(size_t k=0;k<3;++k) {out[k]=axes[0][k];out[4+k]=axes[1][k];out[8+k]=axes[2][k];out[12+k]=(p0[k]+p1[k])*0.5f;}
    return false;
}
Matrix4 RollingRock::world() const {return multiply_matrix(parent,frame);}
namespace {
// 0x4c4f60 (index 0) with the frame of 0x4c4ea0.
void rock_frame(RollingRock& r) {
    Matrix4 f;
    if(path_frame(r.points,r.directions,r.lengths,r.chord,r.distance,r.segment,r.flags,f)) {r.state=0;return;}
    turn_x(f,r.spin);
    r.frame=f;
}
void rock_start(RollingRock& r,int state) {
    r.state=state;r.segment=0;r.spin=0;r.velocity=0;r.distance=0;
    rock_frame(r);
}
}
bool path_distance_point(const std::vector<Vec3>& points,float d,Vec3& at) {
    if(points.size()<2 || d<0) return false;
    for(size_t i=0;i+1<points.size();++i) {
        Vec3 s;for(size_t k=0;k<3;++k) s[k]=points[i+1][k]-points[i][k];
        const float l=std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]);
        if(d<=l && l>0) {for(size_t k=0;k<3;++k) at[k]=points[i][k]+s[k]*d/l;return true;}
        d-=l;
    }
    return false;
}
void CourseObjects::load_log_meshes(const Archive& course,const Scene& scene) {
    for(MistyBridge& b:bridges_) for(size_t i=0;i<2;++i) {
        const Node& n=scene.find(b.halves[i]);
        const uint32_t id=n.link(0x67,"GMDL");
        const Mesh mesh=load_mesh(course,"GMDL",id);
        const Matrix4 m=affine_matrix(mesh.matrix);
        for(const auto& t:mesh.triangles) {
            std::array<Vec3,3> tri;
            const uint32_t k[3]={t.a,t.b,t.c};
            for(int j=0;j<3;++j) {const auto& v=mesh.vertices.at(k[j]);tri[size_t(j)]=point_of(m,{{v.x,v.y,v.z}});}
            b.triangles[i].push_back(tri);
        }
    }
    for(LogRide& r:log_rides_) {
        r.triangles.assign(r.logs.size(),{});
        for(size_t i=0;i<r.logs.size();++i) {
            const Node& n=scene.find(r.logs[i]);
            const auto link=n.properties.find(0x67);
            if(link==n.properties.end() || link->second.size()!=8 || std::memcmp(link->second.data(),"GMDL",4)) continue;
            uint32_t id;std::memcpy(&id,link->second.data()+4,4);
            const Mesh mesh=load_mesh(course,"GMDL",id);
            const Matrix4 m=affine_matrix(mesh.matrix);
            for(const auto& t:mesh.triangles) {
                std::array<Vec3,3> tri;
                const uint32_t k[3]={t.a,t.b,t.c};
                for(int j=0;j<3;++j) {const auto& v=mesh.vertices.at(k[j]);tri[size_t(j)]=point_of(m,{{v.x,v.y,v.z}});}
                r.triangles[i].push_back(tri);
            }
        }
    }
}
namespace {
// The highest triangle of a moving model at frame f on the vertical at (x, z).
void surface_hit(const std::vector<std::array<Vec3,3>>& tris,const Matrix4& f,float x,float z,float top,float bottom,LogHit& best,int ride,int log,int bridge) {
    const Matrix4 inv=inverse_affine_matrix(f);
    const Vec3 a=point_of(inv,{{x,top,z}}),b=point_of(inv,{{x,bottom,z}});
    const Vec3 d{{b[0]-a[0],b[1]-a[1],b[2]-a[2]}};
    for(const auto& t:tris) {
        // Moller-Trumbore on the segment a..b.
        const Vec3 e1{{t[1][0]-t[0][0],t[1][1]-t[0][1],t[1][2]-t[0][2]}},e2{{t[2][0]-t[0][0],t[2][1]-t[0][1],t[2][2]-t[0][2]}};
        const Vec3 p{{d[1]*e2[2]-d[2]*e2[1],d[2]*e2[0]-d[0]*e2[2],d[0]*e2[1]-d[1]*e2[0]}};
        const float det=e1[0]*p[0]+e1[1]*p[1]+e1[2]*p[2];
        if(std::abs(det)<1e-9f) continue;
        const Vec3 s{{a[0]-t[0][0],a[1]-t[0][1],a[2]-t[0][2]}};
        const float u=(s[0]*p[0]+s[1]*p[1]+s[2]*p[2])/det;
        if(u<0 || u>1) continue;
        const Vec3 q{{s[1]*e1[2]-s[2]*e1[1],s[2]*e1[0]-s[0]*e1[2],s[0]*e1[1]-s[1]*e1[0]}};
        const float v=(d[0]*q[0]+d[1]*q[1]+d[2]*q[2])/det;
        if(v<0 || u+v>1) continue;
        const float tt=(e2[0]*q[0]+e2[1]*q[1]+e2[2]*q[2])/det;
        if(tt<0 || tt>1) continue;
        const float y=top+(bottom-top)*tt;
        if(best.hit && y<=best.y) continue;
        Vec3 n{{e1[1]*e2[2]-e1[2]*e2[1],e1[2]*e2[0]-e1[0]*e2[2],e1[0]*e2[1]-e1[1]*e2[0]}};
        Vec3 w{{f[0]*n[0]+f[4]*n[1]+f[8]*n[2],f[1]*n[0]+f[5]*n[1]+f[9]*n[2],f[2]*n[0]+f[6]*n[1]+f[10]*n[2]}};
        const float l=std::sqrt(w[0]*w[0]+w[1]*w[1]+w[2]*w[2]);
        if(!(l>0)) continue;
        for(float& c:w) c/=l;
        if(w[1]<0) for(float& c:w) c=-c;
        best.hit=true;best.y=y;best.normal=w;best.ride=ride;best.log=log;best.bridge=bridge;
    }
}
}
LogHit CourseObjects::log_ground(float x,float z,float top,float bottom) const {
    LogHit best;
    for(size_t ri=0;ri<log_rides_.size();++ri) {
        const LogRide& r=log_rides_[ri];
        if(!r.active) continue;
        for(size_t i=0;i<r.triangles.size();++i) {
            const Matrix4& f=r.frames[i];
            // Cheap reject: within the log's extent of its centre.
            const float dx=x-f[12],dz=z-f[14],reach=r.length+r.width;
            if(dx*dx+dz*dz>reach*reach) continue;
            surface_hit(r.triangles[i],f,x,z,top,bottom,best,int(ri),int(i),-1);
        }
    }
    for(size_t k=0;k<bridges_.size();++k) {
        const MistyBridge& m=bridges_[k];
        for(size_t i=0;i<2;++i) {
            const Matrix4& f=m.frames[i];
            const float dx=x-f[12],dz=z-f[14];
            if(dx*dx+dz*dz>9e6f) continue; // the halves reach about 1400 from their pivots
            surface_hit(m.triangles[i],f,x,z,top,bottom,best,-1,-1,int(k));
        }
    }
    return best;
}
Vec3 CourseObjects::log_motion(int ride,int log) const {
    if(ride<0 || size_t(ride)>=log_rides_.size()) return {};
    const auto& m=log_rides_[size_t(ride)].moved;
    return log>=0 && size_t(log)<m.size()?m[size_t(log)]:Vec3{};
}
Matrix4 Geyser::column_frame() const {
    // WGeyserModel's three values scale x, y and z, turning (0x4c9126:
    // 1 degree x (i + 1) per update; provisional: only y is turned here).
    Matrix4 m=column_local;
    turn_y(m,spin);
    for(size_t k=0;k<3;++k) {m[k]*=rise[0];m[4+k]*=rise[1];m[8+k]*=rise[2];}
    for(size_t k=0;k<3;++k) m[12+k]+=aim[k];
    return m;
}
Matrix4 Geyser::ring_frame() const {
    Matrix4 m=identity_matrix();
    m[0]=m[5]=m[10]=ring_scale;
    for(size_t k=0;k<3;++k) m[12+k]=aim[k];
    return m;
}
void CourseObjects::load(const Scene& scene) {
    crossings_.clear();movers_.clear();inside_.clear();log_rides_.clear();bridges_.clear();geysers_.clear();rolling_logs_.clear();holes_.clear();chisels_.clear();rocks_.clear();
    const Node& root=scene.root();
    const auto visit=[&](const auto& self,const Node& n)->void {
        if(n.class_name=="WFountain") {
            Geyser g;g.node=n.id;g.fountain=true;
            const float alpha=n.real(0x238),omega=n.real(0x239);
            g.delay=n.integer(0x23a);g.chance=n.integer(0x23b);g.frames=n.integer(0x23c);
            g.height=n.integer(0x244);g.width=n.integer(0x245);g.sugar=n.integer(0x243);g.max_fly=100;
            if(!(alpha>0 && alpha<1) || !std::isfinite(omega) || g.frames<1 || g.height<=0 || g.width<=0 || n.children.empty())
                throw std::runtime_error("implausible WFountain members");
            const float c0=2*alpha*std::cos(omega),c1=-alpha*alpha,k=(1-c0-c1)*0.28985509f;
            g.filter={{c0,c1,k,k*1.8f,k*0.65f}};
            g.column=n.children[0].id;g.column_local=node_transform(n.children[0]);
            const Matrix4 w=unwrapped_scene_transform(root,n.id);
            g.home=g.aim={{w[12],w[13],w[14]}};g.state=1;
            geysers_.push_back(std::move(g));
        }
        if(n.class_name=="WChiselCrossover") {
            if(n.children.empty()) throw std::runtime_error("WChiselCrossover without its WLogBase");
            const Node& log=n.children[0];
            ChiselCrossover c;c.node=n.id;c.log=log.id;c.route_node=n.integer(0x26f);
            const Matrix4 w=unwrapped_scene_transform(root,n.id);c.centre={{w[12],w[13],w[14]}};
            c.log_world=unwrapped_scene_transform(root,log.id);
            for(const ScenePrim& p:node_prims(log)) if(p.sensor_group&0x10000) {c.solid_centre=point_of(c.log_world,{{p.centre[0],p.centre[1],p.centre[2]}});c.solid_radius=p.radius*axis_length(c.log_world);break;}
            c.blow=blow_up_of(log);
            chisels_.push_back(std::move(c));
        }
        if(n.class_name=="WRolling") {
            const Node* rock=nullptr;const Node* path=nullptr;
            for(const Node& c:n.children) {if(c.class_name=="WLogModelQ" && !rock) rock=&c;if(c.class_name=="GDPathQ" && !path) path=&c;}
            if(!rock || !path) throw std::runtime_error("WRolling without its boulder and path");
            RollingRock r;r.node=n.id;r.rock=rock->id;r.parent=unwrapped_scene_transform(root,n.id);
            r.centre={{r.parent[12],r.parent[13],r.parent[14]}};
            r.chord=n.real(0x22e);r.radius=n.real(0x22f);r.gravity=n.real(0x230);r.drag=n.real(0x231);
            r.range=n.integer(0x234);r.loop=n.integer(0x235)==1;
            if(!std::isfinite(r.chord) || !(r.radius>0) || !std::isfinite(r.gravity) || !std::isfinite(r.drag) || r.range<0) throw std::runtime_error("implausible WRolling members");
            const Matrix4 pf=node_transform(*path);
            r.points=path_points(*path);
            for(Vec3& p:r.points) for(size_t k=0;k<3;++k) p[k]+=pf[12+k];   // 0x4be822: the path's translation only
            for(size_t i=0;i+1<r.points.size();++i) {
                const Vec3 s{{r.points[i+1][0]-r.points[i][0],r.points[i+1][1]-r.points[i][1],r.points[i+1][2]-r.points[i][2]}};
                r.lengths.push_back(std::sqrt(s[0]*s[0]+s[1]*s[1]+s[2]*s[2]));
                r.directions.push_back(unit_of(s));
            }
            r.flags=r.loop?0xb:0x1b;  // 0x4c76b5, 0x4c76cf
            r.prims=node_prims(*rock);
            r.blow=blow_up_of(*rock);
            rock_start(r,1);
            rocks_.push_back(std::move(r));
        }
        if(n.class_name=="WDynamiteHole") {
            DynamiteHole d;d.node=n.id;d.radius=float(n.integer(0x25a));d.vel=n.real(0x25b);
            if(n.children.size()<2 || !(d.radius>0) || !std::isfinite(d.vel)) throw std::runtime_error("implausible WDynamiteHole");
            d.lid=n.children[0].id;d.fuse=n.children[1].id;
            d.home=unwrapped_scene_transform(root,d.lid);
            const Matrix4 w=unwrapped_scene_transform(root,n.id);d.centre={{w[12],w[13],w[14]}};
            holes_.push_back(d);
        }
        if(n.class_name=="WLogRollingB") {
            RollingLog r;r.node=n.id;r.axis=n.integer(0x21a);r.rate=n.real(0x219);
            if(!std::isfinite(r.rate) || std::abs(r.rate)>1) throw std::runtime_error("implausible WLogRollingB members");
            const Matrix4 w=unwrapped_scene_transform(root,n.id);r.centre={{w[12],w[13],w[14]}};
            rolling_logs_.push_back(r);
        }
        if(n.class_name=="WGeyser") {
            Geyser g;g.node=n.id;
            const float alpha=n.real(0x238),omega=n.real(0x239);
            g.delay=n.integer(0x23a);g.chance=n.integer(0x23b);g.frames=n.integer(0x23c);
            g.sugar=n.integer(0x23d);g.height=n.integer(0x23e);g.steps=n.integer(0x23f);g.width=n.integer(0x240);
            g.distance=float(n.integer(0x241));g.max_fly=n.real(0x242);
            if(!(alpha>0 && alpha<1) || !std::isfinite(omega) || g.frames<1 || g.steps<1 || g.sugar<=0 || g.height<=0 || g.width<=0 || !std::isfinite(g.max_fly))
                throw std::runtime_error("implausible WGeyser members");
            const float c0=2*alpha*std::cos(omega),c1=-alpha*alpha,k=(1-c0-c1)*0.28985509f;
            g.filter={{c0,c1,k,k*1.8f,k*0.65f}};
            if(n.children.size()<2) throw std::runtime_error("WGeyser without its column and ring");
            g.column=n.children[0].id;g.ring=n.children[1].id;g.column_local=node_transform(n.children[0]);
            const Matrix4 w=unwrapped_scene_transform(root,n.id);
            g.home=g.aim={{w[12],w[13],w[14]}};
            g.sensors.push_back(g.home);
            // 0x4c8c47: the children from the third are further sensors.
            for(size_t i=2;i<n.children.size();++i) {const Matrix4 c=unwrapped_scene_transform(root,n.children[i].id);g.sensors.push_back({{c[12],c[13],c[14]}});}
            geysers_.push_back(std::move(g));
        }
        if(n.class_name=="WMisty") {
            MistyBridge b;b.node=n.id;
            b.axis=n.integer(0x222);b.max_angle=n.real(0x223);b.in_radius=float(n.integer(0x224));b.out_radius=float(n.integer(0x225));
            const float alpha=n.real(0x226),omega=n.real(0x227);
            if(b.axis<0 || b.axis>3 || !std::isfinite(b.max_angle) || !(b.in_radius>0) || !(b.out_radius>=b.in_radius) || !(alpha>0 && alpha<1) || !std::isfinite(omega))
                throw std::runtime_error("implausible WMisty members");
            // 0x440ab0(1, alpha, omega).
            const float c0=2*alpha*std::cos(omega),c1=-alpha*alpha,k=(1-c0-c1)*0.28985509f;
            b.filter={{c0,c1,k,k*1.8f,k*0.65f}};
            size_t models=0;
            for(const auto& c:n.children) {
                if(c.class_name=="GDModel" && models<2) {b.halves[models]=c.id;b.base[models]=b.frames[models]=unwrapped_scene_transform(root,c.id);++models;}
                else if(c.class_name=="GDStdBrick") {const Matrix4 w=unwrapped_scene_transform(root,c.id);b.sensors.push_back({{w[12],w[13],w[14]}});}
            }
            if(models!=2) throw std::runtime_error("WMisty without its two halves");
            bridges_.push_back(std::move(b));
        }
        if(n.class_name=="WLogRide") {
            LogRide r;r.node=n.id;
            r.length=n.real(0x24c,500);r.width=n.real(0x24d,188);r.dist=n.real(0x24e,5000);r.vel=n.real(0x24f,50);
            if(!(r.length>0) || !(r.dist>0) || !(r.vel>0) || r.length>100000 || r.dist>1000000 || r.vel>10000)
                throw std::runtime_error("implausible WLogRide members");
            const Node* path=nullptr;
            for(const auto& c:n.children) {
                if(c.class_name=="GDPathQ" && !path) path=&c;
                else if(c.class_name=="WLogBase") {r.logs.push_back(c.id);r.frames.push_back(unwrapped_scene_transform(root,c.id));}
            }
            if(!path || r.logs.empty()) throw std::runtime_error("WLogRide without its path or logs");
            const Matrix4 pw=unwrapped_scene_transform(root,path->id);
            for(const Vec3& p:path_points(*path)) r.points.push_back(point_of(pw,p));
            r.moved.assign(r.logs.size(),Vec3{});
            log_rides_.push_back(std::move(r));
        }
        if(n.class_name=="WTram") {
            // 0x4cad90: the GDPath child (+0x60), its points (+0x126) and
            // its first child, the tram (+0x123).
            const Node* path=nullptr;
            for(const auto& c:n.children) if(c.class_name=="GDPath") {path=&c;break;}
            const Node* tram=path?first_model(*path):nullptr;
            if(!path || !tram) throw std::runtime_error("WTram without its path or tram");
            TramCrossing c;c.node=n.id;c.tram=tram->id;
            const auto pts=path_points(*path);
            c.from=pts[0];
            for(size_t i=0;i<3;++i) c.dir[i]=pts[1][i]-pts[0][i];
            c.length=std::sqrt(c.dir[0]*c.dir[0]+c.dir[1]*c.dir[1]+c.dir[2]*c.dir[2]);
            if(!(c.length>0)) throw std::runtime_error("WTram path has no length");
            for(float& v:c.dir) v/=c.length;
            c.speed=n.real(0x247);c.radius=n.integer(0x248);   // m_Vel, m_Radius
            if(!(c.speed>0) || c.radius<=0 || c.radius>100000) throw std::runtime_error("implausible WTram members");
            c.path_frame=unwrapped_scene_transform(root,path->id);
            // Provisional: 0x4caf40 turns the copied frame by pi/2 about x
            // before writing it (0x43b2b0); the copy is taken as the tram's
            // serialized frame, which puts its spheres (z -125) upright.
            c.tram_local=node_transform(*tram);turn_x(c.tram_local,half_pi);
            const Matrix4 w=unwrapped_scene_transform(root,n.id);c.centre={{w[12],w[13],w[14]}};
            for(const ScenePrim& p:node_prims(*tram)) if(!p.cylinder) c.spheres.push_back(p);
            crossings_.push_back(c);
        }
        for(const auto& a:n.actions) if(a.class_name=="WElTram" || a.class_name=="WGoPath") {
            PathMover m;m.node=n.id;m.el_tram=a.class_name=="WElTram";
            const uint32_t path_id=uint32_t(a.integer(0x27f));          // m_idPath
            const Node& path=scene.find(path_id);
            if(path.class_name!="GDPath") throw std::runtime_error("WGoPath m_idPath is not a GDPath");
            const Matrix4 pw=unwrapped_scene_transform(root,path_id);
            for(const Vec3& p:path_points(path)) m.points.push_back(point_of(pw,p));
            m.closed=path.integer(0x7d)!=0;                               // m_bClosed
            const int steps=a.integer(0x278);                             // m_nStep
            if(steps<0 || steps>1000000) throw std::runtime_error("implausible WGoPath m_nStep");
            m.step=steps>0?1.f/float(steps):0;
            const float length=float(a.integer(0x279)),offset=float(a.integer(0x27a));
            m.t=m.start=offset;m.end=(1-length)+offset;                  // +0xc0, +0xc4
            m.loop=a.integer(0x27b)!=0;m.rotate=a.integer(0x27c)!=0;    // m_bLoop, m_bRotate
            m.spin_step=a.real(0x27d);m.collision=a.integer(0x27e)!=0;  // m_aZrotate, m_bCollision
            if(m.el_tram) m.waiting=a.integer(0x281);                     // m_Waiting
            for(size_t i=0;i<3;++i) m.reference[i]=m.points[1][i]-m.points[0][i];
            m.original=unwrapped_scene_transform(root,n.id);
            m.prims=node_prims(n);m.placed=m.frame();
            movers_.push_back(m);
        }
        for(const auto& c:n.children) self(self,c);
    };
    visit(visit,root);
}
void CourseObjects::link(const std::function<std::vector<int>(uint32_t)>& sections_of) {
    for(TramCrossing& c:crossings_) c.sections=sections_of(c.node);
    for(PathMover& m:movers_) m.sections=sections_of(m.node);
    for(LogRide& r:log_rides_) r.sections=sections_of(r.node);
    for(MistyBridge& b:bridges_) b.sections=sections_of(b.node);
    for(Geyser& g:geysers_) g.sections=sections_of(g.node);
    for(RollingLog& r:rolling_logs_) r.sections=sections_of(r.node);
    for(DynamiteHole& d:holes_) d.sections=sections_of(d.node);
    for(ChiselCrossover& c:chisels_) c.sections=sections_of(c.node);
    for(RollingRock& r:rocks_) r.sections=sections_of(r.node);
}
std::vector<SolidSphere> CourseObjects::solid_spheres() const {
    std::vector<SolidSphere> out;
    for(const ChiselCrossover& c:chisels_) if(c.active && !c.blow.on && c.solid_radius>0) out.push_back({c.solid_centre,c.solid_radius,-1});
    for(size_t i=0;i<rocks_.size();++i) {
        const RollingRock& r=rocks_[i];
        if(!r.active || r.blow.on) continue;
        const Matrix4 w=r.world();
        for(const ScenePrim& p:r.prims) if(p.sensor_group&0x10000) out.push_back({point_of(w,{{p.centre[0],p.centre[1],p.centre[2]}}),p.radius*axis_length(w),int(i)});
    }
    return out;
}
bool CourseObjects::projectile_hit(const Vec3& at,float radius) {
    for(ChiselCrossover& c:chisels_) {
        if(!c.active || c.blow.on || !(c.solid_radius>0) || !spheres_touch(at,c.solid_centre,radius+c.solid_radius)) continue;
        c.blown=true;c.blow.start();   // 0x4d01f0
        return true;
    }
    for(RollingRock& r:rocks_) {
        if(!r.active || r.blow.on) continue;
        const Matrix4 w=r.world();
        for(const ScenePrim& p:r.prims) {
            if(!(p.sensor_group&0x10000) || !spheres_touch(at,point_of(w,{{p.centre[0],p.centre[1],p.centre[2]}}),radius+p.radius*axis_length(w))) continue;
            r.timer=60;r.state=2;r.blow.start();  // 0x4c7734, 0x4c7769
            return true;
        }
    }
    return false;
}
std::vector<RouteWeight> CourseObjects::route_weights() const {
    std::vector<RouteWeight> out;
    for(const ChiselCrossover& c:chisels_) if(c.started && c.section>=0) for(int l=0;l<2;++l) out.push_back({c.section,c.polygon,l,c.link_weight(l)});
    return out;
}
namespace {
// 0x4a8680 / 0x4a8850: the rows enabled are the player's previous, current
// and next ones.
bool enabled(const std::vector<int>& linked,int player,size_t sections) {
    const int n=int(sections);
    if(linked.empty() || n<=0 || player<0) return true;
    return std::any_of(linked.begin(),linked.end(),[&](int s){return s==player || (s+1)%n==player || (player+1)%n==s;});
}
}
void CourseObjects::update(const std::vector<CarProbe>& cars,std::vector<ObjectHit>& hits,std::vector<int>& effect,int player_section,size_t sections) {
    hits.clear();
    lifts_.clear();rolled_.clear();pushes_.clear();
    for(ChiselCrossover& c:chisels_) {
        c.active=enabled(c.sections,player_section,sections);
        if(!c.active) continue;
        c.started=true;      // 0x4d0029: one shot, on the first activation
        c.blow.step();       // the WLogBase's action
        if(c.blown) continue;
        const int car=nearest_car(cars,c.centre,3000);    // 0x4d02b7
        if(car>=0 && size_t(car)<effect.size() && effect[size_t(car)]==0) effect[size_t(car)]=3;
    }
    for(RollingRock& r:rocks_) {
        const bool on=enabled(r.sections,player_section,sections);
        if(!on) {
            if(r.active) {r.active=false;r.blow.finish();}  // 0x41ba90 -> 0x4c7710 -> 0x4bfc20
            continue;
        }
        if(!r.active) {r.active=true;r.timer=0;r.grace=2;rock_start(r,1);}  // 0x41ba60 -> 0x4c7680, 0x4c5000
        if(r.timer>0) --r.timer;
        const int car=nearest_car(cars,r.centre,r.state==0?r.range:20000);
        if(car>=0 && size_t(car)<effect.size() && effect[size_t(car)]==0) effect[size_t(car)]=3;
        const bool any=car>=0;
        if(r.state==0) {
            if(r.loop) rock_start(r,1);
            else if(any) r.state=1;
        } else if(r.state==2) {
            if(!r.blow.on && !any && r.timer==0) rock_start(r,0);
        } else if(!r.loop && !any && r.timer==0) rock_start(r,0);
        r.blow.step();
        if(r.state!=1) continue;
        // 0x4c5030
        r.velocity=(r.velocity-r.gravity*r.directions[size_t(r.segment)][1])*r.drag;
        r.distance+=r.velocity;
        r.spin+=r.velocity/r.radius;
        if(r.spin<0) r.spin+=two_pi;
        if(r.spin>two_pi) r.spin-=two_pi;
        rock_frame(r);
        if(r.grace>0) {--r.grace;continue;}
        const Matrix4 w=r.world();
        bool struck=false;
        for(size_t i=0;i<cars.size() && !struck;++i) {
            if(cars[i].hidden) continue;
            for(const ScenePrim& p:r.prims) if(spheres_touch(cars[i].position,point_of(w,{{p.centre[0],p.centre[1],p.centre[2]}}),car_radius+p.radius*axis_length(w))) {hits.push_back({i});struck=true;break;}
        }
    }
    for(DynamiteHole& d:holes_) {
        d.active=enabled(d.sections,player_section,sections);
        if(!d.active) continue;
        const float range=d.armed?d.radius:10000.f;
        std::vector<size_t> near;
        for(size_t i=0;i<cars.size();++i) {
            if(cars[i].hidden) continue;
            const float dx=cars[i].position[0]-d.centre[0],dy=cars[i].position[1]-d.centre[1],dz=cars[i].position[2]-d.centre[2];
            if(dx*dx+dy*dy+dz*dz<range*range) near.push_back(i);
        }
        const bool was=d.armed;
        if(d.armed) {if(!near.empty()) {d.armed=false;d.offset=0;}}
        else if(near.empty()) {d.armed=true;d.offset=0;}
        if(!d.armed) d.offset=std::min(d.offset+d.vel,10000.f); // 0x512a9c
        if(was && !d.armed) for(size_t i:near) {
            CarPush push;push.car=i;push.decay=0.3f;
            for(size_t k=0;k<3;++k) push.force[k]=cars[i].position[k]-d.centre[k];
            push.force[1]+=100;
            pushes_.push_back(push);
        }
    }
    for(RollingLog& r:rolling_logs_) {
        r.active=enabled(r.sections,player_section,sections);
        if(!r.active || (r.axis!=3 && r.axis!=1)) continue;
        const float c=std::cos(r.rate),s=std::sin(r.rate);
        for(size_t i=0;i<cars.size();++i) {
            // 0x4c23ef: 0x4a7e70 with flags 1 lists the player cars alone (+0x38,
            // +0x3c), in the brick's section. Wine, sh_t2: a robot crosses at
            // y 1811 without turning, x following its yaw only.
            if(cars[i].hidden || cars[i].robot) continue;
            if(cars[i].section>=0 && std::find(r.sections.begin(),r.sections.end(),cars[i].section)==r.sections.end()) continue;
            Vec3 d{{cars[i].position[0]-r.centre[0],cars[i].position[1]-r.centre[1],cars[i].position[2]-r.centre[2]}};
            // u, v: the offsets across the axis.
            const size_t a=r.axis==3?0:1,b=r.axis==3?1:2,along=r.axis==3?2:0;
            if(std::abs(d[a])>=761 || std::abs(d[b])>=761 || std::abs(d[along])>=4315 || d[a]*d[a]+d[b]*d[b]>=579121) continue;
            const float u=c*d[a]+s*d[b],v=c*d[b]-s*d[a];
            CarPlacement m;m.car=i;m.position=cars[i].position;
            m.position[a]=r.centre[a]+u;m.position[b]=r.centre[b]+v;
            rolled_.push_back(m);
        }
    }
    for(Geyser& g:geysers_) {
        g.active=enabled(g.sections,player_section,sections);
        if(!g.active) continue;
        const auto next_random=[&]() {geyser_random_=geyser_random_*214013u+2531011u;return int(geyser_random_>>16&0x7fff);};
        const auto filter=[&](size_t i,float in) {
            const float w=g.filter[0]*g.s0[i]+g.filter[1]*g.s1[i]+in;
            const float out=g.filter[3]*g.s0[i]+g.filter[4]*g.s1[i]+w*g.filter[2];
            g.s1[i]=g.s0[i];g.s0[i]=w;return std::abs(out);
        };
        if(g.fountain) {
            // 0x4ca344 / 0x4ca3db: towards 1 then 0.8, or back.
            ++g.counter;
            for(size_t i=0;i<3;++i) {
                const bool late=g.counter>int(i)*g.delay;
                g.rise[i]=filter(i,(g.state==1)==late?1.f:0.8f);
            }
            g.spin+=0.017453292f;
            if(g.counter>=g.frames) {g.state=g.state==1?2:1;g.counter=0;}
        } else switch(g.state) {
            case 0: {
                const CarProbe* near=nullptr;
                for(const Vec3& s:g.sensors) {
                    for(const CarProbe& c:cars) {
                        if(c.hidden) continue;
                        const float dx=c.position[0]-s[0],dy=c.position[1]-s[1],dz=c.position[2]-s[2];
                        if(dx*dx+dy*dy+dz*dz<float(g.sugar)*float(g.sugar)) {near=&c;break;}
                    }
                    if(near) break;
                }
                bool go=true;
                if(near) {
                    if(next_random()%100<g.chance) g.aim={{near->position[0]+near->forward[0]*g.distance,g.home[1],near->position[2]+near->forward[2]*g.distance}};
                    else go=false;
                } else {
                    // 0x4c8e95: around the brick, within m_Sugar on x and z.
                    const auto around=[&]() {return (float(next_random())/16384.f-1)*float(g.sugar);};
                    g.aim={{g.home[0]+around(),g.home[1],g.home[2]+around()}};
                }
                if(go) {g.state=1;g.ring_scale=0;}
                g.counter=0;
                break;
            }
            case 1:
                g.ring_scale=float(++g.counter)/float(g.steps);
                if(g.counter>=g.steps) {g.counter=0;g.state=2;}
                break;
            case 2:
                ++g.counter;
                for(size_t i=0;i<3;++i) {g.rise[i]=filter(i,g.counter>int(i)*g.delay?1.f:0.f);}
                g.spin+=0.017453292f;
                if(g.counter>=g.frames) {g.state=3;g.counter=0;}
                break;
            case 3:
                ++g.counter;
                for(size_t i=0;i<3;++i) g.rise[i]=filter(i,0.6f);
                g.spin+=0.017453292f;
                if(g.counter>50) {g.rise={};g.s0={};g.s1={};g.state=4;g.counter=0;}
                break;
            case 4:
                ++g.counter;
                g.ring_scale=float(g.steps-g.counter)/float(g.steps);
                if(g.counter>=g.steps) {g.state=5;g.counter=0;}
                break;
            default:
                if(++g.counter>=g.frames) {g.state=0;g.counter=0;}
                break;
        }
        const float h=g.top();
        if(!(h>0.1f)) continue;
        // 0x4c91ed..0x4c9505: the cars in the column.
        const float reach=float(g.height)*h;
        for(size_t i=0;i<cars.size();++i) {
            if(cars[i].hidden) continue;
            // 0x4c9247/0x4ca52e: truncated to integers (__ftol), so a car just
            // under the top stays free until it has fallen a whole unit (Wine,
            // dt_t2: airtime 3..11 in the column, across it in about 5 s).
            const int dx=std::abs(int(cars[i].position[0]-g.aim[0])),dy=std::abs(int(cars[i].position[1]-g.aim[1])),dz=std::abs(int(cars[i].position[2]-g.aim[2]));
            if(dx>=g.width || dy>=int(reach) || dz>=g.width || dx*dx+dz*dz>=g.width*g.width) continue;
            const float top=g.aim[1]+reach,y=cars[i].position[1];
            GeyserLift l;l.car=i;
            if(y<top) {
                // 0x4c9326: a geyser puts the car on its axis. 0x4ca60b: a
                // fountain keeps the car's x and z; its limit is 100 (0x510414).
                const float rise=std::abs(y-top)<(g.fountain?100.f:g.max_fly)?top:y+100.f;
                l.position=g.fountain?Vec3{{cars[i].position[0],rise,cars[i].position[2]}}:Vec3{{g.aim[0],rise,g.aim[2]}};l.airtime=0;
            } else {l.position=cars[i].position;l.airtime=10;}
            lifts_.push_back(l);
        }
    }
    for(size_t k=0;k<bridges_.size();++k) {
        MistyBridge& m=bridges_[k];
        m.active=enabled(m.sections,player_section,sections);
        if(!m.active || m.axis==0) continue;
        // 0x440b40: the biquad towards the target.
        const auto step=[&](float in) {
            const float w=m.filter[0]*m.s0+m.filter[1]*m.s1+in;
            const float out=m.filter[3]*m.s0+m.filter[4]*m.s1+w*m.filter[2];
            m.s1=m.s0;m.s0=w;return out;
        };
        if(m.state==1) {
            const float a=step(m.max_angle);
            if(std::abs(m.angle-a)<1.7453292e-5f) m.state=2;
            m.angle=a;
        } else if(m.state==3) {
            const float a=step(0);
            if(std::abs(m.angle-a)<1.7453292e-5f) m.state=0;
            m.angle=std::abs(a);
        }
        for(size_t i=0;i<2;++i) {
            Matrix4 f=m.base[i];
            const float a=i==0?-m.angle:m.angle;
            if(m.axis==1) turn_x(f,a);
            else if(m.axis==2) turn_y(f,a);
            else turn_z(f,a);
            m.frames[i]=f;
        }
        // 0x4c5d3f: the sensors, stopping at the first that fires.
        const auto within=[&](const Vec3& c,float r) {
            for(const CarProbe& p:cars) {
                if(p.hidden) continue;
                const float dx=p.position[0]-c[0],dy=p.position[1]-c[1],dz=p.position[2]-c[2];
                if(dx*dx+dy*dy+dz*dz<=r*r) return true;
            }
            return false;
        };
        bool fired=false;
        uint32_t bit=1;
        for(const Vec3& c:m.sensors) {
            if(within(c,m.in_radius) && !(m.inside&bit)) {m.inside|=bit;fired=true;}
            if(!within(c,m.out_radius)) m.inside&=~bit;
            if(fired) break;
            bit<<=1;
        }
        // Table 0x4c5f14: shut or shutting opens, opening or open shuts.
        if(fired) m.state=m.state==0 || m.state==3?1:3;
        for(const CarProbe& p:cars) if(p.bridge==int(k) && m.open()) m.state=3;
    }
    for(LogRide& r:log_rides_) {
        r.active=enabled(r.sections,player_section,sections);
        std::fill(r.moved.begin(),r.moved.end(),Vec3{});
        if(!r.active) continue;
        if(r.delay>0) {--r.delay;continue;}
        r.offset+=r.vel;
        if(r.offset>r.wrap()) r.offset-=r.wrap();
        float d=r.offset;
        for(size_t i=0;i<r.logs.size();++i) {
            Vec3 a,b;
            if(path_distance_point(r.points,d,a) && path_distance_point(r.points,d+r.length,b)) {
                Vec3 z{{b[0]-a[0],b[1]-a[1],b[2]-a[2]}};
                const float l=std::sqrt(z[0]*z[0]+z[1]*z[1]+z[2]*z[2]);
                if(l>0) {
                    for(float& c:z) c/=l;
                    // x = up x z, y = z x x; then the half turn about y.
                    Vec3 x{{z[2],0,-z[0]}};const float xl=std::sqrt(x[0]*x[0]+x[2]*x[2]);
                    if(xl>0) {x[0]/=xl;x[2]/=xl;} else x={{1,0,0}};
                    const Vec3 y{{z[1]*x[2]-z[2]*x[1],z[2]*x[0]-z[0]*x[2],z[0]*x[1]-z[1]*x[0]}};
                    Matrix4 f=identity_matrix();
                    for(size_t k=0;k<3;++k) {f[k]=-x[k];f[4+k]=y[k];f[8+k]=-z[k];f[12+k]=(a[k]+b[k])*0.5f;}
                    Vec3 m{{f[12]-r.frames[i][12],f[13]-r.frames[i][13],f[14]-r.frames[i][14]}};
                    if(std::abs(m[0])<2*r.vel && std::abs(m[1])<2*r.vel && std::abs(m[2])<2*r.vel) r.moved[i]=m;
                    r.frames[i]=f;
                }
            }
            d+=(i%3==2)?r.dist:r.length+50;
        }
    }
    effect.resize(cars.size(),0);
    inside_.resize(crossings_.size());
    for(size_t k=0;k<crossings_.size();++k) {
        TramCrossing& c=crossings_[k];
        if(!enabled(c.sections,player_section,sections)) {
            // 0x41ba90 -> 0x4caed0: the tram back at its saved frame, hidden.
            if(c.active) {c.active=false;c.progress=0;}
            continue;
        }
        // 0x41ba60 -> 0x4cad90 on each enabling: +0x15b = 0, +0x122 = 0xff,
        // +0x11e = 2. Robots keep the +0x1c7 a disabled crossing left.
        if(!c.active) {c.active=true;c.progress=0;c.phase=0xff;c.timer=2;}
        if(c.timer==0) {
            c.phase=(c.phase+1)&0xff;
            if(c.phase>5) c.phase=0;
            c.timer=phases[c.phase].updates;
        } else --c.timer;
        // 0x4cb08c: phase 0xff reads past the table; the bytes there are 0 in WPC.EXE.
        if((c.phase<6 && (phases[c.phase].b&4)) || c.progress!=0) {
            if(c.length<=c.progress) {c.progress=0;c.timer=0;}
            else c.progress+=c.speed;
        }
        if(c.progress!=0) {
            const Matrix4 f=c.tram_frame();const float scale=axis_length(f);
            for(size_t i=0;i<cars.size();++i) for(const ScenePrim& s:c.spheres)
                if(!cars[i].hidden && spheres_touch(cars[i].position,point_of(f,{{s.centre[0],s.centre[1],s.centre[2]}}),car_radius+s.radius*scale)) {hits.push_back({i});break;}
        }
        auto& in=inside_[k];in.resize(cars.size(),false);
        for(size_t i=0;i<cars.size();++i) {
            if(!cars[i].robot || cars[i].hidden) continue;
            const Vec3 d{{c.centre[0]-cars[i].position[0],c.centre[1]-cars[i].position[1],c.centre[2]-cars[i].position[2]}};
            const bool now=within(d,c.radius);
            if(!in[i] && now && effect[i]==0 && (c.lamp_a()&1)) effect[i]=5;
            if(now && effect[i]==5 && (c.lamp_a()&4)) effect[i]=0;
            in[i]=now;
        }
    }
    for(PathMover& m:movers_) {
        // 0x4d1c20 (the action's +0x14) only drops its collision entry.
        m.active=enabled(m.sections,player_section,sections);
        if(!m.active) continue;
        if(m.finished) {
            // 0x4d2330: wait m_Waiting updates, then run back from that end.
            if(m.waited++>m.waiting) {
                m.finished=false;m.waited=0;
                m.t=m.step>0?m.end:m.start;m.step=-m.step;
            }
            continue;
        }
        // 0x4d1d06..: the car query at the current frame, then the frame for
        // t, then the step.
        if(m.collision) {
            const Matrix4& f=m.placed;
            for(size_t i=0;i<cars.size();++i) for(const ScenePrim& p:m.prims) {
                const Vec3 c=point_of(f,{{p.centre[0],p.centre[1],p.centre[2]}});
                if(cars[i].hidden) continue;
                const bool touch=p.cylinder?sphere_cylinder(cars[i].position,car_radius,c,p.radius,p.height)
                                           :spheres_touch(cars[i].position,c,car_radius+p.radius);
                if(touch) {hits.push_back({i});break;}
            }
        }
        m.placed=m.frame();
        m.spin+=m.spin_step;
        if(m.spin>two_pi) m.spin-=two_pi; else if(m.spin<0) m.spin+=two_pi;
        const float t=m.t+m.step;m.t=t;
        if(t>m.end) {
            if(m.loop) {m.t=t-1;m.finished=true;continue;}
            m.t=m.end;m.step=0;   // provisional: the deactivation (0x41ba90) stops it
        } else if(t<m.start) {
            if(m.loop) {m.t=t+1;m.finished=true;continue;}
            m.t=m.start;m.step=0;
        }
    }
}
}
