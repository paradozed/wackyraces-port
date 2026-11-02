#include "game_sound.h"
#include "ima.h"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("sound: "+text);}
uint32_t word(const uint8_t* b,size_t size,size_t at) {
    if(at>size || size-at<4) bad("truncated bank field");
    return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;
}
}
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) bad("cannot open "+path);
    in.seekg(0,std::ios::end);
    const std::streamoff length=in.tellg();
    if(length<0 || length>256*1024*1024) bad("unsupported file size: "+path);
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    in.seekg(0);
    if(length && !in.read(reinterpret_cast<char*>(bytes.data()),length)) bad("cannot read "+path);
    return bytes;
}
SndInfo parse_snd_header(const std::vector<uint8_t>& bytes) {
    if(bytes.size()<4) bad("truncated .snd header");
    SndInfo info;
    info.channels=bytes[0]?2:1;info.ima=bytes[1]!=0;
    info.rate=int(bytes[2]|bytes[3]<<8);
    if(info.rate<4000 || info.rate>48000) bad(".snd sample rate out of range");
    return info;
}
PcmBuffer decode_snd(const std::vector<uint8_t>& bytes) {
    const SndInfo info=parse_snd_header(bytes);
    PcmBuffer pcm;pcm.channels=info.channels;pcm.rate=info.rate;
    const size_t payload=bytes.size()-info.payload;
    if(!info.ima) {
        // Uncompressed streams carry 16-bit samples.
        pcm.samples.resize(payload/2/size_t(info.channels)*size_t(info.channels));
        std::memcpy(pcm.samples.data(),bytes.data()+info.payload,pcm.samples.size()*2);
        return pcm;
    }
    // One byte holds two mono samples or one stereo frame (high nibble first).
    const size_t frames=info.channels==2?payload:payload*2;
    if(frames>size_t(INT32_MAX)) bad(".snd stream too long");
    pcm.samples.resize(frames*size_t(info.channels));
    WpcImaState left{0,0},right{0,0};
    wpc_ima_decode(info.channels==2,reinterpret_cast<const signed char*>(bytes.data()+info.payload),
                   pcm.samples.data(),int(frames),&left,&right);
    return pcm;
}
void SoundBank::load(const uint8_t* data,size_t size) {
    const uint32_t table=word(data,size,0),body=word(data,size,4);
    if(table<16 || uint64_t(table)+body+8>size) bad("bank lengths exceed data");
    const uint8_t* t=data+8;
    group_=word(t,table,0);
    const uint32_t count=word(t,table,8);
    if(table!=16+uint64_t(count)*28) bad("bank table size does not match its sample count");
    const uint8_t* b=t+table;
    samples_.clear();
    for(uint32_t i=0;i<count;++i) {
        const size_t row=16+size_t(i)*28;
        const uint32_t id=word(t,table,row),offset=word(t,table,row+4),length=word(t,table,row+8);
        const uint32_t rate=word(t,table,row+12),flags=word(t,table,row+16);
        if(uint64_t(offset)+length>body) bad("sample "+std::to_string(id)+" exceeds the bank body");
        if(rate<4000 || rate>48000) bad("sample "+std::to_string(id)+" has an unsupported rate");
        if(!(flags&2)) bad("8-bit bank samples are not used by the installed banks");
        auto pcm=std::make_shared<PcmBuffer>();
        pcm->channels=(flags&1)?2:1;pcm->rate=int(rate);
        pcm->samples.resize(length/2/size_t(pcm->channels)*size_t(pcm->channels));
        std::memcpy(pcm->samples.data(),b+offset,pcm->samples.size()*2);
        if(!samples_.emplace(id&0x00ffffff,std::move(pcm)).second) bad("duplicate bank sample "+std::to_string(id));
    }
}
void SoundBank::load_sbpc(const std::vector<uint8_t>& file,uint32_t selection) {
    if(file.size()<8 || std::memcmp(file.data(),"SBPC",4)) bad("not an SBPC sound bank");
    const uint32_t count=word(file.data(),file.size(),4);
    if(count>64) bad("SBPC directory too large");
    for(uint32_t i=0;i<count;++i) {
        // 0x44c510: exact selection ID, then seek to its absolute offset.
        if(word(file.data(),file.size(),8+size_t(i)*8)!=selection) continue;
        const uint32_t offset=word(file.data(),file.size(),12+size_t(i)*8);
        if(offset>=file.size()) bad("SBPC bank offset outside file");
        load(file.data()+offset,file.size()-offset);
        return;
    }
    bad("SBPC has no bank for selection "+std::to_string(selection));
}
void SoundBank::load_sbnk(const Archive& archive) {
    const ByteView b=archive.payload(archive.top("SBNK"));
    load(b.data,b.size);
}
SharedPcm SoundBank::find(uint32_t reference) const {
    if((reference>>24)!=(group_&0xff)) return nullptr;
    const auto found=samples_.find(reference&0x00ffffff);
    return found==samples_.end()?nullptr:found->second;
}
PcmBuffer avi_pcm(const Avi& avi,size_t stream) {
    const AviStream& s=avi.streams.at(stream);
    if(!s.audio() || s.format_tag!=1 || s.bits_per_sample!=16 || (s.channels!=1 && s.channels!=2))
        bad("AVI audio is not 16-bit PCM");
    PcmBuffer pcm;pcm.channels=s.channels;pcm.rate=int(s.samples_per_second);
    size_t bytes=0;
    for(const auto& c:avi.chunks.at(stream)) bytes+=c.size;
    pcm.samples.reserve(bytes/2);
    for(const auto& c:avi.chunks.at(stream)) {
        const size_t n=c.size/2;
        const size_t at=pcm.samples.size();
        pcm.samples.resize(at+n);
        std::memcpy(pcm.samples.data()+at,avi.bytes.data()+c.offset,n*2);
    }
    pcm.samples.resize(pcm.samples.size()/size_t(pcm.channels)*size_t(pcm.channels));
    return pcm;
}
}
