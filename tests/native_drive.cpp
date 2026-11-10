#include <array>
#include <algorithm>
#include "game_drive.h"
#include "game_scene.h"
#include "race_session.h"
#include "game_motion.h"
#include "race_weapons.h"
#include "race_robot.h"
#include "game_random.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void near(float a,float b,float tolerance,const char* text) {require(std::abs(a-b)<=tolerance,text);}

void test_camera() {
    // 0x477ea0/0x477b60/0x478070 on a still identity frame with no ground.
    using V=std::array<float,3>;
    const wpc::CameraCast none=[](const V&,const V&){return wpc::CameraHit{};};
    wpc::FollowCamera cam;cam.init({},false,wpc::identity_matrix());
    const wpc::Matrix4 m0=cam.matrix();
    near(m0[12],0,1e-4f,"camera starts on the frame's x");
    near(m0[13],400,1e-3f,"view _2 eye 400 up");
    near(m0[14],-900,1e-3f,"view _2 eye 900 behind");
    require(cam.second_view() && !cam.near(),"0x477b60 sets flag 1 only");
    wpc::FollowCameraState st;
    for(int i=0;i<200;++i) cam.update(wpc::identity_matrix(),st,none);
    near(cam.matrix()[14],-900,0.5f,"a still car keeps the eye on the offset");
    st.view=true;cam.update(wpc::identity_matrix(),st,none);st.view=false;
    require(!cam.second_view(),"the view key toggles to view 1");
    for(int i=0;i<200;++i) cam.update(wpc::identity_matrix(),st,none);
    near(cam.matrix()[13],300,0.5f,"view 1 eye 300 up");
    near(cam.matrix()[14],-550,0.5f,"view 1 eye 550 behind");
    st.look_back=true;cam.update(wpc::identity_matrix(),st,none);
    near(cam.matrix()[14],550,0.5f,"the rear view mirrors the eye ahead");
    require(cam.matrix()[10]<0,"the rear view looks backward");
    st.look_back=false;
    // A wall between the car and the eye pulls the eye in front of it.
    const wpc::CameraCast wall=[](const V& a,const V& b){
        if(!(a[2]>-300 && b[2]<-300)) return wpc::CameraHit{};
        const float t=(a[2]+300)/(a[2]-b[2]);
        return wpc::CameraHit{true,{{a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t,-300}}};
    };
    cam.update(wpc::identity_matrix(),st,wall);
    require(cam.matrix()[14]>-300,"0x478b2e: the eye moves in front of the wall");
    // The intro swing (states 1/0xc): 600 up and on the axis at update 0,
    // the stored offset again from update 40.
    wpc::FollowCamera intro;intro.init({},false,wpc::identity_matrix());
    st.intro=0;intro.update(wpc::identity_matrix(),st,none);
    for(int i=1;i<=200;++i) {st.intro=i;intro.update(wpc::identity_matrix(),st,none);}
    near(intro.matrix()[13],400,0.5f,"the swing ends on the view's offset");
    // The near mode: the car climbs back into the eye.
    wpc::FollowCamera back;back.init({},false,wpc::identity_matrix());st={};
    wpc::Matrix4 f=wpc::identity_matrix();
    // The eye settles at the offset less one update's motion behind it.
    for(int i=0;i<40 && !back.near();++i) {f[13]+=300;f[14]-=800;back.update(f,st,none);}
    require(back.near(),"0x478233: an eye within m_rNearRadius enters the near mode");
}
void test_speed() {
    const wpc::VehicleTuning dingo=wpc::vehicle_tuning(3);
    near(dingo.top_speed,0.58f,1e-6f,"0x52b558 Dingo top speed");
    near(dingo.steer_step,0.05585054f,1e-7f,"0x4660b0 Dingo steering step");
    wpc::SpeedState s;
    wpc::update_speed(s,dingo,{true,false,false,false});
    // Below 0.8 the target is 0.8 (0x5127ec), approached by row[1].
    near(s.speed,0.8f*0.017f,1e-6f,"first acceleration step converges toward max(top,0.8)");
    for(int i=0;i<400;++i) wpc::update_speed(s,dingo,{true,false,false,false});
    near(s.speed,dingo.top_speed,1e-6f,"speed holds at the vehicle top speed");
    wpc::update_speed(s,dingo,{});
    near(s.speed,dingo.top_speed-0.0096f,1e-6f,"coast step 0.0096");
    s={0.01f,0};wpc::update_speed(s,dingo,{});
    require(s.speed==0,"coasting below 0.015 stops");
    s={0.03f,0};wpc::update_speed(s,dingo,{false,true,false,false});
    near(s.speed,0.007f,1e-6f,"brake step 0.023");
    wpc::update_speed(s,dingo,{false,true,false,false});
    require(s.speed==0 && s.brake_state==1,"brake crossing zero holds in state 1");
    wpc::update_speed(s,dingo,{false,true,false,false});
    require(s.speed==0,"continued braking holds at zero");
    wpc::update_speed(s,dingo,{});
    require(s.brake_state==2,"release enters reverse state");
    for(int i=0;i<100;++i) wpc::update_speed(s,dingo,{false,true,false,false});
    near(s.speed,-0.4f,1e-6f,"reverse target -0.4");
    wpc::update_speed(s,dingo,{true,false,false,false});
    near(s.speed,-0.4f+0.023f,1e-6f,"accelerating from reverse uses 0.023");
}
void test_steering_and_motion() {
    const wpc::VehicleTuning t=wpc::vehicle_tuning(0);
    float steer=0;
    steer=wpc::update_steering(steer,t,{false,false,true,false});
    near(steer,t.steer_step*0.8f,1e-6f,"steering step then 0.8 decay");
    for(int i=0;i<100;++i) steer=wpc::update_steering(steer,t,{false,false,true,false});
    near(steer,t.steer_step*4,1e-4f,"held steering settles at 4 steps");
    steer=wpc::update_steering(0.02f,t,{});
    require(steer==0,"0.017 deadband after decay");
    near(wpc::frame_time_factor(1000.0/30),0.9333333f,1e-6f,"33 ms frame factor");
    near(wpc::frame_time_factor(80),1.8666667f,1e-6f,"long frames advance more");
    near(wpc::probe_span(0.3f),204,1e-6f,"probe span below 0.4");
    near(wpc::probe_span(0.6f),102*1.018f+102,1e-3f,"forward probe stretch above 0.4");
    require(wpc::yaw_step(0.3f,0,t,1)==0,"no yaw at rest");
    require(wpc::yaw_step(0.3f,0.5f,t,1)>0 && wpc::yaw_step(-0.3f,0.5f,t,1)<0,"yaw follows steering sign");
    require(wpc::yaw_step(0.3f,-0.2f,t,1)<0,"reverse inverts steering");
    near(wpc::yaw_step(1,0.5f,t,1),0.08726646f*std::abs(t.yaw_reference-0.5f)*0.8f,1e-6f,"5 degree clamp times yaw term");
}
// Lookahead driver used only by this test: try steering and reverse plans
// one second ahead and keep the one with the most race progress. When
// progress stalls for three seconds it backs out with alternating lock.
// Braking to zero holds there (brake state 1) until released, then reverses.
wpc::DriveInput released(wpc::DriveInput in,const wpc::RaceCar& car) {
    if(in.brake && car.speed.speed==0 && car.speed.brake_state==1) in.brake=false;
    return in;
}
struct Autopilot {
    int plan_left=0,stalled=0,escapes=0;
    float best=-1e9f;
    wpc::DriveInput plan;
    // The route cursor is the original's measure; the geodesic field is not
    // monotonic along every route (sh_t4's U, around (3500, 22000)). It still
    // gives the direction where the cursor stays flat (cc_t2's section end).
    static float measure(const wpc::RaceSession& r) {return r.player_route_progress()+0.2f*r.player_progress();}
    wpc::DriveInput operator()(const wpc::RaceSession& race) {
        if(race.phase()!=wpc::RaceSession::Phase::Racing) return {true,false,false,false};
        const float now=measure(race);
        if(now>best+0.002f) {best=now;stalled=0;} else ++stalled;
        if(stalled>90) {
            stalled=0;best=now;++escapes;
            const bool left=escapes&1;
            // Released first: braking to zero only reverses after a release.
            plan={false,true,left,!left};plan_left=45;return {};
        }
        if(plan_left>0) {--plan_left;return released(plan,race.player());}
        // Options: straight, steer for 10/30/60 updates, and reverse with
        // opposite lock for 30.
        static const int dirs[9]={0,1,-1,1,-1,1,-1,1,-1},durs[9]={0,10,10,30,30,60,60,30,30};
        float value_best=-1e9f;int chosen=0;
        for(int option=0;option<9;++option) {
            wpc::RaceSession sim=race;sim.set_robots_enabled(false);float penalty=0;const bool reverse=option>=7;
            for(int k=0;k<60;++k) {
                wpc::DriveInput in;
                if(reverse) in=k==0?wpc::DriveInput{}:k<durs[option]?wpc::DriveInput{false,true,dirs[option]<0,dirs[option]>0}:wpc::DriveInput{true,false,false,false};
                else in={true,false,dirs[option]>0&&k<durs[option],dirs[option]<0&&k<durs[option]};
                sim.update(released(in,sim.player()));
                // A glide off the course (cc_t4's wall slopes) only ends in a
                // return 400 updates later, beyond this look-ahead.
                if(sim.drowning() || sim.respawns()!=race.respawns() || sim.player_glides()!=race.player_glides()) {penalty=-100+k*0.1f;break;}
            }
            // Contacts cost a little, so the driver steers off walls and edges.
            const float value=measure(sim)+penalty-0.002f*float(sim.impacts()-race.impacts())+(option==0?0.0005f:0)-(reverse?0.001f:0);
            if(value>value_best) {value_best=value;chosen=option;}
        }
        if(chosen>=7) {plan={false,true,dirs[chosen]<0,dirs[chosen]>0};plan_left=29;return {};}
        return {true,false,dirs[chosen]>0,dirs[chosen]<0};
    }
};
// The port's own route follower (RoboDriver, 0x4e5d..) drives the player's
// car through its keys: the throttle as given, the steering keys following
// the robot's steer. The keys stop at the 40-degree lock (0x5129d8) while a
// robot's steer does not, so the throttle is released in turns past 80% of
// the lock above speed 0.3 (cc_t3's spiral ramp, sh_t3's helix).
struct RoboPilot {
    wpc::RoboDriver robot;wpc::MsvcRandom random{1};wpc::Vec3 previous{};bool started=false;
    wpc::DriveInput operator()(const wpc::RaceSession& race) {
        if(race.phase()!=wpc::RaceSession::Phase::Racing) return {true,false,false,false};
        const wpc::RaceCar& p=race.player();
        wpc::RoboCarState st;st.position=p.position;st.forward={{-std::sin(p.yaw),0,std::cos(p.yaw)}};
        st.previous=started?previous:p.position;started=true;previous=p.position;
        st.speed=p.speed.speed;st.top_speed=race.tuning().top_speed;st.surface=p.surface;
        const wpc::RoboControl c=robot.drive(race.track(),st,random);
        wpc::DriveInput in;in.accelerate=c.throttle==1;in.brake=c.throttle==2;
        const float want=wpc::robot_steering(c.steer),step=race.tuning().steer_step;
        if(want>p.steer+0.5f*step) in.left=true; else if(want<p.steer-0.5f*step) in.right=true;
        if(std::abs(want)>0.8f*0.6981317f && p.speed.speed>0.3f) in.accelerate=false;
        return released(in,p);
    }
};
// The route follower drives; when the progress stays flat for 300 updates
// the look-ahead planner takes over until the progress passes the stall by
// 0.02 or for 3000 updates (ct_t4's lower road, cc_t1's start).
struct HybridPilot {
    RoboPilot robo;Autopilot planner;int planner_left=0,flat=0,handovers=0;float best=-1e9f,resume=0;
    wpc::DriveInput operator()(const wpc::RaceSession& race) {
        if(race.phase()!=wpc::RaceSession::Phase::Racing) return {true,false,false,false};
        const float now=Autopilot::measure(race);
        if(now>best+0.002f) {best=now;flat=0;} else ++flat;
        if(planner_left>0) {
            --planner_left;
            if(!planner_left || now>resume) {planner_left=0;robo=RoboPilot();flat=0;best=now;}
            else return planner(race);
        }
        if(flat>300) {planner=Autopilot{};planner_left=3000;flat=0;resume=best+0.02f;best=now;++handovers;return planner(race);}
        return robo(race);
    }
};
void test_objects() {
    // 0x444be0 on an open three-point path: two segments, the tangent blended
    // into the next segment, the last point at t = 1 with the previous
    // segment's direction.
    const std::vector<wpc::Vec3> p{{{0,0,0}},{{100,0,0}},{{100,0,100}}};
    wpc::Vec3 at,tan;
    wpc::path_point(p,false,0.25f,at,tan);
    near(at[0],50,1e-3f,"GDPath point halfway along the first segment");
    near(tan[0],50,1e-3f,"GDPath tangent blends half into the next segment");near(tan[2],50,1e-3f,"GDPath tangent z");
    wpc::path_point(p,false,1,at,tan);
    near(at[2],100,1e-3f,"GDPath end at t = 1");near(tan[2],100,1e-3f,"GDPath end keeps the last segment");
    wpc::path_point(p,true,0.75f,at,tan);
    near(at[0],75,1e-3f,"closed GDPath: three segments");near(at[2],75,1e-3f,"closed GDPath third segment back to the first point");
}
void test_effects() {
    // WCarFx (0x47a460): dust from the rear wheels when driving forward.
    const wpc::Matrix4 frame=wpc::identity_matrix();
    std::array<wpc::WheelContact,4> wheels{};
    for(size_t w=0;w<4;++w) wheels[w]={true,{{wpc::wheel_offsets[w][0],0,wpc::wheel_offsets[w][1]}},4};
    wpc::CarEffects fx;
    fx.update(frame,0.5f,wheels,false);
    const auto live=[&]{size_t n=0;for(const auto& p:fx.puffs()) n+=p.age>0;return n;};
    require(live()==1,"one dust puff per update");
    const auto& first=fx.puffs()[0];
    require(first.age==1 && first.reel==2 && first.offset[2]==-60,"puff starts at a rear wheel, reel by surface (4: 2)");
    require((first.offset[0]<0)==(first.velocity[0]<0) && first.velocity[1]>=2 && first.velocity[1]<4,"puff thrown outwards and up 2..4");
    const wpc::Vec3 start=first.offset,velocity=first.velocity;
    fx.update(frame,0.5f,wheels,false);
    near(fx.puffs()[0].offset[1],start[1]+velocity[1],1e-5f,"puff integrates its velocity");
    near(fx.puffs()[0].velocity[1],velocity[1]-0.3f,1e-5f,"puff gravity 0.3");
    require(live()==2,"a second puff");
    for(int i=0;i<21;++i) fx.update(frame,0.5f,wheels,false);
    require(fx.puffs()[0].age==0 || fx.puffs()[0].age==1,"a puff lives 23 updates");
    wpc::CarEffects road;
    for(auto& w:wheels) w.surface=2;
    road.update(frame,0.5f,wheels,false);road.update(frame,-0.5f,wheels,false);
    require(std::none_of(road.puffs().begin(),road.puffs().end(),[](const auto& p){return p.age>0;}),"no dust on surface 2");
    // Deep water: a ripple on even updates, 15 growth steps of 0.4.
    for(auto& w:wheels) w.surface=0x800000;
    wpc::CarEffects water;
    water.update(frame,0,wheels,false);
    require(water.ripples().empty(),"no ripple on an odd update");
    water.update(frame,0,wheels,false);
    require(water.ripples().size()==1 && water.ripples()[0].base[14]==60,"ripple at a front wheel");
    for(auto& w:wheels) w.hit=false;
    for(int i=0;i<15;++i) water.update(frame,0,wheels,false);
    near(water.ripples().at(0).scale,7.f,1e-4f,"ripple grows to 1 + 15 x 0.4");
    water.update(frame,0,wheels,false);
    require(water.ripples().empty(),"ripple ends after 15 updates");
    // WSplash (0x473df0): the column rises and falls, then the clone ends.
    wpc::CarEffects splash;
    splash.update(frame,0,wheels,true);
    require(splash.splashes().size()==1,"splash cloned on entering deep water");
    float peak=0;int updates=0;
    while(!splash.splashes().empty() && updates<200) {
        splash.update(frame,0,wheels,false);++updates;
        if(!splash.splashes().empty()) peak=std::max(peak,splash.splashes()[0].height);
    }
    require(splash.splashes().empty() && updates>31 && updates<80,"splash runs through its 31 states");
    require(peak>1 && peak<2,"column height peaks once");
    // WKBumm (0x486960/0x486b20): eight pieces, the first straight ahead.
    wpc::CarEffects bumm;
    bumm.bumm(frame,0.5f);
    require(bumm.bumms().size()==1,"one WKBumm clone");
    const auto& piece=bumm.bumms()[0].pieces[0];
    require(piece.velocity[0]==0 && piece.velocity[2]>=53 && piece.velocity[2]<61 && piece.velocity[1]>=20 && piece.velocity[1]<38,
            "piece 0 forward with the car's speed x 100, up 20..38");
    require(piece.frame>=0 && piece.frame<5 && std::abs(bumm.bumms()[0].pieces[2].spin-(-16*0.0174533f))<1e-5f,"piece frames and spins");
    int life=0;
    while(!bumm.bumms().empty() && life<100) {bumm.update(frame,0,wheels,false);++life;}
    require(bumm.bumms().empty() && life>15 && life<60,"pieces fall back under height 1");
    // POUSSIERE (0x47b750): off by default; on, rear wheels by parity.
    for(auto& w:wheels) {w.hit=true;w.surface=2;}
    wpc::CarEffects quiet,dusty;dusty.set_smoke(true);
    quiet.update(frame,0.5f,wheels,false);dusty.update(frame,0.5f,wheels,false);
    require(std::none_of(quiet.smoke().begin(),quiet.smoke().end(),[](const auto& m){return m.age>0;}),"POUSSIERE off by default");
    require(dusty.smoke()[0].age==1 && dusty.smoke()[0].position[2]==-60 && dusty.smoke()[0].position[0]==-70,"odd update: rear-left wheel 2");
    dusty.update(frame,0.5f,wheels,false);
    require(dusty.smoke()[1].age==1 && dusty.smoke()[1].position[0]==70,"even update: rear-right wheel 3");
    // +0x25b: braking forward above 0.015 sets +0x25f for the release
    // update; a steered fast car slides above m_farolspeedlimit.
    {
        wpc::SkidState k;
        wpc::update_brake_skid(k,false,true,0.5f,0,true);
        require(k.braking && k.brake_timer==1 && k.marking(),"braking forward marks");
        wpc::update_brake_skid(k,true,false,0.4f,0,true);
        require(k.braking && k.brake_timer==0,"one update after the release");
        wpc::update_brake_skid(k,true,false,0.4f,0,true);
        require(!k.braking,"+0x158 below 0 clears the brake skid");
        wpc::update_brake_skid(k,false,true,0.01f,0,true);
        require(!k.braking,"no brake skid at 0.01");
        wpc::SkidState sl;
        wpc::update_slide(sl,1,0.3f,0.1f,1,true);
        require(!sl.sliding && sl.slide_timer==0,"the serialized timer 1 counts down first");
        wpc::update_slide(sl,1,0.3f,0.1f,1,true);
        require(sl.sliding && sl.slide_timer==3,"slide above the limit");
        wpc::update_slide(sl,1,0.08f,0.1f,1,true);
        require(sl.sliding,"the slide holds above 0.6 of the limit");
        wpc::update_slide(sl,1,0.05f,0.1f,1,true);
        require(!sl.sliding,"the slide ends below 0.6 of the limit");
        wpc::update_slide(sl,1,0.08f,0.1f,1.5f,true);wpc::update_slide(sl,1,0.08f,0.1f,1.5f,true);
        require(sl.sliding,"friction 1.5 scales x");
        // 0x470218: surface modifiers from the 0x52b558 rows.
        wpc::SurfaceGrip g;
        wpc::update_grip(g,0,4);
        require(g.top==0.85f && g.accel==0.9f && g.yaw==0.8f && g.friction==1,"sand: rows 6, 7, 8");
        wpc::update_grip(g,3,8);
        require(g.top==0.9f && g.accel==0.9f && g.yaw==0.9f,"dirt: rows 15, 16, 17");
        wpc::update_grip(g,0,0x80);
        require(g.top==1 && g.accel==0.7f && g.yaw==1.1f && g.friction==1.5f,"0x80: rows 18..20, friction 1.5");
        wpc::update_grip(g,0,0x40);
        require(g.top==0.9f && g.friction==1.5f,"ice keeps the friction");
        wpc::update_grip(g,0,0x1000000);
        require(g.top==0.9f,"other surfaces keep the modifiers");
        wpc::update_grip(g,0,2);
        require(g.top==1 && g.accel==1 && g.yaw==1 && g.friction==1,"road resets them");
        // +0x8db (0x466df6): doubles +0x8bf per update while it runs.
        {
            wpc::SurfaceGrip b;int counter=2;
            wpc::update_grip(b,0,4);wpc::update_boost(b,counter);
            require(b.accel==1.8f && counter==1,"boost doubles the sand acceleration");
            wpc::update_boost(b,counter);
            require(b.accel==3.6f && counter==0,"without a contact rewrite it compounds");
            wpc::update_grip(b,0,4);wpc::update_boost(b,counter);
            require(b.accel==0.9f,"the counter has run out");
        }
        // WackyAutoBomba (0x47e220): 25 held updates, +0x8db on the 25th.
        {
            wpc::BombHit h;h.type=0x80;
            int held=0,boosts=0,boost_at=-1;
            for(int i=0;i<100;++i) {
                const wpc::BombStep st=wpc::update_bomb(h);
                held+=st.hold;
                if(st.boost) {++boosts;boost_at=i;}
            }
            require(held==25 && boosts==1 && boost_at==25,"25 held updates, the boost on the 26th");
            require(!h.active(),"the hit ends after its sequence");
            // 0x47ed6e: m_smokeparticle on every 5th update of the hit, the
            // 10th ones with the spin negated; 16 over a 0x80 hit (79 + 2).
            {
                wpc::BombHit s;s.type=0x80;
                std::vector<int> smoke,flip;
                for(int i=1;s.active() && i<200;++i) {const wpc::BombStep st=wpc::update_bomb(s);if(st.smoke) smoke.push_back(i);if(st.flip) flip.push_back(i);}
                require(smoke.size()==16 && smoke.front()==5 && smoke[1]==10 && smoke.back()==80,"a smoke clone every 5 updates of the hit");
                require(flip.size()==8 && flip.front()==10,"every 10th clone spins the other way");
            }
            // 0x47ebea: m_BoomerangHit at 0.1, x 1.3 an update, then 1.5 from
            // the identity once +0x28 passes 8; 20 degrees an update; hidden
            // past +0x82 = 45.
            {
                wpc::BombHit s;s.type=0x10;
                wpc::update_bomb(s);
                require(s.spin.shown && s.spin.stage==1 && std::abs(s.spin.scale-0.1f)<1e-6f,"0x10: the model starts at 0.1");
                for(int i=0;i<8;++i) wpc::update_bomb(s);
                require(s.spin.stage==1 && std::abs(s.spin.scale-0.1f*std::pow(1.3f,8.f))<1e-5f,"it grows by 1.3 an update");
                wpc::update_bomb(s);
                require(s.spin.stage==2 && s.spin.from_identity && s.spin.scale==1.5f && std::abs(s.spin.turn-0.34906585f)<1e-6f,"the 10th update resets it to 1.5");
                int shown=10;while(s.active() && s.spin.shown) {wpc::update_bomb(s);if(s.spin.shown) ++shown;}
                require(shown==46,"shown for 46 updates");
            }
            // 0x47ea3b: m_bluepool at 0.2, x 1.15 in x and z through counter 17,
            // 52 waiting, x 0.8 for 8, hidden at 78.
            {
                wpc::BombHit s;s.type=4;
                const wpc::BombStep first=wpc::update_bomb(s);
                require(first.pool && s.pool.shown && s.pool.scale[1]==0.2f,"paint: the pool takes the car's frame at 0.2");
                for(int i=0;i<17;++i) wpc::update_bomb(s);
                require(s.pool.stage==2 && std::abs(s.pool.scale[0]-0.2f*std::pow(1.15f,17.f))<1e-4f && s.pool.scale[1]==0.2f,"it spreads for 17 updates");
                int c=18;while(s.pool.stage<5) {wpc::update_bomb(s);++c;}
                require(c==79 && !s.pool.shown,"hidden on the 79th update (counter 78)");
            }
            // 0x47e277: the tank gun's hit. The model at the car's frame x 2,
            // the car hidden through counter 41; segments 1, 3, 4 and 5 pulse
            // (t = +0x28 x 20 degrees) for 35 updates; then (0.7, 0.1, 0.7),
            // x (1.1, 1, 1.1) for 6 updates, hidden at counter 50.
            {
                wpc::BombHit s;s.type=2;
                int hidden=0,first_hidden=0,last_hidden=0,overridden=0,first_override=0,last_override=0,last_shown=0;
                float y43=0,x49=0;std::array<std::array<float,3>,4> seg10{};
                for(int i=1;s.active() && i<200;++i) {
                    const wpc::BombStep st=wpc::update_bomb(s);
                    if(i==1) require(st.tank_axes && st.tank_position && s.tank.shown && s.tank.scale[0]==2,"type 2: the model takes the car's frame at x 2");
                    if(i>=7 && i<=42) require(!st.tank_axes && st.tank_position,"type 2: then the position alone");
                    if(s.tank.car_hidden) {++hidden;if(!first_hidden) first_hidden=i;last_hidden=i;}
                    if(s.tank.override) {++overridden;if(!first_override) first_override=i;last_override=i;}
                    if(s.tank.shown) last_shown=i;
                    if(i==10) seg10=s.tank.segments;
                    if(i==43) y43=s.tank.scale[1];
                    if(i==49) x49=s.tank.scale[0];
                }
                require(hidden==42 && first_hidden==1 && last_hidden==42,"type 2: the car is hidden for counters 0..41");
                require(overridden==35 && first_override==7 && last_override==41,"type 2: the segments pulse for 35 updates");
                const float t=80*0.017453292f,a=std::sin(2*t)*0.5f+1,b=std::sin(t)*0.5f+1,c4=std::sin(4*t)*0.5f+1;
                require(std::abs(seg10[0][1]-b)<1e-5f && std::abs(seg10[1][0]-c4)<1e-5f && std::abs(seg10[2][0]-a)<1e-5f && std::abs(seg10[3][2]-a)<1e-5f,"type 2: the curves at +0x28 = 4");
                require(std::abs(y43-0.2f)<1e-6f && std::abs(x49-1.4f*std::pow(1.1f,6.f))<1e-4f,"type 2: flattened to 0.1, then spread by 1.1 six times");
                require(last_shown==50,"type 2: the model is hidden at counter 50");
            }
            // 0x4e5b0a: the robots aim each item with its WPowerUpSprite's
            // m_range and m_visangle (general.wgm, read under Wine).
            {
                const wpc::ItemAims a=wpc::default_item_aims();
                require(a[14].range==10000 && std::abs(a[14].angle-0.34906585f)<1e-6f && a[25].range==10000 && a[24].range==3000,"the breath and missile aim 10000 at 20 degrees, TNT 3000");
                require(a[9].range==5000 && a[9].angle>6.28f && a[15].range==8000 && a[11].range==2000,"the FireSiren all round at 5000, the boomerang 8000, the kicker 2000");
            }
            // WRocketTail (0x4ea960): every update the points shift back, point 0
            // takes the new position and axes (x 3), the phase steps 0.3; the
            // breath's film loops 0..7.
            {
                wpc::Projectile p;
                for(auto& t:p.tail) t={{{0,0,0}},{{0,1,0}},{{1,0,0}},1};
                p.position={{0,0,300}};wpc::update_tail(p,{{0,1,0}},{{1,0,0}});
                p.position={{0,0,600}};wpc::update_tail(p,{{0,1,0}},{{1,0,0}});
                require(p.tail[0].position[2]==600 && p.tail[1].position[2]==300 && p.tail[2].position[2]==0 && p.tail[0].k==3 && p.tail[2].k==1,"the tail shifts one point an update");
                require(std::abs(p.tail_phase-0.6f)<1e-6f && p.tail_updates==2,"its phase steps 0.3");
                for(int i=0;i<6;++i) wpc::update_tail(p,{{0,1,0}},{{1,0,0}});
                require(p.film==0,"the breath's film returns to frame 0 after frame 7");
            }
            // flamesmoke (0x418310): t = 2 x age; scale 2 x 0x5264c8(t), alpha
            // 255 x 0x526588(t); it rises 30 (x 0.99) past t = 10; gone past 100.
            {
                wpc::CarEffects fx(1);
                fx.hit_smoke(wpc::identity_matrix(),true);
                const std::array<wpc::WheelContact,4> none{};
                fx.update(wpc::identity_matrix(),0,none,false);
                const auto& p=fx.hit_smokes().at(0);
                require(p.age==1 && std::abs(p.scale-0.08f)<1e-6f && p.alpha==25 && p.position[1]==0,"age 1: scale 0.08, alpha 25, not moving");
                require(p.spin<=0 && std::abs(p.angle-(p.spin-0.13962634f))<1e-5f,"a flipped spin turns the other way");
                for(int i=0;i<5;++i) fx.update(wpc::identity_matrix(),0,none,false);
                require(std::abs(fx.hit_smokes().at(0).position[1]-29.7f)<1e-4f && std::abs(fx.hit_smokes().at(0).position[0]-0.2f)<1e-6f,"age 6: up 29.7, drifting 0.2");
                for(int i=0;i<44;++i) fx.update(wpc::identity_matrix(),0,none,false);
                require(fx.hit_smokes().size()==1 && fx.hit_smokes()[0].age==50 && fx.hit_smokes()[0].alpha==0,"age 50: faded out");
                fx.update(wpc::identity_matrix(),0,none,false);
                require(fx.hit_smokes().empty(),"past t = 100 it is gone");
            }
            // Controller 0 (0x440e30): a model moving at a constant velocity
            // gives no sway; one lateral kick (x 20 in one update) rolls it
            // by b0 x -15 (the clamp) on the next step, then rings down.
            {
                wpc::SwayController sw;wpc::MotionPairs pairs(9*2,{0.f,0.f});
                std::array<float,16> m{{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
                const auto step=[&]{std::fill(pairs.begin(),pairs.end(),std::array<float,2>{0.f,0.f});sw.update(m,pairs);};
                for(int i=0;i<40;++i) {m[14]+=10;step();}
                float rest=0;for(float v:sw.values()) rest=std::max(rest,std::abs(v));
                require(rest<0.05f,"sway: a steady drive settles");
                for(int i=0;i<200;++i) {m[14]+=10;step();}
                m[12]+=20;m[14]+=10;step();
                const float c0=2*0.9f*std::cos(0.4f),c1=-0.81f,b0=(1-c0-c1)*0.5f*0.28985509f;
                float roll=sw.values()[5],before=0;for(int i=0;i<1;++i) before=roll;
                require(std::abs(roll-b0*-15)<1e-4f,"sway: the kick's roll input is clamped to -15 (gain 0.5)");
                require(pairs[5][1]==roll && pairs[9+5][1]==roll && pairs[5][0]==0,"sway: every segment takes the value (negative in b)");
                float peak=0;for(int i=0;i<60;++i) {m[12]+=20;m[14]+=10;step();peak=std::max(peak,std::abs(sw.values()[5]));}
                require(peak>std::abs(before) && std::abs(sw.values()[5])<0.05f,"sway: it rings and settles at a steady velocity");
            }
            // WReszecske (0x47d050, 0x47d130): eleven chunks 30..70 up, rising
            // 20..38 x 0.7..1.29 and falling by 1.5 an update; gone after 31 steps.
            {
                wpc::CarEffects fx(1);
                fx.tank_chunks(wpc::identity_matrix());
                require(fx.chunks().size()==11 && fx.chunks()[0].position[1]==30 && fx.chunks()[10].position[1]==70,"tank hit: eleven chunks from 30 to 70 up");
                for(const auto& c:fx.chunks()) require(c.velocity[1]>=20*0.7f-1e-3f && c.velocity[1]<=37.8f*1.29f+1e-3f && std::abs(c.velocity[0])<=18*1.29f+1e-3f,"each thrown up within the init's range");
                const float v0=fx.chunks()[0].velocity[1],y0=fx.chunks()[0].position[1];
                const std::array<wpc::WheelContact,4> none{};
                fx.update(wpc::identity_matrix(),0,none,false);
                require(std::abs(fx.chunks()[0].velocity[1]-(v0-1.5f))<1e-4f && std::abs(fx.chunks()[0].position[1]-(y0+v0-1.5f))<1e-3f,"the first step falls by 1.5");
                for(int i=0;i<30;++i) fx.update(wpc::identity_matrix(),0,none,false);
                require(fx.chunks().size()==11,"still there after 31 steps");
                fx.update(wpc::identity_matrix(),0,none,false);
                require(fx.chunks().empty(),"gone past +0x40 = 30");
            }
            // The end of each type (white model colours): +0x36 at D, cleared
            // the update after. 0x80: the fade out (30 updates), set, held 11,
            // back (30), restored, then 0x47d890 returns 1 at 73.
            // Renderer 1's byte path (Wine): the fades take 33 updates; on
            // vehicle 0's colours 0x80 ends as under Wine (+0x36 after 80).
            for(const auto& [type,done]:std::vector<std::pair<uint16_t,int>>{{0x200,25},{0x400,27},{0x100,46},{0x80,79},{4,78},{2,97},{0x1000,0}}) {
                wpc::BombHit e;e.type=type;int n=0;
                while(e.active() && n<200) {wpc::update_bomb(e);++n;}
                require(n==done+2,("hit type "+std::to_string(type)+" lasts done + 2 updates").c_str());
            }
            {
                wpc::Tint t;t.target={{10/255.f,10/255.f,0}};
                const std::array<float,3> white{{1,1,1}};
                int k=0;while(t.mode==0) {wpc::step_tint(t,white,white);++k;}
                require(k==33 && t.f>0.9f,"the byte fade (truncated / 13) reaches every step below 2 in 33 updates");
                const std::array<float,3> c=wpc::tint_colour(t,white);
                require(std::lround(c[0]*255)==33 && std::lround(c[2]*255)==23,"it stops short of the target: (33, 33, 23)");
                wpc::step_tint(t,white,white);
                require(wpc::tint_colour(t,white)[0]==10/255.f,"mode 1 writes the target");
                {
                    wpc::BombHit v0;v0.type=0x80;v0.colour_lo={{0.231f,0.047f,0}};v0.colour_hi={{1,1,1}};int n=0;
                    while(v0.active() && n<200) {wpc::update_bomb(v0);++n;}
                    require(n==81,"0x80 on vehicle 0's colours: +0x36 after 80 updates, as under Wine (cc_t1, robot 0)");
                }
                wpc::BombHit sq;sq.type=0x200;wpc::update_bomb(sq);
                require(std::abs(sq.scale_y-0.1f)<1e-6f,"0x200 squashes the car to 0.1");
                for(int i=0;i<23;++i) wpc::update_bomb(sq);
                require(sq.scale_y>1,"then springs past 1");
                wpc::BombHit pk;pk.type=0x400;wpc::update_bomb(pk);
                require(pk.tint.f==1 && pk.tint.target[2]==0,"the storm darkens the car");
            }
            const wpc::Landmine m=wpc::drop_landmine({{1,2,3}},0.5f,-1);
            require(m.position[1]==12 && m.state==0 && m.life==300,"the mine starts 10 above the car");
        }
        // WPU_Shield (0x4d7870) and WackyAutoBomba's activation (0x47d450).
        {
            wpc::BombHit h;
            wpc::Shield sh=wpc::start_shield(h);
            require(h.guard==2 && sh.timer==200,"the shield sets guard 2 for m_shieldtime");
            require(!wpc::start_bomb(h,0x80) && h.guard==1 && !h.active(),"it absorbs one hit");
            wpc::update_shield(sh,h);
            require(sh.state==2 && h.guard==4,"then ends");
            int n=0;while(!sh.done && n<20) {wpc::update_shield(sh,h);++n;}
            require(sh.done && h.guard==1 && n==7,"the model hides at timer 1, the shield goes the update after");
            require(wpc::start_bomb(h,0x80) && h.active(),"hits take again");
            wpc::BombHit p;wpc::Shield ps=wpc::start_shield(p);
            require(!wpc::start_bomb(p,4) && p.guard==8,"paint leaves guard 8");
            require(!wpc::start_bomb(p,0x40),"guard 8 still absorbs");
            wpc::update_shield(ps,p);
            require(ps.timer==29,"guard 8 cuts the shield to 30");
        }
        // WBreath (0x475040): aiming, the cone and the steering.
        {
            wpc::Projectile pr;require(wpc::breath_weapon(25,pr) && pr.type==8 && pr.object==0x38c,"icon 25 is the missile");
            require(wpc::breath_weapon(16,pr) && pr.type==2 && wpc::breath_weapon(15,pr) && pr.type==0x10 && pr.p.maneuver==50,"tank gun and boomerang");
            require(!wpc::breath_weapon(19,pr),"the mine is no projectile");
            wpc::breath_weapon(25,pr);pr.velocity={{0,0,300}};
            // The traps' 0x4a71e0 angle 2 pi accepts any direction.
            require(wpc::target_distance({{0,0,-150}},0,6.2831855f,200)==22500 && wpc::target_distance({{150,0,0}},0,6.2831855f,200)==22500,"a 2 pi query sees behind and aside");
            wpc::aim_projectile(pr,{{300,0,0}});
            near(pr.velocity[0],300,1e-3f,"aimed at ABSspd");
            pr.velocity={{0,0,300}};
            const wpc::Vec3 side{{2000,0,2000}};
            wpc::steer_projectile(pr,&side,true,-200);
            require(pr.velocity[0]>0 && pr.velocity[0]<=8.01f,"turns by maneuverspd");
            near(pr.velocity[1],0,1e-3f,"holds H above the ground");
            pr.position={{0,500,0}};wpc::steer_projectile(pr,nullptr,true,0);
            near(pr.velocity[1],-30,1e-3f,"lowers at most loweringspeed");
            pr.position={{0,300,0}};wpc::steer_projectile(pr,nullptr,true,0,2);
            near(pr.velocity[1],-100*30/200.f/2,1e-3f,"lowering scaled by loweringspeed / H over the reach");
            pr.position={{0,-300,0}};wpc::steer_projectile(pr,nullptr,true,0);
            require(std::abs(pr.velocity[1]-100)<1e-3f && std::sqrt(pr.velocity[0]*pr.velocity[0]+pr.velocity[2]*pr.velocity[2])<=300.01f,"climbs at most climb; the horizontal speed is capped at ABSspd");
        }
        // The activation's hop and spin (0x47d66f) and the slot rotation.
        {
            require(wpc::bomb_kick(0x80).hop && std::abs(wpc::bomb_kick(0x80).spin-12.566371f)<1e-4f,"a mine hops and spins twice");
            require(!wpc::bomb_kick(0x400).hop && wpc::bomb_kick(0x1000).spin<0 && wpc::bomb_kick(0x800).spin>0 && !wpc::bomb_kick(0x800).hop,"the detour turns half round");
            wpc::Spin sp;wpc::request_spin(sp,3.1415927f);
            float turned=0;int n=0;
            for(;n<200;++n) {turned+=sp.step;wpc::decay_spin(sp);if(sp.left==0 && sp.step==0 && n>0) break;}
            near(turned,3.1415927f,1e-3f,"the whole turn");
            require(n>20 && n<60,"a ninth per update, the rest below 3 degrees");
            wpc::BombHit d;d.type=0x1000;
            require(!wpc::update_bomb(d).hold && d.active() && !wpc::update_bomb(d).hold && !d.active(),"0x1000 holds nothing and ends on its second update");
        }
        // 0x4e4a10: the robot turns toward a power-up ahead.
        {
            wpc::RoboCarState car;car.forward={{0,0,1}};car.speed=0.5f;
            car.boxes[0]={{100,0,1000}};car.box_count=1;
            float s=0;wpc::pickup_turn(s,car);
            require(s!=0 && std::abs(s)<0.35f,"a box 6 degrees off steers the robot");
            car.boxes[0]={{1000,0,1000}};s=0;wpc::pickup_turn(s,car);
            require(s==0,"45 degrees off is outside the 20-degree cone");
            car.boxes[0]={{0,0,3100}};s=0.1f;wpc::pickup_turn(s,car);
            require(s==0.1f,"beyond 3000 is ignored");
            car.boxes[0]={{100,0,1000}};car.speed=-0.1f;s=0;wpc::pickup_turn(s,car);
            require(s==0,"not while reversing");
        }
        // 0x4e4d30: the rank-based speed scaling.
        {
            const wpc::RankRow r=wpc::rank_row(0,0);
            require(r.lead==2000 && r.gap==6000 && r.close==10000 && r.boost==0.2f && wpc::rank_row(3,0).far_catch_up==1.7f,"0x52b910 rows");
            wpc::RankView v;v.rank=3;v.lap=2;v.player=20000;
            v.progress=10000;require(wpc::rank_scale(v,r,0,0)==1,"behind the player, nobody close ahead: catch up");
            v.progress=23000;require(wpc::rank_scale(v,r,0,0)==0,"past player + lead: no catch-up");
            v.progress=15000;v.has_ahead=true;v.ahead=18000;require(wpc::rank_scale(v,r,0,0)==-1,"near the player and the car ahead: hold back");
            v.lap=1;require(wpc::rank_scale(v,r,0,0)==0,"no hold on the first lap");
            v.lap=2;v.rank=5;v.ahead=v.progress+9000;require(wpc::rank_scale(v,r,0,0)==-1,"rank 5: hold within close of the car ahead");
            v.has_ahead=false;require(wpc::rank_scale(v,r,0,0)==0 && wpc::rank_scale(v,r,4,0)==1,"rank 5 alone; environment 4 uses the main rule");
            int e=-1,c=-1;
            require(wpc::course_indices("x/sh/SH_T3.WGM",e,c) && e==1 && c==2 && !wpc::course_indices("menu.WGM",e,c),"course indices from the file name");
        }
        // 0x4a71e0's racer test.
        {
            require(wpc::target_distance({{0,0,3000}},0,1.0472f,6000)==9000000,"ahead: the integer squared distance");
            require(wpc::target_distance({{0,0,-3000}},0,1.0472f,6000)<0,"behind: outside the cone");
            require(wpc::target_distance({{0,0,6000}},0,1.0472f,6000)<0,"the box is strict (|d| < range)");
            require(wpc::target_distance({{4300,0,4300}},0,3.1415927f,6000)<0,"within the box but beyond range^2");
            require(wpc::target_distance({{0.9f,0.9f,2.9f}},0,1.0472f,6000)==4,"truncated components (0, 0, 2)");
            // Heading 3: forward (-sin 3, cos 3); a target at heading -3
            // lies 0.28 rad away once wrapped.
            const wpc::Vec3 d{{-std::sin(-3.f)*1000,0,std::cos(-3.f)*1000}};
            require(wpc::target_distance(d,3,0.3f,6000)>0 && wpc::target_distance(d,3,0.2f,6000)<0,"the angle is wrapped to +/- pi");
            require(wpc::target_distance({{1000,0,0}},0,1.5707964f,6000)<0 && wpc::target_distance({{1000,0,0}},0,1.5708f,6000)>0,"strictly within the half-angle");
        }
        // WPU_Tnt (0x4dcac0, 0x4dce10).
        {
            wpc::Tnt t=wpc::throw_tnt({{0,0,0}},{{0,0,1}},-1,nullptr);
            require(t.position[1]==100 && t.velocity[2]==300 && t.velocity[1]==25,"thrown 100 up at (0, 25, 300)");
            const wpc::Vec3 at{{300,100,0}};
            wpc::Tnt a=wpc::throw_tnt({{0,0,0}},{{0,0,1}},-1,&at);
            near(a.velocity[0],300,1e-3f,"aimed at the target");near(a.velocity[1],25,1e-3f,"plus 25 up");
            t.velocity={{0,-50,300}};
            require(wpc::bounce_tnt(t,{{0,0,0}},{{0,1,0}},nullptr),"first bounce");
            require(t.position[1]==10 && std::abs(t.speed-240)<1e-3f && t.velocity[1]==50 && std::abs(t.velocity[2]-240)<1e-3f,"reflected, 10 up, x 0.8");
            require(wpc::bounce_tnt(t,{{0,0,0}},{{0,1,0}},nullptr) && !wpc::bounce_tnt(t,{{0,0,0}},{{0,1,0}},nullptr),"the third bounce explodes");
        }
        {
            const wpc::VehicleTuning v=wpc::vehicle_tuning(0);
            wpc::SurfaceGrip sand;wpc::update_grip(sand,0,4);
            wpc::SpeedState q{0.6f,0};
            wpc::update_speed(q,v,{true,false,false,false},sand);
            require(std::abs(q.speed-0.595f)<1e-6f,"sand caps the speed at row[0] x 0.85");
            q={0.3f,0};wpc::SpeedState r=q;
            wpc::update_speed(q,v,{true,false,false,false},sand);wpc::update_speed(r,v,{true,false,false,false});
            near(q.speed-0.3f,(r.speed-0.3f)*0.9f,1e-6f,"sand acceleration x 0.9");
            near(wpc::yaw_step(1,0.5f,v,1,false,0.8f),wpc::yaw_step(1,0.5f,v,1)*0.8f,1e-6f,"sand yaw x 0.8");
        }
        // +0x883: set on the start update only; robots restart at once.
        wpc::SkidState kk;kk.slide_timer=0;
        wpc::update_slide(kk,1,0.3f,0.1f,1,true);
        require(kk.kick && std::abs(kk.started-0.3f)<1e-6f,"slide start sets +0x883 and keeps x");
        wpc::update_slide(kk,1,0.3f,0.1f,1,true);
        require(!kk.kick && kk.started==0 && kk.sliding,"+0x883 clears while the timer runs");
        wpc::SkidState rk;rk.slide_timer=0;
        wpc::update_slide(rk,1,0.3f,0.1f,1,true,true);
        require(rk.kick && rk.slide_timer==0,"a robot puts the timer back to 0");
        wpc::update_slide(rk,1,0.3f,0.1f,1,true,true);
        require(rk.kick,"a robot restarts on the next update");
        // The kick: acceleration x 0.2, forward braking x 0.1.
        const wpc::VehicleTuning v0=wpc::vehicle_tuning(0);
        wpc::SpeedState a{0.3f,0},b=a;
        wpc::update_speed(a,v0,{true,false,false,false});wpc::update_speed(b,v0,{true,false,false,false},{},true);
        near(b.speed-0.3f,(a.speed-0.3f)*0.2f,1e-6f,"slide kick: acceleration x 0.2");
        a={0.3f,0};b=a;
        wpc::update_speed(a,v0,{false,true,false,false});wpc::update_speed(b,v0,{false,true,false,false},{},true);
        near(0.3f-b.speed,(0.3f-a.speed)*0.1f,1e-6f,"slide kick: braking x 0.1");
        // The push: x row[4] 1.2 along the probe diagonal, outward.
        const wpc::SlidePush right=wpc::slide_push(-0.4f,0.2f,102,v0),left=wpc::slide_push(0.4f,0.2f,102,v0);
        near(right.right,-104*0.2f*1.5f*1.2f,1e-4f,"steering right pushes left (FL - RR)");
        near(left.right,104*0.2f*1.5f*1.2f,1e-4f,"steering left pushes right (FR - RL)");
        near(right.forward,204*0.2f*1.5f*1.2f,1e-4f,"forward part 2 x front");
        require(wpc::farol_bend_mod(4)==7 && wpc::farol_bend_mod(0)==13,"m_farolbendmod by vehicle");
        // Robots skip the |row[2] - speed| x 0.8 yaw term.
        near(wpc::yaw_step(1,0.5f,v0,1,true),0.08726646f,1e-6f,"robot yaw: the clamped term alone");
    }
    // Tyre tracks (0x47a590): sand (4) marks with shade 0x30 across the
    // vehicle's half width; segments shift down and fade by 6.
    wpc::CarEffects tracks;tracks.set_track_width(wpc::CarEffects::track_width(3));
    for(auto& w:wheels) w.surface=4;
    tracks.update(frame,0.5f,wheels,false);
    const auto& t0=tracks.tracks()[0][0];
    require(t0.active && t0.shade==0x30 && t0.fade==100 && t0.a[0]==-76 && t0.b[0]==-64,"track segment spans the contact -/+ 6");
    tracks.update(frame,0.5f,wheels,false);
    require(tracks.tracks()[0][1].active && tracks.tracks()[0][1].fade==94,"track segment moves down and fades");
    for(auto& w:wheels) w.surface=2;
    tracks.update(frame,0.5f,wheels,false);
    require(!tracks.tracks()[0][0].active && tracks.tracks()[0][2].fade==88,"no track on surface 2 without a skid");
    tracks.update(frame,0.5f,wheels,false,frame,true);
    require(tracks.tracks()[0][0].active && tracks.tracks()[0][0].shade==0x5c,"skid marks 0x5c on surface 2");
    for(int i=0;i<16;++i) tracks.update(frame,0.5f,wheels,false);
    require(std::none_of(tracks.tracks()[1].begin(),tracks.tracks()[1].end(),[](const auto& s){return s.active;}),"tracks end after 16 updates");
}
wpc::Vec3 transform_point(const wpc::Matrix4& m,const wpc::Vec3& p) {
    return {{m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]}};
}
// A battle arena (*_b.wgm, mode 1): the two spawn frames (0x49eb70), no
// laps, the hits received (+0x3a3) and the end at five (0x4a3136, state 10).
void test_battle(const wpc::RaceTrack& track,const std::string& path) {
    require(track.battle(),"an arena loads as a battle");
    wpc::RaceSession battle(track,3,1,1);
    require(battle.opponents().size()==1 && battle.mode()==1,"battle: the player and one robot");
    const auto& a=track.battle_frame(0);const auto& b=track.battle_frame(1);
    require(std::abs(battle.player().position[0]-a[12])<1 && std::abs(battle.player().position[2]-a[14])<1,"battle: the player at the +0x48 spawn");
    require(std::abs(battle.opponents()[0].car.position[0]-b[12])<1 && std::abs(battle.opponents()[0].car.position[2]-b[14])<1,"battle: the robot at the +0x50 spawn");
    bool rejected=false;try {wpc::RaceSession(track,3,1,0);} catch(const std::runtime_error&) {rejected=true;}
    require(rejected,"an arena refuses a race mode");
    for(int i=0;i<200;++i) battle.update({});
    int hits=0;
    for(int i=0;i<4000 && battle.finish_state()!=10;++i) {
        if(i%120==0 && battle.hit(0,0x8)) ++hits; // a bomb on the robot
        battle.update({});
    }
    require(battle.finish_state()==10 && battle.frags(0)==5 && !battle.battle_lost() && battle.laps_completed()==0,"five hits end the battle, the player wins");
    require(battle.accept(),"Enter at once in one player (0x4a31fe)");
    // Two players (the menu's only battle, page 47): player 2 at the second
    // spawn; its fifth hit ends with the counter at 60 (0x4a31ef).
    wpc::RaceSession two(track,3,1,1,4);
    require(two.two_players() && std::abs(two.opponents()[0].car.position[0]-b[12])<1,"two-player battle: player 2 at the +0x50 spawn");
    for(int i=0;i<200;++i) two.update({},{});
    for(int i=0;i<4000 && two.finish_state()!=10;++i) {if(i%120==0) two.hit(0,0x8);two.update({},{});}
    require(two.finish_state()==10 && two.finish_counter()==60 && !two.accept() && two.frags(0)==5,"two-player battle end waits 60 updates");
    std::printf("%s: battle ends after %d hits\n",path.c_str(),hits);
}
void test_course(const std::string& path) {
    wpc::Archive archive;archive.load(path);
    wpc::Scene scene;scene.load(archive);
    const bool arena=path.size()>6 && (path.compare(path.size()-6,6,"_b.WGM")==0 || path.compare(path.size()-6,6,"_b.wgm")==0);
    const wpc::RaceTrack track(archive,scene,arena);
    if(arena) {test_battle(track,path);return;}
    if(path.find("cc_t1")!=std::string::npos) {
        // Zartter #12 (identity rotation, box -60..8921, 0..1301, -700..1437):
        // no weather inside (0x456180).
        require(track.shelters()==2,"cc_t1: two Zartter boxes");
        const wpc::Matrix4 m=wpc::unwrapped_scene_transform(scene.root(),12);
        const float cx=m[12]+m[0]*4430,cy=m[13]+m[5]*650,cz=m[14]+m[10]*368;
        require(track.sheltered({{cx,cy,cz}}),"cc_t1: the box centre is sheltered");
        require(!track.sheltered({{cx,cy+2000,cz}}),"cc_t1: above the box is not");
    }
    if(path.find("dr_t2")!=std::string::npos) {
        // WRotoHill (0x4cf6f0): caught within m_88, it turns and rises, then lets go.
        wpc::CourseObjects o=track.objects();
        require(o.roto_hills().size()==1 && o.model_spins().size()==1,"dr_t2: one WRotoHill, one spinning model");
        const wpc::Vec3 c=o.roto_hills()[0].centre;
        wpc::CarProbe car;car.position={{c[0]+500,c[1],c[2]}};car.robot=true;car.section=o.roto_hills()[0].sections[0];
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect{0};
        o.update({car},hits,effect);
        require(effect[0]==0xb && o.roto_hills()[0].holds[0].phase==1,"dr_t2: a robot in range is caught and told to use its gadget");
        const float y=car.position[1];int held=0;
        for(int i=0;i<1000 && o.roto_hills()[0].holds[0].phase!=4;++i) {
            o.update({car},hits,effect);
            if(!o.held().empty()) {++held;car.position=o.held()[0].position;}
        }
        require(o.roto_hills()[0].holds[0].phase==4 && held>100 && car.position[1]>y+100,"dr_t2: the car turns and rises until let go");
        car.position={{c[0]+20000,c[1],c[2]}};
        o.update({car},hits,effect);
        require(o.roto_hills()[0].holds[0].phase==0,"dr_t2: past 10000 the table is free again");
        // WBumSteer (0x4cede0): the runners run their paths, the gates open behind
        // them and shut; a touched gate opens.
        {
            wpc::CourseObjects b=track.objects();
            require(b.bum_steers().size()==1,"dr_t2: one WBumSteer");
            std::vector<int> none;
            const auto& w=b.bum_steers()[0];
            b.update({},hits,none);
            require(w.state==0 && w.active,"dr_t2: the runners start");
            const wpc::Matrix4 first=w.runner_frame[0];
            for(int i=0;i<5;++i) b.update({},hits,none);
            require(w.runner_frame[0][12]!=first[12] || w.runner_frame[0][14]!=first[14],"dr_t2: a runner moves along its path");
            bool one=false,two=false,three=false,back=false;float widest=0;
            for(int i=0;i<3000;++i) {
                b.update({},hits,none);
                one=one||w.state==1;two=two||w.state==2;three=three||w.state==3;back=back||(three&&w.state==0);
                widest=std::max(widest,std::abs(std::atan2(w.gate_frame[0][1],w.gate_frame[0][0])-std::atan2(w.gate_home[0][1],w.gate_home[0][0])));
            }
            require(one && two && three && back,"dr_t2: the states cycle");
            require(widest>1.0f && widest<1.6f,"dr_t2: the gates open towards 75 degrees");
            int solid=0;
            for(const auto& s:b.solid_spheres()) solid+=s.bump==0;
            require(solid>=2,"dr_t2: the gates have solid spheres");
            b.gate_touched(0,1);
            require(w.bits==2,"dr_t2: touching the second gate sets bit 2");
        }
        // WVolcano (0x4c9cf0) only pulses: no car is ever held.
        wpc::CourseObjects v=track.objects();
        require(v.geysers().size()>=1,"dr_t2: its volcano loads as a pulsing column");
        for(const auto& g:v.geysers()) if(g.volcano) {
            wpc::CarProbe in;in.position=g.aim;
            for(int i=0;i<300;++i) {v.update({in},hits,effect);require(v.lifts().empty(),"dr_t2: the volcano holds no car");}
        }
    }
    if(path.find("cc_t3")!=std::string::npos) {
        // WDicksTrick2 (0x4c7fe0): one road piece; a car coming and going redraws it.
        wpc::CourseObjects o=track.objects();
        require(o.dicks_roads().size()==1 && o.dicks_roads()[0].section>=0,"cc_t3: one WDicksTrick2 with its route node");
        std::vector<wpc::ObjectHit> hits;std::vector<int> none;
        o.update({},hits,none);
        const auto& r=o.dicks_roads()[0];
        // A point on a piece: its first triangle's centroid, in the world.
        const auto on=[&](size_t k) {
            const auto& t=r.triangles[k].at(0);const wpc::Matrix4& f=r.world[k];
            const float x=(t[0][0]+t[1][0]+t[2][0])/3,y=(t[0][1]+t[1][1]+t[2][1])/3,z=(t[0][2]+t[1][2]+t[2][2])/3;
            return wpc::Vec3{{f[0]*x+f[4]*y+f[8]*z+f[12],f[1]*x+f[5]*y+f[9]*z+f[13],f[2]*x+f[6]*y+f[10]*z+f[14]}};
        };
        const wpc::Vec3 a=on(size_t(r.chosen)),b=on(size_t((r.chosen+1)%3));
        require(o.log_ground(a[0],a[2],a[1]+400,a[1]-400).hit,"cc_t3: the piece in place carries cars");
        require(!o.log_ground(b[0],b[2],b[1]+400,b[1]-400).hit,"cc_t3: the others do not");
        std::array<bool,3> seen{};
        wpc::CarProbe car;car.position=r.centre;
        std::vector<int> effect{0};
        for(int i=0;i<60;++i) {
            o.update({car},hits,effect);
            require(r.triggered,"cc_t3: a car in range holds the piece");
            o.update({},hits,none);
            require(!r.triggered,"cc_t3: no car: a new piece");
            seen[size_t(r.chosen)]=true;
        }
        require(seen[0] && seen[1] && seen[2],"cc_t3: any piece can be drawn");
    }
    if(path.find("sh_t4")!=std::string::npos) {
        // WShiftedRoad (0x4cbb60): one of three slabs swings up; the route follows.
        wpc::CourseObjects o=track.objects();
        require(o.shifted_roads().size()==1,"sh_t4: one WShiftedRoad");
        std::vector<wpc::ObjectHit> hits;std::vector<int> none;
        const auto& r=o.shifted_roads()[0];
        require(r.section>=0,"sh_t4: its route node is found");
        o.update({},hits,none);
        require(r.chosen>=0 && r.chosen<3 && r.timer==r.reload,"sh_t4: a slab is drawn");
        const int first=r.chosen;
        for(int i=0;i<60;++i) o.update({},hits,none);
        const size_t c=size_t(first);
        require(std::abs(r.frame[c][5])<0.4f,"sh_t4: the chosen slab swings towards 90 degrees");
        int changes=0,last=r.chosen;
        for(int i=0;i<3000;++i) {o.update({},hits,none);if(r.chosen!=last) {++changes;last=r.chosen;}}
        require(changes>=3,"sh_t4: the chosen slab changes");
        std::printf("%s: WShiftedRoad links %d %d %d, %zu weights\n",path.c_str(),r.link[0],r.link[1],r.link[2],o.route_weights().size());
        // WGoldenGate (0x4c6b10): doors swing by turns; a touched door opens.
        {
            wpc::CourseObjects g=track.objects();
            require(g.golden_gates().size()==1,"sh_t4: one WGoldenGate");
            const auto& w=g.golden_gates()[0];
            std::array<int,9> first{};
            g.update({},hits,none);
            for(size_t i=0;i<9;++i) first[i]=w.doors[i].state;
            int moved=0;
            for(int i=0;i<1200;++i) {g.update({},hits,none);for(size_t k=0;k<9;++k) if(w.doors[k].state!=first[k]) {++moved;first[k]=w.doors[k].state;}}
            require(moved>=5,"sh_t4: the doors change state");
            int solid=0;for(const auto& s:g.solid_spheres()) solid+=s.golden==0;
            require(solid>=9,"sh_t4: the doors are solid");
            size_t door=0;for(;door<9 && w.doors[door].state==4;++door) {}
            const wpc::Matrix4 f=wpc::multiply_matrix(w.parent,w.doors[door].frame);
            const float sign=w.doors[door].state==1?1.f:-1.f;
            g.door_touched(0,int(door),{{f[8]*sign,f[9]*sign,f[10]*sign}});
            require(w.doors[door].state==4 && w.doors[door].target==360,"sh_t4: a car on the right side opens a door");
        }
    }
    if(path.find("dr_t3")!=std::string::npos) {
        // WJunkyard (0x4ce430): the crane's arms move; WConstructionSite's belts
        // move a car whose last contact has their flag.
        {
            wpc::CourseObjects o=track.objects();
            require(o.cranes().size()==1 && o.conveyors().size()==1,"dr_t3: a WJunkyard, a WConstructionSite");
            std::vector<wpc::ObjectHit> hits;std::vector<int> none;
            const auto& c=o.cranes()[0];
            float low=1,high=-1;
            for(int i=0;i<600;++i) {o.update({},hits,none);low=std::min(low,c.frame[1][1]);high=std::max(high,c.frame[1][1]);}
            require(high-low>0.5f,"dr_t3: the crane's second arm swings");
            // 0x4ce65b: the third arm's PrimSphere (radius 215) squashes the
            // first car it touches, unless that car's WackyAutoBomba runs.
            require(c.prims.size()==1 && std::abs(c.prims[0].radius-215.089f)<0.01f,"dr_t3: the third arm's PrimSphere");
            const auto centre=[&]{const wpc::Matrix4 w=c.worlds()[2];const auto& q=c.prims[0].centre;
                return wpc::Vec3{{w[0]*q[0]+w[4]*q[1]+w[8]*q[2]+w[12],w[1]*q[0]+w[5]*q[1]+w[9]*q[2]+w[13],w[2]*q[0]+w[6]*q[1]+w[10]*q[2]+w[14]}};};
            wpc::CarProbe under;under.position=centre();
            wpc::CarProbe away=under;away.position[1]+=2000;
            wpc::CarProbe busy=under;busy.busy=true;
            std::vector<int> two{0,0};
            o.update({away,under},hits,two);
            require(hits.size()==1 && hits[0].car==1,"dr_t3: the arm squashes the car it touches, not one 2000 above");
            under.position=centre();busy.position=under.position;
            o.update({busy,under},hits,two);
            require(hits.empty(),"dr_t3: only the first car found is looked at; a busy one is not squashed");
            const float belt=std::sqrt(o.conveyors()[0].push[0][0]*o.conveyors()[0].push[0][0]+o.conveyors()[0].push[0][1]*o.conveyors()[0].push[0][1]+o.conveyors()[0].push[0][2]*o.conveyors()[0].push[0][2]);
            near(belt,40,0.01f,"dr_t3: a belt's vector is m_88 long");
            wpc::CarProbe car;car.position={{0,0,0}};car.surface=1u<<26;car.section=o.conveyors()[0].sections.empty()?-1:o.conveyors()[0].sections[0];
            std::vector<int> effect{0};
            o.update({car},hits,effect);
            require(!o.rolled().empty() && std::abs(o.rolled()[0].position[0]-o.conveyors()[0].push[0][0])<0.01f,"dr_t3: a car on a belt moves by it");
        }
        // WMagnet (0x4d09e0): on and off by turns; a car under it turns by m_b8.
        wpc::CourseObjects o=track.objects();
        require(o.magnets().size()==1 && o.magnets()[0].radius>0,"dr_t3: one WMagnet with a radius");
        wpc::CarProbe car;car.position=o.magnets()[0].centre;car.section=o.magnets()[0].sections[0];
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect{0};
        int pulled=0,free=0;
        for(int i=0;i<400;++i) {
            o.update({car},hits,effect);
            const bool p=!o.pushes().empty();
            if(p) {++pulled;near(o.pushes()[0].yaw,o.magnets()[0].yaw,1e-6f,"dr_t3: the magnet's turn");} else ++free;
            require(p==o.magnets()[0].on,"dr_t3: it pulls only while on");
        }
        require(pulled>50 && free>50,"dr_t3: it goes on and off");
    }
    if(path.find("sh_t2")!=std::string::npos) {
        // WSaw (0x4c0a90): it spins and slides; it follows a car, holds it for
        // m_64 updates and throws it away at m_4c.
        wpc::CourseObjects o=track.objects();
        require(o.saws().size()==1 && o.saws()[0].slide_axis==3 && o.saws()[0].amplitude==300 && o.saws()[0].hold==30,"sh_t2: one WSaw");
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        const auto& w=o.saws()[0];
        float low=1e9f,high=-1e9f;
        for(int i=0;i<200;++i) {o.update({},hits,effect);low=std::min(low,w.owner_frame[14]);high=std::max(high,w.owner_frame[14]);}
        near(high-low,600,40,"sh_t2: alone it slides over twice its amplitude");
        const wpc::Matrix4 own=w.owner_world();
        wpc::CarProbe car;car.position={{own[12]+100,own[13],own[14]+50}};car.section=w.sections.empty()?-1:w.sections[0];
        effect.assign(1,0);
        bool held=false;int updates=0;
        for(;updates<400 && !held;++updates) {
            o.update({car},hits,effect);
            held=w.state==1;
        }
        require(held && w.target==0,"sh_t2: a car in reach is caught");
        bool thrown=false;
        for(int i=0;i<40 && !thrown;++i) {
            o.update({car},hits,effect);
            thrown=!o.pushes().empty();
            if(i<25) require(!o.rolled().empty(),"sh_t2: the caught car is held in place");
        }
        require(thrown && w.state==2,"sh_t2: then thrown");
        const auto& f=o.pushes()[0].force;
        near(std::sqrt(f[0]*f[0]+f[2]*f[2]),1000,1,"sh_t2: at m_4c");
        for(int i=0;i<20;++i) o.update({},hits,effect);
        require(w.state==0,"sh_t2: 15 updates later it is free");
    }
    if(path.find("sh_t3")!=std::string::npos) {
        // WLogJam (0x4c4940): compared with Wine (2026-11-06): the rolling log's
        // speeds step by 0.642 and -0.144 an update on path 1's first segment, it
        // hands its speed to the log waiting at n - 3 and keeps a tenth (4.061 on
        // that path), then waits at n - 3; the struck log rolls off and restarts.
        wpc::CourseObjects o=track.objects();
        require(o.log_jams().size()==1 && o.log_jams()[0].logs.size()==6 && o.log_jams()[0].paths.size()==5,"sh_t3: a WLogJam of six logs and five paths");
        std::vector<wpc::ObjectHit> hits;std::vector<int> none;
        o.update({},hits,none);
        const auto& j=o.log_jams()[0];
        int rolling=0,waiting=0;
        for(const auto& l:j.logs) {rolling+=l.state==1;waiting+=l.state==0;}
        require(rolling==1 && waiting==5,"sh_t3: five logs wait, one rolls");
        for(size_t i=1;i<6;++i) require(j.logs[i].segment==int(j.paths[i-1].points.size())-3 && j.logs[i].path==int(i-1),"sh_t3: each path has a log waiting at n - 3");
        bool handed=false,struck=false,restarted=false;
        for(int i=0;i<3000;++i) {
            o.update({},hits,none);
            for(const auto& l:j.logs) {handed=handed || l.state==2;struck=struck || l.state==3;restarted=restarted || (struck && l.state==5);}
        }
        require(handed && struck && restarted,"sh_t3: a log hands on its speed, the struck one rolls off and restarts");
        rolling=waiting=0;
        for(const auto& l:j.logs) {rolling+=l.state==1 || l.state==2 || l.state==3;waiting+=l.state==0;}
        require(rolling>=1 && rolling+waiting==6,"sh_t3: the cycle keeps going");
        require(o.solid_spheres().size()==6 && o.solid_spheres()[0].cylinder,"sh_t3: each log is a solid cylinder");
        // A moving log squashes a car in its way.
        wpc::CourseObjects q=track.objects();
        std::vector<wpc::ObjectHit> h2;
        q.update({},h2,none);
        bool squashed=false;
        for(int i=0;i<3000 && !squashed;++i) {
            wpc::Vec3 at{};bool any=false;
            for(const auto& l:q.log_jams()[0].logs) if(l.state==1) {const wpc::Matrix4 w=wpc::multiply_matrix(q.log_jams()[0].parent,l.frame);at={{w[12]+w[4]*l.prim_centre[1],w[13]+w[5]*l.prim_centre[1]+100,w[14]+w[6]*l.prim_centre[1]}};any=true;}
            q.update(any?std::vector<wpc::CarProbe>{{at}}:std::vector<wpc::CarProbe>{},h2,none);
            squashed=!h2.empty();
        }
        require(squashed,"sh_t3: a moving log squashes a car");
    }
    if(path.find("cc_t4")!=std::string::npos) {
        // WChiselCrossover (0x4d0020): node 98's and 101's second link leads
        // to the node two on, so link 1 weighs 0x8000 whole and link 0 0xffff
        // once blown (0x4d01f0).
        wpc::CourseObjects o=track.objects();
        require(o.chisels().size()==2 && o.chisels()[0].route_node==98 && o.chisels()[1].route_node==101,"cc_t4: two WChiselCrossover on nodes 98 and 101");
        require(o.chisels()[0].section==1 && o.chisels()[0].bits==8 && o.chisels()[0].link_weight(1)==0x8000 && o.chisels()[0].link_weight(0)==0,"cc_t4: the whole wall's route weights");
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect{0,0};
        const wpc::Vec3 wall=o.chisels()[0].centre;
        o.update({{{{wall[0]+100000,wall[1],wall[2]}},false,false},{{{wall[0]-2500,wall[1],wall[2]}},true,false}},hits,effect,1,track.sections());
        require(effect[1]==3 && effect[0]==0 && o.chisels()[0].started,"cc_t4: the nearest car within 3000 is told to fire");
        require(o.solid_spheres().size()==3,"cc_t4: both walls and the rock are solid");
        const wpc::Vec3 solid=o.solid_spheres()[0].centre;
        require(!o.projectile_hit({{solid[0]+2000,solid[1],solid[2]}},100),"cc_t4: a projectile away from the wall");
        require(o.projectile_hit({{solid[0]+600,solid[1],solid[2]}},100),"cc_t4: a projectile blows the wall up");
        require(o.chisels()[0].blown && o.chisels()[0].link_weight(0)==0xffff && o.chisels()[0].link_weight(1)==0 && o.solid_spheres().size()==2,"cc_t4: the blown wall opens and frees its route");
        const wpc::Vec3 piece=o.chisels()[0].blow.pieces[0].home;
        for(int i=0;i<250;++i) o.update({},hits,effect,1,track.sections());
        require(o.chisels()[0].blow.left==0 && o.chisels()[0].blow.pieces[0].local[0]<0.01f && o.chisels()[0].blow.pieces[0].local[12]!=piece[0],"cc_t4: the pieces scale away over 200 updates");
        // WRolling (0x4c77b0): at rest until a car is within m_range of the
        // brick; its path is a valley, so it swings along it, as under Wine
        // (cc_t4, 2026-11-06: turning at d 2200 of segment 0 and 140 of 2,
        // speed about 60 at the bottom).
        require(o.rocks().size()==1 && !o.rocks()[0].loop && o.rocks()[0].flags==0x1b,"cc_t4: one WRolling without m_loop");
        o=track.objects();
        const wpc::Vec3 brick=o.rocks()[0].centre;
        const wpc::CarProbe away{{{brick[0],brick[1]+30000,brick[2]}}},close{{{brick[0]+1000,brick[1],brick[2]}}};
        std::vector<int> one{0};
        o.update({away},hits,one);
        require(o.rocks()[0].state==0 && o.rocks()[0].distance==0,"cc_t4: no car in range goes home");
        one[0]=0;o.update({close},hits,one);
        require(o.rocks()[0].state==1 && one[0]==3,"cc_t4: a car in range starts the rock");
        float low=0,high=0;bool reached=false;int n=0;
        for(;n<1000;++n) {
            o.update({close},hits,one);
            const float v=o.rocks()[0].velocity;low=std::min(low,v);high=std::max(high,v);
            reached=reached || o.rocks()[0].segment==2;
        }
        require(reached && low<-50 && high>50 && o.rocks()[0].state==1,"cc_t4: the rock swings along its valley");
        o=track.objects();one[0]=0;
        o.update({close},hits,one);
        for(n=0;n<1000 && o.rocks()[0].state==1 && hits.empty();++n) {
            const wpc::Matrix4 w=o.rocks()[0].world();
            o.update({{{{w[12],w[13],w[14]}}}},hits,one);
        }
        require(!hits.empty(),"cc_t4: the rock squashes a car in its way");
        o=track.objects();
        o.update({close},hits,one);
        {const wpc::Matrix4 w=o.rocks()[0].world();require(o.projectile_hit({{w[12],w[13],w[14]}},100),"cc_t4: a projectile blows the rock up");}
        require(o.rocks()[0].state==2 && o.rocks()[0].blow.on && o.rocks()[0].timer==60,"cc_t4: the blown rock's state");
        for(int i=0;i<300;++i) o.update({close},hits,one);
        require(o.rocks()[0].state==2,"cc_t4: its WBlowUp holds the blown rock, ended only by leaving the sections");
        o.update({away},hits,one,o.rocks()[0].sections[0]+4,8); // cc_t4 has only 3 sections, so it never happens there
        require(!o.rocks()[0].active && !o.rocks()[0].blow.on,"cc_t4: leaving its sections ends the WBlowUp (0x4c7710)");
    }
    if(path.find("ct_t2")!=std::string::npos) {
        // m_loop: it runs for ever, from any start.
        wpc::CourseObjects o=track.objects();
        require(o.rocks().size()==1 && o.rocks()[0].loop && o.rocks()[0].flags==0xb,"ct_t2: one WRolling with m_loop");
        std::vector<wpc::ObjectHit> hits;std::vector<int> one{0};
        const wpc::CarProbe far{{{0,1e6f,0}}};
        int restarts=0,was=1;
        for(int n=0;n<1500;++n) {o.update({far},hits,one);if(was==0 && o.rocks()[0].distance<100 && o.rocks()[0].segment==0) ++restarts;was=o.rocks()[0].state;}
        require(restarts>=2 && o.rocks()[0].state==1,"ct_t2: the rock loops without any car");
    }
    if(path.find("cc_t2")!=std::string::npos) {
        // WMisty (0x4c5b50): flat when shut, its deck over the gap in the
        // WGNR; a car at a sensor toggles it; the fork's first link follows.
        wpc::CourseObjects o=track.objects();
        require(o.bridges().size()==1 && o.bridges()[0].sensors.size()==2,"cc_t2: one WMisty with two sensors");
        const float x=11500,z=19940;
        const wpc::LogHit shut=o.log_ground(x,z,0,-3000);
        require(shut.hit && shut.bridge==0 && std::abs(shut.y+991)<60,"cc_t2: the shut bridge's deck carries cars");
        require(o.bridges()[0].fork_section==1 && o.bridges()[0].fork_weight()==0,"cc_t2: the fork takes the bridge while shut");
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        const wpc::Vec3 sensor=o.bridges()[0].sensors[0];
        o.update({{sensor,false}},hits,effect);
        require(o.bridges()[0].state==1 && o.bridges()[0].fork_weight()==0xffff,"cc_t2: a car at a sensor opens it");
        int n=0;for(;n<600 && o.bridges()[0].state!=2;++n) o.update({{sensor,false}},hits,effect);
        require(o.bridges()[0].state==2 && std::abs(o.bridges()[0].angle-o.bridges()[0].max_angle)<0.01f,"cc_t2: the bridge opens to m_MaxForgas");
        require(!o.log_ground(x,z,0,-3000).hit,"cc_t2: no deck over the gap when open");
        require(o.bridges()[0].frames[0][5]>0 && o.log_ground(10500,z,500,-3000).y>-900,"cc_t2: the halves rise");
        o.update({{{{0,0,0}},false}},hits,effect);o.update({{sensor,false}},hits,effect);
        require(o.bridges()[0].state==3,"cc_t2: a new car at a sensor shuts it");
        for(n=0;n<600 && o.bridges()[0].state!=0;++n) o.update({},hits,effect);
        require(o.bridges()[0].state==0 && o.bridges()[0].angle<0.01f,"cc_t2: the bridge shuts flat");
        std::printf("%s: WMisty shuts in %d updates\n",path.c_str(),n);
    }
    if(path.find("cc_t3")!=std::string::npos) {
        // WGeyser (0x4c8cc0): a car near aims the column before it; the ring
        // grows, the column rises and holds a car standing in it.
        wpc::CourseObjects o=track.objects();
        require(o.geysers().size()==1 && o.geysers()[0].width==200 && o.geysers()[0].frames==25,"cc_t3: one WGeyser");
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        const wpc::Vec3 home=o.geysers()[0].home;
        for(int i=0;i<4000 && o.geysers()[0].state!=2;++i) o.update({},hits,effect);
        require(o.geysers()[0].state==2 && o.geysers()[0].ring_scale>=1,"cc_t3: the geyser erupts with no car around");
        const wpc::Vec3 aim=o.geysers()[0].aim;
        require(std::abs(aim[0]-home[0])<=4000 && std::abs(aim[2]-home[2])<=4000,"cc_t3: its idle aim stays within m_Sugar");
        wpc::CarProbe probe;probe.position={{aim[0]+50,aim[1]+10,aim[2]}};
        size_t held=0;float top=0;
        for(int i=0;i<60;++i) {o.update({probe},hits,effect);if(!o.lifts().empty() && o.lifts()[0].airtime==0) {++held;top=o.lifts()[0].position[1];probe.position=o.lifts()[0].position;}}
        require(held>0 && top>aim[1] && probe.position[0]==aim[0],"cc_t3: the column centres and lifts a car");
        std::printf("%s: geyser lifts a car %.0f above its base\n",path.c_str(),top-aim[1]);
    }
    if(path.find("dr_t1")!=std::string::npos) {
        // WDynamiteHole (0x4cd800): a car in range fires it and is pushed up;
        // the lid rises; with every car beyond 10000 it re-arms.
        wpc::CourseObjects o=track.objects();
        require(o.dynamite_holes().size()==1 && o.dynamite_holes()[0].armed,"dr_t1: one armed WDynamiteHole");
        const wpc::Vec3 c=o.dynamite_holes()[0].centre;
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        wpc::CarProbe car;car.position={{c[0]+100,c[1],c[2]}};
        o.update({car},hits,effect);
        require(!o.dynamite_holes()[0].armed && o.pushes().size()==1 && o.pushes()[0].force[1]==100 && o.pushes()[0].force[0]==100,"dr_t1: a car in range fires it and is thrown");
        o.update({car},hits,effect);
        require(o.pushes().empty() && o.dynamite_holes()[0].offset==200,"dr_t1: one blast, the lid rising");
        car.position={{c[0]+20000,c[1],c[2]}};
        o.update({car},hits,effect);
        require(o.dynamite_holes()[0].armed && o.dynamite_holes()[0].offset==0,"dr_t1: it re-arms with no car within 10000");
    }
    if(path.find("sh_t2")!=std::string::npos) {
        // WLogRollingB (0x4c23a0): a car on the log turns about its axis.
        wpc::CourseObjects o=track.objects();
        require(o.rolling_logs().size()==1 && o.rolling_logs()[0].axis==3,"sh_t2: one WLogRollingB about z");
        const wpc::Vec3 c=o.rolling_logs()[0].centre;
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        wpc::CarProbe on;on.position={{c[0],c[1]+650,c[2]}};
        o.update({on},hits,effect);
        require(o.rolled().size()==1 && std::abs(o.rolled()[0].position[0]-(c[0]+650*std::sin(o.rolling_logs()[0].rate)))<0.1f && o.rolled()[0].position[2]==c[2],"sh_t2: a car on the log turns about its axis");
        // 0x4c23ef: flags 1, the player cars alone.
        wpc::CarProbe robot=on;robot.robot=true;
        o.update({robot},hits,effect);
        require(o.rolled().empty(),"sh_t2: robots do not turn on the log");
        wpc::CarProbe other=on;other.section=int(track.sections())+5;
        o.update({other},hits,effect);
        require(o.rolled().empty(),"sh_t2: only in the brick's section");
        wpc::CarProbe off;off.position={{c[0]+800,c[1],c[2]}};
        o.update({off},hits,effect);
        require(o.rolled().empty(),"sh_t2: not beyond its radius");
    }
    if(path.find("sh_t3")!=std::string::npos) {
        // WLogRide (0x4cc450): two idle updates, then m_vel (50) per update;
        // logs in groups of three, m_length + 50 apart, then m_dist.
        wpc::CourseObjects o=track.objects();
        require(o.log_rides().size()==1 && o.log_rides()[0].logs.size()==9,"sh_t3: one WLogRide with nine logs");
        std::vector<wpc::ObjectHit> hits;std::vector<int> effect;
        for(int i=0;i<3;++i) o.update({},hits,effect);
        const wpc::LogRide& r=o.log_rides()[0];
        require(std::abs(r.offset-50)<1e-3f,"sh_t3: the offset moves after two idle updates");
        const auto gap=[&](size_t a,size_t b){const auto& p=r.frames[a];const auto& q=r.frames[b];return std::sqrt((p[12]-q[12])*(p[12]-q[12])+(p[14]-q[14])*(p[14]-q[14]));};
        require(gap(0,1)>400 && gap(0,1)<600,"sh_t3: logs of a group m_length + 50 apart");
        o.update({},hits,effect);
        require(std::abs(r.moved[0][0])+std::abs(r.moved[0][2])>10,"sh_t3: a log's motion this update");
        std::printf("%s: log ride offset %.0f, first gap %.0f\n",path.c_str(),r.offset,gap(0,1));
        // A car set down on a log stands on it and rides with it (0x4cc69e).
        wpc::RaceSession ride(track,3);
        const auto& p0=ride.objects().log_rides()[0].points;
        ride.place(p0[5][0]+600,p0[5][2]-600,0);
        for(int i=0;i<300;++i) ride.update({});
        const auto& lr=ride.objects().log_rides()[0];
        size_t best=0;for(size_t i=0;i<lr.frames.size();++i) if(lr.frames[i][13]>lr.frames[best][13]) best=i;
        const auto f=lr.frames[best];
        ride.place(f[12],f[14],0);
        const auto start=ride.player().position;
        for(int i=0;i<10;++i) ride.update({});
        const auto end=ride.player().position;
        const float moved=std::sqrt((end[0]-start[0])*(end[0]-start[0])+(end[2]-start[2])*(end[2]-start[2]));
        std::printf("%s: a car on a log at y %.0f moves %.0f in 10 updates (log y %.0f)\n",path.c_str(),start[1],moved,f[13]);
        require(start[1]>f[13]-100 && moved>200,"sh_t3: a car on a log rides with it");
    }
    // 0x4a6d20: a dropped weapon is filed in the first enabled row (the
    // player's previous, current, next) whose box (row +0x1c..+0x30)
    // holds its x and z; outside every box, none (-1).
    {
        wpc::RaceSession drop(track,3,1);
        for(int i=0;i<120;++i) drop.update({});
        drop.drop_mine(-1);
        const auto& m=drop.mines().back();
        const int k=drop.slot_section(m.position),n=int(track.sections()),s=drop.player().section;
        std::printf("%s: drop at (%.0f %.0f) in section %d, slot row %d, mine section %d\n",path.c_str(),m.position[0],m.position[2],s,k,m.section);
        require(k>=0 && track.row_box_holds(k,m.position[0],m.position[2]),"a mine dropped on the course lies in a slot row's box");
        require(k==((s-1+n)%n) || k==s || k==(s+1)%n,"the slot row is the player's previous, current or next");
        require(m.section==k,"the mine takes its slot row's section");
        require(drop.slot_section({{1e7f,0,1e7f}})==-1,"no slot row holds a point off every box");
    }
    const bool cc=path.find("cc_t1")!=std::string::npos,sh=path.find("sh_t1")!=std::string::npos;
    if(path.find("dt_t1")!=std::string::npos) {
        // dt_t1's WTram (0x4caf40): six light phases (323 updates, as the pass end
        // zeroes the timer), the
        // tram's pass while lamp B is green at m_Vel 100 over its path, and
        // robots waiting (+0x1c7 = 5) at red; WElTram (0x4d1cd0, 0x4d2330)
        // runs its path both ways. The crossing (section 2) runs only with
        // the player in sections 1..3 (Wine), so the player is parked on
        // section 2's road before it, where it was under Wine.
        wpc::RaceSession race(track,3);
        {int e=0,c=0;wpc::course_indices(path,e,c);race.set_course(e,c);}
        // WPU_Tnt's end clones WExplosion (m_idExplosion 0x3ac).
        {
            wpc::RaceSession boom(track,3,1);
            boom.use_weapon_now(24);
            bool blasted=false;
            for(int i=0;i<120 && !blasted;++i) {boom.update({});blasted|=!boom.blasts().empty();}
            require(blasted,"the TNT's end shows a WExplosion");
        }
        // FlyPower (0x4d4d10): the car lifts, flies, and the effect ends once
        // it is down past 95 updates.
        {
            wpc::RaceSession fly(track,3,1,2);
            for(int i=0;i<120;++i) fly.update({});
            const float y0=fly.player().position[1];
            fly.use_gadget_now(3);
            float top=y0;
            wpc::DriveInput go;go.accelerate=true;
            for(int i=0;i<40;++i) {fly.update(go);top=std::max(top,fly.player().position[1]);}
            require(top>y0+200,"FlyPower lifts the car");
            for(int i=0;i<200 && fly.player_effect()==1;++i) fly.update(go);
            require(fly.player_effect()!=1 && std::abs(fly.tuning().gravity-wpc::vehicle_tuning(3).gravity)<1e-6f,"FlyPower ends and the gravity comes back");
        }
        // FlyPower's per-vehicle ends (0x4d5b8c): vehicle 0 stops once down
        // (0x4d59ed), vehicle 5 halves its gravity again at update 10 (0x4d56a2)
        // and stops past 17 once down, vehicle 2 flies with gravity x 0.3.
        for(int vehicle:{0,2,5}) {
            wpc::RaceSession fly(track,vehicle,1,2);
            for(int i=0;i<120;++i) fly.update({});
            const float g0=wpc::vehicle_tuning(vehicle).gravity;
            fly.use_gadget_now(vehicle);
            wpc::DriveInput go;go.accelerate=true;
            int updates=0;float at10=0;
            for(;updates<400 && fly.player_effect()==1;++updates) {fly.update(go);if(fly.flying(-1)==10) at10=fly.tuning().gravity;}
            std::printf("%s: FlyPower vehicle %d ended after %d updates, gravity at 10 %.4g (x %.4g)\n",path.c_str(),vehicle,updates,at10,at10/g0);
            require(fly.player_effect()!=1 && std::abs(fly.tuning().gravity-g0)<1e-6f,"FlyPower's per-vehicle end restores the gravity");
            if(vehicle==2) require(std::abs(at10-g0*0.17f*0.3f)<1e-4f*g0,"FlyPower vehicle 2's gravity x 0.3");
            if(vehicle==5) require(std::abs(at10-g0*0.17f*0.5f)<1e-4f*g0 && updates>16,"FlyPower vehicle 5's gravity halves at update 10");
            if(vehicle==0) require(updates<95,"FlyPower vehicle 0 ends once down");
        }
        // MonsterTires (0x4d6560): the tyres grow to 3 in 10 updates, settle to
        // 2.7, stay 270 updates (m_innormframenum - 30), shrink; 289 in all.
        {
            wpc::RaceSession mt(track,0,1,2);
            mt.use_gadget_now(7);
            int updates=0;float top=0;
            for(;updates<400 && mt.player_effect()==7;++updates) {mt.update({});top=std::max(top,mt.monster_scale(-1));}
            std::printf("%s: MonsterTires ran %d updates, tyre scale up to %.3g\n",path.c_str(),updates,top);
            require(updates==289 && std::abs(top-3.f)<1e-3f && mt.monster_scale(-1)==0,"MonsterTires' stages (289 updates, scale 3)");
        }
        // The tank's turret +0x3ab (0x4dbc76, 0x469a1c): a backward shot with no
        // target turns it to pi; it holds while the weapon runs, then comes back
        // by 3 degrees an update.
        {
            wpc::RaceSession tk(track,5,1,2);
            tk.use_weapon_now(16,-1,true);
            const float at0=tk.turret(-1);
            int back=0;
            for(;back<200 && tk.turret(-1)!=0;++back) tk.update({});
            std::printf("%s: tank turret %.4g after the backward shot, back to 0 after %d updates\n",path.c_str(),at0,back);
            require(std::abs(at0-3.14159265f)<1e-5f && back>60 && back<100,"the tank turret turns back after a shot (+0x3ab)");
        }
        // The wheel spin +0x2a4 (0x46b0d2): driving forward turns it down by
        // 0.05 x the step an update, wrapped at -2 pi.
        {
            wpc::RaceSession ws(track,0,1,2);
            wpc::DriveInput go;go.accelerate=true;
            float lowest=0,moved=0;int steps=0;
            for(int i=0;i<200;++i) {
                const float before=ws.wheel_spin(-1);ws.update(go);
                const float d=ws.wheel_spin(-1)-before;
                if(d<0) {moved-=d;++steps;}
                lowest=std::min(lowest,ws.wheel_spin(-1));
            }
            std::printf("%s: wheel spin turned %.3g rad down in %d updates, now %.3g\n",path.c_str(),moved,steps,ws.wheel_spin(-1));
            require(steps>50 && lowest>=-6.2832f && moved>1,"the wheels roll forward (+0x2a4)");
        }
        // WPU_Paintball: the paint shot hits a robot ahead with type 4.
        {
            wpc::RaceSession pb(track,3,1);
            pb.use_weapon_now(13);
            bool painted=false;
            for(int i=0;i<60 && !painted;++i) {pb.update({});for(size_t r=0;r<pb.opponents().size();++r) painted|=pb.bomb(int(r)).type==4;}
            require(painted,"the paint shot hits a car with type 4");
        }
        // WPU_BeeHive (0x4de9b0): sprung under its dropper, the bees hold
        // the car to 0.3 for 80 updates and kick its steering.
        {
            wpc::RaceSession bees(track,3,1,2);
            for(int i=0;i<120;++i) bees.update({});
            bees.use_weapon_now(18);
            bool sprung=false;
            for(int i=0;i<40 && !sprung;++i) {bees.update({});for(const auto& e:bees.take_item_events()) sprung|=e.find("bee hive on car")!=std::string::npos;}
            require(sprung,"the bee hive springs");
            wpc::DriveInput go;go.accelerate=true;float top=0;
            for(int i=0;i<60;++i) {bees.update(go);top=std::max(top,bees.player().speed.speed);}
            require(top<=0.31f,"the bees cap the speed at m_rTargetSpeed");
            for(int i=0;i<30;++i) bees.update(go);
            require(bees.mines().empty(),"the bees leave after m_nTrapTime");
        }
        // WPU_BrickWall (0x4e2650): with no target (practice, alone) it
        // starts 2000 ahead of the user, lands, and stops another car driving
        // into it; here the user, whose own bit is set, drives through it.
        {
            wpc::RaceSession wall(track,3,1,2);
            wall.use_weapon_now(22);
            for(int i=0;i<40;++i) wall.update({});
            require(wall.barriers().size()==1 && wall.barriers()[0].landed,"the brick wall lands ahead");
            const auto& w=wall.barriers()[0];
            const float d=std::hypot(w.position[0]-wall.player().position[0],w.position[2]-wall.player().position[2]);
            require(std::abs(d-2000)<50,"2000 ahead of the user");
            bool stopped=false;
            wpc::DriveInput go;go.accelerate=true;
            for(int i=0;i<200 && !stopped;++i) {wall.update(go);for(const auto& e:wall.take_item_events()) stopped|=e.find("brick wall stopped")!=std::string::npos;}
            require(!stopped,"the user is never stopped by its own wall (+0x70)");
        }
        // WPU_FireSiren (0x4e0130): every other car in the user's section and
        // within the action's +0x2c (icon 9's m_range, 5000) on each axis
        // (0x4a7e70) runs at x 0.9 an update for 150 updates.
        {
            // After 120 updates the robots are still within 5000 of the grid.
            wpc::RaceSession a(track,3,7),b(track,3,7);
            for(int i=0;i<120;++i) {a.update({});b.update({});}
            a.use_gadget_now(9);
            for(int i=0;i<10;++i) {a.update({});b.update({});}
            float sa=0,sb=0;int near=0;
            const auto within=[&](const wpc::Vec3& p){for(size_t k=0;k<3;++k) if(!(std::abs(p[k]-a.player().position[k])<5000)) return false;return true;};
            for(size_t i=0;i<a.opponents().size();++i) {
                const auto& c=a.opponents()[i].car;
                if(c.section!=a.player().section || !within(c.position)) continue;
                ++near;sa+=c.speed.speed;sb+=b.opponents()[i].car.speed.speed;
            }
            require(near>0 && sa<sb*0.8f,"the fire siren slows the cars of its section within 5000");
            for(int i=0;i<150;++i) a.update({});
            require(a.player_effect()!=9,"the siren ends after its life");
        }
        // WPU_Flipper (0x4d9a20): from the back of the grid, a robot ahead
        // within 3000 and 60 degrees takes the 0x200 squash.
        {
            wpc::RaceSession fl(track,3,1);
            fl.use_weapon_now(12);
            bool struck=false;
            for(int i=0;i<30 && !struck;++i) {fl.update({});for(const auto& e:fl.take_item_events()) struck|=e.find("flipper hit car")!=std::string::npos;}
            require(struck,"the flipper strikes a robot ahead");
        }
        // WPU_BootKicker (0x4d8aa0): the boot reaches out (x 1.1 an update),
        // kicks a robot ahead with 0x100, bounces 10 updates and comes home.
        {
            wpc::RaceSession kb(track,0,1);
            kb.use_weapon_now(11);
            int struck_at=-1,updates=0;float reach=0;
            for(;updates<200 && !kb.kicks().empty();++updates) {
                reach=std::max(reach,kb.kicks()[0].ext[2]);
                kb.update({});
                for(const auto& e:kb.take_item_events()) if(e.find("boot kicker hit car")!=std::string::npos && struck_at<0) struck_at=updates;
            }
            std::printf("%s: boot kicker struck at update %d, reach %.0f, home after %d updates\n",path.c_str(),struck_at,reach,updates);
            require(struck_at>=0 && kb.kicks().empty() && kb.running_weapon(-1)!=11,"the boot kicker strikes a robot ahead and comes home");
        }
        // WPU_OilSpill (0x4ddcf0): armed after 15 updates, it springs under
        // any car, its dropper too; the car spins (1.31 rad decaying by 0.897)
        // and its motion stops 59 updates; the spot fades 10 updates later.
        {
            wpc::RaceSession oil(track,3,1,2);
            for(int i=0;i<120;++i) oil.update({});
            oil.drop_mine(-1,false,true);
            const float yaw0=oil.player().yaw;
            bool slipped=false;
            for(int i=0;i<40 && !slipped;++i) {oil.update({});for(const auto& e:oil.take_item_events()) slipped|=e.find("oil spill")!=std::string::npos;}
            require(slipped,"the oil spill springs under its dropper once armed");
            for(int i=0;i<5;++i) oil.update({});
            require(std::abs(std::remainder(oil.player().yaw-yaw0,6.2831853f))>0.5f && oil.oil_sounds()==1,"the car spins and the +0x3cb sound plays");
            for(int i=0;i<12;++i) oil.update({});
            require(oil.mines().empty(),"the spot fades after m_nScaleNum");
        }
        // Practice (0x49d580 mode 2): the player alone at grid slot 2.
        {
            wpc::RaceSession practice(track,3,1,2);
            const wpc::Matrix4 slot=wpc::race_grid_frame(track.start_line(),2);
            require(practice.opponents().empty() && practice.mode()==2 && std::abs(practice.player().position[0]-slot[12])<1 && std::abs(practice.player().position[2]-slot[14])<1,"practice: alone at slot 2");
            require(practice.split_total()==-3,"practice splits start at -1 each");
        }
        // Two players (0x49d580, 0x49eb70; Wine cc_t1): no robots, player 1
        // at slot 0, player 2 at slot 3, each driven by its own input.
        {
            wpc::RaceSession two(track,3,1,0,1);
            const wpc::Matrix4 s0=wpc::race_grid_frame(track.start_line(),0),s3=wpc::race_grid_frame(track.start_line(),3);
            require(two.two_players() && two.opponents().size()==1 && two.opponents()[0].vehicle==1,"two players, no robots");
            require(std::abs(two.player().position[0]-s0[12])<1 && std::abs(two.opponents()[0].car.position[0]-s3[12])<1,"slots 0 and 3");
            const wpc::Vec3 p1=two.player().position,p2=two.opponents()[0].car.position;
            wpc::DriveInput go;go.accelerate=true;
            for(int i=0;i<200;++i) two.update({},go);
            const auto moved=[](const wpc::Vec3& a,const wpc::Vec3& b){return std::hypot(a[0]-b[0],a[2]-b[2]);};
            require(moved(two.opponents()[0].car.position,p2)>500 && moved(two.player().position,p1)<20,"player 2 drives on its own input");
            require(two.rank2()==1 && two.rank()==2 && two.lap2()==1,"player 2 leads");
        }
        // 0x432bdc: METEO's course choice (rain, snow, none).
        race.set_weather(1,"cc/cc_t2.WGM");require(race.raining() && !race.snowing(),"cc rains");
        race.set_weather(1,"ct/ct_t3.WGM");require(race.raining() && race.snowing(),"ct snows");
        race.set_weather(1,"dr/dr_t4.WGM");require(race.snowing(),"dr_t4 snows");
        race.set_weather(1,"dt/dt_t1.WGM");require(!race.raining(),"dt has no weather");
        race.set_weather(0,"cc/cc_t1.WGM");require(!race.raining(),"METEO off");
        const auto& objs=race.objects();
        require(objs.crossings().size()==1 && objs.movers().size()==1,"dt_t1 has one WTram and one WElTram");
        const wpc::TramCrossing& x=objs.crossings()[0];
        near(x.speed,100,1e-3f,"WTram m_Vel");require(x.radius==2000,"WTram m_Radius");near(x.length,7610,10,"WTram path length");
        const wpc::PathMover& shuttle=objs.movers()[0];
        require(x.sections==std::vector<int>{2},"section 2's WMOD row links the WTram brick (+0x104 under Wine)");
        require(shuttle.sections==std::vector<int>{1},"section 1's row links the WElTram's GDModel");
        for(int i=0;i<300;++i) race.update({});
        require(!x.active && x.phase==0xff && x.progress==0,"the crossing stays disabled with the player in section 0");
        require(shuttle.active && shuttle.t!=shuttle.start,"the shuttle runs with the player in section 0");
        race.place(12600,23790,1.57f);
        require(race.player().section==2,"player parked in section 2");
        int moving=0,waits=0,ends=0;std::vector<int> greens;float last_t=objs.movers()[0].t;
        for(int i=0;i<30*240;++i) {
            race.update({});
            if(x.progress!=0) ++moving;
            if(x.phase==0 && x.timer==120) greens.push_back(i);
            for(const auto& o:race.opponents()) waits+=o.effect==5;
            const float t=objs.movers()[0].t;ends+=(t-last_t)*(objs.movers()[0].step)<0;last_t=t;
        }
        require(greens.size()>=2 && greens[1]-greens[0]==323,"WTram light cycle: each phase lasts its count + 1, the tram's pass end cuts phase 0 (366 - 121 + 78)");
        require(moving>0 && moving%77<=3,"the tram crosses in 77 updates per green phase");
        require(waits>0,"robots wait at the red light");
        require(ends>=2,"WElTram runs its path both ways");
        std::printf("%s: tram moving %d updates, robots waiting %d updates, shuttle turns %d\n",path.c_str(),moving,waits,ends);
        // Section 3 disables the shuttle; it resumes where it stopped (Wine).
        race.place(-6212,23638,1.56f);
        require(race.player().section==3,"player parked in section 3");
        race.update({});
        const float frozen=shuttle.t,frozen_step=shuttle.step;
        for(int i=0;i<60;++i) race.update({});
        require(!shuttle.active && shuttle.t==frozen,"the shuttle stops with the player in section 3");
        race.place(12600,23790,1.57f);
        race.update({});
        require(shuttle.active && shuttle.step==frozen_step && std::fabs(std::remainder(shuttle.t-frozen,1.f))<=std::fabs(frozen_step)*1.01f,"it resumes without its init (t wraps by 1 at an end)");
    }
    require(track.sections()>=3 && track.laps()>=1,"WMOD routes and GDWorld lap count (ct_t2 has 9)");
    const wpc::Vec3 seam=transform_point(track.wrap(),track.exit(int(track.sections())-1));
    const wpc::Vec3& entry=track.entry(0);
    near(std::hypot(seam[0]-entry[0],seam[1]-entry[1],seam[2]-entry[2]),0,100,"WMOD loop transform joins the last exit to the first entry");
    for(size_t i=0;i<track.sections();++i) require(track.progress_field(int(i)),"progress field joins each section's portals");
    {
        // WCtrlRoboCar: the grid stands on the route layer; with the player
        // idle and every robot driving physically, a robot laps within two
        // minutes.
        wpc::RaceSession race(track,3);
        race.set_robots_always_near(true);
        int env=0,variant=0;require(wpc::course_indices(path,env,variant),"championship course indices");race.set_course(env,variant);
        // +0xb4 = 1 + boost + rand x boost / 4 (1.2..1.25 on the t1 courses).
        const float boost=wpc::rank_row(env,variant).boost;
        for(const auto& o:race.opponents()) require(o.rank_boost>=1+boost-1e-6f && o.rank_boost<=1+boost*1.25f+1e-6f,"+0xb4 from the course's rank row");
        if(variant==0 && env<4) for(const auto& o:race.opponents()) require(o.rank_boost>=1.2f && o.rank_boost<=1.25f+1e-6f,"+0xb4 = 1.2 + rand x 0.05 on t1 courses");
        for(const auto& o:race.opponents()) require(track.route(o.car.position).hit,"robot grid slot on the route layer");
        int best=0;
        // Robots run their own WCarFx: on cc_t1's sand, dust or tracks behind
        // some of them during the run (sh_t1's surfaces leave neither).
        size_t marked=cc?0:1;
        // 0x4e69bd..: robots aim beside the car ahead, inside the road
        // edges 0x468890 finds (+0x345/+0x351 offsets on the route layer).
        size_t aims=0,aims_on_route=0,edged=0,sane=0;
        for(int i=0;i<3600;++i) {
            race.update({});
            for(size_t k=0;k<race.opponents().size();++k) {
                const auto& o=race.opponents()[k];
                // Finished robots park at the line and aim beside each other (dt_t2).
                if(o.robot.avoiding() && race.opponent_laps(k)<track.laps()) {++aims;aims_on_route+=track.route(o.robot.avoid_point()).hit;}
                const wpc::RoadEdges e=wpc::road_edges(track,o.robot.module(),o.robot.next_module());
                // A finished robot parks beyond the line, off its module (dr_t3:
                // robot 1 stops in a narrow alley after its third lap).
                if(e.hit && race.opponent_laps(k)<track.laps()) {++edged;const int w=e.left_distance(o.car.position)+e.right_distance(o.car.position);sane+=w>200 && w<8000;}
                best=std::max(best,race.opponent_laps(k));
                if(!cc) continue;
                const auto& fx=race.opponent_effects(k);
                marked+=std::any_of(fx.puffs().begin(),fx.puffs().end(),[](const auto& p){return p.age>0;}) ||
                        std::any_of(fx.tracks().begin(),fx.tracks().end(),[](const auto& t){return t[0].active;});
            }
        }
        require(best>=1,"a robot laps the course");
        // The contact response (0x46bb..) keeps the robots going against
        // walls and banks (sh_t1's tunnel, cc_t1's banks). At most one is
        // trapped: on sh_t1's first hairpin (-6880, 734, -650) against the
        // cliff, or off cc_t1's bend near (-60600, 12400) on a wall-flagged
        // slope (0x1000001 cuts the robot's throttle, 0x4e82cd) after
        // leaving the road there in the first-lap pack, which only this
        // always-near mode produces (with the player parked at the bend the
        // line matches Wine's and nobody falls; see TODO).
        int worst=best,trapped=0;
        for(size_t k=0;k<race.opponents().size();++k) {worst=std::min(worst,race.opponent_laps(k));trapped+=race.opponent_laps(k)<std::min(2,best-1);}
        // Since every weapon has its effect (2026-11-01) the pack reshuffles;
        // on cc_t1 two robots can end at that bend (r0 and r5 at speed 0).
        // Long courses (ct_t3, dr_t2: over 60 s a lap) are measured against
        // the best robot rather than two laps.
        // dt_t3 (2026-11-08): a steep ramp in section 2 around (-40200, -2200,
        // 23800) (+250 over 500 toward -x) that a robot climbs at about 0.19
        // after a hit, then slides back, again and again (always-near pack only).
        // With the robots' item aims read from the WPowerUpSprites (10000 at
        // 20 degrees for the missile and breath), more of them are hit there:
        // 4 trapped, against 2 with TNT's 60 degrees and 5000. The original's
        // robots are trapped there too (Wine, player held above the ramp: one
        // robot 65 s in the hollow, another in and out 17 times). See TODO.
        const bool ramp=path.find("dt_t3")!=std::string::npos;
        require(trapped<=(ramp?4:2),"every robot but two does two laps in 120 s (four on dt_t3's ramp)");
        if(aims_on_route*100<aims*95) std::printf("%s: avoidance aims on the route layer %zu of %zu\n",path.c_str(),aims_on_route,aims);
        // dr_t2: WBumSteer's runners and gates knock robots off the line (86.9%).
        // ct_t1 (2026-11-08): with the race distance in the rows' +0x34 (1.38x
        // the native field a lap there), the early-race avoidance window
        // (progress under 20000, +-120 degrees) ends sooner on the road, as in
        // the original's units: 88.9%.
        const bool loose=path.find("dr_t2")!=std::string::npos || path.find("ct_t1")!=std::string::npos;
        require(aims>100 && aims_on_route*100>=aims*(loose?85:90),"avoidance aims lie on the route layer (90%; dr_t2 and ct_t1 85%)");
        require(edged>1000 && sane*100>=edged*95,"road edges span the car's module");
        size_t moved=0;
        for(const auto& o:race.opponents()) moved+=o.robot.has_goal();
        require(moved==race.opponents().size(),"every robot has a route goal");
        int boosted=0;for(const auto& o:race.opponents()) boosted+=o.rank_boosted;
        std::printf("%s: robots best %d lap%s in 120 s, worst %d (%d catching up at the end, %zu avoiding updates)\n",path.c_str(),best,best==1?"":"s",worst,boosted,aims);
        require(marked>0,"robots leave dust or tracks");
    }
    // 0x4e53df: a far robot hit keeps its last control under the car update:
    // held still for the hit's 25 updates, then driving; it glides again once
    // the hit ends (Wine, cc_t1: robot 0 about 40000 away, hit with 0x80).
    {
        wpc::RaceSession race(track,3);
        int k=-1;
        for(int i=0;i<900 && k<0;++i) {race.update({});for(size_t r=0;r<race.opponents().size();++r) if(race.opponents()[r].far && !race.opponent_hidden(r) && race.opponent_laps(r)<track.laps()) {k=int(r);break;}}
        if(k>=0) {
            require(race.hit(k,0x80),"a far robot takes a hit");
            race.update({});
            const wpc::Vec3 at=race.opponents()[size_t(k)].car.position;
            float moved=0;
            for(int i=0;i<20;++i) {race.update({});const auto& p=race.opponents()[size_t(k)].car.position;moved=std::max(moved,std::hypot(p[0]-at[0],p[2]-at[2]));}
            // The far step would cover about 80 an update (1600 here); held, a
            // car only creeps on a slope after the 0x80 hop (cc_t2: 115).
            require(moved<400,"held: the far robot does not glide during the hold");
            int n=0;while(race.bomb(k).active() && n<200) {race.update({});++n;}
            const wpc::Vec3 after=race.opponents()[size_t(k)].car.position;
            std::printf("%s: far robot %d hit: %.0f during the hold, %.0f by the hit's end\n",path.c_str(),k,moved,std::hypot(after[0]-at[0],after[2]-at[2]));
            // dt_t2: 145 when the robot drives into a wall after the hold.
            require(std::hypot(after[0]-at[0],after[2]-at[2])>100,"then it drives off before the hit ends");
        }
    }
    if(cc) {
        // Wine (2026-10-30): with the player parked at (-57000, 14172) the
        // original's robots cross z 13000 at x -60849 on average
        // (-60636..-61036) and never drop onto the bank below the bend.
        wpc::RaceSession race(track,3);
        {int e=0,c=0;wpc::course_indices(path,e,c);race.set_course(e,c);}
        for(int i=0;i<200;++i) race.update({});
        race.place(-57000,14172,1.5f);
        std::vector<wpc::Vec3> last;for(const auto& o:race.opponents()) last.push_back(o.car.position);
        double sum=0;int n=0,falls=0;
        for(int i=0;i<30*200;++i) {
            race.update({});
            for(size_t k=0;k<last.size();++k) {
                const wpc::Vec3 p=race.opponents()[k].car.position,q=last[k];last[k]=p;
                if(p[0]< -62500 || p[0]> -57500) continue;
                if(q[2]>13000 && p[2]<=13000) {sum+=p[0];++n;}
                if(p[2]>11500 && p[2]<13050 && p[1]<1300 && q[1]>=1300) ++falls;
            }
        }
        std::printf("%s: bend line x %.0f at z 13000 over %d passes (Wine -60849), %d falls\n",path.c_str(),n?sum/n:0.0,n,falls);
        require(n>=10 && std::abs(sum/n+60849)<300 && falls==0,"robots follow the original's line through cc_t1's bend");
    }
    {
        // 0x4e4f10 far mode (0x1000): with the player idle on the grid the
        // robots slide along the route and cross the loop seam where the
        // last section's ground outruns its route; all finish 3 laps (the
        // original's cc_t1 laps take 62..81 s under Wine).
        wpc::RaceSession race(track,3);
        {int e=0,c=0;wpc::course_indices(path,e,c);race.set_course(e,c);}
        // 0x4a1c8b: with more than three sections, a robot leaving the
        // player's rows (here the last, 0 and 1) is hidden and runs its rows
        // at 204 a update (+0x2b5), then shows at the next row's entry
        // portal (Wine, dt_t1: ~3.2 s at section 2's entry, then at
        // (1048, 23084) near section 3's portal).
        const size_t n=race.opponents().size();
        std::vector<int> since(n,-1);int hides=0,shows=0,longest=0;float worst_gap=0;
        for(int i=0;i<30*300;++i) {
            race.update({});
            for(size_t k=0;k<n;++k) {
                const bool h=race.opponent_hidden(k);
                if(h && since[k]<0) {since[k]=i;++hides;require(track.sections()>3,"robots hide only outside the player's rows");}
                if(!h && since[k]>=0) {
                    ++shows;longest=std::max(longest,i-since[k]);since[k]=-1;
                    const auto& c=race.opponents()[k].car;const wpc::Vec3& e=track.entry(c.section);
                    worst_gap=std::max(worst_gap,std::hypot(c.position[0]-e[0],c.position[2]-e[2]));
                }
            }
        }
        if(track.sections()>3) {
            std::printf("%s: robots hidden %d times, shown %d, longest %d updates, at most %.0f from the entry portal\n",path.c_str(),hides,shows,longest,worst_gap);
            require(hides>=int(n) && shows>=int(n),"every robot hides in a far section and shows again");
            require(longest<=track.row_length(2)/204+2,"a hidden robot runs one row at 204 a update");
            require(worst_gap<1200,"it shows at the row's entry portal, spread 200 apart");
        }
        int worst=3;
        for(size_t k=0;k<race.opponents().size();++k) worst=std::min(worst,race.opponent_laps(k));
        std::printf("%s: far robots finish %d of 3 laps in 300 s\n",path.c_str(),worst);
        require(worst>=3,"far robots finish the race");
    }
    if(cc) {
        // A hit (0x80) holds a robot 25 updates at speed 0; its motion
        // counter keeps it still 5 more updates, then +0x8db helps it off.
        wpc::RaceSession race(track,3);
        race.set_robots_always_near(true);
        for(int i=0;i<400;++i) race.update({});
        require(race.hit(0,0x80) && !race.hit(0,0x40),"a running hit refuses another");
        // The hit's own update still moves the car: WackyAutoBomba runs
        // after WackyAutoNormal.
        race.update({});
        const wpc::Vec3 at=race.opponents()[0].car.position;
        for(int i=0;i<29;++i) {
            race.update({});
            if(i<24) require(race.opponents()[0].car.speed.speed==0,"held at speed 0");
        }
        const wpc::Vec3 still=race.opponents()[0].car.position;
        // The hop (0x80) flies and lands along the ground normal (0x46d0f4);
        // on a slope the fall then creeps the car down (about 2 units per
        // update on cc_t1's steepest bank). Driving would cover thousands.
        require(std::abs(still[0]-at[0])<100 && std::abs(still[2]-at[2])<100,"no motion during the hold and the 5 updates after");
        for(int i=0;i<30;++i) race.update({});
        const wpc::Vec3 off=race.opponents()[0].car.position;
        require(std::abs(off[0]-at[0])+std::abs(off[2]-at[2])>50,"the robot drives off");
        // White model colours (no view), renderer 1's byte fades: +0x36 at 79,
        // the hit ends on the 81st update.
        int more=0;while(race.bomb(0).active() && more<100) {race.update({});++more;}
        require(60+more==81,"the hit has ended");
        // TurboSpeed: the limit x 2.2 and the convergence x 10 for 60 updates.
        {
            // Straight from the grid; by 400 updates the car would be in the
            // ravine past the first jump, drowning (which ends the effect).
            wpc::RaceSession tr(track,3);
            for(int i=0;i<100;++i) tr.update({true,false,false,false});
            const float cruise=tr.player().speed.speed;
            tr.use_gadget_now(8);
            require(tr.turbo() && tr.player_effect()==8,"turbo on, +0x385 = 8");
            float peak=0;
            for(int i=0;i<60;++i) {tr.update({true,false,false,false});peak=std::max(peak,tr.player().speed.speed);}
            require(tr.turbo(),"still on after 60 updates");
            tr.update({true,false,false,false});
            require(peak>cruise*1.5f && peak<=wpc::vehicle_tuning(3).top_speed*2.2f+1e-4f,"the speed climbs past the normal limit");
            require(!tr.turbo() && tr.player_effect()==-1,"the rows come back on the 61st update");
            // The shield absorbs a mine.
            tr.use_gadget_now(10);
            require(tr.shielded() && !tr.hit(-1,0x80) && !tr.bomb(-1).active(),"the shield absorbs the hit");
            for(int i=0;i<8;++i) tr.update({});
            require(!tr.shielded() && tr.player_effect()==-1,"the shield ends after the hit");
        }
        // WPU_Missile, WPU_TankGun and WPU_Tnt from behind robot 0.
        for(const int icon:{25,16,24}) {
            wpc::RaceSession fr(track,3);
            fr.set_robots_always_near(true);
            const wpc::RaceCar& r=fr.opponents()[0].car;
            const float back=1500;
            fr.place(r.position[0]+std::sin(r.yaw)*back,r.position[2]-std::cos(r.yaw)*back,r.yaw);
            fr.use_weapon_now(icon);
            if(icon==24) require(fr.tnts().size()==1,"one TNT");
            else require(fr.projectiles().size()==1 && fr.weapon_effect()==(icon==25?0x19:0x10),"one projectile, +0x389 set");
            bool struck=false;
            for(int i=0;i<60 && !struck;++i) {
                fr.update({});
                for(size_t k=0;k<fr.opponents().size();++k) struck=struck || fr.bomb(int(k)).active();
            }
            require(struck,"the projectile hits a robot");
            require(fr.projectiles().empty() && fr.tnts().empty(),"and is gone");
            for(int i=0;i<45;++i) fr.update({});
            require(fr.weapon_effect()==-1,"+0x389 ends");
        }
        // 0x4a71e0 skips a racer whose hit is running.
        {
            wpc::RaceSession ft(track,3);
            const wpc::RaceCar& r0=ft.opponents()[0].car;
            const wpc::Vec3 f{{-std::sin(r0.yaw),0,std::cos(r0.yaw)}};
            const wpc::Vec3 from{{r0.position[0]-f[0]*500,r0.position[1],r0.position[2]-f[2]*500}};
            const int first=ft.find_target(from,r0.yaw,r0.section,0.2f,6000,-1);
            require(first==0,"robot 0 straight ahead is the target");
            ft.hit(0,0x80);
            require(ft.find_target(from,r0.yaw,r0.section,0.2f,6000,-1)!=0,"not while it is hit");
            require(ft.find_target(from,r0.yaw,r0.section,0.2f,6000,0)!=0,"nor as the owner");
        }
        // WPU_Storm from the grid: it strikes robots along the route, each
        // once, and is gone after its life and fade.
        {
            wpc::RaceSession sr(track,3);
            sr.use_weapon_now(21);
            require(sr.storms().size()==1 && sr.storms()[0].position[1]>sr.player().position[1]+399,"the storm starts 400 above");
            size_t struck=0;std::vector<int> hits(sr.opponents().size(),0);
            // The life (+0x38) counts only while roaming: each strike adds
            // up to 30 updates.
            for(int i=0;i<150+40+30*7+2;++i) {
                sr.update({});
                for(size_t k=0;k<sr.opponents().size();++k) if(sr.bomb(int(k)).type==0x400 && sr.bomb(int(k)).counter==1) ++hits[k];
            }
            for(int h:hits) {struck+=h>0;require(h<=1,"each car once");}
            require(struck>0,"the storm strikes a robot");
            require(!sr.bomb(-1).active(),"the thrower is spared");
            require(sr.storms().empty(),"gone after its roaming life, the strikes and the fade");
        }
        // WPU_Detour: dropped by robot 0, the player on it turns round.
        {
            wpc::RaceSession dr(track,3);
            const wpc::RaceCar& r0=dr.opponents()[0].car;
            dr.drop_mine(0,true);
            const wpc::Vec3 sign=dr.mines()[0].position;
            dr.place(sign[0],sign[2],0.3f);
            const float before=dr.player().yaw;
            int n=0;
            while(!dr.bomb(-1).active() && n<40) {dr.update({});++n;}
            require(n>15 && n<25 && (dr.bomb(-1).type==0x800 || dr.bomb(-1).type==0x1000),"the armed sign hits with 0x800 or 0x1000");
            for(int i=0;i<80;++i) dr.update({});
            const float turn=std::abs(std::remainder(dr.player().yaw-before,6.2831853f));
            require(turn>3.0f,"the car has turned round");
            (void)r0;
        }
        // WPU_Landmine (0x4df909): the 0x4a71e0 query has no owner, so the
        // mine waits while its dropper is the nearest car. Robot 0's mine
        // with the player 100 away stays armed.
        {
            wpc::RaceSession wait(track,3);
            const wpc::Vec3 r0=wait.opponents()[0].car.position;
            wait.drop_mine(0);
            wait.place(r0[0]+100,r0[2],0);
            for(int i=0;i<20;++i) wait.update({});
            require(wait.mines().size()==1 && wait.mines()[0].state==2 && !wait.bomb(-1).active() && !wait.bomb(0).active(),"the dropper nearest holds the mine");
        }
        // The player's mine left on robot 0: armed after 15 updates, it hits
        // robot 0 and goes.
        wpc::RaceSession mine(track,3);
        const wpc::Vec3 r0=mine.opponents()[0].car.position;
        mine.place(r0[0],r0[2],0);
        mine.drop_mine(-1);
        mine.place(r0[0]+2000,r0[2],0);
        require(mine.mines().size()==1,"one mine");
        for(int i=0;i<14;++i) mine.update({});
        require(mine.mines().size()==1 && mine.mines()[0].state!=2 && !mine.bomb(0).active(),"locked for 15 updates");
        require(std::abs(mine.mines()[0].position[1]-r0[1])<60,"the mine lies on the ground");
        int armed_updates=14;
        while(!mine.bomb(0).active() && armed_updates<20) {mine.update({});++armed_updates;}
        require(mine.bomb(0).type==0x80 && armed_updates==17 && !mine.bomb(-1).active(),"the mine arms (life < 285) and hits robot 0 next update");
        require(mine.mines().size()==1 && mine.mines()[0].state==0xffff,"then explodes");
        mine.update({});
        require(mine.mines().empty(),"and is removed (0x4df9b1)");
    }
    {
        wpc::RaceSession race(track,3);
        near(race.player().position[1],track.start_line()[13],60,"grid on the ground near the start line");
        // sh_t2's grid stands diagonally across its road, so the slot's progress
        // can put the player sixth.
        require(race.rank()>=6 && race.player().section==0,"player starts last or next to last in route zero");
        for(int i=0;i<61;++i) race.update({true,false,false,false});
        require(race.phase()==wpc::RaceSession::Phase::Ready && race.player().speed.speed==0,"PRET! blocks the throttle");
        if(cc) {
            near(track.wrap()[12],73386.5f,1,"cc_t1 WMOD loop translation");
            // Straight ahead runs into the river at the first bend, as in Wine.
            bool drowned=false;
            for(int i=0;i<300 && !drowned;++i) {race.update({true,false,false,false});drowned=race.drowning();}
            require(drowned,"driving straight drowns in the deep river");
            for(int i=0;i<60;++i) race.update({});
            require(!race.drowning() && race.player().grounded && race.respawns()==1,"drowned car returns to dry ground");
        }
    }
    {
        // 0x432c7f: METEO 1 rains only on the listed courses (cc_t*, ct_*).
        wpc::RaceSession rain(track,3);
        rain.set_weather(1,path);
        bool listed=false;
        for(const char* name:{"cc_t","dr_t1","ct_","dr_t4","dr_b"}) listed=listed || path.find(name)!=std::string::npos;
        require(rain.raining()==listed,"METEO 1 rains on the listed courses (cc_t*, dr_t1, ct_*, dr_t4, dr_b)");
        rain.set_weather(0,path);
        require(!rain.raining(),"METEO 0 is dry");
    }
    if(sh) near(track.wrap()[0],-1,1e-3f,"sh_t1 loop transform turns half about +Y");
    {
        wpc::RaceSession race(track,3);
        const auto strip=race.strip();
        require(strip.size()==7 && std::count_if(strip.begin(),strip.end(),[](const auto& r){return r.player;})==1,
                "progress strip lists the grid with one player");
        for(const auto& r:strip) require(r.lap_fraction>=0 && r.lap_fraction<=1 && !r.finished,"strip lap fractions start in range");
        if(cc) {
            // 0x4a6890: WBendSign 54 (type 1, radius 1000) seen 600 units ahead
            // along its heading gives arrow 1; facing away gives none.
            require(track.bend_signs(0).size()==1 && track.bend_signs(2).size()==7,"cc_t1 WBendSign links per route");
            for(int i=0;i<80;++i) race.update({});
            race.place(-5015,7879,0);race.update({});
            require(race.bend_arrow()==1,"bend arrow within the sign's radius and heading");
            race.place(-5015,7879,3.14159f);race.update({});
            require(race.bend_arrow()==-1,"no bend arrow when facing away");
            // Braking hard on cc_t1's road (surface 2) leaves 0x5c skid marks.
            {
                wpc::RaceSession skid(track,3);
                for(int i=0;i<110;++i) skid.update({true,false,false,false});
                skid.update({false,true,false,false});
                const auto& seg=skid.effects().tracks()[0][0];
                require(skid.skidding() && skid.wheels()[0].surface==2 && seg.active && seg.shade==0x5c,"skid marks on the road");
            }
            // Rock 82 (0x10000, r 63 at (-3213, 14755)): driving into it
            // stops the car, pushes it away along the normal and plays +0x3d3.
            {
                wpc::RaceSession rock=race;
                rock.place(-3213,14355,0);
                bool hit=false;
                for(int i=0;i<90 && !hit;++i) {rock.update({true,false,false,false});hit=rock.rock_sounds()>0;}
                require(hit && rock.player().speed.speed==0,"a rock hit stops the car");
                const float z0=rock.player().position[2];
                // The force applies in the hit's own update, then decays by 0.4.
                for(int i=0;i<4;++i) rock.update({});
                require(rock.player().position[2]<z0-20 && rock.player().position[2]>z0-200,"the rock's push decays");
                require(14755-rock.player().position[2]>163,"the car ends outside the rock");
            }
            // WPowerUp 56 (gadget) 400 units ahead: taken on the way, it adds
            // a charge and runs the gadget roulette (icons 7..10, 10 steps of 5).
            // Every pickup's PRIM is a GDPrimSphere of radius 100 at its origin
            // with sensor group 0x8000 (the object query's power-up bit).
            for(const auto& p:track.power_ups())
                require(p.radius==100 && p.centre==p.position,"WPowerUp PRIM sphere radius 100 at the node origin");
            require(track.power_ups().size()==25 && race.gadget_icon()==3 && race.weapon_icon()==14,
                    "cc_t1 pickups and the Dingo's own gadget and weapon (0x4679d0 state 0)");
            race.place(-5562,5400,0);
            int taken=-1;
            for(int i=0;i<30 && taken<0;++i) {
                race.update({true,false,false,false});
                for(const auto& e:race.take_item_events()) if(e.find("picked up gadget power-up 56")!=std::string::npos) taken=i;
            }
            require(taken>=0 && race.charge_level()==1,"pickup adds a charge");
            for(int i=0;i<60;++i) race.update({});
            require(race.gadget_icon()>=7 && race.gadget_icon()<=10,"gadget roulette lands on icons 7..10");
            wpc::DriveInput use;use.gadget=true;race.update(use);
            require(race.charge_level()==0,"using the gadget spends the charge");
            // State 4 waits for the gadget's effect (+0x385 = -1), then 0 and
            // the default icon.
            for(int i=0;i<1200 && race.gadget_icon()!=3;++i) race.update({});
            require(race.gadget_icon()==3,"the slot returns to the vehicle's own gadget");
        }
    }
    // One lap on sh_t1 covers the tunnel, the pyramid jumps and the seam
    // (the earlier planner took about 300 s a lap there).
    const int laps=sh?1:3;
    wpc::RaceSession race(track,3);
    HybridPilot autopilot;
    int robot_pickups=0,robot_uses=0;
    for(int i=0;i<18000 && race.laps_completed()<laps && race.phase()!=wpc::RaceSession::Phase::Finished;++i) {
        race.update(autopilot(race));
        for(const auto& e:race.take_item_events()) {
            if(e.rfind("robot ",0)==0 && e.find("picked up")!=std::string::npos) ++robot_pickups;
            if(e.find("(robot ")!=std::string::npos) ++robot_uses;
        }
    }
    // WCtrlRoboCar's items: robots take power-ups (0x46aad5) and use them.
    require(robot_pickups>0 && robot_uses>0,"robots take and use power-ups");
    std::printf("%s: robots took %d power-ups and used %d items\n",path.c_str(),robot_pickups,robot_uses);
    std::printf("%s: the test driver: %d laps, %d loops, at (%.0f,%.0f,%.0f) section %d after %.0f s\n",path.c_str(),race.laps_completed(),race.player().loops,race.player().position[0],race.player().position[1],race.player().position[2],race.player().section,race.race_seconds());
    // The native seam can come just after the line (cc_t3), so the last lap
    // may count one loop before its seam.
    require(race.laps_completed()>=laps && race.player().loops>=laps-1,"laps through the loop transform");
    // ct_t2 runs 9 laps (m_nLapNum); its finish is not driven here.
    if(laps==3 && track.laps()==3) {
        // The robots beat the test driver on some courses, so only the
        // finish and the player's place in the standings are checked.
        const int place=race.rank();
        require(race.phase()==wpc::RaceSession::Phase::Finished && place>=1 && place<=7,"race finished");
        std::printf("%s: the test driver finishes %d%s\n",path.c_str(),place,place==1?"st":place==2?"nd":place==3?"rd":"th");
        // WModManager states 5..7: single-race standings, Enter only once the
        // 120-update counter has run out.
        require(race.finish_state()==5 && race.standings().size()==7 && race.standings()[size_t(place-1)]==3,"single-race standings, the player at its place");
        require(!race.accept(),"Enter refused while the standings arrive");
        for(int i=0;i<120;++i) race.update({});
        require(race.finish_counter()==0 && race.accept(),"Enter closes the single race");
        if(cc) {
            // Championship: points 10/6/4/3/2/1/0 by place, then the standings
            // by total (state 7) before the race closes.
            wpc::RaceSession champ(track,3);
            std::array<int,7> totals{};totals[0]=20; // vehicle 0 leads the championship
            champ.set_championship(totals);
            HybridPilot pilot;
            for(int i=0;i<12000 && champ.phase()!=wpc::RaceSession::Phase::Finished;++i) champ.update(pilot(champ));
            static const int by_place[7]={10,6,4,3,2,1,0};
            int awarded=0;for(int v:champ.race_points()) awarded+=v;
            require(champ.finish_state()==6 && champ.race_points()[3]==by_place[champ.rank()-1] && awarded==26,"championship race points by place (0x52cef0)");
            for(int i=0;i<120;++i) champ.update({});
            require(!champ.accept() && champ.finish_state()==7 && champ.finish_counter()==120,"state 6 -> 7 on Enter");
            require(champ.standings().front()==0,"state 7 orders by total points");
            for(int i=0;i<120;++i) champ.update({});
            require(champ.accept(),"Enter closes the championship race");
            // Practice finish (0x4a3421): state 0xf, counter 60, no standings;
            // a faster time replaces the record with the vehicle in bits 28+.
            wpc::RaceSession practice(track,3,1,2);
            practice.set_record(0xfffffffu);
            HybridPilot solo;
            for(int i=0;i<12000 && practice.phase()!=wpc::RaceSession::Phase::Finished;++i) practice.update(solo(practice));
            require(practice.finish_state()==15 && practice.finish_counter()==60 && practice.standings().empty(),"practice ends in state 0xf");
            require(practice.new_record() && practice.record()>>28==3 && (practice.record()&0xfffffffu)<0xfffffffu,"practice record replaced");
            for(int i=0;i<60;++i) practice.update({});
            require(practice.accept(),"Enter closes the practice");
        }
    }
    // The action's +0x5c (look-back held at the use, 0x4d24f0): the missile
    // leaves backward (0x4da605), the TNT is thrown backward (0x4dcaff), and
    // the storm takes the backward links (0x4e14e0).
    {
        const auto along=[](const wpc::Vec3& d,float yaw){return -std::sin(yaw)*d[0]+std::cos(yaw)*d[2];};
        wpc::RaceSession fw(track,3),bk(track,3);
        fw.set_robots_enabled(false);bk.set_robots_enabled(false);
        for(int i=0;i<120;++i) {fw.update({});bk.update({});}
        const wpc::RaceCar& c=bk.player();
        bk.use_weapon_now(25,-1,true);
        require(bk.projectiles().size()==1 && bk.weapon_backward(-1),"a backward missile");
        const wpc::Projectile& m=bk.projectiles()[0];
        const wpc::Vec3 dm{{m.position[0]-c.position[0],0,m.position[2]-c.position[2]}};
        require(along(dm,c.yaw)<-99 && along(m.velocity,c.yaw)<0,"the backward missile starts 100 behind and leaves backward");
        bk.update({});bk.update({});
        for(int i=0;i<45;++i) bk.update({});   // the missile's +0x389 runs out (40)
        bk.use_weapon_now(24,-1,true);
        require(!bk.tnts().empty() && along(bk.tnts().back().velocity,c.yaw)<0,"the backward TNT is thrown backward");
        fw.use_weapon_now(21);bk.use_weapon_now(21,-1,true);
        require(fw.storms().size()==1 && bk.storms().size()==1 && bk.storms()[0].backward && !fw.storms()[0].backward,"one storm each way");
        // Each change of route polygon counted as a step along one of the
        // last polygon's forward or backward links (the start line restarts
        // the remaining fraction inside section 0, so no progress measure).
        struct Steps {bool has=false;int section=-1;uint32_t record=0,polygon=0;int forward=0,backward=0;};
        const auto step=[&](Steps& k,const wpc::Vec3& p) {
            const auto r=track.route(p);if(!r.hit) return;
            if(k.has && r.section==k.section && r.polygon!=k.polygon) {
                const wpc::RouteNode node=track.route_node(k.section,k.record,k.polygon);
                for(const auto& l:node.forward) if(l.target==r.polygon) ++k.forward;
                for(const auto& l:node.backward) if(l.target==r.polygon) ++k.backward;
            }
            k.has=true;k.section=r.section;k.record=r.record;k.polygon=r.polygon;
        };
        Steps sf,sb;
        for(int i=0;i<60 && !fw.storms().empty() && !bk.storms().empty();++i) {
            step(sf,fw.storms()[0].position);step(sb,bk.storms()[0].position);
            fw.update({});bk.update({});
        }
        std::printf("%s: storms' route steps in 60 updates, forward storm %d/%d, backward storm %d/%d (along forward/backward links)\n",path.c_str(),sf.forward,sf.backward,sb.forward,sb.backward);
        require(sf.forward>sf.backward && sb.backward>sb.forward,"the forward storm follows the forward links, the backward one the backward links");
    }
    // A missile fired 1500 before the last exit flies on into section 0
    // through the loop transform (0x4a23c2), instead of off the last section.
    if(track.sections()>1) {
        wpc::RaceSession ms(track,3);
        ms.set_robots_enabled(false);
        const wpc::Matrix4 e=wpc::multiply_matrix(track.unwrap(),track.entry_frame(0));
        const float fx=e[8],fz=e[10],l=std::sqrt(fx*fx+fz*fz);
        ms.place(e[12]-fx/l*1500,e[14]-fz/l*1500,std::atan2(-fx/l,fz/l));
        require(ms.player().section==int(track.sections())-1,"the seam pose is in the last section");
        ms.use_weapon_now(25);
        require(ms.projectiles().size()==1,"one missile before the seam");
        bool crossed=false;int after=0;
        for(int i=0;i<60 && !ms.projectiles().empty();++i) {
            ms.update({});
            if(ms.projectiles().empty()) break;
            const wpc::Projectile& p=ms.projectiles()[0];
            const wpc::RaceTrack::Route r=track.route(p.position);
            if(p.section==0 && r.hit && r.section==0) crossed=true;
            if(crossed) ++after;
        }
        require(crossed && after>3,"the missile flies on into section 0");
    }
    std::printf("%s: %d lap%s in %.1f s, %d respawns, %d glides\n",path.c_str(),race.laps_completed(),laps==1?"":"s",race.race_seconds(),race.respawns(),race.player_glides());
}
}
int main(int argc,char** argv) {
    try {
        test_camera();test_speed();test_steering_and_motion();test_effects();test_objects();
        std::printf("native drive: %zu synthetic checks passed\n",checks);
        for(int i=1;i<argc;++i) test_course(argv[i]);
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native drive: %s (%zu checks)\n",e.what(),checks);return 1;}
}
