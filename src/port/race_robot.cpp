#include "race_robot.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace wpc {
namespace {
constexpr float pi=3.1415925f;   // the wrap bounds 0x4e4f10 uses
float wrap(float a) {
    while(a< -pi) a+=6.283185f;
    while(a>pi) a-=6.283185f;
    return a;
}
// fpatan(dz, dx): headings and directions in the X/Z plane.
float heading(const Vec3& forward) {return std::atan2(forward[2],forward[0]);}
float toward(const Vec3& from,const Vec3& to) {return std::atan2(to[2]-from[2],to[0]-from[0]);}
bool same_side(float a,float b) {return (a>=0 && b>=0) || (a<=0 && b<=0);}
// 0x52e600: route speed classes (route flags >> 24) times the top speed.
constexpr float speed_class[11]={1.f,0.9f,0.8f,0.7f,0.6f,0.5f,0.4f,0.3f,0.2f,0.1f,0.f};
}
int choose_route_link(const RaceTrack& track,int section,uint32_t record,const RouteNode& node,MsvcRandom& random,const std::vector<RouteFork>& forks) {
    std::vector<RouteLink> links=node.forward;
    links.insert(links.end(),node.backward.begin(),node.backward.end());
    if(links.empty()) return -1;
    // 0x4e9610: a first link to 0xffff (a section's exit) is taken as is;
    // the walk starts from the first link.
    if(links[0].target==0xffff) return 0;
    const int r=random.next();
    uint32_t sum=0;int last=0;
    for(size_t i=0;i<links.size();++i) {
        if(links[i].target==0xffff) return last;
        // A WMisty (0x4c5e9d) or WChiselCrossover (0x4d0143) writes its fork's
        // link weights.
        uint32_t weight=links[i].weight;
        for(const RouteFork& f:forks) if(f.section==section && f.polygon==node.polygon && size_t(f.link)==i) weight=uint32_t(f.weight);
        sum+=weight;
        const uint32_t flags=track.route_node(section,record,links[i].target).flags;
        if(flags&0xff0000) continue;
        last=int(i);
        if(sum>uint32_t(r*2-1)) return int(i);
    }
    return last;
}
bool RoboDriver::update_goals(const RaceTrack& track,const RaceTrack::Route& module,MsvcRandom& random) {
    // Off the route layer the original leaves +0x2c5 at 0 and only drives
    // straight. Provisional: the port keeps the last module and goals, since
    // its wall and edge responses can push a car off the layer where the
    // original's would not (dt_t1's first bend).
    if(!module.hit) return goals_known_;
    if(module_.hit && module_.section==module.section && module_.polygon==module.polygon) return goals_known_;
    const bool continued=next_.hit && next_.section==module.section && next_.polygon==module.polygon;
    module_=module;
    const RouteNode node=track.route_node(module.section,module.record,module.polygon);
    module_flags_=node.flags;
    dead_end_=node.forward.empty() || node.forward[0].target==0xffff;
    std::vector<RouteLink> links=node.forward;
    links.insert(links.end(),node.backward.begin(),node.backward.end());
    // 0x4e9610's first loop keeps the link chosen in the previous module for
    // this polygon (+0x54); otherwise a weighted random forward link.
    link_=continued && next_link_>=0 && size_t(next_link_)<links.size()?next_link_:
          choose_route_link(track,module.section,module.record,node,random,forks_);
    next_={};next_link_=-1;
    // No link at all: 0x4e9750 returns with the previous goals kept.
    if(link_<0) return goals_known_;
    goals_known_=true;
    const RouteLink& link=links[size_t(link_)];
    goal_=track.route_point(module.section,link.point);
    // A link to 0xffff (the section's exit): goal 1 is its point, with no
    // second goal (+0x54 and +0x2c9 cleared). The edge point is kept
    // (provisional: +0x70 without a next polygon is not traced).
    if(link.target==0xffff) return true;
    next_={true,module.section,module.record,link.target};
    const RouteNode after=track.route_node(module.section,module.record,link.target);
    next_link_=choose_route_link(track,module.section,module.record,after,random,forks_);
    std::vector<RouteLink> later=after.forward;
    later.insert(later.end(),after.backward.begin(),after.backward.end());
    if(next_link_>=0) goal2_=track.route_point(module.section,later[size_t(next_link_)].point);
    // +0x70: the midpoint of the module's two vertices that the next polygon
    // does not share (its trailing edge).
    int u=-1,v=-1;
    for(size_t i=0;i<node.vertex_count;++i) {
        const uint16_t id=node.vertices[i];
        if(std::find(after.vertices.begin(),after.vertices.begin()+after.vertex_count,id)!=after.vertices.begin()+after.vertex_count) continue;
        if(u<0) u=id; else v=id;
    }
    if(u>=0 && v>=0) {
        const Vec3 a=track.route_vertex(module.section,module.record,uint16_t(u)),b=track.route_vertex(module.section,module.record,uint16_t(v));
        for(size_t i=0;i<3;++i) edge_[i]=(a[i]+b[i])/2;
    }
    return true;
}
float RoboDriver::far_turn(const Vec3& position,const Vec3& forward) const {
    if(!goals_known_) return 0;
    const float h=heading(forward);
    const float a=wrap(h-toward(position,goal_));
    if(next_link_<0) return a;
    const float b=wrap(h-toward(position,goal2_));
    return same_side(a,b) && std::abs(b)<std::abs(a)?b:a;
}
RankRow rank_row(int environment,int course) {
    // 0x52b910: {lead, gap, close, boost, far catch-up, far hold} per course.
    static const RankRow rows[5][4]={
        {{2000,6000,10000,.2f,1.5f,.9f,.4f},{4000,3000,7000,.25f,1.6f,1.1f,.43f},{4000,2000,5000,.25f,1.6f,1.2f,.46f},{5000,2000,3000,.28f,1.6f,1.1f,.47f}},
        {{2000,6000,10000,.2f,1.5f,.9f,.5f},{4000,3000,7000,.21f,1.7f,1.1f,.53f},{5000,2000,5000,.28f,1.6f,1.2f,.56f},{6000,1000,1000,.28f,1.6f,1.2f,.59f}},
        {{2000,6000,10000,.2f,1.5f,.9f,.6f},{4000,3000,7000,.25f,1.6f,1.1f,.63f},{2900,5400,5000,.3f,1.5f,1.2f,.66f},{5000,1500,1500,.29f,1.6f,1.1f,.69f}},
        {{2000,6000,10000,.2f,1.7f,.9f,.7f},{4000,3000,7000,.25f,1.6f,1.1f,.7f},{5000,1000,5000,.3f,1.6f,1.2f,.7f},{6000,1500,1500,.3f,1.7f,1.2f,.7f}},
        {{5000,1000,1000,.29f,1.7f,1.2f,.7f},{7000,1500,1500,.29f,1.7f,1.2f,.7f},{4000,1500,1500,.28f,1.2f,.8f,.7f},{5500,1500,1500,.3f,1.6f,1.2f,.7f}}};
    if(environment<0 || environment>=5 || course<0 || course>=4) return {};
    return rows[environment][course];
}
int rank_scale(const RankView& v,const RankRow& r,int environment,int course) {
    int result=0;
    const bool back=environment!=4 && ((course==2 && v.rank>5) || (course!=2 && course!=3 && v.rank>4));
    if(back) {
        // 0x4e4d74: hold back within close of the car ahead.
        const int ahead=v.has_ahead?v.ahead:v.progress+r.close+1;
        if(ahead-r.close<v.progress) result=-1;
    } else {
        const int ahead=v.has_ahead?v.ahead:v.progress+r.gap+1;
        if(v.progress<r.lead+v.player && v.progress<ahead-r.gap) result=1;
        if(v.progress<r.close+v.player && v.player-r.close<v.progress && ahead-r.gap<v.progress) result=-1;
    }
    if(v.lap<2 && result<0) result=0;
    return result;
}
bool course_indices(const std::string& file,int& environment,int& course) {
    std::string name=file.substr(file.find_last_of("/\\")+1);
    for(char& c:name) c=char(std::tolower(static_cast<unsigned char>(c)));
    static const char* const env[5]={"cc","sh","dt","ct","dr"};
    if(name.size()<5 || name[2]!='_') return false;
    for(int e=0;e<5;++e) if(name.compare(0,2,env[e])==0) {
        environment=e;
        if(name[3]=='t' && name[4]>='1' && name[4]<='4') {course=name[4]-'1';return true;}
        if(name[3]=='b') {course=0;return true;}
        return false;
    }
    return false;
}
void pickup_turn(float& s,const RoboCarState& car) {
    if(car.speed<0 || car.box_count==0) return;
    const float h=heading(car.forward);
    bool found=false;float best=s+pi;
    for(int i=0;i<car.box_count && i<4;++i) {
        const Vec3& b=car.boxes[size_t(i)];
        const int dx=int(b[0]-car.position[0]),dy=int(b[1]-car.position[1]),dz=int(b[2]-car.position[2]);
        const int d2=int32_t(uint32_t(dx)*uint32_t(dx)+uint32_t(dy)*uint32_t(dy)+uint32_t(dz)*uint32_t(dz)); // imul wraps
        if(d2>=0x895441) continue;
        const float alpha=wrap(h-toward(car.position,b));
        const float diff=std::abs(wrap(alpha-s)); // 0x496f90
        if(!(alpha<0.34906584f && alpha> -0.34906584f)) continue;
        const bool close=(d2<250000 && diff<1.3962634f) || (d2<1000000 && diff<1.0471976f) || (d2<4000000 && diff<0.6981317f) || diff<0.5235988f;
        if(close && diff<std::abs(wrap(best-s))) {best=alpha;found=true;}
    }
    if(found) s=best;
}
namespace {
bool same_module(const RaceTrack::Route& a,const RaceTrack::Route& b) {return a.hit && b.hit && a.section==b.section && a.record==b.record && a.polygon==b.polygon;}
Vec3 mid(const Vec3& a,const Vec3& b) {return {{(a[0]+b[0])*0.5f,(a[1]+b[1])*0.5f,(a[2]+b[2])*0.5f}};}
Vec3 sub(const Vec3& a,const Vec3& b) {return {{a[0]-b[0],a[1]-b[1],a[2]-b[2]}};}
Vec3 unit3(Vec3 v) {const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(l>0) for(float& c:v) c/=l;return v;}
// |dx| + |dz| / 2 with the larger first, truncated (0x4e6d45.., 0x46914a..).
float octagonal(float x,float z) {x=std::abs(x);z=std::abs(z);return x>z?x+z*0.5f:z+x*0.5f;}
int line_distance(const Vec3& from,const Vec3& dir,const Vec3& p) {
    const Vec3 d=sub(p,from);const float t=d[0]*dir[0]+d[1]*dir[1]+d[2]*dir[2];
    return int(octagonal(from[0]+t*dir[0]-p[0],from[2]+t*dir[2]-p[2]));
}
// 0x4e6c76..: the distance a car moved in its own update (+0x2ed).
int moved(const Vec3& position,const Vec3& previous) {return int(octagonal(position[0]-previous[0],position[2]-previous[2]));}
}
int RoadEdges::left_distance(const Vec3& p) const {return line_distance(left,left_dir,p);}
int RoadEdges::right_distance(const Vec3& p) const {return line_distance(right,right_dir,p);}
RoadEdges road_edges(const RaceTrack& track,const RaceTrack::Route& module,const RaceTrack::Route& next) {
    RoadEdges e;
    if(!module.hit || !next.hit || same_module(module,next) || module.section!=next.section) return e;
    const RouteNode a=track.route_node(module.section,module.record,module.polygon),b=track.route_node(next.section,next.record,next.polygon);
    std::array<Vec3,2> trail{},lead{};int nt=0,nl=0;
    for(size_t i=0;i<a.vertex_count;++i) {
        const uint16_t id=a.vertices[i];
        const bool shared=std::find(b.vertices.begin(),b.vertices.begin()+b.vertex_count,id)!=b.vertices.begin()+b.vertex_count;
        const Vec3 v=track.route_vertex(module.section,module.record,id);
        if(!shared) {if(nt<2) trail[size_t(nt)]=v;++nt;}
        else if(nl<2) lead[size_t(nl++)]=v;
    }
    if(nt!=2 || nl!=2) return e; // provisional: fewer than two shared vertices leave stale +0x315/+0x321
    // Order both pairs to the same side of the route (from the trailing
    // midpoint towards the leading one).
    const Vec3 m=mid(trail[0],trail[1]),l=mid(lead[0],lead[1]);
    const float fx=-(l[0]-m[0]),fz=l[2]-m[2];
    if((trail[1][2]-m[2])*fx+(trail[1][0]-m[0])*fz<0) std::swap(trail[0],trail[1]);
    if((lead[1][2]-m[2])*fx+(lead[1][0]-m[0])*fz<0) std::swap(lead[0],lead[1]);
    e.hit=true;e.left=trail[0];e.right=trail[1];
    e.left_dir=unit3(sub(lead[0],trail[0]));e.right_dir=unit3(sub(lead[1],trail[1]));
    const Vec3 lat=unit3(sub(mid(lead[0],trail[0]),mid(lead[1],trail[1])));
    e.to_left={{lat[2]*300+lat[0]*600,lat[1]*600,-lat[0]*300+lat[2]*600}};
    e.to_right={{lat[2]*300-lat[0]*600,-lat[1]*600,-lat[0]*300-lat[2]*600}};
    return e;
}
RaceTrack::Route next_polygon(const RaceTrack& track,const RaceTrack::Route& module) {
    RaceTrack::Route r;
    if(!module.hit) return r;
    const RouteNode n=track.route_node(module.section,module.record,module.polygon);
    for(const RouteLink& k:n.forward) if(k.target!=0xffff && k.weight!=0) r={true,module.section,module.record,k.target,0};
    return r;
}
bool RoboDriver::avoid(const RaceTrack& track,const RoboCarState& car) {
    // 0x4e69bd..0x4e72ea, moving faster than 0.07: every other car on this
    // module or the next, ahead (within 300 only when heading the same way,
    // +-20 degrees; within +-120 degrees early in the race, under progress
    // 20000, for a car already kept up with, else in front) and not
    // pulling away (its +0x2ed at most 5, past 20000 15, above ours).
    // The one reached soonest, distance / (our step - its step + 5 or 15)
    // with the distance in 1/4096 units, under 120, gets an aim beside it:
    // to the side with room (the edges more than 300 from us and 700 from
    // it), at its position plus min(room / 700, 1.3) x its +0x345 or +0x351.
    if(!(car.speed>0.07f)) return avoiding_=false;
    const Vec3& p=car.position;const Vec3& f=car.forward;
    float best=120;int side=0;uint32_t mask=follow_mask_;
    const int my_step=moved(car.position,car.previous),k=car.progress>=20000?15:5;
    RoadEdges mine;bool mine_known=false;
    for(int i=0;i<car.other_count;++i) {
        const OtherCar& o=car.others[size_t(i)];
        const uint32_t bit=1u<<o.slot;
        mask&=~bit;
        if(!o.module.hit || !(same_module(o.module,module_) || same_module(o.module,next_))) continue;
        const Vec3 d=sub(o.position,p);
        if(std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2])<300) {
            const float rel=wrap(heading(f)-heading(o.forward));
            if(rel< -0.34906584f || rel>0.34906584f) continue;
        }
        const float dx=d[0]*(1.f/4096),dz=d[2]*(1.f/4096);
        if(car.progress<20000 && (follow_mask_&bit)) {
            const float r=wrap(heading(f)-std::atan2(dz,dx));
            if(r< -2.0943952f || r>2.0943952f) continue;
        } else if(dz*f[2]+dx*f[0]<0) continue;
        const int its_step=moved(o.position,o.previous);
        if(float(its_step-k)>float(my_step)) continue;
        mask|=bit;
        const float t=octagonal(dz,dx)/float(my_step-its_step+k);
        if(!(t<best)) continue;
        const RoadEdges its=road_edges(track,o.module,o.next);
        if(!its.hit) continue;
        if(!mine_known) {mine=road_edges(track,module_,next_);mine_known=true;}
        if(!mine.hit) continue;
        // Only while driving along our left side.
        const float ex=mine.left_dir[0],ez=mine.left_dir[2];
        if(ex*f[0]+ez*f[2]<0) continue;
        const int my_left=mine.left_distance(p),my_right=mine.right_distance(p);
        const int its_left=its.left_distance(o.position),its_right=its.right_distance(o.position);
        if(my_left<301 || its_left<701 || (its_left<my_left && my_right>699 && avoid_side_!=-1)) {
            if(my_right>300 && its_right>700 && (my_right<=its_right || my_left<700 || avoid_side_==1)) {
                const float r=std::min(float(its_right)*0.0014285714f,1.3f);
                for(size_t j=0;j<3;++j) avoid_[j]=o.position[j]+r*its.to_right[j];
                side=1;best=t;
            }
        } else {
            const float r=std::min(float(its_left)*0.0014285714f,1.3f);
            for(size_t j=0;j<3;++j) avoid_[j]=o.position[j]+r*its.to_left[j];
            side=-1;best=t;
        }
    }
    avoid_side_=side;follow_mask_=mask;
    return avoiding_=best<120;
}
RoboControl RoboDriver::drive(const RaceTrack& track,const RoboCarState& car,MsvcRandom& random) {
    RoboControl c;   // 0x4e5d..: throttle 1, steering 0 by default
    const RaceTrack::Route found=track.route(car.position);
    if(!update_goals(track,found,random)) {
        if(car.surface&0x1000001) c.throttle=0;
        return c;
    }
    const Vec3& p=car.position;
    const float h=heading(car.forward);
    const float a=wrap(h-toward(p,goal_));
    const float k=params_.dir_modifier;
    // Route flag 1: turn round when the goal is more than m_aReversingAngle
    // behind, unless this module was already turned on.
    if(!(module_flags_&1)) {
        if(state_==4) {turned_module_=turn_module_;turned_known_=turn_known_;state_=8;}
    } else if((turned_known_ && turned_module_==module_.polygon) || std::abs(a)<=params_.reversing) {
        turned_module_=turn_module_;turned_known_=turn_known_;state_=2;
    } else {state_=4;turn_module_=module_.polygon;turn_known_=true;}
    float s=0;
    const auto turn_round=[&](float angle) {
        c.throttle=2;
        if(car.speed<0) {
            if(angle> -3.054326f && angle<0) c.steer=(pi-angle)*k*0.5f;
            else if(angle>0 && angle<3.054326f) c.steer=(-pi-angle)*k*0.5f;
        }
    };
    if(state_==0x10) {
        // Unstick: 30 updates back, wait to stop, then 30 forward turning
        // 5 degrees to a random side.
        const int n=stuck_&0xffff;
        if(n<30) {c.throttle=2;++stuck_;}
        else if(n==30) {if(std::abs(car.speed)<0.05f) stuck_+=(random.next()%2)*0x10000+1;}
        else if(n<60) {c.steer=(stuck_&0xffff0000)?0.08726646f:-0.08726646f;c.throttle=1;++stuck_;}
        else {c.throttle=0;stuck_=0;state_=2;}
    } else if(state_==1 || state_==2) {
        // Slow updates (less than 50 units on both axes since the car's own
        // update began) count; after 100 the robot unsticks.
        // 0x4e6919: no slow count while waiting at a crossing.
        if(std::abs(car.previous[0]-p[0])>=50 || std::abs(car.previous[2]-p[2])>=50) stuck_=0;
        else if(car.effect!=5) ++stuck_;
        if(stuck_<0x65 && avoid(track,car)) {
            // 0x4e7a9a..: steer for the aim beside the car ahead, at 0.4
            // of the gain up to progress 20000.
            const float b=wrap(h-toward(p,avoid_));
            c.steer=car.progress<20001?-0.4f*k*b:-k*b;
        } else if(stuck_<0x65) {
            float b;
            if(next_link_<0 || (module_flags_&4)) {
                if(dead_end_) {
                    // Past a dead end: aim through the goal, away from the edge.
                    const Vec3 aim{{2*goal_[0]-edge_[0],0,2*goal_[2]-edge_[2]}};
                    b=wrap(h-toward(p,aim));
                } else b=a;
            } else b=wrap(h-toward(p,goal2_));
            const int dx=int(goal_[0]-p[0]),dz=int(goal_[2]-p[2]);
            const bool near=dx*dx+dz*dz<1000000;
            if(same_side(a,b)) {
                s=std::abs(b)<std::abs(a) || (near && car.speed>0.4f)?b:a;
                pickup_turn(s,car); // 0x4e7a5f
                if(car.speed<0) s=-s;
                if(s!=0 && std::abs(s)<=params_.fw_adjust)
                    c.steer=-(k*s*float(random.next()%30+80)*0.01f);
            } else if(!near || car.speed<=0.4f) {s=a;pickup_turn(s,car);}
            else {s=b;pickup_turn(s,car);c.steer=-(k*s);}
        } else {c.throttle=2;state_=0x10;stuck_=0;}
    } else if(state_==4) {
        turn_round(a);
    } else if(state_==8) {
        turn_round(wrap(h-toward(p,edge_)));
        if(std::abs(a)<=params_.fw_ahead_again) {c.throttle=1;state_=2;}
    }
    if(car.effect==5) {
        // 0x4e7db0: waiting at a crossing, brake towards a stop.
        if(car.speed>0) c.throttle=2;
        else if(car.speed<0) c.throttle=1;
    } else if(state_!=0x10) {
        // Target speed: the top speed (or its route class), lowered in bends
        // over m_aMinSlowAngle by distance to the goal, at least m_rMinSpeed.
        const uint32_t cls=module_flags_>>24;
        float base=cls==0?car.top_speed:speed_class[std::min<uint32_t>(cls,10)]*car.top_speed;
        if(s>params_.min_slow_angle || s< -params_.min_slow_angle) {
            const float clipped=s>0.6981317f?0.6981317f:s;
            const float f=k*40.f/std::abs(clipped*57.29578f);
            float t;
            if(f<1) {
                const float d=std::sqrt((goal_[0]-p[0])*(goal_[0]-p[0])+(goal_[1]-p[1])*(goal_[1]-p[1])+(goal_[2]-p[2])*(goal_[2]-p[2]));
                const float span=float(car.probe_span);
                t=d<=span*car.top_speed*0.5f?car.speed:std::min(car.top_speed,d*f/span);
            } else t=car.top_speed;
            if(state_==4 || state_==8) t*=0.5f;
            if(car.speed>0 && t<params_.min_speed) t=params_.min_speed;
            else if(car.speed<0 && t<params_.min_speed*0.5f) t=params_.min_speed*0.5f;
            base=std::min(base,t);
        }
        if(car.speed>base) c.throttle=2;
        else if(car.speed< -base) c.throttle=1;
    } else {
        const int n=stuck_&0xffff;
        if(n<30) {
            if(std::abs(car.speed)>params_.min_speed*0.5f) {
                if(car.speed>0) c.throttle=2;
                else c.throttle=1;
            }
        } else if(n==30) {
            if(car.speed>0) c.throttle=2;
            else if(car.speed<0) c.throttle=1;
        }
    }
    // 0x4e82cd: no throttle on surfaces 0x1000001.
    if(car.surface&0x1000001) c.throttle=0;
    return c;
}
}
