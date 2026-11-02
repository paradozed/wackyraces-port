#ifndef WPC_WINNER_SCENE_H
#define WPC_WINNER_SCENE_H
#include "game_assets.h"
#include "game_audio.h"
#include "game_scene.h"
#include "menu_animation.h"
#include "wpc_platform.h"
#include <array>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wpc {
// win\win*.wgm (0x52bf54 by vehicle), loaded by 0x486fe0 after a won
// championship: WWinnerMovie (the GDWorld's action; init 0x4949f0, update
// 0x494be0, exit 0x494b20) speaks the WMenuItem's sounds in turn, switches
// the winner's film, slides the picture in and drives Satanas and Muttley
// through WMenuAnimation (0x4942b0) when the scene has one.
std::string winner_scene_file(int vehicle);
class WinnerScene {
public:
    // What 0x494b20 loads when the scene ends.
    enum class Next { Menu, DickOff, Outro };
    // file: a Win/*.WGM path; canonical: menu.WGM's member table (the disc
    // Win files' IDs are shifted, Scene::load). environment/course: the last
    // race (0x6f41d8/0x6f41dc); vehicle: the player's (+0x1a68).
    WinnerScene(WpcWindow* window,GameAudio& audio,const std::string& file,
                const std::vector<std::pair<std::string,std::string>>& canonical,
                int vehicle,int environment,int course);
    ~WinnerScene();
    WinnerScene(const WinnerScene&)=delete;
    WinnerScene& operator=(const WinnerScene&)=delete;
    // One 30 Hz update; skip is a new press of input bit 0x40 (0x52aada over
    // 0x6e42b0: accelerate), ignored by type 1 (DickOff). False once it ends.
    bool update(bool skip);
    void draw();
    Next next() const;
    int spoken() const {return spoken_;}
    bool moved_in() const {return moved_;}
private:
    struct Placement {int x=0,y=0;uint32_t material=0;};
    struct Texture {WpcTexture* gpu=nullptr;int width=0,height=0;};
    struct Layer {uint32_t node=0,material=0;int x=0,y=0,priority=0;bool film=false;};
    struct Film {FilmState state;std::vector<std::vector<std::vector<Placement>>> sequences;};
    Texture& material(uint32_t id);
    std::vector<Placement> placements(const Chunk& resource);
    void load_film(const Node& node);
    void finish() {done_=true;audio_.stop_speaking();}
    WpcWindow* window_;
    GameAudio& audio_;
    Archive archive_;
    Scene scene_;
    int vehicle_,environment_,course_;
    int type_=0,move_in_=0,other_=0,other2_=0;   // template +0x24/+0x2c/+0x34/+0x3c
    const Node* visual_=nullptr;   // the world's first child (GDStdBrick)
    const Node* group_=nullptr;    // +0x24: the sound list's current parent row
    const Node* sound_=nullptr;    // +0x28: the next sound
    size_t sound_index_=0;
    uint32_t car_=0;               // the visual group's first film
    bool moving_=false,moved_=false,done_=false; // +0x2c
    int move_count_=0,progress_=0,phase_=0;      // +0x2d, +0x31, +0x35
    int spoken_=0;
    bool speech_ended_=true;       // the last voice's end was reported (0x6f41ff)
    std::vector<Layer> layers_;
    std::map<uint32_t,Film> films_;
    std::map<uint32_t,Texture> textures_;
    std::map<uint32_t,uint32_t> materials_;
    std::unique_ptr<MenuAnimation> animation_;
    std::array<float,3> clear_{{0,0,0}};
};
}
#endif
