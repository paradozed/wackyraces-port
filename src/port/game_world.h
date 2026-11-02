#ifndef WPC_GAME_WORLD_H
#define WPC_GAME_WORLD_H
#include "game_material.h"
#include "game_transform.h"
#include "wpc_platform.h"
#include <map>

namespace wpc {
struct WorldBatch {
    uint32_t node_id=0,material_id=0;
    Material material;
    std::vector<WpcTexturedVertex3D> vertices;
    // GD3dSprite/WGndSprite (0x419b90): turned each frame toward the camera;
    // offsets are (right, up) per vertex from the anchor. facing is property
    // 0x0a (+0x12a): 2 the camera's axes, otherwise (not 1) about the world up
    // axis toward the camera position; 1 sprites keep their frame.
    bool billboard=false;
    int facing=0;
    std::array<float,3> anchor{},up{};
    std::vector<std::array<float,2>> offsets;
    // 0x4b1f30: TRAN != 0 or a 24/32-bit texture (surface flag 8, 0x4b0550).
    bool translucent=false;
    // Draw flags +0x12a (default 0x80062, 0x43f1d0): GDModel::m_RenderSetting,
    // property 0x68; GD3dSprite/WGndSprite copy their m_RenderSetting (+0x15a,
    // property 0x10) into their quad model (0x419f0e). Names from the WGM's
    // CLAS/PRIM member table.
    uint32_t draw_flags=0x80062;
    // The model node's translation, the source of the queue key (0x4affe0).
    std::array<float,3> origin{};
    // Node flag 0x100 at runtime: not drawn.
    bool hidden=false;
};
// 0x4afcc2: translucent parts queue one entry per model, keyed by the
// view-space z of the model origin (ftol, compared unsigned). A new entry goes
// before the first one whose key is not larger, so each list runs far to near
// and ties draw the newest first. The flush 0x4b2d70 draws list +0x3f8
// (flags & 4), then +0x3f4, then +0x3fc (flags & 0x20000, checked last).
class TranslucentQueue {
public:
    void add(uint32_t draw_flags,float view_z,size_t item);
    template<class F> void flush(F&& draw) const {for(const auto& list:lists_) for(const Entry& e:list) draw(e.item);}
    void clear() {for(auto& list:lists_) list.clear();}
private:
    struct Entry {uint32_t key;size_t item;};
    std::array<std::vector<Entry>,3> lists_;
};
// 0x4b3630 per queued entry: draw flag 0x8000 blends ONE/ONE, 0x2000000
// turns depth writes off; both are restored after the entry.
inline uint32_t draw_mode(uint32_t draw_flags) {
    return ((draw_flags&0x8000)?WPC_DRAW_ADDITIVE:0u)|((draw_flags&0x2000000)?WPC_DRAW_NO_DEPTH_WRITE:0u);
}
// Groups a batch list's translucent batches by model (node), in batch order.
struct TranslucentModel {uint32_t draw_flags;std::array<float,3> origin;std::vector<size_t> batches;};
std::vector<TranslucentModel> translucent_models(const std::vector<WorldBatch>& batches);
// Rewrites billboard vertices for the camera's world matrix (column-major:
// right, up, forward, position), as 0x419d37 builds each sprite's matrix.
void face_billboards(std::vector<WorldBatch>& batches,const Matrix4& camera);
struct WorldData {
    std::vector<WorldBatch> batches;
    std::map<uint32_t,Image> textures;
    Matrix4 view_projection{};
    uint32_t camera_id=0;
    bool camera_from_viewport=false;
    bool camera_from_race=false;
    std::array<float,3> clear{{0,0,0}};
    size_t models=0,sprites=0,unsupported_sprites=0,vertex_count=0;
};
struct WorldInstance {
    const Node* root=nullptr;
    // Runtime root frame replaces its serialized transform. Descendant local
    // transforms and mesh MATR records still compose normally.
    Matrix4 transform=identity_matrix();
};
struct WorldSetup {
    std::vector<WorldInstance> instances;
    const Node* camera=nullptr;
    Matrix4 camera_transform=identity_matrix();
};
// Draw only placements in the course tree. General resources supply templates
// and dependencies; their entire scene is not a second world to draw.
WorldData load_world(const ArchivePool& archives,const Node& root,const Node& screen,
                     const WorldSetup& setup={});
// Models below root in root-local coordinates (root's own transform is
// excluded), for actors whose frame changes every update. No camera.
WorldData load_model_group(const ArchivePool& archives,const Node& root);
}
#endif
