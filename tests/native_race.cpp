#include "game_race.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>

namespace {
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void near(float a,float b,float tolerance,const char* text) {require(std::abs(a-b)<=tolerance,text);}
void rejected(const std::function<void()>& action,const char* text) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,text);
}
std::vector<uint8_t> word(uint32_t n) {return {uint8_t(n),uint8_t(n>>8),uint8_t(n>>16),uint8_t(n>>24)};}
wpc::Node templates() {
    wpc::Node root;root.id=1;root.class_name="GDBrick";
    for(unsigned vehicle=0;vehicle<7;++vehicle) {
        const uint32_t base=10+vehicle*10;
        wpc::Node shadow;shadow.id=base;shadow.class_name="GDModel";
        wpc::Node car;car.id=base+1;car.class_name="WackyAuto";
        car.properties={{0xed,word(vehicle)},{0x120,word(base)}};
        wpc::Node model;model.id=base+2;model.class_name="GDModel";
        wpc::Node low=model;low.id=base+3;
        root.children.insert(root.children.end(),{shadow,car,model,low});
    }
    for(uint32_t i=0;i<2;++i) {
        wpc::Node camera;camera.id=100+i;camera.class_name="GDCamera";
        wpc::Node action;action.id=110+i;action.class_name="WFollowCamera_2";
        camera.actions.push_back(action);root.children.push_back(camera);
    }
    return root;
}
void test_templates() {
    const auto root=templates();
    const auto t=wpc::load_race_templates(root);
    for(size_t i=0;i<7;++i) {
        require(t.vehicles[i].actor_id==11+i*10,"vehicle enum registry");
        require(t.vehicles[i].model_ids==std::array<uint32_t,2>{{uint32_t(12+i*10),uint32_t(13+i*10)}},"model siblings retained in order");
        require(t.vehicles[i].shadow_id==10+i*10,"shadow reference retained");
    }
    require(t.camera_ids==std::array<uint32_t,2>{{100,101}}&&t.follow_action_ids==std::array<uint32_t,2>{{110,111}},"shared follow-camera owner binding");
    auto invalid=root;invalid.children[1].properties[0xed]=word(7);
    rejected([&]{wpc::load_race_templates(invalid);},"out-of-range vehicle enum accepted");
    invalid=root;invalid.children[5].properties[0xed]=word(0);
    rejected([&]{wpc::load_race_templates(invalid);},"duplicate vehicle enum accepted");
    invalid=root;invalid.children[2].class_name="GDSound";
    rejected([&]{wpc::load_race_templates(invalid);},"nonmodel following sibling accepted");
    invalid=root;invalid.children[1].properties[0x120]=word(999);
    rejected([&]{wpc::load_race_templates(invalid);},"missing shadow link accepted");
    invalid=root;invalid.children.back().actions.clear();
    rejected([&]{wpc::load_race_templates(invalid);},"missing follow camera accepted");
    invalid=root;invalid.children[0].id=1;
    rejected([&]{wpc::load_race_templates(invalid);},"ambiguous template node IDs accepted");
}
void test_roster() {
    uint32_t seed=1;
    for(uint32_t expected:{41u,18467u,6334u,26500u}) require(wpc::race_random(seed)==expected,"original LCG stream");
    for(int mode=0;mode<4;++mode) for(int vehicle=0;vehicle<7;++vehicle) {
        seed=0;const auto roster=wpc::race_roster(mode,vehicle,seed);
        require(roster.front().vehicle==vehicle&&roster.front().player,"selected enum preserved");
        require(roster.size()==size_t(mode==1?2:mode==2?1:7),"original per-mode participant count");
        std::set<int> vehicles;std::set<unsigned> slots;
        for(const auto& r:roster) {vehicles.insert(r.vehicle);slots.insert(r.grid_slot);}
        require(vehicles.size()==roster.size()&&slots.size()==roster.size(),"one-player roster has unique actors and slots");
        if(mode==0 || mode==3) require(roster.front().grid_slot==6&&seed!=0,"primary grid slot and shuffle consumption");
        if(mode==1) require(roster[1].vehicle==(vehicle+1)%7&&seed==0,"battle next occupied registry slot");
        if(mode==2) require(roster.front().grid_slot==2&&seed==0,"training slot without opponents or RNG consumption");
    }
    seed=0;const auto roster=wpc::race_roster(0,3,seed);
    const int expected[]={3,0,6,4,2,1,5};
    for(size_t i=0;i<7;++i) require(roster[i].vehicle==expected[i],"original pair-swap roster ordering");
    rejected([&]{wpc::race_roster(4,0,seed);},"invalid mode accepted");
    rejected([&]{wpc::race_roster(0,-1,seed);},"negative vehicle enum accepted");
}
void test_frames() {
    auto m=wpc::identity_matrix();m[12]=100;m[13]=200;m[14]=300;
    const auto p=wpc::race_grid_frame(m,6);
    require(p[12]==400&&p[13]==200&&p[14]==-300,"grid translation from original tables");
    const auto practice=wpc::race_grid_frame(m,2);
    require(practice[12]==250&&practice[14]==0,"practice grid entry");
    m=wpc::affine_matrix({{.989176691f,0,.146730438f,0,1,0,-.146730438f,0,.989176691f,3246.199707f,1012.059326f,-6313.069336f}});
    const auto cc=wpc::race_grid_frame(m,6);
    near(cc[12],3630.990977f,.001f,"rotated primary candidate X");
    near(cc[13],1012.059326f,.001f,"rotated primary candidate Y");
    near(cc[14],-6862.556219f,.001f,"rotated primary candidate Z");
    for(size_t i=0;i<12;++i) require(cc[i]==m[i],"grid keeps marker basis");
    const auto camera=wpc::initial_follow_camera(wpc::identity_matrix());
    require(camera[12]==0&&camera[13]==400&&camera[14]==-900,"one-player camera eye from initializer");
    near(camera[0],1,1e-6f,"camera right handed X basis");
    near(camera[9],-100.f/std::sqrt(820000.f),1e-6f,"camera forward pitch");
    const auto two=wpc::initial_follow_camera(wpc::identity_matrix(),true);
    require(two[13]==350&&two[14]==-900,"two-player initial eye");
    m=wpc::affine_matrix({{1,0,0,0,.8f,.6f,0,-.6f,.8f,100,200,300}});
    const auto tilted=wpc::initial_follow_camera(m);
    const auto view=wpc::inverse_affine_matrix(tilted);
    float aim[3]={m[12]+150*m[4],m[13]+150*(m[5]+1),m[14]+150*m[6]};
    for(size_t r=0;r<2;++r) near(view[r]*aim[0]+view[4+r]*aim[1]+view[8+r]*aim[2]+view[12+r],0,.0001f,"tilted actor aim projects onto camera axis");
    const auto visual=wpc::initial_vehicle_model_frame(m);
    require(visual[12]==100&&visual[13]==216&&visual[14]==312,"visual model's initial local up offset");
    rejected([&]{wpc::race_grid_frame(m,7);},"grid index overflow accepted");
    m[0]=std::numeric_limits<float>::quiet_NaN();
    rejected([&]{wpc::initial_follow_camera(m);},"nonfinite follow frame accepted");
    m=wpc::identity_matrix();m[4]=m[5]=m[6]=0;
    rejected([&]{wpc::initial_follow_camera(m);},"degenerate camera up accepted");
}
void test_ground_placement() {
    size_t calls=0;
    const std::vector<wpc::Racer> roster{{3,6,true},{1,0,false},{2,1,false}};
    const auto grid=wpc::place_race_grid(roster,wpc::identity_matrix(),
        [&](wpc::GroundPoint start,wpc::GroundPoint end,uint32_t mask) {
            require(start.x==end.x&&start.z==end.z&&start.y-end.y==150,"original vertical spawn segment");
            require(mask==0x018000ff,"original spawn query mask");
            wpc::GroundQueryResult result;
            if(calls==0) {result.hit=true;result.contact.point={end.x+1,42,end.z+2};}
            if(calls==2) {result.hit=true;result.complete=false;result.contact.point={0,-900,0};result.unsupported={"unported context"};}
            ++calls;return result;
        });
    require(calls==3&&grid.racers.size()==3&&!grid.ground_complete,"ground status retained for each participant");
    require(grid.racers[0].frame[12]==301&&grid.racers[0].frame[13]==42&&grid.racers[0].frame[14]==-598,"complete ground hit changes full translation");
    require(grid.racers[1].frame==grid.racers[1].candidate&&grid.racers[1].ground.complete,"original miss preserves candidate");
    require(grid.racers[2].frame==grid.racers[2].candidate&&!grid.racers[2].ground.complete,"incomplete ground query is not committed as a hit or miss");
    rejected([&]{wpc::place_race_grid(roster,wpc::identity_matrix(),{});},"missing query accepted");
}
}
int main(int argc,char** argv) {
    try {
        test_templates();test_roster();test_frames();test_ground_placement();std::printf("native race: %zu checks passed\n",checks);
        for(int i=1;i<argc;++i) {
            wpc::Archive a;a.load(argv[i]);wpc::Scene s;s.load(a);const auto t=wpc::load_race_templates(s.root());
            for(size_t j=0;j<7;++j) std::printf("%s: vehicle %zu actor 0x%x models 0x%x/0x%x shadow 0x%x\n",argv[i],j,t.vehicles[j].actor_id,t.vehicles[j].model_ids[0],t.vehicles[j].model_ids[1],t.vehicles[j].shadow_id);
            std::printf("Follow cameras: 0x%x/0x%x\n",t.camera_ids[0],t.camera_ids[1]);
        }
        return 0;
    }catch(const std::exception& e) {std::fprintf(stderr,"native race: %s (%zu checks)\n",e.what(),checks);return 1;}
}
