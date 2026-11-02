#include "game_mode.h"
#include "game_scene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& what) { throw std::runtime_error("game mode: " + what); }
struct Reader {
    ByteView bytes;
    size_t position = 0;
    size_t left() const { return bytes.size-position; }
    const uint8_t* take(size_t count) {
        if (count>left()) bad("truncated WMOD field at " + std::to_string(position));
        const auto* result=bytes.data+position; position+=count; return result;
    }
    uint32_t word() {
        const auto* p=take(4);
        return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
    }
    float real() {
        uint32_t bits=word(); float result; std::memcpy(&result,&bits,4);
        if (!std::isfinite(result)) bad("nonfinite WMOD vector/matrix component");
        return result;
    }
    ModeVector vector() { return {{real(),real(),real()}}; }
    uint32_t count(size_t stride, size_t limit, const char* name) {
        const uint32_t value=word();
        if (value>limit || value>left()/stride) bad(std::string("invalid ")+name+" count");
        return value;
    }
};
std::vector<ModeVector> vectors(Reader& in, uint32_t count) {
    std::vector<ModeVector> result; result.reserve(count);
    for (uint32_t i=0;i<count;++i) result.push_back(in.vector());
    return result;
}
ModeRow row(Reader& in) {
    ModeRow result;
    for (auto& value:result.vectors) value=in.vector();
    result.word34=in.word(); result.word38=in.word();
    result.grid_width=in.word(); result.grid_height=in.word();
    for (auto& value:result.object_ids) value=in.word();
    result.ground_object_id=result.object_ids[2];
    const auto count_a=in.count(24,32,"paired vector");
    result.vectors60=vectors(in,count_a); result.vectors1e0=vectors(in,count_a);
    const auto count_b=in.count(16,32,"linked vector");
    result.vectors364=vectors(in,count_b);
    for (uint32_t i=0;i<count_b;++i) result.object_ids4e8.push_back(in.word());
    result.stored_link_words=in.word();
    // Division first prevents both integer overflow and allocations larger
    // than the minimum one-word-per-cell representation remaining on disk.
    if (result.grid_width && result.grid_height>in.left()/4/result.grid_width)
        bad("grid dimensions exceed remaining payload");
    const size_t cell_count=size_t(result.grid_width)*result.grid_height;
    if (cell_count>1024*1024) bad("grid exceeds decoder limit");
    result.cells.reserve(cell_count);
    for (size_t i=0;i<cell_count;++i) {
        ModeCell cell;
        uint32_t link=in.word();
        while (link&~uint32_t(3)) {
            if (result.object_links.size()>=2048) bad("encoded object links exceed original capacity");
            cell.links.push_back(link); result.object_links.push_back(link);
            link=in.word();
        }
        cell.terminator=link;
        if (!cell.links.empty()) {
            if (result.object_links.size()>=2048) bad("encoded object links exceed original capacity");
            result.object_links.push_back(link);
        }
        result.cells.push_back(std::move(cell));
    }
    return result;
}
} // namespace

const ModeRoute& ModeData::route(size_t index) const {
    if (index>=route_indices.size() || route_indices[index]>=definitions.size()) bad("route index out of range");
    return definitions[route_indices[index]];
}
std::optional<uint32_t> ModeData::first_start_line_id(const Scene& scene) const {
    if (route_indices.empty()) return std::nullopt;
    const auto& first=route(0);
    if (first.rows.empty()) return std::nullopt;
    for (uint32_t link:first.rows[0].object_links) {
        if ((link&~uint32_t(3)) && (link&3)==0) {
            // Original 0x49e3a4 skips an unresolved runtime object ID.
            const Node* object=nullptr;
            try { object=&scene.find(link>>2); } catch (const std::runtime_error&) { continue; }
            if (object->class_name=="WStartLine") return object->id;
        }
    }
    return std::nullopt;
}
ModeData decode_mode(ByteView payload) {
    if (!payload.data && payload.size) bad("null payload storage");
    Reader in{payload}; ModeData result;
    result.version=in.word();
    if (result.version!=0x01312ea1) bad("unsupported WMOD version");
    for (auto& value:result.bounds) value=in.vector();
    for (auto& value:result.header_words) value=in.word();
    const auto definition_count=in.count(52,64,"route definition");
    for (uint32_t i=0;i<definition_count;++i) {
        ModeRoute route;
        const auto* name=in.take(32); std::copy(name,name+32,route.name_bytes.begin());
        size_t length=0; while (length<32 && name[length]) ++length;
        route.name.assign(reinterpret_cast<const char*>(name),length);
        route.word20=in.word();
        for (auto& value:route.object_ids) value=in.word();
        route.word34=in.word();
        const auto row_count=in.count(88,4096,"route row");
        for (uint32_t j=0;j<row_count;++j) route.rows.push_back(row(in));
        result.definitions.push_back(std::move(route));
    }
    const auto route_count=in.count(4,8,"route index");
    for (uint32_t i=0;i<route_count;++i) {
        const auto index=in.word();
        if (index>=result.definitions.size()) bad("route references absent definition");
        result.route_indices.push_back(index);
    }
    const auto transform_count=in.count(48,14,"transform");
    for (uint32_t i=0;i<transform_count;++i) {
        ModeTransform transform;
        for (auto& value:transform) value=in.real();
        result.transforms.push_back(transform);
    }
    while (in.left()) {
        ModeExtra extra;
        const auto* tag=in.take(4); extra.tag.assign(reinterpret_cast<const char*>(tag),4);
        const uint32_t length=in.word(); const auto* bytes=in.take(length);
        extra.bytes.assign(bytes,bytes+length); result.extras.push_back(std::move(extra));
    }
    return result;
}
ModeData load_mode(const Archive& archive) { return decode_mode(archive.payload(archive.top("WMOD"))); }
} // namespace wpc
