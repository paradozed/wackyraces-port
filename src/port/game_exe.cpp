#include "game_exe.h"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace wpc {
namespace {
uint32_t u32(const std::vector<uint8_t>& b,size_t at) {
    if(at+4>b.size()) throw std::runtime_error("truncated PE image");
    return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
uint16_t u16(const std::vector<uint8_t>& b,size_t at) {
    if(at+2>b.size()) throw std::runtime_error("truncated PE image");
    return uint16_t(b[at]|b[at+1]<<8);
}
}
void ExeImage::load(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) throw std::runtime_error("cannot read "+path.string());
    load_bytes(std::vector<uint8_t>(std::istreambuf_iterator<char>(file),{}));
}
void ExeImage::load_bytes(std::vector<uint8_t> bytes) {
    if(bytes.size()<0x40 || bytes[0]!='M' || bytes[1]!='Z') throw std::runtime_error("not an MZ executable");
    const size_t pe=u32(bytes,0x3c);
    if(u32(bytes,pe)!=0x4550) throw std::runtime_error("no PE header");
    const unsigned count=u16(bytes,pe+6),optional=u16(bytes,pe+20);
    if(u16(bytes,pe+24)!=0x10b) throw std::runtime_error("not a PE32 image");
    std::vector<Section> sections;
    const size_t table=pe+24+optional;
    for(unsigned i=0;i<count;++i) {
        const size_t s=table+i*40;
        sections.push_back({u32(bytes,s+12),u32(bytes,s+16),u32(bytes,s+20)});
    }
    base_=u32(bytes,pe+52);
    sections_=std::move(sections);
    bytes_=std::move(bytes);
}
bool ExeImage::u32_at(uint32_t va,uint32_t& value) const {
    for(const Section& s:sections_) {
        if(va<base_+s.va || va-base_-s.va+4>s.size) continue;
        const size_t at=size_t(s.offset)+(va-base_-s.va);
        if(at+4>bytes_.size()) return false;
        value=uint32_t(bytes_[at])|uint32_t(bytes_[at+1])<<8|uint32_t(bytes_[at+2])<<16|uint32_t(bytes_[at+3])<<24;
        return true;
    }
    return false;
}
std::string ExeImage::string_at(uint32_t va) const {
    for(const Section& s:sections_) {
        if(va<base_+s.va || va-base_-s.va>=s.size) continue;
        size_t at=size_t(s.offset)+(va-base_-s.va);
        const size_t end=std::min(bytes_.size(),size_t(s.offset)+s.size);
        std::string text;
        while(at<end && bytes_[at]) text+=char(bytes_[at++]);
        return at<end?text:std::string();
    }
    return {};
}
}
