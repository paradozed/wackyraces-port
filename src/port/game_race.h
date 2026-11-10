#ifndef WPC_GAME_RACE_H
#define WPC_GAME_RACE_H
#include "game_transform.h"
#include "game_ground.h"
#include <array>
#include <functional>
#include <vector>

namespace wpc {
struct VehicleTemplate {
    uint32_t actor_id=0,shadow_id=0;
    std::array<uint32_t,2> model_ids{};
};
struct RaceTemplates {
    std::array<VehicleTemplate,7> vehicles;
    std::array<uint32_t,2> camera_ids{},follow_action_ids{};
};
RaceTemplates load_race_templates(const Node& general_root);
// The first 27 WPowerUpSprites in tree order (the icons 0..26): m_range
// (0x284) and m_visangle (0x285); missing ones keep fallback.
struct SpriteAim {float range=0,angle=0;};
std::array<SpriteAim,27> load_sprite_aims(const Node& general_root,const std::array<SpriteAim,27>& fallback);

// 0x430e40/0x430f60. State is shared by successive roster initializations.
uint32_t race_random(uint32_t& state);
struct Racer {
    int vehicle=0;
    unsigned grid_slot=0;
    bool player=false;
};
// One-player branches of 0x49d580. Opponent ordering consumes the original
// random stream; this is not a fixed mapping from grid slot to vehicle enum.
std::vector<Racer> race_roster(int mode,int vehicle,uint32_t& random_state);
Matrix4 race_grid_frame(const Matrix4& start_line,unsigned slot);
// 0x477ea0 -> 0x477b60 initial pose, taking the actor's established follow
// frame (+0x8eb). It does not replace ongoing 0x478070 camera updates.
Matrix4 initial_follow_camera(const Matrix4& actor_follow_frame,bool two_players=false);
// 0x469710 at the initial zero-velocity, zero-angle-offset state: active model
// frame follows the actor with the initialized +0x92f vertical offset (20).
Matrix4 initial_vehicle_model_frame(const Matrix4& actor_frame);
struct PlacedRacer {
    Racer racer;
    Matrix4 candidate{},frame{};
    GroundQueryResult ground;
};
struct RaceGrid {
    std::vector<PlacedRacer> racers;
    bool ground_complete=true;
};
using SpawnGroundQuery=std::function<GroundQueryResult(GroundPoint,GroundPoint,uint32_t)>;
// Original 0x49eb70 start-line branch. An incomplete query leaves a candidate
// explicitly pending; it is not treated as the original no-hit branch.
RaceGrid place_race_grid(const std::vector<Racer>& roster,const Matrix4& marker,
                        const SpawnGroundQuery& query);
}
#endif
