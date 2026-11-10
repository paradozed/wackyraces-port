#include "menu_controller.h"
#include "game_files.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wpc {
namespace {
bool navigation(const Node &node) { return node.class_name.rfind("WMenuNode",0)==0; }
MenuEvent event(MenuEvent::Kind kind,const Node &node,const std::string &detail={}) { return {kind,node.id,detail}; }
const Node *start_node(const Node &node,int code) {
    if (!navigation(node)) return nullptr;
    if (node.integer(0x19d)==code) return &node;
    const Node *result=nullptr;
    for (const Node &child:node.children) if (const Node *found=start_node(child,code)) result=found;
    return result;
}
}
bool menu_item_class(const std::string &name) {
    return name=="WMenuItem" || name=="WMenuSetValueItem" || name=="WMenuSetVolumeItem" ||
           name=="WMenuCarItem" || name=="WMenuControllerItem";
}
void MenuController::index(const Node &node,const Node *parent) {
    if (!nodes_.emplace(node.id,&node).second) throw std::runtime_error("Duplicate menu scene ID");
    parents_[node.id]=parent;
    for (const auto &action:node.actions) if (action.class_name=="WMenu") {
        if (root_) throw std::runtime_error("Multiple WMenu managers in scene");
        root_=&node; manager_=&action;
    }
    for (const auto &child:node.children) index(child,&node);
}
MenuController::MenuController(const Node &scene_root) {
    index(scene_root,nullptr);
    if (!root_ || !navigation(*root_)) throw std::runtime_error("No WMenu navigation root");
    current_=root_;
    if (!page()) throw std::runtime_error("WMenu root has no visual page");
    clamp_selection();
}
const Node *MenuController::lookup(uint32_t id) const {
    auto found=nodes_.find(id); return found==nodes_.end()?nullptr:found->second;
}
const Node *MenuController::page() const {
    uint32_t id=uint32_t(current_->integer(0x1a0));
    const Node *result=lookup(id);
    return result && result->class_name=="WMenuPage" ? result : nullptr;
}
const Node *MenuController::shown_page() const {
    if (current_->class_name=="WMenuNodePause" && session_.player_mode==0) {
        const auto next=children(*current_);
        if (!next.empty())
            if (const Node *p=lookup(uint32_t(next.front()->integer(0x1a0))); p && p->class_name=="WMenuPage") return p;
    }
    return page();
}
std::vector<const Node *> MenuController::children(const Node &node) const {
    std::vector<const Node *> result;
    for (const Node &child:node.children) if (navigation(child)) result.push_back(&child);
    return result;
}
std::vector<const Node *> MenuController::items() const {
    std::vector<const Node *> result;
    if (const Node *visual=page())
        for (const Node &child:visual->children) if (menu_item_class(child.class_name)) result.push_back(&child);
    return result;
}
size_t MenuController::selected() const {
    auto found=cursors_.find(current_->id); return found==cursors_.end()?0:found->second;
}
int MenuController::availability(size_t index) const {
    // WMenuNodeCarSelection slot 18 (0x48dde0): row 0, Satanas et Diabolo,
    // requires the profile unlock; every other car row is available.
    const Profile &p=session_.profile;
    if (current_->class_name=="WMenuNodeCarSelection") return index!=0 || (p.satanas&1);
    // 0x48e2d0: the row both cursors hold is closed; row 0 needs Satanas.
    if (current_->class_name=="WMenuNodeTwoPlayer") {
        if (index==two_.cursor[0] && index==two_.cursor[1]) return 0;
        return index!=0 || (p.satanas&1);
    }
    // The page-entry callbacks (slot +0x3c) write the rows' state through
    // slot +0x44 (0x48c030): 0x490d60 row 4 (dr) of WMenuNodeSelectScenario,
    // 0x4910a0 the five championships, 0x4913f0 the four variants (in a
    // championship, app +0x1a60 = 3, only the current race, 0x6f41dc).
    if (current_->class_name=="WMenuNodeSelectScenario" && index==4) return p.scenario_open(session_.game_mode);
    if (current_->class_name=="WMenuNodeSelectCShip" && index<5) return p.championship_open(int(index));
    if (current_->class_name=="WMenuNodeSelectTrack" && index<4)
        return session_.game_mode==3 ? session_.championship>=0 &&
                   int(index)==championship_course(session_.championship,session_.championship_race).course
               : p.track_open(session_.environment,int(index),session_.game_mode);
    // RETOUR DE FORCE (WMenuSetValueItem type 2): unavailable without a
    // force-feedback device; Wine skips the row (2026-11-01). The native port
    // has no such device. The routine that clears it is not traced.
    const auto choices=items();
    if (index<choices.size() && choices[index]->class_name=="WMenuSetValueItem" && choices[index]->integer(0x194)==2) return 0;
    auto descendants=children(*current_);
    return index<descendants.size()?descendants[index]->integer(0x19c,1):1;
}
bool MenuController::horizontal() const { return current_->integer(0x1a1)!=0; }
void MenuController::clamp_selection() {
    auto choices=items();
    size_t &cursor=cursors_[current_->id];
    if (choices.empty()) { cursor=0; return; }
    if (cursor>=choices.size()) cursor=0;
    // The original assumes at least one available entry. Bound the scan for
    // malformed/incomplete data so it cannot hang the native event loop.
    for (size_t i=0;i<choices.size() && availability(cursor)==0;++i) cursor=(cursor+1)%choices.size();
}
void MenuController::list_files() {
    if (save_directory_.empty()) save_directory_=save_directory();
    files_.names=list_saves(save_directory_,files_.extension());
}
MenuEvent MenuController::type(const std::string& text) {
    if (!file_screen() || files_.state!=3) return {};
    for (const char c:text) {
        if (c=='\b') {if (!files_.edit.empty()) files_.edit.pop_back();}
        // Upper case: the menu font has capitals only (provisional; the
        // original's key-to-character table 0x426a50 is not traced).
        else if (files_.edit.size()<31 && static_cast<unsigned char>(c)>=0x20 && c!='/' && c!='\\')
            files_.edit+=char(std::toupper(static_cast<unsigned char>(c)));
    }
    return event(MenuEvent::ChangedSelection,*current_);
}
MenuEvent MenuController::file_accept() {
    FileScreen &f=files_;
    const bool occupied=f.cursor<int(f.names.size());
    const auto leave=[&]() {f.node=0;return event(MenuEvent::ChangedPage,*current_);};
    switch (f.state) {
        case 4: f.state=3; return event(MenuEvent::ChangedSelection,*current_);
        case 2: return leave();
        case 1:
            if (f.answer!=0) return leave();
            f.replace=true;f.edit=f.names[size_t(f.cursor)];f.state=3; // 0x4961a3
            return event(MenuEvent::ChangedSelection,*current_);
        case 3: {
            // 0x495ec0: the name must differ from the other listed ones (an
            // empty name is refused too, natively).
            for (size_t i=0;i<f.names.size();++i) if (int(i)!=f.cursor && f.names[i]==f.edit) {f.state=4;return event(MenuEvent::ChangedSelection,*current_);}
            if (f.edit.empty()) {f.state=4;return event(MenuEvent::ChangedSelection,*current_);}
            if (f.mode==3 && !last_replay_) return event(MenuEvent::Pending,*current_,"No race recorded to save");
            namespace fs=std::filesystem;
            if (f.replace && occupied && f.names[size_t(f.cursor)]!=f.edit)
                fs::remove(fs::path(save_directory_)/(f.names[size_t(f.cursor)]+f.extension()));
            const std::string target=(fs::path(save_directory_)/(f.edit+f.extension())).string();
            if (f.mode==3) {
                // 0x4b7920: the whole buffer, raw.
                const auto bytes=last_replay_->serialize();
                std::ofstream out(target,std::ios::binary|std::ios::trunc);
                out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
                if (!out) throw std::runtime_error("cannot write "+target);
            } else save_profile(target,session_.profile);
            list_files();f.state=2;
            return event(MenuEvent::ChangedSetting,*current_,"saved "+f.edit);
        }
        default:
            if (f.save() && occupied) {f.state=1;f.answer=1;return event(MenuEvent::ChangedSelection,*current_);}
            if (f.save()) {f.replace=false;f.edit.clear();f.state=3;return event(MenuEvent::ChangedSelection,*current_);}
            if (!occupied) return {};
            if (f.mode==2) {
                // 0x4962e6: 0x4b7a30 reads the buffer, 0x45aff0 restores it
                // and the race starts.
                std::ifstream in((std::filesystem::path(save_directory_)/(f.names[size_t(f.cursor)]+f.extension())).string(),std::ios::binary);
                last_replay_=Replay::parse(std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()));
                f.node=0;
                return event(MenuEvent::PlayReplay,*current_,f.names[size_t(f.cursor)]);
            }
            // 0x4b7e30: the five blocks replace the profile.
            session_.profile=load_profile((std::filesystem::path(save_directory_)/(f.names[size_t(f.cursor)]+f.extension())).string());
            f.state=2;
            return event(MenuEvent::ChangedSetting,*current_,"loaded "+f.names[size_t(f.cursor)]);
    }
}
MenuEvent MenuController::move(int direction) {
    if (credits_) return {};
    if (file_screen()) {
        // 0x495f56: input bits 0x1000/0x4000 step the row (wrapping 0..7),
        // or the OUI/NON answer in the overwrite question.
        if (files_.state==0) files_.cursor=(files_.cursor+(direction<0?7:1))%8;
        else if (files_.state==1) files_.answer^=1;
        else return {};
        return event(MenuEvent::ChangedSelection,*current_);
    }
    auto choices=items();
    if (choices.empty() || direction==0) return {};
    size_t original=selected(), candidate=original;
    for (size_t i=0;i<choices.size();++i) {
        candidate=direction<0 ? (candidate+choices.size()-1)%choices.size() : (candidate+1)%choices.size();
        if (availability(candidate)!=0) {
            cursors_[current_->id]=candidate;
            return candidate!=original ? event(MenuEvent::ChangedSelection,*current_) : MenuEvent{};
        }
    }
    return {};
}
MenuEvent MenuController::bind_key(uint32_t dik) {
    if (binding_<0) return {};
    const int item=binding_;binding_=-1;
    if (!dik) return event(MenuEvent::ChangedSetting,*current_,"binding cancelled");
    const int player=current_->integer(0x1a2);
    // A joystick's table holds buttons; rebinding them is not ported.
    if (session_.prefs.device(player)>=0) return event(MenuEvent::ChangedSetting,*current_,"joystick buttons are not rebound");
    const bool set=set_control(session_.prefs,player,item,dik);
    return event(MenuEvent::ChangedSetting,*current_,set?"key "+dik_name(dik):"key held by the other player");
}
std::string MenuController::control_name(const Node &item) const {
    const int action=item.integer(0x196);
    if (binding_>=0 && binding_==action) return "?";
    const int player=current_->integer(0x1a2);
    if (action<0 || action>11 || player<0 || player>1) return "";
    // A joystick's entries are button numbers (100: none, the stick).
    if (session_.prefs.device(player)>=0) {
        const uint32_t b=session_.prefs.controls(player)[size_t(action)].first;
        return b==100?std::string("-"):"B"+std::to_string(b+1);
    }
    return key_names_.name(session_.prefs.controls(player)[size_t(action)].first,language_);
}
bool MenuController::idle_update(bool input) {
    const int code=current_->integer(0x19d);
    if (input || file_screen() || credits_ || (code!=0 && code!=0x10)) {idle_=0;return false;}
    if (++idle_<=manager_->integer(0x1a4,300)) return false;
    idle_=0;
    return true;
}
MenuEvent MenuController::credits_update(const std::string& typed) {
    if (!credits_) return {};
    const int first=-(scroll_/16);
    --scroll_;
    if (first>int(credit_lines_.size())) scroll_=256;
    static const char sequence[]="WACKY RULEZ";
    for (const char c:typed) {
        if (char(std::toupper(static_cast<unsigned char>(c)))==sequence[cheat_]) ++cheat_;
        else cheat_=0;
        if (cheat_==11) {
            cheat_=0;session_.profile.unlock_all(); // 0x405191 -> 0x496e10
            return event(MenuEvent::ChangedSetting,*current_,"WACKY RULEZ: everything unlocked");
        }
    }
    return {};
}
MenuEvent MenuController::back() {
    if (credits_) {credits_=false;return event(MenuEvent::ChangedPage,*current_);}
    // 0x4960ab: Back leaves the file screen from every state.
    if (file_screen()) {files_.node=0;return event(MenuEvent::ChangedPage,*current_);}
    // YesNo's override consumes Back; its NO item performs the parent return.
    if (current_->class_name=="WMenuNodeYesNo" || current_->integer(0x19e)) return {};
    const Node *parent=parents_.at(current_->id);
    if (parent && navigation(*parent)) return enter(*parent);
    return {};
}
MenuEvent MenuController::selected_action(const Node &item) {
    if (item.class_name=="WMenuControllerItem") {
        binding_=item.integer(0x196);
        if (binding_<0 || binding_>11) throw std::runtime_error("Invalid controller item action");
        return event(MenuEvent::ChangedSetting,item,"waiting for a key");
    }
    if (item.class_name=="WMenuCarItem") {
        int player=current_->integer(0x1a2), vehicle=item.integer(0x18c);
        if (player<0 || player>1 || vehicle<0 || vehicle>6) throw std::runtime_error("Invalid vehicle/player selection");
        session_.vehicles[size_t(player)]=vehicle;
    }
    if (item.class_name!="WMenuSetValueItem") return {};
    const int type=item.integer(0x194), value=item.integer(0x195);
    // 0x48a3d0. Other option callbacks need their original subsystem state.
    switch (type) {
        case 0:
            if (value<0 || value>3) throw std::runtime_error("Invalid game mode");
            session_.game_mode=value; break;
        case 3:
            if (value<4 || value>5) throw std::runtime_error("Invalid player mode");
            session_.player_mode=value-4; break;
        case 7: return event(MenuEvent::Quit,*current_);
        case 9: return event(MenuEvent::PlayIntro,*current_);
        case 10: return event(MenuEvent::PlayDemo,*current_);
        case 5: break; // Controller configuration has a corresponding child.
        // 0x48a660 (the item's accept callback): METEO cycles 0..2, POUSSIERE
        // toggles; each then refreshes the value text.
        case 8: session_.weather=(session_.weather+1)%3; return event(MenuEvent::ChangedSetting,item);
        case 11: session_.dust=!session_.dust; return event(MenuEvent::ChangedSetting,item);
        // 0x48a6ef: RETOUR DE FORCE toggles (both players' copies in a
        // one-player game).
        case 2: session_.force_feedback=!session_.force_feedback; return event(MenuEvent::ChangedSetting,item);
        // 0x48a68c: MONO/STEREO flips 0x6f457c (MenuPrefs +0x14), passed to
        // the sound manager's 0x426c70, which ignores it. No shipped menu
        // has a type-1 item.
        case 1: session_.prefs.set_word(0x14,session_.prefs.word(0x14)^1); return event(MenuEvent::ChangedSetting,item);
        default: return event(MenuEvent::Pending,*current_,"Menu setting callback type "+std::to_string(type));
    }
    return {};
}
std::string MenuController::device_label(const Node &item) const {
    if (item.class_name!="WMenuSetValueItem" || item.integer(0x194)!=5) return "";
    const int player=item.integer(0x195);
    if (player<0 || player>1) return "";
    const int d=session_.prefs.device(player);
    if (d<0) return "";
    return std::to_string(d+1)+":"+(size_t(d)<joysticks_.size()?joysticks_[size_t(d)]:std::string("?"));
}
uint32_t MenuController::value_text(const Node &item) const {
    if (item.class_name!="WMenuSetValueItem") return 0;
    // 0x48a930: type 5 with the keyboard shows string 0x97 (CLAVIER).
    if (item.integer(0x194)==5) return session_.prefs.device(item.integer(0x195)&1)<0 ? 0x97 : 0;
    // 0x48a7a2: strings 0x0a, 0x0b, 0xfd; 0x48a80e: 0x9a INACTIVE, 0x9b ACTIVE.
    switch (item.integer(0x194)) {
        case 8: return session_.weather==0 ? 0x0a : session_.weather==1 ? 0x0b : 0xfd;
        case 11: return session_.dust ? 0x9b : 0x9a;
        case 2: return session_.force_feedback ? 0x9b : 0x9a; // 0x48a75c: 0x9a + value
        case 1: return 0xc+(session_.prefs.word(0x14)&1);     // 0x48a6b8: MONO, STEREO
        default: return 0;
    }
}
MenuEvent MenuController::enter(const Node &target) {
    if (target.class_name=="WMenuNodeLoadSave") {
        // 0x495df0: the slots are listed (0x4b78e0) and the screen opens on
        // its list (+0x13a = 0); the cursor keeps its last row.
        const int cursor=files_.cursor;
        files_=FileScreen{};files_.mode=target.integer(0x1bd);files_.node=target.id;files_.cursor=cursor;
        if (files_.mode<0 || files_.mode>3) throw std::runtime_error("Invalid WMenuNodeLoadSave mode");
        list_files();
        return event(MenuEvent::LoadSave,target);
    }
    if (target.class_name=="WMenuNodeLoadTrack") return event(MenuEvent::LoadTrack,target);
    // general.wgm's WMenuBrick (the in-race pause): resume, and the YES of
    // QUITTER's confirmation loading the menu file.
    if (target.class_name=="WMenuNodeResumeGame") return event(MenuEvent::ResumeGame,target);
    if (target.class_name=="WMenuNodeLoadFile") return event(MenuEvent::LoadFile,target);
    if (target.class_name=="WMenuNode" && target.integer(0x19d)==44) {credits_=true;scroll_=256;cheat_=0;return event(MenuEvent::ShowCredits,target);}
    if (target.class_name=="WMenuNodeStartMenu") {
        int code=target.integer(0x1b0);
        const Node *match=start_node(*root_,code);
        if (!match) return event(MenuEvent::Pending,target,"Missing start-code destination");
        // A chain of start-code callbacks needs its own lifecycle execution;
        // reject it here instead of recursing indefinitely through bad data.
        if (match->class_name=="WMenuNodeStartMenu") return event(MenuEvent::Pending,target,"Chained start-code callback pending");
        // Only the nodes with a visual page can be entered by this native slice.
        return enter(*match);
    }
    const Node *visual=lookup(uint32_t(target.integer(0x1a0)));
    if (!visual || visual->class_name!="WMenuPage") return event(MenuEvent::Pending,target,"Menu-node lifecycle callback pending: "+target.class_name);
    current_=&target;
    if (target.class_name=="WMenuNodeYesNo") cursors_[target.id]=1; // 0x4928b0 defaults to NO.
    if (target.class_name=="WMenuNodeTwoPlayer") {
        // 0x48e1c0: both cursors placed (0x40605f; provisional: player 1 on
        // the first open row, player 2 on the next), neither ready.
        two_=TwoPlayer{};
        const size_t n=items().size();
        size_t first=0;while (first<n && !two_open(first,0)) ++first;
        two_.cursor[0]=first<n?first:0;two_.cursor[1]=n?(two_.cursor[0]+1)%n:0;
        while (n && !two_open(two_.cursor[1],1) && two_.cursor[1]!=two_.cursor[0]) two_.cursor[1]=(two_.cursor[1]+1)%n;
        cursors_[target.id]=two_.cursor[0];
        return event(MenuEvent::ChangedPage,target);
    }
    clamp_selection();
    return event(MenuEvent::ChangedPage,target);
}
bool MenuController::two_open(size_t row,int player) const {
    if (row==two_.cursor[size_t(player^1)]) return false;
    return row!=0 || (session_.profile.satanas&1);
}
MenuEvent MenuController::two_move(int player,int direction) {
    if (!two_player_page() || !direction) return {};
    const size_t p=size_t(player&1),n=items().size();
    if (two_.ready[p] || !n) return {};
    size_t c=two_.cursor[p];
    for (size_t step=0;step<n;++step) {
        c=(c+n+size_t(direction>0?1:n-1))%n;
        if (two_open(c,int(p))) {two_.cursor[p]=c;break;}
    }
    if (p==0) cursors_[current_->id]=two_.cursor[0];
    return event(MenuEvent::ChangedSelection,*current_);
}
MenuEvent MenuController::two_ready(int player) {
    if (!two_player_page()) return {};
    two_.ready[size_t(player&1)]=true;
    if (!two_.ready[0] || !two_.ready[1]) return event(MenuEvent::ChangedSetting,*current_,"player "+std::to_string((player&1)+1)+" ready");
    // 0x48e485: each player's car item takes its selection (game +0x1a68,
    // +0x1a6c), then +0x54 goes on to the node's child.
    const auto choices=items();
    for (size_t p=0;p<2;++p) if (two_.cursor[p]<choices.size()) session_.vehicles[p]=choices[two_.cursor[p]]->integer(0x18c);
    return open_first_child();
}
MenuEvent MenuController::two_back(int player) {
    if (!two_player_page()) return {};
    const size_t p=size_t(player&1);
    if (two_.ready[p]) {two_.ready[p]=false;return event(MenuEvent::ChangedSetting,*current_,"player "+std::to_string(p+1)+" not ready");}
    return p==0 ? back() : MenuEvent{};
}
uint32_t MenuController::text_override(const Node &text) const {
    if (current_->class_name=="WMenuNodePause" && session_.player_mode==1) {
        const Node *visual=page();
        return visual && !visual->children.empty() && &visual->children[0]==&text ? 198u+uint32_t(pause_player_) : 0;
    }
    if (!championship_race_ || current_->integer(0x19d)!=0x21) return 0;
    const Node *visual=page();
    if (!visual || visual->children.size()<6 || visual->children[5].children.empty()) return 0;
    return &visual->children[5].children[0]==&text ? 200 : 0;
}
MenuEvent MenuController::open_first_child() {
    auto descendants=children(*current_);
    return descendants.empty()?MenuEvent{}:enter(*descendants.front());
}
MenuEvent MenuController::accept() {
    if (credits_) return {};
    if (file_screen()) return file_accept();
    auto choices=items();
    if (choices.empty() || selected()>=choices.size() || availability(selected())==0) return {};
    if (auto action=selected_action(*choices[selected()]);action.kind!=MenuEvent::None) return action;
    if (current_->class_name=="WMenuNodePostRace")
        if (auto action=post_race_accept();action.kind!=MenuEvent::None) return action;
    if (current_->class_name=="WMenuNodeYesNo") {
        if (selected()==1) {
            const Node *parent=parents_.at(current_->id);
            return parent && navigation(*parent) ? enter(*parent) : MenuEvent{};
        }
        // The configured YES operation may search a start code. Quit itself
        // was already emitted by its selected WMenuSetValueItem callback.
        if (current_->integer(0x1b2)==0) {
            int code=current_->integer(0x1b1);
            if (const Node *match=start_node(*root_,code);match && match!=current_) return enter(*match);
        }
    }
    auto descendants=children(*current_);
    size_t index=current_->integer(0x19b) ? 0 : selected();
    const Node *parent=parents_.at(current_->id);
    const bool championship_cars=current_->class_name=="WMenuNodeCarSelection" && parent &&
                                 parent->class_name=="WMenuNodeSelectCShip";
    // Under WMenuNodeSelectCShip the car page has one WMenuNodeSelectTrack
    // per environment (pages 518 cc .. 816 dr): a championship starts on its
    // first race's (cc, or dr for championship 4).
    if (championship_cars) index=size_t(championship_course(std::max(0,session_.championship),0).environment);
    // In a championship an environment's track page (four variants) leaves
    // only the championship's variant available (0x4913f0).
    const bool championship_track=current_->class_name=="WMenuNodeSelectTrack" && session_.game_mode==3 && session_.championship>=0;
    if (index>=descendants.size()) return {};
    const Node &target=*descendants[index];
    if (target.integer(0x19c,1)==2) return event(MenuEvent::Pending,target,"Menu entry disabled by game state");
    if (current_->class_name=="WMenuNodeSelectScenario") session_.environment=int(selected());
    if (current_->class_name=="WMenuNodeSelectCShip") {
        session_.championship=int(selected()); session_.championship_race=0; session_.points={};
    }
    if (current_->class_name=="WMenuNodeSelectTrack") {
        if (championship_track) {
            // Wine: Championship I opens with PISTE SUIVANTE / LE GRAND CANYON 1,
            // which loads cc/cc_t1.wgm.
            const CourseChoice choice=championship_course(session_.championship,session_.championship_race);
            session_.environment=choice.environment; session_.course=choice.course;
        } else session_.course=int(selected());
    }
    return enter(target);
}
MenuEvent MenuController::enter_code(int code) {
    const Node *match=start_node(*root_,code);
    if (!match) return event(MenuEvent::Pending,*current_,"Missing start-code destination "+std::to_string(code));
    return enter(*match);
}
MenuEvent MenuController::race_over(int rank,const std::array<int,7>& race_points) {
    session_.last_rank=rank;session_.last_points=race_points;
    if (session_.game_mode==3 && session_.championship>=0) {
        for (size_t v=0;v<race_points.size();++v) session_.points[v]+=race_points[v]; // 0x496d10
        // 0x491d69: the last race (0x6f41d8 >= 3, or 0x6f41dc >= 3 for dr).
        if (session_.championship_race>=championship_races-1) {
            session_.profile.championship_won(session_.championship,rank,session_.vehicles[0],session_.points);
            // 0x486fe0: won (rank <= 3, the vehicle leading 0x496d90), the last
            // race loads win\win*.wgm (0x52bf54) instead of the menu; after it,
            // 0x494b20 loads win\DickOff.wgm for Championship IV (ct_t4) and
            // the menu with outro.avi for Championship V (dr_t4).
            const int best=*std::max_element(session_.points.begin(),session_.points.end());
            const int vehicle=session_.vehicles[0];
            if (rank<=3 && vehicle>=0 && vehicle<7 && session_.points[size_t(vehicle)]==best)
                return event(MenuEvent::PlayWinner,*current_,"Championship "+std::to_string(session_.championship+1)+" won");
        }
        return enter_code(3);
    }
    race_finished(rank);
    return enter_code(2);
}
bool MenuController::practice_time(int time300,int vehicle) {
    const int index=session_.environment*4+session_.course;
    if (session_.game_mode!=2 || time300<=0 || index<0 || index>=20 || vehicle<0 || vehicle>6) return false;
    uint32_t& record=session_.profile.records[size_t(index)];
    if (uint32_t(time300)>=(record&0xfffffffu)) return false;
    record=uint32_t(time300)|uint32_t(vehicle)<<28;
    return true;
}
MenuEvent MenuController::post_race_accept() {
    const int code=current_->integer(0x19d);
    const size_t item=selected();
    // REVOIR LA COURSE (0x491cfe/0x491db6): the race just run, from its buffer.
    if (item==0) return last_replay_ ? event(MenuEvent::PlayReplay,*current_) : event(MenuEvent::Pending,*current_,"No race recorded");
    if (code==2 && item==3) return event(MenuEvent::LoadTrack,*current_); // RECOMMENCER LA COURSE (0x491d14)
    if (code==2 && item==4) return enter_code(session_.game_mode+(session_.player_mode?0xe:4)); // AUTRE COURSE (0x491d1e)
    if (code==3 && item==3) {
        // 0x491e10: placed beyond third, the race's points come off
        // (0x496d50) and the same race runs again; otherwise the next
        // race's environment page (start code 8 + environment), and after
        // the last one the championship page (0xd), or code 1 after dr's.
        if (session_.last_rank>3) {
            for (size_t v=0;v<session_.points.size();++v) session_.points[v]-=session_.last_points[v];
        } else if (session_.championship_race+1<championship_races) {
            ++session_.championship_race;
        } else {
            session_.championship_race=0;
            return enter_code(session_.championship==4 ? 1 : 0xd);
        }
        const CourseChoice next=championship_course(session_.championship,session_.championship_race);
        session_.environment=next.environment;session_.course=next.course;
        return enter_code(8+next.environment);
    }
    return {};
}
bool MenuController::championship_finished(const std::array<int,7>& race_points,int rank) {
    if (session_.championship<0) return false;
    for (size_t v=0;v<race_points.size();++v) session_.points[v]+=race_points[v];
    if (++session_.championship_race<championship_races) return true;
    session_.championship_race=0;
    // 0x491d69: after the last race (0x6f41d8 >= 3; dr's championship 4
    // counts 0x6f41dc) with the place 0x6f41c4.
    if (rank>0) session_.profile.championship_won(session_.championship,rank,session_.vehicles[0],session_.points);
    return false;
}
void MenuController::race_finished(int rank) {
    if (session_.game_mode==1 || session_.player_mode!=0 || rank<1 || rank>3) return;
    session_.profile.race_won(session_.environment,session_.course,rank,session_.vehicles[0],false);
}
int MenuController::grey_level(size_t index) const {
    const std::string &c=current_->class_name;
    // The controls page draws RETOUR DE FORCE, label and value, at half
    // intensity without a force-feedback device (Wine: 115 against 239,
    // provisionally 0x80); the pause only skips the row.
    if (c=="WMenuNodeControllerConfig") {
        const auto choices=items();
        return index<choices.size() && choices[index]->class_name=="WMenuSetValueItem" && choices[index]->integer(0x194)==2 ? 0x80 : 0;
    }
    if (c!="WMenuNodeSelectCShip" && c!="WMenuNodeSelectTrack" && !(c=="WMenuNodeSelectScenario" && index==4)) return 0;
    // 0x48cec0: a row the profile closes is drawn at 0x40.
    return availability(index)==0 ? 0x40 : 0;
}
MenuEvent MenuController::adjust(int direction) {
    if (!direction || file_screen() || credits_) return {};
    auto choices=items(); if (selected()>=choices.size()) return {};
    const Node &item=*choices[selected()];
    if (item.class_name=="WMenuSetValueItem" && item.integer(0x194)==5) {
        // 0x48cab0: left (0x8000) and right (0x2000) call the item's slots +0x50
        // and +0x54 (0x48a620, 0x48a640), both the callback with 1; type 5
        // (0x48a845) steps the player's device: up by one, past the other
        // player's, and back to -1 beyond the joystick count (0x5419d4).
        const int player=item.integer(0x195),count=int(joysticks_.size());
        if (player<0 || player>1) throw std::runtime_error("Invalid controller player");
        const int before=session_.prefs.device(player),other=session_.prefs.device(player^1);
        int d=before+1;
        if (d!=-1 && d==other) ++d;
        if (d>=count || d<0) d=-1;
        if (d==before) return {};
        session_.prefs.set_device(player,d);
        session_.prefs.reset_controls(0);session_.prefs.reset_controls(1);
        return event(MenuEvent::ChangedSetting,item);
    }
    if (item.class_name!="WMenuSetVolumeItem") return {};
    float maximum=item.real(0x18e), step=item.real(0x18f), initial=volume(item);
    if (!std::isfinite(maximum) || !std::isfinite(step) || !std::isfinite(initial) || maximum<0 || step<0)
        throw std::runtime_error("Invalid menu volume range");
    auto value=session_.volumes.emplace(item.id,std::clamp(initial,0.f,maximum)).first;
    value->second=std::clamp(value->second+(direction<0?-step:step),0.f,maximum);
    const int type=item.integer(0x193,-1);
    if (type>=0 && type<=2) session_.prefs.set_level(type,value->second);
    else if (type==3) session_.prefs.set_word(0x20,uint32_t(std::lround(value->second)));
    return event(MenuEvent::ChangedSetting,item);
}
float MenuController::volume(const Node& item) const {
    const auto found=session_.volumes.find(item.id);
    if (found!=session_.volumes.end()) return found->second;
    switch (item.integer(0x193,-1)) {
        case 0: case 1: case 2: return session_.prefs.level(item.integer(0x193));
        case 3: return float(session_.prefs.word(0x20));
        default: return item.real(0x18d);
    }
}
void MenuController::set_prefs(const MenuPrefs &prefs) {
    session_.prefs=prefs;session_.volumes.clear();
    session_.weather=int(std::min<uint32_t>(prefs.word(0xe4),2));
    session_.dust=prefs.word(0xe8)!=0;
    session_.force_feedback=prefs.word(0)!=0;
}
MenuPrefs MenuController::prefs() const {
    MenuPrefs p=session_.prefs;
    p.set_word(0xe4,uint32_t(session_.weather));p.set_word(0xe8,session_.dust);
    p.set_word(0,session_.force_feedback);p.set_word(4,session_.force_feedback);
    return p;
}
}
