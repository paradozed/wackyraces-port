#include "race_view.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("race view: "+text);}
uint32_t word(ByteView b,size_t at) {
    if(at>b.size || b.size-at<4) bad("truncated film resource");
    const uint8_t* p=b.data+at;
    return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
int16_t half(ByteView b,size_t at) {
    if(at>b.size || b.size-at<2) bad("truncated film placement");
    return int16_t(uint16_t(b.data[at])|uint16_t(b.data[at+1])<<8);
}
const Node* find_class(const Node& n,const std::string& name) {
    if(n.class_name==name) return &n;
    for(const auto& c:n.children) if(const Node* found=find_class(c,name)) return found;
    return nullptr;
}
// WHudBrick 0x485b00 one-player layout row 0 at 0x52bd5c (320x240 units).
constexpr int layout_dial[2]={263,207},layout_needle[2]={280,217},layout_message[2]={160,120};
constexpr int layout_lap[2]={13,10},layout_rank[2]={13,30},layout_suffix[2]={24,30},layout_timer[2]={13,50};
// HUD font film (GDPsText 0x1a9 etc.) and its ordinal-suffix frames.
constexpr uint32_t hud_film=66333;
}
RaceView::RaceView(WpcWindow* window,const ArchivePool& archives,const Archive& general,
                   const Scene& course,const Scene& shared,const Strings& strings,const RaceSession& session)
    :window_(window),general_(general),strings_(strings),session_(session) {
    try {
        world_=load_world(archives,course.root(),course.screen());
        upload(world_.textures);
        // GDViewport m_BackDrop (0xc8): cc_t1's sky dome GDModel 0x19.
        if(const Node* viewport=find_class(course.screen(),"GDViewport")) {
            const uint32_t backdrop=uint32_t(viewport->integer(0xc8));
            auto split=std::stable_partition(world_.batches.begin(),world_.batches.end(),
                [&](const WorldBatch& b){return b.node_id!=backdrop;});
            backdrop_.assign(std::make_move_iterator(split),std::make_move_iterator(world_.batches.end()));
            world_.batches.erase(split,world_.batches.end());
            // Dome radius: 2505 on cc_t1, about 5000 on sh_t1/ct_t1, 6420 on dt_t1.
            for(const auto& b:backdrop_) for(const auto& v:b.vertices)
                backdrop_radius_=std::max({backdrop_radius_,std::abs(v.x),std::abs(v.z)});
        }
        // The moving course objects leave the static world with their
        // subtrees and are drawn from their root-local models.
        {
            const auto take=[&](uint32_t id,std::vector<WorldBatch>& out) {
                std::vector<uint32_t> ids;
                const auto walk=[&](const auto& self,const Node& n)->void {ids.push_back(n.id);for(const auto& c:n.children) self(self,c);};
                walk(walk,course.find(id));
                world_.batches.erase(std::remove_if(world_.batches.begin(),world_.batches.end(),[&](const WorldBatch& b) {
                    return std::find(ids.begin(),ids.end(),b.node_id)!=ids.end();
                }),world_.batches.end());
                WorldData d=load_model_group(archives,course.find(id));upload(d.textures);out=std::move(d.batches);
            };
            const CourseObjects& objects=session_.objects();
            crossing_trams_.resize(objects.crossings().size());movers_.resize(objects.movers().size());
            for(size_t i=0;i<objects.crossings().size();++i) take(objects.crossings()[i].tram,crossing_trams_[i]);
            for(size_t i=0;i<objects.movers().size();++i) take(objects.movers()[i].node,movers_[i]);
            logs_.resize(objects.log_rides().size());
            for(size_t i=0;i<objects.log_rides().size();++i) {
                const LogRide& r=objects.log_rides()[i];logs_[i].resize(r.logs.size());
                for(size_t k=0;k<r.logs.size();++k) take(r.logs[k],logs_[i][k]);
            }
            // The lights model's batches are found at the first draw, once
            // the power-ups have left world_.batches.
            for(size_t i=0;i<objects.crossings().size();++i) {
                const Node& brick=course.find(objects.crossings()[i].node);
                if(brick.children.size()>=2) {Lights l;l.crossing=i;l.node=brick.children[1].id;lights_.push_back(std::move(l));}
            }
        }
        // WPowerUp models leave the static world: their state hides, grows
        // and turns them (0x4d3ec0).
        const auto& spots=session_.track().power_ups();
        pickups_.resize(spots.size());
        for(size_t i=0;i<spots.size();++i) {
            auto split=std::stable_partition(world_.batches.begin(),world_.batches.end(),
                [&](const WorldBatch& b){return b.node_id!=spots[i].model;});
            pickups_[i].assign(std::make_move_iterator(split),std::make_move_iterator(world_.batches.end()));
            world_.batches.erase(split,world_.batches.end());
        }
        // GDBrick::m_nFlags (property 0x1a in the member table) bit 0x100
        // marks cc_t1's flat grey occluder slabs; the same-state Wine frame
        // shows the mesas behind them, so they are not drawn. 0x100 is the
        // flag WHudBrick uses to hide children; the world draw test itself
        // is not traced.
        world_.batches.erase(std::remove_if(world_.batches.begin(),world_.batches.end(),[&](const WorldBatch& b) {
            const float setting=course.find(b.node_id).real(0x1a);
            uint32_t bits;std::memcpy(&bits,&setting,4);
            return (bits&0x100)!=0;
        }),world_.batches.end());
        // WCarFxStorage children, in order (0x47bec0, 0x472d00, 0x47c680):
        // GD3DFilm 410, the dust GD3DFilm 411, WSplash 412, WKBumm 417,
        // WalkOnWater 419. WKBumm needs the 0x40000 query hits (not ported).
        if(const Node* storage=find_class(shared.root(),"WCarFxStorage")) {
            const auto& kids=storage->children;
            if(kids.size()<5 || kids[1].class_name!="GD3DFilm" || kids[2].class_name!="WSplash" || kids[4].class_name!="WalkOnWater")
                bad("unexpected WCarFxStorage layout");
            const uint32_t dust=kids[1].link(0x35,"FILM");
            const ByteView data=general_.payload(general_.find("FILM",dust));
            for(uint32_t r=0;r<word(data,4);++r) puff_reels_.push_back(film(dust,r));
            smoke_frames_=film(kids[0].link(0x35,"FILM"));
            // WExplosion's GD3dSprite 0x3ac (m_MaterialID 0x09), centred.
            {
                const Sprite s=sprite(shared.find(0x3ac).link(0x09,"GMAT"));
                blast_.texture=s.texture;blast_.width=s.width;blast_.height=s.height;
                blast_.x=blast_.y=-s.width/2;
            }
            if(kids[3].class_name=="WKBumm") bumm_frames_=film(kids[3].link(0x35,"FILM"));
            std::vector<const Node*> models;
            for(const auto& c:kids[2].children) if(c.class_name=="GDModel") models.push_back(&c);
            if(models.size()!=2) bad("WSplash needs two models");
            const auto group=[&](const Node& n,std::vector<WorldBatch>& out) {
                WorldData d=load_model_group(archives,n);upload(d.textures);out=std::move(d.batches);
            };
            group(*models[0],splash_column_);group(*models[1],splash_ring_);group(kids[4],ripple_);
        }
        // The weapon templates follow WFirstPowerUp in the world (0x483a20).
        // The WBreath projectiles' models: the GDModel under each DragonBreath
        // (0x39a, the breath, is a GD3DFilm and is not drawn).
        for(const uint32_t id:{0x38cu,0x3a8u,0x3a1u}) {
            const Node& breath=shared.find(id);
            for(const auto& c:breath.children) if(c.class_name=="GDModel") {
                WorldData d=load_model_group(archives,c);upload(d.textures);projectiles_[id]=std::move(d.batches);
                break;
            }
        }
        // The paint shot (0x396) is itself the GDModel carrying WPaintShot.
        {
            WorldData d=load_model_group(archives,shared.find(0x396));upload(d.textures);projectiles_[0x396]=std::move(d.batches);
        }
        // WPU_Storm's template is the GD3dSprite itself (general.wgm #889).
        for(const auto& c:shared.root().children) for(const auto& ac:c.actions)
            if(ac.class_name=="WPU_Storm" && c.class_name=="GD3dSprite" && storm_.empty()) {
                WorldData d=load_model_group(archives,c);upload(d.textures);storm_=std::move(d.batches);
            }
        for(const auto& c:shared.root().children) for(const auto& ac:c.actions) {
            std::vector<WorldBatch>* out=ac.class_name=="WPU_Landmine"?&landmine_:ac.class_name=="WPU_Tnt"?&tnt_:
                                         ac.class_name=="WPU_Detour"?&detour_:ac.class_name=="WPU_OilSpill"?&oil_:
                                         ac.class_name=="WPU_BrickWall"?&wall_:ac.class_name=="WPU_FallingTree"?&tree_:
                                         ac.class_name=="WPU_BeeHive"?&hive_:nullptr;
            if(!out || !out->empty()) continue;
            WorldData d=load_model_group(archives,c);upload(d.textures);*out=std::move(d.batches);
        }
        const RaceTemplates templates=load_race_templates(shared.root());
        std::vector<int> vehicles{session_.player().vehicle};
        for(const auto& r:session_.grid()) vehicles.push_back(r.racer.vehicle);
        size_t car_vertices=0;
        for(int v:vehicles) {
            if(cars_.count(v)) continue;
            const auto& t=templates.vehicles.at(size_t(v));
            cars_[v]=load_model_group(archives,shared.find(t.model_ids[0]));
            shadows_[v]=load_model_group(archives,shared.find(t.shadow_id));
            upload(cars_[v].textures);upload(shadows_[v].textures);
        }
        for(const auto& r:session_.grid()) car_vertices+=cars_.at(r.racer.vehicle).vertex_count+shadows_.at(r.racer.vehicle).vertex_count;
        // Shared WFollowCamera_2 parent camera: FOV 55 degrees, near 50, far 20000.
        const Node& follow=shared.find(templates.camera_ids[0]);
        projection_=camera_projection(follow);
        // 0x4ae060: linear fog from half the camera far plane to the far plane,
        // colored by the viewport background (Glide 0x433930 path likewise).
        fog_end_=follow.real(0x2f);fog_start_=fog_end_*0.5f;
        const uint8_t pixel[]={255,255,255,255};
        white_=wpc_vk_texture_create(window_,1,1,pixel);
        if(!white_) bad("flat material upload failed");
        owned_.push_back(white_);
        if(!flake_) {
            // Provisional: the flake texture (renderer +0x23969 -> +0x18) is
            // not identified; a soft white disc stands in.
            std::vector<uint8_t> px(16*16*4);
            for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
                const float d=std::sqrt(float((x-7.5f)*(x-7.5f)+(y-7.5f)*(y-7.5f)))/8;
                uint8_t* p=&px[size_t(y*16+x)*4];p[0]=p[1]=p[2]=255;p[3]=uint8_t(255*std::clamp(1-d,0.f,1.f));
            }
            flake_=wpc_vk_texture_create(window_,16,16,px.data());
            if(flake_) owned_.push_back(flake_);
        }

        // Two players draw the scene twice; the weather adds up to 15000
        // rain or 9000 snow vertices a view.
        const size_t views=session.two_players()?2:1;
        const size_t total=(world_.vertex_count*3+car_vertices+16384+15000)*views;
        if(total>std::numeric_limits<uint32_t>::max() || !wpc_vk_reserve_vertices(window_,uint32_t(total)))
            bad("race vertex allocation failed");
        const Node* hud=find_class(shared.root(),"WHudBrick");
        if(!hud || hud->children.size()<2) bad("shared scene lacks the WHudBrick dial and needle");
        dial_=sprite(hud->children[0].link(0x9e,"GMAT"));
        needle_=sprite(hud->children[1].link(0x9e,"GMAT"));
        // GDSprite 0xa1/0xa2: the dial's top-left is 2*(x,y)-(0xa1,0xa2) in
        // the Wine frame. The needle turns about its knob (fitted pixel 10,52).
        dial_xy_={{hud->children[0].integer(0xa1),hud->children[0].integer(0xa2)}};
        font_=film(hud_film);
        if(hud->children.size()<8) bad("shared scene lacks the WHudBrick head film");
        faces_=film(hud->children[7].link(0x35,"FILM"));
        if(font_.size()<=88) bad("HUD font lacks ordinal suffix frames");
        // Bend warning (WHudBrick child 15) and progress-strip heads.
        bend_=film(66331);
        const Node* panel=find_class(shared.root(),"WFirstPowerUp");
        if(!panel || panel->children.size()<6) bad("shared scene lacks the WFirstPowerUp panel");
        const auto& kids=panel->children;
        big_dot_=sprite(kids[0].link(0x9e,"GMAT"));small_dot_=sprite(kids[1].link(0x9e,"GMAT"));
        gauge_needle_=sprite(kids[2].link(0x9e,"GMAT"));gauge_=sprite(kids[3].link(0x9e,"GMAT"));
        if(kids[4].class_name!="WModMngrFilm") bad("WFirstPowerUp lacks its WModMngrFilm");
        heads_=film(kids[4].link(0x35,"FILM"));
        for(size_t i=5;i<kids.size();++i) if(kids[i].class_name=="WPowerUpSprite") {
            if(icons_.empty()) icon_origin_={{kids[i].integer(0xa1),kids[i].integer(0xa2)}};
            icons_.push_back(sprite(kids[i].link(0x9e,"GMAT")));
        }
        if(icons_.size()<0x1a || heads_.size()<9 || bend_.size()<3) bad("power-up panel resources are incomplete");
        size_t translucent=0;
        const auto group=[&](const std::vector<WorldBatch>& b) {translucent+=(translucent_[&b]=translucent_models(b)).size();};
        group(world_.batches);
        for(const auto& p:pickups_) group(p);
        for(const auto& t:crossing_trams_) group(t);
        for(const auto& m:movers_) group(m);
        for(const auto& r:logs_) for(const auto& l:r) group(l);
        for(const auto& [v,c]:cars_) {group(c.batches);group(shadows_.at(v).batches);}
        group(splash_column_);group(splash_ring_);group(ripple_);group(landmine_);group(tnt_);group(detour_);group(oil_);group(wall_);group(tree_);group(hive_);
        for(const auto& [id,b]:projectiles_) group(b);
        wpc_log("Race view: %zu world batches, %zu world vertices, %zu car models, %zu textures, %zu translucent models",
                world_.batches.size(),world_.vertex_count,cars_.size(),textures_.size(),translucent);
        world_.textures.clear();
    } catch(...) {release();throw;}
}
void RaceView::upload(std::map<uint32_t,Image>& images) {
    for(auto& [id,image]:images) {
        if(textures_.count(id)) continue;
        WpcTexture* t=wpc_vk_texture_create(window_,image.width,image.height,image.rgba.data());
        if(!t) bad("texture upload failed: "+std::to_string(id));
        textures_.emplace(id,t);owned_.push_back(t);
    }
    images.clear();
}
std::vector<RaceView::Glyph> RaceView::film(uint32_t id,uint32_t index) {
    const ByteView data=general_.payload(general_.find("FILM",id));
    if(word(data,4)<=index || data.size<16+size_t(index)*8 || std::memcmp(data.data+8+index*8,"REEL",4))
        bad("film has no reel "+std::to_string(index)+": "+std::to_string(id));
    const ByteView reel=general_.payload(general_.find("REEL",word(data,12+index*8)));
    const uint32_t count=word(reel,4);
    if(count>256 || reel.size!=8+size_t(count)*12) bad("invalid film reel: "+std::to_string(id));
    std::vector<Glyph> frames;
    for(uint32_t i=0;i<count;++i) {
        const size_t at=8+size_t(i)*12;
        if(std::memcmp(reel.data+at+4,"GMAT",4)) bad("film frame is not a material");
        Glyph g;g.x=half(reel,at);g.y=half(reel,at+2);
        const uint32_t material=word(reel,at+8);
        const Sprite s=sprite(material);
        for(const auto& field:general_.children(general_.find("GMAT",material)))
            if(field.tag=="SMOO") {const ByteView b=general_.payload(field);g.smooth=b.size>=4 && word(b,0)!=0;}
        g.texture=s.texture;g.width=s.width;g.height=s.height;
        frames.push_back(g);
    }
    return frames;
}
RaceView::Sprite RaceView::sprite(uint32_t material) {
    const uint32_t id=general_.material_texture(material);
    const Image image=general_.texture(id);
    Sprite s;s.width=int(image.width);s.height=int(image.height);
    if(image.rgba.empty()) return s;
    auto found=textures_.find(id);
    if(found==textures_.end()) {
        WpcTexture* t=wpc_vk_texture_create(window_,image.width,image.height,image.rgba.data());
        if(!t) bad("HUD texture upload failed");
        found=textures_.emplace(id,t).first;owned_.push_back(t);
    }
    s.texture=found->second;
    return s;
}
void RaceView::release() {
    for(WpcTexture* t:owned_) wpc_vk_texture_destroy(window_,t);
    owned_.clear();textures_.clear();white_=nullptr;
}
RaceView::~RaceView() {release();}
WpcTexture* RaceView::texture(const Material& m) const {return m.textured?textures_.at(m.texture_id):white_;}
void RaceView::draw_batches(const std::vector<WorldBatch>& batches,const Matrix4& mvp) {
    wpc_vk_set_mvp(window_,mvp.data());
    for(const auto& b:batches)
        wpc_vk_draw_material_triangles(window_,texture(b.material),b.vertices.data(),uint32_t(b.vertices.size()),
                                       b.material.clamp_axes,b.material.two_sided?1:0);
}
void RaceView::submit(const std::vector<WorldBatch>& batches,const Matrix4& vp,const Matrix4& view,const Matrix4& model) {
    const Matrix4 mvp=multiply_matrix(vp,model);
    wpc_vk_set_mvp(window_,mvp.data());
    for(const auto& b:batches) if(!b.translucent && !b.hidden)
        wpc_vk_draw_material_triangles(window_,texture(b.material),b.vertices.data(),uint32_t(b.vertices.size()),
                                       b.material.clamp_axes,b.material.two_sided?1:0);
    const Matrix4 mv=multiply_matrix(view,model);
    for(const auto& m:translucent_.at(&batches)) {
        const auto& o=m.origin;
        queue_.add(m.draw_flags,mv[2]*o[0]+mv[6]*o[1]+mv[10]*o[2]+mv[14],queued_.size());
        queued_.push_back({&batches,&m,mvp});
    }
}
void RaceView::model_colours(int vehicle,std::array<float,3>& lo,std::array<float,3>& hi) const {
    lo={{1,1,1}};hi={{0,0,0}};bool any=false;
    for(const auto& b:cars_.at(vehicle).batches) for(const auto& w:b.vertices) {
        const float c[3]={w.r,w.g,w.b};
        for(size_t i=0;i<3;++i) {lo[i]=std::min(lo[i],c[i]);hi[i]=std::max(hi[i],c[i]);}
        any=true;
    }
    if(!any) {lo={{1,1,1}};hi=lo;}
}
void RaceView::submit_car(int vehicle,const BombHit& hit,const Matrix4& vp,const Matrix4& view,const Matrix4& frame) {
    Matrix4 model=initial_vehicle_model_frame(frame);
    if(hit.active() && hit.scale_y!=1) for(size_t i=4;i<8;++i) model[i]*=hit.scale_y; // the local up axis
    const auto& batches=cars_.at(vehicle).batches;
    const float f=hit.active()?hit.tint.f:0;
    if(f<=0) {submit(batches,vp,view,model);return;}
    // 0x47d890 rewrites the model's vertex colours; translucent batches keep
    // theirs (provisional).
    const Matrix4 mvp=multiply_matrix(vp,model);
    wpc_vk_set_mvp(window_,mvp.data());
    for(const auto& b:batches) if(!b.translucent && !b.hidden) {
        tinted_=b.vertices;
        for(auto& w:tinted_) {
            w.r+=(hit.tint.target[0]-w.r)*f;w.g+=(hit.tint.target[1]-w.g)*f;w.b+=(hit.tint.target[2]-w.b)*f;
        }
        wpc_vk_draw_material_triangles(window_,texture(b.material),tinted_.data(),uint32_t(tinted_.size()),
                                       b.material.clamp_axes,b.material.two_sided?1:0);
    }
    const Matrix4 mv=multiply_matrix(view,model);
    for(const auto& m:translucent_.at(&batches)) {
        const auto& o=m.origin;
        queue_.add(m.draw_flags,mv[2]*o[0]+mv[6]*o[1]+mv[10]*o[2]+mv[14],queued_.size());
        queued_.push_back({&batches,&m,mvp});
    }
}
bool RaceView::shadow_frame(size_t slot,const RaceCar& car,Matrix4& out) {
    // From 403.7 above the car to 1000 below; f is the hit's fraction of that
    // ray. The shadow lies 5 units up the ground normal, right = n x forward,
    // forward = right x n, scaled by the vehicle's sizes (0x47c070) x (1 - f).
    static const float sizes[7][2]={{1.5f,0.7f},{1,1},{3.5f,1.5f},{3.5f,1.5f},{2,2},{2,1},{5,2}};
    const float top=car.position[1]+403.7f,bottom=car.position[1]-1000;
    const TrackSurface g=session_.track().ground(car.position[0],car.position[2],top,bottom);
    if(g.hit && g.y<car.position[1]+50 && car.vehicle>=0 && car.vehicle<7) {
        const float f=(top-g.y)/(top-bottom);
        const Matrix4 frame=car.frame();
        const Vec3& n=g.normal;
        const Vec3 fwd{{frame[8],frame[9],frame[10]}};
        const auto cross=[](const Vec3& a,const Vec3& b){return Vec3{{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};};
        const auto unit=[](Vec3 v){const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(l>0) for(float& c:v) c/=l;return v;};
        const Vec3 right=unit(cross(n,fwd)),ahead=unit(cross(right,n));
        const float sx=sizes[car.vehicle][0]*(1-f),sz=sizes[car.vehicle][1]*(1-f);
        Matrix4 m=identity_matrix();
        for(size_t i=0;i<3;++i) {m[i]=right[i]*sx;m[4+i]=n[i];m[8+i]=ahead[i]*sz;}
        m[12]=car.position[0]+n[0]*5;m[13]=g.y+n[1]*5;m[14]=car.position[2]+n[2]*5;
        shadow_frames_[slot]=m;
    }
    const auto found=shadow_frames_.find(slot);
    if(found==shadow_frames_.end()) return false;
    out=found->second;return true;
}
void RaceView::effects(const Matrix4& vp,const Matrix4& view,const Matrix4& camera_frame) {
    std::vector<const CarEffects*> cars{&session_.effects()};
    for(size_t i=0;i<session_.opponents().size();++i) cars.push_back(&session_.opponent_effects(i));
    // 0x47a7c0 (Direct3D path, 0x6f5958 = 2): for each wheel, two triangles
    // per pair of active segments, untextured (FVF 0x42), vertex colour
    // (fade << 24 | shade x 0x010101), mode 0x18: alpha blended, no culling,
    // no depth write. The software path lifts the points by 5 (0x47aad6);
    // natively too, against z-fighting with the ground (provisional).
    std::vector<WpcTexturedVertex3D> strip;
    for(const CarEffects* fx:cars) for(const auto& t:fx->tracks()) for(size_t i=0;i+1<t.size();++i) {
        const auto &s0=t[i],&s1=t[i+1];
        if(!s0.active || !s1.active) continue;
        const auto vertex=[](const Vec3& p,const CarEffects::TrackSegment& s) {
            WpcTexturedVertex3D v{};
            v.x=p[0];v.y=p[1]+5;v.z=p[2];
            v.r=v.g=v.b=float(s.shade)/255.f;v.a=float(std::max(s.fade,0))/255.f;
            return v;
        };
        for(const auto& v:{vertex(s0.a,s0),vertex(s0.b,s0),vertex(s1.a,s1),vertex(s1.a,s1),vertex(s0.b,s0),vertex(s1.b,s1)})
            strip.push_back(v);
    }
    if(!strip.empty()) {
        wpc_vk_set_mvp(window_,vp.data());
        wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
        wpc_vk_draw_material_triangles(window_,white_,strip.data(),uint32_t(strip.size()),3,1);
        wpc_vk_set_draw_mode(window_,0);
    }
    for(const Tnt& t:session_.tnts()) {
        RaceCar at;at.position=t.position;at.yaw=std::atan2(-t.velocity[0],t.velocity[2]);
        submit(tnt_,vp,view,at.frame());
    }
    for(const Projectile& p:session_.projectiles()) {
        const auto it=projectiles_.find(p.object);
        if(it==projectiles_.end()) continue;
        RaceCar at;at.position=p.position;at.yaw=std::atan2(-p.velocity[0],p.velocity[2]);
        submit(it->second,vp,view,at.frame());
    }
    for(const Landmine& m:session_.mines()) {
        RaceCar at;at.position=m.position;at.yaw=m.yaw;at.up=m.normal;
        submit(m.detour?detour_:m.oil?oil_:m.hive?hive_:landmine_,vp,view,at.frame());
    }
    // The storm cloud: the sprite at the cloud, faced to the camera
    // (0x419b90, type 0), its alpha the fade +0x152 (provisional: the
    // sprite's own draw of +0x152 is not traced).
    for(const auto& shown:storm_shown_) translucent_.erase(&shown);
    storm_shown_.resize(session_.storms().size());
    for(size_t i=0;i<session_.storms().size();++i) {
        const Storm& st=session_.storms()[i];
        std::vector<WorldBatch>& shown=storm_shown_[i];
        shown=storm_;
        if(st.done) {shown.clear();continue;}
        for(WorldBatch& b:shown) {
            if(!b.billboard) continue;
            b.anchor=st.position;
            for(auto& v:b.vertices) v.a*=std::clamp(st.fade,0.f,1.f);
        }
        face_billboards(shown,camera_frame);
        translucent_[&shown]=translucent_models(shown);
        submit(shown,vp,view,identity_matrix());
    }
    for(const Barrier& w:session_.barriers()) {
        RaceCar at;at.position=w.position;at.yaw=w.yaw;
        submit(w.tree?tree_:wall_,vp,view,at.frame());
    }
    for(const CarEffects* fx:cars) {
        for(const auto& s:fx->splashes()) {
            if(s.state<=0) continue;
            submit(splash_column_,vp,view,s.column());
            submit(splash_ring_,vp,view,s.ring());
        }
        for(const auto& r:fx->ripples()) {
            Matrix4 m=r.base;
            for(size_t i=0;i<3;++i) {m[i]*=r.scale;m[4+i]*=r.scale;m[8+i]*=r.scale;}
            submit(ripple_,vp,view,m);
        }
    }
}
void RaceView::draw() {
    int width,height;wpc_window_get_size(window_,&width,&height);
    if(width<=0 || height<=0) return;
    wpc_vk_clear(window_,0,0,0);
    scale_=std::min(width/640.f,height/480.f);
    const int w=int(640*scale_),h=int(480*scale_);
    offset_x_=(width-w)/2;offset_y_=(height-h)/2;
    if(!session_.two_players()) {
        wpc_vk_set_clip_rect(window_,offset_x_,offset_y_,w,h);
        draw_view(0,width,height);
        hud();
    } else {
        // Two players (Wine, 2026-11-01): player 1 in the top half, player 2
        // in the bottom one, each with its own follow camera and HUD.
        const int base=offset_y_;
        for(int p=0;p<2;++p) {
            wpc_vk_set_clip_rect(window_,offset_x_,base+p*(h/2),w,h/2);
            draw_view(p,width,height);
            offset_y_=base+p*(h/2);
            hud2(p);
            offset_y_=base;
        }
    }
    wpc_vk_set_clip_rect(window_,0,0,width,height);
}
void RaceView::animate_lights() {
    // 0x4caf95 (renderer mode 1): each lights vertex with a serialized v
    // below 0.5 (0x510390) takes v 0 while its lamp is lit, else 0.33
    // (0x5158b0); lamp A (0x52d8cc) when v < 0.24 (0x5158c0), else lamp B
    // (0x52d8cd); the lamp bit from u: red 1 below 0.33, amber 2 below 0.66
    // (0x5158a0), else green 4.
    const CourseObjects& objects=session_.objects();
    for(Lights& l:lights_) {
        if(!l.found) {
            l.found=true;
            for(size_t b=0;b<world_.batches.size();++b) if(world_.batches[b].node_id==l.node) {
                std::vector<std::array<float,2>> uv;
                for(const auto& v:world_.batches[b].vertices) uv.push_back({{v.u,v.v}});
                l.batches.emplace_back(b,std::move(uv));
            }
        }
        const TramCrossing& c=objects.crossings().at(l.crossing);
        for(const auto& [b,uv]:l.batches) {
            auto& vs=world_.batches[b].vertices;
            for(size_t k=0;k<vs.size() && k<uv.size();++k) {
                const float u=uv[k][0],v=uv[k][1];
                if(!(v<0.5f)) continue;
                const uint8_t lamp=v<0.24f?c.lamp_a():c.lamp_b();
                const uint8_t bit=u<0.33f?1:u<0.66f?2:4;
                vs[k].v=(lamp&bit)?0.f:0.33f;
            }
        }
    }
}
void RaceView::draw_view(int human,int width,int height) {
    if(human==0) animate_lights();
    const int w=int(640*scale_),h=int(480*scale_);
    const bool two=session_.two_players();
    wpc_vk_draw_rect(window_,offset_x_,offset_y_+(two?human*(h/2):0),w,two?h/2:h,world_.clear[0],world_.clear[1],world_.clear[2],1);
    // WFollowCamera_2 (0x478070), updated with the race.
    const Matrix4& camera=session_.camera(human);
    Matrix4 vp=multiply_matrix(projection_,inverse_affine_matrix(camera));
    for(size_t c=0;c<4;++c) {vp[c*4]*=float(w)/width;vp[c*4+1]*=float(h)/height;}
    // A half viewport keeps the pixel scale: the view's middle band, moved
    // up (player 1) or down (player 2) by a quarter of the frame (provisional).
    if(two) for(size_t c=0;c<4;++c) vp[c*4+1]+=vp[c*4+3]*(human?0.5f:-0.5f)*float(h)/height;
    // Backdrop around the eye, unfogged, just inside the far plane.
    {
        // Scaled to 93% of the far plane and lowered by 10.5% of the dome
        // radius, both fitted to the cc_t1 Wine frame (dome radius 2505).
        const float k=fog_end_*0.93f/backdrop_radius_;
        Matrix4 place=identity_matrix();
        place[0]=place[5]=place[10]=k;
        place[12]=camera[12];place[13]=camera[13]-backdrop_radius_*(262.f/2505.f)*k;place[14]=camera[14];
        wpc_vk_set_fog(window_,0,0,0,0,0);
        draw_batches(backdrop_,multiply_matrix(vp,place));
    }
    wpc_vk_set_fog(window_,world_.clear[0],world_.clear[1],world_.clear[2],fog_start_,fog_end_);
    // Broken cacti (flag 0x100 from the 0x40000 hits) are no longer drawn.
    if(session_.knocked_objects().size()!=knocked_) {
        for(size_t i=knocked_;i<session_.knocked_objects().size();++i)
            for(auto& b:world_.batches) if(b.node_id==session_.knocked_objects()[i]) b.hidden=true;
        knocked_=session_.knocked_objects().size();
    }
    face_billboards(world_.batches,camera);
    // Opaque parts draw as each model is submitted; translucent ones are
    // queued per model and flushed at the end of the frame (0x4b2d70).
    const Matrix4 view=inverse_affine_matrix(camera),id=identity_matrix();
    queue_.clear();queued_.clear();
    submit(world_.batches,vp,view,id);
    // The course continues through the WMOD loop transform: beyond the last
    // route's exit portal lies the first route, and vice versa.
    const Vec3 eye{{camera[12],camera[13],camera[14]}};
    const auto near=[&](const Vec3& p) {
        const float dx=p[0]-eye[0],dz=p[2]-eye[2];return dx*dx+dz*dz<25000.f*25000.f;
    };
    const RaceTrack& track=session_.track();
    if(near(track.exit(int(track.sections())-1))) submit(world_.batches,vp,view,track.unwrap());
    if(near(track.entry(0))) submit(world_.batches,vp,view,track.wrap());
    // Pickups: the GDModel's local transform becomes the shared spin times
    // the state's scale, about the WPowerUp's origin.
    {
        const auto& spots=session_.track().power_ups();
        const auto& states=session_.power_ups();
        const float spin=session_.power_up_spin(),c=std::cos(spin),sn=std::sin(spin);
        for(size_t i=0;i<pickups_.size() && i<states.size();++i) {
            if(!states[i].enabled || states[i].state==1 || states[i].state==2 || states[i].state==3) continue;
            const Vec3& o=spots[i].position;
            const float k=states[i].scale;
            Matrix4 m=identity_matrix();
            m[0]=c*k;m[2]=-sn*k;m[5]=k;m[8]=sn*k;m[10]=c*k;
            m[12]=o[0]-(m[0]*o[0]+m[8]*o[2]);m[13]=o[1]-k*o[1];m[14]=o[2]-(m[2]*o[0]+m[10]*o[2]);
            submit(pickups_[i],vp,view,m);
        }
    }
    {
        const CourseObjects& objects=session_.objects();
        for(size_t i=0;i<crossing_trams_.size();++i) if(objects.crossings()[i].active) submit(crossing_trams_[i],vp,view,objects.crossings()[i].tram_frame());
        for(size_t i=0;i<movers_.size();++i) if(objects.movers()[i].active) submit(movers_[i],vp,view,objects.movers()[i].placed);
        for(size_t i=0;i<logs_.size();++i) for(size_t k=0;k<logs_[i].size();++k) submit(logs_[i][k],vp,view,objects.log_rides()[i].frames[k]);
    }
    for(size_t i=0;i<session_.opponents().size();++i) {
        // A hidden robot (flag 0x100, 0x4a27b2) is not drawn.
        if(session_.opponent_drowning(i) || session_.opponent_hidden(i)) continue;
        // Player 2's car in its own near camera (see below).
        if(i==0 && human==1 && session_.two_players() && session_.camera_near(1)) continue;
        const auto& o=session_.opponents()[i];
        const Matrix4 frame=o.car.frame();
        Matrix4 shadow;
        // 0x47c180 hides the shadow (flag 0x100) in the far mode.
        if(!o.far && shadow_frame(i+1,o.car,shadow)) submit(shadows_.at(o.vehicle).batches,vp,view,shadow);
        submit_car(o.vehicle,session_.bomb(int(i)),vp,view,frame);
    }
    // 0x4782fb: in the near mode the camera writes itself into +0x24 of the
    // car's body, wheels and template part +0x2e0 (cleared on leaving);
    // read here as those parts not being drawn in that view (provisional).
    if(!session_.drowning() && !(human==0 && session_.camera_near(0))) {
        const Matrix4 frame=session_.player().frame();
        Matrix4 shadow;
        if(shadow_frame(0,session_.player(),shadow)) submit(shadows_.at(session_.player().vehicle).batches,vp,view,shadow);
        submit_car(session_.player().vehicle,session_.bomb(-1),vp,view,frame);
    }
    effects(vp,view,camera);
    queue_.flush([&](size_t i) {
        const Queued& q=queued_[i];
        wpc_vk_set_mvp(window_,q.mvp.data());
        wpc_vk_set_draw_mode(window_,draw_mode(q.model->draw_flags));
        for(size_t b:q.model->batches) {
            const WorldBatch& batch=(*q.batches)[b];
            if(batch.hidden) continue;
            wpc_vk_draw_material_triangles(window_,texture(batch.material),batch.vertices.data(),uint32_t(batch.vertices.size()),
                                           batch.material.clamp_axes,batch.material.two_sided?1:0);
        }
    });
    wpc_vk_set_draw_mode(window_,0);
    // Flush step 5 (0x4b32af): the GD3DFilm list, kept nearest first by
    // squared distance (0x4b50e0, at most 200) and drawn in reverse.
    {
        struct Quad {float distance;const Glyph* frame;Vec3 at;float scale,angle;std::array<float,4> colour;};
        std::vector<Quad> quads;
        const auto add=[&](const Glyph* g,const Vec3& at,float scale,float angle,std::array<float,4> colour) {
            float d=0;
            for(size_t i=0;i<3;++i) d+=(at[i]-camera[12+i])*(at[i]-camera[12+i]);
            quads.push_back({d,g,at,scale,angle,colour});
        };
        for(size_t k=0;k<=session_.opponents().size();++k) {
        const RaceCar& car=k?session_.opponents()[k-1].car:session_.player();
        const CarEffects& fx=k?session_.opponent_effects(k-1):session_.effects();
        for(const auto& p:fx.puffs()) {
            if(!p.age || size_t(p.reel)>=puff_reels_.size() || puff_reels_[size_t(p.reel)].empty()) continue;
            const auto& reel=puff_reels_[size_t(p.reel)];
            Vec3 at{};
            for(size_t i=0;i<3;++i) at[i]=car.position[i]+p.offset[i];
            add(&reel[size_t(p.frame)%reel.size()],at,puff_scale_,0,{{1,1,1,1}});
        }
        // 0x486c40: WKBumm's pieces, scale 0.2, in the clone's frame.
        if(!bumm_frames_.empty()) for(const auto& b:fx.bumms()) for(const auto& p:b.pieces) {
            if(p.landed) continue;
            Vec3 at{};
            for(size_t i=0;i<3;++i) at[i]=b.base[12+i]+b.base[i]*p.position[0]+b.base[4+i]*p.position[1]+b.base[8+i]*p.position[2];
            add(&bumm_frames_[size_t(p.frame)%bumm_frames_.size()],at,0.2f,p.angle,{{1,1,1,1}});
        }
        // 0x47b9d0: the POUSSIERE puffs, two in three slots, frame age - 1,
        // colour (160, 128, 0), alpha (24 - age) x 3, scale 0.5 + 0.05 age.
        if(!smoke_frames_.empty()) for(size_t i=0;i<fx.smoke().size();++i) {
            const auto& m=fx.smoke()[i];
            if(!m.age || i%3==0) continue;
            add(&smoke_frames_[size_t(m.age-1)%smoke_frames_.size()],m.position,float(m.age)*0.05f+0.5f,0,
                {{160/255.f,128/255.f,0,float((24-m.age)*3)/255.f}});
        }
        }
        // WExplosion (provisional: its update is not traced): m_Size 500
        // reached over m_GrowTime (2), then faded over m_FadeOutTime (20),
        // tinted (m_Red, m_Green, m_Blue) = (255, 64, 0).
        if(blast_.texture && blast_.width>0) for(const auto& b:session_.blasts()) {
            const float grow=std::min(1.f,float(b.age)/2.f),fade=std::clamp(1-float(b.age-2)/20.f,0.f,1.f);
            add(&blast_,b.at,500.f/(6.f*float(blast_.width))*std::max(grow,0.05f),0,{{1,64/255.f,0,fade}});
        }
        std::stable_sort(quads.begin(),quads.end(),[](const Quad& a,const Quad& b){return a.distance<b.distance;});
        if(quads.size()>200) quads.resize(200);
        wpc_vk_set_mvp(window_,vp.data());
        for(auto q=quads.rbegin();q!=quads.rend();++q) {
            // 0x42d306: corners ftol(6 x scale x offset) and that plus
            // ftol(6 x scale x size), in the camera's right/up basis. The
            // frame's x offset serves both axes, as in the original; a
            // nonzero +0x14a angle turns the corners in that plane.
            const Glyph& g=*q->frame;
            if(!g.texture) continue;
            const int lo=int(float(g.x)*q->scale*6.f);
            const float x0=float(lo),x1=float(lo-int(float(g.width)*q->scale*-6.f));
            const float y0=float(lo),y1=float(lo-int(float(g.height)*q->scale*-6.f));
            // 0x42d2b4: unless its material is SMOO, the quad moves a fifth
            // of the way toward the camera.
            Vec3 at=q->at;
            if(!g.smooth) for(size_t i=0;i<3;++i) at[i]+=(camera[12+i]-at[i])*0.2f;
            const float c=std::cos(q->angle),sn=std::sin(q->angle);
            const auto corner=[&](float x,float y,float u,float v) {
                const float rx=x*c-y*sn,ry=x*sn+y*c;
                WpcTexturedVertex3D w{};
                w.x=at[0]+camera[0]*rx+camera[4]*ry;w.y=at[1]+camera[1]*rx+camera[5]*ry;w.z=at[2]+camera[2]*rx+camera[6]*ry;
                w.u=u;w.v=v;w.r=q->colour[0];w.g=q->colour[1];w.b=q->colour[2];w.a=q->colour[3];return w;
            };
            const WpcTexturedVertex3D quad[6]={corner(x0,y1,0,0),corner(x1,y1,1,0),corner(x1,y0,1,1),
                                               corner(x0,y1,0,0),corner(x1,y0,1,1),corner(x0,y0,0,1)};
            wpc_vk_draw_material_triangles(window_,g.texture,quad,6,3,1);
        }
    }
    wpc_vk_set_fog(window_,0,0,0,0,0);
    weather(vp,view,camera); // after the scene, before the HUD (provisional order)
}
void RaceView::weather(const Matrix4& vp,const Matrix4& view,const Matrix4& camera) {
    if(!session_.raining()) return;
    // 0x4565e0: no weather with the camera inside a Zartter box.
    if(session_.track().sheltered({{camera[12],camera[13],camera[14]}})) return;
    // The original redraws its weather every frame; here it steps once per
    // race update (at most five behind), as the rest of the race does.
    // rand() is a native generator here (the original shares 0x430e40).
    MsvcRandom& r=weather_random_;
    const auto unit=[&](float x){return r.unit(x);};                       // 0x430fc0
    const auto spread=[&](float x){return r.unit(2*x)-x;};                 // 0x431010
    const uint64_t now=session_.updates();
    int steps=weather_update_==~uint64_t(0) ? 1 : int(std::min<uint64_t>(now-weather_update_,5));
    weather_update_=now;
    if(session_.snowing()) {
        if(flakes_.empty()) flakes_.resize(1500);
        for(;steps>0;--steps) for(Flake& f:flakes_) {
            const auto roll=[&]{f.velocity={{spread(10),unit(2)+2,spread(10)}};};
            if(f.at[0]+f.at[1]+f.at[2]==0) {roll();f.at={{spread(3000),spread(550),spread(3000)}};f.count=8;}
            if(r.next()%100>90 && f.count>0) {roll();--f.count;}
            f.at[0]+=f.velocity[0];f.at[1]-=f.velocity[1];f.at[2]+=f.velocity[2];
            if(f.at[1]<-150) {f.at={{spread(3000),unit(250)+300,spread(3000)}};f.count=8;}
        }
        // Each flake: camera position + offset, into view space (0x4029b4),
        // a 10 x 10 quad (0x510254) with u, v from (0,0) to (1,1).
        std::vector<WpcTexturedVertex3D> v;v.reserve(flakes_.size()*6);
        for(const Flake& f:flakes_) {
            const Vec3 w{{camera[12]+f.at[0],camera[13]+f.at[1],camera[14]+f.at[2]}};
            const float x=view[0]*w[0]+view[4]*w[1]+view[8]*w[2]+view[12];
            const float y=view[1]*w[0]+view[5]*w[1]+view[9]*w[2]+view[13];
            const float z=view[2]*w[0]+view[6]*w[1]+view[10]*w[2]+view[14];
            const auto at=[&](float dx,float dy,float u,float t){WpcTexturedVertex3D q{};q.x=x+dx;q.y=y+dy;q.z=z;q.u=u;q.v=t;q.r=q.g=q.b=q.a=1;return q;};
            for(const auto& q:{at(0,0,0,0),at(10,0,1,0),at(10,-10,1,1),at(0,0,0,0),at(10,-10,1,1),at(0,-10,0,1)}) v.push_back(q);
        }
        if(flake_) {
            wpc_vk_set_fog(window_,0,0,0,0,0);
            wpc_vk_set_mvp(window_,projection_.data());
            wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
            wpc_vk_draw_material_triangles(window_,flake_,v.data(),uint32_t(v.size()),3,1);
            wpc_vk_set_draw_mode(window_,0);
        }
        return;
    }
    // Rain: 2500 streaks (0x9c4) a frame in the camera's frame: z in
    // [0, 10000), x and y in [-8000, 8000); grey c = rand() % 100 + 50; the
    // top transparent, the bottom (500 lower, 30 left) with alpha
    // -(z/8 + 1) & 0xff. D3D draws them as a line list; here each is a strip
    // about a pixel wide (provisional).
    if(steps>0 || rain_.empty()) {
        rain_.clear();rain_.reserve(2500*6);
        const float focal=std::max(projection_[0],1e-3f);
        for(int i=0;i<2500;++i) {
            const float z=unit(10000),y=spread(8000),x=spread(8000);
            const float c=float(r.next()%100+50)/255.f;
            const float a=float((-((int(z)>>3)+1))&0xff)/255.f;
            const float h=0.75f*std::max(z,50.f)/(focal*320);
            const auto at=[&](float px,float py,float alpha){WpcTexturedVertex3D q{};q.x=px;q.y=py;q.z=z;q.r=q.g=q.b=c;q.a=alpha;return q;};
            const WpcTexturedVertex3D t0=at(x-h,y,0),t1=at(x+h,y,0),b0=at(x-30-h,y-500,a),b1=at(x-30+h,y-500,a);
            for(const auto& q:{t0,t1,b1,t0,b1,b0}) rain_.push_back(q);
        }
    }
    const Matrix4 mvp=multiply_matrix(vp,camera);
    wpc_vk_set_fog(window_,0,0,0,0,0);
    wpc_vk_set_mvp(window_,mvp.data());
    wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
    wpc_vk_draw_material_triangles(window_,white_,rain_.data(),uint32_t(rain_.size()),3,1);
    wpc_vk_set_draw_mode(window_,0);
}
void RaceView::glyph(const Glyph& g,int x,int y,Colour colour) {
    if(!g.texture) return;
    wpc_vk_draw_texture_tinted(window_,offset_x_+int(std::lround((x+g.x)*scale_)),offset_y_+int(std::lround((y+g.y)*scale_)),
                               int(std::lround(g.width*scale_)),int(std::lround(g.height*scale_)),g.texture,
                               colour.r,colour.g,colour.b,1);
}
int RaceView::text_width(const std::string& value) const {
    int width=0;
    for(unsigned char c:value) {
        const size_t i=c>=0x20 && size_t(c-0x20)<font_.size()?c-0x20:'?'-0x20;
        width+=c<=0x20?10:std::max(0,font_[i].width-1);
    }
    return width;
}
void RaceView::text(const std::string& value,int x,int y,int align,Colour colour) {
    // Same cursor rule as the menu text renderer (0x42dfd6): 4 centers,
    // 0x20000 right-aligns.
    int cursor=x-(align==4?text_width(value)/2:(align&0x20000)?text_width(value):0);
    for(unsigned char c:value) {
        if(c<=0x20) {cursor+=10;continue;}
        const size_t i=size_t(c-0x20)<font_.size()?c-0x20:'?'-0x20;
        glyph(font_[i],cursor,y,colour);
        cursor+=std::max(0,font_[i].width-1);
    }
}
void RaceView::quad(WpcTexture* t,float x,float y,float width,float height,bool mirror,float alpha,
                    float angle,float px,float py) {
    if(!t) return;
    const float c=std::cos(angle),sn=std::sin(angle);
    const float corners[4][2]={{0,0},{1,0},{0,1},{1,1}};
    WpcTexturedVertex3D q[4]{};
    for(int i=0;i<4;++i) {
        const float lx=corners[i][0]*width-px,ly=corners[i][1]*height-py;
        // Screen Y grows downward; a positive angle turns clockwise.
        q[i].x=x+px+lx*c-ly*sn;q[i].y=y+py+lx*sn+ly*c;
        q[i].u=mirror?1-corners[i][0]:corners[i][0];q[i].v=corners[i][1];
        q[i].r=q[i].g=q[i].b=1;q[i].a=alpha;
    }
    const int order[6]={0,1,2,2,1,3};
    WpcTexturedVertex3D v[6];
    for(int i=0;i<6;++i) v[i]=q[order[i]];
    int w,h;wpc_window_get_size(window_,&w,&h);
    Matrix4 ortho{};
    ortho[0]=2*scale_/w;ortho[5]=2*scale_/h;ortho[10]=1;ortho[15]=1;
    ortho[12]=2.f*offset_x_/w-1;ortho[13]=2.f*offset_y_/h-1;
    wpc_vk_set_mvp(window_,ortho.data());
    wpc_vk_draw_material_triangles(window_,t,v,6,3,1);
}
void RaceView::step_hud() {
    const auto racers=session_.strip();
    if(strip_x_.size()!=racers.size()) strip_x_.assign(racers.size(),0);
    // Once per game update: the original steps these per HUD draw at the
    // 33 ms frame rate.
    for(uint64_t u=std::max<uint64_t>(hud_update_,session_.updates()-std::min<uint64_t>(session_.updates(),30));
        u<session_.updates();++u) {
        // 0x4d2d85: charge level 0..3 -> -56, -30, 23, 56 degrees, 3 per update.
        static const float levels[4]={-56,-30,23,56};
        for(int h=0;h<(session_.two_players()?2:1);++h) {
            float& angle=gauge_angle_[size_t(h)];
            const float target=levels[std::clamp(session_.charge_level(h),0,3)];
            const float diff=target-angle;
            if(diff>0) angle=diff<3?target:angle+3;
            else angle=diff>-3?target:angle-3;
        }
        // 0x499e35: x = 220 * lap distance / lap length, eased by 0.6 unless
        // the jump is 150 or more (the lap wrap); 220 once finished.
        for(size_t i=0;i<racers.size();++i) {
            const float x=racers[i].finished?220.f:float(int(220*racers[i].lap_fraction));
            const float d=x-strip_x_[i];
            strip_x_[i]=std::abs(d)<150?float(int(strip_x_[i]+d*0.6f)):x;
        }
    }
    hud_update_=session_.updates();
}
void RaceView::progress_strip() {
    // WFirstPowerUp 0x4d2faf (single race or championship, one player):
    // end dots at x 18 and 235 (y 220), small dots every 6 from 26 to 230.
    quad(big_dot_.texture,36,440,float(big_dot_.width),float(big_dot_.height),false,1);
    quad(big_dot_.texture,470,440,float(big_dot_.width),float(big_dot_.height),false,1);
    for(int x=26;x<0xec;x+=6) quad(small_dot_.texture,float(2*x),444,float(small_dot_.width),float(small_dot_.height),false,1);
    // WModMngrFilm 0x499d90: each racer's head (frame vehicle+2) at
    // (x+20, 216), scaled 0.65.
    const auto racers=session_.strip();
    for(size_t i=0;i<racers.size() && i<strip_x_.size();++i) {
        const Glyph& g=heads_.at(size_t(racers[i].vehicle+2));
        const float k=0.65f;
        quad(g.texture,2*(strip_x_[i]+20)+g.x*k,432+g.y*k,g.width*k,g.height*k,false,1);
    }
}
void RaceView::standings() {
    // WModMngrFilm 0x499c38 (states 5..7): the standings' heads (frame
    // vehicle+2, scale 0.65) at x 45, y 65 + 20 per place. Each arrives as
    // t = (counter + 8*place - 85)^2 reaches 0: odd places from the right
    // (x 45+t, t <= 800), even places from the left (x 45-t, t <= 400).
    // 0x4a8f30 writes each row's text with the same slide: the place (in
    // state 7 the points total) at x+20 and the name (string 0x52cf0c) at
    // x+60, y = 62 + 20 per place, each over a black copy 2 right, 2 down.
    static const uint32_t names[7]={60,57,58,55,59,56,54};
    const auto& order=session_.standings();
    const int state=session_.finish_state();
    // Rows above y 122 (82 in state 7) are (255,128,128), the rest white;
    // the player's name pulses, g = b = 192 + 63 sin(GetTickCount * 0.01).
    const int highlight=state==7?0x52:0x7a;
    const float pulse=(192+63*std::sin(float(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()%6283185)*0.01f))/255;
    for(size_t r=0;r<order.size();++r) {
        int t=std::max(0,session_.finish_counter()+int(r)*8-0x55);
        t*=t;
        const int slide=(r&1)?std::min(t,800):-std::min(t,400);
        const int x=45+slide;
        const int y=65+int(r)*20;
        const Glyph& g=heads_.at(size_t(order[r]+2));
        const float k=0.65f;
        quad(g.texture,2.f*x+g.x*k,2.f*y+g.y*k,g.width*k,g.height*k,false,1);
        const int vehicle=order[r];
        const int row=0x3e + int(r)*20;
        const int number=state==7?session_.points_totals().at(size_t(vehicle))+session_.race_points().at(size_t(vehicle)):int(r)+1;
        const std::string place=std::to_string(number);
        const std::string& name=strings_.get(names[std::clamp(vehicle,0,6)]);
        Colour colour{1,128/255.f,128/255.f};
        if(vehicle==session_.player().vehicle) colour={1,pulse,pulse};
        else if(row>=highlight) colour={1,1,1};
        text(place,2*(slide+0x16),2*row,0,{0,0,0});
        text(place,2*(slide+0x14),2*(row-2),0,colour);
        text(name,2*(slide+0x3e),2*row,0,{0,0,0});
        text(name,2*(slide+0x3c),2*(row-2),0,colour);
        // Environment 3 (the last course): a player who is not leading is told
        // to win the championship, once the counter is below 5.
        if(state==7 && session_.finish_counter()<5 && vehicle==session_.player().vehicle && r>0)
            for(size_t i=0;i<championship_message_.size();++i)
                text(championship_message_[i],320,2*(5+15*int(i)),4);
    }
}
void RaceView::powerup_panel(int human) {
    // 0x4d2c80, per viewport (+0x13a): player 1's car (+0x11e) or player 2's
    // (+0x122). Gauge at (230,68), needle turned about (10,29) at (273,93),
    // or (276,95) in player 2's viewport.
    const int nx=human?276:273,ny=human?95:93;
    quad(gauge_.texture,460,136,float(gauge_.width),float(gauge_.height),false,1);
    quad(gauge_needle_.texture,float(2*nx-10),float(2*ny-29),float(gauge_needle_.width),float(gauge_needle_.height),false,1,
         gauge_angle_[size_t(human&1)]*0.0174532925f,10,29);
    // The gadget (+0x371) at (250,10) and weapon (+0x375) at (290,10); their
    // materials get transparency 0.4 while the charge is 0.
    const float alpha=session_.charge_level(human)>=1?1.f:0.6f;
    for(const auto& [index,x]:{std::pair<int,int>{session_.gadget_icon(human),250},std::pair<int,int>{session_.weapon_icon(human),290}}) {
        if(index<0 || size_t(index)>=icons_.size()) continue;
        const Sprite& icon=icons_.at(size_t(index));
        quad(icon.texture,float(2*x-icon_origin_[0]),float(20-icon_origin_[1]),float(icon.width),float(icon.height),false,alpha);
    }
}
void RaceView::hud2(int human) {
    step_hud();
    const int own=session_.frags(human?0:-1),other=session_.frags(human?-1:0);
    if(session_.finish_state()==10) {
        // 0x4aa440 per viewport (+0x13a): this player's hits received
        // decide TU AS PERDU! (6) or TU AS GAGNE! (5).
        const std::string& line=strings_.get(own>=RaceSession::frag_limit?6:5);
        text(line,324,124,4,Colour{0,0,0});text(line,320,120,4);
        text(strings_.get(145),320,200,4);text("CONTINUE - X",320,400,4);
        return;
    }
    if(session_.finish_state()!=0) {
        if(human==0) standings();
        if(session_.finish_counter()<5) text(strings_.get(7),320,200,4); // (160,100) in two-player
        return;
    }
    // Layout rows 1/2 of 0x52bd5c, in the viewport's own frame (the timer at
    // (160,216) and the progress strip fall outside the half and are cut;
    // the dial is hidden): lap (13,30), rank (33,50) and suffix (44,50),
    // message (160,60).
    char buffer[32];
    if(session_.mode()==1) {
        // Battle, rows 1/2: children 13/14 at (14,105) and (306,105); each
        // HUD block holds its own player's hits first (0x6f3168/0x6f316c,
        // 0x6f31bc/0x6f31c0 at 0x4aa6c1).
        std::snprintf(buffer,sizeof buffer,"%d",own);text(buffer,14*2,105*2,0);
        std::snprintf(buffer,sizeof buffer,"%d",other);text(buffer,306*2,105*2,0x20000);
    } else {
    std::snprintf(buffer,sizeof buffer,"%d/%d",human?session_.lap2():session_.lap(),session_.laps());
    text(buffer,13*2,30*2,0);
    const int rank=human?session_.rank2():session_.rank();
    std::snprintf(buffer,sizeof buffer,"%d",rank);
    text(buffer,33*2,50*2,0);
    const std::string& suffix=strings_.get(uint32_t(0xb6+std::clamp(rank,1,7)));
    if(!suffix.empty()) text(suffix,44*2,50*2,0);
    }
    std::string message;
    switch(session_.phase()) {
        case RaceSession::Phase::Ready: message=strings_.get(3); break;
        case RaceSession::Phase::Go: message=strings_.get(4); break;
        default: break;
    }
    if(!message.empty()) text(message,160*2,60*2,4);
    powerup_panel(human);
}
void RaceView::hud() {
    step_hud();
    if(session_.finish_state()==10) {
        // 0x4aa440, the battle's end (state 10): TU AS PERDU! (string 6)
        // when the player received the limit, else TU AS GAGNE! (5), at
        // (160,60) over a shadow at (162,62); string 145 at (160,100) and
        // the literal "CONTINUE - X" (0x52d210) at (160,200). The winner's
        // pulsing colour (sin of the time) is not ported.
        const std::string& line=strings_.get(session_.battle_lost()?6:5);
        text(line,324,124,4,Colour{0,0,0});text(line,320,120,4);
        text(strings_.get(145),320,200,4);text("CONTINUE - X",320,400,4);
        return;
    }
    if(session_.finish_state()!=0 && session_.finish_state()!=15) {
        // States 5..7 (0x4a8f30) set HUD mode 0x6f3164 to -1, so WHudBrick
        // hides every child: only the standings and their text are drawn,
        // and the Enter prompt (string 7) at (160,200) once the counter is
        // below 5.
        standings();
        if(session_.finish_counter()<5) text(strings_.get(7),320,400,4);
        return;
    }
    // 0x499c38: the strip draws in states 1-3 only (5-7: the standings;
    // the battle runs in state 9).
    if(session_.finish_state()!=15 && session_.mode()!=1) progress_strip();
    powerup_panel();
    // WHudBrick child 15 (0x4861a0): bend arrow at (160,100). Types 0/1 use
    // frame 0, 3/4 frame 1, 2 frame 2; types 0 and 3 are mirrored.
    if(const int arrow=session_.bend_arrow();arrow>=0) {
        static const int frames[5]={0,0,2,1,1};
        const Glyph& g=bend_.at(size_t(frames[arrow]));
        const bool mirror=arrow==0 || arrow==3;
        const float x=320+(mirror?-(g.x+g.width):g.x);
        quad(g.texture,x,200+g.y,float(g.width),float(g.height),mirror,1);
    }
    char buffer[32];
    // Battle (HUD mode 1, 0x485b00): children 3-12 hidden (no laps, rank,
    // timer or neighbours); child 13 shows the player's hits received
    // (0x6f3168 = +0x3a3) at (14,216), child 14 (right-aligned, 0x4b
    // 0xa0000) the opponent's (0x6f316c) at (306,216).
    if(session_.mode()==1) {
        std::snprintf(buffer,sizeof buffer,"%d",session_.frags(-1));text(buffer,14*2,216*2,0);
        if(!session_.opponents().empty()) {std::snprintf(buffer,sizeof buffer,"%d",session_.frags(0));text(buffer,306*2,216*2,0x20000);}
    } else {
    std::snprintf(buffer,sizeof buffer,"%d/%d",session_.lap(),session_.laps());
    text(buffer,layout_lap[0]*2,layout_lap[1]*2,0);
    }
    // 0x450f00: a 1/300 s time as minutes and seconds (%02d:%02d, 0x52a700).
    const auto clock=[&](int t,int x,int y) {
        const int s=t/300;std::snprintf(buffer,sizeof buffer,"%02d:%02d",(s/60)%60,s%60);text(buffer,x*2,y*2,0);
    };
    if(session_.mode()==2) {
        // Practice (0x4861xx, HUD mode 2): children 4-6 and 8-14 hidden
        // (no rank, timer or neighbours); the laps run so far (0x6f3198) at
        // (13,45), the record (0x6f3184) at (40,215) and its vehicle's head:
        // child 7 (GDFilm, film 66332) frame 0x6f3188 at (23,220). The strip
        // stays (0x499c38 tests only 0x6f2cc0, one player).
        clock(session_.split_total(),13,45);
        clock(int(session_.record()&0xfffffffu),40,215);
        const size_t head=size_t(session_.record()>>28);
        if(head<faces_.size()) {const Glyph& g=faces_.at(head);quad(g.texture,23*2+g.x,220*2+g.y,float(g.width),float(g.height),false,1);}
    }
    if(session_.finish_state()==15) {
        // 0x4a9ccf, state 0xf: with +0xfa4 set the HUD message (child 2,
        // (160,120)) is string 231; string 7 at (160,150) once the counter
        // is below 5.
        if(session_.new_record()) text(strings_.get(231),320,240,4);
        if(session_.finish_counter()<5) text(strings_.get(7),320,300,4);
    }
    const int rank=session_.rank();
    if(session_.mode()!=2 && session_.mode()!=1) {
        std::snprintf(buffer,sizeof buffer,"%d",rank);
        text(buffer,layout_rank[0]*2,layout_rank[1]*2,0);
        // 0x4864c6 draws string rank+0xb6: one HUD-font character per language
        // (English _/a/b/` = ST/ND/RD/TH, French t/u = ER/EME).
        const std::string& suffix=strings_.get(uint32_t(0xb6+std::clamp(rank,1,7)));
        if(!suffix.empty()) text(suffix,layout_suffix[0]*2,layout_suffix[1]*2,0);
        const int seconds=int(session_.race_seconds());
        std::snprintf(buffer,sizeof buffer,"%02d:%02d",std::min(seconds/60,99),seconds%60);
        text(buffer,layout_timer[0]*2,layout_timer[1]*2,0);
    }
    std::string message;
    switch(session_.phase()) {
        case RaceSession::Phase::Ready: message=strings_.get(3); break;
        case RaceSession::Phase::Go: message=strings_.get(4); break;
        default: if(session_.wrong_way()) message=strings_.get(2); break;
    }
    if(!message.empty()) text(message,layout_message[0]*2,layout_message[1]*2,4);
    // Speedometer: 0x485b00 angle = speed*PI/max - PI/2, capped at 0.27.
    if(dial_.texture) wpc_vk_draw_texture_tinted(window_,offset_x_+int(std::lround((layout_dial[0]*2-dial_xy_[0])*scale_)),
        offset_y_+int(std::lround((layout_dial[1]*2-dial_xy_[1])*scale_)),int(std::lround(dial_.width*scale_)),
        int(std::lround(dial_.height*scale_)),dial_.texture,1,1,1,1);
    if(needle_.texture) {
        const float top=session_.tuning().top_speed;
        const float angle=std::min(session_.player().speed.speed*3.14159265f/top-1.5707963f,0.27f);
        const float c=std::cos(angle),s=std::sin(angle);
        const float px=layout_needle[0]*2,py=layout_needle[1]*2,ox=10,oy=52;
        WpcTexturedVertex3D v[6]{};
        const float corners[4][2]={{0,0},{1,0},{0,1},{1,1}};
        WpcTexturedVertex3D quad[4]{};
        for(int i=0;i<4;++i) {
            const float lx=corners[i][0]*needle_.width-ox,ly=corners[i][1]*needle_.height-oy;
            // Screen Y grows downward; a positive angle turns the needle clockwise.
            quad[i].x=px+lx*c-ly*s;quad[i].y=py+lx*s+ly*c;quad[i].z=0;
            quad[i].u=corners[i][0];quad[i].v=corners[i][1];
            quad[i].r=quad[i].g=quad[i].b=quad[i].a=1;
        }
        const int order[6]={0,1,2,2,1,3};
        for(int i=0;i<6;++i) v[i]=quad[order[i]];
        int width,height;wpc_window_get_size(window_,&width,&height);
        Matrix4 ortho{};
        ortho[0]=2*scale_/width;ortho[5]=2*scale_/height;ortho[10]=1;ortho[15]=1;
        ortho[12]=2.f*offset_x_/width-1;ortho[13]=2.f*offset_y_/height-1;
        wpc_vk_set_mvp(window_,ortho.data());
        wpc_vk_draw_material_triangles(window_,needle_.texture,v,6,3,1);
    }
}
}
