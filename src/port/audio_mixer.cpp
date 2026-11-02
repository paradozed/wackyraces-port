#include "audio_mixer.h"
#include <algorithm>
#include <cmath>

namespace wpc {
Spatial directsound3d(const std::array<float,16>& listener,const std::array<float,3>& at,float minimum,float maximum,float rolloff) {
    minimum=std::max(1.f,minimum);maximum=std::max(minimum,maximum);
    std::array<float,3> v{};for(size_t i=0;i<3;++i) v[i]=at[i]-listener[12+i];
    const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    Spatial out;
    out.gain=std::pow(minimum/std::clamp(length,minimum,maximum),rolloff);
    if(length>1e-3f) {
        const float x=(v[0]*listener[0]+v[1]*listener[1]+v[2]*listener[2])/length,
                    z=(v[0]*listener[8]+v[1]*listener[9]+v[2]*listener[10])/length;
        const float azimuth=std::atan2(x,std::max(std::abs(z),1e-6f)); // behind folds to the front
        const float millibels=std::min(10000.f,std::abs(azimuth)*20000.f/3.14159265f);
        out.pan=std::copysign(1-std::pow(10.f,-millibels/2000.f),azimuth);
    }
    return out;
}
uint32_t AudioMixer::play(SharedPcm sound,const Params& params) {
    if(!sound || !sound->frames() || sound->rate<=0 || (sound->channels!=1 && sound->channels!=2)) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    Voice v;v.id=next_id_++;if(!next_id_) next_id_=1;
    v.sound=std::move(sound);v.params=params;
    voices_.push_back(std::move(v));
    return voices_.back().id;
}
void AudioMixer::stop(uint32_t voice) {
    std::lock_guard<std::mutex> lock(mutex_);
    voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[&](const Voice& v){return v.id==voice;}),voices_.end());
}
void AudioMixer::stop_bus(AudioBus bus) {
    std::lock_guard<std::mutex> lock(mutex_);
    voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[&](const Voice& v){return v.params.bus==bus;}),voices_.end());
}
void AudioMixer::set_pitch(uint32_t voice,float pitch) {
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& v:voices_) if(v.id==voice) v.params.pitch=std::clamp(pitch,0.05f,8.f);
}
void AudioMixer::set_volume(uint32_t voice,float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& v:voices_) if(v.id==voice) v.params.volume=std::max(0.f,volume);
}
void AudioMixer::set_spatial(uint32_t voice,float gain,float pan) {
    std::lock_guard<std::mutex> lock(mutex_);
    for(auto& v:voices_) if(v.id==voice) {v.params.spatial=std::clamp(gain,0.f,1.f);v.params.pan=std::clamp(pan,-1.f,1.f);}
}
bool AudioMixer::playing(uint32_t voice) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(voices_.begin(),voices_.end(),[&](const Voice& v){return v.id==voice;});
}
double AudioMixer::position(uint32_t voice) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for(const auto& v:voices_) if(v.id==voice) return v.position/v.sound->rate;
    return -1;
}
void AudioMixer::set_bus_volume(AudioBus bus,float volume) {
    std::lock_guard<std::mutex> lock(mutex_);
    buses_[size_t(bus)]=std::clamp(volume,0.f,1.f);
}
float AudioMixer::bus_volume(AudioBus bus) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buses_[size_t(bus)];
}
uint64_t AudioMixer::frames_mixed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mixed_;
}
void AudioMixer::mix(int16_t* out,size_t frames) {
    std::lock_guard<std::mutex> lock(mutex_);
    accumulator_.assign(frames*2,0.f);
    for(auto& v:voices_) {
        const PcmBuffer& s=*v.sound;
        const size_t length=s.frames();
        const double step=double(s.rate)*v.params.pitch/rate;
        const float gain=directsound_gain(std::min(v.params.volume,buses_[size_t(v.params.bus)]))*v.params.spatial;
        // Attenuation-only pan: the far side drops, the near side stays full.
        const float left=gain*std::min(1.f,1-v.params.pan),right=gain*std::min(1.f,1+v.params.pan);
        for(size_t i=0;i<frames;++i) {
            if(v.position>=double(length)) {
                if(!v.params.loop) break;
                v.position=std::fmod(v.position,double(length));
            }
            const size_t a=size_t(v.position);
            size_t b=a+1;
            if(b>=length) b=v.params.loop?0:a;
            const float t=float(v.position-double(a));
            float l,r;
            if(s.channels==1) {
                l=r=s.samples[a]+(s.samples[b]-s.samples[a])*t;
            } else {
                l=s.samples[a*2]+(s.samples[b*2]-s.samples[a*2])*t;
                r=s.samples[a*2+1]+(s.samples[b*2+1]-s.samples[a*2+1])*t;
            }
            accumulator_[i*2]+=l*left;accumulator_[i*2+1]+=r*right;
            v.position+=step;
        }
    }
    voices_.erase(std::remove_if(voices_.begin(),voices_.end(),[](const Voice& v) {
        return !v.params.loop && v.position>=double(v.sound->frames());
    }),voices_.end());
    for(size_t i=0;i<frames*2;++i) out[i]=int16_t(std::lround(std::clamp(accumulator_[i],-32768.f,32767.f)));
    mixed_+=frames;
}
}
