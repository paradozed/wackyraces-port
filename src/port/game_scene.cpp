#include "game_scene.h"
#include <cmath>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& message) { throw std::runtime_error("game scene: " + message); }
uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

struct Reader {
    ByteView bytes;
    size_t position = 0;
    size_t remaining() const { return bytes.size - position; }
    ByteView take(size_t size) {
        if (size > remaining()) bad("truncated serialized field");
        ByteView value{bytes.data + position, size}; position += size; return value;
    }
    uint32_t integer() { return u32(take(4).data); }
    std::pair<std::string, ByteView> chunk() {
        const auto tag = take(4);
        const uint32_t size = integer();
        return {std::string(reinterpret_cast<const char*>(tag.data), 4), take(size)};
    }
};

std::vector<std::string> class_names(const Archive& archive, const std::string& table, bool required, uint32_t per_entry = 1) {
    const auto children = archive.children(archive.top("CLAS"), 0);
    const Chunk* names = nullptr;
    for (const auto& child : children) if (child.tag == table) {
        if (names) bad("duplicate " + table + " class-name table");
        names = &child;
    }
    if (!names) {
        if (required) bad("missing " + table + " class-name table");
        return {};
    }
    Reader reader{archive.payload(*names)};
    const uint32_t count = reader.integer();
    if (!count || count > 4096 || count > reader.remaining() / (5 * per_entry)) bad("invalid class-name count");
    std::vector<std::string> result;
    result.reserve(size_t(count) * per_entry);
    for (uint32_t i = 0; i < count * per_entry; ++i) {
        const uint32_t size = reader.integer();
        if (!size || size > 1024) bad("invalid class-name length");
        const auto name = reader.take(size);
        if (name.data[size-1] || std::memchr(name.data, 0, size-1)) bad("invalid class-name terminator");
        result.emplace_back(reinterpret_cast<const char*>(name.data), size-1);
    }
    // BRCM's last string is followed by up to three alignment bytes.
    const auto padding = reader.take(reader.remaining());
    if (padding.size > 3 || std::any_of(padding.data, padding.data+padding.size, [](uint8_t b) { return b != 0; }))
        bad("unexpected class-name table trailing bytes");
    return result;
}

}
std::vector<std::pair<std::string, std::string>> member_names(const Archive& archive) {
    // CLAS/PRIM: per entry the member name, then its class.
    const auto names = class_names(archive, "PRIM", true, 2);
    std::vector<std::pair<std::string, std::string>> result;
    for (size_t i = 0; i + 1 < names.size(); i += 2) result.emplace_back(names[i], names[i+1]);
    return result;
}
namespace {
struct TreeReader {
    const std::vector<std::string>& classes;
    const std::vector<std::string>& action_classes;
    std::set<uint32_t> ids;
    std::vector<uint32_t> remap; // archive member ID -> canonical ID (empty: none)
    Node node(ByteView bytes, size_t depth, bool action = false) {
        if (depth > 128 || ids.size() >= 65536) bad("scene tree exceeds decoder limits");
        Reader reader{bytes};
        const uint32_t type = reader.integer();
        Node result; result.id = reader.integer();
        if (!ids.insert(result.id).second) bad("duplicate scene node ID");
        const uint32_t class_index = type & 0xfff;
        const auto& names = action ? action_classes : classes;
        if (class_index >= names.size()) bad("scene node references a missing class");
        result.class_name = names[class_index];
        bool has_properties = false;
        while (reader.remaining()) {
            auto chunk = reader.chunk();
            if (chunk.first == "PRPS") {
                if (has_properties) bad("duplicate PRPS in scene node");
                has_properties = true;
                Reader properties{chunk.second};
                while (properties.remaining()) {
                    auto prop = properties.chunk();
                    if (prop.first != "PROP") bad("unexpected chunk in PRPS");
                    Reader value{prop.second};
                    uint32_t key = value.integer() & 0xffff;
                    if (!remap.empty()) key = key < remap.size() ? remap[key] : key | 0x10000;
                    const auto payload = value.take(value.remaining());
                    if (!result.properties.emplace(key, std::vector<uint8_t>(payload.data, payload.data+payload.size)).second)
                        bad("duplicate scene property ID");
                }
            } else if (chunk.first == "CHBR" && !action) {
                if (!has_properties) bad("scene child precedes PRPS");
                result.children.push_back(node(chunk.second, depth+1));
            } else {
                if (chunk.first == "ACTN" && !action) {
                    if (!has_properties) bad("scene action precedes PRPS");
                    result.actions.push_back(node(chunk.second, depth+1, true));
                }
                result.extra_chunks.emplace_back(chunk.first,
                    std::vector<uint8_t>(chunk.second.data, chunk.second.data+chunk.second.size));
            }
        }
        if (!has_properties) bad("missing PRPS in scene node");
        return result;
    }
};

const Node* find_node(const Node& node, uint32_t id) {
    if (node.id == id) return &node;
    for (const auto& child : node.children) if (const auto* found = find_node(child, id)) return found;
    for (const auto& action : node.actions) if (const auto* found = find_node(action, id)) return found;
    return nullptr;
}
} // namespace

int32_t Node::integer(uint32_t key, int32_t fallback) const {
    const auto value = properties.find(key);
    if (value == properties.end()) return fallback;
    if (value->second.size() != 4) bad("property is not a four-byte integer");
    return int32_t(u32(value->second.data()));
}

float Node::real(uint32_t key, float fallback) const {
    const auto value = properties.find(key);
    if (value == properties.end()) return fallback;
    if (value->second.size() != 4) bad("property is not a four-byte float");
    const uint32_t bits = u32(value->second.data());
    float result; static_assert(sizeof(result) == sizeof(bits), "32-bit float required");
    std::memcpy(&result, &bits, sizeof(result)); return result;
}

uint32_t Node::link(uint32_t key, const std::string& tag) const {
    const auto value = properties.find(key);
    if (value == properties.end()) bad("missing resource-link property");
    const auto& bytes = value->second;
    if (tag.size() != 4 || bytes.size() != 8 || std::memcmp(bytes.data(), tag.data(), 4))
        bad("resource-link property has an unexpected type");
    return u32(bytes.data()+4);
}

void Scene::load(const Archive& archive, const std::vector<std::pair<std::string, std::string>>* canonical) {
    const auto classes = class_names(archive, "BRCM", true);
    const auto action_classes = class_names(archive, "ACCM", false);
    TreeReader reader{classes, action_classes, {}, {}};
    if (canonical) {
        // The k-th (member, class) pair of the archive takes the k-th such
        // pair of the canonical table; members it lacks move above 0xffff.
        const auto members = member_names(archive);
        std::map<std::pair<std::string, std::string>, std::vector<uint32_t>> slots;
        for (size_t i = 0; i < canonical->size(); ++i) slots[(*canonical)[i]].push_back(uint32_t(i));
        std::map<std::pair<std::string, std::string>, size_t> used;
        for (size_t i = 0; i < members.size(); ++i) {
            const auto found = slots.find(members[i]);
            size_t& k = used[members[i]];
            reader.remap.push_back(found != slots.end() && k < found->second.size() ? found->second[k++] : uint32_t(i) | 0x10000);
        }
    }
    Node root = reader.node(archive.payload(archive.top("BRTR")), 0);
    Node screen;
    bool has_screen = false;
    for (const auto& chunk : archive.top_chunks()) if (chunk.tag == "SCRT") {
        screen = reader.node(archive.payload(chunk), 0);
        has_screen = true;
    }
    root_ = std::move(root); screen_ = std::move(screen);
    loaded_ = true; has_screen_ = has_screen;
}

const Node& Scene::screen() const {
    if (!loaded_ || !has_screen_) bad("missing SCRT screen tree");
    return screen_;
}

const Node& Scene::find(uint32_t id) const {
    if (loaded_) if (const Node* node = find_node(root_, id)) return *node;
    if (has_screen_) if (const Node* node = find_node(screen_, id)) return *node;
    bad("missing scene node " + std::to_string(id));
}

void Strings::load(const std::string& path, uint32_t language) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) bad("cannot open string table " + path);
    const auto end = file.tellg();
    if (end < 0 || uint64_t(end) > 16 * 1024 * 1024) bad("invalid string-table file length");
    std::vector<uint8_t> bytes(static_cast<size_t>(end));
    file.seekg(0);
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) bad("cannot read string table");
    load_bytes(bytes, language);
}

void Strings::load_bytes(const std::vector<uint8_t>& bytes, uint32_t language) {
    Reader header{{bytes.data(), bytes.size()}};
    const uint32_t count = header.integer();
    if (!count || count > header.remaining()/12) bad("invalid language count");
    struct Language { uint32_t id, count, offset; };
    std::vector<Language> languages;
    std::set<uint32_t> ids;
    uint32_t selected = 0;
    for (uint32_t i = 0; i < count; ++i) {
        Language row{header.integer(), header.integer(), header.integer()};
        if (!ids.insert(row.id).second) bad("duplicate language ID");
        if (row.offset > bytes.size() || row.count > (bytes.size()-row.offset)/8)
            bad("language entry table exceeds file");
        if (row.id == language) selected = i;
        languages.push_back(row);
    }
    size_t tables_end = header.position;
    std::vector<std::pair<size_t, size_t>> ranges;
    for (const auto& row : languages) {
        if (row.offset < header.position) bad("language entries overlap header");
        const size_t end = row.offset + size_t(row.count)*8;
        tables_end = std::max(tables_end, end);
        if (row.count) ranges.push_back({row.offset, end});
    }
    std::sort(ranges.begin(), ranges.end());
    for (size_t i = 1; i < ranges.size(); ++i)
        if (ranges[i].first < ranges[i-1].second) bad("overlapping language entry tables");
    std::unordered_map<uint32_t, std::string> strings;
    for (size_t index = 0; index < languages.size(); ++index) {
        const auto& row = languages[index];
        for (uint32_t i = 0; i < row.count; ++i) {
            const uint8_t* entry = bytes.data() + row.offset + size_t(i)*8;
            const uint32_t key = u32(entry), offset = u32(entry+4);
            if (offset < tables_end || offset >= bytes.size()) bad("string offset outside text area");
            const void* zero = std::memchr(bytes.data()+offset, 0, bytes.size()-offset);
            if (!zero) bad("unterminated localized string");
            if (index != selected) continue;
            const size_t length = static_cast<const uint8_t*>(zero) - (bytes.data()+offset);
            std::string decoded; decoded.reserve(length);
            for (size_t n = 0; n < length; ++n) {
                const uint8_t raw = bytes[offset+n];
                decoded.push_back(char(raw == 0x7f ? raw : raw ^ 0x7f));
            }
            // The installed tables contain duplicate IDs 254 and 999.
            // WPC's lookup at 0x0044dba0 returns the first matching row.
            strings.emplace(key, std::move(decoded));
        }
    }
    strings_ = std::move(strings); language_ = languages[selected].id;
}

const std::string& Strings::get(uint32_t id) const {
    const auto found = strings_.find(id);
    if (found == strings_.end()) bad("missing localized string " + std::to_string(id));
    return found->second;
}

std::vector<ScenePrim> node_prims(const Node& node) {
    std::vector<ScenePrim> prims;
    for (const auto& [tag, bytes] : node.extra_chunks) {
        if (tag != "PRIM") continue;
        Reader reader{ByteView{bytes.data(), bytes.size()}};
        reader.integer(); reader.integer(); // class/type word and object ID
        ScenePrim prim;
        while (reader.remaining()) {
            auto chunk = reader.chunk();
            if (chunk.first != "PRPS") continue;
            Reader properties{chunk.second};
            while (properties.remaining()) {
                auto prop = properties.chunk();
                if (prop.first != "PROP") bad("unexpected chunk in PRIM properties");
                Reader value{prop.second};
                const uint32_t key = value.integer() & 0xffff;
                const auto payload = value.take(value.remaining());
                const auto word = [&](size_t at) {
                    if (payload.size < at + 4) bad("truncated PRIM property");
                    const uint8_t* p = payload.data + at;
                    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
                };
                const auto real = [&](size_t at) { uint32_t w = word(at); float f; std::memcpy(&f, &w, 4); return f; };
                if (key == 0x8c) prim.centre = {{real(0), real(4), real(8)}};
                else if (key == 0x8d) prim.radius = real(0);
                else if (key == 0x86) prim.flags = word(0);
                else if (key == 0x87) prim.sensor_group = word(0);
                else if (key == 0x88) prim.object = word(0);
                else if (key == 0x1fa) {prim.centre = {{real(0), real(4), real(8)}}; prim.cylinder = true;}
                else if (key == 0x1fb) prim.radius = real(0);
                else if (key == 0x1fc) prim.height = real(0);
                else if (key == 0x85 && payload.size >= 4) {
                    const uint32_t length = word(0);
                    if (length > payload.size - 4) bad("PRIM name exceeds its property");
                    prim.name.assign(reinterpret_cast<const char*>(payload.data + 4), length);
                    while (!prim.name.empty() && prim.name.back() == '\0') prim.name.pop_back();
                }
            }
        }
        if (!std::isfinite(prim.radius) || prim.radius < 0) bad("invalid PRIM radius");
        if (!std::isfinite(prim.height) || prim.height < 0) bad("invalid PRIM cylinder height");
        prims.push_back(prim);
    }
    return prims;
}
} // namespace wpc
