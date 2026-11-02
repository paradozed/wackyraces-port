#ifndef WPC_MENU_MODELS_H
#define WPC_MENU_MODELS_H

#include "game_mesh.h"
#include "game_motion.h"
#include "game_scene.h"
#include "wpc_platform.h"

#include <array>
#include <functional>
#include <map>

namespace wpc {
class MenuModels {
public:
    MenuModels(WpcWindow* window, const Archive& archive, const Scene& scene,
               std::function<WpcTexture*(uint32_t material)> material);
    // Resolve meshes/materials and upload callback textures outside a frame.
    void prepare(const Node& page);
    void draw(uint32_t page_id, size_t selected, int client_width, int client_height, double elapsed_seconds,
              float y_scale = 1, float y_offset = 0);
    // Regression summary of one item's preview at frame 0 (no spin): its
    // projected box in the 640x480 frame (NDC mapped to 0..640, 0..480),
    // vertex/batch counts and mean vertex colour. No window needed.
    struct Silhouette {
        size_t vertices = 0, batches = 0, translucent = 0;
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        std::array<float,4> color{};
    };
    Silhouette silhouette(uint32_t page_id, int item) const;

private:
    using Matrix = std::array<float,16>;
    struct Batch {
        int item = -1;
        WpcTexture* texture = nullptr;
        uint32_t clamp_axes = 0;
        float transparency = 0.f;
        bool two_sided = false;
        // 0x4b1f30: TRAN != 0, or a 24/32-bit texture (0x4b0550 gives those
        // the alpha format and flag 8). Drawn after the opaque geometry.
        bool translucent = false;
        int spin = -1;       // index into Page::spins, or -1 for static geometry
        int model = -1;      // the GDModel the batch came from, in scene order
        uint32_t flags = 0x80062; // draw flags +0x12a (TranslucentQueue picks the list)
        std::array<float,3> origin{}; // the model's world origin (sort key source)
        std::vector<WpcTexturedVertex3D> vertices;
    };
    // Recovered WRot (0x498170) continuous type 2: per 30 Hz menu frame, add
    // m_StepRotDeg{X,Y,Z} and rebuild the node rotation about its own origin.
    struct Spin {
        Matrix world;        // the WRot-owning node's world transform
        Matrix inverse;
        float step[3];       // radians per frame, X/Y/Z
    };
    // Debug only (WPC_MENU_MOTION=clip): a GDHModel playing one MTNL clip.
    // No menu code starts a clip in the original (see game_motion.h), so the
    // preview pose is the static SEGS pose; this shows the channel mapping.
    struct Corner { uint32_t batch, slot, vertex; };
    struct Animated {
        int item = -1;
        Mesh mesh;
        Matrix world;
        MotionController controller;
        size_t clip = 0;
        std::vector<Corner> corners;
    };
    struct Page { std::vector<Batch> batches; std::vector<Spin> spins; std::vector<Animated> animated; int models = 0; double frames = 0; };
    void animate(Page& page, double frames, size_t selected);
    void index_transforms(const Node& node, const Matrix& parent);
    void collect(Page& page, const Node& node, int item, int spin);

    WpcWindow* window_;
    const Archive& archive_;
    const Scene& scene_;
    std::function<WpcTexture*(uint32_t)> material_;
    std::map<uint32_t,Matrix> transforms_;
    std::map<std::pair<std::string,uint32_t>,Mesh> meshes_;
    std::map<uint32_t,WpcTexture*> textures_;
    std::map<uint32_t,bool> translucent_;
    std::map<uint32_t,bool> lit_; // textured: the Direct3D cache colour (0x4b21ae)
    std::map<uint32_t,MotionLib> motions_;
    int debug_clip_ = -1;
    std::map<uint32_t,Page> pages_;
    Matrix view_projection_,view_;
    uint32_t viewport_id_ = 0, camera_id_ = 0;
    bool enabled_ = true; // false without a camera viewport (general.wgm's pause brick)
};
} // namespace wpc
#endif
