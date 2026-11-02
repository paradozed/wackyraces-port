#ifndef WPC_GAME_AUDIO_H
#define WPC_GAME_AUDIO_H
#include "audio_mixer.h"
#include "audio_output.h"
#include "game_scene.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wpc {
extern const char* const voice_table[7][7][4];

// Music file for a course archive, following 0x431690: "music\" plus the
// course's last path component with its extension replaced by ".snd"; dr
// courses fall back to Cc_t1/Sh_t1/Dt_t1/Ct_t1 for _t1.._t4 and Cc_b for _b.
// An empty name gives the menu's default Cc_t1.
std::string music_file(const std::string& course);

// A GDSound's bus (0x427ed2 at play, 0x427f90 every update): GDSound +0x16a
// (property 0x9d) selects the voice slider 0x545320, otherwise the effects
// slider 0x545258. The voice factory also copies it into the voice's flag
// 0x10 (0x44c2f4), which the sound code does not read.
AudioBus sound_bus(const Node& sound);

// The game's sound manager (application +0x183c): music stream, menu and
// race samples, voice-overs, and bus gains from the options sliders.
class GameAudio {
public:
    GameAudio(std::vector<std::filesystem::path> data_roots,std::vector<std::filesystem::path> disc_roots,uint32_t language);
    AudioMixer& mixer() {return mixer_;}
    const std::string& backend() const {return output_->backend();}

    void play_music(const std::string& course);
    void stop_music();

    // Menu.GSW SBPC bank for the language (0x431d87..0x431db1); a winner
    // scene's Win/*.GSW replaces it while that scene runs.
    void load_menu_bank(const std::string& name="Menu.GSW",const std::vector<std::filesystem::path>& roots={});
    // menu.WGM's own SBNK (the cursor click, GSMP 0x7f7).
    void load_menu_effects(const Archive& menu);
    // 0x486ef0 (thunk 0x4041fb): play a menu WMenuSound in the speaking slot.
    // With only_if_idle false (every cursor move, 0x488c29) it cuts off the
    // speaking voice; true leaves a playing voice alone.
    void play_menu_sound(const Node& sound,bool only_if_idle=false);
    // Cursor moved onto item (0x48cd80 -> item select with flag 0, 0x488ba0):
    // the item's WMenuSound child is queued to speak after `delay` menu
    // updates (20 there; 0x486da0), replacing any queued voice. Returns the
    // sample reference, or 0.
    uint32_t queue_menu_item(const Node& item,int delay=20);
    // One menu update of the voice queue (0x486e20): a queued voice whose
    // delay ran out starts, replacing the speaking one; a voice that ended is
    // reported once.
    struct MenuVoiceEvent {
        bool started=false,finished=false;
        int actor=0;      // WMenuSound m_nSpeakingActor, property 0x1bc
        int duration60=0; // voice +0x20 (0x427c00): length in 1/60 s
        uint32_t sample=0;
    };
    MenuVoiceEvent menu_voice_update();
    // 0x486ef0 as WWinnerMovie calls it: the sound speaks at once in the
    // speaking slot (0x6f4200), its length in 1/60 s (0x52bf78) returned.
    MenuVoiceEvent speak(const Node& sound);
    bool speaking() const {return menu_voice_ && mixer_.playing(menu_voice_);}
    // 0x486dd0: stop and release the speaking voice.
    void stop_speaking() {if(menu_voice_) mixer_.stop(menu_voice_);menu_voice_=0;}
    // WMenuSetVolumeItem m_nSoundType 0x193: 0 music, 1 effects, 2 voice.
    void apply_volume_item(const Node& item,float value);

    // 0x426df0: character 0..6 with events 0..6, or 7 with narrator events
    // 100..103 (vo-N-900..903 in strict order). Characters are skipped while
    // another voice-over plays; chance 255 always plays.
    bool voice(int character,int event,int chance=255);
    // Play a bank sample (volume: GDSound 0x97, capped by the bus level).
    uint32_t effect(const SharedPcm& sound,float volume=1,float pitch=1,bool loop=false,AudioBus bus=AudioBus::Effects);
private:
    SharedPcm voice_file(const std::string& name);
    std::vector<std::filesystem::path> data_,disc_;
    uint32_t language_;
    AudioMixer mixer_;
    std::unique_ptr<AudioOutput> output_;
    std::string music_name_;
    uint32_t music_=0,menu_voice_=0,voice_=0;
    const Node* queued_sound_=nullptr; // 0x6f4208
    int queued_delay_=0;               // 0x6f4204
    std::unique_ptr<SoundBank> menu_bank_,menu_effects_;
    SharedPcm menu_sample(const Node& sound) const;
    std::map<std::string,SharedPcm> voices_;
    uint8_t narrator_=0;                       // 0x546331
    std::array<std::array<uint8_t,7>,7> variant_{}; // manager +0xa4 selector words
    uint32_t random_=1;
};
}
#endif
