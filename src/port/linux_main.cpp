// Native entry point: original assets, recovered startup cards, main-menu scene.
#include "game_assets.h"
#include "game_scene.h"
#include "game_config.h"
#include "game_exe.h"
#include "game_files.h"
#include "race_view.h"
#include "movie_player.h"
#include "race_audio.h"
#include "menu_controller.h"
#include "game_replay.h"
#include "menu_view.h"
#include "winner_scene.h"
#include "wpc_platform.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <string>

namespace {
namespace fs=std::filesystem;
fs::path file(const fs::path &directory,const std::string& name) {return wpc::game_file({directory},name);}
unsigned number(const std::string &value, const char *option) {
    char *end=nullptr; errno=0;
    unsigned long result=strtoul(value.c_str(),&end,10);
    if (value.empty() || value[0]=='-' || errno || *end || result>std::numeric_limits<unsigned>::max())
        throw std::runtime_error(std::string("Invalid ")+option+" value");
    return unsigned(result);
}
struct Options {
    fs::path data=".", disc;
    unsigned language=0, frames=0, vehicle=3;
    int vehicle2=-1; // --vehicle2: a two-player --race (debug)
    int width=640, height=480; // window size; the original 640x480 by default
    bool skip_startup=false, inspect=false, dust=false, fullscreen=false;
    std::string race; // course archive to race directly, e.g. sh/sh_t1.wgm
    fs::path config_file;            // the settings file that was read, if any
    std::string data_from="default", disc_from="unset";
};
Options options(int argc,char **argv) {
    Options result;
    // Precedence: command line, then WPC_* environment, then the config file.
    fs::path config_path;
    bool explicit_config=false;
    for (int i=1;i+1<argc;++i) if (std::string(argv[i])=="--config") { config_path=argv[i+1]; explicit_config=true; }
    if (!explicit_config) config_path=wpc::default_config_path();
    if (!config_path.empty() && (explicit_config || std::getenv("WPC_CONFIG") || fs::exists(config_path))) {
        const wpc::GameConfig cfg=wpc::load_config(config_path);
        result.config_file=config_path;
        if (cfg.data) { result.data=*cfg.data; result.data_from="config"; }
        if (cfg.disc) { result.disc=*cfg.disc; result.disc_from="config"; }
        if (cfg.language) result.language=*cfg.language;
        if (cfg.skip_intro) result.skip_startup=*cfg.skip_intro;
        if (cfg.dust) result.dust=*cfg.dust;
        if (cfg.resolution) { result.width=cfg.resolution->first; result.height=cfg.resolution->second; }
        if (cfg.fullscreen) result.fullscreen=*cfg.fullscreen;
    }
    if (const char *full=std::getenv("WPC_FULLSCREEN")) result.fullscreen=std::string(full)!="0";
    if (const char *data=std::getenv("WPC_DATA")) { result.data=data; result.data_from="WPC_DATA"; }
    if (const char *disc=std::getenv("WPC_DISC")) { result.disc=disc; result.disc_from="WPC_DISC"; }
    if (const char *language=std::getenv("WPC_LANG")) result.language=number(language,"WPC_LANG");
    for (int i=1;i<argc;++i) {
        std::string arg=argv[i];
        auto value=[&]() -> std::string {
            if (++i>=argc) throw std::runtime_error("Missing value for "+arg);
            return argv[i];
        };
        if (arg=="--config") value();
        else if (arg=="--data") { result.data=value(); result.data_from="--data"; }
        else if (arg=="--disc") { result.disc=value(); result.disc_from="--disc"; }
        else if (arg=="--lang") result.language=number(value(),"--lang");
        else if (arg=="--frames") result.frames=number(value(),"--frames");
        else if (arg=="--skip-startup") result.skip_startup=true;
        else if (arg=="--fullscreen") result.fullscreen=true;
        else if (arg=="--race") {result.race=value();result.skip_startup=true;}
        else if (arg=="--vehicle") result.vehicle=number(value(),"--vehicle");
        else if (arg=="--vehicle2") result.vehicle2=int(number(value(),"--vehicle2"));
        else if (arg=="--resolution") { const auto r=wpc::parse_resolution(value()); result.width=r.first; result.height=r.second; }
        else if (arg=="--inspect-assets") result.inspect=true;
        else if (arg=="--help") {
            wpc_log("Usage: wackyraces [--config FILE] [--data DIR] [--disc DIR] [--lang 0..6] [--skip-startup] [--frames N]\n"
                    "                  [--resolution WIDTHxHEIGHT] [--fullscreen] [--inspect-assets] [--race COURSE.wgm [--vehicle 0..6] [--vehicle2 0..6]]\n"
                    "Original installed game files and course archives are required. Their locations come from the\n"
                    "command line, else WPC_DATA/WPC_DISC/WPC_LANG, else the config file: --config, $WPC_CONFIG or\n"
                    "~/.config/wackyraces/config.ini ([paths] data/disc, [game] language/skip_intro; see\n"
                    "config/wackyraces.example.ini). --skip-startup or skip_intro = true opens the menu directly.\n"
                    "--resolution or [video] resolution sets the window size (default 640x480; the picture keeps 4:3).\n"
                    "Current port: startup, menu navigation and single-player racing without opponents' AI.\n"
                    "--race starts a single race on a course archive (e.g. sh/sh_t1.wgm), skipping the startup and menu.\n"
                    "Arrows select, Enter accepts, Escape returns. Use the window close button to exit.\n"
                    "--inspect-assets needs no display.");
            std::exit(0);
        } else throw std::runtime_error("Unknown option: "+arg);
    }
    if (result.language>6) throw std::runtime_error("Language must be 0..6");
    if (result.vehicle>6 || result.vehicle2>6) throw std::runtime_error("Vehicle must be 0..6");
    return result;
}
struct WindowDeleter { void operator()(WpcWindow *w) const { wpc_window_destroy(w); } };
// Everything a race borrows from: archives and scenes outlive track, session
// and view, which hold references into them.
struct Race {
    wpc::Archive course,general;
    wpc::Scene course_scene,general_scene;
    std::unique_ptr<wpc::RaceTrack> track;
    std::unique_ptr<wpc::RaceSession> session;
    std::unique_ptr<wpc::RaceView> view;
    std::unique_ptr<wpc::RaceAudio> audio;
    double accumulator=0;
    bool championship=false;
    bool gadget_key=false,weapon_key=false,view_key=false,view2_key=false; // keys as of the last update
    std::string name;int vehicle=0;           // for RECOMMENCER (WMenuNodeLoadTrack)
    // The in-race pause: general.wgm's WMenuBrick graph (WMenuNodePause).
    std::unique_ptr<wpc::MenuController> pause;
    std::unique_ptr<wpc::MenuView> pause_view;
    bool paused=false;double pause_start=0;
    bool backslash=false; // the '\\' key (DIK 0x2b) last update, for the pause's bit 0x100
    // DEMO: the demo's buffer and its controller cursor (0x45ba30); the race
    // takes its inputs and returns to the menu when they run out.
    std::unique_ptr<wpc::Replay> demo;
    std::unique_ptr<wpc::ReplayCursor> demo_cursor;
    wpc::Replay recording; // this race's inputs (0x45b530), for REVOIR and .WSR
};
}
int main(int argc,char **argv) {
    try {
        const Options config=options(argc,argv);
        if (!config.config_file.empty()) wpc_log("Config: %s",config.config_file.string().c_str());
        wpc_log("Game data: %s (%s); disc: %s (%s)",config.data.string().c_str(),config.data_from.c_str(),
                config.disc.empty()?"none":config.disc.string().c_str(),config.disc_from.c_str());
        wpc::Archive archive; archive.load(file(config.data,"menu.wgm").string());
        if (config.inspect) {
            size_t textures=0,empty=0;
            for (const auto &resource:archive.resources()) if (resource.tag=="GTEX") {
                auto image=archive.texture(resource.id); ++textures; if (image.rgba.empty()) ++empty;
            }
            wpc_log("WGM: %zu chunks, %zu resources, %zu decoded textures (%zu empty)",
                    archive.top_chunks().size(),archive.resources().size(),textures,empty);
            return 0;
        }
        wpc::Scene scene; scene.load(archive);
        wpc::Strings strings; strings.load(file(config.data,"strings.dat").string(),config.language);
        // A few lines exist only in the executable; without it they are skipped.
        wpc::ExeImage exe;
        try { exe.load(file(config.data,"wpc.exe")); }
        catch(const std::exception& error) { wpc_log_error("WPC.EXE unavailable: %s",error.what()); }
        wpc::KeyNames key_names;
        if (!exe.empty()) key_names.load(exe);
        // Validate all required startup assets before opening a window.
        wpc::Image cards[2];
        if (!config.skip_startup) {
            cards[0]=wpc::load_bmp(file(config.data,"l2.bmp").string());
            cards[1]=wpc::load_bmp(file(config.data,"l3.bmp").string());
        }
        wpc_log("Window: %dx%d",config.width,config.height);
        std::unique_ptr<WpcWindow,WindowDeleter> window(wpc_window_create("Wacky Races",config.width,config.height));
        if (config.fullscreen) wpc_window_set_fullscreen(window.get(),1);
        if (!window) return 1;
        WpcTexture *card_textures[2]{};
        if (!config.skip_startup) for (int i=0;i<2;++i) {
            card_textures[i]=wpc_vk_texture_create(window.get(),cards[i].width,cards[i].height,cards[i].rgba.data());
            if (!card_textures[i]) throw std::runtime_error("Startup card upload failed");
        }
        wpc::MenuController controller(scene.root());
        controller.set_key_names(key_names,config.language);
        // 0x4603c0: MenuPrefs restores the options at start; the config's
        // [game] dust still turns POUSSIERE on.
        const std::string prefs_path=wpc::config_directory()+"/MenuPrefs.bin";
        std::string credit_keys;
        int demo_index=0;
        bool attract=false; // the idle demo, started by the next menu input pass
        // CREDITS: credit.pc from the install directory (0x47f840).
        try {
            const std::string path=file(config.data,"credit.pc").string();
            std::ifstream in(path,std::ios::binary);
            controller.set_credits(wpc::parse_credits(std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>())));
        } catch(const std::exception& error) { wpc_log_error("Credits unavailable: %s",error.what()); }
        controller.set_prefs(wpc::load_prefs(prefs_path));
        if (config.dust) controller.set_options(controller.session().weather,true);
        wpc::MenuView menu(window.get(),archive,scene,strings,controller);
        wpc::GameAudio audio({config.data},{config.disc},config.language);
        // After the audio: a race's RaceAudio stops its voices in this mixer.
        std::unique_ptr<Race> race;
        wpc_log("Audio output: %s",audio.backend().c_str());
        try { audio.load_menu_bank(); }
        catch(const std::exception& error) { wpc_log_error("Menu sound bank unavailable: %s",error.what()); }
        try { audio.load_menu_effects(archive); }
        catch(const std::exception& error) { wpc_log_error("Menu effects unavailable: %s",error.what()); }
        // WMenu manager property 0x1a8: the WMenuSound every cursor move plays.
        const wpc::Node* cursor_sound=nullptr;
        if (const int id=controller.manager().integer(0x1a8)) cursor_sound=&scene.find(uint32_t(id));
        // Options sliders start at the option defaults (0x496590).
        std::function<void(const wpc::Node&)> volumes=[&](const wpc::Node& n) {
            if (n.class_name=="WMenuSetVolumeItem") audio.apply_volume_item(n,controller.volume(n));
            for (const auto& c:n.children) volumes(c);
        };
        volumes(scene.root());
        auto menu_music=[&]() {
            try { audio.play_music(""); }
            catch(const std::exception& error) { wpc_log_error("Menu music unavailable: %s",error.what()); }
        };
        // Loads a course archive (installed directory first, then the disc)
        // and starts the race; failures are logged and leave the menu active.
        std::string requested_course; // the menu's last LoadTrack request
        std::array<std::array<bool,12>,2> two_keys{}; // page 61's keys last update
        auto start_race=[&](const std::string& name,int vehicle,bool championship) {
            try {
                const auto path=wpc::game_file({config.data,config.disc},name);
                auto next=std::make_unique<Race>();
                next->course.load(path.string());next->general.load(file(config.data,"general.wgm").string());
                next->course_scene.load(next->course);next->general_scene.load(next->general);
                // Arenas (*_b.wgm) are battles (game mode 1, 0x486f90's table).
                const bool arena=name.size()>6 && name.compare(name.size()-6,6,"_b.wgm")==0;
                next->track=std::make_unique<wpc::RaceTrack>(next->course,next->course_scene,arena);
                // Replays and --race run as single races; the menu's request
                // gives its game mode (practice: alone, with the course record).
                // An arena (*_b.wgm) always runs as a battle.
                const int mode=next->track->battle()?1:championship?3:(name==requested_course?controller.session().game_mode:0);
                // Two players (0x49d580): player 2's vehicle from the menu.
                const bool menu_race=name==requested_course;
                const int vehicle2=menu_race && controller.session().player_mode==1 ? controller.session().vehicles[1] :
                                   name==config.race ? config.vehicle2 : -1;
                next->session=std::make_unique<wpc::RaceSession>(*next->track,vehicle,uint32_t(wpc_time_now()*1000),mode,vehicle2);
                {int e=0,c=0;if (mode==2 && wpc::course_indices(name,e,c) && e*4+c<20) next->session->set_record(controller.session().profile.records[size_t(e*4+c)]);}
                // The menu's POUSSIERE and METEO options (0x6f4650, 0x6f464c).
                next->session->set_dust_option(controller.session().dust);
                // Debug: WPC_WEAPON=11..25 gives player 1 that weapon, charged.
                if (const char* w=std::getenv("WPC_WEAPON"); w && *w) next->session->give_weapon(std::atoi(w));
                {int environment=0,course=0;if (wpc::course_indices(name,environment,course)) next->session->set_course(environment,course);}
                // Debug: WPC_WEATHER=0/1/2 overrides METEO (off, on, random).
                const char* forced=std::getenv("WPC_WEATHER");
                next->session->set_weather(forced && *forced ? std::atoi(forced) : controller.session().weather,name);
                wpc_log("Race options: dust %d, weather %d, rain %d",int(controller.session().dust),
                        controller.session().weather,int(next->session->raining()));
                next->session->set_camera_members(wpc::race_camera_members(next->general_scene.root()));
                next->view=std::make_unique<wpc::RaceView>(window.get(),wpc::ArchivePool{&next->course,&next->general},
                    next->general,next->course_scene,next->general_scene,strings,*next->session);
                {
                    std::array<float,3> lo,hi;
                    std::vector<int> vehicles{next->session->player().vehicle};
                    for (const auto& o:next->session->opponents()) vehicles.push_back(o.vehicle);
                    for (int v:vehicles) {next->view->model_colours(v,lo,hi);next->session->set_model_colours(v,lo,hi);}
                }
                for(size_t i=0;i<next->track->sections();++i) if(!next->track->progress_field(int(i)))
                    wpc_log("Race track: route %zu progress field disconnected; using portal distances",i);
                next->audio=std::make_unique<wpc::RaceAudio>(audio,next->general,next->course,next->general_scene,*next->session);
                try { audio.play_music(name); }
                catch(const std::exception& error) { wpc_log_error("Course music unavailable: %s",error.what()); }
                wpc_log("Race audio: music %s",wpc::music_file(name).c_str());
                if (const char* place=std::getenv("WPC_RACE_PLACE")) {
                    float x=0,z=0,yaw=0;
                    if (std::sscanf(place,"%f,%f,%f",&x,&z,&yaw)!=3) throw std::runtime_error("WPC_RACE_PLACE needs x,z,yaw");
                    next->session->place(x,z,yaw);
                }
                next->championship=championship;next->name=name;next->vehicle=vehicle;
                if (championship) next->session->set_championship(controller.session().points);
                // 0x4a8f30 state 7, environment 3 (0x6f41d8, the championship's
                // last course): Spanish, French and Portuguese lines by language
                // (0x6f46e8), else English.
                if (championship && controller.session().environment==3 && !exe.empty()) {
                    static const uint32_t lines[4][3]={{0x52d1e8,0x52d1c4,0x52d1b4},{0x52d0c4,0x52d09c,0x52d088},
                                                       {0x52d12c,0x52d104,0x52d0ec},{0x52d190,0x52d168,0x52d154}};
                    const uint32_t language=strings.language();
                    const auto& row=lines[language==2?1:language==3?2:language==6?3:0];
                    std::vector<std::string> text;
                    for(uint32_t va:row) if(auto line=exe.string_at(va);!line.empty()) text.push_back(line);
                    if(text.size()==3) next->view->set_championship_message(std::move(text));
                }
                race=std::move(next);
                const auto& car=race->session->player();
                wpc_log("Loaded course: %s; %d laps, player at (%.1f, %.1f, %.1f) section %d",path.string().c_str(),
                        race->track->laps(),car.position[0],car.position[1],car.position[2],car.section);
            } catch(const std::exception& error) {
                wpc_log_error("Course load failed: %s",error.what());
            }
        };
        wpc_log("Native Wacky Races: original startup, movies, menu, audio and single-player racing with AI opponents and part of the power-ups; profiles remain unported.");
        // 0x4acad0 startup: legal.avi (0x4ac850), L2/L3 for 2 s each, then the
        // CD's Movies\intro.avi, then the main menu.
        enum class Stage { Legal, CardL2, CardL3, Intro, Outro, Winner, Menu };
        Stage stage=Stage::Menu;
        std::unique_ptr<wpc::MoviePlayer> movie;
        double stage_start=wpc_time_now(),menu_start=stage_start;
        // Movies are opened when their stage begins; a missing file or codec
        // skips it, as the original continues after "Anim Play Error".
        auto open_movie=[&](const std::vector<fs::path>& roots,const std::string& name,int surface_width,int surface_height,
                              const std::string& soundtrack={}) {
            movie.reset();
            audio.stop_music();
            std::string reason;
            try {
                const auto path=wpc::game_file(roots,name);
                std::string track;
                if (!soundtrack.empty()) try { track=wpc::game_file({config.data},soundtrack).string(); }
                    catch(const std::exception& error) { wpc_log_error("Movie soundtrack unavailable: %s",error.what()); }
                movie=wpc::MoviePlayer::open(window.get(),path.string(),surface_width,surface_height,reason,&audio.mixer(),track);
            } catch(const std::exception& error) { reason=error.what(); }
            if (movie) wpc_log("Playing movie %s: %zu frames, %s",name.c_str(),movie->frames(),movie->has_audio()?"with sound":"silent");
            else wpc_log("Movie skipped: %s: %s",name.c_str(),reason.c_str());
            return movie!=nullptr;
        };
        auto enter_stage=[&](Stage next,double now) {
            for (;;) {
                stage=next; stage_start=now;
                if (stage==Stage::Legal) {
                    if (open_movie({config.data},"legal.avi",640,480)) return;
                    next=Stage::CardL2; continue;
                }
                if (stage==Stage::Intro) {
                    if (open_movie({config.disc,config.data},"Movies/intro.avi",320,240,"Voice/iaudio.avi")) return;
                    next=Stage::Menu; continue;
                }
                // 0x432cdc: movie selector 1 plays the CD's outro with its voice track.
                if (stage==Stage::Outro) {
                    if (open_movie({config.disc,config.data},"Movies/outro.avi",320,240,"Voice/oaudio.avi")) return;
                    next=Stage::Menu; continue;
                }
                movie.reset();
                if (stage==Stage::CardL2) wpc_log("Presenting L2.bmp");
                else if (stage==Stage::CardL3) wpc_log("Presenting L3.bmp");
                else { menu_start=now; wpc_log("Presenting main menu"); menu_music(); }
                return;
            }
        };
        // Championship winner scenes (0x486fe0 -> win\win*.wgm, then 0x494b20),
        // at the menu's 30 updates a second.
        std::unique_ptr<wpc::WinnerScene> winner;
        double winner_start=0;uint64_t winner_updates=0;bool winner_accel=true;
        const auto canonical=wpc::member_names(archive);
        int winner_vehicle=0,winner_environment=3,winner_course=0;
        auto open_winner=[&](const std::string& name,double now) {
            winner.reset();
            try {
                const auto path=wpc::game_file({config.disc,config.data},name);
                winner=std::make_unique<wpc::WinnerScene>(window.get(),audio,path.string(),canonical,winner_vehicle,winner_environment,winner_course);
                stage=Stage::Winner;winner_start=now;winner_updates=0;
                audio.stop_music(); // 0x431690 asks for music\Win*.snd, which the disc lacks
                wpc_log("Winner scene %s",name.c_str());
                return true;
            } catch(const std::exception& error) {wpc_log_error("Winner scene %s unavailable: %s",name.c_str(),error.what());}
            return false;
        };
        // 0x494b20: DickOff after ct_t4, the outro and credits after dr_t4,
        // else the menu (the post-race page).
        auto winner_done=[&](double now) {
            const auto next=winner?winner->next():wpc::WinnerScene::Next::Menu;
            winner.reset();
            if (next==wpc::WinnerScene::Next::DickOff && open_winner("Win/Dickoff.WGM",now)) return;
            if (next==wpc::WinnerScene::Next::Outro) {enter_stage(Stage::Outro,now);return;}
            enter_stage(Stage::Menu,now);
            controller.after_winner();menu.prepare();
        };
        enter_stage(config.skip_startup?Stage::Menu:Stage::Legal,wpc_time_now());
        // Debug: WPC_WINNER=vehicle,environment,course opens that winner scene.
        if (const char* w=std::getenv("WPC_WINNER"); w && *w) {
            if (std::sscanf(w,"%d,%d,%d",&winner_vehicle,&winner_environment,&winner_course)==3 && winner_vehicle>=0 && winner_vehicle<7)
                open_winner(wpc::winner_scene_file(winner_vehicle),wpc_time_now());
        }
        if (!config.race.empty()) start_race(config.race,int(config.vehicle),false);
        unsigned rendered=0;
        double previous_now=wpc_time_now();
        bool up=false,down=false,left=false,right=false,enter=false,escape=false;
        bool quit=false;
        while (!quit && !wpc_window_poll_events(window.get())) {
            const double race_clock=previous_now;
            double now=wpc_time_now();
            previous_now=now;
            bool new_up=wpc_input_key_down(window.get(),WPC_KEY_UP);
            bool new_down=wpc_input_key_down(window.get(),WPC_KEY_DOWN);
            bool new_enter=wpc_input_key_down(window.get(),WPC_KEY_RETURN);
            bool new_left=wpc_input_key_down(window.get(),WPC_KEY_LEFT);
            bool new_right=wpc_input_key_down(window.get(),WPC_KEY_RIGHT);
            bool new_escape=wpc_input_key_down(window.get(),WPC_KEY_ESCAPE);
            if (stage==Stage::CardL2 || stage==Stage::CardL3) {
                // FUN_00413bd0(2): whole seconds of timeGetTime.
                if (now-stage_start>=2) enter_stage(stage==Stage::CardL2?Stage::CardL3:Stage::Intro,now);
            } else if (stage==Stage::Legal || stage==Stage::Intro || stage==Stage::Outro) {
                const bool skip=new_escape && !escape;
                const bool playing=movie && !skip && movie->update(now);
                if (!playing) {
                    if (movie) wpc_log("Movie %s at frame %zu of %zu",skip?"skipped by Escape":"finished",movie->frame(),movie->frames());
                    const bool outro=stage==Stage::Outro;
                    enter_stage(stage==Stage::Legal?Stage::CardL2:Stage::Menu,now);
                    if (outro) {
                        // 0x494b7a: the menu opens on start code 0x2c, the credits.
                        const auto e=controller.show_credits();
                        menu.prepare();
                        wpc_log("After the outro: menu event %d, node 0x%x",int(e.kind),controller.node().id);
                    }
                }
            } else if (stage==Stage::Winner) {
                // 0x494bf3: the pressed input bits & 0x52aada (0x40, accelerate:
                // the player's control item 0).
                const bool accel=wpc_input_code_down(window.get(),wpc::dik_to_linux(controller.prefs().controls(0)[0].first))!=0;
                const bool skip=accel && !winner_accel;winner_accel=accel;
                const auto due=uint64_t((now-winner_start)/wpc::menu_update_seconds);
                bool running=true;
                for (bool first=true;running && winner_updates<due;first=false) {
                    ++winner_updates;
                    running=winner && winner->update(first && skip);
                }
                if (!running) winner_done(now);
            } else if (race) {
                const bool finished=race->session->phase()==wpc::RaceSession::Phase::Finished;
                bool leave=false;
                // A demo ends on Escape or Enter (native keys; the original's
                // exit input is not traced) or when its inputs run out.
                if (race->demo_cursor && ((new_escape && !escape) || (new_enter && !enter) || race->demo_cursor->done())) {
                    leave=true;wpc_log("Demo ended at update %llu",(unsigned long long)race->session->updates());
                }
                // Escape opens the pause (general.wgm WMenuNodePause); the race
                // stops while it is shown.
                if (!race->demo_cursor && !finished && !race->paused && new_escape && !escape) {
                    try {
                        // 0x490950: each pause starts on the root page 243, "JEU EN
                        // PAUSE" (179); the view holds the controller, so both
                        // are made again.
                        race->pause_view.reset();
                        race->pause=std::make_unique<wpc::MenuController>(race->general_scene.root());
                        race->pause->set_championship_race(race->championship);
                        race->pause->set_prefs(controller.prefs()); // one 0x6f4568 block
                        race->pause->set_player_mode(race->session->two_players()?1:0);
                        // 0x432b15: player 1's input 0x800 (or player 2's in two
                        // players) pauses; the native Escape is player 1's.
                        race->pause->set_pause_player(0);
                        race->pause_view=std::make_unique<wpc::MenuView>(window.get(),race->general,race->general_scene,strings,*race->pause);
                        race->pause_view->set_overlay(true);
                        race->pause_view->prepare();
                        race->paused=true;race->pause_start=now;
                        wpc_log("Race paused at update %llu",(unsigned long long)race->session->updates());
                    } catch (const std::exception& error) {
                        wpc_log_error("Pause menu unavailable: %s",error.what());
                        leave=true;
                    }
                } else if (race->paused) {
                    wpc::MenuEvent event;
                    const bool root=race->pause->node().class_name=="WMenuNodePause";
                    // 0x4909f0 on the root page: input bit 0x100 opens the
                    // option node (+0x12a), bit 0x800 resumes. The keyboard's
                    // fixed bindings (0x40c5a3..0x40c739) give device bits
                    // 0x1000 to DIK 0x2b ('\') and 0x2000 to Return, which
                    // 0x4399b0 turns into 0x100 and 0x800 (Wine-checked:
                    // Return resumes, '\' opens the options). Escape does
                    // not resume (Wine, 2026-11-01): on the root it starts
                    // the pause again (the list slides in anew, the race
                    // stays stopped); elsewhere it does nothing.
                    const bool backslash=wpc_input_code_down(window.get(),wpc::dik_to_linux(0x2b))!=0;
                    const bool new_backslash=backslash && !race->backslash;race->backslash=backslash;
                    if (root) {
                        if (new_backslash) event=race->pause->open_first_child();
                        else if (new_enter && !enter) event.kind=wpc::MenuEvent::ResumeGame;
                        else if (new_escape && !escape) {race->pause_view->prepare();race->pause_start=now;}
                    } else if (new_up && !up) event=race->pause->move(-1);
                    else if (new_down && !down) event=race->pause->move(1);
                    else if (new_left && !left) event=race->pause->adjust(-1);
                    else if (new_right && !right) event=race->pause->adjust(1);
                    else if (new_enter && !enter) event=race->pause->accept();
                    // Escape: no effect on the option node (0x19e set) or on
                    // OUI/NON (Wine-checked).
                    if (event.kind==wpc::MenuEvent::ChangedSetting) {
                        const auto& values=race->pause->session().volumes;
                        const auto found=values.find(event.node);
                        if (found!=values.end()) audio.apply_volume_item(race->general_scene.find(event.node),found->second);
                        controller.set_prefs(race->pause->prefs());
                    } else if (event.kind==wpc::MenuEvent::ResumeGame) {
                        race->paused=false;wpc_log("Race resumed");
                    } else if (event.kind==wpc::MenuEvent::LoadTrack) {
                        // RECOMMENCER: the same course again (the original's
                        // reload path is not traced).
                        const std::string name=race->name;const int vehicle=race->vehicle;const bool champ=race->championship;
                        wpc_log("Race restarted from the pause menu");
                        race.reset();
                        start_race(name,vehicle,champ);
                    } else if (event.kind==wpc::MenuEvent::LoadFile) {
                        leave=true; // QUITTER, then OUI: back to the menu file
                    }
                    if (race && race->paused) race->pause_view->prepare();
                }
                if (!race) {
                } else if (leave || (finished && new_enter && !enter && race->session->accept())) {
                    wpc_log("Race left after %.2f s, lap %d/%d, rank %d",race->session->race_seconds(),
                            race->session->laps_completed(),race->session->laps(),race->session->rank());
                    // A finished championship race moves on (0x491cf7); the
                    // track page then loads the next course on Enter.
                    // A finished race opens the post-race menu (0x6f41cc 2 or 3).
                    wpc::MenuEvent after{};
                    if (finished && !race->demo) {
                        controller.set_last_replay(race->recording);
                        if (race->session->mode()==2) {
                            const bool record=controller.practice_time(race->session->split_total(),race->session->player().vehicle);
                            wpc_log("Practice time %d/300 s%s",race->session->split_total(),record?": new record":"");
                        }
                        after=controller.race_over(race->session->rank(),race->session->race_points());
                    }
                    race.reset();
                    if (after.kind==wpc::MenuEvent::PlayWinner) {
                        wpc_log("%s: winner scene",after.detail.c_str());
                        const auto& s=controller.session();
                        winner_vehicle=s.vehicles[0];winner_environment=s.environment;winner_course=s.course;
                        if (!open_winner(wpc::winner_scene_file(winner_vehicle),now)) {controller.after_winner();after.kind=wpc::MenuEvent::ChangedPage;}
                    }
                    if (after.kind==wpc::MenuEvent::ChangedPage) {
                        menu.prepare();
                        wpc_log("Post-race menu: node 0x%x, page 0x%x",controller.node().id,controller.page()->id);
                    }
                    if (stage==Stage::Menu) {
                        menu_music();
                        menu_start=now; // the original reloads MENU.WGM after a race
                    }
                    wpc_log("Returned from race to menu");
                } else if (!race->paused) {
                    // The player's keys from the MenuPrefs control table (0x4966f0
                    // defaults: arrows, '/' gadget, right Shift weapon).
                    const auto controls=controller.prefs().controls(0);
                    const auto held=[&](size_t action){return wpc_input_code_down(window.get(),wpc::dik_to_linux(controls[action].first))!=0;};
                    wpc::DriveInput input;
                    input.accelerate=held(0); input.brake=held(1); input.left=held(2); input.right=held(3);
                    // Gadget and weapon act on presses: 0x6e42b0 is the input word
                    // ANDed with the previous update's (0x45c205).
                    const bool gadget_down=held(6);
                    const bool weapon_down=held(7);
                    // Control 4 (Return) toggles the camera view on a press
                    // (0x52aad0), control 5 ('.') holds the rear view (0x52aad4).
                    const bool view_down=held(4);
                    input.look_back=held(5);
                    // Player 2's keys (MenuPrefs controls 1: G, B, V, N, Z, X).
                    wpc::DriveInput input2;
                    if (race->session->two_players()) {
                        const auto c2=controller.prefs().controls(1);
                        const auto held2=[&](size_t a){return wpc_input_code_down(window.get(),wpc::dik_to_linux(c2[a].first))!=0;};
                        input2.accelerate=held2(0);input2.brake=held2(1);input2.left=held2(2);input2.right=held2(3);
                        input2.gadget=held2(6);input2.weapon=held2(7);
                        input2.look_back=held2(5);
                    }
                    // Fixed original updates; long stalls do not replay more than 5.
                    race->accumulator=std::min(race->accumulator+(now-race_clock),5*wpc::race_update_seconds);
                    while (race->accumulator>=wpc::race_update_seconds) {
                        race->accumulator-=wpc::race_update_seconds;
                        const auto before=race->session->phase();
                        input.gadget=gadget_down && !race->gadget_key; input.weapon=weapon_down && !race->weapon_key;
                        race->gadget_key=gadget_down; race->weapon_key=weapon_down;
                        input.view=view_down && !race->view_key; race->view_key=view_down;
                        if (race->session->two_players()) {
                            const bool down=wpc_input_code_down(window.get(),wpc::dik_to_linux(controller.prefs().controls(1)[4].first))!=0;
                            input2.view=down && !race->view2_key;race->view2_key=down;
                        }
                        // A demo's input word for this update (0x45ba30); a normal
                        // race records its own (0x45b530).
                        if (race->demo_cursor) input=wpc::ReplayCursor::input(race->demo_cursor->next());
                        else race->recording.record(wpc::Replay::word(input));
                        race->session->update(input,input2);
                        for (const auto& item:race->session->take_item_events())
                            wpc_log("Power-up at update %llu: %s",(unsigned long long)race->session->updates(),item.c_str());
                        race->audio->update();
                        for (const auto& sound:race->audio->take_events())
                            wpc_log("Race sound at update %llu: %s",(unsigned long long)race->session->updates(),sound.c_str());
                        if (race->session->phase()!=before)
                            wpc_log("Race phase %d at update %llu, %.2f s",int(race->session->phase()),
                                    (unsigned long long)race->session->updates(),race->session->race_seconds());
                    }
                }
            } else {
                auto dispatch=[&](const wpc::MenuEvent &event) {
                    if (event.kind==wpc::MenuEvent::Quit) { quit=true; return; }
                    // A replay (a demo, REVOIR or a .WSR): the recorded course and
                    // vehicle, the player driven by the buffer's inputs.
                    const auto play=[&](std::unique_ptr<wpc::Replay> replay,const std::string& name) {
                        wpc_log("Replay %s: course %d/%d, vehicle %u, %d updates",name.c_str(),replay->environment,replay->course,replay->session[2],replay->frames);
                        start_race(wpc::course_file(0,replay->environment,replay->course),int(replay->session[2]),false);
                        if (race) {race->demo=std::move(replay);race->demo_cursor=std::make_unique<wpc::ReplayCursor>(*race->demo);}
                    };
                    if (event.kind==wpc::MenuEvent::PlayDemo) {
                        // DEMO (0x48a543): demo1..3.pc in turn (0x52beac, wrap after 3).
                        const std::string name="demo"+std::to_string(demo_index+1)+".pc";
                        demo_index=(demo_index+1)%3;
                        try {
                            std::ifstream in(file(config.data,name),std::ios::binary);
                            play(std::make_unique<wpc::Replay>(wpc::Replay::parse(std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()))),name);
                        } catch(const std::exception& error) { wpc_log_error("Demo %s unavailable: %s",name.c_str(),error.what()); }
                        return;
                    }
                    if (event.kind==wpc::MenuEvent::PlayReplay && controller.replay()) {
                        play(std::make_unique<wpc::Replay>(*controller.replay()),event.detail.empty()?"of the last race":event.detail);
                        return;
                    }
                    if (event.kind==wpc::MenuEvent::PlayIntro) {
                        // SEQUENCE D'INTRODUCTION replays Movies\intro.avi.
                        enter_stage(Stage::Intro,now); return;
                    }
                    if (event.kind==wpc::MenuEvent::ChangedPage) {
                        menu.prepare();
                        wpc_log("Menu navigation: node 0x%x, page 0x%x, selection %zu",controller.node().id,controller.page()->id,controller.selected());
                    } else if (event.kind==wpc::MenuEvent::LoadTrack) {
                        const auto &session=controller.session();
                        wpc_log("Race request: mode %d, players %d, vehicle %d, environment %d, course %d",
                            session.game_mode,session.player_mode+1,session.vehicles[0],session.environment,session.course);
                        requested_course=wpc::course_file(session.game_mode,session.environment,session.course);
                        start_race(requested_course,session.vehicles[0],
                                   session.game_mode==3 && session.championship>=0);
                        if (race) {
                            // 0x45b310: the session words, course and rain flag head the buffer.
                            wpc::Replay& r=race->recording;
                            r.session={{uint32_t(session.game_mode),uint32_t(session.player_mode),uint32_t(session.vehicles[0]),uint32_t(session.vehicles[1])}};
                            r.environment=session.environment;r.course=session.course;r.flag=race->session->raining();
                        }
                    } else if (event.kind==wpc::MenuEvent::ChangedSelection) {
                        const auto items=controller.items();
                        if (controller.file_screen()) {
                            if (cursor_sound) audio.play_menu_sound(*cursor_sound);
                        } else if (controller.selected()<items.size()) {
                            // 0x488ba0: queue the item's voice (20 updates), then play the
                            // cursor sound at once, cutting off any speaking voice.
                            if (const uint32_t sample=audio.queue_menu_item(*items[controller.selected()]))
                                wpc_log("Menu voice queued: item 0x%x sample 0x%x",items[controller.selected()]->id,sample);
                            if (cursor_sound) audio.play_menu_sound(*cursor_sound);
                        }
                    } else if (event.kind==wpc::MenuEvent::ChangedSetting) {
                        const auto& values=controller.session().volumes;
                        const auto found=values.find(event.node);
                        if (found!=values.end()) audio.apply_volume_item(scene.find(event.node),found->second);
                    } else if (event.kind!=wpc::MenuEvent::None) {
                        wpc_log("Menu event %d, node 0x%x: %s",int(event.kind),event.node,event.detail.c_str());
                    }
                };
                if (attract) {attract=false;wpc::MenuEvent demo{};demo.kind=wpc::MenuEvent::PlayDemo;dispatch(demo);}
                const bool binding=controller.binding();
                const bool two_page=controller.two_player_page();
                if (two_page) {
                    // Page 61: each player's MenuPrefs keys (left, right,
                    // accelerate = ready 0x40, brake = back 0x10, provisional
                    // for the keyboard); player 1 also backs out with Escape.
                    for (int p=0;p<2;++p) {
                        const auto keys=controller.prefs().controls(p);
                        const auto pressed=[&](size_t item) {
                            const bool down=wpc_input_code_down(window.get(),wpc::dik_to_linux(keys[item].first))!=0;
                            const bool fresh=down && !two_keys[size_t(p)][item];two_keys[size_t(p)][item]=down;return fresh;
                        };
                        const bool l=pressed(2),r=pressed(3),go=pressed(0),stop=pressed(1);
                        if (l) dispatch(controller.two_move(p,-1));
                        if (r) dispatch(controller.two_move(p,1));
                        if (go) dispatch(controller.two_ready(p));
                        if (stop || (p==0 && new_escape && !escape)) dispatch(controller.two_back(p));
                        if (!controller.two_player_page()) break;
                    }
                } else if (binding) {
                    // The controls page waits for a key: the next press binds
                    // it (Escape cancels); the menu keys are suspended.
                    if (const int code=wpc_input_last_code(window.get())) {
                        const auto e=controller.bind_key(code==1?0:wpc::linux_to_dik(code));
                        wpc_log("Controls: %s",e.detail.c_str());
                    }
                } else if (controller.horizontal()) {
                    if (new_left && !left) dispatch(controller.move(-1));
                    if (new_right && !right) dispatch(controller.move(1));
                } else {
                    if (new_up && !up) dispatch(controller.move(-1));
                    if (new_down && !down) dispatch(controller.move(1));
                    if (new_left && !left) dispatch(controller.adjust(-1));
                    if (new_right && !right) dispatch(controller.adjust(1));
                }
                if (!binding && !two_page) {
                    if (new_enter && !enter) dispatch(controller.accept());
                    if (new_escape && !escape) dispatch(controller.back());
                    if (controller.binding()) wpc_input_last_code(window.get()); // not the Enter that asked
                }
                // Name entry on the file screen takes the typed text.
                char typed[64];
                const size_t count=wpc_input_text(window.get(),typed,sizeof typed);
                if (count && controller.file_screen()) dispatch(controller.type(std::string(typed,count)));
                // The credits page reads the keys at its own updates (0x48f180).
                if (count && controller.credits_screen()) credit_keys.append(typed,count);
            }
            if (stage==Stage::Menu && !race) {
                // Menu updates at 30 per second: the voice queue, then
                // WMenuAnimation and the GDFilm2 players.
                const bool any_key=new_up||new_down||new_left||new_right||new_enter||new_escape;
                menu.update(now-menu_start,[&] {
                    if (!race && controller.idle_update(any_key)) {
                        wpc_log("Menu idle: attract demo");
                        attract=true;
                    }
                    if (controller.credits_screen()) {
                        const wpc::MenuEvent cheat=controller.credits_update(credit_keys);
                        credit_keys.clear();
                        if (cheat.kind!=wpc::MenuEvent::None) wpc_log("Credits: %s",cheat.detail.c_str());
                    }
                    const auto voice=audio.menu_voice_update();
                    wpc::MenuAnimation *animation=menu.animation();
                    if (voice.started) {
                        wpc_log("Menu voice: sample 0x%x, actor %d, %d/60 s",voice.sample,voice.actor,voice.duration60);
                        if (animation) animation->speech_started(voice.actor,voice.duration60);
                    }
                    if (voice.finished && animation) animation->speech_finished();
                });
            }
            up=new_up; down=new_down; left=new_left; right=new_right; enter=new_enter; escape=new_escape;
            if (quit) break;
            // LUMINOSITE (prefs +0x20) goes to the renderer at once (0x489bce).
            wpc_vk_set_brightness(window.get(),int(controller.prefs().word(0x20)));
            int frame_status=wpc_vk_begin_frame(window.get());
            if (frame_status<0) return 1;
            if (frame_status>0) {
                if (stage==Stage::Legal || stage==Stage::Intro || stage==Stage::Outro) movie->draw();
                else if (stage==Stage::Menu && race) {
                    race->view->draw();
                    if (race->paused) race->pause_view->draw(now-race->pause_start);
                }
                else if (stage==Stage::Menu) menu.draw(now-menu_start);
                else if (stage==Stage::Winner && winner) winner->draw();
                else {
                    int width,height; wpc_window_get_size(window.get(),&width,&height);
                    float scale=std::min(width/640.f,height/480.f);
                    int draw_width=int(640*scale),draw_height=int(480*scale);
                    wpc_vk_clear(window.get(),0,0,0);
                    wpc_vk_draw_texture(window.get(),(width-draw_width)/2,(height-draw_height)/2,
                                        draw_width,draw_height,card_textures[stage==Stage::CardL2?0:1]);
                }
                if (!wpc_vk_end_frame(window.get())) return 1;
                ++rendered;
                if (config.frames && rendered>=config.frames) break;
            }
            double remaining=1.0/60.0-(wpc_time_now()-now);
            if (remaining>0) wpc_sleep_ms(uint32_t(remaining*1000));
        }
        // 0x4604f0 at application exit (0x4ad650).
        try { wpc::save_prefs(prefs_path,controller.prefs()); }
        catch(const std::exception& error) { wpc_log_error("MenuPrefs not saved: %s",error.what()); }
        wpc_log("Native shutdown after %u presented frames",rendered);
        return 0;
    } catch (const std::exception &error) {
        wpc_log_error("Wacky Races: %s",error.what()); return 1;
    }
}
