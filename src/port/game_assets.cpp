#include "game_assets.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace wpc {
namespace {

[[noreturn]] void bad(const std::string& message) {
    throw std::runtime_error("game asset: " + message);
}

void span(size_t size, size_t offset, size_t count, const char* what) {
    if (offset > size || count > size - offset) bad(std::string("truncated ") + what);
}

uint16_t u16(const uint8_t* p) {
    return uint16_t(p[0]) | uint16_t(p[1]) << 8;
}

uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

size_t image_bytes(uint32_t width, uint32_t height) {
    // Limits bound allocations even for hostile headers; installed images
    // are much smaller. Arithmetic is widened before multiplication.
    const uint64_t pixels = uint64_t(width) * height;
    if (width > 16384 || height > 16384 || pixels > 64 * 1024 * 1024)
        bad("image dimensions exceed decoder limits");
    return size_t(pixels * 4);
}

std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) bad("cannot open " + path);
    const auto end = file.tellg();
    if (end < 0 || uint64_t(end) > 512 * 1024 * 1024)
        bad("invalid or oversized file " + path);
    std::vector<uint8_t> bytes(static_cast<size_t>(end));
    file.seekg(0);
    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()))
        bad("cannot read " + path);
    return bytes;
}

std::vector<Chunk> chunks(const std::vector<uint8_t>& bytes, size_t offset, size_t size) {
    span(bytes.size(), offset, size, "chunk region");
    const size_t end = offset + size;
    std::vector<Chunk> result;
    while (offset < end) {
        span(end, offset, 8, "chunk header");
        const uint32_t count = u32(bytes.data() + offset + 4);
        span(end, offset + 8, count, "chunk payload");
        result.push_back({std::string(reinterpret_cast<const char*>(bytes.data() + offset), 4),
                          offset + 8, count, 0});
        offset += 8 + size_t(count);
    }
    return result;
}

} // namespace

Image decode_bmp(const uint8_t* data, size_t size) {
    if (!data) bad("null BMP input");
    span(size, 0, 54, "BMP header");
    if (data[0] != 'B' || data[1] != 'M') bad("invalid BMP signature");
    const uint32_t declared_size = u32(data + 2), raster = u32(data + 10);
    const uint32_t dib = u32(data + 14);
    if (dib < 40) bad("unsupported BMP DIB header");
    span(size, 14, dib, "BMP DIB header");
    const int64_t signed_width = int32_t(u32(data + 18));
    const int64_t signed_height = int32_t(u32(data + 22));
    if (signed_width <= 0 || signed_height == 0) bad("invalid BMP dimensions");
    if (u16(data + 26) != 1 || u16(data + 28) != 24 || u32(data + 30) != 0)
        bad("unsupported BMP format (expected uncompressed 24-bit RGB)");
    Image image;
    image.width = static_cast<uint32_t>(signed_width);
    image.height = static_cast<uint32_t>(signed_height < 0 ? -signed_height : signed_height);
    const size_t output_size = image_bytes(image.width, image.height);
    const size_t row_bytes = (size_t(image.width) * 3 + 3) & ~size_t(3);
    if (raster < uint64_t(14) + dib) bad("BMP raster overlaps header");
    span(size, raster, row_bytes * image.height, "BMP raster");
    if (declared_size > size || declared_size < uint64_t(raster) + row_bytes * image.height)
        bad("inconsistent BMP file length");
    image.rgba.resize(output_size);
    for (uint32_t y = 0; y < image.height; ++y) {
        const uint32_t source_y = signed_height < 0 ? y : image.height - 1 - y;
        const uint8_t* source = data + raster + source_y * row_bytes;
        uint8_t* out = image.rgba.data() + size_t(y) * image.width * 4;
        for (uint32_t x = 0; x < image.width; ++x) {
            out[4*x] = source[3*x+2]; out[4*x+1] = source[3*x+1];
            out[4*x+2] = source[3*x]; out[4*x+3] = 255;
        }
    }
    return image;
}

Image load_bmp(const std::string& path) {
    const auto bytes = read_file(path);
    return decode_bmp(bytes.data(), bytes.size());
}

Image decode_tgaf(const uint8_t* data, size_t size) {
    if (!data) bad("null TGAF input");
    span(size, 0, 38, "TGAF/TGA header");
    const uint32_t embedded_size = u32(data + 16);
    if (embedded_size < 18) bad("invalid embedded TGA length");
    span(size, 20, embedded_size, "embedded TGA");
    const size_t trailing = size - 20 - embedded_size;
    if (trailing && (trailing > 3 || (size & 3))) bad("unexpected TGAF trailing data");
    const uint8_t* tga = data + 20;
    const uint8_t type = tga[2], depth = tga[16], descriptor = tga[17];
    Image image;
    image.width = u16(tga + 12); image.height = u16(tga + 14); image.depth = depth;
    if (image.width != u32(data + 4) || image.height != u32(data + 8) || depth != u32(data + 12))
        bad("TGAF wrapper and TGA dimensions/depth disagree");
    if (descriptor & 0xc0) bad("interleaved TGA is unsupported");
    const bool indexed = type == 1 && tga[1] == 1 && depth == 8 && tga[7] == 24;
    const bool truecolor = type == 2 && tga[1] == 0 && (depth == 24 || depth == 32);
    if (!indexed && !truecolor) bad("unsupported embedded TGA format");
    const uint32_t palette_first = u16(tga + 3), palette_count = u16(tga + 5);
    if (indexed && (!palette_count || palette_first + palette_count > 256))
        bad("invalid indexed TGA palette range");
    if (truecolor && (palette_first || palette_count || tga[7]))
        bad("unexpected truecolor TGA palette");
    const size_t palette = 18 + size_t(tga[0]);
    const size_t palette_size = indexed ? size_t(palette_count) * 3 : 0;
    span(embedded_size, palette, palette_size, "TGA palette/image ID");
    const size_t raster = palette + palette_size;
    const size_t output_size = image_bytes(image.width, image.height);
    const size_t pixel_count = output_size / 4, source_pixel_size = depth / 8;
    span(embedded_size, raster, pixel_count * source_pixel_size, "TGA raster");
    const size_t end = raster + pixel_count * source_pixel_size;
    if (end != embedded_size) {
        static const char signature[] = "TRUEVISION-XFILE.";
        if (embedded_size - end < 26) bad("incomplete TGA footer");
        const size_t footer = embedded_size - 26;
        if (std::memcmp(tga + footer + 8, signature, sizeof(signature)))
            bad("unsupported data after TGA raster");
        const uint32_t extension = u32(tga + footer), developer = u32(tga + footer + 4);
        if (developer) bad("TGA developer areas are unsupported");
        if (!extension) {
            if (footer != end) bad("unframed data before TGA footer");
        } else {
            // Fourteen installed general-bank images carry the standard
            // 495-byte TGA 2.0 extension. Its descriptive metadata does not
            // alter the stored raster; no metadata offsets are dereferenced.
            if (extension != end || footer - end != 495 || u16(tga + end) != 495)
                bad("unsupported TGA extension framing");
        }
    }
    image.rgba.resize(output_size);
    for (uint32_t y = 0; y < image.height; ++y) {
        const uint32_t source_y = descriptor & 0x20 ? y : image.height - 1 - y;
        for (uint32_t x = 0; x < image.width; ++x) {
            const uint32_t source_x = descriptor & 0x10 ? image.width - 1 - x : x;
            const uint8_t* source = tga + raster + (size_t(source_y) * image.width + source_x) * source_pixel_size;
            if (indexed) {
                const uint32_t index = *source;
                if (index < palette_first || index - palette_first >= palette_count)
                    bad("TGA pixel references a missing palette entry");
                source = tga + palette + (index - palette_first) * 3;
            }
            uint8_t* out = image.rgba.data() + (size_t(y) * image.width + x) * 4;
            out[0] = source[2]; out[1] = source[1]; out[2] = source[0];
            out[3] = indexed ? ((source[0] | source[1] | source[2]) ? 255 : 0) :
                     depth == 32 ? source[3] : 255;
        }
    }
    return image;
}

void Archive::load(const std::string& path) { load_bytes(read_file(path)); }

void Archive::load_bytes(std::vector<uint8_t> bytes) {
    span(bytes.size(), 0, 8, "WGM header");
    if (std::memcmp(bytes.data(), "WPCG", 4) || u32(bytes.data() + 4) != 0x02020000)
        bad("unsupported WGM signature/version");
    auto top_chunks = chunks(bytes, 8, bytes.size() - 8);
    std::set<std::string> top_tags;
    std::vector<Chunk> resources;
    std::set<std::pair<std::string, uint32_t>> resource_keys;
    bool has_resources = false;
    for (const Chunk& top : top_chunks) {
        if (!top_tags.insert(top.tag).second) bad("duplicate top-level WGM chunk " + top.tag);
        if (top.tag != "RSRC") continue;
        has_resources = true;
        resources = chunks(bytes, top.offset, top.size);
        for (Chunk& resource : resources) {
            span(resource.size, 0, 4, "resource ID");
            resource.id = u32(bytes.data() + resource.offset);
            if (!resource_keys.insert({resource.tag, resource.id}).second)
                bad("duplicate WGM resource ID/tag");
        }
    }
    if (!has_resources) bad("missing WGM RSRC chunk");
    bytes_ = std::move(bytes);
    top_chunks_ = std::move(top_chunks);
    resources_ = std::move(resources);
}

const Chunk& Archive::find(const std::string& tag, uint32_t id) const {
    const auto it = std::find_if(resources_.begin(), resources_.end(), [&](const Chunk& chunk) {
        return chunk.tag == tag && chunk.id == id;
    });
    if (it == resources_.end()) bad("missing " + tag + " resource " + std::to_string(id));
    return *it;
}

const Chunk& Archive::top(const std::string& tag) const {
    const auto it = std::find_if(top_chunks_.begin(), top_chunks_.end(), [&](const Chunk& chunk) {
        return chunk.tag == tag;
    });
    if (it == top_chunks_.end()) bad("missing top-level chunk " + tag);
    return *it;
}

ByteView Archive::payload(const Chunk& chunk) const {
    span(bytes_.size(), chunk.offset, chunk.size, "archive payload");
    return {bytes_.data() + chunk.offset, chunk.size};
}

std::vector<Chunk> Archive::children(const Chunk& chunk, size_t prefix) const {
    payload(chunk); // Validate caller-supplied ranges before adding offsets.
    span(chunk.size, 0, prefix, "resource prefix");
    return chunks(bytes_, chunk.offset + prefix, chunk.size - prefix);
}

Image Archive::texture(uint32_t id) const {
    const auto nested = children(find("GTEX", id));
    const Chunk* image = nullptr;
    for (const auto& chunk : nested) {
        if (chunk.tag == "TGAF") {
            if (image) bad("duplicate TGAF in texture");
            image = &chunk;
        }
    }
    if (!image) bad("missing TGAF in texture");
    const auto bytes = payload(*image);
    return decode_tgaf(bytes.data, bytes.size);
}

uint32_t Archive::material_texture(uint32_t id) const {
    const auto nested = children(find("GMAT", id));
    const Chunk* reference = nullptr;
    for (const auto& chunk : nested) {
        if (chunk.tag == "TEXP") {
            if (reference) bad("duplicate TEXP in material");
            reference = &chunk;
        }
    }
    if (!reference) bad("missing TEXP in material");
    const auto bytes = payload(*reference);
    if (bytes.size != 12 || u32(bytes.data) != 1 || std::memcmp(bytes.data + 4, "GTEX", 4))
        bad("unsupported material texture link");
    const uint32_t texture_id = u32(bytes.data + 8);
    find("GTEX", texture_id); // Reject dangling links while resolving.
    return texture_id;
}

} // namespace wpc
