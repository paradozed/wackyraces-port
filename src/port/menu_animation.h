#ifndef WPC_MENU_ANIMATION_H
#define WPC_MENU_ANIMATION_H
#include "game_random.h"
#include <cstdint>
#include <vector>

namespace wpc {
// The menu's animation tick: talk sequence k (10..90 frames at speed 0.5)
// lasts the voice length that selects it only at 30 updates per second.
constexpr double menu_update_seconds=1.0/30.0;

// GDFilm2 playback state. Runtime offsets: sequence +0x15a, frame +0x162,
// m_AnimType +0x16a, m_FirstFrame +0x172, m_LastFrame +0x17a, m_Speed +0x182,
// accumulator +0x196, completion +0x19a; the active bit is flag 0x2 of +0xc.
struct FilmState {
    std::vector<int> lengths; // frames in each sequence of the film's FILQ
    int sequence=0,first=0,last=0,mode=0,frame=0;
    float speed=0,accumulator=0;
    bool done=false,active=true;
    // 0x493f10: select a whole sequence; the accumulator is kept.
    void play(int sequence,int mode);
    // 0x41ba60 -> 0x4300d0 and 0x41ba90.
    void activate();
    void deactivate() {active=false;}
    // 0x430110, once per menu update while active.
    void update();
};

// WSpriteMove (the main-menu road cars' action; runtime object 0x2c bytes,
// vtable 0x51448c): the template's m_nStartPosX +0x24 (0x1b3), m_nEndPosX
// +0x2c (0x1b4), m_nRandomIntervallum +0x34 (0x1b5), m_nIntervallumMin +0x3c
// (0x1b6), m_nStepMultiplier +0x44 (0x1b7, a float); defaults 320, 0, 10, 1,
// 1.0 (0x492e50). menu.WGM's seven cars use 320, -380, 1, 3, 1.0, so every
// step is 3 and a car loops in 234 updates.
struct SpriteMove {
    int start=320,end=0,interval=10,minimum=1;
    float multiplier=1;
    float x=0,step=0;   // runtime +0x24, +0x28
    // 0x492f10: from the sprite's X (+0x12a) with a new step.
    void init(int sprite_x,MsvcRandom& random) {x=float(sprite_x);roll(random);}
    // 0x492f80, once per menu update: the X written back to the sprite.
    int update(MsvcRandom& random);
    void roll(MsvcRandom& random) {step=float(random.next()%interval+minimum)*multiplier;}
};

// WMenuAnimation (action on the main menu's Satanas film, update 0x4942b0).
// It drives the Satanas body film, the next sibling (Muttley) and the one
// after it (the Satanas talking head, serialized inactive).
class MenuAnimation {
public:
    MenuAnimation(FilmState& satanas,FilmState& muttley,FilmState& head);
    // A WMenuSound voice began (0x486e80): m_nSpeakingActor (property 0x1bc;
    // 0 Satanas, 1 and 4 Muttley) and its length in 1/60 s (voice +0x20).
    void speech_started(int actor,int duration60);
    // The voice finished (0x486eb8).
    void speech_finished() {speech_end_=true;}
    // 0x486d20/0x486d50 requested from outside (WWinnerMovie's reactions).
    void request(int satanas_state,int muttley_state) {satanas(satanas_state);muttley(muttley_state);}
    void update();
    int satanas_state() const {return satanas_state_;}
    int muttley_state() const {return muttley_state_;}
private:
    void satanas(int state) {satanas_state_=state;satanas_request_=true;} // 0x486d20
    void muttley(int state) {muttley_state_=state;muttley_request_=true;} // 0x486d50
    FilmState &satanas_film_,&muttley_film_,&head_film_;
    MsvcRandom random_;
    // 0x6f41f8 and 0x52bf74 as set up by 0x48fa26: Satanas 0, Muttley 13.
    int satanas_state_=0,muttley_state_=13;
    int actor_=0,duration60_=-1;                    // 0x6f37dc, 0x52bf78
    bool satanas_request_=false,muttley_request_=false; // 0x6f41fc, 0x6f41fd
    bool speech_start_=false,speech_end_=false;     // 0x6f41fe, 0x6f41ff
};
}
#endif
