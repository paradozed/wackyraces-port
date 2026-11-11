#ifndef WPC_RACE_VIEW_H
#define WPC_RACE_VIEW_H
#include "game_scene.h"
#include "game_world.h"
#include "game_motion.h"
#include "game_random.h"
#include "race_session.h"
#include <functional>
#include <map>
#include <memory>
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
    // film: a GD3DFilm frame (drawn by 0x4b5550); smooth/sill: its material's
    // SMOO and SILL are nonzero.
    struct Glyph {int x=0,y=0;WpcTexture* texture=nullptr;int width=0,height=0;bool smooth=false,sill=false,film=false;};
    struct Sprite {WpcTexture* texture=nullptr;int width=0,height=0;};
    void upload(std::map<uint32_t,Image>& images);
    WpcTexture* texture(const Material& material) const;
    void draw_batches(const std::vector<WorldBatch>& batches,const Matrix4& mvp);
    // Opaque parts now; translucent ones join queue_ under model's frame.
    void submit(const std::vector<WorldBatch>& batches,const Matrix4& vp,const Matrix4& view,const Matrix4& model,const std::vector<char>* mask=nullptr);
    // A car with its hit's 0x47d890 colours (lerp to the tint target) and
    // 0x43b190 squash.
    void submit_car(int car,int vehicle,const BombHit& hit,const Matrix4& vp,const Matrix4& view,const Matrix4& frame);
    // A hit car's model with its 0x47d890 colours, per hit (all its batches).
    struct TintedModel {int vehicle=-1;std::vector<WorldBatch> batches;};
    std::map<const BombHit*,TintedModel> tinted_models_;
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
    // place(section): the seam placement of a group; enabled: the drawn sections.
    void effects(const Matrix4& vp,const Matrix4& view,const Matrix4& camera,const std::function<Matrix4(int)>& place,uint32_t enabled);
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
    // DragonBreath id -> its WRocketTail's m_Length (0x2ec), m_Width (0x2ed)
    // and the fins' bit 0x1 of segments (0x2f7).
    struct TailStyle {int length=16,width=100;bool fins=true;};
    std::map<uint32_t,TailStyle> tails_;
    std::vector<Glyph> breath_film_; // 0x39a's GD3DFilm 0x39d
    // A GDHModel drawn with its MTNL motion (0x437d00): its mesh, its batches
    // (as load_model_group builds them) and, per triangle corner, the batch,
    // the slot and the mesh vertex, so a new pose rewrites the positions.
    struct AnimatedModel {
        Mesh mesh;std::vector<WorldBatch> batches;
        struct Corner {uint32_t batch,slot,vertex;};
        std::vector<Corner> corners;
        const MotionLib* lib=nullptr;
    };
    AnimatedModel load_animated(const ArchivePool& archives,const Node& node);
    std::map<uint32_t,MotionLib> motions_;
    struct AnimatedInstance {
        AnimatedModel model;MotionController controller;uint64_t posed=0;
        // 0x437f70 with flag 1: a segment's scale slots locked (a = -1, b = s - 1).
        std::vector<std::pair<size_t,std::array<float,3>>> overrides;
        // 0x4381a0 with flag 1: a segment's translation slots locked (a = -1, b = t).
        std::vector<std::pair<size_t,std::array<float,3>>> moves;
        std::vector<std::array<float,12>> segments;   // the posed segment matrices
        AnimatedInstance(const AnimatedModel& m,const MotionLib& lib):model(m),controller(lib) {}
    };
    // One controller step of 10 per race update since the last pose (the
    // original steps once per drawn GDHModel; provisional), then pose_mesh.
    void pose(AnimatedInstance& a);
    // WPU_Breath's template (general.wgm 0x35e): GDModel 0x360 and GDHModel 0x361.
    std::vector<WorldBatch> breath_static_;
    AnimatedModel breath_head_;
    std::map<int,std::unique_ptr<AnimatedInstance>> breath_heads_; // by car (-1 the player)
    // Per vehicle: the body model's first GDHModel (the original's car +0x24f)
    // segment 1 frame in the body's model space.
    std::map<int,Matrix4> body_segment_;
    void submit_breath_head(int car,const RaceCar& c,const Matrix4& vp,const Matrix4& view,const Matrix4& placed);
    // Controller 0 (0x440e30) on each car's body GDHModel: the group's first
    // GDHModel, its transform in the group (as WorldBuilder composes it), and
    // the mask of the group's other batches.
    struct CarBody {uint32_t node=0;Matrix4 chain=identity_matrix();AnimatedModel model;std::vector<char> mask;};
    std::map<int,CarBody> bodies_;   // by vehicle
    // MonsterTires: the car's wheels (+0x273..+0x27f, the model root's first
    // four children, 0x469ebc) hidden; the template brick 0x2c0's four tyre
    // GDModels drawn instead (0x4d6380, 0x4d6560).
    std::map<int,std::vector<char>> wheel_masks_;   // by vehicle: 0 on the wheels' batches
    // Each wheel k: its batches (1) and its node transform in the model group.
    struct Wheel {std::vector<char> mask;Matrix4 node=identity_matrix(),inverse=identity_matrix();};
    std::map<int,std::array<Wheel,4>> wheels_;      // by vehicle
    std::map<int,std::array<std::vector<char>,4>> wheel_draw_masks_;   // by car
    std::array<std::vector<WorldBatch>,4> tyres_;
    float tyre_radius_=50;   // m_tireradius (0x295, template +0x3c)
    std::map<int,std::vector<char>> masks_;   // by car: the combined mask drawn
    // The gadgets' body animations: a clip on the body's controller 1, the
    // locks of 0x437f70 (scale) and 0x4383b0 (rotation, degrees) while the
    // GDHModel's +0x17a bit 0 is set, and the gadget state last seen.
    struct CarSway {int vehicle=-1;std::unique_ptr<AnimatedInstance> body;SwayController sway;uint64_t stepped=~uint64_t(0);std::vector<WorldBatch> tinted;
                    int last_effect=-1,last_fly=0,last_weapon=-1;bool locked=false;
                    std::vector<std::pair<size_t,std::array<float,3>>> scale_locks,turn_locks;};
    void body_gadgets(int car,CarSway& w);
    std::map<int,CarSway> sways_;    // by car (-1 the player)
    void apply_pose(AnimatedInstance& a,const MotionPairs& pairs);
    std::vector<std::vector<Glyph>> puff_reels_;
    std::vector<Glyph> smoke_frames_,bumm_frames_,hit_smoke_frames_;
    // m_BoomerangHit's model (general.wgm 0x391) and its local matrix.
    std::vector<WorldBatch> spin_hit_;Matrix4 spin_hit_basis_=identity_matrix();
    std::vector<WorldBatch> paint_pool_; // m_bluepool's model (general.wgm 0x1c1)
    void submit_spin_hit(const BombHit& hit,const RaceCar& car,const Matrix4& vp,const Matrix4& view,const Matrix4& placed);
    // m_TankGunHit (0x14f; general.wgm GDModel 0x392, a container, and its
    // GDHModel 0x393), one instance per hit car (-1 the player).
    // FlyPower's models (the template's 0x290..0x293); vehicle 3's wings:
    // member 0x292 (+0x4c), a GDStdBrick whose child is GDDoubleHModel 0x1cb.
    AnimatedModel fly_wings_;
    AnimatedModel fly_v4_;   // vehicle 4: member 0x290 (+0x3c), GDHModel 0x1c2 itself
    // Vehicle 6: member 0x291 (+0x44), brick 0x1c3 (WPU_Propeller) whose
    // child GDHModel 0x1c5 carries a WRot (type 2, m_StepRotDeg* per update).
    AnimatedModel fly_prop_;
    Matrix4 fly_prop_brick_=identity_matrix(),fly_prop_local_=identity_matrix();
    std::array<float,3> fly_prop_step_{};
    struct FlyProp {int stage=0;uint64_t spun=0;std::array<float,3> angle{};};
    // Vehicle 5: member 0x293 (+0x54), GDHModel 0x1c8 itself (from update 10).
    AnimatedModel fly_chute_;
    Matrix4 fly_chute_local_=identity_matrix();
    std::map<int,FlyProp> fly_props_;
    std::map<int,std::unique_ptr<AnimatedInstance>> fly_instances_;
    std::map<int,std::vector<WorldBatch>> fly_mirrors_;   // a GDDoubleHModel's mirrored half
    void submit_fly(int car,const RaceCar& c,const Matrix4& vp,const Matrix4& view,const Matrix4& placed);
    // WFire (vtable 0x5130b8, draw 0x481390): the car model's WFire nodes,
    // shown by TurboSpeed (0x4d6cb0) and FlyPower (0x4d4f26, 0x4d504b); each
    // draw rebuilds a random flame. Triangles in world space, drawn in effects.
    void submit_fires(int car,const RaceCar& c,const Matrix4& placed);
    // WPU_BootKicker's boot (general.wgm brick 0x32b's GDHModel 0x32d).
    AnimatedModel boot_;Matrix4 boot_local_=identity_matrix();
    std::vector<std::unique_ptr<AnimatedInstance>> boots_;
    void submit_kicks(const Matrix4& vp,const Matrix4& view,const std::function<Matrix4(int)>& place);
    // WPU_Flipper's arm (general.wgm brick 0x32f's GDHModel 0x337, MTNL 0x10359).
    AnimatedModel flipper_;
    struct FlipArm {std::unique_ptr<AnimatedInstance> a;bool frozen=false;};
    std::vector<FlipArm> flip_arms_;
    void submit_flips(const Matrix4& vp,const Matrix4& view,const std::function<Matrix4(int)>& place);
    std::vector<WpcTexturedVertex3D> fire_tris_;
    // WBeeHive (action on brick 0x36c; vtable 0x512bac, init 0x474580, update
    // 0x474600, draw 0x474650): m_BeeNum (32) bees, each two 12-bit phases
    // stepping by m_Speed (60) an update; by trapped car.
    struct Swarm {std::vector<std::array<int,4>> bees;uint64_t stepped=0;};
    std::map<int,Swarm> swarms_;
    int bee_num_=32,bee_radius_=150,bee_speed_=60;
    void submit_bees(const Matrix4& vp,const std::function<Matrix4(int)>& place);
    uint32_t fire_random_=1;   // the draw's rand() (0x404363 -> 0x430ef0), view-local
    AnimatedModel tank_hit_;
    std::map<int,std::unique_ptr<AnimatedInstance>> tank_hits_;
    std::vector<WorldBatch> chunk_model_;   // m_Particle (0x151; general.wgm GDModel 0x394)
    void submit_tank_hit(int car,const BombHit& hit,const CarEffects& fx,const Matrix4& vp,const Matrix4& view,const Matrix4& placed);
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
    // Rain (0x2a1cc74 set, 0x2a1b2b0 = 0): textures converted at half
    // intensity (0x4b0550 doubles the channel divisors, 0x4b08b1).
    bool rain_dim_=false;
    void dim(Image& image) const;
    // Per batch list: its translucent models (0x4afcc2 entries).
    std::map<const std::vector<WorldBatch>*,std::vector<TranslucentModel>> translucent_;
    // mask: when set, only the batches it marks are drawn (the loop seam's sections).
    struct Queued {const std::vector<WorldBatch>* batches;const TranslucentModel* model;Matrix4 mvp;const std::vector<char>* mask;};
    // World batches of section 0 (drawn through unwrap) and of the last section (through wrap).
    std::vector<uint32_t> batch_sections_;std::vector<int> batch_group_;const std::vector<int> batch_group_none_;std::vector<char> drawn_;std::array<std::vector<char>,2> seam_;
    std::vector<Queued> queued_;
    TranslucentQueue queue_;
    float backdrop_radius_=1;
    Vec3 backdrop_origin_{};   // the backdrop node's world translation
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
    std::vector<std::array<std::vector<WorldBatch>,2>> bridges_; // per WMisty, its two halves
    std::vector<std::array<std::vector<WorldBatch>,2>> geysers_; // per WGeyser, column and ring
    std::vector<std::array<std::vector<WorldBatch>,2>> holes_;   // per WDynamiteHole, lid and fuse
    std::vector<std::array<std::vector<WorldBatch>,3>> dicks_;    // per WDicksTrick2
    std::vector<std::array<std::vector<WorldBatch>,10>> golden_;  // per WGoldenGate: doors, marker
    std::vector<std::array<std::vector<WorldBatch>,3>> cranes_;   // per WJunkyard, per arm
    std::vector<std::array<std::vector<WorldBatch>,6>> shifted_;  // per WShiftedRoad
    std::vector<std::array<std::vector<WorldBatch>,2>> saws_; // per WSaw: spinner, owner
    std::vector<std::vector<WorldBatch>> spins_;  // per course WRot
    std::vector<std::vector<std::vector<WorldBatch>>> chisels_,rocks_,jams_,bumps_; // per object, per piece or log
    std::vector<Matrix4> hole_fuses_;
    // The crossing lights (the WTram brick's second child, GDModel 0x3b1 on
    // dt_t1): their world batches and each vertex's serialized u, v.
    struct Lights {size_t crossing=0;uint32_t node=0;bool found=false;std::vector<std::pair<size_t,std::vector<std::array<float,2>>>> batches;};
    std::vector<Lights> lights_;
    void animate_lights();
    void animate_scrolls();
    struct Scroll {uint32_t node=0;bool along_u=false;int units=0;bool found=false;std::vector<std::pair<WorldBatch*,std::vector<float>>> batches;};
    std::vector<Scroll> scrolls_;
    std::map<uint32_t,std::array<uint32_t,2>> texture_sizes_;
    std::array<float,2> gauge_angle_{{-45,-45}}; // degrees, +0x13e/+0x142 per player
    std::vector<float> strip_x_; // WackyAuto +0x1ab per racer
    uint64_t hud_update_=0;
    Sprite dial_,needle_;
    // WPU_FireSiren's light: the template brick 0x311's GDSprite 0x12e
    // (GMAT 0x10222, centre (16, 16)).
    Sprite siren_;std::array<int,2> siren_centre_{{16,16}};
    void siren_lights(int viewer);
    std::array<int,2> dial_xy_{},needle_xy_{};
    float scale_=1;int offset_x_=0,offset_y_=0;
    void release();
};
}
#endif
