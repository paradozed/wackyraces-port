#include "game_assets.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;
size_t checks = 0;

void require(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void rejected(const std::function<void()>& action, const char* message) {
    bool failed = false;
    try { action(); } catch (const std::runtime_error&) { failed = true; }
    require(failed, message);
}

void put16(Bytes& b, size_t p, uint16_t n) {
    b.at(p) = uint8_t(n); b.at(p+1) = uint8_t(n >> 8);
}
void put32(Bytes& b, size_t p, uint32_t n) {
    for (int i = 0; i < 4; ++i) b.at(p+i) = uint8_t(n >> (8*i));
}
void tag(Bytes& b, size_t p, const char* value) {
    std::copy(value, value + 4, b.begin() + p);
}
void append(Bytes& b, const Bytes& more) { b.insert(b.end(), more.begin(), more.end()); }
Bytes framed(const char* name, const Bytes& payload) {
    Bytes b(8); tag(b, 0, name); put32(b, 4, uint32_t(payload.size())); append(b, payload); return b;
}

Bytes bmp() {
    Bytes b(70, 0);
    b[0] = 'B'; b[1] = 'M'; put32(b, 2, uint32_t(b.size())); put32(b, 10, 54);
    put32(b, 14, 40); put32(b, 18, 2); put32(b, 22, 2);
    put16(b, 26, 1); put16(b, 28, 24);
    // Bottom-up BGR raster: blue, white; red, green. Two padding bytes/row.
    const uint8_t pixels[] = {255,0,0, 255,255,255, 88,99, 0,0,255, 0,255,0, 77,66};
    std::copy(std::begin(pixels), std::end(pixels), b.begin()+54);
    return b;
}

Bytes tgaf(uint32_t w, uint32_t h, uint8_t depth, bool indexed, const Bytes& raster) {
    Bytes b(38, 0);
    put32(b, 4, w); put32(b, 8, h); put32(b, 12, depth);
    b[21] = indexed ? 1 : 0; b[22] = indexed ? 1 : 2;
    if (indexed) { put16(b, 23, 7); put16(b, 25, 2); b[27] = 24; }
    put16(b, 32, uint16_t(w)); put16(b, 34, uint16_t(h)); b[36] = depth;
    if (indexed) append(b, {0,255,0, 0,0,0}); // Index 7 green, index 8 black/keyed.
    append(b, raster);
    put32(b, 16, uint32_t(b.size() - 20));
    return b;
}

void test_bmp() {
    auto b = bmp();
    auto image = wpc::decode_bmp(b.data(), b.size());
    require(image.width == 2 && image.height == 2, "BMP dimensions");
    require(image.rgba == Bytes({255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255}),
            "BMP row orientation, BGR conversion, and padded stride");
    for (size_t n = 0; n < b.size(); ++n)
        rejected([&] { wpc::decode_bmp(b.data(), n); }, "truncated BMP accepted");
    put32(b, 22, uint32_t(-2));
    image = wpc::decode_bmp(b.data(), b.size());
    require(image.rgba[2] == 255 && image.rgba[8] == 255, "top-origin BMP rows");
    auto invalid = b; put32(invalid, 22, 0x80000000);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "BMP height overflow accepted");
    invalid = b; put32(invalid, 18, 0x7fffffff);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "BMP width overflow accepted");
    invalid = b; put32(invalid, 10, 0xffffffff);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "BMP raster offset overflow accepted");
    invalid = b; put32(invalid, 10, 4);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "BMP raster/header overlap accepted");
    invalid = b; put32(invalid, 30, 1);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "compressed BMP accepted");
    invalid = b; put32(invalid, 14, 0xffffffff);
    rejected([&] { wpc::decode_bmp(invalid.data(), invalid.size()); }, "BMP DIB length overflow accepted");
    rejected([] { wpc::decode_bmp(nullptr, 100); }, "null BMP accepted");
}

void test_tgaf() {
    auto b = tgaf(2, 2, 32, false, {255,0,0,33, 255,255,255,44, 0,0,255,11, 0,255,0,22});
    auto image = wpc::decode_tgaf(b.data(), b.size());
    require(image.rgba == Bytes({255,0,0,11, 0,255,0,22, 0,0,255,33, 255,255,255,44}),
            "TGAF BGRA channels, origin, and truecolor alpha");
    for (size_t n = 0; n < b.size(); ++n)
        rejected([&] { wpc::decode_tgaf(b.data(), n); }, "truncated TGAF accepted");
    b[37] = 0x20;
    image = wpc::decode_tgaf(b.data(), b.size());
    require(image.rgba[2] == 255 && image.rgba[3] == 33, "top-origin TGA");
    b[37] = 0x10;
    image = wpc::decode_tgaf(b.data(), b.size());
    require(image.rgba[0] == 0 && image.rgba[1] == 255 && image.rgba[3] == 22,
            "right-origin TGA");
    b[37] = 0;
    // Valid TGA 2.0 footer belongs to the embedded extent, not to the wrapper padding.
    append(b, Bytes(8, 0));
    const char signature[] = "TRUEVISION-XFILE.";
    b.insert(b.end(), signature, signature + sizeof(signature)); put32(b, 16, uint32_t(b.size()-20));
    require(wpc::decode_tgaf(b.data(), b.size()).rgba.size() == 16, "standard TGA footer");
    b.back() = 42;
    rejected([&] { wpc::decode_tgaf(b.data(), b.size()); }, "invalid TGA footer accepted");

    auto indexed = tgaf(2, 1, 8, true, {7,8});
    image = wpc::decode_tgaf(indexed.data(), indexed.size());
    require(image.rgba == Bytes({0,255,0,255, 0,0,0,0}), "TGA palette origin and black-color alpha key");
    // Palette entry zero is not inherently transparent.
    put16(indexed, 23, 0); indexed[44] = 0; indexed[45] = 1;
    image = wpc::decode_tgaf(indexed.data(), indexed.size());
    require(image.rgba[3] == 255 && image.rgba[7] == 0, "TGA keys color rather than index zero");
    for (size_t n = 0; n < indexed.size(); ++n)
        rejected([&] { wpc::decode_tgaf(indexed.data(), n); }, "truncated indexed TGAF accepted");
    indexed[44] = 2;
    rejected([&] { wpc::decode_tgaf(indexed.data(), indexed.size()); }, "out-of-range TGA palette index accepted");
    auto blank = tgaf(0, 106, 8, true, {});
    image = wpc::decode_tgaf(blank.data(), blank.size());
    require(image.width == 0 && image.height == 106 && image.rgba.empty(), "zero-width menu glyph");
    auto rgb = tgaf(1, 1, 24, false, {1,2,3});
    require(wpc::decode_tgaf(rgb.data(), rgb.size()).rgba == Bytes({3,2,1,255}), "24-bit TGA conversion");
    auto extended = rgb;
    const uint32_t extension = uint32_t(extended.size()-20);
    append(extended, Bytes(495, 0)); put16(extended, 20+extension, 495);
    const size_t footer = extended.size(); append(extended, Bytes(8, 0));
    put32(extended, footer, extension);
    extended.insert(extended.end(), signature, signature + sizeof(signature));
    put32(extended, 16, uint32_t(extended.size()-20));
    require(wpc::decode_tgaf(extended.data(), extended.size()).rgba == Bytes({3,2,1,255}),
            "bounded TGA 2.0 extension");
    put32(extended, footer, extension+1);
    rejected([&] { wpc::decode_tgaf(extended.data(), extended.size()); }, "bad TGA extension offset accepted");
    // An image ID precedes the raster and changes its offset.
    rgb[20] = 2; rgb.insert(rgb.begin()+38, {99,100}); put32(rgb, 16, uint32_t(rgb.size()-20));
    require(wpc::decode_tgaf(rgb.data(), rgb.size()).rgba == Bytes({3,2,1,255}), "TGA image-ID extent");
    auto invalid = rgb; put32(invalid, 16, 0xffffffff);
    rejected([&] { wpc::decode_tgaf(invalid.data(), invalid.size()); }, "TGA length overflow accepted");
    invalid = rgb; put32(invalid, 4, 0xffffffff);
    rejected([&] { wpc::decode_tgaf(invalid.data(), invalid.size()); }, "TGA wrapper mismatch accepted");
    invalid = rgb; invalid[22] = 10;
    rejected([&] { wpc::decode_tgaf(invalid.data(), invalid.size()); }, "compressed TGA accepted");
    invalid = rgb; invalid[37] = 0x40;
    rejected([&] { wpc::decode_tgaf(invalid.data(), invalid.size()); }, "interleaved TGA accepted");
    invalid = rgb; append(invalid, {0,0});
    rejected([&] { wpc::decode_tgaf(invalid.data(), invalid.size()); }, "invalid wrapper alignment accepted");
    rejected([] { wpc::decode_tgaf(nullptr, 100); }, "null TGAF accepted");
}

Bytes archive() {
    auto texture = Bytes(8, 0); put32(texture, 0, 12); put32(texture, 4, 0x0130e07e);
    append(texture, framed("TGAF", tgaf(1, 1, 32, false, {3,2,1,4})));
    Bytes texref(12, 0); put32(texref, 0, 1); tag(texref, 4, "GTEX"); put32(texref, 8, 12);
    Bytes material(8, 0); put32(material, 0, 34); put32(material, 4, 0x0130e079);
    append(material, framed("TEXP", texref));
    Bytes resources = framed("GTEX", texture); append(resources, framed("GMAT", material));
    Bytes b(8, 0); tag(b, 0, "WPCG"); put32(b, 4, 0x02020000); append(b, framed("RSRC", resources));
    return b;
}

void test_archive() {
    const auto b = archive();
    wpc::Archive ar; ar.load_bytes(b);
    require(ar.resources().size() == 2 && ar.top_chunks().size() == 1, "WGM resource index");
    require(ar.find("GTEX", 12).id == 12 && ar.material_texture(34) == 12, "WGM material/texture join");
    require(ar.texture(12).rgba == Bytes({1,2,3,4}), "WGM nested TGAF decoding");
    auto copy = ar;
    require(copy.texture(12).rgba == ar.texture(12).rgba, "copied archive offset ownership");
    rejected([&] { ar.find("GTEX", 99); }, "missing resource accepted");
    rejected([&] { ar.top("BRTR"); }, "missing top-level chunk accepted");
    rejected([&] { ar.children(ar.find("GTEX", 12), size_t(-1)); }, "overflowing child prefix accepted");
    rejected([&] { ar.payload({"FAKE", size_t(-1), 8, 0}); }, "overflowing archive view accepted");
    for (size_t n = 0; n < b.size(); ++n) {
        Bytes prefix(b.begin(), b.begin()+n);
        rejected([&] { ar.load_bytes(prefix); }, "truncated WGM accepted");
    }
    require(ar.texture(12).rgba == Bytes({1,2,3,4}), "failed reload replaced valid archive");
    auto invalid = b; put32(invalid, 12, 0xffffffff);
    rejected([&] { ar.load_bytes(invalid); }, "WGM chunk length overflow accepted");
    invalid = b; put32(invalid, 4, 0x01000000);
    rejected([&] { ar.load_bytes(invalid); }, "unknown WGM version accepted");
    invalid = b; append(invalid, framed("RSRC", {}));
    rejected([&] { ar.load_bytes(invalid); }, "duplicate RSRC accepted");
    auto resources = Bytes(b.begin()+16, b.end()); const auto duplicate = resources; append(resources, duplicate);
    invalid.assign(b.begin(), b.begin()+8); append(invalid, framed("RSRC", resources));
    rejected([&] { ar.load_bytes(invalid); }, "duplicate resource key accepted");
    invalid = b;
    // Child TGAF length is independent of the valid outer resource framing.
    put32(invalid, 36, 0xffffffff); ar.load_bytes(invalid);
    rejected([&] { ar.texture(12); }, "nested WGM length overflow accepted");
    invalid = b; put32(invalid, invalid.size()-4, 99); ar.load_bytes(invalid);
    rejected([&] { ar.material_texture(34); }, "dangling material texture accepted");
}

void inspect_archive(const std::string& path) {
    wpc::Archive ar; ar.load(path);
    size_t textures = 0, empty = 0, materials = 0, untextured = 0;
    for (const auto& resource : ar.resources()) {
        if (resource.tag == "GTEX") {
            const auto image = ar.texture(resource.id);
            ++textures;
            if (image.rgba.empty()) ++empty;
        } else if (resource.tag == "GMAT") {
            bool no_texture = false;
            for (const auto& child : ar.children(resource)) {
                if (child.tag != "TEXP") continue;
                const auto value = ar.payload(child);
                no_texture = value.size == 4 && std::all_of(value.data, value.data+4,
                                                          [](uint8_t v) { return v == 0; });
            }
            if (no_texture) ++untextured;
            else { ar.material_texture(resource.id); ++materials; }
        }
    }
    std::printf("%s: %zu resources, %zu textures (%zu empty), %zu texture links, %zu untextured materials\n",
                path.c_str(), ar.resources().size(), textures, empty, materials, untextured);
}
} // namespace

int main(int argc, char** argv) {
    try {
        test_bmp(); test_tgaf(); test_archive();
        std::printf("native assets: %zu checks passed\n", checks);
        // Optional read-only corpus inspection; no game data is stored in tests.
        for (int i = 1; i < argc; ++i) inspect_archive(argv[i]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "native assets: %s (after %zu checks)\n", error.what(), checks);
        return 1;
    }
}
