#include <array>
#include "menu_animation.h"
#include "menu_controller.h"
#include "game_replay.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace {
size_t checks=0;
void require(bool ok,const char *message) { ++checks; if (!ok) throw std::runtime_error(message); }
void rejects(const std::function<void()> &action,const char *message) { bool failed=false; try { action(); } catch (const std::runtime_error &) { failed=true; } require(failed,message); }
wpc::Node node(uint32_t id,const std::string &type) { wpc::Node n; n.id=id; n.class_name=type; return n; }
void integer(wpc::Node &n,uint32_t key,int value) {
    uint32_t bits=uint32_t(value); n.properties[key]={uint8_t(bits),uint8_t(bits>>8),uint8_t(bits>>16),uint8_t(bits>>24)};
}
void real(wpc::Node &n,uint32_t key,float value) { uint32_t bits; memcpy(&bits,&value,4); integer(n,key,int(bits)); }
wpc::Node fixture() {
    auto root=node(1,"GDWorld"), page=node(10,"WMenuPage");
    for (uint32_t id=11;id<=13;++id) page.children.push_back(node(id,"WMenuItem"));
    auto nav=node(20,"WMenuNode"); integer(nav,0x1a0,10); nav.actions.push_back(node(21,"WMenu"));
    for (uint32_t i=0;i<3;++i) {
        auto child=node(30+i,"WMenuNode"); integer(child,0x1a0,40+int(i)); integer(child,0x19c,i==0?0:i==1?2:1);
        nav.children.push_back(child);
        auto visual=node(40+i,"WMenuPage"); visual.children.push_back(node(50+i,"WMenuItem"));
        root.children.push_back(visual);
    }
    root.children.push_back(page); root.children.push_back(nav); return root;
}
void synthetic() {
    auto root=fixture(); wpc::MenuController menu(root);
    require(menu.node().id==20 && menu.page()->id==10,"manager root/page binding");
    require(menu.selected()==1,"unavailable first item was not skipped");
    require(menu.accept().kind==wpc::MenuEvent::Pending && menu.node().id==20,"active2 must remain selectable but not enterable");
    menu.move(1); require(menu.selected()==2,"move forward");
    menu.move(1); require(menu.selected()==1,"wrap must skip active0");
    menu.move(-1); require(menu.selected()==2,"reverse wrap must skip active0");
    require(menu.accept().kind==wpc::MenuEvent::ChangedPage && menu.node().id==32,"indexed navigation child");
    require(menu.back().kind==wpc::MenuEvent::ChangedPage && menu.selected()==2,"return must preserve parent cursor");
    require(menu.back().kind==wpc::MenuEvent::None,"root back must not exit");

    root=fixture(); auto &nav=root.children.back(); integer(nav,0x19b,1); integer(nav.children[0],0x19c,1);
    wpc::MenuController one_way(root); one_way.move(-1);
    require(one_way.selected()==2,"one-way fixture selection");
    require(one_way.accept().kind==wpc::MenuEvent::ChangedPage && one_way.node().id==30,"one-way must choose first navigation child");

    root=fixture(); integer(root.children.back().children[2],0x19e,1);
    wpc::MenuController no_back(root); no_back.move(1); no_back.accept();
    require(no_back.back().kind==wpc::MenuEvent::None && no_back.node().id==32,"no-change-up node returned to parent");

    root=fixture(); for (auto &child:root.children.back().children) integer(child,0x19c,0);
    wpc::MenuController hidden(root);
    require(hidden.move(1).kind==wpc::MenuEvent::None,"all-hidden cursor scan must terminate");
    require(hidden.accept().kind==wpc::MenuEvent::None,"all-hidden menu accepted a child");

    root=fixture(); auto &slider=root.children[3].children[1]; slider.class_name="WMenuSetVolumeItem";
    real(slider,0x18d,.5f); real(slider,0x18e,1); real(slider,0x18f,.25f);
    wpc::MenuController volume(root);
    require(volume.adjust(1).kind==wpc::MenuEvent::ChangedSetting,"volume adjustment event");
    require(volume.session().volumes.at(slider.id)==.75f,"serialized volume step");
    for (int i=0;i<10;++i) volume.adjust(1);
    require(volume.session().volumes.at(slider.id)==1,"volume upper bound");
    for (int i=0;i<10;++i) volume.adjust(-1);
    require(volume.session().volumes.at(slider.id)==0,"volume lower bound");
    // m_nSoundType 2 (VOIX) starts at the 0x496590 default, not m_rVolume.
    integer(slider,0x193,2);
    wpc::MenuController voice(root);
    require(voice.volume(slider)==.75f,"voice slider default");
    voice.adjust(1);
    require(voice.session().volumes.at(slider.id)==1 && voice.volume(slider)==1,"voice slider step from its default");
    integer(slider,0x193,0);
    require(wpc::MenuController(root).volume(slider)==1,"music slider default");
    // 0x48a845: JOUEUR's type 5 steps the device with left/right: -1, the
    // joysticks (past the other player's), -1; both tables reset (0x4966f0).
    {
        root=fixture(); auto &pad=root.children[3].children[1]; pad.class_name="WMenuSetValueItem";
        integer(pad,0x194,5); integer(pad,0x195,0);
        wpc::MenuController dev(root); dev.set_joysticks({"Pad A","Pad B"});
        require(dev.value_text(pad)==0x97 && dev.device_label(pad).empty(),"the keyboard shows CLAVIER (0x97)");
        require(dev.adjust(1).kind==wpc::MenuEvent::ChangedSetting && dev.prefs().device(0)==0,"right steps to joystick 0");
        require(dev.device_label(pad)=="1:Pad A" && dev.value_text(pad)==0,"the label is number:name");
        require(dev.prefs().controls(0)[0].first==4 && dev.prefs().controls(0)[2].first==100 && dev.prefs().controls(0)[7].first==3,"joystick buttons 4, 100, ... 3");
        require(dev.prefs().controls(1)[0].first==0x22,"player 2 keeps its keys");
        wpc::MenuPrefs other=dev.prefs(); other.set_device(1,1); dev.set_prefs(other);
        dev.adjust(-1);
        require(dev.prefs().device(0)==-1,"past the other player's joystick and the count: back to the keyboard");
        require(dev.prefs().controls(0)[0].first==0xc8,"the keyboard keys come back");
        wpc::MenuController none(root);
        require(none.adjust(1).kind==wpc::MenuEvent::None && none.prefs().device(0)==-1,"no joystick: the keyboard stays");
    }
}
void profile() {
    // 0x496590 and the .WSG blocks (0x004b7b40): 20 + 5 + 1 + 80 + 20 bytes.
    const wpc::Profile fresh=wpc::Profile::fresh();
    const auto bytes=fresh.serialize();
    require(bytes.size()==126 && bytes[0]==1 && bytes[4]==1 && bytes[16]==0 && bytes[20]==1 && bytes[21]==0 && bytes[25]==0,"fresh profile blocks");
    require(bytes[26]==0x10 && bytes[27]==0xa4 && bytes[29]==0x60 && bytes[106]==0,"records 0x52cac0 | 0x60000000, no wins");
    require(wpc::Profile::parse(bytes).serialize()==bytes,".WSG round trip");
    rejects([&]{wpc::Profile::parse(std::vector<uint8_t>(125,0));},"short .WSG rejected");
    require(!fresh.scenario_open(0) && !fresh.championship_open(1) && fresh.track_open(2,0,0) && !fresh.track_open(2,1,0),"fresh unlocks");
    wpc::Profile p=fresh;
    p.race_won(1,0,2,3,false); require(!p.track_open(1,1,0) && !p.wins[4],"second place opens nothing (0x496c50)");
    p.race_won(1,0,1,3,false); require(p.track_open(1,1,0) && p.wins[4]==8,"a win opens the next variant and marks the vehicle");
    p.race_won(1,1,1,3,true); require(!p.track_open(1,2,0),"two players: no unlock");
    std::array<int,7> points{};points[2]=30;points[3]=30;
    wpc::Profile q=fresh; q.championship_won(3,1,3,points);
    require(q.satanas==1 && q.championships[4]==1 && q.track_open(0,3,0),"Championship IV: Satanas, +0x104, the t4 courses");
    q.championship_won(4,1,3,points); require(q.scenario_open(0) && q.track_open(4,3,0),"the dr championship opens dr");
    points[2]=31; wpc::Profile r=fresh; r.championship_won(0,1,3,points); require(!r.championship_open(1),"a points leader other than the player");
    require(q.scenario_open(2) && !fresh.scenario_open(2),"game mode 2 reads +0x104");
}
void prefs() {
    // 0x496590/0x4966f0 and the MenuPrefs record (0x4604f0: 0x178, then 0x3b words).
    const wpc::MenuPrefs d=wpc::MenuPrefs::defaults();
    require(d.word(0)==1 && d.level(0)==1 && d.level(1)==1 && d.level(2)==.75f && d.word(0x20)==50 && d.word(0xe4)==2 && d.word(0xe8)==0,"MenuPrefs defaults");
    const auto p1=d.controls(0),p2=d.controls(1);
    require(p1[0].first==0xc8 && p1[0].second==0x40000 && p1[4].first==0x1c && p1[7].second==0x200000 && p2[0].first==0x22 && p2[9].second==0x2000,"keyboard defaults (DIK codes, action bits)");
    const auto record=d.record();
    require(record.size()==0x180 && record[0]==0x78 && record[1]==1 && wpc::MenuPrefs::parse(record).block==d.block,"record round trip");
    auto bad=record;bad[0]=0x77;
    rejects([&]{wpc::MenuPrefs::parse(bad);},"another size word is ignored");
    // DirectInput codes against Linux input codes (X keycode - 8).
    require(wpc::dik_to_linux(0x1e)==30 && wpc::dik_to_linux(0xc8)==103 && wpc::dik_to_linux(0x9d)==97 && wpc::linux_to_dik(108)==0xd0 && wpc::linux_to_dik(54)==0x36,"DIK <-> Linux codes");
    for (uint32_t k=1;k<0x100;++k) if (const int c=wpc::dik_to_linux(k)) require(wpc::linux_to_dik(c)==k,"DIK mapping round trip");
    // 0x4969a0: the old key moves to the item that had the new one; with both
    // players on keyboards, player 2's keys are refused.
    auto c=d;
    require(wpc::set_control(c,0,0,0xd0) && c.controls(0)[0].first==0xd0 && c.controls(0)[1].first==0xc8,"set_control swaps within a player");
    require(!wpc::set_control(c,0,2,0x22) && c.controls(0)[2].first==0xcb,"a key player 2 holds is refused");
    require(wpc::set_control(c,1,0,0x10) && c.controls(1)[0].first==0x10,"player 2 takes a free key");
}
void credits_check() {
    // credit.pc rows (0x47f840) and the unlock-all (0x496e10).
    std::vector<uint8_t> b(4+2*50,0);b[0]=2;b[4]=1;b[5]='A';b[54]=0;b[55]='B';
    const auto lines=wpc::parse_credits(b);
    require(lines.size()==2 && lines[0].style==1 && lines[0].text=="A" && lines[1].text=="B","credit rows");
    rejects([&]{auto c=b;c.pop_back();wpc::parse_credits(c);},"credits size check");
    wpc::Profile p=wpc::Profile::fresh();p.unlock_all();
    require(p.scenario_open(0) && p.championship_open(4) && p.satanas==1 && p.wins[0]==0x7f && p.all_races_won(),"0x496e10 opens everything");
}
void animation() {
    // MSVC rand() from seed 1: 41, 18467, 6334.
    wpc::MsvcRandom random(1);
    require(random.next()==41 && random.next()==18467 && random.next()==6334,"MSVC rand sequence (0x430e40)");
    // WSpriteMove 0x492f10/0x492f80 with menu.WGM's road values: steps of 3,
    // back to 320 once at or below -380, a 234-update loop.
    {
        wpc::SpriteMove road;road.start=320;road.end=-380;road.interval=1;road.minimum=3;
        road.init(-280,random);
        require(road.step==3 && road.update(random)==-283,"road step 3");
        int n=1;while (road.update(random)!=320) ++n;
        require(n==33,"first restart after (-280+380)/3 updates");
        int period=1;while (road.update(random)!=320) ++period;
        require(period==234,"road loop of 234 updates");
        wpc::SpriteMove random_steps;random_steps.init(0,random);
        require(random_steps.step>=1 && random_steps.step<=10,"default steps 1..10");
    }
    const std::vector<int> lengths={1,1,4,26,10,20,31,40,50,60,80,90,19,1,48,109,4};
    wpc::FilmState film; film.lengths=lengths; film.speed=0.5f;
    film.play(12,1);
    for (int i=0;i<36;++i) film.update();
    require(film.frame==18 && film.done,"mode 1 holds the last frame (0x430110)");
    film.accumulator=0; film.play(3,2); film.last=24;
    for (int i=0;i<48;++i) film.update();
    require(film.frame==24,"mode 2 reaches the last frame");
    film.update(); require(film.frame==0,"mode 2 wraps past the last frame");
    film.play(2,3); film.accumulator=0;
    for (int i=0;i<7;++i) film.update();
    require(film.frame==3 && film.speed<0,"mode 3 turns at the last frame");
    // Speaking: the gesture length follows the voice (sequence k ~ (k-3)*2/3 s).
    const auto talk=[&](int duration60) {
        wpc::FilmState body,mutt,head;
        body.lengths=mutt.lengths=head.lengths=lengths; body.speed=mutt.speed=head.speed=0.5f;
        body.mode=mutt.mode=2; mutt.sequence=13; head.sequence=3; head.mode=2; head.last=24; head.active=false;
        wpc::MenuAnimation a(body,mutt,head);
        a.speech_started(0,duration60); a.update();
        require(head.active && body.mode==1,"Satanas talks with the head film active");
        return body.sequence;
    };
    require(talk(0)==4 && talk(56)==4 && talk(80)==5 && talk(279)==10 && talk(280)==9 && talk(343)==11 && talk(400)==11,
            "talk gesture chosen from the voice length (0x494441)");
    wpc::FilmState body,mutt,head;
    body.lengths=mutt.lengths=head.lengths=lengths; body.speed=mutt.speed=head.speed=0.5f;
    body.mode=mutt.mode=2; mutt.sequence=13; head.sequence=3; head.mode=2; head.last=24; head.active=false;
    wpc::MenuAnimation a(body,mutt,head);
    a.speech_started(0,56); a.update();
    for (int i=0;i<25;++i) { a.update(); body.update(); }
    require(body.sequence==1 && a.satanas_state()==1,"finished talk gesture holds sequence 1");
    a.speech_finished(); a.update();
    require(!head.active && body.sequence==0 && a.satanas_state()==0,"voice end hides the head and restores sequence 0");
    a.speech_started(1,56); a.update();
    require(mutt.sequence==14,"Muttley speaks with sequence 14");
    a.speech_finished(); a.update();
    require(mutt.sequence==13,"Muttley returns to sequence 13");
}
void installed(const std::string &path) {
    wpc::Archive archive; archive.load(path); wpc::Scene scene; scene.load(archive);
    wpc::MenuController menu(scene.root());
    require(menu.node().id==0x13e && menu.page()->id==4,"installed root");
    require(menu.items().size()==8,"installed main item count");
    require(menu.accept().kind==wpc::MenuEvent::ChangedPage && menu.page()->id==10,"Start -> player-count page");
    menu.accept(); require(menu.page()->id==19 && menu.session().player_mode==0,"one-player mode selection");
    menu.accept(); require(menu.page()->id==28 && menu.session().game_mode==0,"single race -> vehicle page");
    require(menu.horizontal(),"car page must use Left/Right");
    // 0x48dde0 locks row 0 (Satanas et Diabolo) until its profile unlock.
    require(menu.availability(0)==0 && menu.selected()==1,"locked Satanas skipped; car page opens on Dingo");
    menu.move(-1); require(menu.selected()==6,"Left from first unlocked car wraps to Turbo Terrifique");
    menu.move(1); require(menu.selected()==1,"Right wraps past locked Satanas");
    menu.accept();
    require(menu.page()->id==6 && menu.session().vehicles[0]==3,"Dingo vehicle enum must be3, not rowindex1");
    menu.accept(); require(menu.page()->id==0x17b && menu.session().environment==0,"environment child order");
    // 0x4913f0: a fresh profile opens variant 0 only; the closed rows are
    // skipped and drawn at 0x40 (0x48cec0).
    require(menu.availability(0)==1 && menu.availability(1)==0 && menu.greyed(1) && !menu.greyed(0),"fresh profile: cc_t1 only");
    menu.move(1); require(menu.selected()==0,"closed variants skipped");
    menu.profile().tracks[1]=1;
    menu.move(1); auto race=menu.accept();
    require(race.kind==wpc::MenuEvent::LoadTrack && race.node==0x1e5 && menu.session().course==1,"course request retains selected session");
    menu.back(); require(menu.page()->id==6,"track Back");
    menu.back(); require(menu.page()->id==28 && menu.selected()==1,"vehicle cursor retained after Back");
    menu.back(); menu.back(); menu.back(); require(menu.page()->id==4,"return main");
    menu.move(-1); menu.accept();
    require(menu.page()->id==0x251 && menu.selected()==1,"quit confirmation defaults NO");
    require(menu.back().kind==wpc::MenuEvent::None,"YesNo Back override");
    menu.accept(); require(menu.page()->id==4,"NO returns to parent");
    menu.accept(); menu.move(-1); require(menu.accept().kind==wpc::MenuEvent::Quit,"YES quit side effect");
    // Championship I: one WMenuNodeLoadTrack per race, courses cc_t1, sh_t1,
    // dt_t1, ct_t1 (0x491cf7), then the championship ends.
    wpc::MenuController cship(scene.root());
    cship.accept(); cship.accept(); cship.move(1); cship.accept();
    require(cship.page()->id==667 && cship.session().game_mode==3,"championship mode page");
    cship.accept(); require(cship.page()->id==28,"championship car page");
    cship.accept(); require(cship.page()->id==518 && cship.node().id==0x149,"Championship I track page");
    // Each race: the environment's track page (code 8 + environment after
    // the first), only the championship's variant available; then the
    // post-race menu (code 3) and CONTINUER (0x491e10).
    for (int race=0;race<4;++race) {
        const auto request=cship.accept();
        require(request.kind==wpc::MenuEvent::LoadTrack && cship.session().environment==race && cship.session().course==0,"Championship I environment order");
        require(cship.availability(0)==1 && cship.availability(1)==0,"the championship's variant only");
        std::array<int,7> points{};points[3]=10;points[1]=race;
        // The last race won from second place, leading: the winner scene
        // (0x486fe0), then MENU.WGM's post-race page.
        const auto over=cship.race_over(2,points);
        if (race==3) {require(over.kind==wpc::MenuEvent::PlayWinner,"Championship I won: the winner scene");cship.after_winner();}
        else require(over.kind==wpc::MenuEvent::ChangedPage,"championship post-race menu");
        require(cship.node().class_name=="WMenuNodePostRace" && cship.node().integer(0x19d)==3,"championship post-race page");
        while (cship.selected()!=3) cship.move(1);
        const auto next=cship.accept();
        if (race<3) require(next.kind==wpc::MenuEvent::ChangedPage && cship.node().integer(0x19d)==9+race,"CONTINUER: the next environment's track page");
        else require(cship.node().class_name=="WMenuNodeSelectCShip","after the last race: the championship page (0xd)");
    }
    require(cship.session().championship_race==0,"championship restarts after its last race");
    require(cship.session().points[3]==40 && cship.session().points[1]==6,"championship points accumulate per vehicle");
    require(cship.session().profile.championship_open(1),"won from second place and leading: Championship II opens (0x496b80)");
    {
        // Single race: post-race code 2; RECOMMENCER reloads, AUTRE COURSE
        // goes to start code game mode + 4 (the car page 0x14a).
        wpc::MenuController single(scene.root());
        single.accept(); single.accept(); single.accept(); single.accept(); single.accept();
        require(single.race_over(1,{}).kind==wpc::MenuEvent::ChangedPage && single.node().id==0x2ca,"single-race post-race menu");
        while (single.selected()!=3) single.move(1);
        require(single.accept().kind==wpc::MenuEvent::LoadTrack,"RECOMMENCER LA COURSE");
        while (single.selected()!=4) single.move(1);
        single.accept(); require(single.node().id==0x14a,"AUTRE COURSE: the car page");
        // A championship race placed fifth: its points come off, same race.
        wpc::MenuController retry(scene.root());
        retry.accept(); retry.accept(); retry.move(1); retry.accept(); retry.accept(); retry.accept();
        retry.accept();
        std::array<int,7> pts{};pts[3]=2;
        retry.race_over(5,pts);
        while (retry.selected()!=3) retry.move(1);
        retry.accept();
        require(retry.session().championship_race==0 && retry.session().points[3]==0 && retry.node().integer(0x19d)==8,"beyond third: the same race again, its points off");
    }
    require(cship.page()->id==667 && cship.availability(1)==1 && cship.availability(2)==0 && cship.greyed(2),"championship page follows the profile (0x4910a0)");
    {
        // Two players (WMenuNodeTwoPlayer 0x147, page 61, 0x48e320).
        wpc::MenuController two(scene.root());
        two.accept(); two.move(1); two.accept();
        require(two.session().player_mode==1,"2 JOUEURS");
        two.accept();
        require(two.two_player_page() && two.page()->id==61,"two-player car page");
        require(two.two_cursor(0)==1 && two.two_cursor(1)==2 && two.availability(0)==0,"cursors on the first open rows, Satanas locked");
        two.two_move(1,-1); require(two.two_cursor(1)==6,"player 2 skips player 1's row and locked Satanas, wrapping");
        two.two_move(1,1); require(two.two_cursor(1)==2,"and back past them");
        two.two_ready(0); two.two_move(0,1); require(two.two_cursor(0)==1,"a ready player's cursor holds");
        two.two_back(0); require(!two.two_ready_flag(0),"bit 0x10 takes the ready back");
        two.two_ready(0);
        const auto e=two.two_ready(1);
        require(e.kind==wpc::MenuEvent::ChangedPage && two.node().class_name=="WMenuNodeSelectScenario","both ready: the environment page");
        require(two.session().vehicles[1]!=two.session().vehicles[0],"different cars");
        wpc::MenuController leave(scene.root());
        leave.accept(); leave.move(1); leave.accept(); leave.accept();
        leave.two_back(0); require(!leave.two_player_page(),"player 1 not ready: bit 0x10 leaves the page");
    }
    {
        // Practice record (0x4a3473): a faster time takes the record with
        // the vehicle in the high four bits.
        wpc::MenuController practice(scene.root());
        practice.accept(); practice.accept(); practice.move(1); practice.move(1);
        practice.accept();
        require(practice.session().game_mode==2,"ENTRAINEMENT is game mode 2");
        const uint32_t before=practice.session().profile.records[0];
        require(!practice.practice_time(int(before&0xfffffff),3),"an equal time is no record");
        require(practice.practice_time(30000,3) && practice.session().profile.records[0]==(30000u|3u<<28),"a faster time is the record");
    }
    {
        // Championship V won (dr_t4, 0x486fe0 -> win scene -> 0x494b20): the
        // outro, then the credits page (start code 0x2c).
        wpc::MenuController five(scene.root());
        five.profile().unlock_all();
        five.accept(); five.accept(); five.move(1); five.accept();
        for (int i=0;i<8 && five.selected()!=4;++i) five.move(1);
        require(five.page()->id==667 && five.selected()==4,"Championship V row");
        five.accept(); five.accept();
        for (int race=0;race<4;++race) {
            require(five.accept().kind==wpc::MenuEvent::LoadTrack && five.session().environment==4 && five.session().course==race,"Championship V runs dr_t1..dr_t4");
            std::array<int,7> pts{};pts[size_t(five.session().vehicles[0])]=10;
            const auto e=five.race_over(1,pts);
            if (race==3) {require(e.kind==wpc::MenuEvent::PlayWinner,"Championship V won: the winner scene");break;}
            while (five.selected()!=3) five.move(1);
            five.accept();
        }
        five.show_credits(); require(five.credits_screen() && five.session().championship_race==0,"after the scene and the outro: the credits");
    }
    {
        // demo1..3.pc (0x45afb0): course, vehicle and controller runs that
        // add up to the recorded update count.
        const std::string dir=path.substr(0,path.find_last_of('/'));
        const int expect[3][4]={{0,0,4,2767},{1,0,1,2824},{2,2,6,1760}};
        for (int i=0;i<3;++i) {
            std::ifstream in(dir+"/demo"+std::to_string(i+1)+".pc",std::ios::binary);
            const auto r=wpc::Replay::parse(std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>()));
            require(r.environment==expect[i][0] && r.course==expect[i][1] && int(r.session[2])==expect[i][2] && r.frames==expect[i][3],"demo course, vehicle and length");
            wpc::ReplayCursor cursor(r);int updates=0,accelerating=0;
            while (!cursor.done()) {const auto in=wpc::ReplayCursor::input(cursor.next());accelerating+=in.accelerate;++updates;}
            require(updates==r.frames && accelerating>r.frames/2,"controller runs cover the recorded updates");
        }
        rejects([&]{wpc::Replay::parse(std::vector<uint8_t>(100,0));},"a replay is 0x382f9 bytes");
        // Recording (0x45b530's runs) round trips through the buffer.
        wpc::Replay rec;rec.environment=1;rec.course=2;rec.session={{0,0,3,0}};
        for (int i=0;i<300;++i) {wpc::DriveInput in;in.accelerate=true;in.left=i>=290;rec.record(wpc::Replay::word(in));}
        const auto back=wpc::Replay::parse(rec.serialize());
        require(back.frames==300 && back.controller.size()==3 && back.controller[0].first==255 && back.environment==1 && back.course==2,"recorded runs of at most 255");
        wpc::ReplayCursor rc(back);int lefts=0;while (!rc.done()) lefts+=wpc::ReplayCursor::input(rc.next()).left;
        require(lefts==10,"the recording plays back");
    }
    {
        // CREDITS (node 0x21e, code 0x2c): the scroll and WACKY RULEZ.
        wpc::MenuController credits(scene.root());
        const std::string dir=path.substr(0,path.find_last_of('/'));
        for (const char *name:{"/credit.pc","/CREDIT.PC"}) {
            std::ifstream in(dir+name,std::ios::binary);
            if (in) {credits.set_credits(wpc::parse_credits(std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>())));break;}
        }
        require(credits.credits().size()==301 && credits.credits()[0].style==2,"installed credit.pc: 301 rows");
        bool found=false;
        for (size_t i=0;i<8 && !found;++i) {
            while (credits.selected()!=i) credits.move(1);
            found=credits.accept().kind==wpc::MenuEvent::ShowCredits;
            if (!found && (credits.file_screen() || credits.page()->id!=4)) credits.back();
        }
        require(found && credits.credits_screen() && credits.credits_scroll()==256,"CREDITS opens at scroll 256 (0x47f8f0)");
        credits.credits_update("");require(credits.credits_scroll()==255,"one row step a menu update");
        require(credits.credits_update("WACKY RULEZX").kind==wpc::MenuEvent::ChangedSetting && credits.session().profile.satanas==1,"WACKY RULEZ unlocks everything");
        wpc::MenuController wrong(scene.root());
        for (size_t i=0;i<8;++i) {while (wrong.selected()!=i) wrong.move(1); if (wrong.accept().kind==wpc::MenuEvent::ShowCredits) break; if (wrong.file_screen() || wrong.page()->id!=4) wrong.back();}
        require(wrong.credits_update("WACKX RULEZ").kind==wpc::MenuEvent::None && !wrong.session().profile.satanas,"a wrong key restarts the sequence");
        credits.back();require(!credits.credits_screen(),"Back leaves the credits");
    }
    // COMMANDES: WMenuNodeControllerConfig 0x23b (player 1, +0x1a2) on page
    // 560; WMenuControllerItem +0x196 is the control item; Enter waits for a key.
    {
        wpc::MenuController keys(scene.root());
        std::function<bool(int)> reach=[&](int depth) {
            if (keys.node().id==0x23b) return true;
            if (!depth) return false;
            const uint32_t here=keys.node().id;
            for (size_t i=0;i<keys.items().size();++i) {
                while (keys.selected()!=i) keys.move(1);
                const auto e=keys.accept();
                if (keys.binding()) keys.bind_key(0);
                if (keys.file_screen() || keys.credits_screen()) {keys.back();continue;}
                if (keys.node().id!=here && e.kind==wpc::MenuEvent::ChangedPage) {if (reach(depth-1)) return true; keys.back();}
            }
            return false;
        };
        require(reach(3) && keys.page()->id==560 && keys.items().size()>=8,"COMMANDES reaches the player 1 controls page");
        while (keys.items()[keys.selected()]->integer(0x196)!=0) keys.move(1);
        require(keys.control_name(*keys.items()[keys.selected()])==wpc::dik_name(0xc8),"ACCELERER shows Up");
        keys.accept(); require(keys.binding() && keys.control_name(*keys.items()[keys.selected()])=="?","Enter waits for a key");
        keys.bind_key(0x10); require(!keys.binding() && keys.prefs().controls(0)[0].first==0x10,"the key is bound");
        keys.accept(); keys.bind_key(0); require(keys.prefs().controls(0)[0].first==0x10,"Escape cancels");
        keys.accept(); keys.bind_key(0x22); require(keys.prefs().controls(0)[0].first==0x10,"player 2's G is refused");
    }
    // CHARGER/SAUVEGARDER PARTIE (WMenuNodeLoadSave 0x111 mode 0, 0x118
    // mode 1): the file screen of 0x495f30 on a scratch save directory.
    {
        const std::string dir=std::string(std::getenv("TMPDIR")?std::getenv("TMPDIR"):"/tmp")+"/wpc-menu-test-saves";
        std::filesystem::remove_all(dir);
        const auto open_mode=[&](wpc::MenuController &m,int mode) {
            for (size_t i=0;i<m.items().size();++i) {
                while (m.selected()!=i) m.move(1);
                if (m.accept().kind==wpc::MenuEvent::LoadSave && m.files().mode==mode) return true;
                if (m.file_screen()) m.back(); else if (m.page()->id!=4) m.back();
            }
            return false;
        };
        wpc::MenuController files(scene.root());files.set_save_directory(dir);
        files.profile().satanas=1;
        require(open_mode(files,1) && files.file_screen() && files.files().names.empty(),"SAUVEGARDER PARTIE opens an empty list");
        files.accept(); require(files.files().state==3,"an empty slot asks for a name");
        files.type("nicx\bO"); require(files.files().edit=="NICO","typing, backspace, capitals");
        files.accept(); require(files.files().state==2 && std::filesystem::file_size(dir+"/NICO.WSG")==126,"126-byte .WSG written");
        files.accept(); require(!files.file_screen(),"Enter leaves after the message");
        require(open_mode(files,1) && files.files().names.size()==1 && files.files().names[0]=="NICO","the slot list shows the save");
        files.accept(); require(files.files().state==1 && files.files().answer==1,"an occupied slot asks to overwrite, NON first");
        files.move(1); files.accept(); require(files.files().state==3 && files.files().edit=="NICO","OUI keeps the name for editing");
        files.back();
        require(open_mode(files,1),"save screen again");
        files.move(1); files.accept(); files.type("NICO"); files.accept();
        require(files.files().state==4,"a name already listed is refused (0x495ec0)");
        files.back();
        // ENREGISTRER LA COURSE (mode 3) and the replay list (mode 2).
        wpc::Replay last;last.session={{0,0,3,0}};for (int i=0;i<50;++i) last.record(0x40);
        wpc::MenuController recorder(scene.root());recorder.set_save_directory(dir);
        recorder.set_last_replay(last);
        recorder.race_over(1,{});
        while (recorder.selected()!=1) recorder.move(1);
        require(recorder.accept().kind==wpc::MenuEvent::LoadSave && recorder.files().mode==3,"ENREGISTRER LA COURSE opens the .WSR save screen");
        recorder.accept(); recorder.type("RUN"); recorder.accept();
        require(recorder.files().state==2,"replay saved");
        require(std::filesystem::file_size(dir+"/RUN.WSR")==0x382f9,"a .WSR is the raw 0x382f9-byte buffer");
        wpc::MenuController viewer(scene.root());viewer.set_save_directory(dir);
        viewer.accept(); // DEMARRER: page 10 holds the replay loader (0x119, mode 2)
        bool listed=false;
        for (size_t i=0;i<viewer.items().size() && !listed;++i) {
            while (viewer.selected()!=i) viewer.move(1);
            const auto e=viewer.accept();
            if (e.kind==wpc::MenuEvent::LoadSave && viewer.files().mode==2) listed=true;
            else if (viewer.file_screen() || viewer.page()->id!=10) viewer.back();
        }
        require(listed && viewer.files().names.size()==1,"the replay list shows the .WSR");
        const auto play=viewer.accept();
        require(play.kind==wpc::MenuEvent::PlayReplay && viewer.replay() && viewer.replay()->frames==50 && !viewer.file_screen(),"loading a .WSR plays it");
        wpc::MenuController loader(scene.root());loader.set_save_directory(dir);
        require(open_mode(loader,0) && loader.accept().kind==wpc::MenuEvent::ChangedSetting && loader.session().profile.satanas==1,"CHARGER PARTIE loads the profile");
        std::filesystem::remove_all(dir);
    }
    // METEO (item 146, type 8) and POUSSIERE (item 588, type 11): find their
    // page from the main menu, then toggle both (0x48a660).
    // Node 8 (page 29) is a main-menu child.
    bool options=false;
    for (size_t i=0;i<8 && !options;++i) {
        wpc::MenuController m(scene.root());
        for (size_t k=0;k<i;++k) m.move(1);
        if (m.accept().kind!=wpc::MenuEvent::ChangedPage || m.node().id!=8) continue;
        const auto items=m.items();
        const auto find=[&](uint32_t id) -> size_t {
            for (size_t k=0;k<items.size();++k) if (items[k]->id==id) return k;
            return items.size();
        };
        const size_t meteo=find(146),dust=find(588);
        if (meteo==items.size() || dust==items.size()) continue;
        options=true;
        require(m.value_text(*items[meteo])==0xfd && m.value_text(*items[dust])==0x9a,"METEO starts random, POUSSIERE off");
        while (m.selected()!=meteo) m.move(1);
        require(m.accept().kind==wpc::MenuEvent::ChangedSetting && m.session().weather==0 && m.value_text(*items[meteo])==0x0a,"METEO 2 -> 0");
        m.accept(); require(m.session().weather==1 && m.value_text(*items[meteo])==0x0b,"METEO 0 -> 1");
        m.accept(); require(m.session().weather==2,"METEO 1 -> 2");
        while (m.selected()!=dust) m.move(1);
        m.accept(); require(m.session().dust && m.value_text(*items[dust])==0x9b,"POUSSIERE toggles ACTIVE");
        m.accept(); require(!m.session().dust,"POUSSIERE toggles back");
    }
    require(options,"options page with METEO and POUSSIERE");
    std::printf("%s: original navigation, vehicle enum, race request, championship order and quit confirmation passed\n",path.c_str());
}
}
// general.wgm's in-race pause (WMenuBrick, WMenuNodePause): the option
// node, resume, the restart and the QUITTER confirmation.
void pause_menu(const std::string &path) {
    wpc::Archive archive;archive.load(path);
    wpc::Scene scene;scene.load(archive);
    wpc::MenuController pause(scene.root());
    require(pause.node().class_name=="WMenuNodePause" && pause.items().empty(),"pause root page has no items");
    require(pause.open_first_child().kind==wpc::MenuEvent::ChangedPage && pause.items().size()==7,"pause opens its seven-row option page");
    require(pause.accept().kind==wpc::MenuEvent::ResumeGame,"CONTINUER resumes");
    // RETOUR DE FORCE is skipped without a force-feedback device (Wine: the
    // cursor cycles through six rows).
    for (int i=0;i<6;++i) pause.move(1);
    require(pause.selected()==0 && pause.availability(4)==0,"six selectable rows, RETOUR DE FORCE skipped");
    for (int i=0;i<4;++i) pause.move(1);
    require(pause.accept().kind==wpc::MenuEvent::LoadTrack,"RECOMMENCER reloads the track");
    pause.move(1);
    require(pause.accept().kind==wpc::MenuEvent::ChangedPage && pause.node().class_name=="WMenuNodeYesNo" && pause.selected()==1,"QUITTER asks, NON selected");
    pause.move(-1);
    require(pause.accept().kind==wpc::MenuEvent::LoadFile,"OUI loads the menu file");
    wpc::MenuController champ(scene.root());champ.set_championship_race(true);champ.open_first_child();
    const wpc::Node &restart=champ.page()->children[5].children[0];
    require(champ.text_override(restart)==200 && pause.text_override(restart)==0,"REDEMARRER CHAMPIONNAT in a championship (0x48c7db)");
    wpc::MenuController two(scene.root());
    const wpc::Node &title=two.page()->children[0];
    require(two.text_override(title)==0,"one player keeps string 179");
    two.set_player_mode(1);
    require(two.text_override(title)==198,"0x490950: player 1's pause shows 198");
    two.set_pause_player(1);
    require(two.text_override(title)==199,"0x490950: player 2's pause shows 199");
    std::printf("%s: pause menu passed\n",path.c_str());
}
int main(int argc,char **argv) {
    try {
        synthetic(); profile(); prefs(); credits_check(); animation();
        for (int i=1;i<argc;++i) {
            const std::string a=argv[i];
            if (a.size()>=11 && a.compare(a.size()-11,11,"general.wgm")==0) pause_menu(a); else installed(a);
        }
        std::printf("native menu: %zu checks passed\n",checks); return 0;
    } catch (const std::exception &e) { std::fprintf(stderr,"native menu: %s (%zu checks)\n",e.what(),checks); return 1; }
}
