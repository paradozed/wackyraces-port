#include "game_profile.h"
#include "game_exe.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace wpc {
namespace {
constexpr size_t wsg_size=126;
// 0x52cac0: the record table a fresh profile copies (| 0x60000000).
constexpr uint32_t default_records[20]={0xa410,0xdea8,0xdea8,0x80e8,0xafc8,0xa9ec,0x86c4,0xcd14,0x86c4,0x6f54,
                                        0x57e4,0xb5a4,0xafc8,0xe484,0xcd14,0xc738,0xc738,0xf03c,0x639c,0xcd14};
}
Profile Profile::fresh() {
    Profile p;
    for(int e=0;e<4;++e) p.tracks[size_t(e*4)]=1;
    p.championships[0]=1;
    for(size_t i=0;i<20;++i) p.records[i]=default_records[i]|0x60000000u;
    return p;
}
std::vector<uint8_t> Profile::serialize() const {
    std::vector<uint8_t> out;out.reserve(wsg_size);
    out.insert(out.end(),tracks.begin(),tracks.end());
    out.insert(out.end(),championships.begin(),championships.end());
    out.push_back(satanas);
    for(uint32_t r:records) for(int i=0;i<4;++i) out.push_back(uint8_t(r>>(8*i)));
    out.insert(out.end(),wins.begin(),wins.end());
    return out;
}
Profile Profile::parse(const std::vector<uint8_t>& b) {
    // The original reads without checking the counts; a short or long file
    // is rejected here.
    if(b.size()!=wsg_size) throw std::runtime_error("profile: a .WSG save is 126 bytes");
    Profile p;size_t at=0;
    for(auto& v:p.tracks) v=b[at++];
    for(auto& v:p.championships) v=b[at++];
    p.satanas=b[at++];
    for(auto& r:p.records) {r=uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;at+=4;}
    for(auto& v:p.wins) v=b[at++];
    return p;
}
bool Profile::scenario_open(int game_mode) const {
    if(game_mode==2) return championships[4]&1;
    for(int i=0;i<4;++i) if(tracks[size_t(16+i)]&1) return true;
    return false;
}
bool Profile::championship_open(int index) const {
    return index>=0 && index<5 && (championships[size_t(index)]&1);
}
bool Profile::track_open(int environment,int variant,int game_mode) const {
    if(environment<0 || environment>4 || variant<0 || variant>3) return false;
    if(game_mode==2) return environment<=3 ? championships[size_t(variant)]&1 : championships[4]&1;
    return tracks[size_t(environment*4+variant)]&1;
}
void Profile::race_won(int environment,int variant,int rank,int vehicle,bool two_players) {
    if(two_players || rank>1 || environment<0 || environment>4 || variant<0 || variant>3) return;
    if(variant<3) tracks[size_t(environment*4+variant+1)]|=1;
    if(vehicle>=0 && vehicle<8) wins[size_t(environment*4+variant)]|=uint8_t(1u<<vehicle);
}
void Profile::championship_won(int championship,int rank,int vehicle,const std::array<int,7>& points) {
    if(rank>3 || vehicle<0 || vehicle>6) return;
    // 0x496d90: the first vehicle with the most points, or the player's
    // vehicle when it has as many.
    const int best=*std::max_element(points.begin(),points.end());
    if(points[size_t(vehicle)]!=best) return;
    if(championship<4) {
        if(championship>=0) {
            championships[size_t(championship+1)]|=1;
            for(int e=0;e<4;++e) tracks[size_t(e*4+championship)]|=1;
        }
        if(championship==3) satanas|=1;
    } else for(int i=0;i<4;++i) tracks[size_t(16+i)]|=1;
}
bool Profile::all_races_won() const {
    for(size_t i=0;i<16;++i) if(!wins[i]) return false;
    return true;
}
MenuPrefs MenuPrefs::defaults() {
    MenuPrefs p;
    p.set_word(0,1);p.set_word(4,1);
    p.set_real(8,1);p.set_real(0xc,1);p.set_real(0x10,0.75f);
    p.set_word(0x14,1);p.set_word(0x18,0xffffffffu);p.set_word(0x1c,0xffffffffu);p.set_word(0x20,50);
    // 0x4966f0: the action bits of both players' pairs, then the keyboard
    // keys (player 1: arrows, Return, '.', '/', right Shift; player 2: G, B,
    // V, N, A, left Shift, Z, X).
    static const uint32_t bits[12]={0x40000,0x80000,0x10000,0x20000,0x100000,0x1000000,0x800000,0x200000,0x1000,0x2000,0x40000,0x80000};
    static const uint32_t keys[2][8]={{0xc8,0xd0,0xcb,0xcd,0x1c,0x34,0x35,0x36},{0x22,0x30,0x2f,0x31,0x1e,0x2a,0x2c,0x2d}};
    for(size_t player=0;player<2;++player) for(size_t i=0;i<12;++i) {
        const size_t at=0x24+player*0x60+i*8;
        p.set_word(at+4,bits[i]);
        p.set_word(at,i<8?keys[player][i]:0);
    }
    p.set_word(0xe4,2);p.set_word(0xe8,0);
    return p;
}
uint32_t MenuPrefs::word(size_t o) const {
    if(o+4>block.size()) throw std::runtime_error("prefs: offset out of the block");
    return uint32_t(block[o])|uint32_t(block[o+1])<<8|uint32_t(block[o+2])<<16|uint32_t(block[o+3])<<24;
}
void MenuPrefs::set_word(size_t o,uint32_t v) {
    if(o+4>block.size()) throw std::runtime_error("prefs: offset out of the block");
    for(int i=0;i<4;++i) block[o+size_t(i)]=uint8_t(v>>(8*i));
}
float MenuPrefs::real(size_t o) const {const uint32_t w=word(o);float f;std::memcpy(&f,&w,4);return f;}
void MenuPrefs::set_real(size_t o,float v) {uint32_t w;std::memcpy(&w,&v,4);set_word(o,w);}
std::array<std::pair<uint32_t,uint32_t>,12> MenuPrefs::controls(int player) const {
    std::array<std::pair<uint32_t,uint32_t>,12> out{};
    for(size_t i=0;i<12;++i) {const size_t at=0x24+size_t(player)*0x60+i*8;out[i]={word(at),word(at+4)};}
    return out;
}
std::vector<uint8_t> MenuPrefs::record() const {
    std::vector<uint8_t> out(0x180,0);
    out[0]=0x78;out[1]=0x01;
    std::copy(block.begin(),block.end(),out.begin()+8);
    return out;
}
MenuPrefs MenuPrefs::parse(const std::vector<uint8_t>& r) {
    if(r.size()!=0x180 || r[0]!=0x78 || r[1]!=0x01 || r[2] || r[3]) throw std::runtime_error("prefs: not a MenuPrefs record");
    MenuPrefs p;std::copy(r.begin()+8,r.begin()+8+0xec,p.block.begin());
    return p;
}
namespace {
struct Extended {uint32_t dik;int linux_code;const char* name;};
constexpr Extended extended[]={
    {0x9c,96,"PAVE ENTREE"},{0x9d,97,"CTRL DROIT"},{0xb5,98,"PAVE /"},{0xb7,99,"IMPR"},{0xb8,100,"ALT GR"},
    {0xc7,102,"ORIGINE"},{0xc8,103,"HAUT"},{0xc9,104,"PAGE HAUT"},{0xcb,105,"GAUCHE"},{0xcd,106,"DROITE"},
    {0xcf,107,"FIN"},{0xd0,108,"BAS"},{0xd1,109,"PAGE BAS"},{0xd2,110,"INSER"},{0xd3,111,"SUPPR"}};
const char* const base_names[0x59]={
    nullptr,"ECHAP","1","2","3","4","5","6","7","8","9","0","-","=","RETOUR ARRIERE","TAB",
    "Q","W","E","R","T","Y","U","I","O","P","[","]","ENTREE","CTRL GAUCHE","A","S",
    "D","F","G","H","J","K","L",";","'","`","MAJ GAUCHE","\\","Z","X","C","V",
    "B","N","M",",",".","/","MAJ DROITE","PAVE *","ALT","ESPACE","VERR MAJ","F1","F2","F3","F4","F5",
    "F6","F7","F8","F9","F10","VERR NUM","ARRET DEFIL","PAVE 7","PAVE 8","PAVE 9","PAVE -","PAVE 4","PAVE 5","PAVE 6","PAVE +","PAVE 1",
    "PAVE 2","PAVE 3","PAVE 0","PAVE .",nullptr,nullptr,nullptr,"F11","F12"};
}
int dik_to_linux(uint32_t dik) {
    if(dik>0 && dik<0x59) return int(dik);
    for(const auto& e:extended) if(e.dik==dik) return e.linux_code;
    return 0;
}
uint32_t linux_to_dik(int code) {
    if(code>0 && code<0x59) return uint32_t(code);
    for(const auto& e:extended) if(e.linux_code==code) return e.dik;
    return 0;
}
std::string dik_name(uint32_t dik) {
    if(dik<0x59 && base_names[dik]) return base_names[dik];
    for(const auto& e:extended) if(e.dik==dik) return e.name;
    return dik ? "?" : "";
}
void KeyNames::load(const ExeImage& exe) {
    codes_.clear();flags_.clear();english_.clear();french_.clear();
    for(uint32_t i=0;i<106;++i) {
        uint32_t code=0,flag=0,en=0,fr=0;
        if(!exe.u32_at(0x526a4c+4*i,code) || !exe.u32_at(0x526f44+4*i,flag) ||
           !exe.u32_at(0x526bf4+4*i,en) || !exe.u32_at(0x526d9c+4*i,fr) || code>0xff) {codes_.clear();return;}
        codes_.push_back(code);flags_.push_back(flag);
        english_.push_back(exe.string_at(en));french_.push_back(exe.string_at(fr));
    }
    pause_=exe.string_at(0x527d14);
}
std::string KeyNames::name(uint32_t dik,uint32_t language) const {
    if(empty()) return dik_name(dik);
    size_t i=0;while(i<codes_.size() && codes_[i]!=dik) ++i;
    if(i==codes_.size()) return dik_name(dik);
    if(language==3 && (flags_[i]&2)) return french_[i];
    if(dik==0x45) return pause_;
    // GetKeyNameTextA(dik << 16) knows the non-extended codes (the native
    // French names stand for a French Windows'); the extended ones (arrows:
    // font glyphs 0x7f, '~', '}', '|') fall back on the tables.
    if(dik<0x80) return language==3?dik_name(dik):english_[i];
    return language==3?french_[i]:english_[i];
}
bool set_control(MenuPrefs& p,int player,int item,uint32_t key) {
    if(player<0 || player>1 || item<0 || item>11) return false;
    const auto at=[&](int pl,int i){return size_t(0x24+pl*0x60+i*8);};
    if(item<10) {
        if(p.word(0x18)==0xffffffffu && p.word(0x1c)==0xffffffffu)
            for(int i=0;i<10;++i) if(p.word(at(player^1,i))==key) return false;
        const uint32_t old=p.word(at(player,item));
        for(int i=0;i<10;++i) if(p.word(at(player,i))==key) p.set_word(at(player,i),old);
    }
    p.set_word(at(player,item),key);
    return true;
}
std::string config_directory() {
    namespace fs=std::filesystem;
    if(const char* x=std::getenv("XDG_CONFIG_HOME");x && *x) return (fs::path(x)/"wackyraces").string();
    if(const char* h=std::getenv("HOME");h && *h) return (fs::path(h)/".config"/"wackyraces").string();
    throw std::runtime_error("prefs: no HOME for the config directory");
}
MenuPrefs load_prefs(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) return MenuPrefs::defaults();
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    try {return MenuPrefs::parse(bytes);} catch(const std::runtime_error&) {return MenuPrefs::defaults();}
}
void save_prefs(const std::string& path,const MenuPrefs& prefs) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    const std::string temporary=path+".tmp";
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out) throw std::runtime_error("prefs: cannot write "+temporary);
        const auto bytes=prefs.record();
        out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
        if(!out) throw std::runtime_error("prefs: write failed for "+temporary);
    }
    std::filesystem::rename(temporary,path);
}
void Profile::unlock_all() {
    for(auto& t:tracks) t|=1;
    for(auto& c:championships) c|=1;
    satanas|=1;
    wins.fill(0x7f);
}
std::vector<CreditLine> parse_credits(const std::vector<uint8_t>& b) {
    if(b.size()<4) throw std::runtime_error("credits: truncated count");
    const uint32_t count=uint32_t(b[0])|uint32_t(b[1])<<8|uint32_t(b[2])<<16|uint32_t(b[3])<<24;
    if(count>10000 || b.size()!=4+size_t(count)*50) throw std::runtime_error("credits: size is not 4 + count x 50");
    std::vector<CreditLine> lines;
    for(size_t i=0;i<count;++i) {
        const uint8_t* r=b.data()+4+i*50;
        if(r[0]>2) throw std::runtime_error("credits: style above 2");
        CreditLine line;line.style=r[0];
        for(size_t k=1;k<50 && r[k];++k) line.text+=char(r[k]);
        lines.push_back(std::move(line));
    }
    return lines;
}
std::string save_directory() {
    namespace fs=std::filesystem;
    fs::path base;
    if(const char* x=std::getenv("XDG_DATA_HOME");x && *x) base=x;
    else if(const char* h=std::getenv("HOME");h && *h) base=fs::path(h)/".local/share";
    else throw std::runtime_error("profile: no HOME for the save directory");
    return (base/"wackyraces"/"save").string();
}
std::vector<std::string> list_saves(const std::string& directory,const std::string& extension) {
    namespace fs=std::filesystem;
    fs::create_directories(directory);
    std::vector<std::string> names;
    for(const auto& e:fs::directory_iterator(directory)) {
        if(!e.is_regular_file()) continue;
        const std::string name=e.path().filename().string();
        if(name.size()<=extension.size() || name.compare(name.size()-extension.size(),extension.size(),extension)!=0) continue;
        names.push_back(name.substr(0,name.size()-extension.size()));
        if(names.size()==8) break; // 0x4b7690 keeps eight
    }
    std::sort(names.begin(),names.end());
    return names;
}
Profile load_profile(const std::string& path) {
    std::ifstream in(path,std::ios::binary);
    if(!in) throw std::runtime_error("profile: cannot open "+path);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    return Profile::parse(bytes);
}
void save_profile(const std::string& path,const Profile& profile) {
    // Written beside and renamed, so a failed write keeps the old file (the
    // original deletes first, 0x495df0).
    const std::string temporary=path+".tmp";
    {
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
        if(!out) throw std::runtime_error("profile: cannot write "+temporary);
        const auto bytes=profile.serialize();
        out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
        if(!out) throw std::runtime_error("profile: write failed for "+temporary);
    }
    std::filesystem::rename(temporary,path);
}
}
