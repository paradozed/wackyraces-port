#include "game_mesh.h"
#include "game_material.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool value,const char* message) { ++checks; if(!value)throw std::runtime_error(message); }
void rejected(const std::function<void()>& action,const char* message) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,message);
}
void put(Bytes& b,size_t offset,uint32_t value) {for(size_t i=0;i<4;++i)b.at(offset+i)=uint8_t(value>>(i*8));}
Bytes words(std::initializer_list<uint32_t> values) {
    Bytes b(values.size()*4);size_t at=0;for(uint32_t value:values){put(b,at,value);at+=4;}return b;
}
Bytes floats(std::initializer_list<float> values) {
    Bytes b(values.size()*4);size_t at=0;for(float value:values){uint32_t bits;std::memcpy(&bits,&value,4);put(b,at,bits);at+=4;}return b;
}
void append(Bytes& b,const Bytes& other){b.insert(b.end(),other.begin(),other.end());}
Bytes chunk(const char* tag,const Bytes& payload){Bytes b(8);std::memcpy(b.data(),tag,4);put(b,4,uint32_t(payload.size()));append(b,payload);return b;}

struct Parts {
    Bytes matrix,mats,vert,tang,tset,hmts,segs;
};
Parts basic() {
    Parts p;
    p.matrix=words({1});append(p.matrix,floats({1,0,0,0,1,0,0,0,1,0,0,0}));
    p.mats=words({2});append(p.mats,{'G','M','A','T'});append(p.mats,words({100}));append(p.mats,{'G','M','A','T'});append(p.mats,words({101}));
    p.vert=words({3});append(p.vert,chunk("POSI",floats({1,0,0,2,0,0,3,0,0})));append(p.vert,chunk("UVUV",floats({0,0,0.5f,0.5f,1,1})));
    p.tang=words({2});append(p.tang,chunk("VIND",words({0,1,2,2,1,0})));
    p.tset=words({2,0,0,3,0,1,1,0,3,1,1});
    p.hmts=words({0,3,1,2,3,4});
    p.segs=words({2,UINT32_MAX,1,0,2,0,0});append(p.segs,floats({10,0,0}));
    append(p.segs,words({0,1,1,2,1,1}));append(p.segs,floats({0,20,0}));
    return p;
}
wpc::Archive archive(const Parts& p,const char* tag="GMDL") {
    Bytes payload=words({1,0x0130e079});
    for(const auto& field:std::vector<std::pair<const char*,Bytes>>{{"MATR",p.matrix},{"MATS",p.mats},{"VERT",p.vert},{"TANG",p.tang},{"TSET",p.tset}})
        append(payload,chunk(field.first,field.second));
    if(std::strcmp(tag,"GHMS")==0){append(payload,chunk("HMTS",p.hmts));append(payload,chunk("SEGS",p.segs));}
    auto resources=chunk(tag,payload);append(resources,chunk("GMAT",words({100,0x0130e079})));append(resources,chunk("GMAT",words({101,0x0130e079})));
    Bytes bytes={'W','P','C','G'};append(bytes,words({0x02020000}));append(bytes,chunk("RSRC",resources));
    wpc::Archive result;result.load_bytes(std::move(bytes));return result;
}

void test_mesh() {
    const auto p=basic();
    auto mesh=wpc::load_mesh(archive(p),"GMDL",1);
    require(mesh.vertices.size()==3 && mesh.triangles.size()==2,"basic geometry counts");
    require(mesh.vertices[1].x==2 && mesh.vertices[1].u==0.5f && mesh.vertices[1].v==0.5f,"position/UV decoding");
    require(mesh.triangles[0].a==0 && mesh.triangles[1].a==2,"triangle indices");
    require(mesh.triangles[0].material==100 && mesh.triangles[1].material==101,"per-TSET GMAT resource IDs");
    require(mesh.matrix_flags==1 && mesh.matrix[0]==1 && mesh.matrix[11]==0,"resource MATR preserved");
    require(mesh.vertices[2].r==1 && mesh.vertices[2].b==1,"no VCOL leaves vertices white");
    // 0x43dd67: VCOL is kept unless every vertex sums to <= 0.
    auto colored=p;append(colored.vert,chunk("VCOL",floats({0,0,0,0.5f,0.25f,0,0.8f,0.9f,1})));
    mesh=wpc::load_mesh(archive(colored),"GMDL",1);
    require(mesh.vertices[0].r==0 && mesh.vertices[1].r==0.5f && mesh.vertices[1].g==0.25f && mesh.vertices[2].b==1 && mesh.vertex_colors,"VCOL decoding");
    auto black=p;append(black.vert,chunk("VCOL",floats({0,0,0,0,0,0,-1,0,0})));
    mesh=wpc::load_mesh(archive(black),"GMDL",1);
    require(mesh.vertices[0].r==1 && mesh.vertices[2].r==1,"all-dark VCOL dropped");
    auto truncated=p;append(truncated.vert,chunk("VCOL",floats({1,1,1,1})));
    require(mesh.vertex_colors==false,"dropped VCOL flagged");
    // 0x4b21ae: TRAN t scales VCOL and becomes the alpha; white without VCOL.
    const float vc[3]={0.5f,1,0.25f};
    auto tc=wpc::textured_vertex_color(0,vc);
    require(tc[0]==0.5f && tc[1]==1 && tc[2]==0.25f && tc[3]==1,"opaque VCOL colour");
    tc=wpc::textured_vertex_color(0.2f,vc);
    require(tc[0]==0.1f && tc[1]==0.2f && tc[3]==0.2f,"TRAN scales VCOL and sets alpha");
    tc=wpc::textured_vertex_color(0.2f,nullptr);
    require(tc[0]==1 && tc[2]==1 && tc[3]==0.2f,"TRAN without VCOL: white, alpha TRAN");
    rejected([&]{wpc::load_mesh(archive(truncated),"GMDL",1);},"truncated VCOL accepted");
    // 0x437040 rewrites each segment's TSETs in the same descending pass that
    // fills its HMTS span, so segment 1 cannot see slot 0 (filled later).
    rejected([&]{wpc::load_mesh(archive(p,"GHMS"),"GHMS",1);},"GHMS slot filled by a later segment accepted");
    auto hierarchy=p;
    hierarchy.tang=words({2});append(hierarchy.tang,chunk("VIND",words({0,1,2,2,1,1})));
    auto h=wpc::load_mesh(archive(hierarchy,"GHMS"),"GHMS",1);
    require(h.vertices.size()==5 && h.triangles.size()==2,"GHMS per-TSET vertex separation");
    const auto& a=h.vertices[h.triangles[0].a];const auto& b=h.vertices[h.triangles[0].b];const auto& c=h.vertices[h.triangles[0].c];
    require(a.x==11 && a.y==0 && b.x==12 && b.y==0 && c.x==13 && c.y==20,
            "reverse segment map overwrite and parent translation");
    // Slot 1 is overwritten by segment 0 only after segment 1's TSET is built.
    const auto& own=h.vertices[h.triangles[1].b];
    require(own.x==12 && own.y==20,"GHMS segment resolves its own span before lower overwrite");
    auto sharing=p;
    put(sharing.hmts,4,4);put(sharing.segs,4+36+16,0);put(sharing.segs,4+36+20,2);
    sharing.tang=words({2});append(sharing.tang,chunk("VIND",words({2,0,1,2,2,3})));
    h=wpc::load_mesh(archive(sharing,"GHMS"),"GHMS",1);
    require(h.vertices.size()==4 && h.triangles[0].a==h.triangles[0].b && h.triangles[1].a==h.triangles[1].b,
            "GHMS shares source vertices only within each TSET");
    require(h.vertices[h.triangles[0].a].y==20 && h.vertices[h.triangles[0].c].y==0 && h.vertices[h.triangles[1].a].y==20,
            "GHMS first occurrence chooses segment per TSET");
    rejected([&]{wpc::load_mesh(archive(p),"GTEX",1);},"nonmesh resource tag accepted");
    auto invalid=p;put(invalid.mats,4+4,999);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"dangling material reference accepted");
    invalid=p;put(invalid.mats,0,0xffffffff);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"MATS count overflow accepted");
    invalid=p;put(invalid.vert,0,0xffffffff);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"VERT count overflow accepted");
    invalid=p;put(invalid.vert,4+4,0xffffffff);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"POSI extent overflow accepted");
    invalid=p;put(invalid.vert,12,0x7fc00000);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"NaN position accepted");
    invalid=p;put(invalid.vert,56,0xcdcdcdcd);
    auto raw_uv=wpc::load_mesh(archive(invalid),"GMDL",1);
    uint32_t uv_bits;std::memcpy(&uv_bits,&raw_uv.vertices[0].u,4);
    require(raw_uv.uninitialized_uv_vertices==1&&uv_bits==0xcdcdcdcd,"original finite UV sentinel preserved without replacement");
    invalid=p;put(invalid.tang,12,3);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"out-of-range VIND accepted");
    invalid=p;put(invalid.tset,4,2);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"TSET material index accepted");
    invalid=p;put(invalid.tset,8,0xffffffff);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"TSET vertex span overflow accepted");
    invalid=p;put(invalid.tset,4+20+12,0);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"overlapping triangle sets accepted");
    invalid=p;put(invalid.tset,4+20+16,0);
    rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"uncovered triangle accepted");
    invalid=hierarchy;put(invalid.segs,4+36,1);
    rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"non-earlier segment parent accepted");
    invalid=hierarchy;put(invalid.segs,4+36+8,0);
    rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"multiply-owned TSET accepted");
    invalid=hierarchy;put(invalid.segs,4+36+12,0);
    rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"uncovered HMTS map slot accepted");
    invalid=hierarchy;put(invalid.tang,12,3);
    rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"out-of-range GHMS map index accepted");
    invalid=hierarchy;put(invalid.hmts,4,0xffffffff);
    rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"HMTS map length overflow accepted");
    for(size_t n=0;n<p.segs.size();++n){invalid=p;invalid.segs.resize(n);
        rejected([&]{wpc::load_mesh(archive(invalid,"GHMS"),"GHMS",1);},"truncated SEGS accepted");}
    for(size_t n=0;n<p.tset.size();++n){invalid=p;invalid.tset.resize(n);
        rejected([&]{wpc::load_mesh(archive(invalid),"GMDL",1);},"truncated TSET accepted");}
    invalid=p;invalid.mats=words({0});invalid.vert=words({0});invalid.tang=words({0});invalid.tset=words({0});
    mesh=wpc::load_mesh(archive(invalid),"GMDL",1);
    require(mesh.vertices.empty()&&mesh.triangles.empty(),"empty world placeholder mesh");
}
} // namespace

int main(int argc,char** argv){
    try{
        test_mesh();std::printf("native mesh: %zu checks passed\n",checks);
        for(int i=1;i<argc;++i){wpc::Archive ar;ar.load(argv[i]);size_t normal=0,hierarchical=0,vertices=0,triangles=0;
            for(const auto& resource:ar.resources())if(resource.tag=="GMDL"||resource.tag=="GHMS"){
                auto mesh=wpc::load_mesh(ar,resource.tag,resource.id);vertices+=mesh.vertices.size();triangles+=mesh.triangles.size();
                if(resource.tag=="GMDL")++normal;else++hierarchical;}
            std::printf("%s: %zu GMDL, %zu GHMS, %zu posed vertices, %zu triangles\n",argv[i],normal,hierarchical,vertices,triangles);}
        return 0;
    }catch(const std::exception& error){std::fprintf(stderr,"native mesh: %s (after %zu checks)\n",error.what(),checks);return 1;}
}
