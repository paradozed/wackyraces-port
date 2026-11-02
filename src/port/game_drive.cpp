#include "game_drive.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wpc {
namespace {
constexpr float coast_step=0.0096f;      // 0x5129d4
constexpr float stop_threshold=0.015f;   // 0x5129c8
constexpr float reverse_target=-0.4f;    // 0x5129d0
constexpr float minimum_target=0.8f;     // 0x5127ec
constexpr float steer_limit=0.6981317f;  // 0x5129d8/0x5129e0, 40 degrees
constexpr float steer_decay=0.8f;        // 0x5127ec
constexpr float steer_deadband=0.017f;   // 0x5129dc/0x511518
}
namespace {
// general.wgm's WackyAuto m_gravity (0xef) by vehicle.
constexpr float vehicle_gravity[7]={0.09f,0.09f,0.04f,0.05f,0.09f,0.07f,0.09f};
}
VehicleTuning vehicle_tuning(int vehicle) {
    // 0x52b558 rows [0..2], [4] and 0x4660b0's seven +0x937 constants.
    static const VehicleTuning table[7]={
        {0.70f,0.014f,1.07f,0.04886922f,1.5f},{0.65f,0.017f,1.3f,0.04363323f,1.0f},
        {0.70f,0.014f,1.3f,0.05235988f,1.2f},{0.58f,0.017f,1.3f,0.05585054f,1.1f},
        {0.65f,0.017f,1.3f,0.05235988f,0.2f},{0.58f,0.017f,1.3f,0.04712389f,0.3f},
        {0.74f,0.019f,1.6f,0.05061455f,0.7f}};
    if(vehicle<0 || vehicle>=7) throw std::runtime_error("vehicle tuning: invalid vehicle enum");
    VehicleTuning t=table[vehicle];t.gravity=vehicle_gravity[vehicle];
    return t;
}
void update_grip(SurfaceGrip& g,int vehicle,uint32_t surface) {
    // 0x52b558 rows 6..23: (top, accel, yaw) for sand, grass, snow, dirt,
    // 0x80 and ice.
    static const float rows[7][18]={
        {0.85f,0.9f,0.8f,0.85f,0.8f,0.9f,0.85f,0.8f,0.8f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.8f,0.9f,0.8f,0.8f,0.8f,0.9f,0.8f,0.8f,0.8f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.85f,0.9f,0.8f,0.85f,0.8f,0.9f,0.85f,0.8f,0.8f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.95f,0.9f,0.8f,0.95f,0.8f,0.9f,0.95f,0.9f,0.9f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.8f,0.9f,0.8f,0.8f,0.8f,0.9f,0.8f,0.9f,0.9f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.95f,0.9f,0.8f,0.95f,0.8f,0.9f,0.95f,0.9f,0.9f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f},
        {0.9f,0.9f,0.8f,0.9f,0.8f,0.9f,0.9f,0.8f,0.8f,0.9f,0.9f,0.9f,1,0.7f,1.1f,0.9f,0.9f,0.9f}};
    if(vehicle<0 || vehicle>=7) throw std::runtime_error("surface grip: invalid vehicle enum");
    int block;
    switch(surface) {
        case 2: g=SurfaceGrip{};return;
        case 4: block=0;break;
        case 0x10: block=1;break;
        case 0x20: block=2;break;
        case 8: block=3;break;
        case 0x80: block=4;break;
        case 0x40: block=5;break;
        default: return;
    }
    const float* r=rows[vehicle]+3*block;
    g.top=r[0];g.accel=r[1];g.yaw=r[2];
    if(surface==0x80) g.friction=1.5f;
    else if(surface!=0x40) g.friction=1;
}
void update_boost(SurfaceGrip& g,int& counter) {
    if(counter!=0) {g.accel*=2;--counter;}
}
void update_robot_speed(SpeedState& s,const VehicleTuning& t,int throttle,const SurfaceGrip& g,bool follow_lock) {
    // 0x466e17: the robots' branch runs in the air too (the player's skips
    // past airtime 6, 0x466f91; Wine, sh_t1: robots gain speed all through
    // a pyramid jump). Its +0x157 tests never see the flag set (0x46c858
    // clears it in the update that sets it).
    const float top=t.top_speed*g.top;
    switch(throttle) {
        case 1:
            // 0x466eb2: towards row[0] x +0x8c3 (at least 0.8) by row[1] x
            // +0x8bf; at or above the limit, the coast step, not below it.
            if(follow_lock) {}
            else if(s.speed<top) s.speed+=(std::max(top,minimum_target)-s.speed)*t.convergence*g.accel;
            else {s.speed-=coast_step;if(s.speed<top) s.speed=top;}
            s.brake_state=0;
            return;
        case 2:
            // 0x466e46: down to -0.4 x +0x8c3 by +0x933 x +0x8bf, then the
            // coast step back up to it.
            if(s.speed>reverse_target*g.top) s.speed-=t.reverse_step*g.accel;
            else s.speed=std::min(s.speed+coast_step,reverse_target*g.top);
            return;
        case 3: s.speed-=coast_step;return;
        default: {
            DriveInput none;update_speed(s,t,none,g);
            return;
        }
    }
}
float robot_steering(float steer) {
    steer*=steer_decay;
    if(steer>-steer_deadband && steer<steer_deadband) steer=0;
    return steer;
}
void update_speed(SpeedState& s,const VehicleTuning& t,const DriveInput& input,const SurfaceGrip& g,bool kick,bool follow_lock) {
    // 0x467127: the limit is row[0] x +0x8c3; the convergence still aims at
    // the unscaled row[0] (at least 0.8), every step x +0x8bf.
    const float top=t.top_speed*g.top;
    if(input.accelerate && !input.brake) {
        if(s.speed<top) {
            if(s.speed<0) {s.speed+=t.reverse_step*g.accel;s.brake_state=2;}
            else {
                if(!follow_lock) s.speed+=(std::max(t.top_speed,minimum_target)-s.speed)*t.convergence*g.accel*(kick?0.2f:1.f);
                s.brake_state=0;
            }
        } else {
            s.speed-=coast_step;
            if(s.speed<top) s.speed=top;
        }
        return;
    }
    if(input.brake && !input.accelerate) {
        if(s.brake_state==2) {
            // Reverse: approach -0.4 with the vehicle's convergence step.
            s.speed-=t.convergence*g.accel;
            if(s.speed<reverse_target*g.top) s.speed=reverse_target*g.top;
        } else if(s.brake_state==1) {
            s.speed=0;
        } else {
            s.speed-=t.reverse_step*g.accel*(kick?0.1f:1.f);
            if(s.speed<0) {s.speed=0;s.brake_state=1;}
        }
        return;
    }
    if(s.brake_state==1) s.brake_state=2;
    if(s.speed>0) s.speed=std::max(0.f,s.speed-coast_step);
    else if(s.speed<0) s.speed=std::min(0.f,s.speed+coast_step);
    if(std::abs(s.speed)<stop_threshold) s.speed=0;
}
float update_steering(float steer,const VehicleTuning& t,const DriveInput& input) {
    if(input.left!=input.right) steer+=input.left?t.steer_step:-t.steer_step;
    steer=std::clamp(steer,-steer_limit,steer_limit)*steer_decay;
    if(steer>-steer_deadband && steer<steer_deadband) steer=0;
    return steer;
}
float frame_time_factor(double frame_ms) {
    double units=std::trunc(std::max(0.0,frame_ms)*0.06);
    units=std::clamp(units,2.0,4.0);
    return float(std::min(units*0.46666667,2.0));
}
float probe_span(float speed) {
    float front=102;
    if(speed>0.4f) front*=(speed-0.4f)*0.09f+1;
    return front+102;
}
float forward_step(float speed,float time_factor) {return probe_span(speed)*speed*time_factor;}
float yaw_step(float steer,float speed,const VehicleTuning& t,float time_factor,bool robot,float surface_yaw) {
    if(speed==0) return 0;
    // 0x512a80 (-1.05) in reverse, 0x512a00 (0.7) forward, clamped to
    // +/-5 degrees (0x512654/0x51265c).
    float term=std::clamp(steer*(speed<0?-1.05f:0.7f),-0.08726646f,0.08726646f);
    if(robot) return term*surface_yaw*time_factor;
    const float clamped=std::min(speed,t.top_speed);
    term*=std::abs(t.yaw_reference-clamped)*0.8f*surface_yaw;
    return term*time_factor;
}
void update_brake_skid(SkidState& k,bool accelerate,bool brake,float speed,int brake_state,bool grounded) {
    if(accelerate) {
        if(--k.brake_timer<0) {k.brake_timer=0;k.braking=false;}
    } else if(brake) {
        if(brake_state==1) k.braking=false;
        k.brake_timer=0;
        if(brake_state==0 && std::abs(speed)>0.015f && grounded) {k.braking=true;k.brake_timer=1;}
    } else {k.braking=false;k.brake_timer=0;}
}
void update_slide(SkidState& k,float speed,float steer,float limit,float friction,bool grounded,bool robot) {
    const float x=speed*friction*std::abs(steer);
    if(x>limit && grounded && k.slide_timer==0) {
        k.slide_timer=robot?0:3;k.sliding=true;k.kick=true;k.started=x;
        return;
    }
    k.kick=false;k.started=0;
    if(--k.slide_timer<=0) k.slide_timer=0;
    if(x<limit*0.6f && k.brake_timer==0) k.sliding=false;
}
float farol_speed_limit(int vehicle) {
    static const float limits[7]={0.1f,0.1f,0.09f,0.095f,0.1f,0.13f,0.085f};
    return vehicle>=0 && vehicle<7?limits[vehicle]:0.1f;
}
float farol_bend_mod(int vehicle) {
    static const float mods[7]={13,13,13,13,7,8,8};
    return vehicle>=0 && vehicle<7?mods[vehicle]:13;
}
SlidePush slide_push(float steer,float x,float front,const VehicleTuning& t) {
    // FL - RR = 2 front forward - 104 right; FR - RL = 2 front forward + 104 right.
    const float scale=x*t.slide_push*1.2f;
    return {(steer<0?-104.f:104.f)*scale,2*front*scale};
}
}
