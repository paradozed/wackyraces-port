#ifndef WPC_AUDIO_OUTPUT_H
#define WPC_AUDIO_OUTPUT_H
#include "audio_mixer.h"
#include <atomic>
#include <string>
#include <thread>

namespace wpc {
// Feeds an AudioMixer from its own thread. Backends, in order: a WAV file
// when WPC_AUDIO_CAPTURE names one (real-time paced, for headless checks),
// PulseAudio (also served by PipeWire) when built with libpulse-simple, else
// a silent sink that keeps the mixer clock running. WPC_AUDIO=silent forces
// the silent sink.
class AudioOutput {
public:
    explicit AudioOutput(AudioMixer& mixer);
    ~AudioOutput();
    AudioOutput(const AudioOutput&)=delete;
    AudioOutput& operator=(const AudioOutput&)=delete;
    const std::string& backend() const {return backend_;}
private:
    void run();
    AudioMixer& mixer_;
    std::string backend_,capture_;
    void* device_=nullptr;
    std::atomic<bool> running_{true};
    std::thread thread_;
};
}
#endif
