#ifndef WPC_GAME_DRIVE_H
#define WPC_GAME_DRIVE_H
#include <cstdint>

namespace wpc {
// Per-vehicle WackyAuto tuning. Rows come from the 24-float table at
// 0x52b558 (copied to 0x6f2990 by 0x483a20), steering from 0x4660b0.
struct VehicleTuning {
    float top_speed=0;      // row[0], WackyAuto+0x21b
    float convergence=0;    // row[1]
    float yaw_reference=0;  // row[2], used by 0x46a4a0's yaw term
    float steer_step=0;     // +0x937, radians per update
    float slide_push=0;     // row[4], scales the slide's push (0x46af3c)
    float reverse_step=0.023f; // +0x933 (initial +0x223)
    float gravity=0.05f;    // WackyAuto m_gravity (property 0xef, +0x98); doubled for robots (0x4e45e0)
};
VehicleTuning vehicle_tuning(int vehicle);

// gadget and weapon are the 0x52aac8/0x52aacc input bits (0x0001, 0x0002).
// view: input bit 0x8 pressed (control 4), look_back: 0x4 held (control 5),
// read by the follow camera 0x478070 (masks 0x52aad0/0x52aad4).
struct DriveInput { bool accelerate=false, brake=false, left=false, right=false, gadget=false, weapon=false, view=false, look_back=false;
    // A joystick's x axis byte (-128..127, 0 centred). Non-zero, the
    // original's steering takes its analog branch (0x46768e), which skips
    // the keyboard keys and the creep (0x4677da).
    int axis=0; };

// Normalized forward speed (+0x3ef) and brake/reverse state (+0x257).
struct SpeedState { float speed=0; int brake_state=0; };
// Surface modifiers, 1 at construction (0x469d70), set by the ground
// contact's surface +0x8bb in 0x470218 from 0x52b558 rows 6..23 in blocks
// of (top, accel, yaw): sand 4, grass 0x10, snow 0x20, dirt 8, 0x80, ice
// 0x40; surface 2 resets all to 1, others keep them. friction is
// m_*frictionmod (all 1) on 4..0x20, 1.5 on 0x80, kept on 0x40.
struct SurfaceGrip {
    float accel=1;    // +0x8bf: every acceleration and braking step
    float top=1;      // +0x8c3: the top speed and the -0.4 reverse limit
    float yaw=1;      // +0x8c7: the yaw step (0x46ad5d)
    float friction=1; // +0x8cb: the slide's x
};
void update_grip(SurfaceGrip& grip,int vehicle,uint32_t surface);
// 0x466df6, first in the speed update of every car: while the counter
// +0x8db runs, +0x8bf doubles and the counter drops by one. The contact
// (0x470218) rewrites +0x8bf on the known surfaces, so the doubling only
// compounds on the others. WackyAutoBomba (0x47c950, update 0x47e220) sets
// it to 20 once a bombed car has been held 25 updates at speed 0.
void update_boost(SurfaceGrip& grip,int& counter);
// 0x466db0, keyboard branch, with the surface modifiers +0x8bf/+0x8c3.
// slide_kick is +0x883 from the previous update's slide start: the forward
// acceleration is x 0.2 (0x4671f1) and the forward braking x 0.1 (0x467334).
// follow_lock: +0x89b, set by a car contact; while nonzero the forward
// acceleration below the limit is skipped (0x4671f7).
void update_speed(SpeedState& state,const VehicleTuning& tuning,const DriveInput& input,const SurfaceGrip& grip={},bool slide_kick=false,bool follow_lock=false);
// 0x466db0 robot branch (m_isrobotauto +0xc0 = 1): WackyAuto +0x2ac 1 forward (the
// keyboard's acceleration), 2 back (minus +0x933 = 0.023 down to -0.4, then
// +0.0096), 3 minus 0.0096, 0 coast; also in the air.
// While +0x89b runs, throttle 1 leaves the speed as it is (0x466ee9 below
// the limit, 0x466f35 at or above it).
void update_robot_speed(SpeedState& state,const VehicleTuning& tuning,int throttle,const SurfaceGrip& grip={},bool follow_lock=false);
// 0x467886: the robot's +0x2ad becomes the steering, then the 0.8 decay and
// deadband (no 40-degree clamp).
float robot_steering(float steer);
// 0x467640, keyboard branch: step, clamp +/-40 degrees, decay 0.8, deadband.
float update_steering(float steer,const VehicleTuning& tuning,const DriveInput& input);
// 0x4677da..0x467806 (the steering update 0x467640, after the speed's
// 0x466db0 in the car update 0x46a719/0x46a720): a steering key held at a
// speed in [0, 0.1) sets the speed to 0.05 and +0x257 to 1, so a stopped
// car turns and creeps (Wine, cc_t1 grid: Left alone gives 0.0500 at once).
// Keyboard steering of a human car only: skipped while a joystick axis is off centre.
void steer_creep(SpeedState& state,const DriveInput& input);

// The skid flag +0x25b (0x47060f) = +0x25f || +0x263, which WCarFx's tyre
// tracks use on surface 2.
struct SkidState {
    bool braking=false; // +0x25f
    int brake_timer=0;  // +0x158
    bool sliding=false; // +0x263
    int slide_timer=1;  // the template's m_faroltime slot (+0xb8), 1 serialized
    bool kick=false;    // +0x883: set on the update a slide starts
    float started=0;    // x on that update, else 0
    bool marking() const {return braking || sliding;}
};
// 0x4670c3..0x4673d8, after the speed update: accelerating counts +0x158
// down and clears +0x25f below 0; braking forward (state 0) above |speed|
// 0.015 on the ground sets +0x25f and +0x158 = 1, in state 1 clears it;
// neither key clears both. accelerate/brake are the keys or robot throttle
// 1/2; speed and brake_state are taken before that update's change.
void update_brake_skid(SkidState& skid,bool accelerate,bool brake,float speed,int brake_state,bool grounded);
// 0x46ae42..0x46b0cc: x = speed * friction +0x8cb * |steer +0x403|. Above
// m_farolspeedlimit on the ground with the timer at 0, the slide starts
// (+0x263 = 1, timer 3); otherwise the timer counts down to 0 and the slide
// ends once x < 0.6 limit with +0x158 clear.
// A start also sets +0x883 and keeps x in started; robots (m_isrobotauto,
// +0xc0 = 1, set by 0x49d580 for every racer but the player) put the timer
// back to 0 at once (0x46b063), so they restart on every update above it.
void update_slide(SkidState& skid,float speed,float steer,float limit,float friction,bool grounded,bool robot=false);
// general.wgm WackyAuto m_farolspeedlimit (member 0xf0) by vehicle.
float farol_speed_limit(int vehicle);
// general.wgm WackyAuto m_farolbendmod (template +0xa8) by vehicle: on a
// slide start the update's yaw is x m_farolbendmod x x (0x46aee8..0x46af25).
float farol_bend_mod(int vehicle);
// 0x46af30..0x46b051: the player's slide-start push, the diagonal between
// the movement probes (FL +0x40b - RR +0x42f when steering right, steer < 0,
// else FR +0x417 - RL +0x423) x x x row[4] x 1.2, into a +0x48b force slot
// decaying by 0.8 (flags 1; its 0x12c yaw part has no flag 2, unused).
// front is 0x468280 mode 2's stretched +102; the probes sit at +/-52 across
// and +/-front along. Returned in the car's (right, forward) frame.
struct SlidePush { float right=0, forward=0; };
SlidePush slide_push(float steer,float x,float front,const VehicleTuning& tuning);

// 0x469550: int(frame_ms * 0.06) clamped to 2..4 (0x4ad540), times 0.4667,
// capped at 2.0 (WackyAuto+0x163). A 33 ms frame gives 0.9333.
float frame_time_factor(double frame_ms);
// 0x468280 mode 2: rear probe at -102, front probe at +102 stretched by
// 1 + 0.09 * (speed - 0.4) above speed 0.4. 0x46abaa uses front - rear.
float probe_span(float speed);
// Displacement along the actor forward axis for one update (0x46abaa..ac3a,
// scaled by +0x163 at 0x46b8c2).
float forward_step(float speed,float time_factor);
// 0x46ac92..0x46ad5d then *+0x163: yaw change in radians for one update.
// Positive values turn toward local -X (left), as rotate_basis_y does.
// Robots (m_isrobotauto) skip the |row[2] - speed| x 0.8 term (0x46ad04).
// surface_yaw is +0x8c7.
float yaw_step(float steer,float speed,const VehicleTuning& tuning,float time_factor,bool robot=false,float surface_yaw=1);
}
#endif
