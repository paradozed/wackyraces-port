#ifndef WPC_RACE_WEAPONS_H
#define WPC_RACE_WEAPONS_H
#include <array>
#include <cstdint>

namespace wpc {
using Vec3=std::array<float,3>;
// 0x47d890(r, g, b, n), the hit's vertex colour sequence for the car model
// (+0x3e, +0x42 vertices; the start colours saved at +0x56): mode +0x4a 0
// steps every vertex 1/13 (0x513010) toward the target and goes on once
// every step is below 2/255 (0x51300c); 1 sets the target; 2 holds it for
// n + 1 updates (+0x46); 3 steps back toward the saved colours the same way;
// 4 restores them; 5 returns 1. Every vertex is lerp(start, target, f), so
// the steps depend only on spread, each channel's largest |target - start|.
struct Tint {
    std::array<float,3> target{};
    float f=0;
    int mode=0,hold=0;
};
bool step_tint(Tint& t,const std::array<float,3>& spread,int n=10);

// WackyAutoBomba (factory 0x47c950, update 0x47e220, the car's +0x36d): a
// car's reaction to a hit. A weapon writes the hit type into +0x2c only
// while it is 0 (so a held car cannot be hit again) and activates the
// action: 4 WPaintShot, 0x40 WPU_Tnt, 0x80 WPU_Landmine, 0x200 course
// hazards, 0x400 WPU_Storm, 0x1000 WPU_Detour. Every type holds the car for
// 25 updates (+0x82 < 0x19: speed +0x3ef = 0, motion +0x3eb = -5), sets
// +0x8db = 20 on the 25th and ends with its visual sequence.
struct BombHit {
    uint16_t type=0; // +0x2c
    int counter=0;   // +0x82
    // +0x86: 1 at construction (0x47d316), 2 while WPU_Shield runs, 4 when
    // it is ending, 8 after it absorbed a paint hit (it then ends within 30
    // updates and still absorbs). Kept across hits.
    int guard=1;
    // The car model's vertex colour range, for 0x47d890 (provisional default:
    // white; the session takes it from the drawn model when given).
    std::array<float,3> colour_lo{{1,1,1}},colour_hi{{1,1,1}};
    Tint tint;          // f = 0 shows the model's own colours
    float scale_y=1;    // 0x43b190(1, +0x66, 1), type 0x200's squash
    float squash_v=0;   // +0x62
    int sub=0;          // +0x28
    bool done=false;    // +0x36
    bool active() const {return type!=0;}
};
// WackyAutoBomba's activation (0x47d450) in a race: guard 8 cancels the
// hit; guard 2 cancels it and becomes 1, or 8 for paint (type 4). Returns
// whether the hit takes; the cancelled action clears +0x2c (0x47f690).
bool start_bomb(BombHit& hit,uint16_t type);
struct BombStep {bool hold=false;bool boost=false;};
// What the activation (0x47d66f..) adds to the car: types & 0xda a hop, a
// +0x48b slot (0, 50, 0) decaying by 0.9; types & 0x3fc a 4 pi spin; 0x1000
// a -pi turn and 0x800 a +pi turn (WPU_Detour).
struct BombKick {bool hop=false;float spin=0;};
BombKick bomb_kick(uint16_t type);
// The slots' rotation (0x4726c0 flag 0x10, 0x472890 tail, 0x4727b0): a
// request V sets the remaining turn to V - done when it widens the turn;
// each update turns by a ninth of the rest (0x512a98), or the whole rest
// below 3 degrees (0x512a24); the step applies to the next update's yaw.
struct Spin {float left=0,step=0,done=0;};
void request_spin(Spin& s,float v);
void decay_spin(Spin& s);
// One action update, after the car's own (WackyAutoNormal runs first in the
// template's action list). Each type sets the done flag +0x36 and the next
// update clears +0x2c (0x47f690) and does nothing else:
//   0x200 at 25 (stage 2 from +0x28 > 0x17), squashed meanwhile;
//   0x400 at 27 (stage 1 -> 2 past 0x19, then 3), dark (10, 10, 0) until 27;
//   0x10/0x100 at 0x2e;
//   8/0x40/0x80 at 25 or later, once 0x47d890(10, 10, 0, 10) returns 1;
//   4 (paint) at stage 5 (78), with 0x47d890(20, 20, 255, 10) done;
//   2 (tank gun) at stage 8 (from 50), once 0x47d890(100, 100, 0, 10), from
//   the (100, 100, 0) written at stage 4 (42), returns 1.
// Other types set it at once and hold nothing. The paths without their
// model (+0x52, +0x5a) are not ported.
BombStep update_bomb(BombHit& hit);
constexpr int bomb_hold_updates=25;

// TurboSpeed (gadget icon 8; init 0x4d6cb0, update 0x4d71e0): row[0] (the
// speed limit) x 2.2 (0x5162fc) and row[1] (the convergence) x 10
// (0x510254) of the car's 0x6f2990 row, +0x385 = 8; the rows come back on
// the 61st update. The yaw's +0x21b keeps the unscaled top speed.
constexpr int turbo_updates=61;
constexpr float turbo_top=2.2f,turbo_convergence=10;
// WPU_Shield (gadget icon 10; init 0x4d7740, update 0x4d7870): +0x385 = 10,
// guard 2, m_shieldtime (template +0x3c, 200 in general.wgm) updates.
struct Shield {
    int state=0; // +0x68
    int timer=0; // +0x64
    bool done=false;
};
Shield start_shield(BombHit& hit,int time=200);
// One update: guard 1 (the shield took a hit) or the timer at 0 goes to
// state 1 (guard 4, 6 more updates), then 3 (guard 1, +0x385 = -1). Guard 8
// cuts the time to 30.
void update_shield(Shield& shield,BombHit& hit);

// WBreath, the guided projectile of a DragonBreath clone (vtable 0x512c00,
// init 0x474e30, update 0x475040; the DragonBreath, 0x4762c0, applies the
// hit type +0x12a to the car it struck once the action ends, then clones
// m_idExplosion). Template members, values at +0x24..+0x6c.
struct BreathParams {
    float range=6000,angle=1.0472f,speed=300,maneuver=8,scan=2000,height=200,aim=1000,travel=20000,climb=100,lowering=30;
};
struct Projectile {
    Vec3 position{},velocity{};
    BreathParams p{};
    uint16_t type=0;     // DragonBreath +0x12a
    int owner=-1;        // WBreath +0x24
    int target=-2;       // +0x28, -2 none
    int struck=-2;       // +0x40, the car hit
    float travelled=0;   // +0x38
    float radius=150;    // the DragonBreath's PRIM sphere
    int section=0;       // +0x104, for the loop seam in 0x4a71e0
    uint32_t object=0;   // the cloned DragonBreath
    bool done=false;
};
// The weapons that clone one: icon 25 WPU_Missile (DragonBreath 0x38c, type
// 8), 14 WPU_Breath (0x39a, type 8, maneuverspd 20), 16 WPU_TankGun (0x3a8,
// type 2, aimed at once), 15 WPU_Boomerang (0x3a1, type 0x10, aimed at
// once; maneuverspd 50, angle 2.1, range and travel_dist 10000).
bool breath_weapon(int icon,Projectile& out);
// 0x474d50: the velocity towards the target at ABSspd.
void aim_projectile(Projectile& p,const Vec3& target);
// 0x4a71e0's test of one racer, d its offset from the frame (already moved
// across the loop seam): every axis within range (a box), then the
// truncated integer squared distance at most range^2, and the horizontal
// angle atan2(-dx, dz) less the heading, wrapped to +/-pi (0x496f90),
// strictly within +/-angle. Returns the squared distance, or -1.
int64_t target_distance(const Vec3& d,float heading,float angle,int range);
// 0x475130..0x475576: towards the target by maneuverspd (horizontally
// unless within aimontarget, then in 3D); away from it the projectile
// keeps H above the ground found FWDscan ahead, climbing at most climb and
// lowering at most loweringspeed per update (provisional reading of
// 0x47529a..0x475480); the horizontal speed is capped at ABSspd.
// reach is the number of updates to the ground found (FWDscan / ABSspd).
void steer_projectile(Projectile& p,const Vec3* target,bool ground,float ground_y,float reach=1);

// WPU_Tnt (weapon icon 24; init 0x4dcac0, update 0x4dce10). Template:
// m_rForwardSpeed 300 (+0x4c), m_rRiseSpeed 25 (+0x54), m_rGravity 4
// (+0x5c), m_rSpeedReducer 0.8 (+0x64), m_idBounceFilm (+0x44),
// m_idExplosion (+0x3c, WExplosion 0x3ac); a 150 sphere.
struct Tnt {
    Vec3 position{},velocity{}; // +0x68 the velocity
    float speed=300;            // +0x74
    int bounces=0;              // +0x64
    int owner=-1;               // +0x28
    int section=0;              // the object's +0x104, the thrower's at first
    int life=300;               // m_nLifeTime (provisional: not read here)
    bool done=false;
};
constexpr float tnt_forward=300,tnt_rise=25,tnt_gravity=4,tnt_reducer=0.8f,tnt_radius=150,tnt_range=5000,tnt_angle=1.0472f;
// 0x4dcac0: the car's frame lifted by 100; at a target (0x4a71e0, 60
// degrees, 5000) the velocity is the unit direction x 300 plus 25 up,
// otherwise the car's (0, 25, 300).
Tnt throw_tnt(const Vec3& position,const Vec3& forward,int owner,const Vec3* target);
// 0x4dd087..0x4dd38c, a ground hit: on the third bounce it explodes;
// otherwise it sits 10 along the normal, its speed x 0.8, and heads for a
// car in view with 25 up, or reflects off the ground with X and Z x 0.8.
// Returns false when it explodes. The view is 0x4dd1e9's 0x4a71e0 query
// (thunk 0x403d69): owner the TNT itself (so the thrower is a candidate),
// a frame at the new position along the incoming velocity, 60 degrees, 5000.
bool bounce_tnt(Tnt& t,const Vec3& hit,const Vec3& normal,const Vec3* target);

// WPU_Storm (weapon icon 21; init 0x4e15f0, update 0x4e1890). Template:
// m_nLifeTime 150 (+0x24), spd 250 (+0x3c). The cloud flies 400 above
// (0x5169dc) along the route; each car within 700 (0x4a71e0, any
// direction) is struck once (the +0x90 vehicle mask, the thrower's bit
// set at the start) with type 0x400, written unconditionally, and followed
// for 30 updates (2 when its guard is not 1); past its life it rises by 4
// per update and fades for 40 updates.
struct Storm {
    Vec3 position{},velocity{},waypoint{};
    int state=0;        // +0x34: 0 roaming, 1 over a car, 2 fading
    int counter=0;      // +0x38
    int timer=0;        // +0x3c
    unsigned mask=0;    // +0x90
    int owner=-1,target=-2;
    int section=-1;uint32_t polygon=0xffffffff; // +0x8c, the last route polygon
    float fade=1;       // +0x152
    bool done=false;
};
constexpr float storm_speed=250,storm_height=400,storm_range=700;
constexpr int storm_life=150;

// WPU_Landmine (weapon icon 19; init 0x4df5b0, update 0x4df670). Template:
// m_nLifeTime 300 (+0x24), m_nUnlockTime 15 (+0x3c), m_rGravity 50 (+0x44),
// m_rTrapRange 200 (+0x54), m_idExplosion (+0x4c, WExplosion 0x3ac).
struct Landmine {
    Vec3 position{};
    float yaw=0;
    Vec3 normal{{0,1,0}};
    int state=0;    // +0x34: 0 falling, 1 locked, 2 armed, 0xffff exploding
    int life=300;   // +0x38, counted down every update
    int lifetime=300; // m_nLifeTime, for the unlock test
    int owner=-1;   // +0x28: the car that dropped it (-1 the player)
    int section=0;  // the object's +0x104 (provisional: the dropper's)
    uint16_t type=0x80;
    float gravity=50,trap=200;
    bool detour=false;
    bool oil=false;     // WPU_OilSpill (icon 17): slides the car, fades after
    // WPU_BeeHive (icon 18; init 0x4de850, update 0x4de9b0): sprung, the
    // hive follows its car for m_nTrapTime (80, +0x3c) updates: every 15 a
    // steer kick (rand(1000) x 0.001 + 0.2) x m_aMaxSteerMod (0.524) of
    // alternating sign is added each update to the car's steer (+0x2ad
    // robot, +0x403 player), clamped to +/-0.698, and its speed is capped at
    // m_rTargetSpeed (0.3).
    bool hive=false;int victim=-2,trap_time=0;float kick=0;
    int scale_num=10,fade=0; // WPowerUpAction m_nScaleNum (+0x2c), the fade count (+0x3c)
    bool done=false;
};
// WPU_OilSpill (init 0x4ddb90, update 0x4ddcf0): as the mine, from the car's
// frame lifted 10, m_nLifeTime 300, m_nUnlockTime 15, m_rGravity 50,
// m_rTrapRange 200, m_nScaleNum 10.
Landmine drop_oil(const Vec3& position,float yaw,int owner);
Landmine drop_hive(const Vec3& position,float yaw,int owner);
// WPU_FallingTree (20; init 0x4e0710, update 0x4e09e0) and WPU_BrickWall
// (22; 0x4e2380, 0x4e2650): the object starts m_rForwardDist (2000) ahead of
// the car and m_rStartHeight (1000) up, falls m_rGravity (100) an update to
// the ground, then each update the query 0x405051 (range 300, angle 2 pi,
// flags 7) stops dead (+0x3ef = 0) every car near it once (vehicle mask
// +0x70); after m_nLifeTime (150) it fades m_nScaleNum updates (3 wall, 5
// tree). The held preview (+0x395) and the camera shake (+0x20/+0x24 =
// rand() % 5) are not drawn.
struct Barrier {
    Vec3 position{};
    float yaw=0;
    int owner=-1,section=0;
    bool tree=false,landed=false,done=false;
    int life=150,fade=0,scale_num=3;
    uint32_t stopped=0;
};
Barrier drop_barrier(const Vec3& position,float yaw,int owner,bool tree);
// The mine starts on the dropping car's frame lifted by 10 (0x4df5f3).
Landmine drop_landmine(const Vec3& position,float yaw,int owner);
constexpr float landmine_gravity=50,landmine_trap_range=200;
constexpr int landmine_unlock=15;
// WPU_Detour (weapon icon 23; init 0x4e3290, update 0x4e33c0): a sign 270
// behind the car (along its horizontal forward axis), m_nLifeTime 150,
// m_rGravity 100, m_nUnlockTime 15; on landing rand() % 100 < 50 picks
// type 0x1000, else 0x800 (the sign turned round); armed, the car the
// 0x4a71e0 query finds (0x4e373a: owner excluded, 2 pi, flag 1, range
// +0x2c, provisional: m_rTrapRange's 150), idle and strictly within 150 in
// 3D (0x51493c), takes the type.
Landmine drop_detour(const Vec3& position,float yaw,int owner,bool left);
}
#endif
