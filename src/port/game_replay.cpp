#include "game_replay.h"
#include <stdexcept>

namespace wpc {
namespace {
constexpr size_t replay_size=0x382f9;
uint16_t u16(const std::vector<uint8_t>& b,size_t at) {return uint16_t(b[at]|b[at+1]<<8);}
uint32_t u32(const std::vector<uint8_t>& b,size_t at) {return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
}
Replay Replay::parse(const std::vector<uint8_t>& b) {
    if(b.size()!=replay_size) throw std::runtime_error("replay: a demo/.WSR buffer is 0x382f9 bytes");
    Replay r;
    for(size_t i=0;i<4;++i) r.session[i]=u32(b,4*i);
    r.environment=int(u32(b,0x10));r.course=int(u32(b,0x14));r.flag=b[0x18];
    if(r.environment<0 || r.environment>4 || r.course<0 || r.course>3) throw std::runtime_error("replay: course outside the 5 x 4 table");
    if(r.session[2]>6) throw std::runtime_error("replay: vehicle outside 0..6");
    const size_t length=u16(b,0x382f3);
    if(length%3 || length>0x2716) throw std::runtime_error("replay: controller stream length");
    for(size_t at=0x49;at<0x49+length;at+=3) r.controller.push_back({b[at],u16(b,at+1)});
    r.frames=u16(b,0x382f7);
    return r;
}
std::vector<uint8_t> Replay::serialize() const {
    std::vector<uint8_t> b(replay_size,0);
    const auto put32=[&](size_t at,uint32_t v){for(int i=0;i<4;++i) b[at+size_t(i)]=uint8_t(v>>(8*i));};
    const auto put16=[&](size_t at,uint16_t v){b[at]=uint8_t(v);b[at+1]=uint8_t(v>>8);};
    for(size_t i=0;i<4;++i) put32(4*i,session[i]);
    put32(0x10,uint32_t(environment));put32(0x14,uint32_t(course));b[0x18]=flag;
    if(controller.size()*3>0x2716) throw std::runtime_error("replay: controller stream too long");
    size_t at=0x49;
    for(const auto& [run,w]:controller) {b[at]=run;put16(at+1,w);at+=3;}
    put16(0x382f3,uint16_t(controller.size()*3));
    put16(0x382f7,uint16_t(frames));
    return b;
}
void Replay::record(uint16_t w) {
    if(frames>=40000) return; // the buffer's 40,000-update arrays
    if(!controller.empty() && controller.back().second==w && controller.back().first<255) ++controller.back().first;
    else if(controller.size()*3+3<=0x2716) controller.push_back({1,w});
    else return;
    ++frames;
}
uint16_t Replay::word(const DriveInput& in) {
    return uint16_t((in.accelerate?0x40:0)|(in.brake?0x80:0)|(in.left?0x8000:0)|(in.right?0x2000:0));
}
uint16_t ReplayCursor::next() {
    if(run_==0) {
        if(index_>=replay_.controller.size()) return 0;
        // The byte counter is decremented after use (0x45babf): a run of 0
        // wraps to 256 updates.
        run_=replay_.controller[index_].first ? replay_.controller[index_].first : 256;
        word_=replay_.controller[index_].second&uint16_t(~0x800u);
        ++index_;
    }
    --run_;
    return word_;
}
DriveInput ReplayCursor::input(uint16_t w) {
    DriveInput in;
    in.accelerate=w&0x40;in.brake=w&0x80;in.left=w&0x8000;in.right=w&0x2000;
    return in;
}
}
