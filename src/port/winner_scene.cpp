#include "winner_scene.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <stdexcept>

namespace wpc {
namespace {
uint32_t word(ByteView b,size_t offset) {
    if(offset>b.size || b.size-offset<4) throw std::runtime_error("Truncated winner-scene resource");
    const uint8_t* p=b.data+offset;
    return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
int signed_half(const uint8_t* p) {return int16_t(uint16_t(p[0])|uint16_t(p[1])<<8);}
uint32_t link(const Archive& archive,const Chunk& resource,const std::string& tag,size_t index) {
    const ByteView data=archive.payload(resource);
    const uint32_t count=word(data,4);
    if(count>(data.size-8)/8 || data.size!=8+size_t(count)*8 || index>=count)
        throw std::runtime_error("Invalid winner-scene resource link table");
    if(std::string(reinterpret_cast<const char*>(data.data+8+index*8),4)!=tag)
        throw std::runtime_error("Unexpected winner-scene resource link type");
    return word(data,12+index*8);
}
bool is_sound(const Node& n) {return n.class_name=="WMenuSound" || n.class_name=="GDSound";}
}
std::string winner_scene_file(int vehicle) {
    // 0x52bf54: win\wintur.wgm .. win\winmean.wgm by vehicle enum.
    static const char* const names[7]={"WinTur","WinPen","WinRed","WinCrep","WinMobi","WinArmy","WinMean"};
    if(vehicle<0 || vehicle>6) throw std::runtime_error("winner scene: invalid vehicle");
    return std::string("Win/")+names[vehicle]+".WGM";
}
WinnerScene::WinnerScene(WpcWindow* window,GameAudio& audio,const std::string& file,
                         const std::vector<std::pair<std::string,std::string>>& canonical,
                         int vehicle,int environment,int course)
    :window_(window),audio_(audio),vehicle_(vehicle),environment_(environment),course_(course) {
    archive_.load(file);
    scene_.load(archive_,&canonical);
    const Node& world=scene_.root();
    const Node* movie=nullptr;
    for(const Node& a:world.actions) if(a.class_name=="WWinnerMovie") movie=&a;
    if(!movie) throw std::runtime_error("winner scene: no WWinnerMovie");
    type_=movie->integer(0x1b8);move_in_=movie->integer(0x1b9);
    other_=movie->integer(0x1ba);other2_=movie->integer(0x1bb);
    // 0x4949f0: +0x24 = the world's second child (the WMenuItem of sounds),
    // +0x28 its first child; the first child's first film is the winner's.
    if(world.children.size()<2 || world.children[0].children.empty())
        throw std::runtime_error("winner scene: unexpected world layout");
    visual_=&world.children[0];
    group_=&world.children[1];
    sound_index_=0;
    sound_=group_->children.empty() ? nullptr : &group_->children[0];
    if(sound_ && sound_->class_name!="WMenuSound") sound_=nullptr;
    for(const Node& child:visual_->children) {
        Layer layer;layer.node=child.id;
        if(child.class_name=="GDSprite") {
            layer.material=child.link(0x9e,"GMAT");material(layer.material);
            layer.x=child.integer(0x9f);layer.y=child.integer(0xa0);layer.priority=child.integer(0xa3);
        } else if(child.class_name=="GDFilm2") {
            load_film(child);layer.film=true;
            layer.x=child.integer(0x60);layer.y=child.integer(0x61);layer.priority=child.integer(0x62);
            if(!car_) car_=child.id;
        } else continue;
        layers_.push_back(layer);
    }
    std::stable_sort(layers_.begin(),layers_.end(),[](const Layer& a,const Layer& b){return a.priority>b.priority;});
    if(!car_) throw std::runtime_error("winner scene: no winner film");
    // 0x4949f0: type 1 (DickOff) leaves the first film as serialized.
    if(type_!=1) films_.at(car_).state.play(0,2);
    // WMenuAnimation on a film drives it and the next two films (0x4942b0);
    // 0x4949f0 resets the shared states to Satanas 0, Muttley 13.
    for(size_t i=0;i<visual_->children.size();++i) {
        const Node& c=visual_->children[i];
        bool owner=false;
        for(const Node& a:c.actions) owner|=a.class_name=="WMenuAnimation";
        if(!owner) continue;
        if(i+2>=visual_->children.size() || !films_.count(c.id) || !films_.count(visual_->children[i+1].id) || !films_.count(visual_->children[i+2].id))
            throw std::runtime_error("winner scene: WMenuAnimation needs two following films");
        animation_=std::make_unique<MenuAnimation>(films_.at(c.id).state,films_.at(visual_->children[i+1].id).state,films_.at(visual_->children[i+2].id).state);
    }
    for(size_t i=0;i<3;++i) clear_[i]=std::clamp(scene_.screen().integer(0xc5+uint32_t(i)),0,255)/255.f;
    // 0x431d87: the scene's own SBPC bank, next to the WGM.
    const std::filesystem::path wgm(file);
    audio_.load_menu_bank(wgm.stem().string()+".GSW",{wgm.parent_path()});
}
WinnerScene::~WinnerScene() {
    audio_.stop_speaking();
    for(const auto& [id,texture]:textures_) if(texture.gpu) wpc_vk_texture_destroy(window_,texture.gpu);
    try {audio_.load_menu_bank();} catch(const std::exception&) {}
}
WinnerScene::Texture& WinnerScene::material(uint32_t id) {
    auto found=materials_.find(id);
    const uint32_t texture_id=found!=materials_.end() ? found->second : (materials_[id]=archive_.material_texture(id));
    auto it=textures_.find(texture_id);
    if(it!=textures_.end()) return it->second;
    const Image decoded=archive_.texture(texture_id);
    Texture texture;texture.width=int(decoded.width);texture.height=int(decoded.height);
    if(!decoded.rgba.empty()) {
        texture.gpu=wpc_vk_texture_create(window_,decoded.width,decoded.height,decoded.rgba.data());
        if(!texture.gpu) throw std::runtime_error("Cannot upload winner-scene texture "+std::to_string(texture_id));
    }
    return textures_.emplace(texture_id,texture).first->second;
}
std::vector<WinnerScene::Placement> WinnerScene::placements(const Chunk& resource) {
    const ByteView data=archive_.payload(resource);
    const uint32_t count=word(data,4);
    if(count>(data.size-8)/12 || data.size!=8+size_t(count)*12) throw std::runtime_error("Invalid winner-scene placement table");
    std::vector<Placement> result;
    for(uint32_t i=0;i<count;++i) {
        const uint8_t* p=data.data+8+size_t(i)*12;
        if(std::string(reinterpret_cast<const char*>(p+4),4)!="GMAT") throw std::runtime_error("Invalid winner-scene placement material");
        Placement placement{signed_half(p),signed_half(p+2),word(data,16+size_t(i)*12)};
        material(placement.material);result.push_back(placement);
    }
    return result;
}
void WinnerScene::load_film(const Node& node) {
    // As the menu's GDFilm2 (MenuView): FILQ sequences of REEQ frames of
    // FRMQ pieces, drawn back to front; 0x58/0x5a/0x5b/0x5c/0x5d playback.
    const Chunk& filq=archive_.find("FILQ",node.link(0x51,"FILQ"));
    const uint32_t sequences=word(archive_.payload(filq),4);
    if(sequences>64) throw std::runtime_error("Invalid winner-scene film sequence count");
    Film film;
    for(uint32_t i=0;i<sequences;++i) {
        const Chunk& reeq=archive_.find("REEQ",link(archive_,filq,"REEQ",i));
        const uint32_t count=word(archive_.payload(reeq),4);
        if(count>10000) throw std::runtime_error("Invalid winner-scene film frame count");
        std::vector<std::vector<Placement>> frames;
        for(uint32_t f=0;f<count;++f) {
            frames.push_back(placements(archive_.find("FRMQ",link(archive_,reeq,"FRMQ",f))));
            std::reverse(frames.back().begin(),frames.back().end());
        }
        film.state.lengths.push_back(int(count));
        film.sequences.push_back(std::move(frames));
    }
    FilmState& s=film.state;
    s.sequence=node.integer(0x58);s.mode=node.integer(0x5a);s.first=node.integer(0x5b);s.last=node.integer(0x5c);s.speed=node.real(0x5d);
    const float setting=node.real(0x1a);uint32_t bits;std::memcpy(&bits,&setting,4);
    s.active=(bits&2)!=0;
    if(s.sequence<0 || uint32_t(s.sequence)>=sequences || !std::isfinite(s.speed) || std::abs(s.speed)>1000 ||
       s.first<0 || s.last<s.first || s.last>=s.lengths[size_t(s.sequence)])
        throw std::runtime_error("Invalid winner-scene film playback bounds");
    s.accumulator=float(s.first);s.frame=s.first;
    films_.emplace(node.id,std::move(film));
}
bool WinnerScene::update(bool skip) {
    if(done_) return false;
    // 0x494be0, then the scene's other updates: WMenuAnimation and the films.
    const auto rest=[&]{
        if(animation_) animation_->update();
        for(auto& [id,film]:films_) if(film.state.active) film.state.update();
        return !done_;
    };
    FilmState& car=films_.at(car_).state;
    if(type_!=1 && skip) {finish();return false;}
    if(moving_) {
        // 0x494ab0(4): the visual group's sprites (+0x12a) and films (+0x1a2)
        // move 4 left each update, for 40 updates.
        for(Layer& l:layers_) l.x-=4;
        if(++move_count_>=40) {move_count_=0;moving_=false;moved_=true;}
        return rest();
    }
    // 0x494c46: while a voice speaks nothing advances; its end is reported
    // to WMenuAnimation (0x6f41ff).
    if(audio_.speaking()) return rest();
    if(spoken_>0 && !speech_ended_) {speech_ended_=true;audio_.stop_speaking();if(animation_) animation_->speech_finished();return rest();}
    if(!sound_) {finish();return false;}
    ++phase_;
    // The winner's film: sequence 1 from m_nStartOtherSpeak, 2 from
    // m_nStartOtherSpeak2, each waiting for the running one to end (0x493fa0),
    // and back to 0 on the next sound.
    if(phase_==other2_+1 && phase_!=other_) car.play(0,2);
    if(phase_==other_+1 && phase_!=other2_) car.play(0,2);
    if(phase_==other_) {
        if(car.frame!=car.last) {--phase_;return rest();}
        car.play(1,2);
    }
    if(phase_==other2_) {
        if(car.frame!=car.last) {--phase_;return rest();}
        car.play(2,2);
    }
    if(++progress_==move_in_) {moving_=true;car.play(0,2);return rest();}
    const GameAudio::MenuVoiceEvent voice=audio_.speak(*sound_);
    ++spoken_;speech_ended_=false;
    const int actor=sound_->integer(0x1bc);
    if(animation_ && voice.started) animation_->speech_started(actor,voice.duration60);
    if(vehicle_==6 && type_!=1) {
        // 0x494ea4: in WinMean the speaker picks the film of Satanas and
        // Muttley in their car.
        static const int sequence[6]={2,3,1,-1,3,2};
        if(actor>=0 && actor<6 && sequence[actor]>=0) car.play(sequence[actor],2);
    } else if(animation_) {
        // 0x494ebc: Satanas and Muttley react to the speaker.
        static const int satanas[6]={3,0,0,-1,0,2},muttley[6]={0xd,0xe,0xd,-1,0x10,0xd};
        if(actor>=0 && actor<6 && satanas[actor]>=0) animation_->request(satanas[actor],muttley[actor]);
    }
    // 0x494e33: the next sibling; a row that is not a sound stands for the
    // environments: the environment-th row from it gives the next sounds.
    ++sound_index_;
    sound_=sound_index_<group_->children.size() ? &group_->children[sound_index_] : nullptr;
    if(sound_ && !is_sound(*sound_)) {
        const size_t row=sound_index_+size_t(std::max(environment_,0));
        if(row>=group_->children.size()) sound_=nullptr;
        else {group_=&group_->children[row];sound_index_=0;sound_=group_->children.empty()?nullptr:&group_->children[0];}
    }
    if(sound_ && sound_->class_name!="WMenuSound") sound_=nullptr;
    return rest();
}
WinnerScene::Next WinnerScene::next() const {
    // 0x494b20.
    if(type_==1) return Next::Menu;
    if(environment_==3 && course_==3) return Next::DickOff;
    if(environment_==4 && course_==3) return Next::Outro;
    return Next::Menu;
}
void WinnerScene::draw() {
    int width,height;wpc_window_get_size(window_,&width,&height);
    const float scale=std::min(width/640.f,height/480.f);
    const int ox=(width-int(640*scale))/2,oy=(height-int(480*scale))/2;
    wpc_vk_clear(window_,0,0,0);
    wpc_vk_draw_rect(window_,ox,oy,int(640*scale),int(480*scale),clear_[0],clear_[1],clear_[2],1);
    wpc_vk_set_clip_rect(window_,ox,oy,int(640*scale),int(480*scale));
    const auto image=[&](uint32_t mat,int x,int y) {
        const Texture& t=material(mat);
        if(!t.gpu) return;
        wpc_vk_draw_texture_tinted(window_,ox+int(std::lround(x*scale)),oy+int(std::lround(y*scale)),
            int(std::lround(t.width*scale)),int(std::lround(t.height*scale)),t.gpu,1,1,1,1);
    };
    // Logical 320x240 positions, doubled.
    for(const Layer& l:layers_) {
        if(!l.film) {image(l.material,l.x*2,l.y*2);continue;}
        const Film& film=films_.at(l.node);
        if(!film.state.active) continue;
        const auto& frames=film.sequences.at(size_t(film.state.sequence));
        if(frames.empty()) continue;
        for(const Placement& p:frames.at(size_t(std::clamp(film.state.frame,0,int(frames.size())-1))))
            image(p.material,l.x*2+p.x,l.y*2+p.y);
    }
}
}
