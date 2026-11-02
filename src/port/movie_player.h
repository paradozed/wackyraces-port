#ifndef WPC_MOVIE_PLAYER_H
#define WPC_MOVIE_PLAYER_H
#include "audio_mixer.h"
#include "movie_decoder.h"
#include "wpc_platform.h"
#include <memory>
#include <string>

namespace wpc {
// Recovered playback loops 0x4ac850 (legal.avi) and 0x4acad0 (intro):
// the shown frame is elapsed_ms * rate / 1000 since the start, playback
// ends when that index reaches the frame count, and an Escape press skips.
// The movie is centered in a surface (640x480 for legal.avi, 320x240 for
// the intro: 0x402e5f) that fills the 4:3 game area, as the display-mode
// change does on the original's monitor.
class MoviePlayer {
public:
    // Returns null and sets reason when the file or its codec is unusable.
    // With a mixer, the movie's own PCM stream plays, or the separate audio
    // file when audio_path is set (0x4026d0's second file: the intro pairs
    // Movies\intro.avi with Voice\iaudio.avi). An unreadable audio file
    // leaves the movie silent rather than failing it.
    static std::unique_ptr<MoviePlayer> open(WpcWindow* window,const std::string& path,
                                             int surface_width,int surface_height,std::string& reason,
                                             AudioMixer* mixer=nullptr,const std::string& audio_path={});
    ~MoviePlayer();
    MoviePlayer(const MoviePlayer&)=delete;
    MoviePlayer& operator=(const MoviePlayer&)=delete;
    void start(double now);
    // Decode up to the frame due at now and upload it. Call outside frame
    // recording. Returns false once the last frame's time has passed.
    bool update(double now);
    void draw();
    size_t frame() const {return shown_;}
    bool has_audio() const {return bool(audio_);}
    size_t frames() const {return frames_;}
private:
    MoviePlayer(WpcWindow* window,Avi avi,int stream,std::unique_ptr<VideoDecoder> decoder,int surface_width,int surface_height);
    double clock(double now);
    AudioMixer* mixer_=nullptr;
    SharedPcm audio_;
    uint32_t voice_=0;
    double audio_end_=0,audio_end_time_=-1;
    WpcWindow* window_;
    Avi avi_;
    int stream_;
    std::unique_ptr<VideoDecoder> decoder_;
    WpcTexture* texture_=nullptr;
    int surface_width_,surface_height_;
    size_t frames_=0,decoded_=0,shown_=0;
    double start_=0;
    bool started_=false,visible_=false;
};
}
#endif
