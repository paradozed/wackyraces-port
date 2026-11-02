#ifndef WPC_GAME_SCENE_H
#define WPC_GAME_SCENE_H

#include "game_assets.h"

#include <array>
#include <unordered_map>
#include <utility>

namespace wpc {

struct Node {
    uint32_t id = 0;
    std::string class_name;
    std::unordered_map<uint32_t, std::vector<uint8_t>> properties;
    std::vector<Node> children;
    // Action classes use CLAS/ACCM; brick/screen classes use CLAS/BRCM.
    std::vector<Node> actions;
    // Auxiliary chunks, including the original ACTN bytes for compatibility.
    // Decoding an action does not execute its behavior.
    std::vector<std::pair<std::string, std::vector<uint8_t>>> extra_chunks;

    int32_t integer(uint32_t key, int32_t fallback = 0) const;
    float real(uint32_t key, float fallback = 0) const;
    uint32_t link(uint32_t key, const std::string& tag) const;
};

// A node's PRIM chunk: a serialized collision primitive (GDPrimSphere for
// cars and power-ups). Properties: 0x8c centre (local), 0x8d radius, 0x85
// name, 0x86 flags, 0x87 m_SensorGroup (runtime +0x24, the bits an object
// query mask selects: 0x8000 power-up, 0x100 car), 0x88 m_iObj.
// WPrimCylinder (0x1fa centre, the base, 0x1fb r, 0x1fc yheight; 0x482310)
// sets cylinder; 0x88 m_iObj's high half is the linked object (a cc_t1
// cactus WGndSprite for sensor group 0x40000).
struct ScenePrim {
    std::array<float, 3> centre{};
    float radius = 0, height = 0;
    bool cylinder = false;
    uint32_t flags = 0, sensor_group = 0, object = 0;
    std::string name;
};
std::vector<ScenePrim> node_prims(const Node& node);
// The archive's CLAS/PRIM member table: property ID i names entry i as
// (member, class). The loader 0x41bd20 remaps file IDs to registered members
// by it (table 0x6efa7c). Every installed and Championship I archive carries
// the same 764 entries, so the port's fixed IDs hold for them.
std::vector<std::pair<std::string, std::string>> member_names(const Archive& archive);

class Scene {
public:
    // canonical: a member table (member_names) whose IDs the port reads; the
    // archive's own IDs are then mapped by (member, class) name. The disc
    // Win/*.WGM lack seven test-class members (0xd5..0xdb), shifting theirs.
    void load(const Archive& archive, const std::vector<std::pair<std::string, std::string>>* canonical = nullptr);
    const Node& root() const { return root_; }
    const Node& screen() const;
    const Node& find(uint32_t id) const;
private:
    Node root_;
    Node screen_;
    bool loaded_ = false;
    bool has_screen_ = false;
};

class Strings {
public:
    void load(const std::string& path, uint32_t language);
    void load_bytes(const std::vector<uint8_t>& bytes, uint32_t language);
    const std::string& get(uint32_t id) const;
    uint32_t language() const { return language_; }
    size_t size() const { return strings_.size(); }
private:
    std::unordered_map<uint32_t, std::string> strings_;
    uint32_t language_ = 0;
};

} // namespace wpc
#endif
