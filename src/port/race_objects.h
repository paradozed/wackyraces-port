#ifndef WPC_RACE_OBJECTS_H
#define WPC_RACE_OBJECTS_H
#include "game_scene.h"
#include "game_transform.h"
#include <algorithm>
#include <array>
#include <functional>
#include <vector>

namespace wpc {
using Vec3=std::array<float,3>;
// Moving course objects (dt_t1's trams). Each update moves them and lists
// the car hits; the session applies them (WackyAutoBomba type 0x200).
//
// WTram (brick update 0x4caf40, init 0x4cad90; members with a 4-byte base:
// m_Pal +0x88, m_Vel +0x90, m_Radius +0x98): a level crossing. Its light
// cycle 0x52d8cc has six phases {A, B, updates}: lamp bits 1 red, 2 amber,
// 4 green. The tram (the GDPath child's first child, +0x123) runs from the
// path's first point towards its second by m_Vel per update while B is
// green, and finishes its pass. Robots (+0xc0 = 1) entering m_Radius of
// the brick while A is red wait (+0x1c7 = 5) until A turns green.
// The brick runs only while its node is enabled (0x432970: node flag 2;
// 0x41ba60 then re-runs the init, 0x41ba90 calls 0x4caed0, which puts the
// tram back to its saved frame +0x127 and hides it). Flag 2 is set on the
// top-level nodes a WMOD route row links (0x4a8680) for the player's
// previous, current and next rows (+0x830, +0x8c4, +0x958; 0x49dc90 at
// the start, 0x4a12a0 / 0x4a09b0 when the player moves on or back), and
// cleared by 0x4a8850 on the row that leaves the window.
struct TramCrossing {
    uint32_t node=0,tram=0;
    Matrix4 path_frame=identity_matrix();  // the GDPath's world frame (the tram's parent)
    Matrix4 tram_local=identity_matrix();  // the tram's serialized frame x Rx(pi/2) (0x43b2b0)
    Vec3 centre{},from{},dir{};            // brick position; path start and unit direction (local)
    float length=0,speed=0;int radius=0;   // +0x15f, m_Vel, m_Radius
    std::vector<ScenePrim> spheres;        // the tram's PRIM spheres (local)
    int phase=0xff,timer=2;                // +0x122 (byte), +0x11e
    float progress=0;                      // +0x15b
    std::vector<int> sections;             // rows linking the brick (none: always enabled)
    bool active=false;                     // node flag 0x1000
    uint8_t lamp_a() const;
    uint8_t lamp_b() const;
    Matrix4 tram_frame() const;            // world
};
// WGoPath (update 0x4d1cd0, init 0x4d1a90) with WElTram's end handler
// (0x4d2330): the object follows a GDPath at t += 1 / m_nStep per update
// between m_nOffset and 1 - m_nItemLength + m_nOffset. Past an end with
// m_bLoop it wraps by 1 and flags the end; WElTram then waits m_Waiting
// updates and runs back from that end with the step negated. The action
// is walked only while its node is enabled (as above); disabling the node
// leaves the action's flags, so it resumes where it stopped, without its
// init (Wine, dt_t1), and a disabled node is not drawn (0x432490).
struct PathMover {
    uint32_t node=0;
    Matrix4 original=identity_matrix();    // +0x74: the node's frame at init
    std::vector<Vec3> points;              // the path's points in world space
    bool closed=false,loop=false,rotate=false,collision=false,el_tram=false;
    float t=0,step=0,start=0,end=1,spin=0,spin_step=0;
    Vec3 reference{};                      // +0xb4: second point - first
    bool finished=false;int waited=0,waiting=0;
    std::vector<int> sections;             // rows linking the node (none: always enabled)
    bool active=true;                      // node flag 0x1000
    std::vector<ScenePrim> prims;          // the node's PRIM (local)
    Matrix4 placed=identity_matrix();      // the frame the last update wrote (world)
    Matrix4 frame() const;                 // the frame at t (world)
};
// 0x444be0: the point at t on a GDPath (segments n - 1 when open, n when
// closed) and the blended tangent there.
void path_point(const std::vector<Vec3>& points,bool closed,float t,Vec3& at,Vec3& tangent);

// WLogRide (sh_t3; record 0x515944, init 0x4cc3e0, update 0x4cc450): a
// brick whose GDPathQ child (+0x23e) carries its WLogBase children (from
// +0x242) in groups of three. After 2 updates (+0x23a) the offset +0x246
// grows by m_vel (+0xa0, property 0x24f) per update and wraps past
// (m_length + 50) x 2 + m_dist (+0x88/0x24c, +0x98/0x24e). Log i sits at
// distance d along the path, d starting at the offset and growing by
// m_length + 50 inside a group, by m_dist after its third log. 0x4be7f0
// (flags 0xe) frames it at the midpoint of the points at d and d +
// m_length, y along that chord; 0x43b4a0 turns it by pi. Past the path's
// end a log keeps its frame. Each log's motion this update (kept when
// below 2 x m_vel per axis, 0x2aaf9c8) moves the cars whose ground object
// +0x89f is that log (0x4cc69e).
struct LogRide {
    uint32_t node=0;
    std::vector<Vec3> points;              // the GDPathQ's points (world)
    std::vector<Vec3> directions;std::vector<float> lengths;
    float length=500,width=188,dist=5000,vel=50;
    int delay=2;float offset=0;
    std::vector<uint32_t> logs;            // WLogBase node ids, child order
    std::vector<Matrix4> frames;           // each log's world frame
    std::vector<Vec3> moved;               // each log's motion this update
    std::vector<int> sections;
    bool active=true;
    // Each log's GMDL triangles in its node frame (the resource MATR
    // applied): the ground cars stand on (provisional: the sensor-group-2
    // PRIM is taken to be the model mesh).
    std::vector<std::vector<std::array<Vec3,3>>> triangles;
    float wrap() const {return (length+50)*2+dist;}
};
struct LogHit {bool hit=false;float y=0;Vec3 normal{};int ride=-1,log=-1,bridge=-1;};
// WMisty (cc_t2's bascule bridge; record 0x5153f0, init 0x4c5840, update
// 0x4c5b50). Children: two GDModel halves (+0x196, +0x19a), two GDSounds,
// then GDStdBrick sensors from +0x1a6. A car coming within m_BeSugar of a
// sensor (0x403d69), the sensor's bit +0x18a being clear, sets the bit and
// toggles the state +0x1af (0 shut, 1 opening, 2 open, 3 shutting); the bit
// clears when no car is within m_KiSugar. The angle +0x186 follows m_MaxForgas
// or 0 through a biquad (0x440ab0, 0x440b40; m_bqAlpha, m_bqOmega); within
// 1.745e-5 of the last value the state moves on. The halves turn by -angle
// and +angle about the axis m_Forgas (3: z, 0x43b690) from their saved
// frames. A car on a half while opening or open shuts it (0x4c5ec3).
// 0x4c5e83 writes the first forward link's weight of the section's route
// node 14: 0xffff or 0, by whether that link leads to node 40 (+0x1ae) and
// the state: on cc_t2 robots take the bridge when shut, the other way open.
struct MistyBridge {
    uint32_t node=0;
    std::array<uint32_t,2> halves{};
    std::array<Matrix4,2> base{{identity_matrix(),identity_matrix()}},frames{{identity_matrix(),identity_matrix()}};
    std::vector<Vec3> sensors;
    int axis=0,state=0;
    float max_angle=0,in_radius=0,out_radius=0,angle=0;
    std::array<float,5> filter{};float s0=0,s1=0;
    uint32_t inside=0;
    // The halves' GMDL triangles (the sensor-group-2 PRIMs taken as the mesh).
    std::array<std::vector<std::array<Vec3,3>>,2> triangles;
    std::vector<int> sections;
    bool active=true;
    int fork_section=-1;uint32_t fork_polygon=0;bool fork_to_40=false;
    bool open() const {return state==1 || state==2;}
    int fork_weight() const {return fork_to_40!=open()?0xffff:0;}
};
// 0x4be7f0's point at distance d along the polyline (false past the end).
bool path_distance_point(const std::vector<Vec3>& points,float d,Vec3& at);

// WGeyser (cc_t3, dr_t4; record 0x515640, init 0x4c8bd0, update 0x4c8cc0).
// States +0x17b: 0 idle: a car within m_Sugar of a sensor (the brick, then
// its children from the third) and rand % 100 < m_Chance aims at the car
// + its z axis x m_Distance; no car aims at random within m_Sugar. 1: the
// ring (second child) grows over m_Steps. 2: three biquad values rise, each
// starting m_Delay later, for m_Frames. 3: towards 0.6, fading over 50
// updates, then zeroed. 4: the ring shrinks over m_Steps. 5: m_Frames idle.
// With the highest value h above 0.1 a car in the column (m_Width across,
// m_Height x h high) stops, centred on it: it rises 100 an update (0x510414),
// or to the top once within m_MaxFly (0x4c930b), airtime 0; above the top
// it gets airtime 10 and falls.
// WFountain (dt_t2; record 0x5157b0, update 0x4ca320) shares the base: no aim
// and no ring; states 1 and 2 swap every m_Frames, the values (i-th after
// i x m_Delay) going from 0.8 to 1 and back. Its column holds cars the same
// way, within 100 of the top.
// WVolcano (dr_t2; init 0x4c9cc0, update 0x4c9cf0) shares the base too: as a
// fountain, from 0.6 to 1, with no column to hold cars.
struct Geyser {
    bool fountain=false,volcano=false;
    uint32_t node=0,column=0,ring=0;
    Vec3 home{},aim{};
    Matrix4 column_local=identity_matrix();
    std::vector<Vec3> sensors;
    float distance=0,max_fly=0;
    int sugar=0,height=0,steps=1,width=0,delay=0,chance=0,frames=1;
    std::array<float,5> filter{};
    std::array<float,3> s0{},s1{},rise{};
    int state=0,counter=0;
    float ring_scale=0,spin=0;
    std::vector<int> sections;
    bool active=true;
    float top() const {return std::max({rise[0],rise[1],rise[2]});}
    Matrix4 column_frame() const;
    Matrix4 ring_frame() const;
};
struct GeyserLift {size_t car=0;Vec3 position{};int airtime=0;};
// WLogRollingB (sh_t2; record 0x515158, update 0x4c23a0): the course's own
// WGNR is the log; this only turns the cars on it. Within 761 of the axis
// m_Dir (3: z, 1: x; 0x8d631 = 761 squared) and 4315 along it, a car's
// offset turns by m_ForgasLepes an update (0x4530e0: u' = c u + s v,
// v' = c v - s u).
struct RollingLog {
    uint32_t node=0;
    Vec3 centre{};
    int axis=0;float rate=0;
    std::vector<int> sections;
    bool active=true;
};
struct CarPlacement {size_t car=0;Vec3 position{};};
// WDynamiteHole (dr_t1; record 0x515aac, init 0x4cd790, update 0x4cd800).
// Armed (+0x126), a car within m_radius fires it: the fuse (second child)
// hides and the cars then in range are pushed by their offset + 100 up
// (0x40400c, decay 0.3). Fired, the lid (first child) rises by m_vel an
// update up to 10000 above its frame (0x512a9c); with
// no car within 10000 it re-arms, lid home and fuse shown.
struct DynamiteHole {
    uint32_t node=0,lid=0,fuse=0;
    Matrix4 home=identity_matrix();
    Vec3 centre{};
    float radius=0,vel=0,offset=0;
    bool armed=true;
    std::vector<int> sections;
    bool active=true;
    Matrix4 lid_frame() const {Matrix4 m=home;m[13]+=offset;return m;}
};
struct CarPush {size_t car=0;Vec3 force{};float decay=0;float yaw=0;float yaw_decay=-1;};
// WMagnet (dr_t3; init 0x4d0940, update 0x4d09e0): on for m_88..m_90 updates, off
// for m_98..m_a0. While on, every car of its section within |dz| < 1000 and the
// circle x, y of the radius between its first and third children (+0x123; Wine, dr_t3: 266) turns by
// m_b8 that update (a type-2 slot, 0x4d0b49).
struct Magnet {
    uint32_t node=0;
    Vec3 centre{};
    int radius=0,on_min=0,on_max=0,off_min=0,off_max=0,timer=0;
    float yaw=0;
    bool on=false;
    std::vector<int> sections;
    bool active=true;
};
// WRotoHill (dr_t2; update 0x4cf6f0): a turntable. A car of its section within
// m_88 (robots are told at twice that to use their gadget: +0x1c7 = 0xb) and with
// no gadget effect running is caught. Phases (+0x1f3 bits 2..4): 1 for m_98..m_a0
// updates the turn speed +0x1f7 grows by m_90; 2 for m_a8..m_b0 updates it holds;
// 3 it falls by m_90 to 0; 4 until the car is 10000 away. Through 1..3 the car stops
// (+0x3ef, +0x403, its slots), its heading and its offset from the centre turn
// (0x4530a0), and it rises m_b8 an update with airtime 0 and no ground ray reach
// below (+0x93b = 0, restored in phase 4).
struct RotoHill {
    uint32_t node=0;
    Vec3 centre{};
    int radius=0,up_min=0,up_max=0,hold_min=0,hold_max=0;
    float step=0,rise=0;
    struct Hold {int phase=0,timer=0;float spin=0;};
    std::vector<Hold> holds;               // per car
    std::vector<int> sections;
    bool active=true;
};
// WBumSteer (dr_t2; vtable 0x515bd0, init 0x4cec40, update 0x4cede0, hit 0x4ceba0):
// two gates (children 0, 1) and two runners (2, 3) on GDPathQs (4, 5). State 0: the
// runners go along their paths at 100 an update (0x4be7f0, chord 2000, flags 0xe;
// 0x510414), squashing cars (mask 0x100); past the end, state 1. States 1, 2, 3 last
// m_88 updates each; 2 -> 3 clears the hit bits, 3 -> 0 restarts. A gate's angle
// follows 75 degrees (biquad m_90, m_98) in states 1 and 2 or once hit, else 0; it
// turns about z, the first gate negatively, folded to at most 90 degrees. A car
// touching a gate (0x46a786) sets its bit, is squashed, and bounces.
struct BumSteer {
    uint32_t node=0;
    Matrix4 parent=identity_matrix();
    std::array<uint32_t,2> gates{},runners{};
    std::array<Matrix4,2> gate_home{{identity_matrix(),identity_matrix()}},gate_frame{{identity_matrix(),identity_matrix()}},runner_frame{{identity_matrix(),identity_matrix()}},runner_home{{identity_matrix(),identity_matrix()}};
    std::array<std::vector<Vec3>,2> points,directions;
    std::array<std::vector<float>,2> lengths;
    std::array<std::vector<ScenePrim>,2> gate_prims,runner_prims;
    std::array<float,5> filter{};
    std::array<float,2> s0{},s1{};
    int reload=0,state=0,timer=0;          // m_88, +0x11e, +0x122
    float distance=0;                      // +0x256
    unsigned bits=0;                       // +0x25a
    std::vector<int> sections;
    bool active=false;
};
// WSaw (sh_t2; action on a model, vtable 0x5150b8, init 0x4c0890, update 0x4c0a90, end
// 0x4c0960). The owner's parent spins about axis m_24 by m_2c an update; the owner
// slides along axis m_34 in a triangle wave of amplitude m_3c, step m_44
// (0x4c0a40). The first car of its section within m_4c becomes its target, and the
// wave follows that car's coordinate. A robot passing on the near side of it steers
// aside: +0x1c7 7 or 8 by the wave's and the car's zones (167, -167), for 3 updates
// (+0x1cb; 0x4e75c0). Within m_54 + 200 on x and 100 on z, the car is held for m_64
// updates, the spin growing (m_64 - timer) x m_5c / m_64, then thrown from the saw at
// m_4c (decay 0.3), and 15 updates pass.
struct Saw {
    uint32_t node=0,spinner=0;
    Matrix4 world=identity_matrix();          // the spinner's parent
    Matrix4 spinner_home=identity_matrix(),owner_home=identity_matrix();  // +0x24, +0x58
    Matrix4 spinner_frame=identity_matrix(),owner_frame=identity_matrix();
    int spin_axis=0,slide_axis=0,range=0,hold=0;       // m_24, m_34, m_4c, m_64
    float spin_step=0,amplitude=0,slide_step=0,reach=0,spin_gain=0;  // m_2c, m_3c, m_44, m_54, m_5c
    float angle=0,phase=0;                    // +0x8c, +0xb1
    int state=0,timer=0,target=-1;            // +0xbd, +0xad, +0x90
    Vec3 target_last{},held_at{};             // +0x95, +0xa1
    std::vector<int> sections;
    bool active=false;
    Matrix4 owner_world() const;
    // The owner's motion from its home, for what the spinner's model holds of it.
    Matrix4 owner_motion() const;
};
// WJunkyard (dr_t3; vtable 0x515b48, init 0x4ce1d0, update 0x4ce430): a crane of three
// nested arms, each following a target through a biquad. Arm 0 turns about y to
// a random -150..30 degrees every m_a0 updates (+0x277); arm 1 about z, to 5 then -55
// degrees, m_b8 updates each (+0x27b); arm 2's target stays 0. The third arm's
// primitive (+0x12f's PRIM, a GDPrimSphere) squashes cars (0x4ce65b).
struct Crane {
    uint32_t node=0;
    Matrix4 parent=identity_matrix();
    std::array<uint32_t,3> arms{};
    std::array<Matrix4,3> home{{identity_matrix(),identity_matrix(),identity_matrix()}},frame{{identity_matrix(),identity_matrix(),identity_matrix()}};
    std::array<std::array<float,5>,3> filter{};
    std::array<float,3> s0{},s1{},target{};
    int rest=0,move=0,phase=0,wait=0,move_left=0;   // m_a0, m_b8, +0x122, +0x277, +0x27b
    std::vector<int> sections;
    std::vector<ScenePrim> prims;   // the third arm's PRIM (local)
    bool active=false;
    std::array<Matrix4,3> worlds() const;
};
// WConstructionSite (dr_t3; vtable 0x515db0, init 0x4d1080, update 0x4d11a0): two belts,
// each the unit vector from a child to its first child x m_88. A car of its section
// whose last contact (+0x8a3) has flag bit 26 + belt moves by that vector.
struct Conveyor {
    uint32_t node=0;
    std::array<Vec3,2> push{};
    std::vector<int> sections;
    bool active=true;
};
// WShiftedRoad (sh_t4; vtable 0x515900, init 0x4cb900, update 0x4cbb60): three road slabs
// (children 0..2) and three signs (3..5). Every m_88 updates one slab is drawn
// (+0x142): it swings to 90 degrees about x (biquad m_90, m_98) and the slab before
// it swings back 30 updates later. Route node 1's links towards nodes 0x20, 0x23, 0x28
// weigh 0xffff for that slab and 0 otherwise. A car touching a slab is squashed and
// makes it the chosen one. The signs take their homes in the slab's order.
struct ShiftedRoad {
    uint32_t node=0;
    Matrix4 parent=identity_matrix();
    std::array<uint32_t,6> kids{};
    std::array<Matrix4,6> home,frame;
    std::array<std::vector<ScenePrim>,3> prims;
    std::array<float,5> filter{};
    std::array<float,3> s0{},s1{};
    int reload=0,timer=0,swing=0,chosen=-1,previous=-1;   // m_88, +0x11e, +0x122, +0x142, +0x146
    int section=-1;uint32_t polygon=0;
    std::array<int,3> link{{2,2,2}};          // +0x172: each slab's link (2 and up: none)
    int routed=-1;                            // the slab the weights were last written for
    Vec3 route_at{};
    std::vector<int> sections;
    bool active=false;
};
// WGoldenGate (sh_t4; vtable 0x51549c, init 0x4c6610, update 0x4c6b10, end 0x4c6750, hit
// 0x4c67d0, choice 0x4c69b0, table 0x52d7c8): nine swinging doors (children 0..8) and a marker
// (child 9). Every m_88 updates one door among those that can move (a random one by
// 0x4c69b0, which reads its kind and neighbours' states) is chosen, and m_88 - 16
// updates later (at once with m_88 <= 16) it heads for its open (state 1: 90 x 5 degrees)
// or shut (state 2: 54 x 5) angle through a biquad; the marker stands 170 above it. A car
// hitting a door opens it (state 4, 72 x 5) from the right side, stops, and is pushed by
// the normal x m_a8 with 10 up (decay m_b0); the slot returns 1, so no bounce. A robot
// without a gadget effect within 3800 of the brick gets +0x1c7 = 1.
struct GoldenDoor {
    uint32_t node=0;
    int kind=0,state=1;                    // table +0, +4
    int open=90,shut=54,hit=72;            // table +1, +2, +3
    float target=0,s0=0,s1=0;              // +0x11, +9, +0xd
    Matrix4 home=identity_matrix(),frame=identity_matrix();
    std::vector<ScenePrim> prims;
};
struct GoldenGate {
    uint32_t node=0,marker=0;
    Matrix4 parent=identity_matrix(),marker_home=identity_matrix(),marker_frame=identity_matrix();
    Vec3 centre{};
    std::array<GoldenDoor,9> doors;
    std::array<float,5> filter{};
    int reload=0,timer=0,chosen=0,choice=0;   // m_88, +0x11e, +0x13d, +0x13e
    float force=0,decay=0;                    // m_a8, m_b0
    std::vector<int> sections;
    bool active=false;
};
// WDicksTrick2 (cc_t3; vtable 0x515580, init 0x4c7cd0, update 0x4c7fe0, choice 0x4c7e00, end
// 0x4c7f40): three road pieces of which one is drawn (rand % 3): the others take flag
// 0x100 (not drawn, out of the queries) and sink 1000. Picked again once a car has
// come within m_88 and none is left within m_90. Route node 0x1c's first link, when
// it leads to node 0x1d, weighs 0 for piece 0 and 0xffff otherwise.
struct DicksRoad {
    uint32_t node=0;
    Vec3 centre{};
    std::array<uint32_t,3> pieces{};
    std::array<Matrix4,3> world{{identity_matrix(),identity_matrix(),identity_matrix()}};
    std::array<std::vector<std::array<Vec3,3>>,3> triangles;
    int near=0,far=0,chosen=0;             // m_88, m_90, +0x11e
    bool started=false,triggered=false;    // +0x157
    int section=-1;uint32_t polygon=0;bool fork=false;  // +0x152, +0x156
    std::vector<int> sections;
    bool active=false;
    int weight() const {return fork && chosen==0?0:0xffff;}
};
struct CarHeld {size_t car=0;Vec3 position{};float yaw=0;};  // 0x4cfa50: stopped and placed
struct CarSignal {size_t car=0;int effect=0;};
// WRot on a course model (0x497b00; type 2: unbounded, wrapped): a spin that only
// shows (no primitive on dr_t2's and sh_t2's).
struct ModelSpin {
    uint32_t node=0;
    Matrix4 world=identity_matrix();
    std::array<float,3> step{},angle{};
    std::vector<int> sections;
    bool active=true;
    Matrix4 frame() const;
};
// WBlowUp (action on a WLogBase or WLogModelQ; record 0x514fb0, init 0x4bf920,
// update 0x4bfb30, end 0x4bfc20). Init clears the owner's primitives' solid
// bit 0x10000 and sends each child model off from its saved position p, with
// its frame reset: velocity p / 13 (0x513010), y + 20 (0x51152c), y at least
// 20. For 200 updates a piece scales by 0.96, moves, turns pi/36 about x
// (0x43b2b0) and loses 2.78 (0x51500c) of y velocity. The end puts the pieces
// back, with identity axes, and sets the bit again.
struct BlowUp {
    struct Piece {uint32_t node=0;Matrix4 local=identity_matrix();Vec3 home{},velocity{};};
    std::vector<Piece> pieces;
    int left=0;bool on=false;   // +0x2c, flag 2
    void start();
    void step();
    void finish();
};
// WChiselCrossover (cc_t4; record 0x515c9c, init 0x4d0020, update 0x4d0290,
// hit slot +0x34 0x4d01f0): a rock wall, a WLogBase with a WBlowUp. Its first
// activation (one shot +0x12f) rewrites route node m_node's two first links:
// bit 1 / 2 when link 0 leads to node m_node + 1 / + 2, 4 / 8 for link 1. While
// whole, a link with its bits weighs 0x8000 and the other 0; once blown 0 and
// 0xffff. Whole, the nearest car within 3000 gets +0x1c7 = 3 (fire). It stays
// blown for the race: nothing ends its WBlowUp.
struct ChiselCrossover {
    uint32_t node=0,log=0;
    Vec3 centre{},solid_centre{};
    float solid_radius=0;
    int route_node=-1,section=-1;uint32_t polygon=0;uint32_t bits=0;
    bool started=false,blown=false;
    Matrix4 log_world=identity_matrix();
    BlowUp blow;
    std::vector<int> sections;
    bool active=false;
    int link_weight(int link) const {const bool ours=(bits&(link==0?3u:0xcu))!=0;return blown?(ours?0:0xffff):(ours?0x8000:0);}
};
// WRolling (cc_t4, ct_t2, dr_t4; record 0x5154e4, init 0x4c7680, update
// 0x4c77b0, end 0x4c7710, hit slot +0x34 0x4c7730) and its WLogModelQ boulder
// (+0x11e; start 0x4c4f60, frame 0x4c4ea0, update 0x4c5030 in state 1). States
// +0x152: 0 at rest, 1 rolling, 2 blown. Activation restarts it rolling from
// the path's start. At rest with m_loop it restarts at once; otherwise a car
// within m_range of the brick sets it rolling. A rolling or blown one goes home
// with no car within 20000 and its 60-update timer run out (a blown one only
// once its WBlowUp has ended). The nearest car in those ranges gets +0x1c7 = 3.
// Rolling: v = (v - m_gravity x segment direction y) x m_drag; d += v; the
// turn grows by v / m_radius (wrapped to 0..2 pi). Past the path's end it rests.
// After 2 updates (+0x172) it squashes a car touching its primitive (0x4a60c0,
// mask 0x100; type 0x200).
struct RollingRock {
    uint32_t node=0,rock=0;
    Matrix4 parent=identity_matrix();      // the brick's world frame
    std::vector<Vec3> points,directions;   // +0x136 in the brick's space, +0x13a
    std::vector<float> lengths;            // +0x13e
    Vec3 centre{};
    float chord=0,radius=1,gravity=0,drag=1;int range=0;bool loop=false;  // m_88, m_90, m_98, m_a0, m_b8, m_c0
    std::vector<ScenePrim> prims;
    int state=0,timer=0,grace=0;           // +0x152, +0x12f, +0x172
    float velocity=0,distance=0,spin=0;int segment=0;  // +0x15a, +0x15e, +0x166, +0x162
    uint32_t flags=0xb;                    // +0x176
    Matrix4 frame=identity_matrix();       // in the brick's space
    BlowUp blow;
    std::vector<int> sections;
    bool active=false;
    Matrix4 world() const;
};
// 0x4be7f0: the frame at distance d along a path: the midpoint of the points
// at d and d + chord; flags low bits name the axis along the chord (3: z) and
// the next two bits the other named axis (2: y). Bit 0x10 takes the direction
// from the path's first point to its last. True past either end.
bool path_frame(const std::vector<Vec3>& points,const std::vector<Vec3>& directions,const std::vector<float>& lengths,float chord,float& d,int& segment,uint32_t flags,Matrix4& out);
// WLogJam (sh_t3; vtable 0x5152fc, init 0x4c4840, update 0x4c4940): six WLogModel
// logs (vtable 0x515278, update 0x4c4490, step 0x4c3720, frame 0x4c3350) and five
// GDPaths. Five logs wait at n - 3 of their paths (state 0); the sixth rolls down a
// random one (state 1, restart 0x4c48d0). At n - 4, within m_radius of n - 3, it
// hands its speed to the log waiting on that path (state 3) and keeps a tenth
// (state 2) until within 20 of n - 3, where it waits. A log within 50 of its path's
// end (state 5) restarts. Moving logs squash cars (0x4c43c0, mask 0x100); all have a
// solid vertical cylinder from their end (0x10000), and their hit slot refuses all
// (0x411640).
struct JamPath {std::vector<Vec3> points;Vec3 translation{};};
struct JamLog {
    uint32_t node=0;
    int path=0,segment=0,state=0;          // +0x152, +0x17a, +0x17e
    float along=0,y=0,vh=0,vy=0;           // +0x172, +0x176, +0x16a, +0x16e
    float spin=0,yaw=0,target=0;           // +0x182, +0x186, +0x18e
    bool turn_up=false,air=false;          // +0x196, +0x19b
    Vec3 position{};                       // +0x19c
    Matrix4 frame=identity_matrix();       // in the brick's space
    Vec3 prim_centre{};float prim_radius=0,prim_height=0;
};
struct LogJam {
    uint32_t node=0;
    Matrix4 parent=identity_matrix();
    float radius=1,gravity=0,drag=1,bounce=1,max_turn=0;   // m_90, m_98, m_a0, m_a8, m_b0
    std::vector<JamPath> paths;
    std::vector<JamLog> logs;
    std::vector<int> sections;
    bool active=false;
};
struct SolidSphere {Vec3 centre{};float radius=0;int rock=-1;bool cylinder=false;float height=0;int bump=-1,gate=-1;int golden=-1,door=-1;};
struct RouteWeight {int section=-1;uint32_t polygon=0;int link=0,weight=0;};
struct CarProbe {Vec3 position{};bool robot=false,hidden=false;int bridge=-1;Vec3 forward{};int section=-1;bool busy=false;bool gadget_idle=true;uint32_t surface=0;};  // hidden: flag 0x100, out of the queries; busy: WackyAutoBomba running
struct ObjectHit {size_t car=0;};
class CourseObjects {
public:
    void load(const Scene& scene);
    // The logs' meshes from the course archive (after load).
    void load_log_meshes(const Archive& course,const Scene& scene);
    // The highest log surface between top and bottom at (x, z).
    LogHit log_ground(float x,float z,float top,float bottom) const;
    // 0x4cc69e: a car standing on a log moves with it.
    Vec3 log_motion(int ride,int log) const;
    // The WMOD route rows (sections) linking each object's node.
    void link(const std::function<std::vector<int>(uint32_t)>& sections_of);
    // One update; cars[i] are the racers' positions. hits lists cars a tram
    // touched (0x4a60c0, mask 0x100); effect[i] is each car's +0x1c7, which
    // a crossing sets to 5 (wait) and back to 0. An object runs while
    // one of its sections is player_section or next to it (of sections, a
    // loop).
    void update(const std::vector<CarProbe>& cars,std::vector<ObjectHit>& hits,std::vector<int>& effect,int player_section=-1,size_t sections=0);
    const std::vector<TramCrossing>& crossings() const {return crossings_;}
    const std::vector<PathMover>& movers() const {return movers_;}
    const std::vector<LogRide>& log_rides() const {return log_rides_;}
    const std::vector<MistyBridge>& bridges() const {return bridges_;}
    const std::vector<Geyser>& geysers() const {return geysers_;}
    // The cars the last update's geysers hold (0x4c92cb..0x4c9505).
    const std::vector<GeyserLift>& lifts() const {return lifts_;}
    const std::vector<RollingLog>& rolling_logs() const {return rolling_logs_;}
    const std::vector<CarPlacement>& rolled() const {return rolled_;}
    const std::vector<DynamiteHole>& dynamite_holes() const {return holes_;}
    const std::vector<CarPush>& pushes() const {return pushes_;}
    const std::vector<ChiselCrossover>& chisels() const {return chisels_;}
    std::vector<ChiselCrossover>& chisels() {return chisels_;}
    const std::vector<RollingRock>& rocks() const {return rocks_;}
    const std::vector<LogJam>& log_jams() const {return log_jams_;}
    const std::vector<Magnet>& magnets() const {return magnets_;}
    const std::vector<DicksRoad>& dicks_roads() const {return dicks_roads_;}
    std::vector<DicksRoad>& dicks_roads() {return dicks_roads_;}
    const std::vector<GoldenGate>& golden_gates() const {return golden_gates_;}
    // 0x4c67d0: a car at a door, with the contact's normal.
    void door_touched(int gate,int door,const Vec3& normal);
    const std::vector<Crane>& cranes() const {return cranes_;}
    const std::vector<Conveyor>& conveyors() const {return conveyors_;}
    const std::vector<ShiftedRoad>& shifted_roads() const {return shifted_roads_;}
    std::vector<ShiftedRoad>& shifted_roads() {return shifted_roads_;}
    const std::vector<Saw>& saws() const {return saws_;}
    const std::vector<BumSteer>& bum_steers() const {return bum_steers_;}
    // 0x4ceba0: a car at a gate sets its hit bit.
    void gate_touched(int bump,int gate) {if(bump>=0 && size_t(bump)<bum_steers_.size()) bum_steers_[size_t(bump)].bits|=1u<<gate;}
    const std::vector<RotoHill>& roto_hills() const {return roto_hills_;}
    const std::vector<ModelSpin>& model_spins() const {return model_spins_;}
    const std::vector<CarHeld>& held() const {return held_;}
    // Cars whose ground rays have no reach below (+0x93b = 0).
    const std::vector<bool>& hovering() const {return hovering_;}
    // The primitives with the solid bit 0x10000 that a blow can clear.
    std::vector<SolidSphere> solid_spheres() const;
    // 0x46a786: a car against one of them only restarts a rock's timer.
    void car_touched(int rock) {if(rock>=0 && size_t(rock)<rocks_.size()) rocks_[size_t(rock)].timer=60;}
    // 0x475926 (mask 0x10000): a projectile at this sphere blows up what it
    // touches (slot +0x34). True if something blew up or was already hit.
    bool projectile_hit(const Vec3& position,float radius);
    // 0x4d0143..: the route links the chisels have written.
    std::vector<RouteWeight> route_weights() const;
    std::vector<MistyBridge>& bridges() {return bridges_;}
private:
    std::vector<LogRide> log_rides_;
    std::vector<MistyBridge> bridges_;
    std::vector<Geyser> geysers_;
    std::vector<GeyserLift> lifts_;
    std::vector<RollingLog> rolling_logs_;
    std::vector<CarPlacement> rolled_;
    std::vector<DynamiteHole> holes_;
    std::vector<CarPush> pushes_;
    std::vector<ChiselCrossover> chisels_;
    std::vector<RollingRock> rocks_;
    std::vector<LogJam> log_jams_;
    std::vector<Magnet> magnets_;
    std::vector<DicksRoad> dicks_roads_;
    std::vector<GoldenGate> golden_gates_;
    std::vector<Crane> cranes_;
    std::vector<Conveyor> conveyors_;
    std::vector<ShiftedRoad> shifted_roads_;
    std::vector<Saw> saws_;
    std::vector<BumSteer> bum_steers_;
    std::vector<RotoHill> roto_hills_;
    std::vector<ModelSpin> model_spins_;
    std::vector<CarHeld> held_;
    std::vector<bool> hovering_;
    uint32_t geyser_random_=1; // provisional: the original draws from rand()
    std::vector<TramCrossing> crossings_;
    std::vector<PathMover> movers_;
    std::vector<std::vector<bool>> inside_;  // per crossing, per car: +499 bit 2
};
}
#endif
