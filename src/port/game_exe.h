#ifndef WPC_GAME_EXE_H
#define WPC_GAME_EXE_H
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace wpc {
// The installed WPC.EXE, read as data: a few interface strings are only in
// the executable, and the port takes them from there rather than carrying
// game text in its sources.
class ExeImage {
public:
    // Throws std::runtime_error when the file is missing or is not a PE image.
    void load(const std::filesystem::path& path);
    void load_bytes(std::vector<uint8_t> bytes);
    bool empty() const {return bytes_.empty();}
    // The NUL-terminated string at virtual address va (image base included),
    // or "" when va is outside every section's file data.
    std::string string_at(uint32_t va) const;
    // The little-endian word at va; false when it lies outside the file data.
    bool u32_at(uint32_t va,uint32_t& value) const;
private:
    struct Section {uint32_t va=0,size=0,offset=0;};
    std::vector<uint8_t> bytes_;
    std::vector<Section> sections_;
    uint32_t base_=0;
};
}
#endif
