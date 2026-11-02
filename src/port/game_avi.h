#ifndef WPC_GAME_AVI_H
#define WPC_GAME_AVI_H
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace wpc {
// RIFF AVI 1.0 as read by WPC through AVIFile (AVIFileOpen/AVIStreamRead).
struct AviStream {
    std::array<char,4> type{},handler{};   // strh fccType/fccHandler
    uint32_t scale=1,rate=0,length=0;       // strh dwScale/dwRate/dwLength
    // Video strf (BITMAPINFOHEADER).
    uint32_t compression=0;int32_t width=0,height=0;uint16_t bit_count=0;
    // Audio strf (WAVEFORMATEX).
    uint16_t format_tag=0,channels=0,bits_per_sample=0,block_align=0;uint32_t samples_per_second=0;
    std::vector<uint8_t> format;            // raw strf payload
    bool video() const {return type==std::array<char,4>{{'v','i','d','s'}};}
    bool audio() const {return type==std::array<char,4>{{'a','u','d','s'}};}
};
struct AviChunk {uint32_t offset=0,size=0;}; // payload range in Avi::bytes
struct Avi {
    std::vector<uint8_t> bytes;
    uint32_t microseconds_per_frame=0,total_frames=0;
    std::vector<AviStream> streams;
    std::vector<std::vector<AviChunk>> chunks; // per stream, movi order
    int first_video() const;
};
Avi parse_avi(std::vector<uint8_t> bytes);
Avi load_avi(const std::string& path);
}
#endif
