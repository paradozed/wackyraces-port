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
            const Matrix4 inv=inverse_affine_matrix(f);
            const Vec3 a=point_of(inv,{{x,top,z}}),b=point_of(inv,{{x,bottom,z}});
            const Vec3 d{{b[0]-a[0],b[1]-a[1],b[2]-a[2]}};
            for(const auto& t:r.triangles[i]) {
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
                best.hit=true;best.y=y;best.normal=w;best.ride=int(ri);best.log=int(i);
            }
        }
    }
    return best;
}
Vec3 CourseObjects::log_motion(int ride,int log) const {
    if(ride<0 || size_t(ride)>=log_rides_.size()) return {};
    const auto& m=log_rides_[size_t(ride)].moved;
    return log>=0 && size_t(log)<m.size()?m[size_t(log)]:Vec3{};
}
void CourseObjects::load(const Scene& scene) {
    crossings_.clear();movers_.clear();inside_.clear();log_rides_.clear();
    const Node& root=scene.root();
    const auto visit=[&](const auto& self,const Node& n)->void {
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
        if((phases[c.phase].b&4) || c.progress!=0) {
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
