#include "game_mode.h"
#include "game_scene.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool value,const char* why) { ++checks; if(!value) throw std::runtime_error(why); }
void rejected(const std::function<void()>& action,const char* why) {
    bool threw=false; try {action();} catch(const std::runtime_error&) {threw=true;} require(threw,why);
}
void put(Bytes& b,size_t p,uint32_t value) { for(size_t i=0;i<4;++i)b.at(p+i)=uint8_t(value>>(i*8)); }
void word(Bytes& b,uint32_t value) {const auto p=b.size(); b.resize(p+4);put(b,p,value);}
void real(Bytes& b,float value) {uint32_t bits;std::memcpy(&bits,&value,4);word(b,bits);}
void append(Bytes& b,const Bytes& extra) {b.insert(b.end(),extra.begin(),extra.end());}
Bytes chunk(const char* tag,const Bytes& payload) {
    Bytes b(tag,tag+4);word(b,uint32_t(payload.size()));append(b,payload);return b;
}
void header(Bytes& b,uint32_t definitions) {
    word(b,0x01312ea1);for(size_t i=0;i<6;++i)real(b,float(i));
    word(b,1);word(b,2);word(b,3);word(b,definitions);
}
void definition(Bytes& b,const char* name,uint32_t rows) {
    const auto start=b.size(); b.resize(start+32,0xcd);std::memcpy(b.data()+start,name,std::strlen(name)+1);
    word(b,100);word(b,200);word(b,201);word(b,0);word(b,rows);
}
struct Fixture {
    Bytes data;size_t dims,paired_count,linked_count,first_cell,route_index,transform_count;
    Fixture() {
        header(data,2);definition(data,"unused",0);definition(data,"selected",1);
        for(size_t i=0;i<12;++i)real(data,float(i)+0.25f);
        word(data,123);word(data,456);dims=data.size();word(data,2);word(data,1);
        word(data,300);word(data,301);word(data,302);
        paired_count=data.size();word(data,1);
        for(size_t i=0;i<6;++i)real(data,float(i)+20);
        linked_count=data.size();word(data,1);
        for(size_t i=0;i<3;++i)real(data,float(i)+30);
        word(data,400);
        word(data,4);first_cell=data.size();
        word(data,(22<<2)|3);word(data,17<<2);word(data,2); // two links, typed terminator
        word(data,3); // empty-cell typed terminator, not in flattened links
        word(data,2);route_index=data.size();word(data,1);word(data,0);
        transform_count=data.size();word(data,1);
        for(size_t i=0;i<12;++i)real(data,float(i)+40);
    }
};
wpc::ModeData decode(const Bytes& b) {return wpc::decode_mode({b.data(),b.size()});}
wpc::Archive archive(const Bytes& mode,const Bytes& tree={},const Bytes& names={}) {
    Bytes b={'W','P','C','G'};word(b,0x02020000);append(b,chunk("RSRC",{}));append(b,chunk("WMOD",mode));
    if(!names.empty())append(b,chunk("CLAS",chunk("BRCM",names)));
    if(!tree.empty())append(b,chunk("BRTR",tree));
    wpc::Archive a;a.load_bytes(std::move(b));return a;
}
Bytes node(uint32_t type,uint32_t id) {Bytes b;word(b,type);word(b,id);append(b,chunk("PRPS",{}));return b;}
void synthetic() {
    const Fixture f;const auto mode=decode(f.data);
    require(mode.definitions.size()==2&&mode.route_indices==std::vector<uint32_t>({1,0}),"route indirection lost");
    require(mode.route(0).name=="selected"&&mode.route(1).name=="unused","definition order used as route order");
    require(mode.route(0).name_bytes[9]==0xcd,"name padding not preserved");
    const auto& row=mode.route(0).rows.at(0);
    require(row.vectors[3][2]==11.25f&&row.word34==123&&row.word38==456,"row vectors/scalars");
    require(row.object_ids==std::array<uint32_t,3>{{300,301,302}}&&row.ground_object_id==302,"plain ground object ID");
    require(row.vectors60[0][0]==20&&row.vectors1e0[0][2]==25,"paired vectors framing");
    require(row.vectors364[0][1]==31&&row.object_ids4e8==std::vector<uint32_t>({400}),"linked vectors framing");
    require(row.stored_link_words==4&&row.cells.size()==2&&row.cells[0].terminator==2&&row.cells[1].terminator==3,"cell terminator framing");
    require(row.object_links==std::vector<uint32_t>({(22<<2)|3,17<<2,2}),"runtime flattened list framing");
    require(mode.transforms.size()==1&&mode.transforms[0][9]==49&&mode.transforms[0][11]==51,"matrix48 byte framing");
    require(wpc::load_mode(archive(f.data)).route(0).name=="selected","Archive WMOD lookup");
    Bytes names;word(names,2);
    for(const char* name:{"GDWorld","WStartLine"}) {word(names,uint32_t(std::strlen(name)+1));names.insert(names.end(),name,name+std::strlen(name)+1);}
    while(names.size()&3)names.push_back(0);
    auto tree=node(0,1);append(tree,chunk("CHBR",node(1,22)));append(tree,chunk("CHBR",node(1,17)));
    wpc::Scene scene;scene.load(archive(f.data,tree,names));
    require(mode.first_start_line_id(scene)==17,"typed link/start-line selection changed");
    auto changed=mode;changed.definitions[1].rows[0].object_links.insert(changed.definitions[1].rows[0].object_links.begin(),999<<2);
    require(changed.first_start_line_id(scene)==17,"unresolved object was not skipped as original");
    changed.route_indices={0};require(!changed.first_start_line_id(scene),"empty first row fallback invented");
    rejected([&]{mode.route(2);},"out-of-bounds route access accepted");
    for(size_t n=0;n<f.data.size();++n) {
        Bytes b(f.data.begin(),f.data.begin()+n);rejected([&]{decode(b);},"truncated WMOD prefix accepted");
    }
    auto mutate=[&](size_t p,uint32_t v){auto b=f.data;put(b,p,v);rejected([&]{decode(b);},"malformed WMOD accepted");};
    mutate(0,0x01312ea0);mutate(40,0xffffffff);mutate(f.paired_count,33);mutate(f.linked_count,33);
    mutate(f.dims,0xffffffff);mutate(f.dims+4,0xffffffff);mutate(f.route_index,2);
    mutate(f.transform_count,0xffffffff);mutate(4,0x7fc00000);mutate(f.transform_count+4,0x7f800000);
    auto b=f.data;append(b,chunk("TEST",{1,2,3}));const auto extra=decode(b);
    require(extra.extras.size()==1&&extra.extras[0].tag=="TEST"&&extra.extras[0].bytes==Bytes({1,2,3}),"unknown WMOD chunks lost");
    for(size_t n=1;n<11;++n) {if(n==8)continue;b=f.data;b.resize(b.size()+n,0);rejected([&]{decode(b);},"unframed/truncated tail accepted");}
    b=f.data;append(b,chunk("TEST",{}));put(b,f.data.size()+4,0xffffffff);rejected([&]{decode(b);},"extra length overflow accepted");
    Bytes empty;header(empty,0);word(empty,0);word(empty,0);
    require(decode(empty).definitions.empty(),"valid empty WMOD rejected");
    rejected([&]{wpc::decode_mode({nullptr,4});},"null storage accepted");
    // A missing terminator cannot consume unlimited memory or overflow the
    // original fixed 2048-word encoded-link buffer.
    b=f.data;b.resize(f.first_cell);for(size_t i=0;i<2049;++i)word(b,4);
    rejected([&]{decode(b);},"unterminated oversized object list accepted");
}
}
int main(int argc,char** argv) {
    try {
        synthetic();
        size_t archives=0,definitions=0,routes=0,rows=0,cells=0,links=0,transforms=0,starts=0;
        for(int i=1;i<argc;++i) {
            wpc::Archive a;a.load(argv[i]);const auto mode=wpc::load_mode(a);
            ++archives;definitions+=mode.definitions.size();routes+=mode.route_indices.size();transforms+=mode.transforms.size();
            for(const auto& definition:mode.definitions)for(const auto& row:definition.rows){++rows;cells+=row.cells.size();links+=row.object_links.size();}
            wpc::Scene scene;scene.load(a);const auto start=mode.first_start_line_id(scene);if(start)++starts;
            std::printf("%s: definitions=%zu routes=%zu transforms=%zu first_start=%s%08x\n",argv[i],mode.definitions.size(),mode.route_indices.size(),mode.transforms.size(),start?"":"absent/",start.value_or(0));
        }
        std::printf("native mode: %zu checks; archives=%zu definitions=%zu routes=%zu rows=%zu cells=%zu link_words=%zu transforms=%zu selected_starts=%zu\n",checks,archives,definitions,routes,rows,cells,links,transforms,starts);
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"native mode: %s\n",e.what());return 1;}
}
