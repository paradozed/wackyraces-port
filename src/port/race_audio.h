#ifndef WPC_RACE_AUDIO_H
#define WPC_RACE_AUDIO_H
#include "game_audio.h"
#include "race_session.h"

namespace wpc {
// Race sounds driven by session state, following WackyAuto 0x469bf0 and the
// WModManager countdown 0x4a8f3c. The actor's GDSound children are taken in
// order: the first is the engine loop (+0x3bf), the third +0x3c3, and the
// next seven fill +0x3cb..+0x3e3 (+0x3d7 is the wall-hit sample).
class RaceAudio {
public:
    RaceAudio(GameAudio& audio,const Archive& general,const Archive& course,const Scene& shared,const RaceSession& session);
    ~RaceAudio();
    RaceAudio(const RaceAudio&)=delete;
    RaceAudio& operator=(const RaceAudio&)=delete;
    // Call once after each RaceSession::update.
    void update();
    // Sounds started since the last call, for the log.
    std::vector<std::string> take_events() {return std::move(events_);}
private:
    // One car's sounds: its actor's GDSound children, the engine loop
    // (+0x3bf, pitch +0x3bb) and the skid loop (+0x3c3).
    struct Car {
        std::vector<const Node*> sounds;
        uint32_t engine=0,skid=0;
        float min_pitch=1,max_pitch=1,pitch_rate=0,pitch=1; // m_minpitch/m_maxpitch/m_pitchmul, +0x3bb
        uint32_t random=7;
        RaceSession::CarSounds heard{};  // the robots' counters already played
        uint64_t wall_until=0;           // +0x3e7 for the wall sound
    };
    // A robot's one-shot sample of slot, placed at its position.
    void one_shot(const Node& sound,const Vec3& at,const char* name,size_t robot);
    SharedPcm sample(const Node& sound) const;
    void setup(Car& car,int vehicle,const Scene& shared);
    void update_car(Car& car,const RaceCar& state,float top,bool skidding,bool silent);
    // DirectSound3D for m_3D (0x9b) voices: the listener is the follow
    // camera (0x4276ce/0x4276fa), rolloff 5 (0x42730f), the voice's
    // m_MinRange/m_MaxRange (0x427c50).
    void place(uint32_t voice,const Node& sound,const Vec3& at,bool silent);
    GameAudio& audio_;
    const RaceSession& session_;
    SoundBank general_,course_;
    Car player_;
    std::vector<Car> opponents_;
    Matrix4 listener_{};
    SharedPcm splash_,bumm_;
    const Node* splash_sound_=nullptr;const Node* bumm_sound_=nullptr;
    uint64_t cooldown_until_=0;         // +0x3e7, 30 updates
    int impacts_=0,rank_=0;
    bool drowning_=false;
    int bumms_=0,rock_sounds_=0,cactus_sounds_=0,hit_sounds_=0,oil_sounds_=0,pickup_sounds_=0;
    std::vector<std::string> events_;
};
}
#endif
