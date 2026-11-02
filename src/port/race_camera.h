#ifndef WPC_RACE_CAMERA_H
#define WPC_RACE_CAMERA_H
#include "game_transform.h"
#include <array>
#include <functional>

namespace wpc {
// WFollowCamera_2 (vtable 0x512d88). Its init 0x477ea0 overwrites every
// serialized offset and look member (one player: m_rOrigPos (0,300,-550),
// look (110,200); _2 (0,400,-900), (300,0); _Near (0,100,100), (100,200);
// two players: (0,100,-800), (150,200); (0,350,-900), (150,0)), the look
// heights x 0.5, m_rDistSpeed 0.2 and m_rS 0.6. It keeps these three
// (general.wgm #440/#443: 0.05, 400, 0).
struct FollowCameraMembers {
    float speed_rot_z=0.05f; // 0x14a m_rSpeedRotZ, brick +0xa4: the up vector's ease
    float near_radius=400;   // 0x14d m_rNearRadius, +0xbc
    float inertia=0;         // 0x14e m_rInertia, +0xc4: x the last eye motion
};
FollowCameraMembers follow_camera_members(const Node& action);
// The first WFollowCamera_2 action under general.wgm's root (both share them).
FollowCameraMembers race_camera_members(const Node& general_root);

struct CameraHit {bool hit=false;std::array<float,3> point{};};
// 0x4a4830 with mask 0x018000ff (the spawn query's): the first hit from a to b.
using CameraCast=std::function<CameraHit(const std::array<float,3>& a,const std::array<float,3>& b)>;

// The race state 0x478070 reads besides the follow frame.
struct FollowCameraState {
    bool view=false;      // input 0x8 pressed (0x52aad0: control 4, Return)
    bool look_back=false; // input 0x4 held (0x52aad4: control 5, '.')
    int intro=-1;         // manager +0xf34 in states 1/0xc (before the countdown), else -1
    bool reset=false;     // actor +0x8e7 (the drowning counter) nonzero: 0x477b60 again
    bool near_locked=false; // actor +0x8e3 (25 updates after a return) nonzero
    float speed=0,yaw=0;  // actor +0x3ef, +0x26b
    bool finished=false;  // brick +0xc0 nonzero (2 past the finish, 0x49eb20)
};

class FollowCamera {
public:
    // 0x477ea0 then 0x477b60 on the actor's follow frame +0x8eb.
    void init(const FollowCameraMembers& members,bool two_players,const Matrix4& frame);
    // 0x478070, once per race update.
    void update(const Matrix4& frame,const FollowCameraState& state,const CameraCast& cast);
    const Matrix4& matrix() const {return matrix_;}
    // The WMOD loop transform applied to the camera with its car (native).
    void carry(const Matrix4& m);
    // Flag 0x100: the eye came within m_rNearRadius; the near offset is used
    // and the car's parts take the camera in +0x24 (not drawn by it).
    bool near() const {return (flags_&0x100)!=0;}
    bool second_view() const {return (flags_&1)!=0;}
private:
    using V=std::array<float,3>;
    void pose(const Matrix4& frame);
    void select(int view);
    FollowCameraMembers m_{};
    std::array<V,3> offset_{};          // brick +0x2c, +0x54, +0x7c
    std::array<std::array<float,2>,3> look_{}; // +0x28/+0x38, +0x30/+0x40, +0x48/+0x50
    float dist_speed_=0.2f,s_=0.6f;     // brick +0xac, +0xb4
    V offset{},eye{},up{},vel{},prev{},intro_base{};  // +0x5c, +0x78, +0x84, +0x90, +0xa8, 0x6f1eb8
    float target_y=0,y=0,target_z=0,z=0; // +0x68, +0x6c, +0x70, +0x74
    uint32_t flags_=0;                   // +0x9c: 1 view _2, 0x10 look back, 0x100 near
    int cooldown_=0,near_updates_=0;     // +0xa0, +0xa4
    float last_yaw_=0;                   // +0xb4
    Matrix4 matrix_=identity_matrix();
};
}
#endif
