#include "game_material.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void rejected(const std::function<void()>& action,const char* message){
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,message);
}
void put(Bytes& bytes,size_t offset,uint32_t value){for(size_t i=0;i<4;++i)bytes.at(offset+i)=uint8_t(value>>(8*i));}
Bytes words(std::initializer_list<uint32_t> values){
    Bytes bytes(values.size()*4);size_t offset=0;for(uint32_t value:values){put(bytes,offset,value);offset+=4;}return bytes;
}
Bytes floats(std::initializer_list<float> values){
    Bytes bytes(values.size()*4);size_t offset=0;for(float value:values){uint32_t bits;std::memcpy(&bits,&value,4);put(bytes,offset,bits);offset+=4;}return bytes;
}
void append(Bytes& bytes,const Bytes& extra){bytes.insert(bytes.end(),extra.begin(),extra.end());}
Bytes chunk(const char* tag,const Bytes& payload){
    Bytes bytes(8);std::memcpy(bytes.data(),tag,4);put(bytes,4,uint32_t(payload.size()));append(bytes,payload);return bytes;
}
struct Parts{
    Bytes texture=words({1,0x58455447,0x10001});
    Bytes colors=floats({0.1f,0.2f,0.3f,0.25f,0.5f,0.75f,0.7f,0.8f,0.9f});
    Bytes transparency=floats({0.8f}),clamp=words({7}),two_sided=words({3});
};
wpc::Archive archive(const Parts& parts,uint32_t material_id=0x77,uint32_t version=0x0130e079,
                     bool include_texture=false,uint32_t texture_id=0x10001,bool duplicate_colors=false){
    auto material=words({material_id,version});
    append(material,chunk("TEXP",parts.texture));append(material,chunk("COLS",parts.colors));
    append(material,chunk("TRAN",parts.transparency));append(material,chunk("CLAM",parts.clamp));append(material,chunk("TWOS",parts.two_sided));
    // Unknown chunks remain bounded; the parser need not invent their semantics.
    append(material,chunk("UNKN",{1,2,3,4}));
    if(duplicate_colors)append(material,chunk("COLS",parts.colors));
    auto resources=chunk("GMAT",material);
    if(include_texture)append(resources,chunk("GTEX",words({texture_id,0x0130e07e})));
    Bytes bytes={'W','P','C','G'};append(bytes,words({0x02020000}));append(bytes,chunk("RSRC",resources));
    wpc::Archive result;result.load_bytes(std::move(bytes));return result;
}

void test_material(){
    Parts parts;const auto local=archive(parts);
    auto material=wpc::parse_material(local,0x77);
    require(material.textured&&material.texture_id==0x10001,"typed texture link and full group ID");
    require(material.diffuse==std::array<float,3>{{0.25f,0.5f,0.75f}},"middle COLS triplet");
    require(material.transparency==0.8f,"raw TRAN preserved");
    require(material.clamp_axes==3&&material.two_sided,"CLAM low two bits/TWOS low bit");
    parts.texture=words({0});parts.clamp=words({4});parts.two_sided=words({2});
    material=wpc::parse_material(archive(parts),0x77);
    require(!material.textured&&material.texture_id==0,"zero-count material is flat");
    require(material.clamp_axes==0&&!material.two_sided,"unused CLAM/TWOS bits ignored");
    parts.texture=words({1,0,0});
    material=wpc::parse_material(archive(parts),0x77);
    require(!material.textured&&material.texture_id==0,"null resource-tag slot is flat");
    parts.texture=words({1,0,12345});
    require(!wpc::parse_material(archive(parts),0x77).textured,"null tag ignores ID as original link reader does");
    parts.texture=words({1,0x58455447,0});
    material=wpc::parse_material(archive(parts,0x77,0x0130e079,true,0),0x77);
    require(material.textured&&material.texture_id==0,"typed ID zero is not a null-tag slot");
    parts=Parts{};parts.transparency=floats({2.0f});
    require(wpc::parse_material(archive(parts),0x77).transparency==2.0f,"TRAN is not silently clamped");
    parts=Parts{};parts.texture=words({2,0x58455447,1,0x58455447,2});
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"unimplemented multiple texture slots accepted");
    parts=Parts{};parts.texture=words({1,0x54414d47,1});
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"nontexture typed reference accepted");
    parts=Parts{};parts.texture=words({0,0,0});
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"zero-count trailing link bytes accepted");
    parts=Parts{};put(parts.texture,0,0xffffffff);
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"TEXP count overflow accepted");
    parts=Parts{};put(parts.colors,12,0x7fc00000);
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"NaN diffuse component accepted");
    parts=Parts{};put(parts.colors,0,0x7f800000);
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"nonfinite unused COLS component accepted");
    parts=Parts{};put(parts.transparency,0,0x7f800000);
    rejected([&]{wpc::parse_material(archive(parts),0x77);},"nonfinite TRAN accepted");
    parts=Parts{};
    rejected([&]{wpc::parse_material(archive(parts,0x77,0),0x77);},"unknown material version accepted");
    rejected([&]{wpc::parse_material(archive(parts,0x77,0x0130e079,false,0,true),0x77);},"duplicate material chunk accepted");
    const Parts original;
    for(size_t n=0;n<original.texture.size();++n){parts=original;parts.texture.resize(n);
        rejected([&]{wpc::parse_material(archive(parts),0x77);},"truncated TEXP accepted");}
    for(size_t n=0;n<original.colors.size();++n){parts=original;parts.colors.resize(n);
        rejected([&]{wpc::parse_material(archive(parts),0x77);},"truncated COLS accepted");}
    for(size_t n=0;n<4;++n){parts=original;parts.transparency.resize(n);
        rejected([&]{wpc::parse_material(archive(parts),0x77);},"truncated TRAN accepted");
        parts=original;parts.clamp.resize(n);rejected([&]{wpc::parse_material(archive(parts),0x77);},"truncated CLAM accepted");
        parts=original;parts.two_sided.resize(n);rejected([&]{wpc::parse_material(archive(parts),0x77);},"truncated TWOS accepted");}
}

void test_resource_pool(){
    const Parts parts;const auto world=archive(parts);
    const auto general=archive(parts,0x10077,0x0130e079,true);
    const wpc::ArchivePool pool{&world,&general};
    require(&wpc::resolve_resource(pool,"GMAT",0x77)==&world,"world material lookup");
    require(&wpc::resolve_resource(pool,"GMAT",0x10077)==&general,"general material group retained");
    require(&wpc::resolve_resource(pool,"GTEX",0x10001)==&general,"cross-archive texture lookup");
    require(wpc::parse_material(pool,0x77).texture_id==0x10001,"pooled material cross-reference validation");
    rejected([&]{wpc::parse_material(wpc::ArchivePool{&world},0x77);},"missing cross-archive texture accepted");
    rejected([&]{wpc::resolve_resource(pool,"GTEX",1);},"resource group bits stripped");
    rejected([&]{wpc::resolve_resource(pool,"GSMP",0x10001);},"resource tag ignored");
    rejected([&]{wpc::resolve_resource(pool,"TOOLONG",0x77);},"invalid resource tag length accepted");
    rejected([&]{wpc::resolve_resource(wpc::ArchivePool{nullptr},"GMAT",0x77);},"null archive accepted");
    const auto duplicate=general;
    rejected([&]{wpc::resolve_resource(wpc::ArchivePool{&general,&duplicate},"GTEX",0x10001);},"ambiguous resource key accepted");
    require(&wpc::resolve_resource(wpc::ArchivePool{&general,&general},"GTEX",0x10001)==&general,
            "repeated pointer does not create a second resource identity");
}
} // namespace

int main(int argc,char**argv){
    try{
        test_material();test_resource_pool();std::printf("native material: %zu checks passed\n",checks);
        // Optional corpus arguments form one resource pool, e.g. a world and
        // general.wgm. Supplying alternative same-group archives together is
        // deliberately rejected as ambiguous rather than arbitrarily chosen.
        std::vector<wpc::Archive> archives(size_t(argc-1));
        for(int i=1;i<argc;++i)archives[size_t(i-1)].load(argv[i]);
        wpc::ArchivePool pool;for(const auto& archive:archives)pool.push_back(&archive);
        for(size_t i=0;i<archives.size();++i){size_t total=0,textured=0,flat=0;
            for(const auto& resource:archives[i].resources())if(resource.tag=="GMAT"){
                auto material=wpc::parse_material(pool,resource.id);++total;if(material.textured)++textured;else++flat;}
            std::printf("%s: %zu materials, %zu textured, %zu flat; all texture links resolved\n",argv[i+1],total,textured,flat);}
        return 0;
    }catch(const std::exception&error){std::fprintf(stderr,"native material: %s (after %zu checks)\n",error.what(),checks);return 1;}
}
