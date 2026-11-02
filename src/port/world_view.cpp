#include "world_view.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace wpc {
WorldView::WorldView(WpcWindow* window,WorldData data):window_(window),data_(std::move(data)) {
    try {
        if(!window_ || data_.vertex_count>std::numeric_limits<uint32_t>::max()-4096 ||
           !wpc_vk_reserve_vertices(window_,uint32_t(data_.vertex_count+4096)))
            throw std::runtime_error("World vertex allocation failed");
        for(auto& [id,image]:data_.textures) {
            WpcTexture* texture=wpc_vk_texture_create(window_,image.width,image.height,image.rgba.data());
            if(!texture) throw std::runtime_error("World texture upload failed: "+std::to_string(id));
            textures_.emplace(id,texture);
        }
        if(std::any_of(data_.batches.begin(),data_.batches.end(),[](const WorldBatch& b){return !b.material.textured;})) {
            const uint8_t pixel[]={255,255,255,255};
            white_=wpc_vk_texture_create(window_,1,1,pixel);
            if(!white_) throw std::runtime_error("Flat material upload failed");
        }
        wpc_log("World prepared: %zu models, %zu sprites, %zu unsupported sprite types, %zu batches, %zu vertices, %zu textures; camera 0x%x (%s)",
            data_.models,data_.sprites,data_.unsupported_sprites,data_.batches.size(),data_.vertex_count,textures_.size(),data_.camera_id,
            data_.camera_from_race?"shared race follow camera":data_.camera_from_viewport?"serialized viewport link":"static track camera; race camera pending");
        data_.textures.clear();
    } catch(...) {release();throw;}
}
void WorldView::release() {
    for(const auto& [id,texture]:textures_) wpc_vk_texture_destroy(window_,texture);
    textures_.clear();
    if(white_) wpc_vk_texture_destroy(window_,white_);
    white_=nullptr;
}
WorldView::~WorldView() {release();}
void WorldView::draw() {
    int width,height;wpc_window_get_size(window_,&width,&height);
    if(width<=0 || height<=0) return;
    wpc_vk_clear(window_,0,0,0);
    const float scale=std::min(width/640.f,height/480.f);
    const int w=int(640*scale),h=int(480*scale),x=(width-w)/2,y=(height-h)/2;
    wpc_vk_set_clip_rect(window_,x,y,w,h);
    wpc_vk_draw_rect(window_,x,y,w,h,data_.clear[0],data_.clear[1],data_.clear[2],1);
    Matrix4 mvp=data_.view_projection;
    for(size_t c=0;c<4;++c) {mvp[c*4]*=float(w)/width;mvp[c*4+1]*=float(h)/height;}
    wpc_vk_set_mvp(window_,mvp.data());
    for(const auto& b:data_.batches) {
        WpcTexture* texture=b.material.textured?textures_.at(b.material.texture_id):white_;
        wpc_vk_draw_material_triangles(window_,texture,b.vertices.data(),uint32_t(b.vertices.size()),b.material.clamp_axes,b.material.two_sided?1:0);
    }
    wpc_vk_set_clip_rect(window_,0,0,width,height);
}
}
