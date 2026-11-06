#ifndef WPC_RACE_TRACK_H
#define WPC_RACE_TRACK_H
#include "game_ground.h"
#include "game_mode.h"
#include "race_objects.h"
#include "game_transform.h"
#include <array>
#include <unordered_map>
#include <vector>

namespace wpc {
using Vec3=std::array<float,3>;
struct TrackSurface {
    bool hit=false;
    float y=0;
    Vec3 point{};           // ray(): the hit point
    Vec3 normal{{0,1,0}};
    uint32_t flags=0;
    int section=-1;
};
// WBendSign linked from a WMOD route row: 0x1e0 radius, 0x1e1 arrow type
// (0..4, the HUD's FILM 66331 frame and mirror).
struct BendSign {
    Vec3 position{},forward{};
    float radius=0;
    int type=-1;
    uint32_t id=0;
};
// A WPowerUp pickup: 0x28b type (0 gadget, 1 weapon, 2 refill); its GDModel
// child, turned and scaled by the pickup state; and its PRIM GDPrimSphere
// (sensor group 0x8000) in world space: centre and radius 100 x node scale.
struct PowerUpSpot {
    uint32_t node=0,model=0;
    Vec3 position{},centre{};
    float radius=0;
    int type=0;
    std::vector<int> sections; // the WMOD rows linking it or its ancestor (none: always enabled)
};
// A WGndBrick's WPrimCylinder in world space: base centre, radius, height.
// Sensor group 0x40000 (cc_t1's cacti) makes the car clone WKBumm
// (0x46aa58); 0x10000 (rocks, start lines) are solid contacts (not ported).
struct TrackCylinder {
    Vec3 base{};
    float radius=0,height=0;
    uint32_t sensor_group=0,object=0;
};
struct TrackWall {
    bool hit=false;
    float fraction=1;
    Vec3 normal{{0,0,0}};
};
// A contact ray's first hit (WackyAuto's probe rays, mask 0x1e000ff): the
// normal facing the ray's start, made horizontal (provisional), and the
// record flags.
struct TrackContact {
    bool hit=false;
    float fraction=1;
    Vec3 normal{{0,0,0}};
    uint32_t flags=0;
};
// Course physics: one WGndBrick/WGNR pair per WMOD route definition, in
// route order. Each WGNR is queried in its brick's local frame (brick 71 of
// cc_t1 sits at y=448, so local ground 564 is the start line's y=1012).
class RaceTrack {
public:
    // battle: game mode 1 (app+0x1a60 = 1), set by the caller for a *_b.wgm.
    RaceTrack(const Archive& course,const Scene& scene,bool battle=false);
    // Vertical WGNR query (mask 0x018000ff, as the spawn writer uses):
    // the highest surface between top and bottom.
    TrackSurface ground(float x,float z,float top,float bottom) const;
    // The same query in one section's WGNR only.
    TrackSurface ground_in(int section,float x,float z,float top,float bottom) const;
    // The same mask along any segment: the hit nearest from (0x46c97e, the
    // robots' ray along the car's up axis).
    TrackSurface ray(const Vec3& from,const Vec3& to) const;
    // Movement-probe query (mask 0x01e000ff). Mostly-horizontal surfaces are
    // ground, handled by ground(); only steeper hits are reported as walls.
    TrackWall wall(const Vec3& from,const Vec3& to) const;
    // 0x46bdb1's probe ray: every surface of the mask, at any slope.
    TrackContact contact(const Vec3& from,const Vec3& to) const;
    // The route polygon under a point (0x47c180's query: from 403.7 above to
    // 1000 below, mask 0x818000ff, the secondary 0x80000000 layer), the
    // robots' module. Route points are in the section's frame (all route
    // records are record 0 with matrix 0 in Championship I).
    struct Route {bool hit=false;int section=-1;uint32_t record=0,polygon=0;float y=0;};
    // near >= 0: only that section and its neighbours (the route cursor).
    Route route(const Vec3& p,int near=-1) const;
    RouteNode route_node(int section,uint32_t record,uint32_t polygon) const;
    Vec3 route_point(int section,const GroundPoint& local) const;
    Vec3 route_vertex(int section,uint32_t record,uint16_t index) const;
    const Matrix4& start_line() const {return start_line_;}
    float start_radius() const {return start_radius_;} // WStartLine m_rRadius (0x1df)
    // A battle arena (*_b.wgm; sh_b alone has a WStartLine). 0x49eb70 mode 1: the
    // player takes the frame of the start row's +0x48 object moved 300
    // along its z axis; the opponent the +0x50 object's, its x and z axes
    // negated, moved -300 along the original z (0x512610, 0x5121e4,
    // 0x514bb8). start_line() is then the player's frame.
    bool battle() const {return battle_;}
    const Matrix4& battle_frame(int slot) const {return battle_frames_.at(size_t(slot));}
    // Rigid WMOD loop transform carrying the last route's exit portal onto
    // the first route's entry portal: a translation on cc_t1 (+73386,-0.13,
    // -31976) and ct_t1, a half turn about +Y on sh_t1, a quarter turn on dt_t1.
    // Zartter ("enclosed space", class 0xac11747a): an oriented box (node
    // frame, 0x8e..0x93 min/max) inside which the weather is not drawn
    // (0x4565e0 -> 0x456180 on the viewport camera). Planes from 0x45a270.
    bool sheltered(const Vec3& p) const;
    size_t shelters() const {return shelters_.size();}
    const Matrix4& wrap() const {return wrap_;}
    const Matrix4& unwrap() const {return unwrap_;}
    int laps() const {return laps_;}
    const std::vector<BendSign>& bend_signs(int section) const {return sections_.at(size_t(section)).signs;}
    const std::vector<PowerUpSpot>& power_ups() const {return power_ups_;}
    const std::vector<TrackCylinder>& cylinders() const {return cylinders_;}
    // The moving course objects at their initial state (dt_t1's trams).
    const CourseObjects& objects() const {return objects_;}
    size_t sections() const {return sections_.size();}
    // A section's route length (+0x34; provisional: the entry node's distance
    // in the section's distance field).
    float section_length(int section) const {return sections_.at(size_t(section)).entry_distance;}
    // A section's brick frame (local -> world); the original keeps car
    // positions (+0x288) in their section's local frame.
    const Matrix4& section_transform(int section) const {return sections_.at(size_t(section)).transform;}
    const Matrix4& section_inverse(int section) const {return sections_.at(size_t(section)).inverse;}
    // 0..1 inside a section: one minus the geodesic distance to the exit
    // portal over the section's own drivable WGNR surfaces (150-unit columns
    // inside the WMOD row's world bounds, one node per surface layer, a fixed
    // cost per wall crossed), divided by that distance from the entry portal.
    // A progress estimate from collision data, not the recovered WModManager
    // route cursor.
    float section_fraction(int section,const Vec3& p) const;
    // Unit horizontal direction of increasing progress at p (zero if unknown).
    Vec3 section_direction(int section,const Vec3& p) const;
    // False when the field could not join the portals; portal distances are used.
    bool progress_field(int section) const {return !sections_.at(size_t(section)).distance.empty();}
    const Vec3& entry(int section) const {return sections_.at(size_t(section)).entry;}
    // The entry WPortal's world frame (row +0x48) and the row's route length
    // (+0x34), for robots outside the player's rows (0x4a1c8b).
    const Matrix4& entry_frame(int section) const {return sections_.at(size_t(section)).entry_frame;}
    int row_length(int section) const {return sections_.at(size_t(section)).row_length;}
    const Vec3& exit(int section) const {return sections_.at(size_t(section)).exit;}
    // The WGNR record flags' low byte is a surface type, all inside the
    // 0x018000ff ground mask: cc_t1 uses 0x1 banks, 0x2 road/sand and 0x4 the
    // shallow water surface (it crosses the Snake road twice, so it must be
    // drivable); sh_t1 and dt_t1 add 0x10 and 0x40, ct_t1 0x20/0x40/0x80.
    // 0x800000 is cc_t1's and sh_t1's deep river bed. The Wine car vanished in
    // the splash after entering the deep river; the car is drowned there.
    static bool drowning_surface(uint32_t flags) {return (flags&0x00800000)!=0;}
private:
    struct Section {
        GroundResource ground;
        Matrix4 transform{},inverse{};
        Vec3 entry{},exit{};
        Matrix4 entry_frame{};int row_length=1;
        std::vector<BendSign> signs;
        // Progress field: nodes are the drivable surfaces of each 150-unit
        // column (first[c]..first[c+1]), with distances to the exit portal,
        // -1 where unreached; entry_distance is the entry node's.
        float x0=0,z0=0;int width=0,height=0;
        std::vector<uint32_t> first;
        std::vector<float> node_y,distance;
        float entry_distance=1;
    };
    void build_field(Section& s,size_t index,const Vec3& low,const Vec3& high);
    // Nearest reached node to p (within a few columns and 250 units of
    // height), or -1.
    long cell(const Section& s,const Vec3& p) const;
    std::vector<Section> sections_;
    std::unordered_map<uint32_t,std::vector<int>> links_;  // node id -> sections whose rows link it
    Matrix4 start_line_{};
    float start_radius_=5000;
    bool battle_=false;
    std::array<Matrix4,2> battle_frames_{};
    struct Shelter {Matrix4 frame{};std::array<float,6> box{};};
    std::vector<Shelter> shelters_;
    Matrix4 wrap_=identity_matrix(),unwrap_=identity_matrix();
    std::vector<PowerUpSpot> power_ups_;
    std::vector<TrackCylinder> cylinders_;
    CourseObjects objects_;
    int laps_=3;
};
}
#endif
