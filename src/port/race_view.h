#ifndef WPC_RACE_VIEW_H
#define WPC_RACE_VIEW_H
#include "game_scene.h"
#include "game_world.h"
#include "game_random.h"
#include "race_session.h"
#include <map>
#include <string>
#include <vector>

namespace wpc {
struct Colour {float r,g,b;};
class RaceView {
public:
    RaceView(WpcWindow* window,const ArchivePool& archives,const Archive& general,
             const Scene& course,const Scene& shared,const Strings& strings,
             const RaceSession& session);
    ~RaceView();
    // The three lines 0x4a8f30 shows under the championship standings when
    // the player is not leading (environment 3, a championship's last course).
    void set_championship_message(std::vector<std::string> lines) {championship_message_=std::move(lines);}
    RaceView(const RaceView&)=delete;
    // The vertex colour range of a vehicle's model (0x47d890 steps them all).
    void model_colours(int vehicle,std::array<float,3>& lo,std::array<float,3>& hi) const;
    RaceView& operator=(const RaceView&)=delete;
    void draw();
private:
    // smooth: the frame material's SMOO (GMAT +0x68), which 0x42d280 tests.
    struct Glyph {int x=0,y=0;WpcTexture* texture=nullptr;int width=0,height=0;bool smooth=false;};
    struct Sprite {WpcTexture* texture=nullptr;int width=0,height=0;};
    void upload(std::map<uint32_t,Image>& images);
    WpcTexture* texture(const Material& material) const;
    void draw_batches(const std::vector<WorldBatch>& batches,const Matrix4& mvp);
    // Opaque parts now; translucent ones join queue_ under model's frame.
    void submit(const std::vector<WorldBatch>& batches,const Matrix4& vp,const Matrix4& view,const Matrix4& model);
    // A car with its hit's 0x47d890 colours (lerp to the tint target) and
    // 0x43b190 squash.
    void submit_car(int vehicle,const BombHit& hit,const Matrix4& vp,const Matrix4& view,const Matrix4& frame);
    std::vector<WpcTexturedVertex3D> tinted_;
    Sprite sprite(uint32_t material);
    // 0x42e330: align 4 centres on x; the colour (0..1) tints the font.
    void text(const std::string& value,int x,int y,int align,Colour colour=Colour{1,1,1});
    void glyph(const Glyph& g,int x,int y,Colour colour=Colour{1,1,1});
    int text_width(const std::string& value) const;
    void hud();
    // Frames of a general.wgm FILM's first reel (placement offset + material).
    std::vector<Glyph> film(uint32_t id,uint32_t reel=0);
    // WCarFx effects (race_effects): WSplash's column and ring models, the
    // WalkOnWater ripple, and the dust GD3DFilm's reels.
    void effects(const Matrix4& vp,const Matrix4& view,const Matrix4& camera);
    void draw_view(int human,int width,int height);
    // A two-player viewport's HUD (layout rows 1/2, 0x52bd5c).
    void hud2(int human);
    // METEO (0x4b32f9): rain 0x4b6190 or snow 0x4b6510 around the camera.
    void weather(const Matrix4& vp,const Matrix4& view,const Matrix4& camera);
    struct Flake {Vec3 at{},velocity{};int count=0;}; // 0x2690c40, 0x120 bytes each
    std::vector<Flake> flakes_;
    std::vector<WpcTexturedVertex3D> rain_;
    MsvcRandom weather_random_;
    uint64_t weather_update_=~uint64_t(0);
    WpcTexture* flake_=nullptr;
    // WCarFx's shadow ray (0x47c180): the shadow's frame for a car, kept from
    // the last update that found ground within 50 units above the car.
    bool shadow_frame(size_t slot,const RaceCar& car,Matrix4& out);
    std::map<size_t,Matrix4> shadow_frames_;
    size_t knocked_=0;
    std::vector<WorldBatch> splash_column_,splash_ring_,ripple_;
    // WPU_Storm's cloud: its GD3dSprite (#889) and one facing copy per storm.
    std::vector<WorldBatch> storm_;std::vector<std::vector<WorldBatch>> storm_shown_;
    std::vector<WorldBatch> landmine_,tnt_,detour_,oil_,wall_,tree_,hive_; // WPU_Landmine's and WPU_Tnt's GDModels (general.wgm 0x372, 0x382)
    std::map<uint32_t,std::vector<WorldBatch>> projectiles_; // DragonBreath id -> its GDModel
    std::vector<std::vector<Glyph>> puff_reels_;
    std::vector<Glyph> smoke_frames_,bumm_frames_;
    Glyph blast_; // WExplosion's sprite
    float puff_scale_=0.3f;
    // A textured quad in 640x480 HUD space: top-left (x,y) before rotation,
    // turned by angle (radians, clockwise) about (x+px, y+py).
    void quad(WpcTexture* texture,float x,float y,float width,float height,bool mirror,float alpha,
              float angle=0,float px=0,float py=0);
    void powerup_panel(int human=0);
    void progress_strip();
    void standings();
    void step_hud();
    WpcWindow* window_;
    const Archive& general_;
    const Strings& strings_;
    const RaceSession& session_;
    WorldData world_;
    std::vector<WorldBatch> backdrop_;
    // Per batch list: its translucent models (0x4afcc2 entries).
    std::map<const std::vector<WorldBatch>*,std::vector<TranslucentModel>> translucent_;
    struct Queued {const std::vector<WorldBatch>* batches;const TranslucentModel* model;Matrix4 mvp;};
    std::vector<Queued> queued_;
    TranslucentQueue queue_;
    float backdrop_radius_=1;
    float fog_start_=0,fog_end_=0;
    std::map<int,WorldData> cars_,shadows_;
    std::map<uint32_t,WpcTexture*> textures_;
    std::vector<WpcTexture*> owned_;
    WpcTexture* white_=nullptr;
    Matrix4 projection_{};
    std::vector<Glyph> font_,bend_,heads_,faces_; // faces_: WHudBrick child 7's film
    std::vector<std::string> championship_message_;
    // WFirstPowerUp children (0x483a20): dots, gauge needle, winged gauge;
    // its WPowerUpSprite icons with their 0xa1/0xa2 origin.
    Sprite big_dot_,small_dot_,gauge_,gauge_needle_;
    std::vector<Sprite> icons_;
    std::array<int,2> icon_origin_{};
    // WPowerUp GDModel batches, drawn per pickup state; index = track spot.
    std::vector<std::vector<WorldBatch>> pickups_;
    // dt_t1's trams (race_objects): the crossing trams, then the path movers, in
    // CourseObjects order, drawn at the session's frames.
    std::vector<std::vector<WorldBatch>> crossing_trams_,movers_;
    std::vector<std::vector<std::vector<WorldBatch>>> logs_; // per WLogRide, per log
    // The crossing lights (the WTram brick's second child, GDModel 0x3b1 on
    // dt_t1): their world batches and each vertex's serialized u, v.
    struct Lights {size_t crossing=0;uint32_t node=0;bool found=false;std::vector<std::pair<size_t,std::vector<std::array<float,2>>>> batches;};
    std::vector<Lights> lights_;
    void animate_lights();
    std::array<float,2> gauge_angle_{{-45,-45}}; // degrees, +0x13e/+0x142 per player
    std::vector<float> strip_x_; // WackyAuto +0x1ab per racer
    uint64_t hud_update_=0;
    Sprite dial_,needle_;
    std::array<int,2> dial_xy_{},needle_xy_{};
    float scale_=1;int offset_x_=0,offset_y_=0;
    void release();
};
}
#endif
