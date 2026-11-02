#include "menu_models.h"
#include "game_material.h"
#include "menu_controller.h"
#include "game_world.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace wpc {
namespace {
using Matrix = std::array<float,16>;
[[noreturn]] void bad(const std::string& text) { throw std::runtime_error("menu models: "+text); }
Matrix identity() { return {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}}; }
Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix out{};
    for(size_t col=0;col<4;++col) for(size_t row=0;row<4;++row)
        for(size_t k=0;k<4;++k) out[col*4+row]+=a[k*4+row]*b[col*4+k];
    for(float value:out) if(!std::isfinite(value)) bad("transform composition overflow");
    return out;
}
uint32_t word(const uint8_t* p) {
    return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
float real(const uint8_t* p) {
    const uint32_t bits=word(p);float value;std::memcpy(&value,&bits,4);
    if(!std::isfinite(value)) bad("nonfinite serialized affine component");
    return value;
}
Matrix affine(const std::array<float,12>& raw) {
    Matrix out=identity();
    for(size_t col=0;col<3;++col) for(size_t row=0;row<3;++row) out[col*4+row]=raw[col*3+row];
    for(size_t row=0;row<3;++row) out[12+row]=raw[9+row];
    return out;
}
Matrix node_matrix(const Node& node) {
    const auto found=node.properties.find(0x1b);
    if(found==node.properties.end()) return identity();
    if(found->second.size()!=48) bad("node "+std::to_string(node.id)+" has invalid affine length");
    std::array<float,12> raw{};
    for(size_t i=0;i<12;++i) raw[i]=real(found->second.data()+i*4);
    return affine(raw);
}
// Read a GMAT material's TRAN (transparency) and TWOS (two-sided) flags directly
// from the archive, parallel to game_material::parse_material.
void material_flags(const Archive& archive,uint32_t id,float& transparency,bool& two_sided) {
    transparency=0.f; two_sided=false;
    const auto& resource=archive.find("GMAT",id);
    for(const auto& chunk:archive.children(resource)) {
        if(chunk.tag=="TRAN") {
            const auto bytes=archive.payload(chunk);
            if(bytes.size==4) transparency=real(bytes.data);
        } else if(chunk.tag=="TWOS") {
            const auto bytes=archive.payload(chunk);
            if(bytes.size==4) two_sided=(word(bytes.data)&1u)!=0;
        }
    }
}
Matrix inverse_affine(const Matrix& m) {
    // Invert the actual 3x3 basis, preserving any serialized scale/shear.
    const double a=m[0],b=m[4],c=m[8], d=m[1],e=m[5],f=m[9], g=m[2],h=m[6],i=m[10];
    const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(determinant)||std::abs(determinant)<1e-12) bad("singular serialized camera transform");
    Matrix out=identity();
    out[0]=float((e*i-f*h)/determinant);out[4]=float((c*h-b*i)/determinant);out[8]=float((b*f-c*e)/determinant);
    out[1]=float((f*g-d*i)/determinant);out[5]=float((a*i-c*g)/determinant);out[9]=float((c*d-a*f)/determinant);
    out[2]=float((d*h-e*g)/determinant);out[6]=float((b*g-a*h)/determinant);out[10]=float((a*e-b*d)/determinant);
    for(size_t row=0;row<3;++row) out[12+row]=-(out[row]*m[12]+out[4+row]*m[13]+out[8+row]*m[14]);
    for(float value:out) if(!std::isfinite(value)) bad("camera inverse overflow");
    return out;
}
const Node* render_viewport(const Node& node) {
    if(node.class_name=="GDViewport" && node.integer(0xc4)!=0) return &node;
    for(const auto& child:node.children) if(const Node* found=render_viewport(child)) return found;
    return nullptr;
}
} // namespace

MenuModels::MenuModels(WpcWindow* window,const Archive& archive,const Scene& scene,
                       std::function<WpcTexture*(uint32_t)> material)
    :window_(window),archive_(archive),scene_(scene),material_(std::move(material)) {
    if(!material_) bad("missing material callback");
    if(const char* clip=std::getenv("WPC_MENU_MOTION"); clip && *clip) debug_clip_=std::max(0,std::atoi(clip));
    index_transforms(scene_.root(),identity());
    index_transforms(scene_.screen(),identity());
    const Node* viewport=render_viewport(scene_.screen());
    // The in-race pause (general.wgm) has no menu viewport and no models.
    if(!viewport) {enabled_=false;return;}
    viewport_id_=viewport->id;camera_id_=uint32_t(viewport->integer(0xc4));
    const Node& camera=scene_.find(camera_id_);
    if(camera.class_name!="GDCamera" && camera.class_name!="WTrackCamera") bad("viewport references a non-camera node");
    const float fov=camera.real(0x2c),ratio=float(camera.integer(0x2d));
    const float near_plane=camera.real(0x2e),far_plane=camera.real(0x2f);
    if(!std::isfinite(fov)||!std::isfinite(ratio)||!std::isfinite(near_plane)||!std::isfinite(far_plane)||
       fov<=0 || fov>=3.14159265358979323846f || ratio<=0 || near_plane<=0 || far_plane<=near_plane)
        bad("invalid serialized camera projection tuple");
    const float tangent=std::tan(fov*0.5f),root_ratio=std::sqrt(ratio*0.0075f);
    Matrix projection{};
    projection[0]=root_ratio/tangent;
    projection[5]=-1.f/(root_ratio*tangent); // WPC positive Y becomes Vulkan's top-down viewport.
    projection[10]=far_plane/(far_plane-near_plane);
    projection[11]=1.f;
    projection[14]=-(far_plane*near_plane)/(far_plane-near_plane);
    view_=inverse_affine(transforms_.at(camera_id_));
    view_projection_=multiply(projection,view_);
    // SCRT directly proves viewport 0x25 -> camera 2 in the installed menu.
    // Selecting the first camera-bearing viewport reproduces that serialized
    // link. Its active runtime binding on each menu page remains unverified.
    wpc_log("Menu models: serialized viewport 0x%x -> camera 0x%x, FOV=%g ratio=%g near=%g far=%g",
            viewport_id_,camera_id_,fov,ratio,near_plane,far_plane);
}

void MenuModels::index_transforms(const Node& node,const Matrix& parent) {
    const Matrix world=multiply(parent,node_matrix(node));
    if(!transforms_.emplace(node.id,world).second) bad("duplicate node in transform index");
    for(const auto& child:node.children) index_transforms(child,world);
}

void MenuModels::collect(Page& page,const Node& node,int item,int spin) {
    for(const auto& action:node.actions) {
        if(action.class_name!="WRot") continue;
        // 0x497b00 registers m_MaxRotDeg{X,Y,Z}=0x1cf..0x1d1,
        // m_StepRotDeg{X,Y,Z}=0x1d2..0x1d4 and m_lType=0x1d5. Every menu WRot
        // is type 2 (unbounded, wrapped). Type 1 ping-pongs between the
        // maxima and is not used by menu.WGM; leave such nodes static.
        if(action.integer(0x1d5)<=1) continue;
        Spin rotation{};
        rotation.world=transforms_.at(node.id);
        rotation.inverse=inverse_affine(rotation.world);
        for(size_t axis=0;axis<3;++axis) {
            rotation.step[axis]=action.real(0x1d2+uint32_t(axis));
            if(!std::isfinite(rotation.step[axis])) bad("nonfinite WRot step");
        }
        spin=int(page.spins.size());
        page.spins.push_back(rotation);
    }
    if(node.class_name=="GDModel" || node.class_name=="GDHModel") {
        const auto link=node.properties.find(0x67);
        if(link==node.properties.end() || link->second.size()!=8) bad("model has invalid resource link");
        const auto& bytes=link->second;
        const bool empty=std::all_of(bytes.begin(),bytes.end(),[](uint8_t byte){return byte==0;});
        if(!empty) {
            const std::string tag(reinterpret_cast<const char*>(bytes.data()),4);
            if(tag!="GMDL" && tag!="GHMS") bad("unsupported model resource link type");
            const uint32_t id=word(bytes.data()+4);
            const auto key=std::make_pair(tag,id);
            auto found=meshes_.find(key);
            if(found==meshes_.end()) found=meshes_.emplace(key,load_mesh(archive_,tag,id)).first;
            const Mesh& mesh=found->second;
            const Matrix world=multiply(transforms_.at(node.id),affine(mesh.matrix));
            std::map<uint32_t,size_t> batches;
            // Draw flags +0x12a: GDModel::m_RenderSetting, property 0x68 in
            // the WGM's member table (registered at 0x43ee70 with offset
            // 0x8c from the property base +0x9e; constructor default 0x80062).
            const uint32_t flags=uint32_t(node.integer(0x68));
                        const int model=page.models++;
            const Matrix& origin=transforms_.at(node.id);
            Animated* animated=nullptr;
            const auto motion=node.properties.find(0x65);
            if(debug_clip_>=0 && tag=="GHMS" && motion!=node.properties.end() && motion->second.size()==8 &&
               std::equal(motion->second.begin(),motion->second.begin()+4,"MTNL")) {
                const uint32_t motion_id=word(motion->second.data()+4);
                auto lib=motions_.find(motion_id);
                if(lib==motions_.end()) lib=motions_.emplace(motion_id,parse_motion(archive_,motion_id)).first;
                if(size_t(debug_clip_)<lib->second.clips.size() && lib->second.segments==mesh.segments.size()) {
                    page.animated.push_back({item,mesh,world,MotionController(lib->second),size_t(debug_clip_),{}});
                    animated=&page.animated.back();
                    wpc_log("Menu models: GDHModel 0x%x plays MTNL 0x%x clip %d (WPC_MENU_MOTION)",node.id,motion_id,debug_clip_);
                }
            }
            for(const auto& triangle:mesh.triangles) {
                auto batch=batches.find(triangle.material);
                if(batch==batches.end()) {
                    auto texture=textures_.find(triangle.material);
                    if(texture==textures_.end()) {
                        WpcTexture* gpu=material_(triangle.material);
                        if(!gpu) bad("material "+std::to_string(triangle.material)+" supplied no GPU texture");
                        texture=textures_.emplace(triangle.material,gpu).first;
                    }
                    const size_t index=page.batches.size();
                    uint32_t clamp_axes=0;
                    for (const auto &field:archive_.children(archive_.find("GMAT",triangle.material))) if (field.tag=="CLAM") {
                        auto bytes=archive_.payload(field);
                        if (bytes.size!=4) bad("invalid material CLAM length");
                        clamp_axes=word(bytes.data)&3;
                    }
                    float transparency=0.f;bool two_sided=false;
                    material_flags(archive_,triangle.material,transparency,two_sided);
                    bool textured=false;
                    for(const auto& field:archive_.children(archive_.find("GMAT",triangle.material))) if(field.tag=="TEXP") {
                        const auto bytes=archive_.payload(field);
                        // As parse_material: one link whose tag is nonzero.
                        textured=bytes.size==12 && word(bytes.data)==1 && word(bytes.data+4)!=0;
                    }
                    lit_.emplace(triangle.material,textured);
                    auto alpha_format=translucent_.find(triangle.material);
                    if(alpha_format==translucent_.end()) {
                        const bool wide=textured && archive_.texture(archive_.material_texture(triangle.material)).depth>=24;
                        alpha_format=translucent_.emplace(triangle.material,wide).first;
                    }
                    Batch added{item,texture->second,clamp_axes,transparency,two_sided,
                                transparency!=0.f || alpha_format->second,spin,model,flags,{}};
                    added.origin={origin[12],origin[13],origin[14]};
                    page.batches.push_back(std::move(added));
                    batch=batches.emplace(triangle.material,index).first;
                }
                auto& vertices=page.batches[batch->second].vertices;
                const float batch_alpha=1.f-page.batches[batch->second].transparency;
                for(uint32_t index:{triangle.a,triangle.b,triangle.c}) {
                    const MeshVertex& vertex=mesh.vertices.at(index);
                    const float x=world[0]*vertex.x+world[4]*vertex.y+world[8]*vertex.z+world[12];
                    const float y=world[1]*vertex.x+world[5]*vertex.y+world[9]*vertex.z+world[13];
                    const float z=world[2]*vertex.x+world[6]*vertex.y+world[10]*vertex.z+world[14];
                    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) bad("model vertex transform overflow");
                    // Untextured vertex alpha stays 1-TRAN (Glide 0x433d40; Direct3D untraced).
                    // The native decoder already normalizes bottom-origin TGA rows
                    // to top-down, so source V is used directly (equivalent to WPC's
                    // bottom-up storage plus T complement).
                    if(animated) animated->corners.push_back({uint32_t(batch->second),uint32_t(vertices.size()),index});
                    // Textured: the Direct3D cache colour (0x4b21ae).
                    if(lit_.at(triangle.material)) {
                        const float vcol[3]={vertex.r,vertex.g,vertex.b};
                        const auto c=textured_vertex_color(page.batches[batch->second].transparency,mesh.vertex_colors?vcol:nullptr);
                        vertices.push_back({x,y,z,vertex.u,vertex.v,c[0],c[1],c[2],c[3]});
                    } else vertices.push_back({x,y,z,vertex.u,vertex.v,1,1,1,batch_alpha});
                }
            }
        }
    }
    for(const auto& child:node.children) collect(page,child,item,spin);
}

void MenuModels::prepare(const Node& page) {
    if(!enabled_) return;
    if(pages_.count(page.id)) return;
    if(page.class_name!="WMenuPage") bad("prepare requires a WMenuPage");
    Page prepared;
    int item=0;
    for(const auto& child:page.children) {
        if(menu_item_class(child.class_name)) collect(prepared,child,item++,-1);
        else collect(prepared,child,-1,-1);
    }
    size_t vertices=0;
    for(const auto& batch:prepared.batches) vertices+=batch.vertices.size();
    wpc_log("Menu models page 0x%x: %zu batches, %zu expanded vertices (all selections)",
            page.id,prepared.batches.size(),vertices);
    pages_.emplace(page.id,std::move(prepared));
}

void MenuModels::animate(Page& page,double frames,size_t selected) {
    if(!enabled_) return;
    // One controller step of 10 per GDHModel draw (0x437d00), one draw per
    // 30 Hz menu frame; a finished clip starts again.
    const int steps=int(std::min(30.0,std::max(0.0,frames-page.frames)));
    page.frames=frames;
    for(auto& a:page.animated) {
        if(a.item>=0 && size_t(a.item)!=selected) continue;
        if(!steps) continue;
        MotionPairs pairs(a.mesh.segments.size()*9,{0.f,0.f});
        for(int i=0;i<steps;++i) {
            if(!a.controller.active()) a.controller.play(a.clip);
            std::fill(pairs.begin(),pairs.end(),std::array<float,2>{0.f,0.f});
            a.controller.update(10,&pairs);
        }
        pose_mesh(a.mesh,pairs);
        const Matrix& w=a.world;
        for(const Corner& c:a.corners) {
            const MeshVertex& v=a.mesh.vertices[c.vertex];
            auto& out=page.batches[c.batch].vertices[c.slot];
            out.x=w[0]*v.x+w[4]*v.y+w[8]*v.z+w[12];
            out.y=w[1]*v.x+w[5]*v.y+w[9]*v.z+w[13];
            out.z=w[2]*v.x+w[6]*v.y+w[10]*v.z+w[14];
        }
    }
}

MenuModels::Silhouette MenuModels::silhouette(uint32_t page_id,int item) const {
    const auto page=pages_.find(page_id);
    if(page==pages_.end()) bad("page was not prepared before the silhouette query");
    Silhouette s;s.x0=s.y0=1e9f;s.x1=s.y1=-1e9f;
    const Matrix& m=view_projection_;
    for(const auto& batch:page->second.batches) {
        if(batch.item!=item) continue;
        ++s.batches;s.translucent+=batch.translucent;
        for(const auto& v:batch.vertices) {
            const float w=m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15];
            if(!(w>1e-6f)) continue;
            const float x=(m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12])/w,y=(m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13])/w;
            const float px=(x+1)*320,py=(y+1)*240;
            s.x0=std::min(s.x0,px);s.x1=std::max(s.x1,px);s.y0=std::min(s.y0,py);s.y1=std::max(s.y1,py);
            s.color[0]+=v.r;s.color[1]+=v.g;s.color[2]+=v.b;s.color[3]+=v.a;++s.vertices;
        }
    }
    if(s.vertices) for(float& c:s.color) c/=float(s.vertices);
    return s;
}
void MenuModels::draw(uint32_t page_id,size_t selected,int width,int height,double elapsed_seconds,float y_scale,float y_offset) {
    if(!enabled_ || !window_) return;
    const auto page=pages_.find(page_id);
    if(page==pages_.end()) bad("page was not prepared before frame drawing");
    if(width<=0 || height<=0) return;
    const float scale=std::min(width/640.f,height/480.f);
    const float sx=640.f*scale/width,sy=480.f*scale/height;
    Matrix mvp=view_projection_;
    // Centered 640x480 letterboxing is a scale of clip X/Y, with zero NDC
    // translation. Projection depth and W retain the original camera values.
    for(size_t col=0;col<4;++col) {mvp[col*4]*=sx;mvp[col*4+1]*=sy;}
    // A viewport within the frame: clip y' = y_scale y + y_offset w.
    if(y_scale!=1 || y_offset!=0) for(size_t col=0;col<4;++col) mvp[col*4+1]=mvp[col*4+1]*y_scale+mvp[col*4+3]*y_offset*sy;
    // WRot advances once per frame; 0x4ad540 holds frames to at least 33 ms.
    // Measured Wine period for the 5-degree car step: 2.667 s (27 Hz frames).
    const double frames=std::floor(std::max(0.0,elapsed_seconds)*30.0);
    if(!page->second.animated.empty()) animate(page->second,frames,selected);
    std::vector<Matrix> spins,spun;
    for(const auto& spin:page->second.spins) {
        // 0x498443: identity, rotate_basis_y, _z, _x (0x43b4a0 maps local X
        // to (cos,0,sin)), then restore scale and position. For the menu's
        // identity-rotation groups this equals world * R * world^-1 applied
        // to the pre-baked world-space vertices.
        Matrix rotation=identity();
        const auto basis=[&](size_t a,size_t b,float step) {
            const double angle=std::remainder(frames*double(step),6.28318530717958647692);
            if(angle==0) return;
            const float c=float(std::cos(angle)),s=float(std::sin(angle));
            for(size_t row=0;row<3;++row) {
                const float u=rotation[a*4+row],v=rotation[b*4+row];
                rotation[a*4+row]=c*u+s*v;
                rotation[b*4+row]=c*v-s*u;
            }
        };
        basis(0,2,spin.step[1]);
        basis(0,1,spin.step[2]);
        basis(1,2,spin.step[0]);
        spun.push_back(multiply(spin.world,multiply(rotation,spin.inverse)));
        spins.push_back(multiply(mvp,spun.back()));
    }
    const auto submit=[&](const Batch& batch) {
        if(batch.vertices.size()>UINT32_MAX) bad("expanded vertex batch exceeds draw limit");
        wpc_vk_set_mvp(window_,batch.spin>=0 ? spins[size_t(batch.spin)].data() : mvp.data());
        wpc_vk_draw_material_triangles(window_,batch.texture,batch.vertices.data(),uint32_t(batch.vertices.size()),batch.clamp_axes,batch.two_sided?1:0);
    };
    const auto shown=[&](const Batch& batch){return batch.item<0 || size_t(batch.item)==selected;};
    // Opaque parts draw as their models are traversed; translucent parts are
    // queued (0x4afcc2) and drawn by the end-of-frame flush 0x4b2d70, with
    // alpha blending on, list +0x3f8 (flags & 4), then +0x3f4, then +0x3fc
    // (flags & 0x20000).
    for(const auto& batch:page->second.batches) if(!batch.translucent && shown(batch)) submit(batch);
    // One entry per model (TranslucentQueue, 0x4afcc2), keyed by the view
    // z of the model's origin through the view matrix 0x26fa530.
    TranslucentQueue queue;
    std::vector<int> queued;
    for(const auto& batch:page->second.batches) {
        if(!batch.translucent || !shown(batch)) continue;
        if(std::find(queued.begin(),queued.end(),batch.model)!=queued.end()) continue;
        std::array<float,3> o=batch.origin;
        if(batch.spin>=0) {
            // A spinning model's origin turns with it about the WRot node.
            const Matrix& r=spun[size_t(batch.spin)];
            o={r[0]*o[0]+r[4]*o[1]+r[8]*o[2]+r[12],r[1]*o[0]+r[5]*o[1]+r[9]*o[2]+r[13],
               r[2]*o[0]+r[6]*o[1]+r[10]*o[2]+r[14]};
        }
        queue.add(batch.flags,view_[2]*o[0]+view_[6]*o[1]+view_[10]*o[2]+view_[14],queued.size());
        queued.push_back(batch.model);
    }
    queue.flush([&](size_t i) {
        for(const auto& batch:page->second.batches)
            if(batch.translucent && batch.model==queued[i] && shown(batch)) {
                wpc_vk_set_draw_mode(window_,draw_mode(batch.flags));
                submit(batch);
            }
    });
    wpc_vk_set_draw_mode(window_,0);
}
} // namespace wpc
