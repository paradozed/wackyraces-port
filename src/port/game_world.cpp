#include "game_world.h"
#include "game_mesh.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

namespace wpc {
namespace {
uint32_t word(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("game world: "+text);}
void position(WpcTexturedVertex3D& v,const Matrix4& m,float x,float y,float z) {
    v.x=m[0]*x+m[4]*y+m[8]*z+m[12];
    v.y=m[1]*x+m[5]*y+m[9]*z+m[13];
    v.z=m[2]*x+m[6]*y+m[10]*z+m[14];
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) bad("vertex transform overflow");
}
const Node* viewport(const Node& n) {
    if(n.class_name=="GDViewport") return &n;
    for(const auto& c:n.children) if(const Node* v=viewport(c)) return v;
    return nullptr;
}
}
namespace {
struct WorldBuilder {
    const ArchivePool& archives;
    WorldData& out;
    std::map<uint32_t,Material> materials;
    std::map<std::pair<std::string,uint32_t>,Mesh> meshes;
    const Node* first_camera=nullptr;
    std::map<uint32_t,std::pair<const Node*,Matrix4>> cameras;
    const Material& material(uint32_t id) {
        auto found=materials.find(id);
        if(found!=materials.end()) return found->second;
        Material m=parse_material(archives,id);
        if(m.textured && !out.textures.count(m.texture_id)) {
            Image image=resolve_resource(archives,"GTEX",m.texture_id).texture(m.texture_id);
            if(image.rgba.empty()) bad("visible geometry uses an empty texture "+std::to_string(m.texture_id));
            out.textures.emplace(m.texture_id,std::move(image));
        }
        return materials.emplace(id,m).first->second;
    }
    WorldBatch batch(const Node& n,uint32_t id,uint32_t flags_key,const Matrix4& transform) {
        WorldBatch b{n.id,id,material(id),{}};
        b.translucent=b.material.transparency!=0.f || (b.material.textured && out.textures.at(b.material.texture_id).depth>=24);
        b.draw_flags=uint32_t(n.integer(flags_key,0x80062));
        b.origin={{transform[12],transform[13],transform[14]}};
        return b;
    }
    void vertex(WorldBatch& b,const Matrix4& m,float x,float y,float z,float u,float v,const float* color=nullptr) {
        if(out.vertex_count>=4*1024*1024) bad("expanded geometry exceeds world limit");
        WpcTexturedVertex3D result{};
        position(result,m,x,y,z);result.u=u;result.v=v;
        // Flat colors are proven COLS[1] (Glide; the Direct3D colour of
        // untextured parts is not traced). Textured: 0x4b21ae, Wine-checked
        // on ct_t1's snow and the cars' shadows.
        if(b.material.textured) {
            const auto c=textured_vertex_color(b.material.transparency,color);
            result.r=c[0];result.g=c[1];result.b=c[2];result.a=c[3];
        } else {
            result.r=b.material.diffuse[0];result.g=b.material.diffuse[1];result.b=b.material.diffuse[2];
            result.a=1-b.material.transparency;
        }
        b.vertices.push_back(result);++out.vertex_count;
    }
    void visit(const Node& n,const Matrix4& parent,bool replace,bool instance);
};
void WorldBuilder::visit(const Node& n,const Matrix4& parent,bool replace,bool instance) {
    const Matrix4 transform=replace?parent:multiply_matrix(parent,node_transform(n));
    for(float v:transform) if(!std::isfinite(v)) bad("nonfinite runtime instance transform");
    if(!instance && (n.class_name=="WTrackCamera" || n.class_name=="GDCamera")) {
        if(!first_camera || (first_camera->class_name!="WTrackCamera" && n.class_name=="WTrackCamera")) first_camera=&n;
        if(!cameras.emplace(n.id,std::make_pair(&n,transform)).second) bad("duplicate camera ID");
    }
    const auto link=n.properties.find(0x67);
    if(link!=n.properties.end()) {
        if(link->second.size()!=8) bad("invalid model resource link");
        const auto& bytes=link->second;
        if(!std::all_of(bytes.begin(),bytes.end(),[](uint8_t b){return b==0;})) {
            const std::string tag(reinterpret_cast<const char*>(bytes.data()),4);
            if(tag!="GMDL" && tag!="GHMS") bad("unsupported model resource type "+tag);
            const auto key=std::make_pair(tag,word(bytes.data()+4));
            auto found=meshes.find(key);
            if(found==meshes.end()) found=meshes.emplace(key,load_mesh(archives,tag,key.second)).first;
            const Mesh& mesh=found->second;
            const Matrix4 world=multiply_matrix(transform,affine_matrix(mesh.matrix));
            std::map<uint32_t,size_t> batches;
            for(const auto& triangle:mesh.triangles) {
                auto entry=batches.find(triangle.material);
                if(entry==batches.end()) {
                    const size_t index=out.batches.size();
                    out.batches.push_back(batch(n,triangle.material,0x68,transform));
                    entry=batches.emplace(triangle.material,index).first;
                }
                for(uint32_t i:{triangle.a,triangle.b,triangle.c}) {
                    const auto& v=mesh.vertices.at(i);
                    const float color[3]={v.r,v.g,v.b};
                    vertex(out.batches[entry->second],world,v.x,v.y,v.z,v.u,v.v,mesh.vertex_colors?color:nullptr);
                }
            }
            ++out.models;
        }
    }
    // GD3dSprite carries the same 0x09..0x0f sprite fields that WGndSprite
    // (which adds 0x170..0x173) uses: sh_t1/dt_t1/ct_t1 trees and signs.
    if(n.class_name=="WGndSprite" || n.class_name=="GD3dSprite") {
        {
            const float scale=n.real(0x0d,1);
            if(scale!=0) {
                WorldBatch b=batch(n,n.link(9,"GMAT"),0x10,transform);
                float aspect=n.real(0x0e);
                if(aspect==0) {
                    if(!b.material.textured) bad("sprite without explicit aspect has no texture");
                    const auto& image=out.textures.at(b.material.texture_id);
                    aspect=float(image.height)/image.width;
                }
                const float cx=n.real(0x0b,.5f),cy=n.real(0x0c,.5f),angle=n.real(0x0f);
                if(!std::isfinite(scale)||!std::isfinite(aspect)||!std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(angle)) bad("nonfinite sprite geometry");
                // The inspected type-0 path has zero angle. Preserve that
                // boundary until the rotation helper's units are recovered.
                if(angle!=0) bad("nonzero world sprite angle needs recovered rotation units");
                for(unsigned corner:{0u,1u,3u,0u,3u,2u}) {
                    const float u=float(corner&1),v=float(corner>>1);
                    // 0x419c36: x from -cx s, y from -cy s (the anchor
                    // fraction scales by s, not by the height), width s,
                    // height s x aspect. Image row 0 is the top: the upper
                    // corners take v=0.
                    const float x=(u-cx)*scale,y=v*scale*aspect-cy*scale;
                    vertex(b,transform,x,y,0,u,1-v);
                    b.offsets.push_back({{x,y}});
                }
                // 0x419d37, both classes (vtable +0xc of 0x510464 and
                // 0x5131c4): mode 1 uses the sprite's frame; the others keep
                // only its translation and face the camera each frame.
                b.facing=n.integer(0x0a);
                if(b.facing!=1) {
                    b.billboard=true;
                    b.anchor={{transform[12],transform[13],transform[14]}};
                    b.up={{0,1,0}};
                } else b.offsets.clear();
                out.batches.push_back(std::move(b));++out.sprites;
            }
        }
    }
    for(const auto& c:n.children) visit(c,transform,false,instance);
}
}
WorldData load_world(const ArchivePool& archives,const Node& root,const Node& screen,const WorldSetup& setup) {
    WorldData out;
    WorldBuilder builder{archives,out};
    const Node*& first_camera=builder.first_camera;
    auto& cameras=builder.cameras;
    builder.visit(root,identity_matrix(),false,false);
    for(const auto& instance:setup.instances) {
        if(!instance.root) bad("missing instance root");
        builder.visit(*instance.root,instance.transform,true,true);
    }
    const Node* selected=nullptr;
    const Node* view=viewport(screen);
    if(setup.camera) {
        if(setup.camera->class_name!="GDCamera") bad("runtime race camera has an invalid class");
        selected=setup.camera;out.camera_from_race=true;
    } else if(view) {
        const uint32_t id=uint32_t(view->integer(0xc4));
        if(id) {
            const auto found=cameras.find(id);
            if(found==cameras.end()) bad("viewport references a missing camera");
            selected=found->second.first;out.camera_from_viewport=true;
        }
    }
    // Course SCRT camera IDs are zero: the original binds a runtime race
    // camera. The first serialized track camera supplies a static initial
    // world view while that lifecycle is being ported; it is not a chase rig.
    if(!selected) selected=first_camera;
    // Battle arenas (*_b.wgm) hold no camera; the race view's follow camera
    // replaces this static view anyway (camera_id 0, identity).
    if(selected) {
        out.camera_id=selected->id;
        out.view_projection=multiply_matrix(camera_projection(*selected),inverse_affine_matrix(
            out.camera_from_race?setup.camera_transform:cameras.at(selected->id).second));
    } else out.view_projection=identity_matrix();
    const Node& background=view?*view:screen;
    for(size_t i=0;i<3;++i) {
        const int component=background.integer(0xc5+uint32_t(i));
        if(component<0 || component>255) bad("invalid viewport background component");
        out.clear[i]=component/255.f;
    }
    return out;
}
WorldData load_model_group(const ArchivePool& archives,const Node& root) {
    WorldData out;
    WorldBuilder builder{archives,out};
    builder.visit(root,identity_matrix(),true,true);
    return out;
}
void TranslucentQueue::add(uint32_t flags,float z,size_t item) {
    auto& list=lists_[(flags&0x20000)?2:(flags&4)?0:1];
    const uint32_t key=uint32_t(int32_t(std::isfinite(z) && std::abs(z)<2e9f?z:0));
    list.insert(std::partition_point(list.begin(),list.end(),[&](const Entry& e){return e.key>key;}),{key,item});
}
std::vector<TranslucentModel> translucent_models(const std::vector<WorldBatch>& batches) {
    std::vector<TranslucentModel> out;
    std::map<uint32_t,size_t> index;
    for(size_t i=0;i<batches.size();++i) {
        const WorldBatch& b=batches[i];
        if(!b.translucent) continue;
        auto found=index.find(b.node_id);
        if(found==index.end()) {
            found=index.emplace(b.node_id,out.size()).first;
            out.push_back({b.draw_flags,b.origin,{}});
        }
        out[found->second].batches.push_back(i);
    }
    return out;
}
void face_billboards(std::vector<WorldBatch>& batches,const Matrix4& camera) {
    for(auto& b:batches) {
        if(!b.billboard) continue;
        std::array<float,3> right,up;
        if(b.facing==2) {
            // 0x419d6d: the inverse camera rotation, so the camera's axes.
            right={{camera[0],camera[1],camera[2]}};up={{camera[4],camera[5],camera[6]}};
        } else {
            // 0x419e05: n = the horizontal direction from the camera to the
            // sprite; rows (n.z, 0, -n.x), (0, 1, 0), (n.x, 0, n.z) (0x4050d8).
            const float dx=b.anchor[0]-camera[12],dz=b.anchor[2]-camera[14],l=std::sqrt(dx*dx+dz*dz);
            if(!(l>0)) continue;
            right={{dz/l,0,-dx/l}};up={{0,1,0}};
        }
        for(size_t i=0;i<b.vertices.size();++i) {
            const auto& o=b.offsets[i];
            auto& v=b.vertices[i];
            v.x=b.anchor[0]+right[0]*o[0]+up[0]*o[1];
            v.y=b.anchor[1]+right[1]*o[0]+up[1]*o[1];
            v.z=b.anchor[2]+right[2]*o[0]+up[2]*o[1];
        }
    }
}
}
