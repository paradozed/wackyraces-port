#ifndef WPC_GAME_MOTION_H
#define WPC_GAME_MOTION_H
#include "game_mesh.h"
#include <array>

namespace wpc {
// GDMotionLib (MTNL, loader 0x43fa60, version 0x1310988).
//
// Header: an opaque word (+0x18), the clip count, the channel-index count,
// the key count and the segment count (+0x2c). A clip (row) drives
// 3 x segments channels: channel c belongs to segment c / 3 and supplies its
// translation (c % 3 == 0), rotation in degrees (1) or scale - 1 (2), the
// x/y/z slots that 0x440650 reads from the 9-pair buffer. Clip channel c
// starts at keys[indices[first_index + c]]; keys are (local time, x, y, z).
struct MotionClip {
    int32_t controller=0;  // +0x04: controller = this + 1 (0x440900)
    int32_t next=-1;       // +0x08: clip chained when this one ends, -1 none
    int32_t duration=0;    // +0x14
    uint32_t channels=0;   // +0x0c
    uint32_t first_index=0;// +0x10, into indices
};
struct MotionKey {int32_t time;float x,y,z;};
struct MotionLib {
    uint32_t opaque=0,segments=0;
    std::vector<MotionClip> clips;
    std::vector<uint32_t> indices;
    std::vector<MotionKey> keys;
};
MotionLib parse_motion(const Archive& archive,uint32_t id);
MotionLib parse_motion(const ArchivePool& archives,uint32_t id);

// Per segment, nine (a, b) pairs: translation, rotation (degrees), scale - 1.
// a = -1 locks a slot. Controllers keep the largest positive value in a and
// the smallest non-positive one in b (0x43feb0); 0x440650 then resolves each
// slot to b, a, or a + b (motion_value).
using MotionPairs=std::vector<std::array<float,2>>;
float motion_value(const std::array<float,2>& pair);

// One clip controller (0x30-byte object, ctor 0x43fc80, bind 0x43fd10,
// play 0x440240, update 0x43feb0). Time is an integer; each GDHModel draw
// (0x437d00) advances active controllers by ftol(10 x speed).
class MotionController {
public:
    explicit MotionController(const MotionLib& lib);
    // 0x440240: weight scales the key values; offset delays the clip start.
    void play(size_t clip,int32_t offset=0,float weight=1.f);
    // 0x440290 commands 1 and 2: stop at the clip's end, or freeze on it.
    void stop_at_end() {flags_|=2;}
    void freeze_at_end() {flags_|=8;}
    bool active() const {return (flags_&1)!=0;}
    // Flag 4: frozen on the last values (command 2 reached the clip's end).
    bool frozen() const {return (flags_&4)!=0;}
    // Flag 1 set or cleared directly, as FlyPower's vehicle 6 does
    // (0x4d57a3, 0x4d5818: [GDHModel +0x166][1] +0x28).
    void set_active(bool on) {flags_=on?flags_|1:flags_&~1u;}
    // 0x440290 command 3: flag 4 toggled (frozen on the current values).
    void toggle_freeze() {flags_^=4;}
    // The controller's time (+0x2c), as WPU_Flipper writes it (0x4d9bff, 0x4da02e).
    void set_time(int32_t t) {time_=t;}
    // Advance by dt and accumulate into pairs (9 per segment); pairs may be
    // null, as for the zero step that play makes.
    void update(int32_t dt,MotionPairs* pairs);
private:
    struct Channel {const MotionKey* key=nullptr;int32_t interval=0,next_time=0,now=0,from=0;
                    std::array<float,3> rate{},target{},value{};};
    void accumulate(MotionPairs& pairs) const;
    const MotionLib& lib_;
    const MotionClip* next_=nullptr;
    int32_t start_=0,end_=0,next_start_=0,time_=0;
    float weight_=0,speed_=1;
    uint32_t flags_=0;
    std::vector<Channel> channels_;
};

// Controller 0 (0x440e30; built by 0x440be0, filters 0x440d40): a GDHModel's
// procedural sway. Each step differentiates the model's world position twice,
// turns the acceleration a into model space (0x43ba80: the dot products with
// the frame's axes), and filters seven inputs (0x440b40, coefficients
// 0x440ab0(gain, alpha, omega)):
//   translation -a (clamped to +-15), gains (0.1, 0.8, 0.1);
//   rotation (0, 1, 0.2) x a (0x452ed0, clamped to +-15), gains GDHModel
//   +0x17e (0.25, 0.25, 0.5 from the constructor 0x437720; a car's body
//   gets body_sway_gains);
//   y scale -0.01 a.y (clamped to +-20), gain 0.1.
// The nine values (t, r, (0, s, 0)) go into every segment's pairs as the clip
// controllers do: the largest positive in a, the smallest non-positive in b,
// locked slots (a = -1) left alone.
// 0x469f87..0x46a126 (the car init 0x469bf0, by vehicle, before the body's
// vtable +0x4c builds its controllers): the body GDHModel's +0x17e.
std::array<float,3> body_sway_gains(int vehicle);
class SwayController {
public:
    explicit SwayController(const std::array<float,3>& rotation_gain={{0.25f,0.25f,0.5f}});
    // world: the GDHModel's world matrix (column-major, axes in columns 0..2).
    void update(const std::array<float,16>& world,MotionPairs& pairs);
    // The last step's nine values (translation, rotation in degrees, scale - 1).
    const std::array<float,9>& values() const {return values_;}
private:
    struct Biquad {
        float c0=0,c1=0,b0=0,b1=0,b2=0,s0=0,s1=0;
        Biquad()=default;
        Biquad(float gain,float alpha,float omega);
        float step(float x);
    };
    std::array<Biquad,7> filters_;
    std::array<float,3> position_{},velocity_{};
    std::array<float,9> values_{};
    bool started_=false;
};

// 0x440650: rebuilds each segment matrix (identity, scale 1 + v, then X, Y
// and Z rotations, translation = SEGS translation + v, then times the
// parent's) and transforms each vertex's rest position by its segment.
// pairs.size() must be 9 x mesh.segments.size().
// segments, when given: each segment's posed matrix (row-vector basis, nine
// floats, then the translation), as the GDHModel keeps them (+0x15e -> +0xc).
void pose_mesh(Mesh& mesh,const MotionPairs& pairs,std::vector<std::array<float,12>>* segments=nullptr);
}
#endif
