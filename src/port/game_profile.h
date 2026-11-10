#ifndef WPC_GAME_PROFILE_H
#define WPC_GAME_PROFILE_H
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace wpc {
// The player profile at 0x6f4568, as far as a .WSG save holds it: the five
// blocks 0x004b7b40 writes (20, 5, 1, 80 and 20 bytes, 126 in all) from
// +0xec, +0x100, +0x105, +0x114 and +0x164.
struct Profile {
    std::array<uint8_t,20> tracks{};        // +0xec [environment*4+variant], bit 0: single race open
    std::array<uint8_t,5> championships{};  // +0x100..+0x104, bit 0: open (+0x104: practice's dr)
    uint8_t satanas=0;                      // +0x105, bit 0: Satanas et Diabolo selectable
    std::array<uint32_t,20> records{};      // +0x114: low 28 bits best time (300 a second), high 4 the vehicle
    std::array<uint8_t,20> wins{};          // +0x164: bit v set when vehicle v won that race
    // 0x496590: variant 0 of cc, sh, dt and ct, championship I, the record
    // table 0x52cac0 with vehicle 6.
    static Profile fresh();
    // Raw .WSG bytes (no header, version or checksum in the original).
    std::vector<uint8_t> serialize() const;
    static Profile parse(const std::vector<uint8_t>& bytes);
    // 0x496a40: environment row 4 (dr) on the scenario page; game mode 2
    // reads +0x104, the others any open dr variant.
    bool scenario_open(int game_mode) const;
    // 0x496af0: championship i on WMenuNodeSelectCShip.
    bool championship_open(int index) const;
    // 0x496a80: variant i of an environment on WMenuNodeSelectTrack; game
    // mode 2 reads the championship flags (+0x104 for dr).
    bool track_open(int environment,int variant,int game_mode) const;
    // 0x496c50 (from 0x491cdd, single player, not battle, rank <= 3): a
    // win (rank 1, 1-based) opens the environment's next variant and marks
    // the vehicle in +0x164.
    void race_won(int environment,int variant,int rank,int vehicle,bool two_players);
    // 0x496b80 (from 0x491da0 after a championship's last race): with rank
    // <= 3 and the player's vehicle leading the points (0x496d90, ties
    // included), championship c < 4 opens c + 1 and variant c of cc, sh, dt
    // and ct, c = 3 also Satanas, c = 4 every dr variant.
    void championship_won(int championship,int rank,int vehicle,const std::array<int,7>& points);
    // 0x496b40: every cc..ct race has been won by some vehicle.
    bool all_races_won() const;
    // 0x496e10 (the credits page's WACKY RULEZ, 0x48f180): every course,
    // championship and Satanas open, every race won by all seven (0x7f).
    void unlock_all();
};
// credit.pc (read by 0x47f840): a u32 count, then 50-byte rows of a style
// byte (0 name, 1 heading, 2 title) and NUL-terminated text.
struct CreditLine {int style=0;std::string text;};
std::vector<CreditLine> parse_credits(const std::vector<uint8_t>& bytes);
// The options block 0x6f4568..+0xec (0x3b words) the MenuPrefs registry
// value holds (0x4603c0 load at start, 0x4604f0 save at exit 0x4ad650): a
// 0x180-byte record whose first word is 0x178, the block from +8.
struct MenuPrefs {
    std::array<uint8_t,0xec> block{};
    // 0x496590 and 0x4966f0 (keyboards: +0x18/+0x1c = -1).
    static MenuPrefs defaults();
    uint32_t word(size_t offset) const;
    void set_word(size_t offset,uint32_t value);
    float real(size_t offset) const;
    void set_real(size_t offset,float value);
    // +0 RETOUR DE FORCE (player 1, +4 player 2); +8 MUSIQUE, +0xc EFFETS,
    // +0x10 VOIX; +0xe4 METEO; +0xe8 POUSSIERE.
    float level(int sound_type) const {return real(8+4*size_t(sound_type));}
    void set_level(int sound_type,float v) {set_real(8+4*size_t(sound_type),v);}
    // A player's twelve (key, action bit) pairs from +0x24 (+0x84 for player
    // 2): DirectInput scan codes when the device +0x18/+0x1c is -1.
    std::array<std::pair<uint32_t,uint32_t>,12> controls(int player) const;
    // +0x18 / +0x1c: the player's input device, -1 the keyboard, else a joystick.
    int device(int player) const {return int(word(player?0x1c:0x18));}
    void set_device(int player,int device) {set_word(player?0x1c:0x18,uint32_t(device));}
    // 0x4966f0 (called for both players after a device change, 0x48a8c7): the
    // keyboard keys, or with a joystick its buttons 4, 5, 100 (none), 100, 0,
    // 1, 2, 3 (player 2's joystick branch is not traced: the same, provisional).
    void reset_controls(int player);
    std::vector<uint8_t> record() const;
    // The original ignores a record of another size (0x178 word); so does this.
    static MenuPrefs parse(const std::vector<uint8_t>& record);
};
// DirectInput scan codes (the MenuPrefs control table) against Linux input
// codes (X keycode - 8): equal below 0x59 (through F12); the extended keys (0x80 | code)
// have their own Linux codes. 0 when there is none.
int dik_to_linux(uint32_t dik);
uint32_t linux_to_dik(int code);
// A key's name for the controls page (the original asks DirectInput for it;
// native French names, provisional).
std::string dik_name(uint32_t dik);
// 0x4969a0: player's control item takes key; with both players on the
// keyboard a key the other player holds is refused (false); the item that
// had the key gets the old one.
bool set_control(MenuPrefs& prefs,int player,int item,uint32_t key);
// 0x426930 (the controls page's key names): the DirectInput code's index in
// the 106-entry table 0x526a4c; in French (language 3) with flag 2 of
// 0x526f44 the French name 0x526d9c, otherwise GetKeyNameTextA's, falling
// back to the French (language 3) or English (0x526bf4) table; 0x45 takes
// 0x527d14. The tables are read from the installed WPC.EXE (no game text in
// the sources). GetKeyNameTextA fails for the extended codes (>= 0x80), so
// those take the table names (the arrows' are font glyphs); for the others
// the native French names stand for a French Windows' (English: the table).
class ExeImage;
class KeyNames {
public:
    void load(const ExeImage& exe);
    bool empty() const {return codes_.empty();}
    std::string name(uint32_t dik,uint32_t language) const;
private:
    std::vector<uint32_t> codes_,flags_;
    std::vector<std::string> english_,french_;
    std::string pause_;
};
// $XDG_CONFIG_HOME/wackyraces (else ~/.config/wackyraces).
std::string config_directory();
// MenuPrefs.bin there: defaults when absent or invalid.
MenuPrefs load_prefs(const std::string& path);
void save_prefs(const std::string& path,const MenuPrefs& prefs);
// Save names under the user's save directory, as 0x004b7690 lists them: at
// most eight, extension stripped, sorted bytewise; the directory is created.
std::vector<std::string> list_saves(const std::string& directory,const std::string& extension);
// $XDG_DATA_HOME/wackyraces/save (else ~/.local/share/...): never the install
// directory (the original writes save\ under it).
std::string save_directory();
Profile load_profile(const std::string& path);
void save_profile(const std::string& path,const Profile& profile);
}
#endif
