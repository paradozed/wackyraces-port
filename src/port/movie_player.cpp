#include "movie_player.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wpc {
std::unique_ptr<MoviePlayer> MoviePlayer::open(WpcWindow* window,const std::string& path,int surface_width,int surface_height,
                                               std::string& reason,AudioMixer* mixer,const std::string& audio_path) {
    try {
        Avi avi=load_avi(path);
        const int stream=avi.first_video();
        if(stream<0) {reason="no video stream";return nullptr;}
        auto decoder=create_video_decoder(avi.streams[size_t(stream)],reason);
        if(!decoder) return nullptr;
        if(decoder->width()>surface_width || decoder->height()>surface_height) {reason="movie is larger than its surface";return nullptr;}
        SharedPcm audio;
        if(mixer) {
            try {
                if(!audio_path.empty()) {
                    const Avi track=load_avi(audio_path);
                    for(size_t i=0;i<track.streams.size();++i)
                        if(track.streams[i].audio()) {audio=std::make_shared<PcmBuffer>(avi_pcm(track,i));break;}
                } else {
                    for(size_t i=0;i<avi.streams.size();++i)
                        if(avi.streams[i].audio()) {audio=std::make_shared<PcmBuffer>(avi_pcm(avi,i));break;}
                }
            } catch(const std::exception&) {audio=nullptr;}
        }
        std::unique_ptr<MoviePlayer> player(new MoviePlayer(window,std::move(avi),stream,std::move(decoder),surface_width,surface_height));
        if(audio && audio->frames()) {player->mixer_=mixer;player->audio_=std::move(audio);}
        return player;
    } catch(const std::exception& e) {reason=e.what();return nullptr;}
}
MoviePlayer::MoviePlayer(WpcWindow* window,Avi avi,int stream,std::unique_ptr<VideoDecoder> decoder,int surface_width,int surface_height)
    :window_(window),avi_(std::move(avi)),stream_(stream),decoder_(std::move(decoder)),
     surface_width_(surface_width),surface_height_(surface_height) {
    frames_=avi_.chunks[size_t(stream_)].size();
    if(!frames_) throw std::runtime_error("movie has no frames");
    texture_=wpc_vk_texture_create(window_,uint32_t(decoder_->width()),uint32_t(decoder_->height()),decoder_->rgba().data());
    if(!texture_) throw std::runtime_error("movie texture allocation failed");
}
MoviePlayer::~MoviePlayer() {
    if(voice_) mixer_->stop(voice_);
    if(texture_) wpc_vk_texture_destroy(window_,texture_);
}
void MoviePlayer::start(double now) {
    start_=now;started_=true;
    if(audio_) {
        AudioMixer::Params params;params.bus=AudioBus::Movie;
        voice_=mixer_->play(audio_,params);
        audio_end_=double(audio_->frames())/audio_->rate;
    }
}
double MoviePlayer::clock(double now) {
    // 0x4acad0 with audio (0x53f4e7) times frames from the soundtrack's start;
    // here the mixed soundtrack position is the clock while it lasts.
    if(voice_) {
        const double position=mixer_->position(voice_);
        if(position>=0) return position;
        voice_=0;audio_end_time_=now;
    }
    if(audio_end_time_>=0) return audio_end_+(now-audio_end_time_);
    return now-start_;
}
bool MoviePlayer::update(double now) {
    if(!started_) start(now);
    const AviStream& s=avi_.streams[size_t(stream_)];
    const double due=std::floor(std::max(0.0,clock(now))*double(s.rate)/double(s.scale));
    if(due>=double(frames_)) return false;
    const size_t target=size_t(due);
    // Inter frames depend on their predecessors: decode every frame in
    // order, uploading only the one that is due.
    while(decoded_<=target) {
        const AviChunk& chunk=avi_.chunks[size_t(stream_)][decoded_];
        decoder_->decode(avi_.bytes.data()+chunk.offset,chunk.size);
        ++decoded_;
    }
    if(!visible_ || shown_!=target) {
        if(!wpc_vk_texture_update(window_,texture_,decoder_->rgba().data())) throw std::runtime_error("movie frame upload failed");
        shown_=target;visible_=true;
    }
    return true;
}
void MoviePlayer::draw() {
    int width,height;wpc_window_get_size(window_,&width,&height);
    wpc_vk_clear(window_,0,0,0);
    if(!visible_ || width<=0 || height<=0) return;
    const float scale=std::min(width/640.f,height/480.f);
    const float x0=(width-640*scale)/2,y0=(height-480*scale)/2;
    // Surface pixels to window pixels.
    const float sx=640*scale/surface_width_,sy=480*scale/surface_height_;
    const int fx=(surface_width_-decoder_->width())/2,fy=(surface_height_-decoder_->height())/2;
    wpc_vk_draw_texture(window_,int(std::lround(x0+fx*sx)),int(std::lround(y0+fy*sy)),
                        int(std::lround(decoder_->width()*sx)),int(std::lround(decoder_->height()*sy)),texture_);
}
}
