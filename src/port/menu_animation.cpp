#include "menu_animation.h"
#include <stdexcept>

namespace wpc {
void FilmState::play(int index,int playback) {
    if(index<0 || size_t(index)>=lengths.size() || lengths[size_t(index)]<1)
        throw std::runtime_error("menu film sequence out of range");
    sequence=index;
    last=lengths[size_t(index)]-1;
    first=0;frame=0;mode=playback;
}
void FilmState::activate() {
    active=true;
    if(mode) {accumulator=float(first);done=false;}
}
void FilmState::update() {
    switch(mode) {
        case 1: // play once: hold the last frame and mark completion
            accumulator+=speed;
            frame=int(accumulator);
            if(frame>=last) {done=true;frame=last;}
            break;
        case 2: // loop: past the last frame, back to the first
            accumulator+=speed;
            if(accumulator>float(last)) accumulator=float(first);
            frame=int(accumulator);
            break;
        case 3: // ping-pong, jumping to each end as it turns
            accumulator+=speed;
            if(accumulator>float(last)) {accumulator=float(last);speed=-speed;}
            else if(accumulator<0) {speed=-speed;accumulator=0;}
            frame=int(accumulator);
            break;
        default: break;
    }
}
MenuAnimation::MenuAnimation(FilmState& satanas,FilmState& muttley,FilmState& head)
    :satanas_film_(satanas),muttley_film_(muttley),head_film_(head) {}
void MenuAnimation::speech_started(int actor,int duration60) {
    speech_start_=true;actor_=actor;duration60_=duration60;
}
void MenuAnimation::update() {
    FilmState &body=satanas_film_,&mutt=muttley_film_,&head=head_film_;
    // Idle gestures: the claw (12) and Muttley's laugh (15) start with a
    // 5-in-1000 chance per update and return to the rest poses when done.
    if(satanas_state_==12 && body.frame==body.last) satanas(0);
    if(satanas_state_==0 && random_.next()%1000<5) satanas(12);
    if(muttley_state_==15 && mutt.frame==mutt.last) muttley(13);
    if(muttley_state_==13 && random_.next()%1000<5) muttley(15);
    if(speech_start_) {
        speech_start_=false;
        if(actor_==0) satanas(3);
        if(actor_==1) muttley(14);
        if(actor_==4) muttley(16);
    }
    if(speech_end_) {
        speech_end_=false;
        if(actor_==0) {head.deactivate();satanas(0);body.play(0,2);}
        if(actor_==1) muttley(13);
        if(actor_==4) muttley(13);
    }
    // A finished talk gesture holds the listening pose (1) until the voice ends.
    if(body.sequence>=4 && body.sequence<=11 && body.frame==body.last) satanas(1);
    if(satanas_request_) {
        satanas_request_=false;
        if(satanas_state_==3) {
            // Talking: the head film runs, and the body plays the gesture whose
            // length fits the voice (sequence k lasts about (k-3)*2/3 s).
            head.activate();
            const int v=duration60_>>2;
            int sequence;
            if(v<=10) sequence=4;
            else if(v>85) sequence=11;
            else if(v>=70) sequence=(v+5)/10+2;
            else sequence=(v+5)/10+3;
            body.play(sequence,1);
        } else body.play(satanas_state_,2);
    }
    if(muttley_request_) {muttley_request_=false;mutt.play(muttley_state_,2);}
}
int SpriteMove::update(MsvcRandom& random) {
    x-=step;
    // _ftol (0x4ee040) truncates; at or past the end the car restarts.
    if(int(x)<=end) {x=float(start);roll(random);}
    return int(x);
}
}
