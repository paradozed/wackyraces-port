#include "game_avi.h"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("AVI: "+text);}
uint32_t word(const std::vector<uint8_t>& b,size_t at) {
    if(at>b.size() || b.size()-at<4) bad("truncated field");
    return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
uint16_t half(const std::vector<uint8_t>& b,size_t at) {
    if(at>b.size() || b.size()-at<2) bad("truncated field");
    return uint16_t(b[at]|b[at+1]<<8);
}
bool tag(const std::vector<uint8_t>& b,size_t at,const char* name) {
    return at<=b.size() && b.size()-at>=4 && std::memcmp(b.data()+at,name,4)==0;
}
std::array<char,4> fourcc(const std::vector<uint8_t>& b,size_t at) {
    if(at>b.size() || b.size()-at<4) bad("truncated FOURCC");
    return {{char(b[at]),char(b[at+1]),char(b[at+2]),char(b[at+3])}};
}
struct Parser {
    Avi& avi;
    int depth=0;
    // Visit chunks in [begin,end); RIFF payloads are padded to even sizes.
    void chunks(size_t begin,size_t end,const std::string& list) {
        if(++depth>8) bad("LIST nesting too deep");
        const auto& b=avi.bytes;
        for(size_t at=begin;at<end;) {
            if(end-at<8) bad("truncated chunk header in "+list);
            const uint32_t size=word(b,at+4);
            const size_t payload=at+8;
            if(size>end-payload) bad("chunk exceeds its "+list+" list");
            if(tag(b,at,"LIST")) {
                if(size<4) bad("LIST without type");
                std::string type(reinterpret_cast<const char*>(b.data()+payload),4);
                if(type=="strl") avi.streams.emplace_back();
                chunks(payload+4,payload+size,type);
            } else if(list=="hdrl" && tag(b,at,"avih")) {
                if(size<40) bad("short avih");
                avi.microseconds_per_frame=word(b,payload);avi.total_frames=word(b,payload+16);
            } else if(list=="strl" && tag(b,at,"strh")) {
                if(size<36 || avi.streams.empty()) bad("invalid strh");
                auto& s=avi.streams.back();
                s.type=fourcc(b,payload);s.handler=fourcc(b,payload+4);
                s.scale=word(b,payload+20);s.rate=word(b,payload+24);s.length=word(b,payload+32);
            } else if(list=="strl" && tag(b,at,"strf")) {
                if(avi.streams.empty()) bad("strf before strh");
                auto& s=avi.streams.back();
                s.format.assign(b.begin()+long(payload),b.begin()+long(payload+size));
                if(s.video()) {
                    if(size<40) bad("short BITMAPINFOHEADER");
                    s.width=int32_t(word(b,payload+4));s.height=int32_t(word(b,payload+8));
                    s.bit_count=half(b,payload+14);s.compression=word(b,payload+16);
                } else if(s.audio()) {
                    if(size<16) bad("short WAVEFORMAT");
                    s.format_tag=half(b,payload);s.channels=half(b,payload+2);s.samples_per_second=word(b,payload+4);
                    s.block_align=half(b,payload+12);s.bits_per_sample=half(b,payload+14);
                }
            } else if(list=="movi" || list=="rec ") {
                // "NNxx": two decimal stream digits, then the data kind.
                const uint8_t* id=b.data()+at;
                if(id[0]>='0' && id[0]<='9' && id[1]>='0' && id[1]<='9') {
                    const size_t stream=size_t((id[0]-'0')*10+(id[1]-'0'));
                    if(stream>=avi.streams.size()) bad("movi chunk for a missing stream");
                    const bool palette=id[2]=='p' && id[3]=='c';
                    if(!palette) avi.chunks[stream].push_back({uint32_t(payload),size});
                }
            }
            if(list=="strl" && tag(b,at,"strh")) avi.chunks.resize(avi.streams.size());
            at=payload+size+(size&1);
            if(at>end) at=end;
        }
        --depth;
    }
};
}
int Avi::first_video() const {
    for(size_t i=0;i<streams.size();++i) if(streams[i].video()) return int(i);
    return -1;
}
Avi parse_avi(std::vector<uint8_t> bytes) {
    Avi avi;avi.bytes=std::move(bytes);
    const auto& b=avi.bytes;
    if(b.size()<12 || !tag(b,0,"RIFF") || !tag(b,8,"AVI ")) bad("not a RIFF AVI file");
    const uint32_t size=word(b,4);
    if(size<4 || size>b.size()-8) bad("RIFF size exceeds file");
    Parser parser{avi};
    parser.chunks(12,8+size_t(size),"AVI ");
    if(avi.streams.empty()) bad("no streams");
    avi.chunks.resize(avi.streams.size());
    for(const auto& s:avi.streams) {
        if(!s.scale || !s.rate) bad("stream has no rate");
        if(s.video() && (s.width<=0 || s.width>4096 || s.height==0 || std::abs(s.height)>4096)) bad("invalid video size");
    }
    return avi;
}
Avi load_avi(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) bad("cannot open "+path);
    in.seekg(0,std::ios::end);
    const std::streamoff length=in.tellg();
    if(length<0 || length>512*1024*1024) bad("unsupported file size: "+path);
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    in.seekg(0);
    if(!in.read(reinterpret_cast<char*>(bytes.data()),length)) bad("cannot read "+path);
    return parse_avi(std::move(bytes));
}
}
