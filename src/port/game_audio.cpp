#include "game_audio.h"
#include "game_files.h"
#include <algorithm>
#include <cctype>

namespace wpc {
namespace {
std::string lower(std::string s) {for(char& c:s) c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
// 0x430e40: the MSVC rand() sequence the game uses for its gates.
int next_random(uint32_t& state) {state=state*0x343fdu+0x269ec3u;return int((state>>16)&0x7fff);}
}
std::string music_file(const std::string& course) {
    std::string name=course;
    std::replace(name.begin(),name.end(),'\\','/');
    const size_t slash=name.find_last_of('/');
    std::string base=slash==std::string::npos?name:name.substr(slash+1);
    const std::string folded=lower(base);
    if(base.empty()) base="Cc_t1.";
    else if(folded.find("dr")!=std::string::npos) {
        static const char* const suffix[]={"_t1","_t2","_t3","_t4","_b"};
        static const char* const fallback[]={"Cc_t1.","Sh_t1.","Dt_t1.","Ct_t1.","Cc_b."};
        for(size_t i=0;i<5;++i) if(folded.find(suffix[i])!=std::string::npos) {base=fallback[i];break;}
    }
    const size_t dot=base.find_last_of('.');
    if(dot!=std::string::npos) base.resize(dot);
    return "Music/"+base+".snd";
}
GameAudio::GameAudio(std::vector<std::filesystem::path> data_roots,std::vector<std::filesystem::path> disc_roots,uint32_t language)
    :data_(std::move(data_roots)),disc_(std::move(disc_roots)),language_(language) {
    output_=std::make_unique<AudioOutput>(mixer_);
}
void GameAudio::play_music(const std::string& course) {
    const std::string name=music_file(course);
    if(name==music_name_ && music_ && mixer_.playing(music_)) return;
    stop_music();
    // The original reads D:\music\...: disc first, then the install folder.
    std::vector<std::filesystem::path> roots=disc_;
    roots.insert(roots.end(),data_.begin(),data_.end());
    auto pcm=std::make_shared<PcmBuffer>(decode_snd(read_file(game_file(roots,name).string())));
    AudioMixer::Params params;params.bus=AudioBus::Music;params.loop=true; // 0x428dc0 wraps the stream
    music_=mixer_.play(std::move(pcm),params);music_name_=name;
}
void GameAudio::stop_music() {
    if(music_) mixer_.stop(music_);
    music_=0;music_name_.clear();
}
void GameAudio::load_menu_bank(const std::string& name,const std::vector<std::filesystem::path>& where) {
    auto bank=std::make_unique<SoundBank>();
    std::vector<std::filesystem::path> roots=where;
    if(roots.empty()) {roots=data_;roots.insert(roots.end(),disc_.begin(),disc_.end());}
    bank->load_sbpc(read_file(game_file(roots,name).string()),language_);
    menu_bank_=std::move(bank);
}
AudioBus sound_bus(const Node& sound) {return sound.integer(0x9d)?AudioBus::Voice:AudioBus::Effects;}
namespace {
uint32_t sample_reference(const Node& sound) {
    const auto ref=sound.properties.find(0x94);
    if(ref==sound.properties.end() || ref->second.size()!=8) return 0;
    const auto& b=ref->second;
    return uint32_t(b[4])|uint32_t(b[5])<<8|uint32_t(b[6])<<16|uint32_t(b[7])<<24;
}
}
uint32_t GameAudio::queue_menu_item(const Node& item,int delay) {
    for(const Node& child:item.children) {
        if(child.class_name!="WMenuSound") continue;
        queued_sound_=&child;queued_delay_=delay;
        return sample_reference(child);
    }
    return 0;
}
void GameAudio::load_menu_effects(const Archive& menu) {
    auto bank=std::make_unique<SoundBank>();
    bank->load_sbnk(menu);
    menu_effects_=std::move(bank);
}
SharedPcm GameAudio::menu_sample(const Node& sound) const {
    const uint32_t ref=sample_reference(sound);
    // References with the high bit select the language bank (Menu.GSW).
    const SoundBank* bank=(ref&0x80000000u)?menu_bank_.get():menu_effects_.get();
    return bank?bank->find(ref):nullptr;
}
void GameAudio::play_menu_sound(const Node& sound,bool only_if_idle) {
    if(only_if_idle && menu_voice_ && mixer_.playing(menu_voice_)) return;
    if(menu_voice_) mixer_.stop(menu_voice_);
    menu_voice_=0;
    if(const SharedPcm pcm=menu_sample(sound)) {
        AudioMixer::Params params;params.bus=sound_bus(sound);params.volume=sound.real(0x97,1);
        menu_voice_=mixer_.play(pcm,params);
    }
}
GameAudio::MenuVoiceEvent GameAudio::menu_voice_update() {
    MenuVoiceEvent event;
    if(queued_sound_ && --queued_delay_<=0) {
        const Node& sound=*queued_sound_;
        queued_sound_=nullptr;
        // One speaking menu character at a time.
        if(menu_voice_) mixer_.stop(menu_voice_);
        menu_voice_=0;
        const SharedPcm pcm=menu_sample(sound);
        if(pcm && pcm->frames()) {
            AudioMixer::Params params;params.bus=sound_bus(sound);params.volume=sound.real(0x97,1);
            menu_voice_=mixer_.play(pcm,params);
            event.started=true;event.sample=sample_reference(sound);
            event.actor=sound.integer(0x1bc,0);
            event.duration60=int(double(pcm->frames())/pcm->rate*60);
            return event;
        }
    }
    if(menu_voice_ && !mixer_.playing(menu_voice_)) {menu_voice_=0;event.finished=true;}
    return event;
}
GameAudio::MenuVoiceEvent GameAudio::speak(const Node& sound) {
    MenuVoiceEvent event;
    stop_speaking();
    const SharedPcm pcm=menu_sample(sound);
    if(!pcm || !pcm->frames()) return event;
    AudioMixer::Params params;params.bus=sound_bus(sound);params.volume=sound.real(0x97,1);
    menu_voice_=mixer_.play(pcm,params);
    event.started=true;event.sample=sample_reference(sound);event.actor=sound.integer(0x1bc,0);
    event.duration60=int(double(pcm->frames())/pcm->rate*60);
    return event;
}
void GameAudio::apply_volume_item(const Node& item,float value) {
    const float maximum=item.real(0x18e,1);
    // 0x426c70 stores the slider value as the bus level.
    const float gain=maximum>0?value/maximum:0;
    switch(item.integer(0x193,-1)) {
        case 0: mixer_.set_bus_volume(AudioBus::Music,gain); break;
        case 1: mixer_.set_bus_volume(AudioBus::Effects,gain); break;
        case 2: mixer_.set_bus_volume(AudioBus::Voice,gain); break;
        default: break;
    }
}
SharedPcm GameAudio::voice_file(const std::string& name) {
    auto found=voices_.find(name);
    if(found!=voices_.end()) return found->second;
    SharedPcm pcm;
    try {pcm=std::make_shared<PcmBuffer>(decode_snd(read_file(game_file(data_,"Voice/"+name).string())));}
    catch(const std::exception&) {pcm=nullptr;} // 34 table names are not installed
    return voices_.emplace(name,pcm).first->second;
}
bool GameAudio::voice(int character,int event,int chance) {
    if(character<7 && voice_ && mixer_.playing(voice_)) return false;
    std::string name;
    if(event>=100 && event<=103) {
        if(narrator_!=uint8_t(event-100)) return false;
        narrator_=uint8_t((event-100+1)%4);
        name="vo-N-90"+std::to_string(event-100)+".snd";
    } else {
        if(character<0 || character>=7 || event<0 || event>=7) return false;
        // Cycle through the four table entries for this pair.
        for(int tries=0;tries<4 && name.empty();++tries) {
            const char* base=voice_table[character][event][variant_[size_t(character)][size_t(event)]];
            variant_[size_t(character)][size_t(event)]=uint8_t((variant_[size_t(character)][size_t(event)]+1)%4);
            if(voice_file(std::string("vo-")+base)) name=std::string("vo-")+base;
        }
        if(name.empty()) return false;
    }
    if(chance!=0xff && chance<=(next_random(random_)%0xff)*3) return false;
    const SharedPcm pcm=voice_file(name);
    if(!pcm) return false;
    AudioMixer::Params params;params.bus=AudioBus::Voice;
    voice_=mixer_.play(pcm,params);
    return voice_!=0;
}
uint32_t GameAudio::effect(const SharedPcm& sound,float volume,float pitch,bool loop,AudioBus bus) {
    AudioMixer::Params params;params.bus=bus;params.volume=volume;params.pitch=pitch;params.loop=loop;
    return mixer_.play(sound,params);
}
}
