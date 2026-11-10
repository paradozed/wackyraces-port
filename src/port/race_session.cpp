#include "race_session.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wpc {
namespace {
constexpr float pi=3.14159265358979f;
// WModManager countdown (0x4a2b30): m_nStartWait=60 forced by 0x4a0550; state 1
// lasts 61 updates (PRET!), state 2 lasts 12 (PARTEZ!).
constexpr uint64_t ready_updates=61,go_updates=12;
// 0x46b357: the fall is min(+0x8a7^2 x m_gravity, 100) units per update,
// then scaled with the whole motion by the frame factor (+0x163).
constexpr float fall_cap=100;
// 0x468280 mode 2 movement probes: +/-52 across, +/-half the stretched
// probe span along; their ground rays (mask 0x18000ff) run from 80 above
// (0x512a3c) to +0x93b = 30 below (0x46610b) along the car's down axis.
constexpr float ray_above=80,ray_below=30,probe_across=52;
constexpr float step_height=150;  // 0x49eb70 spawn query height
constexpr int drown_delay=45;     // Wine: splash then return after ~1.5 s
// WackyAuto PRIM GDPrimSphere radius (0x8d) on all seven vehicles, centred
// on the car's origin; the cars are unscaled.
constexpr float car_sphere=100;
// 0x482190: spheres touch when each axis difference is within the sum of the
// radii and then the integer-truncated squared centre distance is at most
// its square (the box test first keeps the integer squares in range).
// 0x482310: a sphere and an upright cylinder (base centre c, radius rc,
// height h) touch when the X/Z boxes overlap, the sphere's vertical extent
// meets [c.y, c.y + h] and the integer-truncated X/Z distance squared is at
// most int(rs + rc) squared.
bool sphere_cylinder(const Vec3& s,float rs,const TrackCylinder& c) {
    const float dx=s[0]-c.base[0],dz=s[2]-c.base[2],r=rs+c.radius;
    if(std::abs(dx)>r || std::abs(dz)>r) return false;
    if(c.base[1]>s[1]+rs || s[1]-rs>c.base[1]+c.height) return false;
    const int ix=int(dx),iz=int(dz),ir=int(r);
    return ix*ix+iz*iz<=ir*ir;
}
bool spheres_touch(const Vec3& a,const Vec3& b,float radii) {
    for(size_t i=0;i<3;++i) if(std::abs(a[i]-b[i])>radii) return false;
    const int dx=int(a[0]-b[0]),dy=int(a[1]-b[1]),dz=int(a[2]-b[2]),r=int(radii);
    return dx*dx+dy*dy+dz*dz<=r*r;
}
Vec3 forward_of(float yaw) {return {{-std::sin(yaw),0,std::cos(yaw)}};}
Vec3 right_of(float yaw) {return {{std::cos(yaw),0,std::sin(yaw)}};}
Vec3 cross(const Vec3& a,const Vec3& b) {return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};}
float dot(const Vec3& a,const Vec3& b) {return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 unit(Vec3 v) {const float l=std::sqrt(dot(v,v));if(l>0) for(float& c:v) c/=l;return v;}
// The up axis from the pitch and roll about the heading (0x43c250).
Vec3 tilted_up(float yaw,float pitch,float roll) {
    const Vec3 f=forward_of(yaw),r=right_of(yaw);
    Vec3 a{},sd{};
    for(size_t i=0;i<3;++i) {a[i]=f[i]*std::cos(pitch);sd[i]=r[i]*std::cos(roll);}
    a[1]+=std::sin(pitch);sd[1]-=std::sin(roll);
    Vec3 u=unit(cross(a,sd));
    if(u[1]<0) for(float& v:u) v=-v;
    return u;
}
constexpr float one_degree=0.01745329238f;
float wrap_angle(float a) {return std::remainder(a,2*pi);}
Vec3 transform_point(const Matrix4& m,const Vec3& p) {
    return {{m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]}};
}
Vec3 rotate(const Matrix4& m,const Vec3& v) {
    return {{m[0]*v[0]+m[4]*v[1]+m[8]*v[2],m[1]*v[0]+m[5]*v[1]+m[9]*v[2],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]}};
}
// Heading change of a transform: the yaw of its image of the +Z heading.
float yaw_turn(const Matrix4& m) {const Vec3 f=rotate(m,forward_of(0));return std::atan2(-f[0],f[2]);}
}
Matrix4 RaceCar::frame() const {
    const Vec3 f0=forward_of(yaw);
    const Vec3 r=unit(cross(up,f0));
    const Vec3 f=unit(cross(r,up));
    Matrix4 m=identity_matrix();
    for(size_t i=0;i<3;++i) {m[i]=r[i];m[4+i]=up[i];m[8+i]=f[i];m[12+i]=position[i];}
    return m;
}
RaceSession::RaceSession(const RaceTrack& track,int vehicle,uint32_t seed,int mode,int vehicle2)
    :track_(track),tuning_(vehicle_tuning(vehicle)),objects_(track.objects()),mode_(mode) {
    const bool two=vehicle2>=0 && vehicle2<7;
    const auto roster=two ? std::vector<Racer>{{vehicle,0,true},{vehicle2,3,false}} : race_roster(mode==3?0:mode,vehicle,seed);
    grid_=place_race_grid(roster,track_.start_line(),[&](GroundPoint start,GroundPoint end,uint32_t) {
        GroundQueryResult result;
        const TrackSurface s=ground(start.x,start.z,start.y,end.y);
        result.hit=s.hit;
        if(s.hit) result.contact.point={start.x,s.y,start.z};
        return result;
    });
    const Matrix4& line=track_.start_line();
    float yaw=std::atan2(-line[8],line[10]);
    // Battle (mode 1, 0x49eb70): the two spawn frames replace the grid.
    if(track_.battle()) {
        if(mode!=1) throw std::runtime_error("a battle arena needs game mode 1");
        for(auto& placed:grid_.racers) {
            const Matrix4& f=track_.battle_frame(placed.racer.player?0:1);
            for(size_t r=0;r<3;++r) placed.frame[12+r]=f[12+r];
        }
    } else if(mode==1) throw std::runtime_error("battle mode needs an arena without a WStartLine");
    for(const auto& placed:grid_.racers) {
        const Vec3 p{{placed.frame[12],placed.frame[13],placed.frame[14]}};
        if(track_.battle()) {const Matrix4& f=track_.battle_frame(placed.racer.player?0:1);yaw=std::atan2(-f[8],f[10]);}
        if(placed.racer.player) {
            player_.vehicle=vehicle;player_.position=p;player_.yaw=yaw;player_previous_=p;
        } else {
            Opponent o;o.vehicle=placed.racer.vehicle;o.tuning=vehicle_tuning(o.vehicle);
            o.human=two;
            if(!two) o.tuning.gravity*=2; // 0x4e45e0 doubles the robots' +0x98
            o.car.vehicle=o.vehicle;o.car.position=p;o.car.yaw=yaw;o.previous=p;
            opponents_.push_back(o);opponent_bodies_.push_back(Body{});
            opponent_bodies_.back().fx.set_track_width(CarEffects::track_width(o.vehicle));
        }
    }
    // 0x49f51d: each robot's +0xb4 = 1 + c + rand() x 2^-15 x c / 4, with c
    // the course's 0x6f2cd4.
    for(Opponent& o:opponents_) o.rank_unit=random_.unit();
    set_course(0,0);
    body_.fx.set_track_width(CarEffects::track_width(vehicle));
    init_cameras();
    power_ups_.assign(track_.power_ups().size(),PowerUpState{});
    settle(player_,body_,true);
    for(size_t i=0;i<opponents_.size();++i) settle(opponents_[i].car,opponent_bodies_[i],false);
}
float RaceSession::progress(const Vec3& p,int section,int loops) const {
    return float(loops*int(track_.sections())+section)+track_.section_fraction(section,p);
}
float RaceSession::opponent_progress(size_t i) const {
    const RaceCar& c=opponents_.at(i).car;
    return float(c.loops*int(track_.sections())+c.section)+fraction(int(i));
}
float RaceSession::fraction(int car) const {
    // A hidden robot's distance into its row is +0x2b5 (0x4a3a92).
    const RaceCar& c=car<0?player_:opponents_.at(size_t(car)).car;
    const Body& b=car<0?body_:opponent_bodies_.at(size_t(car));
    if(car>=0 && b.hidden)
        return std::clamp(float(b.virtual_distance)/float(std::max(1,track_.row_length(c.section))),0.f,1.f);
    // 0x4a3937: the distance left in the row is the cursor polygon's +0x17
    // x the row length >> 8; the geodesic field stands in before the first
    // hit in this section.
    if(b.cursor>=0 && b.cursor_section==c.section) return 1-float(b.cursor)/256.f;
    return track_.section_fraction(c.section,c.position);
}
void RaceSession::set_course(int environment,int course) {
    environment_=environment;course_=course;rank_row_=rank_row(environment,course);
    for(Opponent& o:opponents_) o.rank_boost=1+rank_row_.boost+o.rank_unit*rank_row_.boost*0.25f;
}
float RaceSession::progress_length(int section) const {
    // The rows' route lengths (+0x34): the lap is their sum (0x4a3ba0, the
    // manager's +0xf38), and a car's distance in its row that row's length x
    // its fraction (0x4a3937: the cursor polygon's +0x17 x the length >> 8).
    return float(track_.row_length(section));
}
int RaceSession::race_distance(int car) const {
    const RaceCar& c=car<0?player_:opponents_.at(size_t(car)).car;
    float lap=0,before=0;
    for(size_t s=0;s<track_.sections();++s) {
        const float l=progress_length(int(s));
        lap+=l;if(int(s)<c.section) before+=l;
    }
    return int(float(c.loops)*lap+before+fraction(car)*progress_length(c.section));
}
int RaceSession::race_rank(int car) const {
    const auto finished=[&](int i)->const Body& {return i<0?body_:opponent_bodies_.at(size_t(i));};
    const Body& me=finished(car);
    const int mine=race_distance(car);
    int ahead=0;
    for(int i=-1;i<int(opponents_.size());++i) {
        if(i==car) continue;
        const Body& b=finished(i);
        if(b.finished && (!me.finished || b.finish_update<me.finish_update)) ++ahead;
        else if(!b.finished && !me.finished) {const int d=race_distance(i);if(d>mine || (d==mine && i<car)) ++ahead;}
    }
    return ahead+1;
}
void RaceSession::robot_boxes(size_t i) {
    // 0x4e7480: every 31 updates (+0xcc counts to 30), the four nearest
    // power-ups (flag 0x2000000) within 6000 (0x516b3c) on each axis, by
    // integer squared distance (provisional: every course power-up rather
    // than the section cell's object list).
    Opponent::Items& it=opponents_[i].items;
    if(it.list_timer<30) {++it.list_timer;return;}
    it.list_timer=0;it.boxes={{-1,-1,-1,-1}};
    std::array<int64_t,4> best{{INT64_MAX,INT64_MAX,INT64_MAX,INT64_MAX}};
    const Vec3& p=opponents_[i].car.position;
    const auto& spots=track_.power_ups();
    for(size_t k=0;k<spots.size();++k) {
        const Vec3& q=spots[k].centre;
        if(std::abs(q[0]-p[0])>6000 || std::abs(q[1]-p[1])>6000 || std::abs(q[2]-p[2])>6000) continue;
        const int64_t dx=int64_t(q[0]-p[0]),dy=int64_t(q[1]-p[1]),dz=int64_t(q[2]-p[2]);
        const int64_t d2=dx*dx+dy*dy+dz*dz;
        for(size_t j=0;j<4;++j) if(d2<best[j]) {
            for(size_t m=3;m>j;--m) {best[m]=best[m-1];it.boxes[m]=it.boxes[m-1];}
            best[j]=d2;it.boxes[j]=int(k);break;
        }
    }
}
void RaceSession::robot_slots(size_t i) {
    Opponent& o=opponents_[i];Opponent::Items& it=o.items;const Body& b=opponent_bodies_[i];
    switch(it.gadget_state) {
        case 0: if(it.gadget<0) {it.gadget=o.vehicle;it.gadget_state=0xff;} break;
        case 1: it.gadget=(random_.next()&3)+7;it.gadget_state=0xff;break;
        case 4: if(b.effect<0) {it.gadget=-1;it.gadget_state=0;} break;
        case 5:
            --it.charge;use_gadget(it.gadget,int(i));
            it.gadget=-1;it.gadget_state=4;
            break;
        default: break;
    }
    switch(it.weapon_state) {
        case 0:
            if(it.weapon<0) {it.weapon=o.vehicle==6?random_.next()%9+0x11:o.vehicle+0xb;it.weapon_state=0xff;}
            break;
        case 1: it.weapon=random_.next()%9+0x11;it.weapon_state=0xff;break;
        case 4: if(b.weapon.id<0) {it.weapon=-1;it.weapon_state=0;} break;
        case 5:
            --it.charge;use_weapon(it.weapon,int(i));
            it.weapon=-1;it.weapon_state=4;
            break;
        default: break;
    }
    if(it.charge>3) it.charge=3; // 0x46809f
}
void RaceSession::robot_items(size_t i) {
    Opponent& o=opponents_[i];Opponent::Items& it=o.items;const Body& b=opponent_bodies_[i];
    const RaceCar& c=o.car;
    // 0x4e5a..: every 300 + rand() % 300 updates a robot decides, with the
    // course's 0x6f2c48 chance, to aim only at the player (+0xe4).
    if(--it.prefer_timer<1) {
        it.prefer_timer=random_.next()%300+300;
        it.prefer_player=random_.unit()<rank_row_.prefer;
    }
    const auto distance=[&](int t) {
        const Vec3& q=t<0?player_.position:opponents_[size_t(t)].car.position;
        return std::sqrt((q[0]-c.position[0])*(q[0]-c.position[0])+(q[1]-c.position[1])*(q[1]-c.position[1])+(q[2]-c.position[2])*(q[2]-c.position[2]));
    };
    // The targets' angle and range: the item's WPowerUpSprite m_visangle and
    // ftol(m_range) (0x4e5b0a).
    const auto target=[&](float angle,int range) {return find_target(c.position,c.yaw,c.section,angle,range,int(i));};
    const auto aim=[&](int icon) {const ItemAim& a=aims_.at(size_t(std::clamp(icon,0,26)));return target(a.angle,int(a.range));};
    if(it.gadget_state==0xff && it.field_wait==0) {
        bool use=false;
        switch(it.gadget) {
            case 8: use=b.turbo==0;break;                       // 0x2ab07ac: not already boosted
            case 7:                                              // 0x2ab07a8; 0x4e5a4b: or a WRotoHill's +0x1c7 = 0xb
                use=(c.surface&0x80007c)!=0 || o.effect==0xb;
                if(use && o.effect==0xb) o.effect=0;             // 0x4e5d68
                break;
            case 9: {                                            // 0x2ab07b0 (0x4e5b61)
                const int t=aim(9);
                use=t!=-2 && distance(t)>700 && (t<0 || !it.prefer_player); // 0x516b54
                if(use) it.prefer_timer=0;
                break;
            }
            case 10: use=b.hit.guard==1 && !b.hit.active() && aim(10)!=-2;break; // 0x2ab07b4 (5000, 2 pi)
            default: break; // the vehicle's own gadget: +0x1c7 states 1/0xb (not ported)
        }
        if(use) it.gadget_state=5;
    }
    if(it.weapon>=0 && it.weapon_state==0xff && it.charge>0 && it.gadget_state!=5 && it.field_wait==0 && !b.hit.active()) {
        switch(it.weapon) {
            case 0x11:case 0x12:case 0x13:case 0x15:case 0x17: it.weapon_state=5;break; // dropped at once
            default: {
                const int t=aim(it.weapon);   // 0x4e5af8 (11..16, 20, 22, 24, 25)
                // 0x4e5ea1: a brick's +0x1c7 = 3 (or 4) fires without the wait,
                // and is cleared by firing (0x4e5ef5).
                if(t!=-2 && distance(t)>700 && (t<0 || !it.prefer_player) && (it.weapon_wait<1 || t<0 || o.effect==3 || o.effect==4)) {
                    if(o.effect==3 || o.effect==4) o.effect=0;
                    it.weapon_state=5;it.prefer_timer=0;
                    it.weapon_wait=random_.next()%100+100;
                }
                break;
            }
        }
    }
}
void RaceSession::scale_by_rank(size_t i) {
    Opponent& o=opponents_[i];const Body& b=opponent_bodies_[i];
    RankView v;
    v.rank=race_rank(int(i));v.lap=b.laps+1;v.progress=race_distance(int(i));v.player=race_distance(-1);
    for(size_t j=0;j<opponents_.size();++j) if(race_rank(int(j))==v.rank-1) {v.has_ahead=true;v.ahead=race_distance(int(j));break;}
    const int r=rank_scale(v,rank_row_,environment_,course_);
    o.rank_scale=r;
    if(r<1 && o.rank_boosted && b.laps+1>1) {
        // 0x4e4f6b: back to the saved rows, m_rDirModifier 0x52e62c (0.25).
        o.rank_boosted=false;
        o.tuning.top_speed=o.base.top_speed;o.tuning.slide_push=o.base.slide_push;o.tuning.reverse_step=o.base.reverse_step;
        o.tuning.gravity=o.base.gravity; // +0x365
        o.robot.set_dir_modifier(0.25f);
    } else if(r==1 && !o.rank_boosted) {
        // 0x4e4fe9: top speed, +0x933 and the gravity +0x98 x +0xb4, row[4]
        // x 0x52e670 (0.3), m_rDirModifier 0x52e630 (0.4).
        o.rank_boosted=true;o.base=o.tuning;
        o.tuning.top_speed*=o.rank_boost;o.tuning.reverse_step*=o.rank_boost;o.tuning.slide_push*=0.3f;
        o.tuning.gravity*=o.rank_boost;
        o.robot.set_dir_modifier(0.4f);
    }
}
int RaceSession::lap() const {return std::min(body_.laps+1,track_.laps());}
int RaceSession::rank() const {
    const float mine=float(player_.loops*int(track_.sections())+player_.section)+fraction(-1);
    int ahead=0;
    // Racers level with the player (the same grid row) count as ahead, so a
    // fresh grid shows 7th as in the Wine start frame.
    for(size_t i=0;i<opponents_.size();++i) {
        const Body& b=opponent_bodies_[i];
        if((b.finished && (!body_.finished || b.finish_update<=body_.finish_update)) ||
           (!body_.finished && opponent_progress(i)>=mine-0.01f)) ++ahead;
    }
    return ahead+1;
}
double RaceSession::race_seconds() const {
    if(phase_==Phase::Ready || phase_==Phase::Go) return 0;
    const uint64_t end=phase_==Phase::Finished?finish_update_:updates_;
    return double(end-start_update_)*race_update_seconds;
}
void RaceSession::set_camera_members(const FollowCameraMembers& m) {camera_members_=m;init_cameras();}
void RaceSession::init_cameras() {
    follow_[0]=player_.frame();
    follow_[1]=two_players()?opponents_[0].car.frame():follow_[0];
    for(size_t k=0;k<2;++k) cameras_[k].init(camera_members_,two_players(),follow_[k]);
}
void RaceSession::update_cameras() {
    const CameraCast cast=[&](const Vec3& a,const Vec3& b) {
        const TrackSurface s=track_.ray(a,b);
        return CameraHit{s.hit,s.point};
    };
    const auto one=[&](size_t k,const RaceCar& car,const Body& b,const DriveInput& in) {
        // 0x470535: 0x468620(+0x3eb > 0).
        const Matrix4 now=car.frame();
        if(b.motion>0) follow_[k]=now; else for(size_t r=0;r<3;++r) follow_[k][12+r]=now[12+r];
        FollowCameraState st;
        st.view=in.view;st.look_back=in.look_back;
        // States 1 (race) and 0xc (practice) before the countdown; the
        // battle starts in state 9.
        st.intro=phase_==Phase::Ready && mode_!=1 ? int(phase_updates_)-1 : -1;
        st.reset=b.drown>0;
        st.near_locked=near_lock_[k]>0;if(near_lock_[k]>0) --near_lock_[k];
        st.speed=car.speed.speed;st.yaw=car.yaw;st.finished=b.finished;
        cameras_[k].update(follow_[k],st,cast);
    };
    one(0,player_,body_,input1_);
    if(two_players()) one(1,opponents_[0].car,opponent_bodies_[0],input2_);
}
void RaceSession::finish() {
    if(mode_==2) {
        // Practice (0x4a3421): state 0xf, counter 60; no standings. A time
        // below the record's low 28 bits becomes the record, with the
        // vehicle in the high 4 bits (0x4a3473), and sets +0xfa4.
        finish_state_=15;finish_counter_=60;
        const uint32_t time=uint32_t(std::lround(race_seconds()*300));
        new_record_=time<(record_&0xfffffffu);
        if(new_record_) record_=time|uint32_t(player_.vehicle)<<28;
        return;
    }
    // State 3 -> 4: the rank table orders the racers; the player's place is
    // rank(). Parked opponents keep their grid order behind it.
    // Racers that have finished come first by finishing update, then the
    // others by progress; the player's place is rank().
    struct Entry {bool finished;uint64_t update;float progress;int vehicle;};
    std::vector<Entry> order;
    const int player_rank=rank();
    for(size_t i=0;i<opponents_.size();++i) {
        const RaceCar& c=opponents_[i].car;const Body& b=opponent_bodies_[i];
        order.push_back({b.finished,b.finish_update,opponent_progress(i),c.vehicle});
    }
    std::stable_sort(order.begin(),order.end(),[](const Entry& a,const Entry& b){
        if(a.finished!=b.finished) return a.finished;
        if(a.finished) return a.update<b.update;
        return a.progress>b.progress;
    });
    standings_.clear();
    for(const auto& o:order) standings_.push_back(o.vehicle);
    standings_.insert(standings_.begin()+std::min<size_t>(size_t(player_rank-1),standings_.size()),player_.vehicle);
    // State 4: single race -> 5; other modes give 10/6/4/3/2/1/0 by place
    // (0x52cef0) and go to 6. Both start the counter at 120.
    static const int points[7]={10,6,4,3,2,1,0};
    race_points_={};
    if(championship_) for(size_t r=0;r<standings_.size() && r<7;++r) race_points_.at(size_t(standings_[r]))=points[r];
    finish_state_=championship_?6:5;
    finish_counter_=120;
}
bool RaceSession::accept() {
    if(finish_state_==0 || finish_counter_>0) return false;
    if(finish_state_!=6) return true;
    // State 6 -> 7: selection sort of the rank table by total points
    // (previous total + this race), largest first; counter back to 120.
    std::vector<int> sorted=standings_;
    const auto score=[&](int v){return totals_.at(size_t(v))+race_points_.at(size_t(v));};
    for(size_t i=0;i+1<sorted.size();++i) {
        size_t best=i;int top=score(sorted[i]);
        for(size_t j=i+1;j<sorted.size();++j) if(score(sorted[j])>top) {top=score(sorted[j]);best=j;}
        std::swap(sorted[i],sorted[best]);
    }
    standings_=sorted;
    finish_state_=7;finish_counter_=120;
    return false;
}
std::vector<std::string> RaceSession::take_item_events() {
    std::vector<std::string> out;out.swap(item_events_);return out;
}
void RaceSession::update_power_ups() {
    // 0x4d40c0, called once per manager update with 0.2269 rad (0x3e685696).
    spin_+=0.2268928f;
    if(spin_>pi) spin_-=2*pi; else if(spin_<-pi) spin_+=2*pi;
    const auto& spots=track_.power_ups();
    const int n=int(track_.sections());
    for(size_t i=0;i<power_ups_.size();++i) {
        PowerUpState& p=power_ups_[i];
        const auto& linked=spots[i].sections;
        p.enabled=linked.empty() || n<=0 || std::any_of(linked.begin(),linked.end(),[&](int s){
            return s==player_.section || (s+1)%n==player_.section || (player_.section+1)%n==s;});
        if(!p.enabled) continue; // a disabled brick is not updated (0x432970)
        switch(p.state) {
            case 1: p.state=2;p.counter=0;break; // model hidden (flag 0x100)
            case 2: if(++p.counter>60) p.state=3;break; // 1000 in battle mode
            case 3:
                // 0x4040e8: reappear only when no car's sphere touches the pickup's.
                {
                    bool touched=spheres_touch(player_.position,spots[i].centre,car_sphere+spots[i].radius);
                    if(robots_enabled_) for(size_t r=0;r<opponents_.size();++r) touched=touched || (!opponent_bodies_[r].hidden && spheres_touch(opponents_[r].car.position,spots[i].centre,car_sphere+spots[i].radius));
                    if(!touched) {p.state=4;p.counter=0;p.scale=0.2f;}
                }
                break;
            case 4:
                if(++p.counter>20) {p.state=0;p.counter=0;p.scale=1;}
                else p.scale=float(p.counter)*0.04f+0.2f;
                break;
            default: break;
        }
    }
}
void RaceSession::pick_up() {
    // 0x46aad5: an idle WPowerUp hit by the car's object query (0x4a5730,
    // mask 0x79600, sensor group 0x8000) is taken and adds a charge. The
    // query tests the car's GDPrimSphere against the pickup's (0x482190).
    const auto& spots=track_.power_ups();
    for(size_t i=0;i<power_ups_.size();++i) {
        if(power_ups_[i].state!=0 || !power_ups_[i].enabled) continue;
        if(!spheres_touch(player_.position,spots[i].centre,car_sphere+spots[i].radius)) continue;
        power_ups_[i].state=1;++items_.charge;++body_.pickup_sounds;
        static const char* const kinds[3]={"gadget","weapon","refill"};
        // Node +0xa8 (0x28b): 0 starts the gadget roulette, 1 the weapon one,
        // 2 empties both slots so they return to the vehicle's own items.
        switch(spots[i].type) {
            case 0: items_.gadget_state=1;break;
            case 1: items_.weapon_state=1;break;
            default: items_.gadget_state=items_.weapon_state=0;items_.gadget=items_.weapon=-1;break;
        }
        item_events_.push_back(std::string("picked up ")+kinds[spots[i].type]+" power-up "+std::to_string(spots[i].node));
    }
    // Every WackyAuto runs the same query (0x46aad5); a robot's pickup
    // starts its slot (state 1) for 0x4679d0's robot path. Far robots do not
    // run the car update (provisional: no pickups while sliding).
    if(!robots_enabled_) return;
    for(size_t r=0;r<opponents_.size();++r) {
        Opponent& o=opponents_[r];
        if(o.far || opponent_bodies_[r].drown>0) continue;
        for(size_t i=0;i<power_ups_.size();++i) {
            if(power_ups_[i].state!=0 || !power_ups_[i].enabled || !spheres_touch(o.car.position,spots[i].centre,car_sphere+spots[i].radius)) continue;
            if(o.human) {
                // Player 2 takes the human slot path (roulette), as player 1.
                ++opponent_bodies_[r].pickup_sounds;power_ups_[i].state=1;++items2_.charge;
                switch(spots[i].type) {
                    case 0: items2_.gadget_state=1;break;
                    case 1: items2_.weapon_state=1;break;
                    default: items2_.gadget_state=items2_.weapon_state=0;items2_.gadget=items2_.weapon=-1;break;
                }
                item_events_.push_back("player 2 picked up power-up "+std::to_string(spots[i].node));
                continue;
            }
            power_ups_[i].state=1;++o.items.charge;
            switch(spots[i].type) {
                case 0: o.items.gadget_state=1;break;
                case 1: o.items.weapon_state=1;break;
                default: o.items.gadget_state=o.items.weapon_state=0;o.items.gadget=o.items.weapon=-1;break;
            }
            item_events_.push_back("robot "+std::to_string(r)+" picked up power-up "+std::to_string(spots[i].node));
        }
    }
}
void RaceSession::update_items(const DriveInput& input) {
    human_items(items_,player_.vehicle,rank(),body_,!drowning(),input,-1);
}
void RaceSession::human_items(Items& it,int vehicle,int place,Body& body,bool can_use,const DriveInput& input,int car) {
    // 0x4679d0 for a human player (+0xc0 != 1) in a race (not battle mode 1);
    // input.gadget/weapon are presses (0x6e42b0). car: -1 player 1, 0 player 2.
    switch(it.gadget_state) {
        case 0:
            if(it.gadget<0) {it.gadget=vehicle;it.gadget_state=3;}
            break;
        case 1: {
            it.gadget=(random_.next()&3)+7;
            // Trailing racers (rank 4 and below) are favoured with 7 or 8.
                        if(place>3 && random_.unit()<float(place-4)*0.15f+0.3f) it.gadget=random_.unit()>=0.7f?8:7;
            it.gadget_steps=1;it.gadget_tick=1;it.gadget_state=2;
            break;
        }
        case 2:
            // The roulette steps every 5 updates for 10 steps; the gadget key stops it.
            if(!input.gadget) {
                if(++it.gadget_tick==5) {
                    ++it.gadget_steps;it.gadget_tick=0;
                    if(++it.gadget==11) it.gadget=7;
                    if(it.gadget_steps==10) it.gadget_state=3;
                }
            } else it.gadget_state=3;
            break;
        case 3:
            if(input.gadget && it.charge>0 && can_use && body.effect<0) {
                --it.charge;
                use_gadget(it.gadget,car);
                it.gadget=-1;it.gadget_state=4;
            }
            break;
        case 4: if(body.effect<0) it.gadget_state=0;break; // the effect (+0x385) has ended
        default: break;
    }
    switch(it.weapon_state) {
        case 0:
            if(it.weapon<0) {it.weapon=vehicle==6?random_.next()%9+0x11:vehicle+0xb;it.weapon_state=3;}
            break;
        case 1: {
            it.weapon=random_.next()%9+0x11;
                        if(place>3 && random_.unit()<float(place-4)*0.1f+0.5f) {
                const float f=random_.unit();
                it.weapon=f>=0.25f?(f>=0.5f?0x15:0x19):0x18;
            }
            it.weapon_steps=1;it.weapon_tick=1;it.weapon_state=2;
            break;
        }
        case 2:
            if(!input.weapon) {
                if(++it.weapon_tick==5) {
                    ++it.weapon_steps;it.weapon_tick=0;
                    if(++it.weapon==0x1a) it.weapon=0x11;
                    if(it.weapon_steps==10) {it.weapon_preview=false;it.weapon_state=3;}
                }
            } else it.weapon_state=3;
            break;
        case 3:
            // Weapons 0x14 and 0x16 spawn a held preview effect first.
            if((it.weapon==0x16 || it.weapon==0x14) && !it.weapon_preview && it.charge>0) it.weapon_preview=true;
            if(input.weapon && it.charge>0 && can_use && body.weapon.id<0) {
                --it.charge;
                // 0x4d2970 clones world object 0x6f2c50[icon]: 0x483a20
                // follows each data record's next-sibling id (+0x10, set
                // at 0x41b4bd), so icon i is WFirstPowerUp's i+1th sibling.
                // 0x4d24f0: the action's +0x5c is the look-back key (input
                // 0x4, mask 0x52aad4) of a human driver, 0 for robots.
                use_backward_=input.look_back;
                use_weapon(it.weapon,car);
                use_backward_=false;
                it.weapon=-1;it.weapon_state=4;
            }
            break;
        case 4: if(body.weapon.id<0) {it.weapon_state=0;it.weapon_preview=false;} break; // +0x389 ended
        default: break;
    }
    if(it.charge>3) it.charge=3;
}
void RaceSession::update_bend_arrow() {
    // 0x4a4650 refreshes the arrow only while racing (states 2, 3, 14).
    bend_arrow_=-1;
    if(phase_!=Phase::Racing || player_.section<0) return;
    for(const BendSign& sign:track_.bend_signs(player_.section)) {
        // Components truncated to integers, as 0x4a6890's __ftol calls.
        const int dx=int(sign.position[0]-player_.position[0]),dy=int(sign.position[1]-player_.position[1]),dz=int(sign.position[2]-player_.position[2]);
        const int r=int(sign.radius);
        if(int32_t(uint32_t(dx)*uint32_t(dx)+uint32_t(dy)*uint32_t(dy)+uint32_t(dz)*uint32_t(dz))>r*r) continue; // imul wraps
        // Headings atan2(-x,z) of the car's and the sign's forward axes.
        const float sign_heading=std::atan2(-sign.forward[0],sign.forward[2]);
        const float turn=wrap_angle(player_.yaw-sign_heading);
        if(turn<pi/3 && turn>-pi/3) {bend_arrow_=sign.type;return;}
    }
}
std::vector<RaceSession::StripRacer> RaceSession::strip() const {
    // 0x499d90: x = 220 x the distance into the lap (+0x1a3) / the lap
    // length (+0xf38, 0x4a3ba0), with the race distance's lengths.
    std::vector<StripRacer> out;
    float lap=0;for(size_t s=0;s<track_.sections();++s) lap+=progress_length(int(s));
    size_t opponent=0;
    for(const auto& placed:grid_.racers) {
        StripRacer r;r.vehicle=placed.racer.vehicle;r.player=placed.racer.player;
        float d;
        if(r.player) {d=float(race_distance(-1));r.finished=phase_==Phase::Finished;}
        else {
            d=float(race_distance(int(opponent)));r.finished=opponent_bodies_.at(opponent).finished;++opponent;
        }
        r.lap_fraction=r.finished?1.f:lap>0?std::clamp(std::fmod(std::max(0.f,d),lap)/lap,0.f,1.f):0.f;
        out.push_back(r);
    }
    return out;
}
void RaceSession::place(float x,float z,float yaw) {
    const TrackSurface g=ground(x,z,9000,-9000);
    if(!g.hit) throw std::runtime_error("race placement has no ground");
    player_.position={{x,g.y,z}};player_.yaw=yaw;player_.section=g.section;
    settle(player_,body_,true);init_cameras();
}
void RaceSession::set_dust_option(bool on) {
    body_.fx.set_smoke(on);
    for(Body& b:opponent_bodies_) b.fx.set_smoke(on);
}
void RaceSession::set_weather(int option,const std::string& course) {
    // 0x432bdc..0x432c79: cc_t1..cc_t4 and dr_t1 rain; past them
    // 0x2a1b2b0 = 1 (snow) and the flag needs ct_, dr_t4 or dr_b.
    bool rain=false,snow=false;
    for(const char* name:{"cc_t1","cc_t2","cc_t3","cc_t4","dr_t1"}) rain=rain || course.find(name)!=std::string::npos;
    if(!rain) for(const char* name:{"ct_","dr_t4","dr_b"}) snow=snow || course.find(name)!=std::string::npos;
    raining_=(rain || snow) && (option==1 || (option==2 && random_.next()%100>25));
    snowing_=raining_ && snow;
}
void RaceSession::update(const DriveInput& raw,const DriveInput& raw2) {
    input1_=raw;input2_=raw2;
    voice_events_.clear();
    const bool was_drowning=drowning();
    std::vector<bool> robots_drowning;
    for(const Body& b:opponent_bodies_) robots_drowning.push_back(b.drown>0);
    step(raw);
    // 0x4a3136, state 9 (the battle): once either car has received the
    // limit, state 10 (0x4a318e) stops both (+0x2b1 = 0x10000000); in one
    // player the counter +0xf34 is not set (Enter at once, 0x4a31fe). The
    // menu offers battle only to two players (menu.WGM page 47).
    if(mode_==1 && phase_==Phase::Racing && !opponent_bodies_.empty() &&
       (body_.frags>=frag_limit || opponent_bodies_[0].frags>=frag_limit)) {
        phase_=Phase::Finished;finish_update_=updates_;finish_state_=10;
        finish_counter_=two_players()?60:0; // 0x4a31ef: 60 in two players
        body_.finished=true;for(Body& b:opponent_bodies_) {b.finished=true;b.finish_update=updates_;} // brake to a stop
    }
    // The WModManager's route cursor (+0x2c5) of every car: the route
    // polygon under it, kept when the query misses.
    // The robots' driver already keeps theirs (RoboDriver::module); the
    // player's is queried.
    const auto cursor=[&](const RaceTrack::Route& r,Body& b) {
        if(!r.hit) return;
        try {b.cursor=track_.route_node(r.section,r.record,r.polygon).remaining;b.cursor_section=r.section;} catch(const std::exception&) {}
    };
    cursor(track_.route(player_.position,player_.section),body_);
    for(size_t i=0;i<opponents_.size();++i) if(!opponent_bodies_[i].hidden) cursor(opponents_[i].robot.module(),opponent_bodies_[i]);
    // Every WackyAuto runs its WCarFx; the player first, then the robots in
    // grid order (provisional: the original object order is not traced).
    car_effects(player_,body_,was_drowning,false,true);
    if(robots_enabled_) for(size_t i=0;i<opponents_.size();++i)
        if(!opponent_bodies_[i].hidden) car_effects(opponents_[i].car,opponent_bodies_[i],robots_drowning[i],opponents_[i].far,false);
    update_cameras();
}
void RaceSession::car_effects(RaceCar& car,Body& b,bool was_drowning,bool far,bool player) {
    // Wheel contacts stay as they were when the car went under. A far robot
    // skips its physics and so its wheel queries (provisional: no contacts).
    if(!was_drowning) b.wheels=far?std::array<WheelContact,4>{}:wheel_contacts(car);
    // WSplash takes the car's matrix when it enters deep water (0x472d00),
    // where the original car floats level; the native car drowns on the
    // bank's slope, so the splash gets the car's heading with a level up
    // axis (provisional: the drowning trigger is native).
    RaceCar level=car;level.up={{0,1,0}};
    // All WCarFx draw from the one effects stream (0x430ef0).
    std::swap(b.fx.random(),fx_random_);
    b.fx.update(car.frame(),car.speed.speed,b.wheels,b.drown>0 && !was_drowning,level.frame(),b.skid.marking());
    // 0x46aa58: an 0x40000 object hit (cc_t1's cacti) clones WKBumm with
    // the car's frame and speed. The query (0x4a5bd6) hides the primitive's
    // object (flag 0x100) and clears the 0x40000 bit, so each cactus breaks
    // once.
    if(knocked_.size()!=track_.cylinders().size()) knocked_.assign(track_.cylinders().size(),false);
    if(b.drown==0 && !far) for(size_t i=0;i<track_.cylinders().size();++i) {
        const TrackCylinder& c=track_.cylinders()[i];
        if(knocked_[i] || !(c.sensor_group&0x40000) || !sphere_cylinder(car.position,car_sphere,c)) continue;
        knocked_[i]=true;knocked_objects_.push_back(c.object);
        b.fx.bumm(car.frame(),car.speed.speed);
        if(player) ++bumms_;
        ++b.bumms;
        // The car's own +0x3cf sound, unless its 30-update timer runs.
        if(b.sound_timer==0) {b.sound_timer=30;++b.cactus_sounds;}
    }
    std::swap(b.fx.random(),fx_random_);
}
std::array<WheelContact,4> RaceSession::wheel_contacts(const RaceCar& car) const {
    // The four wheel queries of 0x46d8xx, at the template's tyre offsets.
    std::array<WheelContact,4> out{};
    if(!car.grounded) return out;
    const Matrix4 m=car.frame();
    const auto wheels=vehicle_wheels(car.vehicle);
    for(size_t w=0;w<4;++w) {
        const float x=m[12]+m[0]*wheels[w][0]+m[8]*wheels[w][1];
        const float z=m[14]+m[2]*wheels[w][0]+m[10]*wheels[w][1];
        const TrackSurface s=ground(x,z,car.position[1]+step_height,car.position[1]-4000);
        if(!s.hit) continue;
        out[w]={true,{{x,s.y,z}},s.flags&0x018000ffu};
    }
    return out;
}
void RaceSession::step(const DriveInput& raw) {
    ++updates_;++phase_updates_;
    if(phase_==Phase::Ready && phase_updates_>ready_updates) {phase_=Phase::Go;phase_updates_=0;}
    else if(phase_==Phase::Go && phase_updates_>go_updates) {
        phase_=Phase::Racing;phase_updates_=0;start_update_=updates_;
    }
    if(finish_counter_>0) --finish_counter_;
    DriveInput input=raw;
    if(phase_==Phase::Ready || phase_==Phase::Finished || body_.drown>0) input={};
    update_power_ups();
    update_items(input);
    update_opponents(input);
    if(body_.drown>0) {
        if(--body_.drown==0) respawn(player_,body_,true);
        // The effect objects update on their own while the car drowns.
        update_gadgets();update_weapons();update_tnts();update_storms();update_strikes();update_barriers();update_mines();update_blasts();
        return;
    }
    drive(input);
    update_gadgets();
    update_weapons();
    update_tnts();
    update_storms();
    update_strikes();
    update_kicks();
    update_barriers();
    update_mines();
    update_blasts();
    if(phase_==Phase::Racing) pick_up();
    update_objects();
    update_bend_arrow();
}
void RaceSession::update_opponents(const DriveInput&) {
    // Robots get no control before PARTEZ!, as the player.
    const bool controlled=phase_!=Phase::Ready;
    if(!robots_enabled_) return;
    std::vector<int> placed(track_.sections(),0);
    for(size_t i=0;i<opponents_.size();++i) {
        Opponent& o=opponents_[i];Body& b=opponent_bodies_[i];
        if(b.drown>0) {if(--b.drown==0) respawn(o.car,b,o.human);continue;}
        if(o.human) {
            const DriveInput in=controlled && !b.finished ? input2_ : DriveInput{};
            // Presses, as player 1's (the key words are ANDed with the last update's).
            DriveInput pressed=in;pressed.gadget=in.gadget && !o.last.gadget;pressed.weapon=in.weapon && !o.last.weapon;
            human_items(items2_,o.vehicle,race_rank(0),b,b.drown==0,pressed,int(i));
            drive_human(i,in);continue;
        }
        if(controlled && virtual_robot(i,placed)) continue;
        // 0x4e4f6b, in single races and championships (app +0x1a60 0 or 3).
        if(controlled && !b.finished) scale_by_rank(i);
        Opponent::Items& it=o.items;
        if(it.weapon_wait>0) --it.weapon_wait;
        if(it.field_wait>0) --it.field_wait;
        if(controlled && !b.finished && !o.far) {robot_boxes(i);robot_items(i);}
        robot_slots(i);
        drive_opponent(o,b,controlled);
    }
}
void RaceSession::drive_opponent(Opponent& o,Body& b,bool controlled) {
    RaceCar& car=o.car;
    const Vec3 start=car.position;
    // 0x4e4f10: away from the player (5000 units on an axis, or 25e6 squared
    // with integer coordinates) and with no item effect running, the robot
    // switches to 0x1000 and slides along the route; back near, it drives.
    const int dx=int(car.position[0]-player_.position[0]),dz=int(car.position[2]-player_.position[2]);
    const bool far=controlled && !robots_near_ && (std::abs(dx)>=5000 || std::abs(dz)>=5000 || dx*dx+dz*dz>24999999);
    if(far!=o.far) {o.far=far;car.speed.speed=o.tuning.top_speed*0.8f;car.speed.brake_state=0;}
    const float time=frame_time_factor(race_update_seconds*1000);
    // 0x4e53df: in the far state (0x1000) a running WackyAutoBomba (+0x2c set)
    // skips the far step, so the car update moves the robot with its last
    // control: held for the hit, then driving, until the hit ends. Wine,
    // cc_t1, robot 0 about 40000 away hit with 0x80: still for about 25
    // frames, accelerating 0.0216 a frame, gliding again at the hit's end.
    const bool hit=b.hit.active();
    // 0x4e53f0: a far robot waiting at a crossing (+0x1c7 = 5) stays put.
    if(far && !b.finished && !hit && o.effect==5) {o.previous=start;return;}
    if(far && !b.finished && !hit) {
        Vec3 from=start;
        // The last section's ground runs past its route (cc_t1: about 200
        // units); there the route continues only through the loop
        // transform, so the far robot crosses the seam with it, or it would
        // circle the exit goal (provisional: the original's route lookup
        // across sections, +0x2c5, is not traced).
        RaceTrack::Route here=track_.route(car.position);
        if(!here.hit) for(const int k:{1,-1}) {
            // The one seam joins the last section's exit to the first's entry
            // (dr_t3: a route gap in section 1 otherwise sent the robot 6000 back).
            if(k>0?car.section!=int(track_.sections())-1:car.section!=0) continue;
            if(!track_.route(transform_point(k>0?track_.wrap():track_.unwrap(),car.position)).hit) continue;
            cross_seam(car,b,k,false);from=transform_point(k>0?track_.wrap():track_.unwrap(),from);here=track_.route(car.position);break;
        }
        // 0x4e5300..: speed x 0.56 when ahead of the player, else 0.48, in
        // units per update (top x +0x163 x the probe span); turn by half the
        // goal angle (0x4530a0), then rest on the ground. 0x4e5538: x
        // 0x6f2cd8 while catching up, x 0x6f2cdc while holding back.
        const Vec3 f=forward_of(car.yaw);
        const bool ahead=progress(car.position,car.section,car.loops)>player_progress();
        float step=o.tuning.top_speed*time*204.f*(ahead?0.56f:0.48f);
        if(o.rank_scale==1) step*=rank_row_.far_catch_up;
        else if(o.rank_scale==-1) step*=rank_row_.far_hold;
        float turn=0;
        if(o.robot.update_goals(track_,here,random_)) turn=o.robot.far_turn(car.position,f);
        car.yaw=wrap_angle(car.yaw-0.5f*turn);
        const Vec3 g=forward_of(car.yaw);
        car.position[0]+=g[0]*step;car.position[2]+=g[2]*step;
        // The far motion rests on the route contact (+0x2bd, +0x2d5): it is
        // put on the route layer, or without one on the ground, before the
        // contact ray (which reaches 30 below). On the ground alone it would
        // drop off sh_t1's jumps (-19700) onto the floor below.
        {const RaceTrack::Route on=track_.route(car.position);
         const TrackSurface gr=ground(car.position[0],car.position[2],car.position[1]+step_height,car.position[1]-4000);
         if(on.hit) car.position[1]=on.y; else if(gr.hit) car.position[1]=gr.y;}
        // Far motion leaves the angles alone (Wine, cc_t1 lap: +0x267 and +0x26f
        // stay 0 and +0x26b frozen while y runs 562..1081). Level here, or a tilt
        // aims the contact ray into the slope and holds the robot (cc_t4's fork
        // at (-8817, -356, 28591), dr_t3 at (-218, 107, 18168)). Whether the
        // original's far step runs a contact at all is not traced.
        car.up={{0,1,0}};car.pitch=car.roll=0;
        const int loops=car.loops;
        b.sliding=true;settle(car,b,false);b.sliding=false;
        if(car.loops!=loops) from=transform_point(car.loops>loops?track_.wrap():track_.unwrap(),from);
        crossings(car,b,from,false);
        // No hit runs here (see 0x4e53df above); a type 0 bomba still steps.
        bomb_step(car,b);
        // Provisional: the car update 0x46a4a0 consumes its slots (0x4727b0,
        // 0x472890) and counts the motion counter up (0x46a70d) without
        // testing the far state. So a hit taken while far (its hop, its 4 pi
        // spin, the -5 hold) runs out as the robot slides. The far motion skips
        // move(), so natively they waited for the return to near mode (dr_t2:
        // 76 s later the robot spun in place, turned round and fell). They run
        // out here without moving the car. Not checked under Wine.
        if(b.motion<=0) ++b.motion;
        decay_spin(b.spin);
        for(Body::Push& p:b.pushes) {for(float& c:p.force) c*=p.decay;p.yaw*=p.yaw_decay<0?p.decay:p.yaw_decay;}
        b.pushes.erase(std::remove_if(b.pushes.begin(),b.pushes.end(),[](const Body::Push& p){return dot(p.force,p.force)<2 && std::abs(p.yaw)<1e-3f;}),b.pushes.end());
        o.previous=from;
        return;
    }
    DriveInput none;
    if(!controlled) {
        update_speed(car.speed,o.tuning,none);
        car.steer=update_steering(car.steer,o.tuning,none);
    } else {
        RoboCarState state;
        state.position=car.position;state.forward=forward_of(car.yaw);state.previous=o.previous;
        state.speed=car.speed.speed;state.top_speed=o.tuning.top_speed;state.surface=car.surface;
        // 0x4e4a10 skips taken power-ups (+0x142).
        for(int k:o.items.boxes) if(k>=0 && power_ups_[size_t(k)].state==0 && power_ups_[size_t(k)].enabled) state.boxes[size_t(state.box_count++)]=track_.power_ups()[size_t(k)].centre;
        other_cars(size_t(&o-opponents_.data()),state);
        state.effect=o.effect;
        // 0x4e9750 ff. after the finish: brake to a stop (0x10000000/0x20000000).
        if(b.finished) {
            o.control={car.speed.speed< -0.1f?1:car.speed.speed>0.1f?2:0,0};
            if(o.control.throttle==0) car.speed.speed=0;
        } else if(!b.hit.active()) {
            o.control=o.robot.drive(track_,state,random_);
            // 0x4e75c0: a WSaw's +0x1c7 = 7 or 8 steers aside for +0x1cb (3) updates.
            if(o.effect==7 || o.effect==8) {
                if(o.side_timer>0) {--o.side_timer;o.control.steer=o.effect==7?0.17453292f:-0.17453292f;}
                else o.effect=0;
            }
        }
        // 0x4e53df: no driving decision while WackyAutoBomba runs (the last
        // control stays; provisional).
        const SpeedState before=car.speed;
        // 0x4701f5: the modifiers follow +0x8bb (the last contact's flags)
        // at every update's end, in the air too.
        update_grip(b.grip,o.vehicle,car.surface);
        update_boost(b.grip,b.boost);
        VehicleTuning row=o.tuning;
        if(b.turbo>0) {row.top_speed*=turbo_top;row.convergence*=turbo_convergence;}
        update_robot_speed(car.speed,row,o.control.throttle,b.grip,b.follow_lock!=0);
        car.steer=robot_steering(o.control.steer);
        update_brake_skid(b.skid,o.control.throttle==1,o.control.throttle==2,before.speed,before.brake_state,car.grounded);
        update_slide(b.skid,car.speed.speed,car.steer,farol_speed_limit(o.vehicle),b.grip.friction,car.grounded,true);
    }
    move(car,b,o.tuning,false);
    bomb_step(car,b);
    o.previous=start;
}
void RaceSession::drive_human(size_t i,const DriveInput& input) {
    Opponent& o=opponents_[i];Body& b=opponent_bodies_[i];RaceCar& car=o.car;
    const Vec3 start=car.position;
    // Items: human_items (items2_) runs before.
    o.last=input;
    car.steer=update_steering(car.steer,o.tuning,input);
    const SpeedState before=car.speed;
    update_grip(b.grip,car.vehicle,car.surface);
    update_boost(b.grip,b.boost);
    VehicleTuning row=o.tuning;
    if(b.turbo>0) {row.top_speed*=turbo_top;row.convergence*=turbo_convergence;}
    if(car.airtime<=6) update_speed(car.speed,row,input,b.grip,b.skid.kick,b.follow_lock!=0);
    update_brake_skid(b.skid,input.accelerate,input.brake,before.speed,before.brake_state,car.grounded);
    update_slide(b.skid,car.speed.speed,car.steer,farol_speed_limit(car.vehicle),b.grip.friction,car.grounded);
    move(car,b,o.tuning,true);
    bomb_step(car,b);
    o.previous=start;
}
void RaceSession::update_objects() {
    // The course objects run after the cars (provisional order). Car 0 is
    // the player, then the robots in grid order.
    std::vector<CarProbe> cars{{player_.position,false,false,body_.bridge,forward_of(player_.yaw),player_.section,body_.hit.active(),body_.effect<0,player_.surface}};
    for(size_t i=0;i<opponents_.size();++i) cars.push_back({opponents_[i].car.position,!opponents_[i].human,opponent_bodies_[i].hidden,opponent_bodies_[i].bridge,forward_of(opponents_[i].car.yaw),opponents_[i].car.section,opponent_bodies_[i].hit.active(),opponent_bodies_[i].effect<0,opponents_[i].car.surface});
    object_effects_.resize(cars.size());
    for(size_t i=0;i<opponents_.size();++i) object_effects_[i+1]=opponents_[i].effect;
    objects_.update(cars,object_hits_,object_effects_,player_.section,track_.sections());
    // 0x4cd9eb: a dynamite hole's blast.
    for(const CarPush& p:objects_.pushes()) (p.car==0?body_:opponent_bodies_[p.car-1]).pushes.push_back({p.force,p.decay,p.yaw,p.yaw_decay});
    // 0x4cfa50: a WRotoHill holds the car: stopped, its slots cleared, turned and
    // placed, with airtime 0; +0x93b = 0 takes the ground rays' reach below away.
    for(size_t i=0;i<objects_.hovering().size();++i) (i==0?body_:opponent_bodies_[i-1]).hover=objects_.hovering()[i];
    for(const CarHeld& h:objects_.held()) {
        RaceCar& car=h.car==0?player_:opponents_[h.car-1].car;
        Body& b=h.car==0?body_:opponent_bodies_[h.car-1];
        car.speed.speed=0;b.pushes.clear();car.position=h.position;car.yaw=wrap_angle(car.yaw+h.yaw);car.airtime=0;
    }
    // 0x4c2547: the rolling log turns the cars on it.
    for(const CarPlacement& m:objects_.rolled()) (m.car==0?player_:opponents_[m.car-1].car).position=m.position;
    // 0x4c92cb: a car held by a geyser stops (+0x3ef, +0x403, its pushes).
    for(const GeyserLift& l:objects_.lifts()) {
        RaceCar& car=l.car==0?player_:opponents_[l.car-1].car;
        Body& b=l.car==0?body_:opponent_bodies_[l.car-1];
        car.speed.speed=0;b.pushes.clear();car.position=l.position;car.airtime=l.airtime;
    }
    for(const MistyBridge& m:objects_.bridges()) if(m.fork_section>=0)
        for(Opponent& o:opponents_) o.robot.set_route_fork({m.fork_section,m.fork_polygon,m.fork_weight()});
    for(const RouteWeight& w:objects_.route_weights())
        for(Opponent& o:opponents_) o.robot.set_route_fork({w.section,w.polygon,w.weight,w.link});
    // 0x4cc69e: a car whose +0x89f is a log moves by that log's motion,
    // and +0x89f is cleared.
    const auto ride=[&](RaceCar& car,Body& b) {
        if(b.log<0) return;
        const Vec3 m=objects_.log_motion(b.log_ride,b.log);
        for(size_t k=0;k<3;++k) car.position[k]+=m[k];
        b.log_ride=b.log=-1;
    };
    ride(player_,body_);
    for(size_t i=0;i<opponents_.size();++i) ride(opponents_[i].car,opponent_bodies_[i]);
    for(size_t i=0;i<opponents_.size();++i) {
        const int was=opponents_[i].effect,now=object_effects_[i+1];
        if((now==7 || now==8) && was!=7 && was!=8) opponents_[i].side_timer=3;   // 0x4c0d38
        opponents_[i].effect=now;
    }
    // 0x4d1c80/0x4cb27b: a touched car whose WackyAutoBomba is idle gets
    // type 0x200 (the squash).
    for(const ObjectHit& h:object_hits_) {
        const int car=int(h.car)-1;
        if((car<0?body_:opponent_bodies_.at(size_t(car))).drown>0) continue;
        hit(car,0x200);
    }
}
void RaceSession::other_cars(size_t self,RoboCarState& state) const {
    // 0x4e69bd walks 0x6f2ce0[0..7], the cars by vehicle slot.
    const Opponent& me=opponents_[self];
    state.slot=me.vehicle;state.progress=race_distance(int(self));
    std::array<OtherCar,8> by_slot{};std::array<bool,8> used{};
    const auto put=[&](const OtherCar& c) {if(c.slot>=0 && c.slot<8) {by_slot[size_t(c.slot)]=c;used[size_t(c.slot)]=true;}};
    {OtherCar c;c.slot=player_.vehicle;c.position=player_.position;c.previous=player_previous_;c.forward=forward_of(player_.yaw);
     c.module=track_.route(player_.position);c.next=next_polygon(track_,c.module);put(c);}
    for(size_t j=0;j<opponents_.size();++j) {
        if(j==self || opponent_bodies_[j].hidden) continue;
        const Opponent& o=opponents_[j];
        OtherCar c;c.slot=o.vehicle;c.position=o.car.position;c.previous=o.previous;c.forward=forward_of(o.car.yaw);
        c.module=o.robot.module();c.next=o.robot.next_module();put(c);
    }
    for(size_t k=0;k<8;++k) if(used[k] && int(k)!=me.vehicle) state.others[size_t(state.other_count++)]=by_slot[k];
}
void RaceSession::drive(const DriveInput& input) {
    player_previous_=player_.position;
    player_.steer=update_steering(player_.steer,tuning_,input);
    const SpeedState before=player_.speed;
    // 0x470218 (0x4701f5) runs at the previous update's end, from +0x8bb,
    // the last contact's flags, in the air too.
    update_grip(body_.grip,player_.vehicle,player_.surface);
    update_boost(body_.grip,body_.boost);
    // TurboSpeed scales the 0x6f2990 row the speed update reads.
    VehicleTuning row=tuning_;
    if(body_.turbo>0) {row.top_speed*=turbo_top;row.convergence*=turbo_convergence;}
    // 0x466f91: more than 6 updates in the air skips the throttle and brake
    // (the Wine sh_t1 jump holds its speed from airtime 7).
    if(player_.airtime<=6) update_speed(player_.speed,row,input,body_.grip,body_.skid.kick,body_.follow_lock!=0);
    update_brake_skid(body_.skid,input.accelerate,input.brake,before.speed,before.brake_state,player_.grounded);
    update_slide(body_.skid,player_.speed.speed,player_.steer,farol_speed_limit(player_.vehicle),body_.grip.friction,player_.grounded);
    move(player_,body_,tuning_,true);
    bomb_step(player_,body_);
    // MAUVAISE DIRECTION (string 2): heading against the section guide.
    if(phase_==Phase::Racing && player_.speed.speed>0.1f) {
        const Vec3 toward=track_.section_direction(player_.section,player_.position);
        if(dot(toward,forward_of(player_.yaw))<-0.3f) ++wrong_way_updates_;
        else wrong_way_updates_=0;
    } else if(player_.speed.speed<=0) wrong_way_updates_=0;
}
bool RaceSession::hit(int car,uint16_t type) {
    Body& b=car<0?body_:opponent_bodies_.at(size_t(car));
    if(!start_bomb(b.hit,type)) return false;
    ++b.frags; // 0x47d756: the activation counts the hit (+0x3a3)
    // 0x47d768: the car's +0x3e3 sound, when the sound slot +0x3c7 is free
    // (natively the 30-update timer +0x3e7, which it then sets).
    if(b.sound_timer==0) {b.sound_timer=30;++b.hit_sounds;}
    // 0x47d66f: the activation's hop and spin.
    const BombKick k=bomb_kick(type);
    if(k.hop) b.pushes.push_back({{{0,50,0}},0.9f});
    if(k.spin!=0) request_spin(b.spin,k.spin);
    return true;
}
void RaceSession::use_gadget(int icon,int car) {
    Body& b=car<0?body_:opponent_bodies_.at(size_t(car));
    const std::string who=car<0?"":" (robot "+std::to_string(car)+")";
    switch(icon) {
        case 8:
            // TurboSpeed 0x4d6cb0.
            b.turbo=1;b.effect=8;
            item_events_.push_back("used gadget 8 (TurboSpeed)"+who);
            break;
        case 10:
            b.shield=start_shield(b.hit);b.shielded=true;b.effect=10;
            item_events_.push_back("used gadget 10 (WPU_Shield)"+who);
            break;
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: {
            // FlyPower (init 0x4d4d10): a +0x48b slot lifting 62 (decay 0.92),
            // the gravity +0x98 x 0.17 (0x516228, the old value kept in +0x6c),
            // the top-speed row x 1.2 (0x512a74), airtime 4, +0x385 = 1, the
            // +0x3df sound. The per-vehicle branches (0x4d5354: wings,
            // propeller, parachute, boulder models) are drawn by RaceView for
            // vehicles 3, 4 and 6 only (WPU_Propeller's own update 0x4d8110 is empty).
            VehicleTuning& t=car<0?tuning_:opponents_.at(size_t(car)).tuning;
            RaceCar& rc=car<0?player_:opponents_.at(size_t(car)).car;
            if(b.fly>0) break;
            b.fly=1;b.fly_gravity=t.gravity;b.fly_top=t.top_speed;
            t.gravity*=0.17f;t.top_speed*=1.2f;
            // 0x4d4e19, 0x4d5172, 0x4d523f: vehicles 3, 4 and 6 halve it again.
            if(rc.vehicle==3 || rc.vehicle==4 || rc.vehicle==6) t.gravity*=0.5f;
            // 0x4d520f, 0x4d5309 -> 0x4d5337: vehicles 1 and 2 by 0.3 (0x512a04).
            if(rc.vehicle==1 || rc.vehicle==2) t.gravity*=0.3f;
            b.pushes.push_back({{{0,62,0}},0.92f});
            rc.airtime=4;b.effect=1;
            item_events_.push_back("used gadget "+std::to_string(icon)+" (FlyPower)"+who);
            break;
        }
        case 7:
            // MonsterTires (init 0x4d6380): the tyre scale +0x6c = 1, stage +0x68
            // and counter +0x64 = 0. The original sets +0x385 = 0 (natively 7).
            // Its physics (+0x15b = 0x52b800[vehicle], the contact spread, and
            // the wheel records +0x44b = 24) are not ported.
            b.monster=1;b.monster_count=0;b.monster_scale=1;b.effect=7;
            item_events_.push_back("used gadget 7 (MonsterTires)"+who);
            break;
        case 9:
            // WPU_FireSiren (init 0x4dff20): +0x385 = 9 for m_nLifeTime (150).
            b.siren=150;b.effect=9;
            item_events_.push_back("used gadget 9 (WPU_FireSiren)"+who);
            break;
        default:
            item_events_.push_back("used gadget "+std::to_string(icon)+" (effect not ported)"+who);
            break;
    }
}
void RaceSession::update_gadgets() {
    // 0x4d71e0: the counter (turbo - 1) starts at 0; the rows come back
    // when it reaches 61.
    const auto one=[&](Body& b) {
        if(b.turbo>0 && ++b.turbo>turbo_updates) {
            b.turbo=0;
            if(b.effect==8) b.effect=-1;
        }
        if(b.shielded) {
            update_shield(b.shield,b.hit);
            if(b.shield.done) {b.shielded=false;b.effect=-1;}
        }
    };
    one(body_);
    for(Body& b:opponent_bodies_) one(b);
    // WPU_FireSiren (update 0x4e0130): each update the query 0x405051 (angle
    // 2 pi, flags 7, range the action's +0x2c) takes every other car in the
    // user's section within the range on each axis (0x4a7e70), whose speed is
    // x m_rSpeedMod (0.9); past the life the effect ends (+0x385 = -1). The
    // use 0x4d2970 sets +0x2c/+0x30 from the icon's WPowerUpSprite m_range
    // and m_visangle (icon 9: 5000, 2 pi).
    const auto siren=[&](const RaceCar& user,Body& b) {
        if(b.siren<=0) return;
        if(--b.siren<0 || b.siren==0) {b.siren=0;if(b.effect==9) b.effect=-1;return;}
        const float range=aims_[9].range;
        const auto within=[&](const RaceCar& c) {
            if(!(range>0)) return true; // 0x4a7eca: no distance test
            for(size_t k=0;k<3;++k) if(!(std::abs(c.position[k]-user.position[k])<range)) return false;
            return true;
        };
        const auto slow=[&](RaceCar& c,const Body& cb){if(&c!=&user && !cb.hidden && c.section==user.section && within(c)) c.speed.speed*=0.9f;};
        slow(player_,body_);
        for(size_t i=0;i<opponents_.size();++i) slow(opponents_[i].car,opponent_bodies_[i]);
    };
    // MonsterTires' update (0x4d6560, table 0x4d6918; general.wgm 0x2c0's
    // members): the counter +0x64 counts; stage 0 grows the scale by
    // m_tomaxscalediff (0.2) until m_tomaxframenum (10), 1 by m_tonormscalediff
    // (-0.1) until m_tonormframenum (3), 2 waits m_innormframenum - 30 (270,
    // 0x4d661e), 3 adds m_tominscalediff (-0.4) until m_tominframenum (5); each
    // limit resets the counter. Stage 4 ends it (0x4d6659): 289 updates.
    const auto tyres=[&](Body& b){
        if(b.monster<=0) return;
        static const int limit[4]={10,3,300-30,5};
        static const float step[4]={0.2f,-0.1f,0,-0.4f};
        const int stage=b.monster-1;
        if(stage==4) {b.monster=0;b.monster_scale=1;if(b.effect==7) b.effect=-1;return;}
        ++b.monster_count;
        b.monster_scale+=step[stage];
        if(b.monster_count>=limit[stage]) {b.monster_count=0;++b.monster;}
    };
    // FlyPower (update 0x4d5510): the gravity doubles at the 20th update,
    // then the per-vehicle branch (table 0x4d5b8c) decides the end (0x4d5a56:
    // the gravity and the top speed come back):
    //   vehicles 0 and 1 (0x4d59ed): once down (airtime < 3);
    //   vehicle 5 (0x4d5555): at update 10 the gravity halves again and the
    //     airtime is set to 4 (0x4d56a2); past update 17, once down;
    //   the others (0x4d5a2e): past update 95, once down.
    const auto fly=[&](RaceCar& c,Body& b,VehicleTuning& t) {
        if(b.fly<=0) return;
        ++b.fly;
        if(b.fly==20) t.gravity*=2;
        if(c.vehicle==5 && b.fly==10) {t.gravity*=0.5f;c.airtime=4;}
        const int last=c.vehicle==0 || c.vehicle==1?0:c.vehicle==5?17:95;
        if(b.fly>last && c.airtime<3) {t.gravity=b.fly_gravity;t.top_speed=b.fly_top;b.fly=0;if(b.effect==1) b.effect=-1;}
    };
    // 0x469a1c..0x469a7a: vehicle 5's turret, with no weapon running (+0x389
    // = -1), comes back to 0 by 3 degrees (0x512a24) an update.
    const auto turret=[](const RaceCar& c,Body& b) {
        if(c.vehicle!=5 || b.weapon.id>=0) return;
        if(b.turret<-0.05235988f) b.turret+=0.05235988f;
        else if(b.turret>0.05235988f) b.turret-=0.05235988f;
        else b.turret=0;
    };
    turret(player_,body_);
    for(size_t i=0;i<opponents_.size();++i) turret(opponents_[i].car,opponent_bodies_[i]);
    fly(player_,body_,tuning_);
    for(size_t i=0;i<opponents_.size();++i) fly(opponents_[i].car,opponent_bodies_[i],opponents_[i].tuning);
    tyres(body_);for(Body& b:opponent_bodies_) tyres(b);
    siren(player_,body_);
    for(size_t i=0;i<opponents_.size();++i) siren(opponents_[i].car,opponent_bodies_[i]);
}
void RaceSession::bomb_step(RaceCar& car,Body& b) {
    const BombStep s=update_bomb(b.hit);
    if(s.smoke) b.fx.hit_smoke(car.frame(),s.flip);
    if(s.pool) b.hit.pool.car=car.frame();
    if(s.tank_chunks) b.fx.tank_chunks(car.frame());
    if(s.tank_axes || s.tank_position) {
        // 0x47e2c8..0x47e60d: the car's frame (0x404def): its axes (0x405065,
        // 0x401f64, 0x4024fa) while following, its position (0x4038f5).
        const Matrix4 f=car.frame();
        auto& m=b.hit.tank.frame;
        if(s.tank_axes) for(size_t i=0;i<12;++i) m[i]=f[i];
        for(size_t i=12;i<16;++i) m[i]=f[i];
    }
    if(s.hold) {car.speed.speed=0;b.motion=-5;}
    if(s.boost) b.boost=20;
}
void RaceSession::use_weapon(int icon,int car) {
    Body& b=car<0?body_:opponent_bodies_.at(size_t(car));
    const RaceCar& c=car<0?player_:opponents_.at(size_t(car)).car;
    WeaponEffect& w=b.weapon;
    switch(icon) {
        case 17: drop_mine(car,false,true);break; // WPU_OilSpill
        case 18: {                                 // WPU_BeeHive
            const RaceCar& d=car<0?player_:opponents_.at(size_t(car)).car;
            mines_.push_back(drop_hive(d.position,d.yaw,car));
            const int k=slot_section(mines_.back().position);mines_.back().section=k>=0?k:d.section;
            break;
        }
        case 20: case 22: {
            // 0x4e23fc: the target is the query's car (0x403d69 from the user,
            // flags 1; range and angle the action's +0x2c/+0x30, set by the use
            // 0x4d2970 from the icon's WPowerUpSprite: 5000 and 60 degrees for
            // both), else the user; the object starts ahead of it; the user is
            // never stopped (+0x70).
            const ItemAim& a=aims_[size_t(icon)];
            const int t=find_target(c.position,c.yaw,c.section,a.angle,int(a.range),car);
            const RaceCar& base=t==-2?c:t<0?player_:opponents_[size_t(t)].car;
            barriers_.push_back(drop_barrier(base.position,base.yaw,car,icon==20));
            const int k=slot_section(barriers_.back().position);
            barriers_.back().section=k>=0?k:base.section;barriers_.back().filed=k>=0;
            barriers_.back().stopped=1u<<unsigned(c.vehicle&31);
            break;
        }
        case 11: {
            // WPU_BootKicker (0x4d8830): +0x389 = 0xb while the arm runs; the
            // target 0x403d69(user, frame, section, range + 1000, angle, 0, 0, 2).
            Kick k;k.owner=car;k.backward=use_backward_;
            const float yaw=use_backward_?wrap_angle(c.yaw+pi):c.yaw;
            k.target=find_target(c.position,yaw,c.section,1.0471976f,2000+1000,car);
            k.angle=k.backward?pi:0;
            kicks_.push_back(k);
            w={0xb,100000,-1,icon};
            break;
        }
        case 12: {
            // WPU_Flipper (0x4d9a20): +0x389 = 0xb while the arm runs; the
            // target as the kicker's (range + 1000, 60 degrees).
            Flip f;f.owner=car;f.backward=use_backward_;
            const float yaw=use_backward_?wrap_angle(c.yaw+pi):c.yaw;
            f.target=find_target(c.position,yaw,c.section,1.0471976f,2000+1000,car);
            f.angle=f.backward?pi:0;
            flips_.push_back(f);
            w={0xb,100000,-1,icon};
            break;
        }
        case 19: drop_mine(car);break;
        case 23: drop_mine(car,true);break;
        // 0x4da500/0x4da5a0: +0x389 = 0x19 for 40 updates, firing at once.
        case 25: w={0x19,40,-1,icon};fire(icon,car,use_backward_);break;
        // 0x4dab40/0x4dac70: 0xe for 33 updates, firing when 28 are left.
        case 14: w={0xe,33,28,icon};break;
        // 0x4db980/0x4dba50: 0x10, firing on the first update, then 20.
        case 16: w={0x10,21,-1,icon};fire(icon,car,use_backward_);break;
        // 0x4dc110/0x4dc180: fires and ends at once.
        case 15: fire(icon,car,use_backward_);break;
        // 0x4db280: +0x389 = 0xd while it runs; one paint shot (provisional:
        // m_nWaitShot 5 and the shot count are not traced).
        case 13: w={0xd,20,-1,icon};fire(icon,car,use_backward_);break;
        case 21: {
            // 0x4e15f0: 400 above the car, along its forward axis at spd.
            Storm st;st.owner=car;st.position=c.position;st.position[1]+=storm_height;
            const Vec3 f=forward_of(c.yaw);
            for(size_t i=0;i<3;++i) st.velocity[i]=f[i]*storm_speed;
            st.mask=1u<<unsigned(c.vehicle);st.backward=use_backward_;
            // 0x4a6b30: the slot row holding it, else the thrower's (native).
            {const int k=slot_section(st.position);st.section=k>=0?k:c.section;}
            storms_.push_back(st);
            break;
        }
        case 24: {
            // 0x4dcac0: aimed at a car in view of the thrower (not itself);
            // with +0x5c the frame is turned by pi first (0x4dcaff).
            const float yaw=use_backward_?wrap_angle(c.yaw+pi):c.yaw;
            const Vec3 f=forward_of(yaw);
            Vec3 from=c.position;from[1]+=100;
            const int t=find_target(from,yaw,c.section,tnt_angle,int(tnt_range),car);
            const Vec3* at=t==-2?nullptr:t<0?&player_.position:&opponents_[size_t(t)].car.position;
            tnts_.push_back(throw_tnt(c.position,f,car,at));
            {const int k=slot_section(tnts_.back().position);tnts_.back().section=k>=0?k:c.section;}
            break;
        }
        default:
            item_events_.push_back("used weapon "+std::to_string(icon)+" (effect not ported)"+(car<0?"":" (robot "+std::to_string(car)+")"));
            return;
    }
    if(w.id>=0) w.backward=use_backward_;
    item_events_.push_back("used weapon "+std::to_string(icon)+(use_backward_?" backward":"")+(car<0?"":" (robot "+std::to_string(car)+")"));
}
void RaceSession::fire(int icon,int owner,bool backward) {
    Projectile p;
    if(!breath_weapon(icon,p)) return;
    const RaceCar& c=owner<0?player_:opponents_.at(size_t(owner)).car;
    // The clone takes the car's frame, moved 100 along its forward axis
    // (0x43b140), and leaves along it at ABSspd (0x474e30). With the
    // action's +0x5c the frame is first turned by pi about y (0x403279 with
    // 0x40490fda: the missile 0x4da605, the tank gun 0x4dbbaa, the boomerang
    // 0x4dc213; the breath's head frame 0x4dacea), so it leaves backward.
    const float yaw=backward?wrap_angle(c.yaw+pi):c.yaw;
    const Vec3 f=forward_of(yaw);
    for(size_t i=0;i<3;++i) {p.position[i]=c.position[i]+f[i]*100;p.velocity[i]=f[i]*p.p.speed;}
    // 0x4a6b30: registered in the first slot row whose box holds it (the
    // same boxes as 0x4a6d20), whose section becomes its group; none: the
    // tracker is not registered (native: the thrower's section).
    p.owner=owner;
    {const int k=slot_section(p.position);p.section=k>=0?k:c.section;}
    // The tank gun and the boomerang pick and aim at their target at once
    // (0x474c80, 0x474d50); the others look for one every update.
    if(icon==16 || icon==15) {
        p.target=find_target(p.position,yaw,p.section,p.p.angle,int(p.p.range),p.owner);
        if(p.target!=-2) aim_projectile(p,p.target<0?player_.position:opponents_[size_t(p.target)].car.position);
    }
    // 0x4dbc06..0x4dbc76: the tank gun turns the car's turret +0x3ab to the
    // shot's heading less the (turned) frame's (0x496f90: wrapped into
    // (-pi, pi]) when it has a target, else to pi (backward) or 0.
    if(icon==16) {
        Body& ob=owner<0?body_:opponent_bodies_.at(size_t(owner));
        ob.turret=p.target!=-2?wrap_angle(std::atan2(-p.velocity[0],p.velocity[2])-yaw):backward?pi:0.f;
    }
    // 0x4ea7d0: every tail point starts at the clone, with its axes x 1.
    {
        RaceCar at;at.position=p.position;at.yaw=std::atan2(-p.velocity[0],p.velocity[2]);
        const Matrix4 f=at.frame();
        for(auto& t:p.tail) t={p.position,{{f[4],f[5],f[6]}},{{f[0],f[1],f[2]}},1};
    }
    projectiles_.push_back(p);
}
void RaceSession::update_weapons() {
    const auto one=[&](Body& b,int car) {
        WeaponEffect& w=b.weapon;
        if(w.id>=0) {
            if(--w.timer==w.fire_at) fire(w.icon,car,w.backward);
            if(w.timer<=0) w=WeaponEffect{};
        }
    };
    one(body_,-1);
    for(size_t i=0;i<opponent_bodies_.size();++i) one(opponent_bodies_[i],int(i));
    const auto car_at=[&](int id)->const RaceCar& {return id<0?player_:opponents_.at(size_t(id)).car;};
    for(Projectile& p:projectiles_) {
        if(p.done) continue;
        // 0x475106: 0x474c80 clears the target and looks again every
        // update, about the projectile's heading (its frame follows the
        // velocity, 0x475822). Its section is the route's under it.
        {
            // The tail's points cross with it (provisional: the original's are
            // world positions that 0x4a23c2 does not move).
            if(const int k=carry_across_seam(p.position,p.section,{&p.velocity})) {
                const Matrix4& m=k>0?track_.wrap():track_.unwrap();
                for(auto& t:p.tail) {t.position=transform_point(m,t.position);t.up=rotate(m,t.up);t.side=rotate(m,t.side);}
            }
            const RaceTrack::Route r=track_.route(p.position);
            if(r.hit) p.section=r.section;
            p.target=find_target(p.position,std::atan2(-p.velocity[0],p.velocity[2]),p.section,p.p.angle,int(p.p.range),p.owner);
        }
        const Vec3* target=p.target!=-2?&car_at(p.target).position:nullptr;
        // FWDscan ahead along the horizontal heading.
        const float h=std::sqrt(p.velocity[0]*p.velocity[0]+p.velocity[2]*p.velocity[2]);
        float ax=p.position[0],az=p.position[2];
        if(h>0) {ax+=p.velocity[0]/h*p.p.scan;az+=p.velocity[2]/h*p.p.scan;}
        TrackSurface ahead=ground(ax,az,p.position[1]+p.p.height,p.position[1]-2*p.p.height);
        float reach=h>0?p.p.scan/p.p.speed:1;
        if(!ahead.hit) {ahead=ground(p.position[0],p.position[2],p.position[1],p.position[1]-2*p.p.height);reach=1;}
        steer_projectile(p,target,ahead.hit,ahead.y,reach);
        // 0x4755fe: the move ray (mask 0x18000ff) stops it on the ground.
        bool grounded=false;
        const TrackSurface g=ground(p.position[0]+p.velocity[0],p.position[2]+p.velocity[2],p.position[1],p.position[1]+std::min(0.f,p.velocity[1])-1);
        if(g.hit && g.y>=p.position[1]+p.velocity[1]) grounded=true;
        // 0x475694..0x4757c6: n sub-steps of ABSspd / the sphere radius, each
        // testing the cars' spheres (0x4040e8) but the owner's.
        const int n=std::max(1,int(p.p.speed/p.radius));
        for(int k=0;k<n && p.struck==-2;++k) {
            for(size_t i=0;i<3;++i) p.position[i]+=p.velocity[i]/float(n);
            if(p.owner!=-1 && spheres_touch(player_.position,p.position,car_sphere+p.radius)) p.struck=-1;
            if(robots_enabled_) for(size_t i=0;i<opponents_.size() && p.struck==-2;++i)
                if(int(i)!=p.owner && !opponent_bodies_[i].hidden && spheres_touch(opponents_[i].car.position,p.position,car_sphere+p.radius)) p.struck=int(i);
        }
        p.travelled+=p.p.speed;
        // WRocketTail runs after WBreath (the DragonBreath's action order).
        {
            RaceCar at;at.position=p.position;at.yaw=std::atan2(-p.velocity[0],p.velocity[2]);
            const Matrix4 f=at.frame();
            update_tail(p,{{f[4],f[5],f[6]}},{{f[0],f[1],f[2]}});
        }
        // 0x475908: a brick the projectile touches takes the hit (slot +0x34:
        // WChiselCrossover, WRolling), and the projectile flies on.
        objects_.projectile_hit(p.position,p.radius);
        if(p.struck!=-2 || grounded || p.travelled>=p.p.travel) {
            // 0x4762c0: the struck car takes the DragonBreath's type.
            if(p.struck!=-2 && hit(p.struck,p.type)) item_events_.push_back("projectile hit car "+std::to_string(p.struck));
            p.done=true;
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(),projectiles_.end(),[](const Projectile& p){return p.done;}),projectiles_.end());
}
int RaceSession::find_target(const Vec3& from,float heading,int section,float angle,int range,int owner) const {
    const int last=int(track_.sections())-1;
    int best=-2;int64_t best_d=-1;
    const auto test=[&](int id,const RaceCar& c,const Body& b) {
        if(id==owner || b.hit.active() || b.hidden) return;
        Vec3 at=c.position;
        if(section==0 && c.section==last && last>0) at=transform_point(track_.wrap(),at);
        else if(section==last && c.section==0 && last>0) at=transform_point(track_.unwrap(),at);
        const int64_t d=target_distance({{at[0]-from[0],at[1]-from[1],at[2]-from[2]}},heading,angle,range);
        if(d>=0 && (best_d<0 || d<=best_d)) {best_d=d;best=id;}
    };
    test(-1,player_,body_);
    if(robots_enabled_) for(size_t i=0;i<opponents_.size();++i) test(int(i),opponents_[i].car,opponent_bodies_[i]);
    return best;
}
void RaceSession::update_tnts() {
    for(Tnt& t:tnts_) {
        if(t.done) continue;
        if(--t.life<=0) {t.done=true;continue;}
        carry_across_seam(t.position,t.section,{&t.velocity});
        // 0x4dced1: gravity, then sub-steps of speed / the sphere radius,
        // each testing the cars but the thrower's (0x4040e8).
        t.velocity[1]-=tnt_gravity;
        const Vec3 start=t.position;
        const int n=std::max(1,int(std::min(t.speed/tnt_radius,1e6f)));
        int struck=-2;
        for(int k=0;k<n && struck==-2;++k) {
            for(size_t i=0;i<3;++i) t.position[i]+=t.velocity[i]/float(n);
            if(t.owner!=-1 && spheres_touch(player_.position,t.position,car_sphere+tnt_radius)) struck=-1;
            if(robots_enabled_) for(size_t i=0;i<opponents_.size() && struck==-2;++i)
                if(int(i)!=t.owner && !opponent_bodies_[i].hidden && spheres_touch(opponents_[i].car.position,t.position,car_sphere+tnt_radius)) struck=int(i);
        }
        if(struck!=-2) {
            // 0x4dd468: it explodes on the car, which takes type 0x40.
            if(hit(struck,0x40)) item_events_.push_back("TNT hit car "+std::to_string(struck));
            blasts_.push_back({t.position,0});
            t.done=true;continue;
        }
        // 0x4dd077: the ray from the start to the new position (mask
        // 0x1e000ff); natively the ground under the new position.
        const TrackSurface g=ground(t.position[0],t.position[2],std::max(start[1],t.position[1]),std::min(start[1],t.position[1])-1);
        if(g.hit && g.y>=t.position[1]) {
            const Vec3 at{{t.position[0],g.y,t.position[2]}};
            const RaceTrack::Route r=track_.route(at);
            if(r.hit) t.section=r.section;
            const int view=find_target(at,std::atan2(-t.velocity[0],t.velocity[2]),t.section,tnt_angle,int(tnt_range),-2);
            const Vec3* target=view==-2?nullptr:view<0?&player_.position:&opponents_[size_t(view)].car.position;
            if(!bounce_tnt(t,at,g.normal,target)) {t.done=true;blasts_.push_back({at,0});} // the third bounce explodes
        }
    }
    tnts_.erase(std::remove_if(tnts_.begin(),tnts_.end(),[](const Tnt& t){return t.done;}),tnts_.end());
}
void RaceSession::update_storms() {
    const auto car_at=[&](int id)->const RaceCar& {return id<0?player_:opponents_.at(size_t(id)).car;};
    for(Storm& st:storms_) {
        if(st.done) continue;
        if(const int k=carry_across_seam(st.position,st.section,{&st.velocity}))
            st.waypoint=transform_point(k>0?track_.wrap():track_.unwrap(),st.waypoint);
        switch(st.state) {
            case 0: {
                // 0x4e1410: on a new route polygon, head for its first
                // forward link (backward when the key is held, not ported),
                // 400 up (provisional).
                const RaceTrack::Route r=track_.route(st.position);
                if(r.hit && (r.polygon!=st.polygon || r.section!=st.section)) {
                    st.polygon=r.polygon;st.section=r.section;
                    const RouteNode node=track_.route_node(r.section,r.record,r.polygon);
                    // 0x4e14e0: with the action's +0x5c the first backward
                    // link (after the forward ones); none keeps the waypoint.
                    const std::vector<RouteLink>& links=st.backward?node.backward:node.forward;
                    if(!links.empty()) {
                        st.waypoint=track_.route_point(r.section,links.front().point);
                        st.waypoint[1]+=storm_height;
                        Vec3 d{{st.waypoint[0]-st.position[0],st.waypoint[1]-st.position[1],st.waypoint[2]-st.position[2]}};
                        const float l=std::sqrt(dot(d,d));
                        if(l>0) for(size_t i=0;i<3;++i) st.velocity[i]=d[i]/l*storm_speed;
                    }
                }
                for(size_t i=0;i<3;++i) st.position[i]+=st.velocity[i];
                if(++st.counter>storm_life) {st.state=2;st.timer=40;break;}
                // 0x4a71e0 (angle pi, range 700, flag 1: racers only): the
                // nearest idle racer but the thrower; when its vehicle bit is
                // already set the cloud strikes nobody this update.
                const RaceTrack::Route here=track_.route(st.position);
                if(here.hit) st.section=here.section;
                const int best=find_target(st.position,std::atan2(-st.velocity[0],st.velocity[2]),st.section,pi,int(storm_range),st.owner);
                if(best==-2) break;
                if(st.mask&(1u<<unsigned(best<0?player_.vehicle:opponents_[size_t(best)].vehicle))) break;
                Body& b=best<0?body_:opponent_bodies_[size_t(best)];
                st.target=best;st.state=1;st.timer=b.hit.guard!=1?2:30;
                st.mask|=1u<<unsigned(best<0?player_.vehicle:opponents_[size_t(best)].vehicle);
                // The type is written whatever runs (provisional here: a
                // running hit keeps its own).
                if(hit(best,0x400)) item_events_.push_back("storm struck car "+std::to_string(best));
                break;
            }
            case 1: {
                if(st.timer<1) {st.state=0;break;}
                --st.timer;
                // Over the car, 400 up, at most spd per update.
                const Vec3& at=car_at(st.target).position;
                Vec3 d{{at[0]-st.position[0],at[1]+storm_height-st.position[1],at[2]-st.position[2]}};
                const float l=std::sqrt(dot(d,d));
                if(l>=storm_speed) for(size_t i=0;i<3;++i) st.position[i]+=d[i]/l*storm_speed;
                break;
            }
            default:
                if(--st.timer<1) {st.done=true;break;}
                st.velocity[1]+=4;
                for(size_t i=0;i<3;++i) st.position[i]+=st.velocity[i];
                st.angle=float(st.timer)*0.025f; // 0x4e196a
                break;
        }
    }
    storms_.erase(std::remove_if(storms_.begin(),storms_.end(),[](const Storm& s){return s.done;}),storms_.end());
}
void RaceSession::set_model_colours(int vehicle,const std::array<float,3>& lo,const std::array<float,3>& hi) {
    const auto set=[&](Body& b){b.hit.colour_lo=lo;b.hit.colour_hi=hi;};
    if(player_.vehicle==vehicle) set(body_);
    for(size_t i=0;i<opponents_.size();++i) if(opponents_[i].vehicle==vehicle) set(opponent_bodies_[i]);
}
void RaceSession::drop_mine(int car,bool detour,bool oil) {
    const RaceCar& c=car<0?player_:opponents_.at(size_t(car)).car;
    mines_.push_back(detour?drop_detour(c.position,c.yaw,car,false):oil?drop_oil(c.position,c.yaw,car):drop_landmine(c.position,c.yaw,car));
    // The detour's query range is its action's +0x2c (0x4e373a), the icon's
    // WPowerUpSprite m_range set by the use 0x4d2970 (icon 23: 5000); the
    // strict 150 test (0x4e37d0) follows.
    if(detour) mines_.back().trap=aims_[23].range;
    const int k=slot_section(mines_.back().position);
    mines_.back().section=k>=0?k:c.section;
}
int RaceSession::slot_section(const Vec3& p) const {
    // 0x4a6d20 (WPU_OilSpill, BeeHive, Landmine, FallingTree, BrickWall and
    // Detour): the dropped object goes into the power-up table of the first
    // enabled slot whose row has a brick (+0x58) and whose box (the row's
    // +0x1c..+0x30, copied to the slot's +0x78 by 0x4a89c0) holds its x and z.
    // The slots are each tracker's previous, current and next rows, player 1's
    // first (0x49dc90 at the start, 0x4a0880 on a move back; 3 x 0x94 bytes
    // per player from +0x830, six with two players). On a hit the group
    // (+0x104) is that row's section (0x49c890).
    // Not ported: none holding it, the original leaves the group 0xffff
    // (always drawn); natively the callers keep the dropper's section.
    const int n=int(track_.sections());
    if(n<=0) return -1;
    const auto look=[&](int s)->int {
        if(s<0) s=0;
        for(const int d:{-1,0,1}) {const int k=((s+d)%n+n)%n;if(track_.row_box_holds(k,p[0],p[2])) return k;}
        return -1;
    };
    int k=look(player_.section);
    if(k<0 && two_players()) k=look(opponents_[0].car.section);
    return k;
}
void RaceSession::update_blasts() {
    for(Blast& b:blasts_) ++b.age;
    blasts_.erase(std::remove_if(blasts_.begin(),blasts_.end(),[](const Blast& b){return b.age>22;}),blasts_.end());
}
void RaceSession::update_barriers() {
    for(Barrier& w:barriers_) {
        if(w.fade>0) {if(--w.fade<=0) w.done=true;continue;}
        if(--w.life<=0) {w.fade=w.scale_num;continue;}
        if(!w.landed) {
            const TrackSurface g=ground(w.position[0],w.position[2],w.position[1],w.position[1]-100);
            // Filed in a slot row (0x4a6d20), it keeps that group; otherwise
            // the ground's section (native, provisional).
            if(g.hit) {w.position[1]=g.y;w.landed=true;if(g.section>=0 && !w.filed) w.section=g.section;}
            else w.position[1]-=100;
            continue;
        }
        // 0x4a7e70: |dx|, |dy|, |dz| < 300 in the object's section.
        const auto stop=[&](RaceCar& c,const Body& b) {
            const uint32_t bit=1u<<unsigned(c.vehicle&31);
            if((w.stopped&bit) || b.hidden || c.section!=w.section) return;
            for(size_t i=0;i<3;++i) if(!(std::abs(c.position[i]-w.position[i])<300)) return;
            c.speed.speed=0;w.stopped|=bit;
            item_events_.push_back(std::string(w.tree?"falling tree":"brick wall")+" stopped vehicle "+std::to_string(c.vehicle));
        };
        stop(player_,body_);
        for(size_t i=0;i<opponents_.size();++i) stop(opponents_[i].car,opponent_bodies_[i]);
    }
    barriers_.erase(std::remove_if(barriers_.begin(),barriers_.end(),[](const Barrier& b){return b.done;}),barriers_.end());
}
void RaceSession::update_strikes() {
    for(Strike& s:strikes_) {
        if(--s.wait>0) continue;
        // 0x4d9f84: only a car with no hit running (+0x36d +0x2c = 0).
        if(hit(s.target,s.type)) {
            const int user=s.owner<0?player_.vehicle:opponents_[size_t(s.owner)].vehicle;
            const int target=s.target<0?player_.vehicle:opponents_[size_t(s.target)].vehicle;
            voice_events_.push_back({user,6});voice_events_.push_back({target,5});
            item_events_.push_back((s.type==0x100?"boot kicker":"flipper")+std::string(" hit car ")+std::to_string(s.target));
        }
        s.wait=-1000;
    }
    strikes_.erase(std::remove_if(strikes_.begin(),strikes_.end(),[](const Strike& s){return s.wait<=-1000;}),strikes_.end());
}
void RaceSession::update_kicks() {
    const auto car_at=[&](int id)->const RaceCar& {return id<0?player_:opponents_.at(size_t(id)).car;};
    const auto body_at=[&](int id)->Body& {return id<0?body_:opponent_bodies_.at(size_t(id));};
    const auto turned_y=[](float a) {Matrix4 r=identity_matrix();const float c=std::cos(a),s=std::sin(a);r[0]=c;r[2]=s;r[8]=-s;r[10]=c;return r;}; // 0x43c380
    for(Kick& k:kicks_) {
        if(k.done) continue;
        const RaceCar& user=car_at(k.owner);
        // 0x4d8710: the brick takes the car model's frame (+0x14), moved to
        // the offset in it (0x43b880).
        const Matrix4 model=initial_vehicle_model_frame(user.frame());
        const Vec3 off=k.backward?Vec3{{0,20,-160}}:Vec3{{0,20,190}};
        k.brick=model;
        for(size_t r=0;r<3;++r) k.brick[12+r]=model[r]*off[0]+model[4+r]*off[1]+model[8+r]*off[2]+model[12+r];
        const Vec3 from{{k.brick[12],k.brick[13],k.brick[14]}};
        // 0x4d88fc: the GDHModel's local matrix is reset to the identity, then
        // only turned (0x43c380 writes the axes).
        const auto boot_frame=[&]{return multiply_matrix(k.brick,turned_y(k.angle));};
        if(k.state==0) {
            // 0x4d8b4b..0x4d8bf9: the aim is the target's yaw in the brick's
            // frame (0x43bb00), within +-m_angle (1.047) of 0, or of pi
            // backward; none: pi or 0.
            float a=k.backward?pi:0;
            if(k.target!=-2) {
                const Vec3& t=car_at(k.target).position;
                const Vec3 d{{t[0]-from[0],t[1]-from[1],t[2]-from[2]}};
                const float lx=d[0]*k.brick[0]+d[1]*k.brick[1]+d[2]*k.brick[2];
                const float lz=d[0]*k.brick[8]+d[1]*k.brick[9]+d[2]*k.brick[10];
                a=std::atan2(-lx,lz);
                const float lim=1.0471976f;
                if(!k.backward) a=std::clamp(a,-lim,lim);
                else if(a>0) a=std::max(a,pi-lim); else a=std::min(a,lim-pi);
            }
            k.angle=a;
            // 0x4d8d84: the speed x 1.1 (0x516450); 0x4d8d9c: substeps of
            // the sphere's radius (100), at least one.
            k.vel[2]*=1.1f;
            const int n=std::max(1,int(std::max(1.f,k.vel[2]/100.f)));
            const Matrix4 g=boot_frame();
            bool struck=false;
            for(int i=0;i<n && !struck;++i) {
                for(size_t j=0;j<3;++j) k.ext[j]+=k.vel[j]/float(n);
                const Vec3 boot=transform_point(g,k.ext);
                // 0x4040e8: the first other car whose sphere (100) touches
                // the boot's (100).
                const auto touches=[&](int id){return id!=k.owner && !(id>=0 && opponent_bodies_[size_t(id)].hidden) && spheres_touch(boot,car_at(id).position,car_sphere+100);};
                int hit_car=-2;
                if(touches(-1)) hit_car=-1;
                for(size_t r=0;r<opponents_.size() && hit_car==-2;++r) if(touches(int(r))) hit_car=int(r);
                if(hit_car==-2) continue;
                struck=true;
                // 0x4d8f04..0x4d8fa1: a +0x48b slot along the unit direction
                // brick -> car x 80 (0x512a3c), 40 up, decay 0.8; then the
                // hit type 0x100 if its bomba is idle (0x404edf) and voice 6.
                const Vec3& t=car_at(hit_car).position;
                Vec3 d{{t[0]-from[0],t[1]-from[1],t[2]-from[2]}};
                const float l=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
                if(l>0) for(float& v:d) v/=l;
                body_at(hit_car).pushes.push_back({{{d[0]*80,40,d[2]*80}},0.8f});
                if(hit(hit_car,0x100)) item_events_.push_back("boot kicker hit car "+std::to_string(hit_car));
                voice_events_.push_back({user.vehicle,6});
                // 0x4d9045..0x4d90d2: back to the hit, less 100.
                k.ext={{0,0,l-100}};k.vel={{0,0,-100}};k.state=1;k.count=10;
            }
            // 0x4d90d5: past the range (2000) it holds there.
            if(!struck && k.ext[2]>2000) {k.ext[2]=2000;k.vel={{0,0,-100}};k.state=1;k.count=10;}
        } else if(k.state==1) {
            // 0x4d8cc9: the bounce, x -0.8 (0x51644c) for 10 updates; then
            // back at (0, 10, spd x -0.7) (0x516448).
            for(size_t j=0;j<3;++j) k.ext[j]+=k.vel[j];
            k.vel[2]*=-0.8f;
            if(--k.count<=0) {k.state=2;k.vel={{0,10,-140}};}
        } else {
            // 0x4d8c18: home, x 1.1; past z < 0 the arm ends (+0x389 = -1).
            for(size_t j=0;j<3;++j) k.ext[j]+=k.vel[j];
            k.vel[2]*=1.1f;
            if(k.ext[2]<0) {k.done=true;WeaponEffect& w=body_at(k.owner).weapon;if(w.icon==11) w=WeaponEffect{};continue;}
            if(k.ext[1]>k.ext[2]) k.ext[1]=k.ext[2];
        }
    }
    kicks_.erase(std::remove_if(kicks_.begin(),kicks_.end(),[](const Kick& k){return k.done;}),kicks_.end());
}
void RaceSession::slip(int car) {
    RaceCar& c=car<0?player_:opponents_.at(size_t(car)).car;
    Body& b=car<0?body_:opponent_bodies_.at(size_t(car));
    // A +0x48b slot (0x40400c, type 3): the force is the front probe less the
    // rear one (+0x40b - +0x423) x the speed, decaying by 0.96; the turn
    // 1.31 rad (0x3fa78d36) decaying by 0.897 (0x3f65a1cb). Then speed 0,
    // the motion counter -59 (0x4de107), voice event 5 and the +0x3cb sound.
    Body::Push p;
    const Vec3 f=forward_of(c.yaw); // horizontal (the probes carry the pitch; provisional)
    const float l=probe_span(c.speed.speed)*c.speed.speed;
    for(size_t i=0;i<3;++i) p.force[i]=f[i]*l;
    p.decay=0.96f;p.yaw=1.3089969f;p.yaw_decay=0.897f;
    b.pushes.push_back(p);
    c.speed.speed=0;b.motion=-59;
    voice_events_.push_back({c.vehicle,5});
    ++b.oil_sounds;
}
void RaceSession::update_mines() {
    for(Landmine& m:mines_) {
        // 0x4df681: the life runs out into the explosion state.
        // WPU_OilSpill (0x4ddd01): then the spot stays m_nScaleNum (+0x2c, 10)
        // updates before it goes.
        if(--m.life<=0 && m.state!=0xffff) {m.state=0xffff;m.fade=m.scale_num;}
        switch(m.state) {
            case 0: {
                // 0x4df6dd: a ray m_rGravity down (mask 0x8000fe); a hit
                // lays the mine on the ground, locked until 15 updates old.
                const TrackSurface g=ground(m.position[0],m.position[2],m.position[1],m.position[1]-m.gravity);
                if(g.hit) {
                    m.position[1]=g.y;m.normal=g.normal;
                    m.state=m.life<m.lifetime-landmine_unlock?2:1;
                    // 0x4e3585: the detour sign faces one way or the other.
                    if(m.detour) {
                        const bool left=random_.next()%100<50;
                        m.type=left?0x1000:0x800;
                        if(!left) m.yaw=wrap_angle(m.yaw+pi);
                    }
                } else m.position[1]-=m.gravity;
                break;
            }
            case 1:
                if(m.life<m.lifetime-landmine_unlock) m.state=2;
                break;
            case 2: {
                // 0x4a71e0 with angle 2 pi (any direction) and flag 1 (racers
                // only), so cars with a hit running are not candidates. The
                // mine (0x4df909) asks with no owner, range m_rTrapRange, and
                // springs only when the nearest is not its dropper; the
                // detour (0x4e3707) skips its owner and wants the car
                // strictly within 150 in 3D (0x4e37d0).
                int best=find_target(m.position,0,m.section,2*pi,int(m.trap),m.detour?m.owner:-2);
                if(m.hive) {
                    // 0x4deca4: no owner (flags 1); voice event 5 for the car.
                    if(best!=-2) {
                        m.victim=best;m.trap_time=80;m.state=3;
                        const RaceCar& v=best<0?player_:opponents_[size_t(best)].car;
                        voice_events_.push_back({v.vehicle,5});
                        item_events_.push_back("bee hive on car "+std::to_string(best));
                    }
                    break;
                }
                if(m.oil) {
                    // 0x4ddfd1: no owner test; the car slips and the spot fades.
                    if(best!=-2) {slip(best);item_events_.push_back("oil spill under car "+std::to_string(best));m.state=0xffff;m.fade=m.scale_num;}
                    break;
                }
                if(!m.detour && best==m.owner) best=-2;
                if(best!=-2 && m.detour) {
                    const Vec3& at=best<0?player_.position:opponents_[size_t(best)].car.position;
                    float d=0;for(size_t i=0;i<3;++i) d+=(at[i]-m.position[i])*(at[i]-m.position[i]);
                    if(!(std::sqrt(d)<150)) best=-2;
                }
                if(best!=-2) {
                    if(hit(best,m.type)) item_events_.push_back(std::string(m.detour?"detour":"landmine")+" hit car "+std::to_string(best));
                    m.state=0xffff;
                }
                break;
            }
            case 3: {
                // 0x4ded9e: the bees.
                if(--m.trap_time<=0) {m.done=true;break;}
                RaceCar& v=m.victim<0?player_:opponents_[size_t(m.victim)].car;
                m.position=v.position;
                if(m.trap_time%15==0) {
                    const float k=(float(random_.next()%1000)*0.001f+0.2f)*0.5235988f;
                    m.kick=m.kick>0?-k:k;
                }
                if(m.victim<0) v.steer=std::clamp(v.steer+m.kick,-0.698f,0.698f); // +0x403
                else {float& s=opponents_[size_t(m.victim)].control.steer;s=std::clamp(s+m.kick,-0.698f,0.698f);} // +0x2ad
                if(v.speed.speed>0.3f) v.speed.speed=0.3f;
                break;
            }
            default:
                // 0x4de16b: the oil spot counts +0x3c down, then goes.
                if(m.oil) {if(--m.fade<=0) m.done=true;break;}
                // 0x4df9b1: the WExplosion clone (m_idExplosion) and the
                // mine's removal.
                if(!m.detour && !m.hive) blasts_.push_back({m.position,0});
                m.done=true;
                break;
        }
    }
    mines_.erase(std::remove_if(mines_.begin(),mines_.end(),[](const Landmine& m){return m.done;}),mines_.end());
}
void RaceSession::solid_contacts(RaceCar& car,Body& b) {
    if(b.sound_timer>0) --b.sound_timer; // 0x46a6dc
    // 0x46a768: the car's object query (0x4a5730, mask 0x79600) at the
    // start of its update; its first hit is an 0x10000 primitive (rocks,
    // 0x46a797) when the group has that bit. Natively only the cylinders.
    const auto contact=[&](const Vec3& n) {
        voice_events_.push_back({car.vehicle,5});   // 0x46a7c6, every update in contact
        if(car.grounded) {
            // 0x46a7d7 (+0x15a clear): above speed 0.2 the speed counts only
            // along the normal, through the last update's motion
            // (+0x40b - +0x423); at most 0.5. The force n x (25 + 270 s),
            // its up part made positive, decays by 0.4 (0x4726c0 slot,
            // flags 1); the speed drops to 0.
            float sp=std::abs(car.speed.speed);
            if(sp>0.2f) {
                const Vec3 m=b.has_last?unit({{car.position[0]-b.last[0],car.position[1]-b.last[1],car.position[2]-b.last[2]}}):Vec3{};
                sp*=std::abs(dot(n,m));
            }
            const float f=std::min(sp,0.5f)*270+25;
            b.pushes.push_back({{{n[0]*f,std::abs(n[1]*f),n[2]*f}},0.4f});
            car.speed.speed=0;
        } else car.speed.speed*=0.8f; // 0x46a9f6: in the air
        if(b.sound_timer==0) {b.sound_timer=30;++b.rock_sounds;}
    };
    bool touched=false;
    for(const TrackCylinder& c:track_.cylinders()) {
        if(!(c.sensor_group&0x10000) || !sphere_cylinder(car.position,car_sphere,c)) continue;
        // 0x482403: the normal is down below the base, up above the top and
        // otherwise the horizontal direction from the axis to the car.
        Vec3 n{{0,-1,0}};
        if(car.position[1]>=c.base[1]) n={{0,1,0}};
        if(car.position[1]>=c.base[1] && car.position[1]<=c.base[1]+c.height)
            n=unit({{car.position[0]-c.base[0],0,car.position[2]-c.base[2]}});
        contact(n);touched=true;
        break;
    }
    // A sphere primitive (0x482190): the normal runs from its centre to the
    // car. A WRolling's handler refuses a car (0x4c7745) but keeps its timer.
    if(!touched) for(const SolidSphere& s:objects_.solid_spheres()) {
        if(s.cylinder) {
            const TrackCylinder c{s.centre,s.radius,s.height};
            if(!sphere_cylinder(car.position,car_sphere,c)) continue;
            Vec3 n{{0,-1,0}};
            if(car.position[1]>=c.base[1]) n={{0,1,0}};
            if(car.position[1]>=c.base[1] && car.position[1]<=c.base[1]+c.height)
                n=unit({{car.position[0]-c.base[0],0,car.position[2]-c.base[2]}});
            contact(n);
            break;
        }
        if(!spheres_touch(car.position,s.centre,car_sphere+s.radius)) continue;
        // 0x4c67d0: a WGoldenGate door opens, stops the car and pushes it by the
        // normal x m_a8, 10 up (flags 1, decay m_b0); returning 1, no bounce.
        if(s.golden>=0) {
            const Vec3 n=unit({{car.position[0]-s.centre[0],car.position[1]-s.centre[1],car.position[2]-s.centre[2]}});
            objects_.door_touched(s.golden,s.door,n);
            const GoldenGate& g=objects_.golden_gates()[size_t(s.golden)];
            car.speed.speed=0;
            b.pushes.push_back({{{n[0]*g.force,10,n[2]*g.force}},g.decay});
            break;
        }
        // 0x4ceba0: a WBumSteer gate opens, and squashes the car that hit it.
        if(s.bump>=0) {
            objects_.gate_touched(s.bump,s.gate);
            int who=-1;
            for(size_t i=0;i<opponents_.size();++i) if(&opponents_[i].car==&car) who=int(i);
            hit(who,0x200);
        }
        objects_.car_touched(s.rock);
        contact(unit({{car.position[0]-s.centre[0],car.position[1]-s.centre[1],car.position[2]-s.centre[2]}}));
        break;
    }
}
namespace {
// The car's forward axis in its frame: the heading tilted into the plane
// normal to its up axis (the rear-to-front probe direction).
Vec3 car_forward(const RaceCar& car) {
    const Vec3 f=forward_of(car.yaw);
    const float d=f[0]*car.up[0]+f[1]*car.up[1]+f[2]*car.up[2];
    Vec3 v{{f[0]-car.up[0]*d,f[1]-car.up[1]*d,f[2]-car.up[2]*d}};
    const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(!(l>0)) return f;
    for(float& c:v) c/=l;
    return v;
}
}
void RaceSession::car_contacts(RaceCar& car,Body& b,Vec3& move) {
    // The first other car whose sphere touches this one's; a car held by a
    // hazard hit (0x200) is ignored.
    const RaceCar* other=nullptr;
    const auto test=[&](const RaceCar& c,const Body& ob) {
        if(other || &c==&car || ob.hit.type==0x200 || ob.drown>0 || ob.hidden) return;
        if(spheres_touch(car.position,c.position,2*car_sphere)) other=&c;
    };
    test(player_,body_);
    if(robots_enabled_) for(size_t i=0;i<opponents_.size();++i) test(opponents_[i].car,opponent_bodies_[i]);
    // 0x46b8a0/0x46b894: without a contact the lock counts down.
    if(!other) {if(b.follow_lock>0) --b.follow_lock;return;}
    voice_events_.push_back({car.vehicle,4});   // 0x46b429, every update in contact
    // 0x46c6..: the collision sound +0x3cf on the 30-update timer.
    if(b.sound_timer==0) {b.sound_timer=30;++b.cactus_sounds;}
    const Vec3 d=car_forward(car);
    const Vec3 rel{{other->position[0]-car.position[0],other->position[1]-car.position[1],other->position[2]-car.position[2]}};
    float turn=0;
    if(dot(d,rel)>0) {
        // The car ahead: the speed drops to its speed along this car's axis
        // (at least 0), a 6-degree turn away from its side and half this
        // update's motion.
        const float v=dot(car_forward(*other),d)*other->speed.speed;
        if(v<car.speed.speed) car.speed.speed=v;
        if(car.speed.speed<0) car.speed.speed=0;
        // 0x46b643: the follow lock, 10 updates for m_isrobotauto (+0xc0)
        // 1, else 2; a contact from behind leaves it as it is.
        b.follow_lock=&b==&body_ ? 2 : 10;
        // 0x46b662: the other car's RL - RR probes (+0x423 - +0x42f, 0x468405:
        // RL at -across on the frame's x row), unit, against this car's
        // axis: at most 0 turns +6 degrees, else -6.
        const Matrix4 om=other->frame();
        const Vec3 rl_rr=unit({{-om[0],-om[1],-om[2]}});
        turn=dot(rl_rr,d)<=0?0.10471975f:-0.10471975f;
        for(float& c:move) c*=0.5f;
    }
    // Pushed away by (here - there) x 0.025 x 2.1 (0x512a68, 0x512a64), a
    // slot decaying by 0.6 (type 3).
    Body::Push p;
    for(size_t i=0;i<3;++i) p.force[i]=-rel[i]*0.025f*2.1f;
    p.decay=0.6f;p.yaw=turn;
    b.pushes.push_back(p);
}
void RaceSession::move(RaceCar& car,Body& b,const VehicleTuning& tuning,bool player) {
    // Native safety, not in the original: a car thrown out of any course's
    // range returns to the road rather than failing the ground queries.
    for(const float c:car.position) if(!std::isfinite(c) || std::abs(c)>5e6f) {respawn(car,b,player);return;}
    // The same for the speed: +0x8bf doubles each update of the boost counter
    // where no contact rewrites it (0x466df6; in the air, surface 0), so a
    // held car's speed can compound past any range (cc_t2, a robot struck by
    // the storm: -5.9, -11.8 ... -188 an update) and its front probe, which
    // stretches with the speed, past the ground query's integer range.
    if(!std::isfinite(car.speed.speed) || std::abs(car.speed.speed)>1e4f) {respawn(car,b,player);return;}
    if(glide(car,b)) return;
    solid_contacts(car,b);
    b.last=car.position;b.has_last=true;
    Vec3 before=car.position;
    const float time=frame_time_factor(race_update_seconds*1000);
    float yaw=yaw_step(car.steer,car.speed.speed,tuning,time,!player,b.grip.yaw);
    // 0x46ad3b: the player's turn x 0.3 (0x512a04) past 4 updates in the
    // air, unless +0x385 is 1 (Wine, sh_t1: 0.11 -> 0.033 a step off a ledge).
    if(player && car.airtime>4 && b.effect!=1) yaw*=0.3f;
    if(b.skid.kick) {
        // 0x46aee0: a slide start scales the update's yaw by x m_farolbendmod;
        // the player alone (m_isrobotauto 0) also gets the outward push.
        // Robots restart every update above the limit (0x46b063).
        yaw*=b.skid.started*farol_bend_mod(car.vehicle);
        if(player) {
            const SlidePush s=slide_push(car.steer,b.skid.started,probe_span(car.speed.speed)-102,tuning);
            const Vec3 f=forward_of(car.yaw),r=right_of(car.yaw);
            Vec3 p{};
            for(size_t i=0;i<3;++i) p[i]=r[i]*s.right+f[i]*s.forward;
            b.pushes.push_back({p,0.8f});
        }
    }
    // 0x4727b0 adds the slots' rotation step; 0x472890 then takes the next.
    yaw+=b.spin.step;
    decay_spin(b.spin);
    car.yaw=wrap_angle(car.yaw+yaw);
    // 0x46a70d: a non-positive +0x3eb counts up by one and the motion block
    // (0x46ab98..0x46ac7c: probes and the forward step) is skipped; the
    // yaw, pushes and ground contact still run.
    if(b.motion<=0) ++b.motion;
    const float distance=b.motion>0?forward_step(car.speed.speed,time):0;
    // 0x46b0d2..0x46b157: the wheel spin +0x2a4 moves by 0.05 (0x512a6c) x the
    // step's length (the probe step accumulated at 0x46ac23), added when the
    // speed copy [esp+0x44] is negative, else subtracted, wrapped at +-2 pi.
    {
        const float x=std::abs(distance)*0.05f;
        if(car.speed.speed<0) {b.wheel_spin+=x;if(b.wheel_spin>6.283185f) b.wheel_spin-=6.283185f;}
        else {b.wheel_spin-=x;if(b.wheel_spin<-6.283185f) b.wheel_spin+=6.283185f;}
    }
    // 0x46ab98: the step runs along the rear-to-front probe axis, which
    // carries the car's pitch, so a ramp's lip launches it.
    Vec3 move=car_forward(car);
    for(float& c:move) c*=distance;
    car_contacts(car,b,move);
    // 0x4727b0 sums the flag-1 forces into the update's motion; 0x472890
    // then scales each by its decay and drops it below a squared length
    // of 2 (provisional order within the update).
    float push_yaw=0;
    for(Body::Push& p:b.pushes) {
        for(size_t i=0;i<3;++i) move[i]+=p.force[i];
        for(float& c:p.force) c*=p.decay;
        push_yaw+=p.yaw;p.yaw*=p.yaw_decay<0?p.decay:p.yaw_decay;
    }
    car.yaw=wrap_angle(car.yaw+push_yaw);
    // 0x46b357: then the fall, min(airtime^2 x m_gravity, 100), scaled by
    // the frame factor with the motion (0x46ac.. x +0x163). Checked under
    // Wine on sh_t1's first ramp: ~30 updates in the air, peak ~+510.
    move[1]-=std::min(float(car.airtime)*float(car.airtime)*tuning.gravity,fall_cap)*time;
    car.position[1]+=move[1];move[1]=0;
    b.pushes.erase(std::remove_if(b.pushes.begin(),b.pushes.end(),[](const Body::Push& p){return dot(p.force,p.force)<2 && std::abs(p.yaw)<1e-3f;}),b.pushes.end());
    // No edge guard: the original lets a car leave the WGNR (Wine, sh_t1:
    // one hangs at (-4028, 850, -3515) past the edge); its probes and wall
    // rays keep it, or it falls and returns (0x4a6730).
    car.position[0]+=move[0];car.position[2]+=move[2];
    // Same native safety as above, for the update that throws the car: +0x8bf
    // compounds on surfaces 0x470218 keeps (0x466df6, surface 1 on cc_t4), and
    // the original's __ftol would not throw here.
    for(const float c:car.position) if(!std::isfinite(c) || std::abs(c)>5e6f) {respawn(car,b,player);return;}
    // 0x46b9a0/0x46ba57: the slots' pitch and roll steps turn the frame the
    // wall rays and ground probes use (stored at 0x46c500). The step above
    // used the last update's probes (Wine, sh_t1: pitch 0.0175 lifts the car
    // from the next update).
    if(b.tilt_pitch!=0 || b.tilt_roll!=0) {
        car.pitch+=b.tilt_pitch;car.roll+=b.tilt_roll;b.tilt_pitch=b.tilt_roll=0;
        car.up=tilted_up(car.yaw,car.pitch,car.roll);
    }
    wall_contacts(car,b,before);
    const int loops=car.loops;
    settle(car,b,player);
    // Compare the start-line crossing in the frame the car now occupies.
    if(car.loops!=loops) before=transform_point(car.loops>loops?track_.wrap():track_.unwrap(),before);
    // A return put the car ahead of the start line without crossing it.
    if(b.respawns>0 && b.last_respawn==updates_) return;
    crossings(car,b,before,player);
}
void RaceSession::wall_contacts(RaceCar& car,Body& b,const Vec3& frame_position) {
    // 0x46bb..0x46c3f: after the move, 0x468280 mode 1 places four contact
    // probes (lateral -/+68, 24 up, -/+122 along; vehicle 0 88 and 190,
    // vehicle 6 180; the front one stretched above speed 0.4 as in mode 2).
    // From the centre 30 up (its height the mean of the FL and RR probes),
    // pushed 20% past it away from each probe, a ray (mask 0x1e000ff) runs
    // to the probe; a hit's penetration along its normal is the probe's
    // correction N ((H - P) . N).
    const Matrix4 m=car.frame();
    const Vec3 r{{m[0],m[1],m[2]}},u{{m[4],m[5],m[6]}},f{{m[8],m[9],m[10]}};
    float side=68,along=122;
    if(car.vehicle==0) {side=88;along=190;}
    else if(car.vehicle==6) along=180;
    if(car.speed.speed>0.4f) along*=(car.speed.speed-0.4f)*0.09f+1;
    std::array<Vec3,4> probe{};
    const float lat[4]={-side,side,-side,side},lon[4]={along,along,-along,-along}; // FL FR RL RR
    for(size_t k=0;k<4;++k) for(size_t i=0;i<3;++i) probe[k][i]=car.position[i]+r[i]*lat[k]+u[i]*24+f[i]*lon[k];
    Vec3 centre{};
    // 0x46bab5: 0x404def reads the car's world frame, whose translation is
    // still the last update's: this update's motion has only reached +0x288.
    for(size_t i=0;i<3;++i) centre[i]=frame_position[i]+u[i]*30;
    centre[1]=(probe[0][1]+probe[3][1])*0.5f+u[1]*30;
    int hits=0;uint32_t flags=0;   // the hits' polygon flags, or-ed (0x46bdd1)
    std::array<Vec3,4> normal{},correction{};
    Vec3 most{{-1,-1,-1}},least{{1,1,1}},mean{},start{};
    for(size_t k=0;k<4;++k) {
        for(size_t i=0;i<3;++i) start[i]=centre[i]+(centre[i]-probe[k][i])*0.2f;
        const TrackContact w=track_.contact(start,probe[k]);
        if(!w.hit) continue;
        Vec3 hit{};
        for(size_t i=0;i<3;++i) hit[i]=start[i]+(probe[k][i]-start[i])*w.fraction;
        float depth=0;
        for(size_t i=0;i<3;++i) depth+=(hit[i]-probe[k][i])*w.normal[i];
        Vec3 c{};
        for(size_t i=0;i<3;++i) {
            c[i]=w.normal[i]*depth;
            // Per axis: the largest positive and the smallest negative
            // correction (from -1 and +1, as the original starts them).
            if(c[i]>=0) {if(most[i]<c[i]) most[i]=c[i];}
            else if(c[i]<least[i]) least[i]=c[i];
            mean[i]+=hit[i];
        }
        normal[size_t(hits)]=w.normal;correction[size_t(hits)]=c;++hits;flags|=w.flags;
    }
    if(hits==0) return;
    Vec3 push{};
    const auto axes=[&]() {for(size_t i=0;i<3;++i) push[i]=most[i]+least[i];};
    if(hits==1) push=correction[0];
    else if(hits==2) {
        // Two walls at a sharp angle (normals' dot below 0.4): a corner; the
        // car stops and the second correction is carried along the part of
        // its normal off the first.
        const float d=dot(normal[0],normal[1]);
        if(d>=0.4f) axes();
        else {
            car.speed.speed=0;
            Vec3 e{};
            for(size_t i=0;i<3;++i) e[i]=d*normal[0][i]-normal[1][i];
            const float g=dot(normal[1],e);
            if(g==0) axes();
            else {
                const float k=std::sqrt(dot(correction[1],correction[1]))/g;
                for(size_t i=0;i<3;++i) push[i]=correction[0][i]+k*e[i];
            }
        }
    } else {
        // 0x46c28d: three or more hits stop the car on flags 0x1600001.
        axes();
        if(flags&0x1600001) car.speed.speed=0;
    }
    for(float& c:mean) c/=float(hits);
    // 0x46c366: airborne (+0x8a7 > 1), a running spin is dropped (0x472680).
    if(car.airtime>1 && b.spin.left!=0) b.spin=Spin{};
    // 0x46c1f1: never pushed down, nor reversing while pushed down.
    if(push[1]<0) {push[1]=0;if(car.speed.speed<0) car.speed.speed=0;}
    for(size_t i=0;i<3;++i) car.position[i]+=push[i];
    // A small turn: the y of push x lever (0x452ed0 at 0x46c4bb, ecx = arg1
    // x arg2 with arg1 the push, arg2 the lever (mean contact - the last
    // ray's start) x speed x 2e-5), clamped to 0.07 rad; then the speed x
    // 0.95 (0x512a4c).
    Vec3 lever{};
    for(size_t i=0;i<3;++i) lever[i]=(mean[i]-start[i])*car.speed.speed*2e-5f;
    const float turn=std::clamp(push[2]*lever[0]-push[0]*lever[2],-0.07f,0.07f);
    car.yaw=wrap_angle(car.yaw+turn);
    car.speed.speed*=0.95f;
    // 0x46c5ad: a push whose unit y is below 0.92 (a wall, not a floor)
    // ends a running TurboSpeed (+0x385 = 8): the rows back, speed x 0.4.
    const float length=std::sqrt(dot(push,push));
    if(length>0 && push[1]/length<0.92f && b.effect==8) {b.turbo=0;b.effect=-1;car.speed.speed*=0.4f;}
    // 0x46c607: in the air (+0x8a7 > 4), a hit on flags 0x600000 sets
    // +0x157 and keeps the unit push in +0x23b.
    if((flags&0x600000) && car.airtime>4 && length>0) {b.glide=1;for(size_t i=0;i<3;++i) b.glide_dir[i]=push[i]/length;}
    ++b.impacts;
}
void RaceSession::cross_seam(RaceCar& car,Body& b,int step,bool player) {
    // The car, its recent safe contacts and the camera all cross the seam
    // together (a rotation on sh_t1 and dt_t1).
    const Matrix4& m=step>0?track_.wrap():track_.unwrap();
    const float turn=yaw_turn(m);
    car.position=transform_point(m,car.position);car.yaw=wrap_angle(car.yaw+turn);
    car.up=unit(rotate(m,car.up));car.loops+=step;
    // The camera crosses with its car (native: the original's seam handling
    // of the camera is not traced).
    if(&car==&player_) cameras_[0].carry(m);
    else if(two_players() && &car==&opponents_[0].car) cameras_[1].carry(m);
}
int RaceSession::carry_across_seam(Vec3& at,int& section,std::initializer_list<Vec3*> along) const {
    // The manager tracks a thrown or fired object (0x4a6b30: registered in
    // the player's tracker slot whose box holds it) and steps its row when
    // it passes the row's exit or entry WPortal (0x4a1a20). From the last
    // row of the last section into section 0 its matrix is multiplied by
    // the WMOD transform count + 3 (0x4a23c2, flag 0x100000), and back by
    // count + 5 (0x4a25b5, 0x200000). Natively (provisional, as for the far
    // robots): no route under it, but one through the transform.
    const int last=int(track_.sections())-1;
    if(last<=0 || (section!=0 && section!=last) || track_.route(at).hit) return 0;
    for(const int k:{1,-1}) {
        if(k>0?section!=last:section!=0) continue;
        const Matrix4& m=k>0?track_.wrap():track_.unwrap();
        const Vec3 moved=transform_point(m,at);
        const RaceTrack::Route r=track_.route(moved);
        if(!r.hit) continue;
        at=moved;section=r.section;
        for(Vec3* v:along) *v=rotate(m,*v);
        return k;
    }
    return 0;
}
TrackSurface RaceSession::ground(float x,float z,float top,float bottom) const {
    TrackSurface s=track_.ground(x,z,top,bottom);
    last_log_={{-1,-1}};last_bridge_=-1;
    const LogHit l=objects_.log_ground(x,z,top,bottom);
    if(l.hit && (!s.hit || l.y>s.y)) {
        // A log: plain ground flags (provisional), the static query's section.
        if(!s.hit) s.section=-1;
        s.hit=true;s.y=l.y;s.normal=l.normal;s.flags=0;s.point={{x,l.y,z}};
        last_log_={{l.ride,l.log}};last_bridge_=l.bridge;
        if(s.section<0) {const TrackSurface below=track_.ground(x,z,top,-1e6f);s.section=below.hit?below.section:0;}
    }
    return s;
}
TrackSurface RaceSession::ray(const Vec3& from,const Vec3& to) const {
    TrackSurface s=track_.ray(from,to);
    last_log_={{-1,-1}};last_bridge_=-1;
    // The robots' ray runs along their up axis, nearly vertical: the logs
    // are tested vertically at the ray's start (provisional).
    const LogHit l=objects_.log_ground(from[0],from[2],from[1],to[1]);
    if(l.hit && (!s.hit || l.y>s.point[1])) {
        const int section=s.hit?s.section:0;
        s.hit=true;s.y=l.y;s.normal=l.normal;s.flags=0;s.point={{from[0],l.y,from[2]}};s.section=section;
        last_log_={{l.ride,l.log}};last_bridge_=l.bridge;
    }
    return s;
}
void RaceSession::settle(RaceCar& car,Body& b,bool player) {
    const float top=car.position[1]+step_height,bottom=car.position[1]-4000;
    TrackSurface center=ground(car.position[0],car.position[2],top,bottom);
    b.log_ride=last_log_[0];b.log=last_log_[1];b.bridge=last_bridge_; // +0x89f
    // Sections overlap past their portals at the same height (dr_t4). A tie
    // with the section before keeps the car's own, or a robot shown at a portal
    // would count as that section and hide again at once; going on to the next
    // stays free (provisional: the original changes row at the portal, 0x4a12a0).
    if(center.hit && car.section>=0 && center.section!=car.section && last_log_[1]<0 && last_bridge_<0 &&
       (center.section+1)%int(track_.sections())==car.section) {
        const TrackSurface own=track_.ground_in(car.section,car.position[0],car.position[2],top,bottom);
        if(own.hit && own.y>=center.y-1) center=own;
    }
    if(!center.hit) {
        // Past the last portal the course continues in the first route; the
        // original renders/queries it through the WMOD loop transform.
        for(const int step:{1,-1}) {
            if(step>0?car.section!=int(track_.sections())-1:car.section!=0) continue; // the seam's two sections
            const Matrix4& m=step>0?track_.wrap():track_.unwrap();
            const Vec3 high=transform_point(m,{{car.position[0],top,car.position[2]}});
            const float low=transform_point(m,{{car.position[0],bottom,car.position[2]}})[1];
            const TrackSurface shifted=ground(high[0],high[2],high[1],low);
            if(!shifted.hit) continue;
            cross_seam(car,b,step,player);
            center=shifted;
            break;
        }
    }
    // Nothing below the centre does not stop the contact rays: a car hanging
    // off a ledge keeps a probe on it (sh_t1, Wine: one probe at (-4028,
    // -3515) for ~140 updates, tilting 1 degree a contact, then a straight
    // fall).
    if(center.hit) {car.section=center.section;car.surface=center.flags;}
    uint32_t touched=0; // +0x8bb: the flags of the last ray that hit (puStack_820)
    const Vec3 fwd=car_forward(car),side=unit(cross(car.up,fwd));
    const Vec3 up=car.up;
    Vec3 up_new=car.up;
    bool contact=false;
    if(!player) {
        b.glide=0;   // 0x46c858: the robots' contact clears +0x157
        // 0x46c7f2..0x46c97e: robots (m_isrobotauto, not battle) cast one
        // ray along the car's up axis x 1.3 (0x512a40), from 2 x 80 above
        // (0x512a3c) to +0x93b (30) below, so 208 up to 39 down; on a hit
        // (0x46cf8a) the car sits at the hit + its normal x 3 (0x511894),
        // the normal becoming its up axis.
        Vec3 from{},to{};
        for(size_t i=0;i<3;++i) {from[i]=car.position[i]+up[i]*1.3f*ray_above*2;to[i]=car.position[i]-up[i]*1.3f*(b.hover?0.f:ray_below);}
        const TrackSurface g=ray(from,to);
        if(g.hit) {b.log_ride=last_log_[0];b.log=last_log_[1];b.bridge=last_bridge_;}
        if(g.hit) {
            contact=true;touched=g.flags;
            up_new=g.normal[1]<0?Vec3{{-g.normal[0],-g.normal[1],-g.normal[2]}}:g.normal;
            for(size_t i=0;i<3;++i) car.position[i]=g.point[i]+up_new[i]*3;
            // 0x46d0c6 (0x4035e9): the frame's angles go to +0x267/+0x26f.
            if(up_new[1]>0) {car.pitch=std::atan(-dot(forward_of(car.yaw),up_new)/up_new[1]);car.roll=std::atan(dot(right_of(car.yaw),up_new)/up_new[1]);}
        }
    } else {
        // 0x46cf..: four rays from the mode-2 probes, FL 1, FR 2, RL 4, RR 8
        // (provisional: vertical rays at each probe rather than along the
        // car's down axis).
        const float along=probe_span(car.speed.speed)*0.5f;
        Vec3 hit[4]{};int mask=0;
        // 0x46d7fa: probes on flags 0x1000001 (walls) count, from 1 when
        // +0x157 is already set; each sets +0x157 to its number (1..4) and
        // +0x23b to the polygon's normal.
        int walls=b.glide!=0;
        for(int i=0;i<4;++i) {
            const float lx=(i&1)?probe_across:-probe_across,lz=(i&2)?-along:along;
            Vec3 p{};
            for(size_t k=0;k<3;++k) p[k]=car.position[k]+side[k]*lx+fwd[k]*lz;
            // 0x46c7f6: the up axis x 1.3 (0x512a40) serves these rays too, so
            // 104 above to 39 below (Wine, sh_t1: a car landing on the barrier
            // 39 above the ground has airtime 1 at once).
            const TrackSurface g=ground(p[0],p[2],p[1]+1.3f*ray_above,p[1]-1.3f*(b.hover?0.f:ray_below));
            if(g.hit) {
                mask|=1<<i;hit[i]={{p[0],g.y,p[2]}};touched=g.flags;
                if(g.flags&0x1000001) {b.glide=i+1;b.glide_dir=g.normal;++walls;}
            }
        }
        count_glide(b,walls);
        const auto slope=[](const Vec3& a,const Vec3& b) {
            const float dx=a[0]-b[0],dz=a[2]-b[2];
            return std::atan2(a[1]-b[1],std::sqrt(dx*dx+dz*dz)); // 0x471ff0(dy, 0x472030(d))
        };
        const auto rest=[&](int a,int b2) {car.position[1]=(hit[a][1]+hit[b2][1])*0.5f;};
        enum {FL=0,FR=1,RL=2,RR=3};
        // The cases of 0x46f..: pitch from a front/rear pair, roll from a
        // left/right pair, and the height from a diagonal's midpoint.
        switch(mask) {
            case 3: car.roll=slope(hit[FL],hit[FR]);break;
            case 5: car.pitch=slope(hit[FL],hit[RL]);break;
            case 10: car.pitch=slope(hit[FR],hit[RR]);break;
            case 12: car.roll=slope(hit[RL],hit[RR]);break;
            case 6: rest(FR,RL);break;
            case 9: rest(FL,RR);break;
            case 7: car.pitch=slope(hit[FL],hit[RL]);car.roll=slope(hit[FL],hit[FR]);rest(FR,RL);break;
            case 11: car.pitch=slope(hit[FR],hit[RR]);car.roll=slope(hit[FL],hit[FR]);rest(FL,RR);break;
            case 13: car.pitch=slope(hit[FL],hit[RL]);car.roll=slope(hit[RL],hit[RR]);rest(FL,RR);break;
            case 14: car.pitch=slope(hit[FR],hit[RR]);car.roll=slope(hit[RL],hit[RR]);rest(FR,RL);break;
            case 15: car.pitch=slope(hit[FL],hit[RL]);car.roll=slope(hit[RL],hit[RR]);rest(FR,RL);break;
            default: break; // 0 or one probe: no change
        }
        contact=mask!=0;
        // 0x46fb6f..0x46ff95: +0x8ab/+0x8af by the probes on the ground,
        // then a type-2 slot (0x40400c at 0x46ffe0) tilts the car the next
        // update by 1 degree (0x511518) towards the unsupported side: one
        // probe or one pair tips pitch and/or roll, the diagonals, three or
        // four probes and none leave it.
        static const signed char tilt[16][2]={{0,0},{1,1},{1,-1},{1,0},{-1,1},{0,1},{0,0},{0,0},
                                             {-1,-1},{0,0},{0,-1},{0,0},{-1,0},{0,0},{0,0},{0,0}};
        b.tilt_pitch=tilt[mask][0]*one_degree;b.tilt_roll=tilt[mask][1]*one_degree;
        up_new=tilted_up(car.yaw,car.pitch,car.roll);
    }
    // 0x46ff..: any contact resets the airtime to 1; without one it counts
    // up, and past 10 updates the pitch and roll ease by 0.85 (0x512a38),
    // as the Wine sh_t1 jump shows (0.2731 for 10 updates, then x 0.85).
    if(contact) car.airtime=1;
    else if(++car.airtime>10) {car.pitch*=0.85f;car.roll*=0.85f;}
    // In the air the robots' frame also follows the eased angles, so a
    // robot levels out instead of keeping the last plane's tilt.
    if(!player && !contact) up_new=tilted_up(car.yaw,car.pitch,car.roll);
    car.grounded=contact;
    car.up=up_new;
    if(contact) car.surface=touched;
    // No WGNR record below and no contact: the car has left the course area.
    // It falls (move) and returns after 40 updates in the air (provisional:
    // the original 0x800000 return, 0x46ff..0x4a6730, is not ported).
    if(!center.hit) {
        if(!contact && car.airtime>40) respawn(car,b,player);
        return;
    }
    // +0x8bb takes the contact rays' flags (0x46ffxx), so only a car that
    // touches the deep river bed drowns, not one flying over it.
    // Provisional: a far robot only slides along the route, which can pass
    // within the contact ray's reach of deep water (sh_t2's drop off the
    // rolling log, 37 above it at z 94844), so it does not drown.
    if(contact && !b.sliding && RaceTrack::drowning_surface(car.surface)) {
        b.drown=drown_delay;car.speed={};car.steer=0;return;
    }
}
void RaceSession::respawn(RaceCar& car,Body& b,bool player) {
    // 0x4a6730: the frame of the entry node of the car's row (+0x1b7,
    // +0x1bf; the start line for row 0's first), moved 200 (0x512a44) along
    // its z axis, then lifted 50 (0x512a5c). Wine, sh_t1: drowned in
    // section 2 -> (22599, 3000, -11006), off section 0's ledge -> the start
    // line + 200.
    ++b.respawns;b.last_respawn=updates_;
    const Matrix4& m=car.section>0?track_.entry_frame(car.section):track_.start_line();
    for(size_t i=0;i<3;++i) car.position[i]=m[12+i]+m[8+i]*200;
    car.position[1]+=50;
    car.yaw=std::atan2(-m[8],m[10]);
    if(car.section<0) car.section=0;
    car.speed={};car.steer=0;car.up={{0,1,0}};car.grounded=true;car.airtime=1;car.pitch=car.roll=0;
    b.pushes.clear();b.has_last=false;b.skid=SkidState{};b.grip=SurfaceGrip{};
    if(&car==&player_) near_lock_[0]=25;
    else if(two_players() && &car==&opponents_[0].car) near_lock_[1]=25;
    b.drown=0;
    settle(car,b,player);
}
void RaceSession::crossings(RaceCar& car,Body& b,const Vec3& before,bool player) {
    if(car.section>=0) b.visited|=1u<<car.section;
    const Matrix4& line=track_.start_line();
    const float radius=track_.start_radius();
    // 0x49a2f0: the current position within the line's radius (horizontally),
    // then the side of the previous and current positions along its z axis:
    // +1 forward, -1 backward.
    const auto crossing=[&](const Vec3& from,const Vec3& to) {
        const float x=to[0]-line[12],z=to[2]-line[14];
        if(std::abs(x)>radius || std::abs(z)>radius || x*x+z*z>radius*radius) return 0;
        const float now=x*line[8]+z*line[10],then=(from[0]-line[12])*line[8]+(from[2]-line[14])*line[10];
        return now>0 && then<0?1:now<0 && then>0?-1:0;
    };
    // The native seam comes where the section's ground ends, which can lie
    // past the line (cc_t3: 450 past it), where the original's comes at the
    // portal (provisional): the last section is also tested through the wrap.
    // The callers already carry before over a seam taken this update.
    int r=crossing(before,car.position);
    if(!r && car.section==int(track_.sections())-1) r=crossing(transform_point(track_.wrap(),before),transform_point(track_.wrap(),car.position));
    // 0x4a4201..0x4a4284: the count, never below -1; past 0 it adds to the
    // lap +0x19f and returns to 0. Both start at 0 (0x49f4c9), so the first
    // crossing makes the shown lap 1 and four finish a three-lap race. Native
    // laps are the completed ones: line starts at -1, so that crossing adds none.
    b.line=std::max(b.line+r,-1);
    if(b.line>0) {b.line=0;count_lap(b,&b==&body_);}
}
void RaceSession::count_lap(Body& b,bool player) {
    if(phase_!=Phase::Racing || mode_==1) return; // a battle has no laps
    ++b.laps;b.visited=1;
    if(player && b.laps>=1 && b.laps<=3) {
        // 0x4a33ac..0x4a33f6: each lap's time, the race timer less the laps before.
        const int now=int(std::lround(race_seconds()*300));
        int before=0;for(int k=0;k<b.laps-1;++k) before+=std::max(splits_[size_t(k)],0);
        splits_[size_t(b.laps-1)]=now-before;
    }
    if(b.laps>=track_.laps() && !b.finished) {
        b.finished=true;b.finish_update=updates_;
        // Two players: the race ends once both have finished (provisional).
        const bool both=!two_players() || (body_.finished && opponent_bodies_[0].finished);
        if((player || two_players()) && both) {phase_=Phase::Finished;finish_update_=updates_;finish();}
    }
}
void RaceSession::count_glide(Body& b,int walls) {
    // 0x46df00: past 3 updates with wall contacts +0x157 = 1 and the count
    // restarts; until then +0x157 = 0. None: the count is 0 (0x46d0ef).
    if(walls>0) {
        if(++b.glide_count>3) {b.glide=1;b.glide_count=0;} else b.glide=0;
    } else b.glide_count=0;
}
bool RaceSession::glide(RaceCar& car,Body& b) {
    // 0x46a58a: the cooldown counts down while no glide contact is counted.
    if(b.glide_timer!=0 && b.glide_count==0) --b.glide_timer;
    // 0x46a5a3: with +0x157 and a push of more than 0.1 across, the glide
    // replaces the update. Entering it (count 0) needs no cooldown and a
    // tilted car (|up.x| or |up.z| > 0.1); the push is then scaled by
    // (its y + 1) x 20 with its y zeroed, and the count starts at 1.
    if(!b.glide || (std::abs(b.glide_dir[0])<=0.1f && std::abs(b.glide_dir[2])<=0.1f)) return false;
    if(b.glide_count==0) {
        if(b.glide_timer!=0 || (std::abs(car.up[0])<=0.1f && std::abs(car.up[2])<=0.1f)) return false;
        // Scale a unit push only: the port can re-enter with the direction of
        // the last glide, already scaled, which grew by 20 each time up to
        // 1e8 units (cc_t4; provisional: the original's rewrite of +0x23b
        // between glides is not traced).
        const float len=std::sqrt(dot(b.glide_dir,b.glide_dir));
        if(len<=1.5f) {
            const float k=(b.glide_dir[1]+1)*20;
            b.glide_dir[0]*=k;b.glide_dir[2]*=k;
        }
        b.glide_dir[1]=0;
        b.glide_count=1;++b.glides;
    }
    // 0x4721f0: the car moves by +0x23b; a ray (mask 0x18000ff) from 250
    // above to 200 below; missing, it drops 160; hitting, it sits 3 off
    // the polygon along its normal, aligned with it, and drivable flags
    // (0x8000fe) count up to 5, which ends the glide. It also ends past 400
    // updates or once 1 above its height at the fifth: speed 0, a cooldown
    // of 7.
    for(size_t i=0;i<3;++i) car.position[i]+=b.glide_dir[i];
    const auto end=[&]() {b.glide_timer=7;b.glide_count=0;car.speed.speed=0;};
    const int n=++b.glide_timer;
    if(n>400) {end();return true;}
    if(n==5) b.glide_base=car.position[1];
    else if(n>5 && car.position[1]-b.glide_base>1) {end();return true;}
    const TrackSurface g=ground(car.position[0],car.position[2],car.position[1]+250,car.position[1]-200);
    if(!g.hit) {car.position[1]-=160;return true;}
    const Vec3 at{{car.position[0],g.y,car.position[2]}};
    for(size_t i=0;i<3;++i) car.position[i]=at[i]+g.normal[i]*3;   // 0x511894
    car.up=g.normal;car.section=g.section;
    // The frame from the normal (0x452ed0, 0x43c170): pitch and roll that
    // put the heading and the side axis in the polygon's plane.
    const Vec3 f=forward_of(car.yaw),r=right_of(car.yaw);
    if(g.normal[1]>0) {car.pitch=std::atan(-dot(f,g.normal)/g.normal[1]);car.roll=std::atan(dot(r,g.normal)/g.normal[1]);}
    if((g.flags&0x8000fe) && ++b.glide_count>=5) {b.glide=0;end();}
    return true;
}
bool RaceSession::virtual_robot(size_t i,std::vector<int>& placed) {
    Opponent& o=opponents_[i];Body& b=opponent_bodies_[i];RaceCar& car=o.car;
    const int n=int(track_.sections()),ps=player_.section;
    // The player's slots +0x830/+0x8c4/+0x958: the previous, current and
    // next rows (one row per section here).
    const auto in_window=[&](int s) {return ps<0 || n<=0 || s==ps || (s+1)%n==ps || (ps+1)%n==s;};
    if(!b.hidden) {
        if(robots_near_ || car.section<0 || in_window(car.section)) return false;
        // 0x4a2776: +0x2b1 = 0x10000, +0x1c7 = 0, flag 0x100 on the car and
        // its children; +0x2b5 keeps the far step's distance into the row
        // (0x4e545f).
        b.hidden=true;o.effect=0;o.far=true;
        b.virtual_distance=int(track_.section_fraction(car.section,car.position)*float(track_.row_length(car.section)));
        return true;
    }
    // 0x4e530f: +0x2b5 += +0x4c a robot update; 0x4a1fe7: past the row's
    // +0x34 it starts the next row.
    const int before=b.virtual_distance;
    b.virtual_distance+=204;   // the probe span, as in the far step
    // Provisional: the lap counts where the run passes the start line's
    // distance into section 0 (the original's lap trigger for hidden
    // robots is not traced).
    if(car.section==0) {
        const Matrix4& line=track_.start_line();
        const int at=int(track_.section_fraction(0,{{line[12],line[13],line[14]}})*float(track_.row_length(0)));
        if(before<at && b.virtual_distance>=at && b.visited==(1u<<n)-1) count_lap(b,false);
    }
    if(b.virtual_distance>track_.row_length(car.section)) {
        b.virtual_distance=0;
        car.section=(car.section+1)%n;
        b.visited|=1u<<car.section;
        if(car.section==0) ++car.loops;
    }
    if(!in_window(car.section)) return true;
    // 0x4a20aa..: shown again at the row's entry WPortal (row +0x48),
    // 200 ahead along its z axis, 50 up and (k - robots / 2) x 200 along
    // its x axis for the k-th robot placed there in this pass; +0x2b1 = 1,
    // +0x8a7 = 1 (airborne), the slow counter +0xa8 = 0.
    const Matrix4& f=track_.entry_frame(car.section);
    int& k=placed.at(size_t(car.section));
    const float side=float((k++)-int(opponents_.size())/2)*200;
    const Vec3 x=unit({{f[0],f[1],f[2]}}),z=unit({{f[8],f[9],f[10]}});
    for(size_t a=0;a<3;++a) car.position[a]=f[12+a]+z[a]*200+x[a]*side;
    car.position[1]+=50;
    car.yaw=std::atan2(-z[0],z[2]);
    car.grounded=false;car.airtime=1;
    b.hidden=false;b.virtual_distance=0;
    o.previous=car.position;
    settle(car,b,false);
    return false;
}
}
