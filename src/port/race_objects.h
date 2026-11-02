#ifndef WPC_RACE_OBJECTS_H
#define WPC_RACE_OBJECTS_H
#include "game_scene.h"
#include "game_transform.h"
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
// m_length, along that chord; 0x43b4a0 turns it by pi (provisional axis
// choice: z along the chord, then the half turn about y). Past the path's
// end a log keeps its frame. Each log's motion this update (kept when
// below 2 x m_vel per axis, 0x2aaf9c8) moves the cars whose ground object
// +0x89f is that log (0x4cc69e).
struct LogRide {
    uint32_t node=0;
    std::vector<Vec3> points;              // the GDPathQ's points (world)
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
struct LogHit {bool hit=false;float y=0;Vec3 normal{};int ride=-1,log=-1;};
// 0x4be7f0's point at distance d along the polyline (false past the end).
bool path_distance_point(const std::vector<Vec3>& points,float d,Vec3& at);

struct CarProbe {Vec3 position{};bool robot=false,hidden=false;};  // hidden: flag 0x100, out of the queries
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
private:
    std::vector<LogRide> log_rides_;
    std::vector<TramCrossing> crossings_;
    std::vector<PathMover> movers_;
    std::vector<std::vector<bool>> inside_;  // per crossing, per car: +499 bit 2
};
}
#endif
