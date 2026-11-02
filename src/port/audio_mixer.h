#ifndef WPC_AUDIO_MIXER_H
#define WPC_AUDIO_MIXER_H
#include "game_sound.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace wpc {
// Groups with independent gain: the options page sliders (EFFETS, MUSIQUE,
// VOIX) and movie soundtracks.
enum class AudioBus { Effects, Music, Voice, Movie, Count };

// Volume to amplitude (0x427f90, 0x428c50): DirectSound SetVolume(min(v,1)*4000-4000)
// in hundredths of a dB, so 1 is 0 dB, 0.5 is -20 dB and 0 is -40 dB.
inline float directsound_gain(float volume) {
    return float(std::pow(10.0,(std::min(1.f,std::max(0.f,volume))-1.f)*2.0));
}

// DirectSound3D as the original sets it up (provisional law: DirectSound's
// documented model). The distance is held within [min, max] (GDSound
// m_MinRange/m_MaxRange, 0x427c50) and each doubling past min costs
// rolloff x 6 dB: gain = (min / d)^rolloff, the listener's rolloff being 5
// (0x42730f). Pan from the azimuth in the listener frame (X right, Z
// forward), +/-90 degrees to +/-10000 mB on the far side as Wine's dsound.
struct Spatial {float gain=1,pan=0;};
Spatial directsound3d(const std::array<float,16>& listener,const std::array<float,3>& at,float minimum,float maximum,float rolloff=5);

// Software mixer: 44.1 kHz interleaved stereo s16, linear resampling. The
// original hands clips to DirectSound buffers (0x427dc0) and streams .snd
// through a 500 ms timer; this replaces both with one callback. A voice's
// volume and its bus level are DirectSound volumes: the buffer gets
// directsound_gain(min(volume, level)) (0x427f90 caps a sample's volume by
// its slider, 0x428c50 gives a stream the slider itself).
class AudioMixer {
public:
    static constexpr int rate=44100;
    struct Params {
        AudioBus bus=AudioBus::Effects;
        float volume=1,pitch=1,pan=0; // pan -1 left .. +1 right
        float spatial=1;              // DirectSound3D distance gain (linear)
        bool loop=false;
    };
    // Returns a nonzero voice ID.
    uint32_t play(SharedPcm sound,const Params& params);
    void stop(uint32_t voice);
    void stop_bus(AudioBus bus);
    void set_pitch(uint32_t voice,float pitch);
    void set_volume(uint32_t voice,float volume);
    // 3D voices: the distance gain and the pan the listener gives them.
    void set_spatial(uint32_t voice,float gain,float pan);
    bool playing(uint32_t voice) const;
    // Source position of a voice in seconds (movie clock), -1 if stopped.
    double position(uint32_t voice) const;
    // The bus level, 0..1.
    void set_bus_volume(AudioBus bus,float volume);
    float bus_volume(AudioBus bus) const;
    // Fill frames of stereo output (called from the output thread).
    void mix(int16_t* out,size_t frames);
    uint64_t frames_mixed() const;
private:
    struct Voice {
        uint32_t id=0;
        SharedPcm sound;
        Params params;
        double position=0; // in source frames
    };
    mutable std::mutex mutex_;
    std::vector<Voice> voices_;
    std::array<float,size_t(AudioBus::Count)> buses_{{1,1,1,1}};
    std::vector<float> accumulator_;
    uint32_t next_id_=1;
    uint64_t mixed_=0;
};
}
#endif
