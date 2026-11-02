#include "game_world.h"
#include "game_files.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace {
using Bytes=std::vector<uint8_t>;
size_t checks=0;
void require(bool v,const char* text) {++checks;if(!v) throw std::runtime_error(text);}
void rejected(const std::function<void()>& action,const char* text) {
    bool failed=false;try{action();}catch(const std::runtime_error&){failed=true;}require(failed,text);
}
Bytes words(std::initializer_list<uint32_t> values) {
    Bytes out;for(uint32_t v:values) for(unsigned i=0;i<4;++i) out.push_back(uint8_t(v>>(i*8)));return out;
}
Bytes floats(std::initializer_list<float> values) {
    Bytes out;for(float f:values) {uint32_t v;std::memcpy(&v,&f,4);auto b=words({v});out.insert(out.end(),b.begin(),b.end());}return out;
}
void append(Bytes& a,const Bytes& b) {a.insert(a.end(),b.begin(),b.end());}
Bytes chunk(const char* tag,const Bytes& data) {
    Bytes b(tag,tag+4);append(b,words({uint32_t(data.size())}));append(b,data);return b;
}
wpc::Archive archive(Bytes resources) {
    Bytes out={'W','P','C','G'};append(out,words({0x02020000}));append(out,chunk("RSRC",resources));
    wpc::Archive a;a.load_bytes(std::move(out));return a;
}
wpc::Archive mesh_archive() {
    Bytes mesh=words({1,0x0130e079}),matrix=words({1});
    append(matrix,floats({1,0,0,0,1,0,0,0,1,0,0,3}));append(mesh,chunk("MATR",matrix));
    append(mesh,chunk("MATS",words({1,0x54414d47,0x10065})));
    Bytes vert=words({3});append(vert,chunk("POSI",floats({0,0,0,1,0,0,0,1,0})));
    append(mesh,chunk("VERT",vert));
    Bytes tang=words({1});append(tang,chunk("VIND",words({0,1,2})));append(mesh,chunk("TANG",tang));
    append(mesh,chunk("TSET",words({1,0,0,3,0,1})));
    return archive(chunk("GMDL",mesh));
}
wpc::Archive material_archive() {
    Bytes mat=words({0x10065,0x0130e079});append(mat,chunk("TEXP",words({1,0,0})));
    append(mat,chunk("COLS",floats({0,0,0,.25f,.5f,.75f,0,0,0})));
    append(mat,chunk("TRAN",floats({.2f})));append(mat,chunk("CLAM",words({3})));append(mat,chunk("TWOS",words({1})));
    return archive(chunk("GMAT",mat));
}
wpc::Node camera() {
    wpc::Node n;n.id=4;n.class_name="WTrackCamera";
    n.properties={{0x2c,floats({1.04719758f})},{0x2d,words({100})},{0x2e,floats({.5f})},{0x2f,floats({20000})}};
    return n;
}
void test_world() {
    const auto mesh=mesh_archive(),general=material_archive();
    wpc::Node root;root.id=1;root.class_name="GDBrick";
    // Parent rotates X into Y; the child and MATR translations must also
    // rotate before the parent's translation is applied.
    root.properties[0x1b]=floats({0,1,0,-1,0,0,0,0,1,10,20,30});
    wpc::Node model;model.id=2;model.class_name="WGndModel";
    model.properties[0x67]=words({0x4c444d47,1});
    model.properties[0x1b]=floats({1,0,0,0,1,0,0,0,1,2,0,0});
    root.children={model,camera()};
    wpc::Node screen;screen.class_name="GDScreen";screen.id=3;
    wpc::Node view;view.id=5;view.class_name="GDViewport";
    view.properties={{0xc4,words({4})},{0xc5,words({222})},{0xc6,words({165})},{0xc7,words({82})}};
    screen.children={view};
    auto data=wpc::load_world({&mesh,&general},root,screen);
    require(data.models==1&&data.sprites==0&&data.vertex_count==3,"course model collection");
    require(data.batches.size()==1&&data.textures.empty(),"null-tag flat material needs no arbitrary texture");
    const auto& b=data.batches[0];
    require(b.node_id==2&&b.material_id==0x10065,"full resource-group material ID preserved");
    require(b.vertices[0].x==10&&b.vertices[0].y==22&&b.vertices[0].z==33,"ancestor, instance and resource transform order");
    require(b.vertices[1].x==10&&b.vertices[1].y==23&&b.vertices[2].x==9,"basis rotation applied to geometry");
    require(b.vertices[0].r==.25f&&b.vertices[0].g==.5f&&b.vertices[0].b==.75f&&b.vertices[0].a==.8f,"flat material color/opacity retained");
    require(data.camera_id==4&&data.camera_from_viewport,"serialized viewport camera selection");
    require(std::abs(data.clear[0]-222.f/255)<1e-6,"viewport background selection");
    const auto m=wpc::multiply_matrix(wpc::node_transform(root),wpc::inverse_affine_matrix(wpc::node_transform(root)));
    for(size_t i=0;i<16;++i) require(std::abs(m[i]-wpc::identity_matrix()[i])<1e-5,"actual affine inverse");
    rejected([&]{wpc::load_world({&mesh},root,screen);},"missing cross-archive material accepted");
    const auto duplicate=mesh_archive();
    rejected([&]{wpc::load_world({&mesh,&general,&duplicate},root,screen);},"ambiguous resource accepted");
    screen.children[0].properties[0xc4]=words({99});
    rejected([&]{wpc::load_world({&mesh,&general},root,screen);},"missing viewport camera accepted");
    screen.children[0].properties[0xc4]=words({0});
    data=wpc::load_world({&mesh,&general},root,screen);
    require(data.camera_id==4&&!data.camera_from_viewport,"unbound viewport exposes provisional static camera");
    wpc::Node sprite;sprite.id=6;sprite.class_name="WGndSprite";
    sprite.properties={{9,words({0x54414d47,0x10065})},{0xb,floats({.5f})},{0xc,floats({0})},{0xd,floats({2})},{0xe,floats({.5f})}};
    root.children.push_back(sprite);
    data=wpc::load_world({&mesh,&general},root,screen);
    require(data.sprites==1&&data.vertex_count==9,"type-zero ground sprite quad");
    require(data.batches[1].vertices[0].x==10&&data.batches[1].vertices[0].y==19,"sprite pivot and ancestor transform");
    require(data.batches[1].vertices[2].x==9&&data.batches[1].vertices[2].y==21,"sprite scale and aspect");
    // 0x419b90: facing 0 turns about the world up axis toward the camera
    // position; the anchor fraction scales by s (0x419c36).
    require(data.batches[1].billboard&&data.batches[1].facing==0&&data.batches[1].up[1]==1,"type-zero sprite faces the camera");
    {
        auto faced=data.batches;
        wpc::Matrix4 cam=wpc::identity_matrix();cam[12]=10;cam[13]=19;cam[14]=-1000; // straight behind on -z
        wpc::face_billboards(faced,cam);
        const auto& q=faced[1].vertices;
        require(q[0].x==9&&q[1].x==11&&q[0].z==q[1].z,"facing a camera on -z spans x");
        cam[12]=-1000;cam[14]=data.batches[1].anchor[2];
        faced=data.batches;wpc::face_billboards(faced,cam);
        require(std::abs(faced[1].vertices[0].x-10)<1e-3f&&std::abs(faced[1].vertices[0].z-faced[1].anchor[2]-1)<1e-3f,"facing a camera on -x spans z");
    }
    root.children.back().properties[0xc]=floats({.5f});
    data=wpc::load_world({&mesh,&general},root,screen);
    require(data.batches[1].vertices[0].x==11,"the anchor fraction scales by s, not by the height"); // local y -0.5 x 2 = -1, the ancestor maps local y to -x
    root.children.back().properties[0xc]=floats({0});
    root.children.back().properties[0xa]=words({1});
    data=wpc::load_world({&mesh,&general},root,screen);
    require(data.sprites==1&&data.unsupported_sprites==0&&!data.batches[1].billboard,"facing 1 keeps the sprite's frame");
    root.children.erase(root.children.begin()+1);
    // Battle arenas hold no camera: the static view is the identity.
    data=wpc::load_world({&mesh,&general},root,screen);
    require(data.camera_id==0 && data.view_projection==wpc::identity_matrix(),"a course without a camera keeps an identity view");
    // Shared race instances must replace the template root's old world
    // position, while retaining descendants and mesh-local MATR translation.
    wpc::Node shared;shared.id=20;shared.class_name="GDModel";
    shared.properties[0x1b]=floats({1,0,0,0,1,0,0,0,1,9000,9000,9000});
    shared.children={model};
    wpc::Node race_camera=camera();race_camera.id=30;race_camera.class_name="GDCamera";
    wpc::WorldSetup setup;
    auto frame=wpc::identity_matrix();frame[12]=100;frame[13]=200;frame[14]=300;
    setup.instances.push_back({&shared,frame});setup.camera=&race_camera;
    data=wpc::load_world({&mesh,&general},root,screen,setup);
    require(data.models==2&&data.camera_from_race&&!data.camera_from_viewport&&data.camera_id==30,"shared runtime model and camera binding");
    const auto& instance=data.batches.back().vertices;
    require(instance[0].x==102&&instance[0].y==200&&instance[0].z==303,"runtime root override keeps child and resource matrices");
    require(data.view_projection==wpc::camera_projection(race_camera),"runtime camera frame drives projection");
    setup.instances[0].root=nullptr;
    rejected([&]{wpc::load_world({&mesh,&general},root,screen,setup);},"null runtime instance accepted");
}
void test_translucent() {
    // 0x4afcc2: lists by draw flags, each far to near, ties newest first;
    // keys are truncated and compared unsigned (negative z sorts as huge).
    wpc::TranslucentQueue q;
    q.add(0x80062,10.9f,0);q.add(0x80062,30,1);q.add(0x80062,10.2f,2);q.add(0x80066,5,3);
    q.add(0x20004,50,4);q.add(0x80062,-1,5);
    std::vector<size_t> order;q.flush([&](size_t i){order.push_back(i);});
    require(order==std::vector<size_t>({3,5,1,2,0,4}),"translucent queue order");
    q.clear();order.clear();q.flush([&](size_t i){order.push_back(i);});
    require(order.empty(),"translucent queue clear");
    std::vector<wpc::WorldBatch> batches(4);
    batches[0].node_id=7;batches[0].translucent=true;batches[1].node_id=8;
    batches[2].node_id=9;batches[2].translucent=true;batches[3].node_id=7;batches[3].translucent=true;
    const auto models=wpc::translucent_models(batches);
    require(models.size()==2 && models[0].batches==std::vector<size_t>({0,3}) && models[1].batches==std::vector<size_t>({2}),
            "translucent batches grouped per model");
}
void test_files() {
    namespace fs=std::filesystem;
    const auto dir=fs::temp_directory_path()/("wpc-file-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir/"first"/"CC");fs::create_directories(dir/"second"/"cc");
    struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{dir};
    std::ofstream(dir/"first"/"CC"/"cc_t1.WGM")<<"first";
    std::ofstream(dir/"second"/"cc"/"CC_T1.wgm")<<"second";
    require(wpc::game_file({dir/"first",dir/"second"},"cc\\CC_T1.wgm")==dir/"first"/"CC"/"cc_t1.WGM","component case matching, Windows separator and root priority");
    require(wpc::game_file({dir/"absent",dir/"second"},"CC/cc_t1.WGM")==dir/"second"/"cc"/"CC_T1.wgm","missing installed file resolves from disc root");
    rejected([&]{wpc::game_file({dir},"../cc_t1.wgm");},"parent traversal accepted");
    rejected([&]{wpc::game_file({dir},"/cc_t1.wgm");},"absolute asset path accepted");
    rejected([&]{wpc::game_file({dir},"C:\\cc_t1.wgm");},"drive path accepted");
    rejected([&]{wpc::game_file({dir},"missing.wgm");},"missing file accepted");
    std::ofstream(dir/"first"/"CC"/"CC_T1.wgm")<<"ambiguous";
    rejected([&]{wpc::game_file({dir/"first"},"cc/cc_t1.wgm");},"case ambiguity accepted");
    for(int env=0;env<5;++env) for(int c=0;c<4;++c) {
        const auto name=wpc::course_file(0,env,c);
        require(name.substr(name.size()-6)=="t"+std::to_string(c+1)+".wgm","course variant table");
        require(wpc::course_file(2,env,c)==name&&wpc::course_file(3,env,c)==name,"training and championship course table");
    }
    require(wpc::course_file(1,4,3)=="dr/dr_b.wgm","battle course table");
    rejected([]{wpc::course_file(0,5,0);},"environment bounds");
    rejected([]{wpc::course_file(0,0,4);},"course bounds");
    // Championship I is the first variant of each environment in turn.
    const char* first[4]={"cc/cc_t1.wgm","sh/sh_t1.wgm","dt/dt_t1.wgm","ct/ct_t1.wgm"};
    for(int race=0;race<4;++race) {
        const auto c=wpc::championship_course(0,race);
        require(wpc::course_file(3,c.environment,c.course)==first[race],"Championship I course order (0x491cf7)");
    }
    const auto c3=wpc::championship_course(2,3);
    require(wpc::course_file(3,c3.environment,c3.course)=="ct/ct_t3.wgm","championship N keeps variant N");
    const auto dr=wpc::championship_course(4,2);
    require(wpc::course_file(3,dr.environment,dr.course)=="dr/dr_t3.wgm","championship 4 steps dr variants");
    rejected([]{wpc::championship_course(0,4);},"championship race bounds");
    rejected([]{wpc::course_file(-1,0,0);},"mode bounds");
}
}
int main(int argc,char**argv) {
    try {
        test_world();test_translucent();test_files();std::printf("native world: %zu checks passed\n",checks);
        if(argc>1) {
            wpc::Archive general;general.load(argv[1]);
            for(int i=2;i<argc;++i) {
                wpc::Archive a;a.load(argv[i]);wpc::Scene s;s.load(a);
                const auto w=wpc::load_world({&a,&general},s.root(),s.screen());
                size_t translucent=0;for(const auto& b:w.batches) translucent+=b.translucent;
                std::printf("%s: %zu models, %zu sprites (%zu unsupported), %zu batches (%zu translucent, %zu models), %zu vertices, %zu textures, camera 0x%x\n",
                    argv[i],w.models,w.sprites,w.unsupported_sprites,w.batches.size(),translucent,wpc::translucent_models(w.batches).size(),
                    w.vertex_count,w.textures.size(),w.camera_id);
            }
        }
        return 0;
    }catch(const std::exception& e) {std::fprintf(stderr,"native world: %s (%zu checks)\n",e.what(),checks);return 1;}
}
