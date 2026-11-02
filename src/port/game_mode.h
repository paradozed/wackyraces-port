#ifndef WPC_GAME_MODE_H
#define WPC_GAME_MODE_H

#include "game_assets.h"

#include <array>
#include <optional>

namespace wpc {
class Scene;
using ModeVector = std::array<float,3>;
// Basis columns X/Y/Z followed by position. The runtime's reserved word at
// matrix+0x24 is not serialized by 0x43ce40.
using ModeTransform = std::array<float,12>;

struct ModeCell {
    std::vector<uint32_t> links;
    uint32_t terminator = 0; // A word with (word & ~3)==0, preserved literally.
};
struct ModeExtra { std::string tag; std::vector<uint8_t> bytes; };
struct ModeRow {
    // Original runtime offsets +04,+10,+1c,+28. Meanings beyond their
    // original collision/grid use are deliberately left unnamed.
    std::array<ModeVector,4> vectors{};
    uint32_t word34 = 0, word38 = 0, grid_width = 0, grid_height = 0;
    // Original ID-wrapper values at +48,+50,+58; these are plain object IDs,
    // unlike the low-two-bit encoded links in the cells below.
    std::array<uint32_t,3> object_ids{};
    uint32_t ground_object_id = 0;
    std::vector<ModeVector> vectors60, vectors1e0, vectors364;
    std::vector<uint32_t> object_ids4e8;
    uint32_t stored_link_words = 0;
    std::vector<ModeCell> cells; // x + z*grid_width, original +cec table.
    // Matches original +ce8/+ce4: nonempty lists INCLUDING terminators;
    // empty-cell terminators are absent from this flattened buffer.
    std::vector<uint32_t> object_links;
};
struct ModeRoute {
    std::array<uint8_t,32> name_bytes{};
    std::string name;
    uint32_t word20 = 0;
    std::array<uint32_t,2> object_ids{}; // Original +28,+30 wrapper values.
    uint32_t word34 = 0;
    std::vector<ModeRow> rows;
};
struct ModeData {
    uint32_t version = 0;
    std::array<ModeVector,2> bounds{};
    std::array<uint32_t,3> header_words{};
    std::vector<ModeRoute> definitions;
    std::vector<uint32_t> route_indices;
    std::vector<ModeTransform> transforms;
    std::vector<ModeExtra> extras;

    const ModeRoute& route(size_t route_index) const;
    // Normal race/championship: route zero, row zero, first kind-zero
    // encoded object whose scene class is WStartLine (0x49dc90).
    std::optional<uint32_t> first_start_line_id(const Scene& scene) const;
};

// Only the fully recovered shipped version 0x01312ea1 is accepted.
ModeData decode_mode(ByteView payload);
ModeData load_mode(const Archive& archive);
} // namespace wpc
#endif
