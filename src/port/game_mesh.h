#ifndef WPC_GAME_MESH_H
#define WPC_GAME_MESH_H

#include "game_assets.h"
#include "game_material.h"
#include <array>

namespace wpc {
// r, g, b: VCOL (0x43dd67, kept unless every vertex sums to <= 0), else 1.
struct MeshVertex { float x, y, z, u, v, r=1, g=1, b=1; };
struct MeshTriangle { uint32_t a, b, c, material; }; // material is a GMAT ID
// A GHMS SEGS row (0x436c30): parent (UINT32_MAX for a root) and the
// serialized local translation; segments are stored parents first.
struct MeshSegment { uint32_t parent; float x, y, z; };
struct Mesh {
    std::vector<MeshVertex> vertices;
    std::vector<MeshTriangle> triangles;
    // Resource-local affine matrix, preserved separately from the vertices.
    std::array<float, 12> matrix{{1,0,0, 0,1,0, 0,0,1, 0,0,0}};
    uint32_t matrix_flags = 0;
    size_t uninitialized_uv_vertices = 0;
    bool vertex_colors = false; // VCOL kept (mesh +0x30 non-null)
    // GHMS only: the segment table, and per output vertex its segment and
    // unposed VERT position, so motion can re-pose it (game_motion).
    std::vector<MeshSegment> segments;
    std::vector<uint32_t> vertex_segment;
    std::vector<std::array<float,3>> rest;
};

// GHMS applies the recovered static SEGS pose and HMTS vertex remapping.
// It does not apply MTNL motion (pose_mesh in game_motion) or a scene node's
// transform.
Mesh load_mesh(const Archive& archive, const std::string& tag, uint32_t id);
Mesh load_mesh(const ArchivePool& archives, const std::string& tag, uint32_t id);
} // namespace wpc
#endif
