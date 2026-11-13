#ifndef WPC_MENU_VIEW_H
#define WPC_MENU_VIEW_H
#include "game_assets.h"
#include "game_scene.h"
#include "menu_animation.h"
#include "menu_controller.h"
#include "menu_models.h"
#include "wpc_platform.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wpc {
class MenuView {
public:
    MenuView(WpcWindow *window, const Archive &archive, const Scene &scene,
             const Strings &strings, const MenuController &controller);
    // Upload newly active page assets before beginning a Vulkan frame.
    void prepare();
    // Run the menu updates due at `elapsed` seconds since menu entry, at
    // 30 per second; per_update runs before each (the voice queue). A smaller
    // elapsed than before restarts the animations, as reloading MENU.WGM does.
    void update(double elapsed,const std::function<void()>& per_update={});
    // The main menu's WMenuAnimation, once its page has been prepared.
    MenuAnimation* animation() {return animation_.get();}
    void draw(double elapsed);
    // The page's layers and labels for one cursor.
    // ready: page 61's player is ready (0x405f79 shows the inactive texts).
    void draw_page(int selected,bool ready=true);
    // Drawn over another frame (the in-race pause over the race view): no
    // background clear.
    void set_overlay(bool overlay) {overlay_=overlay;}
private:
    struct Texture { WpcTexture *gpu{}; int width{},height{}; };
    struct Placement { int x{},y{}; uint32_t material{}; };
    struct Layer {
        int x{},y{},priority{};
        int item=-1;
        bool only_one=false;
        uint32_t road=0; // the GDSprite moved by its WSpriteMove (road_)
        float scale=1;
        uint32_t film=0; // GDFilm2 node drawn from its playback state
        // The page child this layer comes from (its index in the page's
        // list, which 0x48cec0 walks): on the controls page 0 is the
        // keyboard and 1 the joystick image (0x48eff0); on the selection
        // pages children 2r and 2r + 1 are row r's banner halves.
        int page_child=-1;
        std::array<float,3> color{{1,1,1}};
        // A volume notch (0x489e30): lit, with the item's 0x190..0x192
        // colour, while its rank x 0.1 is below the item's value.
        const Node* notch_item=nullptr;int notch=-1;
        std::array<float,3> lit{{1,1,1}};
        std::vector<std::vector<Placement>> frames;
    };
    struct Film {
        FilmState initial,state;
        std::vector<std::vector<std::vector<Placement>>> sequences;
    };
    struct Label {
        int item=-1,x=0,y=0,align=4;
        bool only_one=false, highlight=false;
        uint32_t font=0;
        float scale=1;
        std::array<float,3> color{{1,1,1}};
        std::string text;
        const Node *value_item=nullptr; // the option item whose value this shows
        const Node *source=nullptr;     // the WMenuText, for runtime text changes
        bool inactive=false;            // flag 0x2 of 0x1a clear (page 61's SELECTIONNE)
        int kern=0;                     // property 0x4e (descriptor +0x150)
    };
    struct Page { std::vector<Layer> layers; std::vector<Label> labels; };
    Texture &material(uint32_t id);
    std::vector<Placement> placements(const Chunk &resource);
    void collect(Page &page, const Node &node, int item=-1, int cursor_x=0,int cursor_y=0,bool common=false);
    void image(uint32_t material,int x,int y,float r=1,float g=1,float b=1,float size=1);
    void text(const Label &label,bool selected);
    // WMenuNodeLoadSave's screen (0x495790), drawn by 0x4015e6 with the
    // first GDPsText of font class 13 (node 0x117 in menu.WGM).
    void draw_files();
    void draw_credits();
    uint32_t file_font_=0;
    float file_pulse_=240,file_step_=-10; // 0x6f4010/0x6f4014, 0x52c034
    WpcWindow *window_;
    const Archive &archive_;
    const Scene &scene_;
    const Strings &strings_;
    const MenuController &controller_;
    const Node *value_owner_=nullptr;
    MenuModels models_;
    std::map<uint32_t,Texture> textures_;
    std::map<uint32_t,uint32_t> materials_;
    std::map<uint32_t,std::vector<Placement>> fonts_;
    std::map<uint32_t,Page> pages_;
    std::map<uint32_t,Film> films_;
    uint32_t animation_owner_=0;
    std::unique_ptr<MenuAnimation> animation_;
    uint64_t updates_=0;
    // The road cars' WSpriteMove state by sprite, kept across pages (the
    // common group); the shared rand() of 0x430e40.
    std::map<uint32_t,SpriteMove> road_;
    MsvcRandom road_random_;
    void restart_animation();
    uint32_t active_page_=0;
    std::array<float,3> highlight_{{1,1,0}},clear_{{0,0,0}};
    float scale_=1;
    int offset_x_=0,offset_y_=0;
    bool overlay_=false;
    const Node* volume_item_=nullptr;int notch_=0;
};
}
#endif
