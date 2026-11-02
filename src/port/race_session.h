#ifndef WPC_RACE_SESSION_H
#define WPC_RACE_SESSION_H
#include "game_drive.h"
#include "game_race.h"
#include "game_random.h"
#include "race_camera.h"
#include "race_effects.h"
#include "race_robot.h"
#include "race_track.h"
#include "race_weapons.h"
#include <string>
#include <array>
#include <deque>
#include <utility>

namespace wpc {
// One original game update (0x4ad540 holds frames to >= 33 ms).
constexpr double race_update_seconds=1.0/30.0;

struct RaceCar {
    int vehicle=0;
    Vec3 position{};
    float yaw=0;           // radians about +Y; forward = (-sin, 0, cos)
    Vec3 up{{0,1,0}};
    SpeedState speed;
    float steer=0;
    bool grounded=true;    // any probe contact this update
    int airtime=1;         // +0x8a7: 1 on the ground, +1 per update without a contact
    float pitch=0,roll=0;  // +0x267, +0x26f: from the contact rays (0x46ff..)
    int section=0;
    int loops=0;           // times carried across the WMOD loop transform
    uint32_t surface=0;    // the centre contact's WGNR flags (WackyAuto +0x8bb)
    Matrix4 frame() const; // actor frame: X right, Y up, Z forward
};

class RaceSession {
public:
    enum class Phase {Ready, Go, Racing, Finished};
    // mode: the game mode (app +0x1a60) for the roster (0x49d580): 0/3 a
    // single race, 1 battle (one robot), 2 practice (alone at slot 2).
    // vehicle2 >= 0: two players (app +0x1a64 = 1, 0x49d580): no robots,
    // player 1 at grid slot 0 and player 2 at slot 3 (Wine, cc_t1); player 2
    // is opponents()[0] with human set.
    RaceSession(const RaceTrack& track,int vehicle,uint32_t random_seed=0,int mode=0,int vehicle2=-1);
    bool two_players() const {return !opponents_.empty() && opponents_[0].human;}
    int mode() const {return mode_;}
    // Practice's lap splits 0x6f318c/0x6f3190/0x6f3194 (1/300 s, -1 until
    // run, 0x4a33ac..0x4a33f6) and the course record (0x6f467c, loaded at
    // 0x49e0c5: low 28 bits the time, high 4 the vehicle).
    const std::array<int,3>& lap_splits() const {return splits_;}
    int split_total() const {return splits_[0]+splits_[1]+splits_[2];} // 0x6f3198
    void set_record(uint32_t record) {record_=record;}
    uint32_t record() const {return record_;}
    bool new_record() const {return new_record_;} // +0xfa4 after a practice finish
    // Battle (mode 1): the hits each car has received (+0x3a3; car -1 the
    // player, 0 the opponent) and the limit app+0x1a74, 5 in mode 1 (0x4a0602).
    int frags(int car) const {return car<0?body_.frags:opponent_bodies_.at(size_t(car)).frags;}
    static constexpr int frag_limit=5;
    bool battle_lost() const {return body_.frags>=frag_limit;}
    void update(const DriveInput& input,const DriveInput& input2={});
    // Debug placement (WPC_RACE_PLACE): put the player on the ground at x,z.
    void place(float x,float z,float yaw);

    Phase phase() const {return phase_;}
    const RaceCar& player() const {return player_;}
    const std::vector<PlacedRacer>& grid() const {return grid_.racers;}
    const VehicleTuning& tuning() const {return tuning_;}
    // 1-based lap shown on the HUD, clamped to the course lap count.
    int lap() const;
    int laps() const {return track_.laps();}
    int laps_completed() const {return body_.laps;}
    int rank() const;
    double race_seconds() const;
    bool drowning() const {return body_.drown>0;}
    int respawns() const {return body_.respawns;}
    // Updates spent in the current phase (the 0x4a2b30 counter at +0xf34).
    uint64_t phase_updates() const {return phase_updates_;}
    // Wall contacts so far (each slows the car).
    int impacts() const {return body_.impacts;}
    bool wrong_way() const {return wrong_way_updates_>=30;}
    // 0x4a6890: the arrow type (0..4) of a WBendSign of the current route
    // within its radius whose heading is within 60 degrees of the car's, or -1.
    int bend_arrow() const {return bend_arrow_;}
    // WackyAuto +0x399: power-up charge 0..3, raised by pickups and spent
    // by using the gadget or weapon; it drives the HUD gauge.
    int charge_level(int human=0) const {return human?items2_.charge:items_.charge;}
    // Icon indexes of the gadget (+0x38d) and weapon (+0x391) slots into the
    // WFirstPowerUp sprite list, or -1 while a slot is empty.
    int gadget_icon(int human=0) const {return human?items2_.gadget:items_.gadget;}
    int weapon_icon(int human=0) const {return human?items2_.weapon:items_.weapon;}
    // WPowerUp pickups (0x4d3ec0): 0 idle, 1 taken, 2 waiting, 3 clearing,
    // 4 growing; the model is drawn turned by the shared spin and scaled.
    // enabled: the node runs, is drawn and can be taken only while a WMOD
    // row linking it (or an ancestor) is the player's or next to it (0x4a8680).
    struct PowerUpState {int state=0,counter=0;float scale=1;bool enabled=true;};
    const std::vector<PowerUpState>& power_ups() const {return power_ups_;}
    float power_up_spin() const {return spin_;}
    // Pickup and item-use notices since the last call, for the log.
    std::vector<std::string> take_item_events();
    // Voice-over requests (character = vehicle, event) from the cars'
    // contacts in the last update: 4 on a car contact (0x46b429), 5 on a
    // rock (0x46a7c6), both with chance 255.
    const std::vector<std::pair<int,int>>& voice_events() const {return voice_events_;}
    // Progress-strip entries (0x499d90), in grid order: vehicle, the
    // fraction of the current lap (0..1), and whether the racer finished.
    struct StripRacer {int vehicle=0;float lap_fraction=0;bool finished=false;bool player=false;};
    std::vector<StripRacer> strip() const;
    // Finish (WModManager 0x4a2b30 states 4..7). A championship race adds
    // 10/6/4/3/2/1/0 points by rank (0x52cef0) to the totals passed here,
    // indexed by vehicle (profile bytes 0x6f4675).
    void set_championship(const std::array<int,7>& totals) {championship_=true;totals_=totals;}
    // 5 single-race standings, 6 championship race standings, 7 championship
    // standings by points; 0 before the finish. The counter (+0xf34) starts
    // at 120 and counts down to 0 once per update.
    int finish_state() const {return finish_state_;}
    int finish_counter() const {return finish_counter_;}
    // Vehicles in the order the standings show them (+0x7f0, or +0x810 sorted
    // by points in state 7).
    const std::vector<int>& standings() const {return standings_;}
    const std::array<int,7>& race_points() const {return race_points_;}
    const std::array<int,7>& points_totals() const {return totals_;}
    // Enter on the finish screens: false while the counter runs or when
    // state 6 moves on to 7; true when the race should close.
    bool accept();
    // The actor's follow frame +0x8eb (0x468620): the car frame while the
    // motion counter +0x3eb is positive, else only its translation moves.
    const Matrix4& follow_frame() const {return follow_[0];}
    // Player 2's follow frame, lap and rank (two players).
    const Matrix4& follow_frame2() const {return two_players()?follow_[1]:follow_[0];}
    // WFollowCamera_2's camera (0x478070) for player 1 (0) or 2 (1).
    const Matrix4& camera(int human=0) const {return cameras_[two_players() && human?1:0].matrix();}
    bool camera_near(int human=0) const {return cameras_[two_players() && human?1:0].near();}
    // The general.wgm members the camera keeps (follow_camera_members).
    void set_camera_members(const FollowCameraMembers& m);
    int lap2() const {return two_players()?std::min(opponent_bodies_[0].laps+1,track_.laps()):0;}
    int rank2() const {return two_players()?race_rank(0):0;}
    uint64_t updates() const {return updates_;}
    const RaceTrack& track() const {return track_;}
    // The other racers, in grid order, driven by WCtrlRoboCar (race_robot).
    struct Opponent {
        int vehicle=0;
        bool human=false;   // player 2 (MenuPrefs controls 1)
        DriveInput last{};  // player 2's input an update ago (item presses)
        RaceCar car;
        VehicleTuning tuning;
        RoboDriver robot;
        RoboControl control;
        Vec3 previous{};    // +0x173
        bool far=false;     // +0x2b1 0x1000: sliding along the route away from the player
        // The rank-based speed scaling (0x4e4d30 in 0x4e4f10).
        float rank_unit=0;      // 0x49f52c's rand() draw at the race setup
        float rank_boost=1;     // WCtrlRoboCar +0xb4
        bool rank_boosted=false; // +0x30
        int rank_scale=0;       // the last 0x4e4d30 result
        int effect=0;           // +0x1c7: 5 waits at a WTram crossing
        VehicleTuning base;     // +0x21b, +0x21f, +0x223: the unscaled rows
        // The robot's items: 0x4679d0's slots for m_isrobotauto (no
        // roulette: a pickup draws the icon and the slot waits, 0xff, for
        // WCtrlRoboCar to write 5).
        struct Items {
            int gadget=-1,weapon=-1;          // +0x38d, +0x391
            int gadget_state=0,weapon_state=0; // +0x3a0, +0x39f
            int charge=0;                     // +0x399
            int weapon_wait=0,field_wait=0;   // WCtrlRoboCar +0xac, +0xb0
            int list_timer=0;                 // +0xcc
            std::array<int,4> boxes{{-1,-1,-1,-1}}; // +0xd0: power-up indices
            int prefer_timer=0;bool prefer_player=false; // +0xe0, +0xe4
        } items;
    };
    // Tests: keep every robot in the near (physical) mode.
    void set_robots_always_near(bool v) {robots_near_=v;}
    // Tests: freeze the robots (lookahead copies of a session).
    void set_robots_enabled(bool v) {robots_enabled_=v;}
    const std::vector<Opponent>& opponents() const {return opponents_;}
    // A robot outside the player's previous, current and next rows is hidden
    // (0x4a2776) until its virtual run reaches one of them (0x4a1fbd).
    bool opponent_hidden(size_t i) const {return opponent_bodies_.at(i).hidden;}
    bool opponent_skidding(size_t i) const {return opponent_bodies_.at(i).skid.marking();}
    bool opponent_drowning(size_t i) const {return opponent_bodies_.at(i).drown>0;}
    // A robot's one-shot sound counters, as the player's: wall hits (+0x3d7),
    // rocks (+0x3d3), cactus and car hits (+0x3cf), WKBumm clones.
    // pickups: player 2's power-ups (robots, +0xc0 = 1, make no sound).
    struct CarSounds {int impacts=0,rocks=0,hits=0,bumms=0,bombs=0,oils=0,pickups=0;};
    CarSounds opponent_sounds(size_t i) const {
        const Body& b=opponent_bodies_.at(i);
        return {b.impacts,b.rock_sounds,b.cactus_sounds,b.bumms,b.hit_sounds,b.oil_sounds,b.pickup_sounds};
    }
    // The player's glides off wall-flagged surfaces so far (0x4721f0).
    int player_glides() const {return body_.glides;}
    const CourseObjects& objects() const {return objects_;}
    // The course's environment and variant, for 0x52b910 (0x483e06).
    void set_course(int environment,int course);
    // +0x1a7 (0x4a3d20): (lap - 1) x the lap length + the sections passed +
    // the distance into this one; a car -1 is the player.
    int race_distance(int car) const;
    // +0x1af: 1 for the leader.
    int race_rank(int car) const;
    int opponent_laps(size_t i) const {return opponent_bodies_.at(i).laps;}
    // WCarFx for the player's car: dust, tracks, ripples and the splash.
    const CarEffects& effects() const {return body_.fx;}
    const CarEffects& opponent_effects(size_t i) const {return opponent_bodies_.at(i).fx;}
    const std::array<WheelContact,4>& wheels() const {return body_.wheels;}
    // The POUSSIERE option (0x6f4650) applies to every car's WCarFx.
    void set_dust_option(bool on);
    // 0x432c7f at course load: METEO 0 clears the rain flag 0x2a1cc74, 1
    // sets it and 2 sets it when rand() % 100 > 25, only on the courses
    // whose name contains cc_t1..cc_t4, dr_t1, ct_, dr_t4 or dr_b (strstr).
    // The rain itself (0x456xxx, 0x4b3315 draw) is not ported.
    void set_weather(int option,const std::string& course);
    bool raining() const {return raining_;}
    // 0x2a1b2b0: the weather draws snow (0x4b6510) instead of rain (0x4b6190).
    bool snowing() const {return snowing_;}
    int bumms() const {return bumms_;}
    // The player's 0x10000 hits (rocks) that played the +0x3d3 sound and
    // the cactus hits that played +0x3cf (0x46aa11, 30-update timer +0x3e7).
    int rock_sounds() const {return body_.rock_sounds;}
    // 0x46ab05: a pickup plays the actor's second GDSound (+0x60 -> +0x14,
    // 0x44c380) unless the brick's +0xc0 is 1 (a robot-driven car, 0x49eb20).
    int pickup_sounds() const {return body_.pickup_sounds;}
    // Debug (WPC_WEAPON): player 1 holds weapon icon 11..25 with a full charge.
    void give_weapon(int icon) {if(icon>=0x0b && icon<=0x19) {items_.weapon=icon;items_.weapon_state=3;items_.charge=3;}}
    int hit_sounds() const {return body_.hit_sounds;} // WackyAutoBomba activations heard (+0x3e3)
    int oil_sounds() const {return body_.oil_sounds;}
    // +0x25b: braking or sliding (skid marks on surface 2).
    bool skidding() const {return body_.skid.marking();}
    // +0x8db for the player (car -1) or an opponent: the acceleration
    // doubling WackyAutoBomba starts (20 updates); the bomb itself is not
    // ported, so nothing calls this yet but the tests.
    void start_boost(int car,int updates=20) {(car<0?body_:opponent_bodies_.at(size_t(car))).boost=updates;}
    // A weapon's hit on the player (car -1) or an opponent: WackyAutoBomba
    // takes the type only while none is running.
    bool hit(int car,uint16_t type);
    const BombHit& bomb(int car) const {return (car<0?body_:opponent_bodies_.at(size_t(car))).hit;}
    // The vehicle model's vertex colour range, for the hits' 0x47d890 timing.
    void set_model_colours(int vehicle,const std::array<float,3>& lo,const std::array<float,3>& hi);
    const std::vector<Landmine>& mines() const {return mines_;}
    const std::vector<Barrier>& barriers() const {return barriers_;}
    // WExplosion clones (m_idExplosion 0x3ac: a landmine's end, the TNT): their
    // position and age in updates (m_GrowTime 2, m_FadeOutTime 20).
    struct Blast {Vec3 at{};int age=0;};
    const std::vector<Blast>& blasts() const {return blasts_;}
    const std::vector<Projectile>& projectiles() const {return projectiles_;}
    // 0x4a71e0 on the racer list (+0x48): the nearest racer but owner, with
    // no hit running, within range and the cone about heading; positions
    // across the loop seam go through the loop transform (0x43b880) when
    // one side is in the first section and the other in the last. -2 none.
    int find_target(const Vec3& from,float heading,int section,float angle,int range,int owner) const;
    const std::vector<Tnt>& tnts() const {return tnts_;}
    const std::vector<Storm>& storms() const {return storms_;}
    int weapon_effect() const {return body_.weapon.id;}
    void use_weapon_now(int icon,int car=-1) {use_weapon(icon,car);} // tests
    int player_effect() const {return body_.effect;}
    bool turbo() const {return body_.turbo>0;}
    bool shielded() const {return body_.shielded;}
    void use_gadget_now(int icon,int car=-1) {use_gadget(icon,car);} // tests
    // A car's running effects (-1 the player).
    int gadget_effect(int car) const {return (car<0?body_:opponent_bodies_.at(size_t(car))).effect;}
    int weapon_effect(int car) const {return (car<0?body_:opponent_bodies_.at(size_t(car))).weapon.id;}
    void drop_mine(int car,bool detour=false,bool oil=false);
    // WPU_OilSpill's trap (0x4ddfd1): the car slides on its own motion and spins.
    void slip(int car); // WPU_Landmine (or WPU_Detour) from the player (-1) or an opponent
    const SurfaceGrip& grip() const {return body_.grip;}
    int cactus_sounds() const {return body_.cactus_sounds;}
    // Objects hidden by 0x40000 hits (the broken cacti), in hit order.
    const std::vector<uint32_t>& knocked_objects() const {return knocked_objects_;}
    // Race-order progress: route sections passed plus the current fraction.
    float player_progress() const {return progress(player_.position,player_.section,player_.loops);}
private:
    float progress(const Vec3& position,int section,int loops) const;
    float opponent_progress(size_t i) const;
    float fraction(int car) const;   // into the car's section (-1: the player)
    void step(const DriveInput& input);
    std::array<WheelContact,4> wheel_contacts(const RaceCar& car) const;
    struct Body;
    void car_effects(RaceCar& car,Body& b,bool was_drowning,bool far,bool player);
    void solid_contacts(RaceCar& car,Body& b);
    void drive(const DriveInput& input);
    // One car's motion for an update after its steering and speed: wall and
    // edge responses, ground, the loop seam and the start line.
    void move(RaceCar& car,Body& b,const VehicleTuning& tuning,bool player);
    // 0x46b88f: the car-to-car query (0x4a60c0, mask 0x100) and response.
    void car_contacts(RaceCar& car,Body& b,Vec3& move);
    void settle(RaceCar& car,Body& b,bool player);
    void cross_seam(RaceCar& car,Body& b,int step,bool player);
    void other_cars(size_t self,RoboCarState& state) const;
    void respawn(RaceCar& car,Body& b,bool player);
    void crossings(RaceCar& car,Body& b,const Vec3& before,bool player);
    void count_lap(Body& b,bool player);
    // 0x46a5a3: the glide in place of the car's update; true when it ran.
    bool glide(RaceCar& car,Body& b);
    void count_glide(Body& b,int walls);
    // 0x4a1c8b's robot pass: hide a robot leaving the player's rows, run a
    // hidden one along its rows and place it at a row's entry portal.
    bool virtual_robot(size_t i,std::vector<int>& placed);
    const RaceTrack& track_;
    VehicleTuning tuning_;
    RaceGrid grid_;
    RaceCar player_;
    Phase phase_=Phase::Ready;
    uint64_t updates_=0,phase_updates_=0,start_update_=0,finish_update_=0;
    int wrong_way_updates_=0,bend_arrow_=-1;
    void update_bend_arrow();
    void update_objects();
    void finish();
    bool championship_=false;
    std::array<int,7> totals_{},race_points_{};
    int finish_state_=0,finish_counter_=0;
    std::vector<int> standings_;
    void update_power_ups();
    void pick_up();
    void update_items(const DriveInput& input);
    // 0x4679d0 slot state: gadget +0x3a0/+0x39e/+0x3a1, weapon +0x39f/+0x39d/+0x3a2.
    struct Items {
        int charge=0,gadget=-1,weapon=-1;
        int gadget_state=0,gadget_steps=0,gadget_tick=0;
        int weapon_state=0,weapon_steps=0,weapon_tick=0;
        bool weapon_preview=false; // +0x395
    };
    Items items_,items2_; // player 1's; player 2's (two players)
    void human_items(Items& it,int vehicle,int place,Body& body,bool can_use,const DriveInput& input,int car);
    std::vector<PowerUpState> power_ups_;
    float spin_=0;
    MsvcRandom random_;
    std::vector<std::string> item_events_;
    std::vector<std::pair<int,int>> voice_events_;
    std::array<Matrix4,2> follow_{{identity_matrix(),identity_matrix()}};
    std::array<FollowCamera,2> cameras_;
    FollowCameraMembers camera_members_;
    std::array<int,2> near_lock_{}; // actor +0x8e3: 25 after a return (0x4701cf)
    void init_cameras();
    void update_cameras();
    Vec3 player_previous_{};   // the player's +0x173
    CourseObjects objects_;    // dt_t1's trams (race_objects)
    // The static ground with sh_t3's moving logs on top; the log the last
    // query hit is kept in last_log_ (ride, log).
    TrackSurface ground(float x,float z,float top,float bottom) const;
    TrackSurface ray(const Vec3& from,const Vec3& to) const;
    mutable std::array<int,2> last_log_{{-1,-1}};
    std::vector<ObjectHit> object_hits_;std::vector<int> object_effects_;
    // The running weapon effect (+0x389): its id, the updates left and, for
    // WPU_Breath, the update its projectile leaves.
    struct WeaponEffect {int id=-1,timer=0,fire_at=-1,icon=-1;};
    // Per-car simulation state beside the RaceCar: returns, drowning, laps
    // and the start-line sections visited.
    struct Body {
        int drown=0,respawns=0,impacts=0,laps=0;
        int frags=0; // +0x3a3: hits received (0x47d756), the battle count
        int log_ride=-1,log=-1; // +0x89f: the WLogBase the car last stood on
        uint64_t last_respawn=0,finish_update=0;
        unsigned visited=1;
        bool finished=false;
        CarEffects fx;
        std::array<WheelContact,4> wheels{};
        SkidState skid;
        SurfaceGrip grip; // +0x8bf..+0x8cb from the last contact's surface
        int boost=0;      // +0x8db
        BombHit hit;      // the WackyAutoBomba action (+0x36d)
        // A robot outside the player's rows (+0x2b1 0x10000, node flag
        // 0x100): hidden and still, it runs +0x2b5 along its row's length.
        bool hidden=false;int virtual_distance=0;
        // 0x4721f0's slide off a wall-flagged surface: +0x157 (0, a probe
        // 1..4, or 1), +0x237 (contact count, then ground count), +0x247
        // (glide updates, then a cooldown of 7), +0x24b (the height at the
        // fifth glide update), +0x23b (the push per update).
        int glide=0,glide_count=0,glide_timer=0,glides=0;float glide_base=0;Vec3 glide_dir{};
        float tilt_pitch=0,tilt_roll=0;   // the next update's slot steps (0x46ffb2)
        Spin spin;        // the +0x48b slots' rotation
        int motion=1;     // +0x3eb: the motion block runs while > 0
        int effect=-1;    // +0x385: the running gadget effect (8 turbo, 10 shield)
        int turbo=0;      // TurboSpeed's update counter (+0x38), 0 when off
        Shield shield{};bool shielded=false;
        WeaponEffect weapon{};
        // WackyAuto +0x48b force slots (0x4726c0, 25 of 0x28 bytes): each
        // displacement is added to the move and decays by its slot +0x18
        // (0x472890): 0.4 for rocks, 0.8 for the slide start.
        // yaw: the slot's rotation step (+0x10), decaying by yaw_decay (+0x1c;
        // < 0: by decay, as the car contacts' slots are ported).
        struct Push {Vec3 force; float decay; float yaw=0; float yaw_decay=-1;};
        std::vector<Push> pushes;
        Vec3 last{};bool has_last=false; // +0x423: the position an update ago
        int sound_timer=0,rock_sounds=0,cactus_sounds=0,pickup_sounds=0; // +0x3e7
        int bumms=0;     // WKBumm clones (cactus breaks)
        int hit_sounds=0; // +0x3e3 played on a hit's activation (0x47d768)
        int oil_sounds=0; // +0x3cb played by an oil spill (0x4de12c)
        int siren=0;      // WPU_FireSiren's life left (+0x38)
        int monster=0;    // MonsterTires' updates left
        int fly=0;float fly_gravity=0,fly_top=0; // FlyPower's counter (+0x64) and the saved rows (+0x6c)
        int cursor=-1,cursor_section=-1; // +0x2c5's +0x17 (remaining, 1/256) and its section
        int follow_lock=0; // +0x89b: 10 (robot) or 2 (player) after a car contact (0x46b64c)
    };
    Body body_;
    void update_opponents(const DriveInput& input);
    void scale_by_rank(size_t robot);
    void robot_items(size_t robot);      // WCtrlRoboCar's item decisions (0x4e5a..)
    void robot_slots(size_t robot);      // 0x4679d0 for a robot
    void robot_boxes(size_t robot);      // the +0xd0 list, every 31 updates
    RankRow rank_row_;int environment_=0,course_=0;
    void drive_opponent(Opponent& o,Body& b,bool controlled);
    // Player 2 through the player's speed, steering and move (0x466db0
    // keyboard branch); items use the robots' slots, spent on key presses.
    void drive_human(size_t i,const DriveInput& input);
    DriveInput input1_{},input2_{};
    std::vector<Opponent> opponents_;
    bool robots_near_=false,robots_enabled_=true;
    std::vector<Body> opponent_bodies_;
    bool raining_=false,snowing_=false;
    int mode_=0;
    std::array<int,3> splits_{{-1,-1,-1}};
    uint32_t record_=0;
    bool new_record_=false;
    MsvcRandom fx_random_{0x1312d6e}; // 0x5472c4, shared by every WCarFx
    int bumms_=0;
    std::vector<bool> knocked_;
    std::vector<uint32_t> knocked_objects_;
    std::vector<Landmine> mines_;
    std::vector<Projectile> projectiles_;
    std::vector<Tnt> tnts_;
    void update_tnts();
    std::vector<Storm> storms_;
    void update_storms();
    // WPU_BootKicker (11, init 0x4d8830) and WPU_Flipper (12, 0x4d9a20): the
    // target is the car the query finds at use (range + 1000, angle; flags
    // 2); it takes hit type +0x64 (0x100 kick, 0x200 flipper) when the arm,
    // moving spd (200) a step, reaches it (provisional timing), if no hit runs;
    // voice events 6 (user) and 5 (target).
    struct Strike {int owner=-1,target=-2;uint16_t type=0;int wait=0;};
    std::vector<Strike> strikes_;
    void update_strikes();
    std::vector<Barrier> barriers_;
    std::vector<Blast> blasts_;
    void update_barriers();
    void update_blasts();
    // 0x4a71e0 / 0x403d69 (provisional): the nearest car but skip within
    // range and within angle of heading, -2 when none.
    void use_weapon(int icon,int car=-1);
    void fire(int icon,int owner);
    void update_weapons();
    void use_gadget(int icon,int car=-1);
    void update_gadgets();
    void update_mines();
    void bomb_step(RaceCar& car,Body& b);
    void wall_contacts(RaceCar& car,Body& b);
};
}
#endif
