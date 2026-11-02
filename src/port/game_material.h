#ifndef WPC_GAME_MATERIAL_H
#define WPC_GAME_MATERIAL_H

#include "game_assets.h"
#include <array>

namespace wpc {
struct Material {
    bool textured = false;
    uint32_t texture_id = 0;
    std::array<float,3> diffuse{{1,1,1}};
    float transparency = 0;
    // Low two CLAM bits, passed independently to the original U/V clamp call.
    uint32_t clamp_axes = 0;
    bool two_sided = false;
};

using ArchivePool = std::vector<const Archive*>;

// Resolve exact (tag, full 32-bit ID) keys. High ID bits are resource groups;
// do not remove them when joining world and general archives. Missing and
// ambiguous keys are errors; caller order is not an implicit override policy.
const Archive& resolve_resource(const ArchivePool& archives,
                                const std::string& tag, uint32_t id);
Material parse_material(const Archive& archive, uint32_t id);
Material parse_material(const ArchivePool& archives, uint32_t id);
// Direct3D mesh cache 0x4b1f30 (0x4b21ae): a textured vertex's colour.
// TRAN 0: rgb = VCOL (white without VCOL), alpha 1. TRAN t != 0: rgb =
// t x VCOL (white without VCOL), alpha t; the texture alpha modulates it
// (0x4b3630). Not 1 - TRAN, which is the Glide updater 0x433d40. Values are
// clamped (provisional: the cache truncates with ftol, unclamped).
inline std::array<float,4> textured_vertex_color(float transparency,const float* vcol) {
    const auto c=[](float v){return v<0?0.f:v>1?1.f:v;};
    const float k=transparency==0?1.f:c(transparency);
    if(!vcol) return {{1,1,1,k}};
    return {{c(k*vcol[0]),c(k*vcol[1]),c(k*vcol[2]),k}};
}
} // namespace wpc
#endif
