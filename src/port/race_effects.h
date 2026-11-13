#ifndef WPC_RACE_EFFECTS_H
#define WPC_RACE_EFFECTS_H
#include "game_random.h"
#include "game_transform.h"
#include <array>
#include <vector>

namespace wpc {
using Vec3=std::array<float,3>;
// A wheel's ground contact (WackyAuto +0x43b + 0x14 w: position, +0x447
// the hit record's flags masked by the 0x018000ff probe query, 0x46d96f).
// Wheels follow the template's m_monstertires11/12/21/22 (0x11c..0x11f,
// matrices whose translation is the tyre): (-x,0,z), (x,0,z), (-x,0,-z),
// (x,0,-z) in the actor frame, (x, z) per m_iVehicle (0xed) in general.wgm:
// 0 (80,60), 1 (80,60), 2 (45,60), 3 (60,60), 4 (70,60), 5 (88,66), 6 (60,60).
struct WheelContact {bool hit=false;Vec3 position{};uint32_t surface=0;};
constexpr std::array<std::array<float,2>,4> wheel_offsets{{{{-70,60}},{{70,60}},{{-70,-60}},{{70,-60}}}};
inline std::array<std::array<float,2>,4> vehicle_wheels(int vehicle) {
    static constexpr float tyres[7][2]={{80,60},{80,60},{45,60},{60,60},{70,60},{88,66},{60,60}};
    if(vehicle<0 || vehicle>6) return wheel_offsets;
    const float x=tyres[vehicle][0],z=tyres[vehicle][1];
    return {{{{-x,z}},{{x,z}},{{-x,-z}},{{x,-z}}}};
}

// WCarFx (the action on every WackyAuto, update 0x47a460) and the objects
// it clones from WCarFxStorage. One update per game update.
class CarEffects {
public:
    // Effects draw from their own MSVC stream (0x430ef0, state 0x5472c4),
    // seeded with rand()'s seed by 0x430e20.
    CarEffects():CarEffects(0x1312d6e) {}
    explicit CarEffects(uint32_t seed):random_(seed) {}
    // frame: the actor frame (X right, Y up, Z forward); speed +0x3ef;
    // splash: the car has just entered deep water (0x470052 clones WSplash).
    // skid: +0x25b, set while +0x25f or the slide state +0x263 is (0x47060f).
    void update(const Matrix4& frame,float speed,const std::array<WheelContact,4>& wheels,bool splash,
                const Matrix4& splash_frame,bool skid=false);
    void update(const Matrix4& frame,float speed,const std::array<WheelContact,4>& wheels,bool splash) {update(frame,speed,wheels,splash,frame);}

    // Dust puffs (0x47bb10, 32 slots at +0xa28), drawn with WCarFxStorage's
    // second GD3DFilm at the car's position plus the puff's offset.
    struct Puff {int age=0,reel=0,frame=0;Vec3 offset{},velocity{};};
    const std::array<Puff,32>& puffs() const {return puffs_;}
    // WalkOnWater ripples (0x47c790/0x47c7d0): world frame and scale.
    struct Ripple {Matrix4 base{};int counter=0;float scale=1;};
    const std::vector<Ripple>& ripples() const {return ripples_;}
    // WSplash (0x473df0): column model 414 scaled (a, h, a) and ring 415
    // scaled (b, 0.01, b), each x 0.2, in the car frame at the drowning.
    struct Splash {
        Matrix4 base{};
        int state=0;
        float height=0,a=0,b=0,rise=0,grow_a=0,grow_b=0;
        Matrix4 column() const;
        Matrix4 ring() const;
    };
    const std::vector<Splash>& splashes() const {return splashes_;}
    // WKBumm (0x472dc0 on an 0x40000 object hit, init 0x486960, update
    // 0x486b20): eight film pieces thrown from the car's frame at the hit.
    struct Piece {Vec3 position{},velocity{};float angle=0,spin=0;int frame=0;bool landed=false;};
    struct Bumm {Matrix4 base{};std::array<Piece,8> pieces{};};
    void bumm(const Matrix4& frame,float speed);
    const std::vector<Bumm>& bumms() const {return bumms_;}
    // A hit's smoke (WackyAutoBomba 0x47ed6e, types 8, 0x40, 0x80): a clone of
    // m_smokeparticle (general.wgm GDStdBrick 0x1be) in the car's frame at the
    // spawn; its GD3DFilm 0x1bf (FILM 0x102f6, frame 0, no depth write) runs
    // the flamesmoke action (record 0x541cb8, init 0x4181e0, update 0x418310).
    // position: in the clone's frame, from (0, 0, 0); alpha: 0..255.
    struct HitSmoke {Matrix4 base{};Vec3 position{},velocity{};int age=0;float spin=0,scale=0,angle=0;int alpha=0;};
    void hit_smoke(const Matrix4& frame,bool flip);
    const std::vector<HitSmoke>& hit_smokes() const {return hit_smokes_;}
    // The tank gun's hit, stage 3 (0x47e7b6): eleven clones of m_Particle
    // (general.wgm GDModel 0x394) running WReszecske (vtable 0x512f98, init
    // 0x47d050, update 0x47d130). frame: the clone's world matrix.
    struct Chunk {Matrix4 frame{};Vec3 position{},velocity{};int age=0;};
    void tank_chunks(const Matrix4& frame);
    const std::vector<Chunk>& chunks() const {return chunks_;}
    // The POUSSIERE option (0x6f4650, menu item 588; off by default): 24 film
    // puffs from the rear wheels (0x47b750, drawn by 0x47b9d0 with
    // WCarFxStorage's first GD3DFilm, 410).
    struct Smoke {int age=0;Vec3 position{},velocity{};};
    void set_smoke(bool on) {smoke_on_=on;}
    const std::array<Smoke,24>& smoke() const {return smoke_;}
    // Tyre tracks (0x47a590, drawn by 0x47a7c0): per wheel 16 segments of
    // 0x1d bytes at +0x24, newest first. A segment spans the wheel's contact
    // -/+ the vehicle's half width (0x52b804) along the car's X axis, stored
    // as integers; shade 0x30 or 0x5c is the grey, fade starts at 100 and
    // loses 6 each time the segment moves down.
    struct TrackSegment {Vec3 a{},b{};bool active=false;int shade=0,fade=0;};
    using TrackStrip=std::array<TrackSegment,16>;
    void set_track_width(float half_width) {track_width_=half_width;}
    float track_half_width() const {return track_width_;}
    const std::array<TrackStrip,4>& tracks() const {return tracks_;}
    // 0x52b804 by vehicle: the tracks' half width (0x469c78).
    static float track_width(int vehicle);
    // 0x47bec0: puff reel by surface type, -1 for none.
    static int puff_reel(uint32_t surface);
    // The stream itself, so cars can share the one global state in turn.
    MsvcRandom& random() {return random_;}
private:
    float unit() {return float(random_.next())*(1.f/32768.f);} // 0x430f30
    MsvcRandom random_;
    uint32_t updates_=0;          // +0xe10
    std::array<Puff,32> puffs_{};
    std::vector<Ripple> ripples_;
    std::vector<Splash> splashes_;
    std::vector<Bumm> bumms_;
    std::vector<HitSmoke> hit_smokes_;
    std::vector<Chunk> chunks_;
    bool smoke_on_=false;
    float track_width_=24;
    std::array<TrackStrip,4> tracks_{};
    void update_tracks(const Matrix4& frame,const std::array<WheelContact,4>& wheels,bool skid);
    std::array<Smoke,24> smoke_{};
};
}
#endif
