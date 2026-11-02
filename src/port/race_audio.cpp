#include "race_audio.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace wpc {
namespace {
uint32_t sample_reference(const Node& sound) {
    const auto ref=sound.properties.find(0x94); // m_SampleID: "GSMP" + reference
    if(ref==sound.properties.end() || ref->second.size()!=8) return 0;
    const auto& b=ref->second;
    return uint32_t(b[4])|uint32_t(b[5])<<8|uint32_t(b[6])<<16|uint32_t(b[7])<<24;
}
const Node* find_class(const Node& n,const std::string& name) {
    if(n.class_name==name) return &n;
    for(const auto& c:n.children) if(const Node* f=find_class(c,name)) return f;
    return nullptr;
}
}
RaceAudio::RaceAudio(GameAudio& audio,const Archive& general,const Archive& course,const Scene& shared,const RaceSession& session)
    :audio_(audio),session_(session) {
    general_.load_sbnk(general);
    course_.load_sbnk(course);
    setup(player_,session.player().vehicle,shared);
    if(player_.engine) events_.push_back("engine loop");
    // Every WackyAuto runs 0x469bf0 and 0x4705ca: the robots' engines loop
    // too, placed in 3D.
    for(const auto& o:session.opponents()) {opponents_.emplace_back();setup(opponents_.back(),o.vehicle,shared);}
    if(const Node* splash=find_class(shared.root(),"WSplash"))
        for(const Node& child:splash->children) if(child.class_name=="GDSound") {splash_=sample(child);splash_sound_=&child;break;}
    // WKBumm's init (0x486960) plays its GDSound child.
    if(const Node* bumm=find_class(shared.root(),"WKBumm"))
        for(const Node& child:bumm->children) if(child.class_name=="GDSound") {bumm_=sample(child);bumm_sound_=&child;break;}
    rank_=session.rank();
    listener_=session.camera();
}
RaceAudio::~RaceAudio() {
    for(Car* c:{&player_}) {if(c->engine) audio_.mixer().stop(c->engine);if(c->skid) audio_.mixer().stop(c->skid);}
    for(Car& c:opponents_) {if(c.engine) audio_.mixer().stop(c.engine);if(c.skid) audio_.mixer().stop(c.skid);}
}
void RaceAudio::setup(Car& car,int vehicle,const Scene& shared) {
    const RaceTemplates templates=load_race_templates(shared.root());
    const Node& actor=shared.find(templates.vehicles.at(size_t(vehicle)).actor_id);
    for(const Node& child:actor.children) if(child.class_name=="GDSound") car.sounds.push_back(&child);
    car.min_pitch=actor.real(0xf6,1);car.max_pitch=actor.real(0xf7,1);car.pitch_rate=actor.real(0xf8,0);
    car.pitch=car.min_pitch;car.random=7u+unsigned(vehicle);
    // The engine voice is created at actor setup and loops for the race.
    if(!car.sounds.empty())
        if(const SharedPcm engine=sample(*car.sounds[0]))
            car.engine=audio_.effect(engine,car.sounds[0]->real(0x97,1),car.pitch,car.sounds[0]->integer(0x9a)!=0,sound_bus(*car.sounds[0]));
}
SharedPcm RaceAudio::sample(const Node& sound) const {
    const uint32_t reference=sample_reference(sound);
    if(SharedPcm pcm=general_.find(reference)) return pcm;
    return course_.find(reference);
}
void RaceAudio::place(uint32_t voice,const Node& sound,const Vec3& at,bool silent) {
    if(!voice) return;
    if(silent) {audio_.mixer().set_spatial(voice,0,0);return;}
    if(sound.integer(0x9b)==0) {audio_.mixer().set_spatial(voice,1,0);return;}
    const Spatial sp=directsound3d(listener_,at,sound.real(0x95,1000),sound.real(0x96,10000));
    audio_.mixer().set_spatial(voice,sp.gain,sp.pan);
}
void RaceAudio::one_shot(const Node& sound,const Vec3& at,const char* name,size_t robot) {
    const SharedPcm pcm=sample(sound);
    if(!pcm) return;
    const uint32_t voice=audio_.effect(pcm,sound.real(0x97,1),1,false,sound_bus(sound));
    place(voice,sound,at,false);
    events_.push_back("robot "+std::to_string(robot)+" "+name);
}
void RaceAudio::update_car(Car& car,const RaceCar& state,float top,bool skidding,bool silent) {
    // Engine pitch (0x4705ca): target from |speed/top| between m_minpitch and
    // m_maxpitch, with +/-0.3 jitter of that ratio within 0.15 of top speed,
    // then +0x3bb += (target - +0x3bb) * m_pitchmul.
    if(car.engine) {
        const float speed=state.speed.speed;
        float ratio=std::abs(speed/top);
        if(top-speed<0.15f) {
            car.random=car.random*0x343fdu+0x269ec3u;
            ratio+=float(int((car.random>>16)&0x7fff)%20-10)*0.03f;
        }
        const float target=(car.max_pitch-car.min_pitch)*ratio+car.min_pitch;
        car.pitch+=(target-car.pitch)*car.pitch_rate;
        audio_.mixer().set_pitch(car.engine,silent?0.05f:car.pitch);
        place(car.engine,*car.sounds[0],state.position,silent);
    }
    // 0x4705f6..0x470644: the third GDSound (+0x3c3) plays while +0x25f
    // (brake skid) or +0x263 (slide) is set and stops otherwise.
    if(car.sounds.size()>2) {
        const Node& sound=*car.sounds[2];
        if(skidding && !silent) {
            if(!car.skid || !audio_.mixer().playing(car.skid))
                if(const SharedPcm pcm=sample(sound)) car.skid=audio_.effect(pcm,sound.real(0x97,1),1,sound.integer(0x9a)!=0,sound_bus(sound));
            place(car.skid,sound,state.position,false);
        } else if(car.skid) {audio_.mixer().stop(car.skid);car.skid=0;}
    }
}
void RaceAudio::update() {
    const RaceSession& s=session_;
    // Countdown narrator (0x4a8f3c): while PRET!, n = max(1, 3 - counter*3/60)
    // asks for event 0x67-n each update; PARTEZ! asks for 0x67. The voice
    // sequence byte lets each of vo-N-900..903 play once, in order.
    if(s.phase()==RaceSession::Phase::Ready) {
        const int n=std::max<int>(1,3-int(s.phase_updates()*3/60));
        if(audio_.voice(7,0x67-n)) events_.push_back("narrator "+std::to_string(n));
    } else if(s.phase()==RaceSession::Phase::Go && audio_.voice(7,0x67)) events_.push_back("narrator GO");
    listener_=s.camera();
    update_car(player_,s.player(),s.tuning().top_speed,s.skidding(),s.drowning());
    for(size_t i=0;i<opponents_.size() && i<s.opponents().size();++i) {
        const auto& o=s.opponents()[i];
        update_car(opponents_[i],o.car,o.tuning.top_speed,s.opponent_skidding(i),s.opponent_hidden(i) || s.opponent_drowning(i));
    }
    // Voice events 4 (car contact) and 5 (rock) of every car, chance 255;
    // the voice player skips them while another voice speaks.
    for(const auto& [vehicle,event]:s.voice_events())
        if(audio_.voice(vehicle,event)) events_.push_back("voice "+std::to_string(vehicle)+" event "+std::to_string(event));
    // Wall contact (0x46c64b): sample +0x3d7 unless one played in the last
    // 30 updates (+0x3e7).
    if(s.impacts()!=impacts_) {
        impacts_=s.impacts();
        if(s.updates()>=cooldown_until_ && player_.sounds.size()>6) {
            if(const SharedPcm hit=sample(*player_.sounds[6])) {audio_.effect(hit,player_.sounds[6]->real(0x97,1),1,false,sound_bus(*player_.sounds[6]));events_.push_back("wall hit");}
            cooldown_until_=s.updates()+30;
        }
    }
    if(s.drowning() && !drowning_ && splash_) {audio_.effect(splash_,splash_sound_->real(0x97,1),1,false,sound_bus(*splash_sound_));events_.push_back("splash");}
    drowning_=s.drowning();
    if(s.bumms()>bumms_ && bumm_) {audio_.effect(bumm_,bumm_sound_->real(0x97,1),1,false,sound_bus(*bumm_sound_));events_.push_back("bumm");}
    bumms_=s.bumms();
    // 0x46aa11 (rock, +0x3d3) and 0x46aa86 (cactus, +0x3cf): the session
    // counts the hits whose 30-update timer +0x3e7 was free.
    const auto contact=[&](int count,int& seen,size_t slot,const char* name) {
        if(count>seen && player_.sounds.size()>slot)
            if(const SharedPcm hit=sample(*player_.sounds[slot])) {audio_.effect(hit,player_.sounds[slot]->real(0x97,1),1,false,sound_bus(*player_.sounds[slot]));events_.push_back(name);}
        seen=count;
    };
    contact(s.rock_sounds(),rock_sounds_,5,"rock hit");
    contact(s.cactus_sounds(),cactus_sounds_,4,"cactus hit");
    contact(s.hit_sounds(),hit_sounds_,9,"weapon hit"); // +0x3e3, the tenth GDSound
    contact(s.oil_sounds(),oil_sounds_,3,"oil slide");  // +0x3cb (0x4de145)
    contact(s.pickup_sounds(),pickup_sounds_,1,"pickup"); // the second GDSound (0x46ab2c)
    // The robots run the same WackyAuto contacts (0x46c64b, 0x46aa11,
    // 0x46aa86, 0x46aa58), heard through DirectSound3D at their position;
    // hidden robots (far mode) make none.
    for(size_t i=0;i<opponents_.size() && i<s.opponents().size();++i) {
        Car& car=opponents_[i];
        const RaceSession::CarSounds now=s.opponent_sounds(i);
        const Vec3& at=s.opponents()[i].car.position;
        const bool quiet=s.opponent_hidden(i);
        if(!quiet && now.impacts!=car.heard.impacts && s.updates()>=car.wall_until && car.sounds.size()>6) {
            one_shot(*car.sounds[6],at,"wall hit",i);car.wall_until=s.updates()+30;
        }
        if(!quiet && now.rocks>car.heard.rocks && car.sounds.size()>5) one_shot(*car.sounds[5],at,"rock hit",i);
        if(!quiet && now.hits>car.heard.hits && car.sounds.size()>4) one_shot(*car.sounds[4],at,"hit",i);
        if(!quiet && now.bumms>car.heard.bumms && bumm_sound_) one_shot(*bumm_sound_,at,"bumm",i);
        if(!quiet && now.bombs>car.heard.bombs && car.sounds.size()>9) one_shot(*car.sounds[9],at,"weapon hit",i);
        if(!quiet && now.oils>car.heard.oils && car.sounds.size()>3) one_shot(*car.sounds[3],at,"oil slide",i);
        if(now.pickups>car.heard.pickups && car.sounds.size()>1) one_shot(*car.sounds[1],at,"pickup",i);
        car.heard=now;
    }
    // Leader change (0x4a8b7d): the new leader speaks voice event 0.
    const int rank=s.rank();
    if(rank==1 && rank_!=1 && s.phase()==RaceSession::Phase::Racing && audio_.voice(s.player().vehicle,0))
        events_.push_back("leader voice");
    rank_=rank;
}
}
