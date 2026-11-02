#include "audio_output.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#if WPC_HAVE_PULSE
#include <pulse/error.h>
#include <pulse/simple.h>
#endif

namespace wpc {
namespace {
constexpr size_t period=AudioMixer::rate/50; // 20 ms
void put16(FILE* f,uint16_t v) {std::fputc(v&255,f);std::fputc(v>>8,f);}
void put32(FILE* f,uint32_t v) {put16(f,uint16_t(v));put16(f,uint16_t(v>>16));}
void wav_header(FILE* f,uint32_t data_bytes) {
    std::fwrite("RIFF",1,4,f);put32(f,36+data_bytes);std::fwrite("WAVEfmt ",1,8,f);
    put32(f,16);put16(f,1);put16(f,2);put32(f,AudioMixer::rate);put32(f,AudioMixer::rate*4);put16(f,4);put16(f,16);
    std::fwrite("data",1,4,f);put32(f,data_bytes);
}
}
AudioOutput::AudioOutput(AudioMixer& mixer):mixer_(mixer) {
    if(const char* path=std::getenv("WPC_AUDIO_CAPTURE")) {capture_=path;backend_="capture "+capture_;}
    const char* mode=std::getenv("WPC_AUDIO");
    const bool silent=mode && std::string(mode)=="silent";
    if(silent && capture_.empty()) backend_="silent (WPC_AUDIO=silent)";
#if WPC_HAVE_PULSE
    if(capture_.empty() && !silent) {
        pa_sample_spec spec{PA_SAMPLE_S16LE,uint32_t(AudioMixer::rate),2};
        pa_buffer_attr attr{};
        attr.maxlength=uint32_t(-1);attr.tlength=uint32_t(period*4*4);attr.prebuf=uint32_t(-1);
        attr.minreq=uint32_t(-1);attr.fragsize=uint32_t(-1);
        int error=0;
        device_=pa_simple_new(nullptr,"Wacky Races",PA_STREAM_PLAYBACK,nullptr,"game",&spec,nullptr,&attr,&error);
        backend_=device_?"PulseAudio":std::string("silent (PulseAudio: ")+pa_strerror(error)+")";
    }
#endif
    if(backend_.empty()) backend_="silent (built without libpulse-simple)";
    thread_=std::thread([this]{run();});
}
AudioOutput::~AudioOutput() {
    running_=false;
    if(thread_.joinable()) thread_.join();
#if WPC_HAVE_PULSE
    if(device_) pa_simple_free(static_cast<pa_simple*>(device_));
#endif
}
void AudioOutput::run() {
    std::vector<int16_t> buffer(period*2);
    FILE* file=capture_.empty()?nullptr:std::fopen(capture_.c_str(),"wb");
    uint64_t written=0;
    if(file) wav_header(file,0);
    auto next=std::chrono::steady_clock::now();
    while(running_) {
        mixer_.mix(buffer.data(),period);
#if WPC_HAVE_PULSE
        if(device_) {
            int error=0;
            // Blocks until the server accepts the period, pacing the mixer.
            if(pa_simple_write(static_cast<pa_simple*>(device_),buffer.data(),buffer.size()*2,&error)<0) {
                pa_simple_free(static_cast<pa_simple*>(device_));device_=nullptr;
            }
            continue;
        }
#endif
        if(file) {
            std::fwrite(buffer.data(),2,buffer.size(),file);written+=buffer.size()*2;
            // Keep the header valid every second, so an interrupted run still
            // leaves a playable capture.
            if(written%(AudioMixer::rate*4)==0) {
                std::fseek(file,0,SEEK_SET);wav_header(file,uint32_t(written));
                std::fseek(file,0,SEEK_END);std::fflush(file);
            }
        }
        next+=std::chrono::milliseconds(20);
        std::this_thread::sleep_until(next);
    }
    if(file) {std::fseek(file,0,SEEK_SET);wav_header(file,uint32_t(written));std::fclose(file);}
}
}
