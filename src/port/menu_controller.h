#ifndef WPC_MENU_CONTROLLER_H
#define WPC_MENU_CONTROLLER_H
#include "game_profile.h"
#include "game_replay.h"
#include <optional>
#include "game_scene.h"
#include <array>
#include <map>
#include <string>
#include <vector>

namespace wpc {
bool menu_item_class(const std::string &name);
struct MenuSession {
    int game_mode=0;                 // WPC +0x1a60: race0/battle1/practice2/championship3
    int player_mode=0;               // WPC +0x1a64: single0/two-player1
    std::array<int,2> vehicles{{0,0}};
    int environment=0, course=0;
    // Championship chosen on the WMenuNodeSelectCShip page (DAT_006f41e0)
    // and the race within it; environment/course follow championship_course.
    int championship=-1, championship_race=0;
    // Championship points per vehicle (profile bytes 0x6f4675), added after
    // each finished race and cleared when a championship is chosen.
    std::array<int,7> points{};
    // The finished race: the player's rank (0x6f41c4) and its points (+0x106).
    int last_rank=0;
    std::array<int,7> last_points{};
    // The profile 0x6f4568 (unlocks, records; game_profile): fresh at start
    // (0x496590), replaced by a loaded .WSG. Its +0x105 opens Satanas;
    // 0x496e10 (the credits-page WACKY RULEZ cheat, 0x48f180) sets every
    // unlock.
    Profile profile=Profile::fresh();
    // WMenuSetVolumeItem values by item ID, once adjusted (see volume()).
    std::map<uint32_t,float> volumes;
    // The MenuPrefs block (sliders by m_nSoundType, controls, toggles).
    MenuPrefs prefs=MenuPrefs::defaults();
    // WMenuSetValueItem type 8 (METEO, item 146): 0x6f464c, 0 off, 1 rain,
    // 2 random, set to 2 (0x52aae8) by the profile defaults 0x45b990, as
    // Wine's first menu shows (ALEATOIRE); type 11 (POUSSIERE, item 588):
    // 0x6f4650, BSS, off. Profiles, which save both, are not ported.
    int weather=2;
    bool dust=false;
    // RETOUR DE FORCE (WMenuSetValueItem type 2): the profile's 0x6f4568
    // (per player through 0x6f41d4), 1 from 0x496590. Toggled at 0x48a6ef;
    // force-feedback output is not ported.
    bool force_feedback=true;
};
// The file screen of WMenuNodeLoadSave (0x495df0 enter, 0x495f30 input,
// 0x495790 drawing): eight slots of .WSG (profile) or .WSR (replay) names.
struct FileScreen {
    int mode=0;          // +0x1bd: 0 load .WSG, 1 save .WSG, 2 load .WSR, 3 save .WSR
    int state=0;         // +0x13a: 0 list, 1 overwrite?, 2 done, 3 name entry, 4 name taken
    int cursor=0;        // +0x12e, wraps 0..7
    int answer=1;        // +0x132: 0 OUI, 1 NON
    std::vector<std::string> names; // 0x2746d50 (0x4b7690: at most 8, sorted)
    std::string edit;    // the slot's text during name entry (+0x13e its length, at most 31)
    bool replace=false;  // 0x6f4230: the slot's old file is deleted before the write
    uint32_t node=0;     // nonzero while the screen is open
    bool save() const {return mode==1 || mode==3;}
    std::string extension() const {return mode<2 ? ".WSG" : ".WSR";}
};
struct MenuEvent {
    enum Kind { None, ChangedPage, ChangedSelection, ChangedSetting,
                LoadTrack, LoadSave, PlayDemo, PlayIntro, ShowCredits, Quit, Pending, ResumeGame, LoadFile, PlayReplay,
                PlayOutro, PlayWinner } kind=None;
    uint32_t node=0;
    std::string detail;
};
// Menu-node traversal recovered from 0x48fac0, with separate state for each
// runtime node. Page/item order comes from the original scene, not label text.
class MenuController {
public:
    explicit MenuController(const Node &scene_root);
    const Node &node() const { return *current_; }
    const Node *page() const;
    const Node &manager() const { return *manager_; }
    std::vector<const Node *> items() const;
    size_t selected() const;
    int availability(size_t index) const;
    bool horizontal() const;
    MenuEvent move(int direction);
    // WMenuNodeTwoPlayer (page 61, vtable 0x513dac): each player's cursor
    // (+0x12a, +0x132) and ready flag (+0x136, +0x13a). Input 0x48e320:
    // while not ready a player's left/right move their cursor (0x40316b);
    // bit 0x40 (0x52aada, accelerate) makes them ready; bit 0x10 (0x52aad8)
    // takes it back, and leaves the page for player 1 when not ready. With
    // both ready the cars are chosen (0x800000 player 1, 0x1000000 player 2)
    // and the node goes on (+0x54). A row both cursors hold is closed
    // (0x48e2d0), so they cannot meet.
    bool two_player_page() const { return current_->class_name=="WMenuNodeTwoPlayer"; }
    // The page drawn: the pause root (WMenuNodePause) of a one-player game
    // shows its option node's page with no cursor until bit 0x100 opens it
    // (Wine, 2026-11-01: no JEU EN PAUSE text); otherwise page().
    const Node *shown_page() const;
    void set_player_mode(int mode) { session_.player_mode=mode; }
    bool cursor_shown() const { return shown_page()==page(); }
    MenuEvent two_move(int player,int direction);
    MenuEvent two_ready(int player);
    MenuEvent two_back(int player);
    size_t two_cursor(int player) const { return two_.cursor[size_t(player&1)]; }
    bool two_ready_flag(int player) const { return two_.ready[size_t(player&1)]; }
    MenuEvent accept();
    MenuEvent back();
    MenuEvent adjust(int direction);
    // Enters the current node's first navigation child (the in-race pause:
    // WMenuNodePause opens its option node; provisional, from the Wine
    // capture showing the option list at once on Escape).
    MenuEvent open_first_child();
    // A WMenuSetVolumeItem's value. The slider shows (0x489aa0) the option
    // for its m_nSoundType 0x193, not m_rVolume 0x18d: 0 music 0x6f4570,
    // 1 effects 0x6f4574, 2 voice 0x6f4578, 3 the 0..100 value 0x6f4588.
    // 0x496590 sets them to 1, 1, 0.75 and 50 before the MenuPrefs registry
    // record (0x4603c0, 0x3b words at 0x6f4568) replaces them; profiles and
    // that record are not ported. Other types keep 0x18d.
    float volume(const Node& item) const;
    // A championship race was finished: add its points and step to the next
    // race (0x491cf7 case 3). Returns false after the last race, resetting
    // the race index (the totals stay until the next championship).
    // rank is the player's 1-based place; after the last race 0x496b80 may
    // unlock the next championship (Profile::championship_won).
    bool championship_finished(const std::array<int,7>& race_points={},int rank=0);
    // A finished single race (0x491cc0): one player, not battle, rank <= 3
    // calls 0x496c50 (Profile::race_won).
    void race_finished(int rank);
    // After the standings: the race's points join the totals (0x496d10)
    // and the menu opens on WMenuNodePostRace (start code 0x6f41cc: 2 for a
    // single race, 3 in a championship), whose enter callback applies the
    // unlocks (0x491c98..0x491da0).
    MenuEvent race_over(int rank,const std::array<int,7>& race_points);
    // 0x4a3473: practice's three-lap time (1/300 s) beats the course record
    // 0x6f467c[env*4+course] (low 28 bits): the record takes it with the
    // vehicle in the high 4 bits. True for a new record (+0xfa4).
    bool practice_time(int time300,int vehicle);
    // 0x494b2e: after Championship V's winner scene, MENU.WGM opens on the
    // credits page (start code 0x2c) once outro.avi has played.
    MenuEvent show_credits() { session_.championship_race=0; return enter_code(0x2c); }
    // After a winner scene that returns to MENU.WGM: the championship's
    // post-race page (start code 3, 0x6f41cc).
    MenuEvent after_winner() { return enter_code(3); }
    // 0x48cec0 from the three availability callbacks: an unavailable row's
    // texts and sprites are drawn at 0x40 instead of 0xff.
    bool greyed(size_t index) const { return grey_level(index)!=0; }
    // The page children whose banner pairs the row callbacks grey: 10 for
    // the five championships (0x4910a0) and the scenario's dr row 4
    // (0x490d60, children 8 and 9), 8 for the four variants (0x4913f0).
    int row_banners() const { const std::string &c=current_->class_name; return c=="WMenuNodeSelectTrack" ? 8 : c=="WMenuNodeSelectCShip" || c=="WMenuNodeSelectScenario" ? 10 : 0; }
    // 0 for a normal row, else the grey the row's texts and sprites take.
    int grey_level(size_t index) const;
    Profile& profile() { return session_.profile; }
    // The file screen; move/accept/back act on it while it is open.
    bool file_screen() const { return files_.node!=0; }
    const FileScreen& files() const { return files_; }
    // Name entry (state 3): printable bytes are appended up to 31, '\b'
    // erases (0x495fe6..0x4960a5); other states ignore it.
    MenuEvent type(const std::string& text);
    void set_save_directory(const std::string& directory) { save_directory_=directory; }
    // The last race's recording (the buffer 0x6abee0): REVOIR LA COURSE
    // plays it, ENREGISTRER LA COURSE saves it as a .WSR (0x4b7920), and a
    // loaded .WSR (0x4b7a30) replaces it; PlayReplay asks for replay().
    void set_last_replay(const Replay &replay) { last_replay_=replay; }
    const std::optional<Replay>& replay() const { return last_replay_; }
    // CREDITS (node 0x21e, start code 0x2c, no page): 0x48c7ee loads
    // credit.pc and 0x47f8f0 sets the scroll to 256; each menu update
    // (0x48fc54) draws 17 rows 16 apart from row -scroll/16 and steps the
    // scroll down by one (0x47f910), from 256 again past the last row.
    // 0x48fac0: on a node of code 0 or 0x10, each menu update without input
    // counts up; past the WMenu action's m_nMaxIdleTime (0x1a4: 800 in
    // menu.WGM, 300 by default) the next demo starts (true once).
    bool idle_update(bool input);
    // WMenuNodeControllerConfig (code 41, player 0x1a2): Enter on a
    // WMenuControllerItem (action 0x196) waits for a key, which 0x4969a0
    // stores in the MenuPrefs table; Escape cancels (native).
    bool binding() const { return binding_>=0; }
    MenuEvent bind_key(uint32_t dik);
    std::string control_name(const Node &item) const;
    // The executable's key-name tables (0x426930) and the language.
    void set_key_names(KeyNames names,uint32_t language) { key_names_=std::move(names); language_=language; }
    bool credits_screen() const { return credits_; }
    int credits_scroll() const { return scroll_; }
    const std::vector<CreditLine>& credits() const { return credit_lines_; }
    void set_credits(std::vector<CreditLine> lines) { credit_lines_=std::move(lines); }
    // One menu update on the credits page: the scroll and, from typed text,
    // the WACKY RULEZ sequence (0x48f180: a wrong key restarts it).
    MenuEvent credits_update(const std::string& typed);
    const MenuSession &session() const { return session_; }
    // 0x48a660 shows an option item's value through 0x405f10, which sets the
    // string (+0x130, member 0x4a) of the item's second child; 0 if none.
    uint32_t value_text(const Node &item) const;
    // The joysticks present (their names), for the JOUEUR items' device cycle.
    void set_joysticks(const std::vector<std::string>& names) {joysticks_=names;}
    // Type 5's value with a joystick: "number:name" (0x48a939); empty for the keyboard.
    std::string device_label(const Node &item) const;
    // 0x48c7a8: entering a node whose 0x19d is 0x21 (the pause's option
    // node) in a championship (app +0x1a60 = 3) sets its page's sixth item's
    // first text to string 200, REDEMARRER CHAMPIONNAT. 0 when unchanged.
    // 0x490950: in two players the pause page's first text is 198 or 199
    // (the pausing player, +0xc0 from 0x432b6e) instead of 179.
    uint32_t text_override(const Node &text) const;
    void set_pause_player(int player) { pause_player_=player&1; }
    void set_championship_race(bool on) { championship_race_=on; }
private:
    int pause_player_=0;
public:
    // Starting option values (the config file stands in for the profile).
    void set_options(int weather, bool dust) { session_.weather=weather; session_.dust=dust; }
    // MenuPrefs (0x4603c0): the option values the sliders and toggles start
    // from; prefs() returns them as changed, for the save at exit (0x4604f0).
    void set_prefs(const MenuPrefs &prefs);
    MenuPrefs prefs() const;
private:
    std::vector<std::string> joysticks_;
    void index(const Node &node, const Node *parent);
    const Node *lookup(uint32_t id) const;
    std::vector<const Node *> children(const Node &node) const;
    void clamp_selection();
    MenuEvent enter(const Node &target);
    MenuEvent selected_action(const Node &item);
    MenuEvent file_accept();
    MenuEvent post_race_accept();
    MenuEvent enter_code(int code);
    void list_files();
    KeyNames key_names_;
    uint32_t language_=3;
    struct TwoPlayer {std::array<size_t,2> cursor{{0,0}};std::array<bool,2> ready{{false,false}};};
    TwoPlayer two_;
    bool two_open(size_t row,int player) const;
    const Node *root_=nullptr,*current_=nullptr,*manager_=nullptr;
    std::map<uint32_t,const Node *> nodes_,parents_;
    std::map<uint32_t,size_t> cursors_;
    bool championship_race_=false;
    MenuSession session_;
    FileScreen files_;
    bool credits_=false;
    int idle_=0;
    int binding_=-1;
    int scroll_=256;
    size_t cheat_=0;
    std::vector<CreditLine> credit_lines_;
    std::string save_directory_;
    std::optional<Replay> last_replay_;
};
}
#endif
