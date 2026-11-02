#include "game_motion.h"
#include "game_scene.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool value,const char* message) {++checks;if(!value) throw std::runtime_error(message);}
void rejected(const std::function<void()>& action,const char* message) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,message);
}
bool near(float a,float b) {return std::fabs(a-b)<1e-4f;}
void put(Bytes& b,uint32_t v) {for(size_t i=0;i<4;++i) b.push_back(uint8_t(v>>(i*8)));}
void putf(Bytes& b,float f) {uint32_t v;std::memcpy(&v,&f,4);put(b,v);}
Bytes chunk(const char* tag,const Bytes& payload) {
    Bytes b(tag,tag+4);put(b,uint32_t(payload.size()));b.insert(b.end(),payload.begin(),payload.end());return b;
}
wpc::Archive archive(const Bytes& mtnl) {
    Bytes bytes={'W','P','C','G'};put(bytes,0x02020000);
    bytes=[&]{Bytes b=bytes;const Bytes r=chunk("RSRC",chunk("MTNL",mtnl));b.insert(b.end(),r.begin(),r.end());return b;}();
    wpc::Archive a;a.load_bytes(std::move(bytes));return a;
}
struct Key {int32_t t;float x,y,z;};
// One segment, clip 0 (20 units) chained to clip 1 (10 units).
Bytes library(int32_t chain=1,uint32_t index_limit=0) {
    Bytes b;put(b,5);put(b,0x1310988);
    const std::vector<Key> keys={{0,0,0,0},{20,10,0,0},{0,0,0,0},{20,0,0,0},{0,0,0,0},{20,0,0,0},
                                 {0,10,0,0},{10,0,0,0},{0,0,0,0},{10,0,0,0},{0,0,0,0},{10,0,0,0}};
    put(b,0);put(b,2);put(b,6);put(b,uint32_t(keys.size()));put(b,1);
    for(uint32_t w:{0u,uint32_t(chain),20u,3u,0u}) put(b,w);
    for(uint32_t w:{0u,0xffffffffu,10u,3u,3u}) put(b,w);
    for(uint32_t i:{0u,2u,4u,6u,8u,index_limit?index_limit:10u}) put(b,i);
    for(const Key& k:keys) {put(b,uint32_t(k.t));putf(b,k.x);putf(b,k.y);putf(b,k.z);}
    return b;
}
float step(wpc::MotionController& c,int32_t dt=10) {
    wpc::MotionPairs pairs(9,{0.f,0.f});c.update(dt,&pairs);return wpc::motion_value(pairs[0]);
}
void test_motion() {
    const wpc::MotionLib lib=wpc::parse_motion(archive(library()),5);
    require(lib.segments==1 && lib.clips.size()==2 && lib.keys.size()==12,"MTNL counts");
    require(lib.clips[0].next==1 && lib.clips[0].duration==20 && lib.clips[1].first_index==3,"MTNL clip fields in serialized order");
    // 0x440650 slot resolution.
    require(wpc::motion_value({0,0})==0 && wpc::motion_value({3,0})==3 && wpc::motion_value({0,-2})==-2,"slot from one side");
    require(wpc::motion_value({3,-2})==1 && wpc::motion_value({-1,4})==4,"slot sum and lock");
    wpc::MotionController c(lib);
    c.play(0);
    require(c.active(),"play activates the controller");
    // 0x43feb0: a channel starts one unit before the clip, so the first
    // segment spans 21 units: 11/21 of the key after the first update.
    require(near(step(c),110.f/21.f),"first interpolated value");
    require(near(step(c),10.f),"value reaches the key");
    // Clip 1 follows; its channel interpolates from 10 towards its key 0.
    require(near(step(c),0.f),"chained clip");
    step(c);
    require(!c.active(),"chain end stops the controller");
    wpc::MotionController held(lib);
    held.play(0);held.stop_at_end();step(held);step(held);step(held);
    require(!held.active(),"stop at end");
    rejected([&]{wpc::parse_motion(archive(library(2)),5);},"clip chain out of range accepted");
    rejected([&]{wpc::parse_motion(archive(library(1,13)),5);},"channel index outside the keys accepted");
    Bytes short_lib=library();short_lib.pop_back();
    rejected([&]{wpc::parse_motion(archive(short_lib),5);},"truncated MTNL accepted");
    rejected([&]{wpc::parse_motion(archive(library()),6);},"MTNL identity mismatch accepted");
}
void test_pose() {
    wpc::Mesh mesh;
    mesh.segments={{UINT32_MAX,10,0,0},{0,0,5,0}};
    mesh.vertices.resize(2);mesh.vertex_segment={1,0};mesh.rest={{{1,0,0}},{{0,1,0}}};
    wpc::MotionPairs pairs(18,{0.f,0.f});
    wpc::pose_mesh(mesh,pairs);
    require(near(mesh.vertices[0].x,11) && near(mesh.vertices[0].y,5),"zero motion is the static SEGS pose");
    // Segment 0 turned 90 degrees about Z (0x43b690): local X -> (0,-1,0).
    pairs[5]={90.f,0.f};
    wpc::pose_mesh(mesh,pairs);
    require(near(mesh.vertices[0].x,15) && near(mesh.vertices[0].y,-1),"child follows its parent's rotation");
    require(near(mesh.vertices[1].x,11) && near(mesh.vertices[1].y,0),"root vertex rotated about its segment");
    pairs[5]={0.f,0.f};pairs[7]={1.f,0.f};pairs[0]={2.f,0.f};
    wpc::pose_mesh(mesh,pairs);
    require(near(mesh.vertices[1].x,12) && near(mesh.vertices[1].y,2),"scale 1 + v on the Y row, translation added");
    pairs.pop_back();
    rejected([&]{wpc::pose_mesh(mesh,pairs);},"pair count mismatch accepted");
}
const wpc::Node* find_node(const wpc::Node& n,uint32_t id) {
    if(n.id==id) return &n;
    for(const auto& c:n.children) if(const auto* f=find_node(c,id)) return f;
    return nullptr;
}
uint32_t word(const std::vector<uint8_t>& b,size_t at) {uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
// Corpus: every MTNL parses; each GDHModel's MTNL matches its GHMS segment
// count; every clip plays to its end with finite poses.
void corpus(const wpc::Archive& a,const char* path) {
    size_t libs=0,models=0,clips=0;
    std::map<uint32_t,wpc::MotionLib> parsed;
    for(const auto& r:a.resources()) if(r.tag=="MTNL") {parsed.emplace(r.id,wpc::parse_motion(a,r.id));++libs;}
    wpc::Scene scene;scene.load(a);
    std::function<void(const wpc::Node&)> walk=[&](const wpc::Node& n) {
        const auto m=n.properties.find(0x65),g=n.properties.find(0x67);
        if(n.class_name=="GDHModel" && m!=n.properties.end() && g!=n.properties.end() && m->second.size()==8 &&
           std::memcmp(m->second.data(),"MTNL",4)==0 && std::memcmp(g->second.data(),"GHMS",4)==0) {
            const auto found=parsed.find(word(m->second,4));
            if(found!=parsed.end()) {
                wpc::Mesh mesh=wpc::load_mesh(a,"GHMS",word(g->second,4));
                require(found->second.segments==mesh.segments.size(),"MTNL segment count matches its GHMS");
                for(size_t c=0;c<found->second.clips.size();++c) {
                    wpc::MotionController ctl(found->second);ctl.play(c);ctl.stop_at_end();
                    for(int f=0;f<2000 && ctl.active();++f) {
                        wpc::MotionPairs pairs(mesh.segments.size()*9,{0.f,0.f});
                        ctl.update(10,&pairs);wpc::pose_mesh(mesh,pairs);
                        for(const auto& v:mesh.vertices) require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"finite motion pose");
                    }
                    ++clips;
                }
                ++models;
            }
        }
        for(const auto& c:n.children) walk(c);
    };
    walk(scene.root());
    std::printf("%s: %zu MTNL, %zu animated GDHModels, %zu clips played\n",path,libs,models,clips);
}
}
int main(int argc,char** argv) {
    try {
        test_motion();test_pose();
        std::printf("native motion: %zu checks passed\n",checks);
        for(int i=1;i<argc;++i) {wpc::Archive a;a.load(argv[i]);corpus(a,argv[i]);}
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native motion: %s (%zu checks)\n",e.what(),checks);return 1;}
}
