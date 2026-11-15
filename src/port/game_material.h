#ifndef WPC_GAME_MATERIAL_H
#define WPC_GAME_MATERIAL_H

#include "game_assets.h"
#include <array>
#include <cmath>
#include <cstdint>

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
// (0x4b3630). Not 1 - TRAN, which is the Glide updater 0x433d40.
// The channels are ftol(k x VCOL x 255) (0x4b221a..0x4b2290, truncated, no
// clamp) packed by ORs (0x4b2297..0x4b22be): ((a << 8 | r) << 8 | g) << 8
// | b, so a channel past 255 spills into the one above and a negative one
// sets every bit above it (ct_t1's GMDL 361 carries VCOL up to +-3000). An
// all-zero colour gets alpha 0 (0x4b22a7; kept for TRAN != 0 only, where
// the alpha is used).
inline int32_t ftol32(double x) {   // 0x4ee040: truncation; out of range the x87 indefinite
    return std::isfinite(x) && x>-2147483648.0 && x<2147483648.0 ? int32_t(x) : INT32_MIN;
}
inline std::array<float,4> textured_vertex_color(float transparency,const float* vcol) {
    const float k=transparency==0?1.f:transparency;
    if(!vcol) {const float a=k<0?0.f:k>1?1.f:k;return {{1,1,1,a}};}
    const int32_t r=ftol32(double(k)*vcol[0]*255.0),g=ftol32(double(k)*vcol[1]*255.0),b=ftol32(double(k)*vcol[2]*255.0);
    int32_t a=transparency==0?255:ftol32(double(k)*255.0);
    if(!(r|g|b) && transparency!=0) a=0;
    const uint32_t u=(((uint32_t(a)<<8|uint32_t(r))<<8|uint32_t(g))<<8)|uint32_t(b);
    const float alpha=transparency==0?1.f:float(u>>24)/255.f;
    return {{float(u>>16&255)/255.f,float(u>>8&255)/255.f,float(u&255)/255.f,alpha}};
}
} // namespace wpc
#endif
