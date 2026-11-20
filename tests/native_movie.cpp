#include "game_avi.h"
#include "movie_decoder.h"
#include <cstdio>
#include <array>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void rejected(const std::function<void()>& action,const char* text) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,text);
}
void put(Bytes& b,uint32_t v) {for(int i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8)));}
Bytes chunk(const char* id,const Bytes& payload) {
    Bytes b(id,id+4);put(b,uint32_t(payload.size()));b.insert(b.end(),payload.begin(),payload.end());
    if(payload.size()&1) b.push_back(0);
    return b;
}
Bytes list(const char* type,const std::vector<Bytes>& items) {
    Bytes payload(type,type+4);
    for(const auto& i:items) payload.insert(payload.end(),i.begin(),i.end());
    return chunk("LIST",payload);
}
Bytes words(std::initializer_list<uint32_t> values) {Bytes b;for(uint32_t v:values) put(b,v);return b;}
Bytes sample() {
    Bytes avih=words({40000,0,0,0x10,3,0,2,0,320,208});
    Bytes vstrh=Bytes{'v','i','d','s','I','V','5','0'};
    for(uint32_t v:{0u,0u,0u,1u,25u,0u,3u,0u,0u,0u}) put(vstrh,v);
    Bytes bih=words({40,320,208});bih.push_back(1);bih.push_back(0);bih.push_back(24);bih.push_back(0);
    bih.insert(bih.end(),{'I','V','5','0'});for(int i=0;i<5;++i) put(bih,0);
    Bytes astrh=Bytes{'a','u','d','s',0,0,0,0};
    for(uint32_t v:{0u,0u,0u,2u,44100u,0u,4u,0u,0u,0u}) put(astrh,v);
    Bytes wave{1,0,1,0};put(wave,22050);put(wave,44100);wave.insert(wave.end(),{2,0,16,0});
    const Bytes hdrl=list("hdrl",{chunk("avih",avih),list("strl",{chunk("strh",vstrh),chunk("strf",bih)}),
                                 list("strl",{chunk("strh",astrh),chunk("strf",wave)})});
    // Frame 1 is a dropped (empty) chunk; frame 2 sits in a rec list and
    // has an odd size that is padded.
    const Bytes movi=list("movi",{chunk("00dc",{1,2,3,4}),chunk("01wb",{9,9}),chunk("00dc",{}),
                                 list("rec ",{chunk("00pc",{0,0,0,0}),chunk("00dc",{5,6,7})}),chunk("01wb",{8,8})});
    Bytes riff{'A','V','I',' '};riff.insert(riff.end(),hdrl.begin(),hdrl.end());riff.insert(riff.end(),movi.begin(),movi.end());
    return chunk("RIFF",riff);
}
void test_avi() {
    const wpc::Avi avi=wpc::parse_avi(sample());
    require(avi.microseconds_per_frame==40000 && avi.total_frames==3,"avih frame timing");
    require(avi.streams.size()==2 && avi.first_video()==0,"strl streams");
    const auto& v=avi.streams[0];
    require(v.video() && v.rate==25 && v.scale==1 && v.length==3,"video strh");
    require(v.width==320 && v.height==208 && v.bit_count==24 && std::memcmp(&v.compression,"IV50",4)==0,"BITMAPINFOHEADER");
    const auto& a=avi.streams[1];
    require(a.audio() && a.channels==1 && a.samples_per_second==22050 && a.bits_per_sample==16,"WAVEFORMATEX");
    require(avi.chunks[0].size()==3 && avi.chunks[1].size()==2,"movi chunks per stream, palette chunks excluded");
    require(avi.chunks[0][1].size==0,"dropped frame kept in order");
    const auto& odd=avi.chunks[0][2];
    require(odd.size==3 && avi.bytes[odd.offset]==5 && avi.bytes[odd.offset+2]==7,"rec list and odd padding");
    require(avi.bytes[avi.chunks[1][1].offset]==8,"chunk after padded rec list");
    Bytes bad=sample();bad[8]='W';
    rejected([&]{wpc::parse_avi(bad);},"non-AVI RIFF accepted");
    bad=sample();bad[4]=0xff;bad[5]=0xff;
    rejected([&]{wpc::parse_avi(bad);},"oversized RIFF accepted");
    bad=sample();bad.resize(bad.size()-6);
    rejected([&]{wpc::parse_avi(bad);},"truncated movi accepted");
    std::string reason;
    Bytes cvid=sample();
    const wpc::Avi other=wpc::parse_avi(cvid);
    wpc::AviStream audio=other.streams[1];
    require(!wpc::create_video_decoder(audio,reason) && !reason.empty(),"non-video stream rejected");
}
void test_color() {
    // 8x4 picture: two chroma samples per row of 4x4 blocks. Expected values
    // from ir50_32's MMX converter (0x100641a0), worked by hand.
    uint8_t y[32],u[2]={128,90},v[2]={128,240},out[8*4*4];
    for(int i=0;i<32;++i) y[i]=(i%8)<4?235:82;
    wpc::yuv410_to_rgba(y,8,u,2,v,2,8,4,out);
    const auto at=[&](int x,int r) {const uint8_t* p=out+(size_t(r)*8+size_t(x))*4;return std::array<int,4>{{p[0],p[1],p[2],p[3]}};};
    require(at(0,0)==std::array<int,4>{{253,255,254,255}},"studio white: (219 x 149 + 204 x 128 - 0x668c) x 2 >> 8 and the others");
    require(at(0,1)==std::array<int,4>{{255,255,252,255}},"the next row takes the 4x4 dither's second row");
    require(at(7,0)==std::array<int,4>{{255,0,4,255}},"the second block's right half is red");
    // x 2..3 take the horizontal midpoint (U 109, V 184), one value per 2 pixels.
    require(at(2,0)==std::array<int,4>{{255,217,215,255}},"chroma halfway between the two samples");
    require(at(1,0)[1]==253 && at(2,0)[1]==217,"pixels 0..1 keep the first sample (dither 0x4c3e), 2..3 the midpoint");
}
}
int main(int argc,char** argv) {
    try {
        test_avi();test_color();
        std::printf("native movie: %zu synthetic checks passed\n",checks);
        for(int i=1;i<argc;++i) {
            const wpc::Avi avi=wpc::load_avi(argv[i]);
            const int stream=avi.first_video();
            require(stream>=0,"corpus movie has video");
            std::string reason;
            auto decoder=wpc::create_video_decoder(avi.streams[size_t(stream)],reason);
            if(!decoder) {std::printf("%s: decoder unavailable: %s\n",argv[i],reason.c_str());continue;}
            for(const auto& c:avi.chunks[size_t(stream)]) decoder->decode(avi.bytes.data()+c.offset,c.size);
            std::printf("%s: decoded %zu frames at %dx%d, %u/%u fps\n",argv[i],avi.chunks[size_t(stream)].size(),
                        decoder->width(),decoder->height(),avi.streams[size_t(stream)].rate,avi.streams[size_t(stream)].scale);
        }
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native movie: %s (%zu checks)\n",e.what(),checks);return 1;}
}
