#ifndef WPC_RACE_ROBOT_H
#define WPC_RACE_ROBOT_H
#include "game_random.h"
#include "race_track.h"

namespace wpc {
// WCtrlRoboCar (the action on every WackyAuto template, update 0x4e4f10,
// init 0x4e45e0). Its serialized members, read through +0x18 with a 4-byte
// base: m_rDirModifier (+0x24), m_aFWAdjustAngle (+0x34),
// m_aFWAheadAgainAngle (+0x3c), m_aReversingAngle (+0x44). 0x4e45e0 then
// sets m_aMinSlowAngle (+0x8c) to 0.349 and m_rMinSpeed (+0x94) to 0.55.
struct RoboParams {
    float dir_modifier=0.9f,fw_adjust=6.2831855f,fw_ahead_again=0.7853982f,reversing=2.0943952f;
    float min_slow_angle=0.3490658f,min_speed=0.55f;
};
// The control the robot leaves in the WackyAuto: +0x2ac throttle (0 none,
// 1 forward, 2 back) and +0x2ad steering, which the robot branch
// (m_isrobotauto +0xc0) copies into +0x403 (0x467886) before the 0.8 decay.
struct RoboControl {int throttle=1;float steer=0;};
// 0x468890: a car's road edges in its module (+0x2c5) towards the next
// polygon (+0x2c9). The module's two vertices the next polygon does not
// share are the trailing edge (+0x2fd, +0x309), the two shared ones the
// leading edge (+0x315, +0x321), ordered so that +0x2fd/+0x315 run along one
// side ("left") and +0x309/+0x321 along the other. +0x32d and +0x339 are
// the sides' unit directions, +0x345 and +0x351 the offsets 300 along the
// route and 600 to the left or right. None (+0x35d = -2) unless exactly two
// vertices are not shared.
struct RoadEdges {
    bool hit=false;
    Vec3 left{},right{},left_dir{},right_dir{},to_left{},to_right{};
    // +0x35d, +0x361: |dx| + |dz| / 2 (the larger first) from p to the
    // left and right sides' lines, truncated.
    int left_distance(const Vec3& p) const;
    int right_distance(const Vec3& p) const;
};
RoadEdges road_edges(const RaceTrack& track,const RaceTrack::Route& module,const RaceTrack::Route& next);
// Another car as 0x4e69bd.. reads it: its slot (0x6f2ce0[slot], the
// vehicle), position, the position before its own update (+0x173),
// heading, module (+0x2c5) and next polygon (+0x2c9).
struct OtherCar {
    int slot=0;
    Vec3 position{},previous{},forward{};
    RaceTrack::Route module{},next{};
};
// What the robot reads of its car each update.
struct RoboCarState {
    Vec3 position{},forward{},previous{};  // +0x173: position before the car's update
    float speed=0,top_speed=0;
    uint32_t surface=0;                    // +0x8bb, the centre contact's flags
    int probe_span=204;                    // +0x4c: |front - rear movement probe|
    // +0xd0: up to four power-ups still to take, for 0x4e4a10.
    std::array<Vec3,4> boxes{};int box_count=0;
    int slot=0,progress=0;                 // 0x6f2ce0 slot, +0x1a7
    int effect=0;                          // +0x1c7 (5: waiting at a WTram crossing)
    std::array<OtherCar,8> others{};int other_count=0;
};
// +0x2c9 for a car without WCtrlRoboCar (0x468890): the last forward link
// of its module with a target and a weight.
RaceTrack::Route next_polygon(const RaceTrack& track,const RaceTrack::Route& module);
// 0x4e4a10: with the speed >= 0, s turns toward the best power-up: within
// 3000 (integer squares below 0x895441), within 20 degrees of the heading,
// and close to s by distance (80 degrees within 500, 60 within 1000, 40
// within 2000, else 30), the one whose angle is nearest s.
void pickup_turn(float& s,const RoboCarState& car);
class RoboDriver {
public:
    explicit RoboDriver(const RoboParams& params={}):params_(params) {}
    // Near the player: the route-following driver (0x4e5d..0x4e82cd).
    RoboControl drive(const RaceTrack& track,const RoboCarState& car,MsvcRandom& random);
    // Updates the goals when the module changes (0x4e9750); false off-route.
    bool update_goals(const RaceTrack& track,const RaceTrack::Route& module,MsvcRandom& random);
    // Steering angle a (heading minus the direction to the goals) for the
    // far mode (0x1000), which turns the car by a / 2 and slides it along.
    float far_turn(const Vec3& position,const Vec3& forward) const;
    // m_rDirModifier, which the rank scaling rewrites (0x52e62c/0x52e630).
    void set_dir_modifier(float k) {params_.dir_modifier=k;}
    float dir_modifier() const {return params_.dir_modifier;}
    int state() const {return state_;}
    int stuck() const {return stuck_;}
    const Vec3& goal() const {return goal_;}
    const Vec3& goal2() const {return goal2_;}
    bool has_goal() const {return goals_known_;}
    const RaceTrack::Route& module() const {return module_;}
    const RaceTrack::Route& next_module() const {return next_;}
    // The last avoidance (0x4e69bd..0x4e72ea): side +0xc4 (1 right, -1
    // left, 0 none), the aim beside the car ahead.
    int avoid_side() const {return avoid_side_;}
    bool avoiding() const {return avoiding_;}
    const Vec3& avoid_point() const {return avoid_;}
private:
    bool avoid(const RaceTrack& track,const RoboCarState& car);
    RoboParams params_;
    int state_=1;              // WackyAuto +0x2b1
    int stuck_=0;              // +0xa8: slow updates, then the unstick counter (low 16 bits) and side (bit 16)
    RaceTrack::Route module_{},next_{};   // +0x2c5 and +0x2c9
    int link_=-1,next_link_=-1;           // +0x50 in the module, +0x54 in the next polygon
    Vec3 goal_{},goal2_{},edge_{};        // +0x58, +0x64, +0x70
    uint32_t module_flags_=0;
    bool dead_end_=false;                 // first forward link target 0xffff
    bool goals_known_=false;
    uint32_t turn_module_=0,turned_module_=0; // +0xbc, +0xc0
    bool turn_known_=false,turned_known_=false;
    int avoid_side_=0;                    // +0xc4
    uint32_t follow_mask_=0;              // +0xc8: cars kept up with, by slot
    bool avoiding_=false;Vec3 avoid_{};
};
// The rank-based speed scaling (0x4e4d30) for a course: 0x483e06 copies
// 0x52b910[course + environment * 4] to 0x6f2cc8..0x6f2cdc.
struct RankRow {int lead=2000,gap=6000,close=10000;float boost=0.2f,far_catch_up=1.5f,far_hold=0.9f;
    float prefer=0.4f; // 0x52b8bc -> 0x6f2c48: the chance a robot aims only at the player
};
RankRow rank_row(int environment,int course);
// What 0x4e4d30 reads of a robot and the race: its rank (+0x1af, 1 first),
// lap (+0x187), progress (+0x1a7), the first player's progress and that of
// the robot ranked just ahead (none when it is the player).
struct RankView {int rank=1,lap=1,progress=0,player=0;bool has_ahead=false;int ahead=0;};
// 1 to catch up, -1 to hold back, else 0. Ranks above 4 (above 5 on course
// variant 2; never on 3 or in environment 4) only hold back, within close
// of the car ahead. The others catch up while short of the player + lead
// and more than gap behind the car ahead, and hold back within close of
// the player and within gap of the car ahead. No hold before lap 2.
int rank_scale(const RankView& v,const RankRow& r,int environment,int course);
// The environment (cc, sh, dt, ct, dr) and variant (_t1 = 0) of a course file.
bool course_indices(const std::string& file,int& environment,int& course);

// 0x4e9610 with random choice: the current link when it belongs to the
// node, else a forward link by cumulative weight against rand() x 2 - 1,
// skipping targets whose route flags have bits 0xff0000.
int choose_route_link(const RaceTrack& track,int section,uint32_t record,const RouteNode& node,MsvcRandom& random);
}
#endif
