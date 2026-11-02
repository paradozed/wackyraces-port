#ifndef WPC_GAME_SOUND_H
#define WPC_GAME_SOUND_H
#include "game_assets.h"
#include "game_avi.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wpc {
// Interleaved signed 16-bit PCM.
struct PcmBuffer {
    std::vector<int16_t> samples;
    int channels=1;
    int rate=22050;
    size_t frames() const {return channels>0?samples.size()/size_t(channels):0;}
};
using SharedPcm=std::shared_ptr<const PcmBuffer>;

// .snd stream header (0x428790): channels-1, IMA flag, u16 sample rate.
struct SndInfo {
    int channels=1,rate=22050;
    bool ima=false;
    size_t payload=4;
};
SndInfo parse_snd_header(const std::vector<uint8_t>& bytes);
// Whole-file decode with the recovered 0x40bdd0 IMA routine (or raw PCM).
PcmBuffer decode_snd(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> read_file(const std::string& path);

// SBPC (.GSW) and WGM SBNK banks: rows (id, offset, length, rate, flags,
// x, y). Flags bit 0 = stereo, bit 1 = 16-bit (0x427dc0); every installed
// row is 16-bit mono. Lookup uses the reference's low 24 bits within the
// bank group held in its high byte (0x44c640).
class SoundBank {
public:
    // Bank bytes start at the table_size word.
    void load(const uint8_t* data,size_t size);
    // Selects the SBPC directory entry for a language ID.
    void load_sbpc(const std::vector<uint8_t>& file,uint32_t selection);
    void load_sbnk(const Archive& archive);
    uint32_t group() const {return group_;}
    // Null when the reference is not in this bank.
    SharedPcm find(uint32_t reference) const;
    size_t size() const {return samples_.size();}
private:
    uint32_t group_=0;
    std::map<uint32_t,SharedPcm> samples_;
};

// An AVI audio stream (PCM WAVEFORMAT tag 1) as one buffer.
PcmBuffer avi_pcm(const Avi& avi,size_t stream);
}
#endif
