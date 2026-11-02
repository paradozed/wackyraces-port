#ifndef WPC_GAME_ASSETS_H
#define WPC_GAME_ASSETS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wpc {

// All decoded images use top-left-origin, unpremultiplied RGBA bytes.
struct Image {
    uint32_t width = 0, height = 0;
    // Source TGA bits per pixel (8, 24 or 32); 0 for other image sources.
    uint32_t depth = 0;
    std::vector<uint8_t> rgba;
};

struct ByteView {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

Image decode_bmp(const uint8_t* data, size_t size);
Image load_bmp(const std::string& path);
// TGAF is a 20-byte wrapper followed by an embedded uncompressed TGA.
// Indexed alpha uses WPC's black-color key with opaque nonblack colors;
// linked-resource alpha thresholds have not yet been reconstructed.
Image decode_tgaf(const uint8_t* data, size_t size);

// Offsets refer to payload bytes (after the tag/length framing), in the
// Archive's owned storage. Resource IDs are only assigned in RSRC records.
struct Chunk {
    std::string tag;
    size_t offset = 0, size = 0;
    uint32_t id = 0;
};

class Archive {
public:
    void load(const std::string& path);
    void load_bytes(std::vector<uint8_t> bytes);
    const std::vector<Chunk>& resources() const { return resources_; }
    const std::vector<Chunk>& top_chunks() const { return top_chunks_; }
    const Chunk& find(const std::string& tag, uint32_t id) const;
    const Chunk& top(const std::string& tag) const;
    ByteView payload(const Chunk& chunk) const;
    // GTEX/GMAT have an ID and format token before their child chunks.
    std::vector<Chunk> children(const Chunk& chunk, size_t prefix = 8) const;
    Image texture(uint32_t id) const;
    uint32_t material_texture(uint32_t id) const;

private:
    std::vector<uint8_t> bytes_;
    std::vector<Chunk> top_chunks_, resources_;
};

} // namespace wpc
#endif
