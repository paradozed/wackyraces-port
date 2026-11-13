#include "race_view.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
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
        rain_dim_=session_.raining() && !session_.snowing();
        if(rain_dim_) {
            // 0x4ae364: with rain the clear and fog colour (D3DRS_FOGCOLOR 0x22)
            // is (R >> 2, G >> 2, B x 192 >> 9). Wine, cc_t1 (METEO 1): the sky
            // (255, 231, 115) clear becomes (58, 57, 41) in rain.
            const auto byte=[](float c){return int(std::lround(std::clamp(c,0.f,1.f)*255));};
            const int r=byte(world_.clear[0]),g=byte(world_.clear[1]),b=byte(world_.clear[2]);
            world_.clear[0]=float(r>>2)/255.f;world_.clear[1]=float(g>>2)/255.f;world_.clear[2]=float((b*192)>>9)/255.f;
        }
        upload(world_.textures);
        // GDViewport m_BackDrop (0xc8): cc_t1's sky dome GDModel 0x19.
        if(const Node* viewport=find_class(course.screen(),"GDViewport")) {
            const uint32_t backdrop=uint32_t(viewport->integer(0xc8));
            auto split=std::stable_partition(world_.batches.begin(),world_.batches.end(),
                [&](const WorldBatch& b){return b.node_id!=backdrop;});
            backdrop_.assign(std::make_move_iterator(split),std::make_move_iterator(world_.batches.end()));
            world_.batches.erase(split,world_.batches.end());
            // The dome is drawn in its node's frame less the translation: dr_t2's
            // sits at (8898, 20, 60420), cc_t3's at (0, 262, 1464).
            // Dome radius: 2505 on cc_t1, about 5000 on sh_t1/ct_t1, 6420 on dt_t1.
            if(!backdrop_.empty()) {
                const Matrix4 m=unwrapped_scene_transform(course.root(),backdrop);
                backdrop_origin_={{m[12],m[13],m[14]}};
            }
            for(const auto& b:backdrop_) for(const auto& v:b.vertices)
                backdrop_radius_=std::max({backdrop_radius_,std::abs(v.x-backdrop_origin_[0]),std::abs(v.z-backdrop_origin_[2])});
            // 0x4b4347 (the backdrop's draw 0x4b41a0): with rain every vertex
            // colour is B 0xc0, G 0x80, R 0x80, its alpha kept. Wine, cc_t1: the
            // dome's (255, 231, 115) becomes (58, 57, 41) with the halved texture.
            if(rain_dim_) for(auto& b:backdrop_) for(auto& v:b.vertices) {v.r=128/255.f;v.g=128/255.f;v.b=192/255.f;}
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
            bridges_.resize(objects.bridges().size());
            for(size_t i=0;i<bridges_.size();++i) for(size_t k=0;k<2;++k) take(objects.bridges()[i].halves[k],bridges_[i][k]);
            holes_.resize(objects.dynamite_holes().size());
            for(size_t i=0;i<holes_.size();++i) {take(objects.dynamite_holes()[i].lid,holes_[i][0]);take(objects.dynamite_holes()[i].fuse,holes_[i][1]);}
            hole_fuses_.clear();
            for(const DynamiteHole& d:objects.dynamite_holes()) hole_fuses_.push_back(unwrapped_scene_transform(course.root(),d.fuse));
            geysers_.resize(objects.geysers().size());
            for(size_t i=0;i<geysers_.size();++i) {take(objects.geysers()[i].column,geysers_[i][0]);if(objects.geysers()[i].ring) take(objects.geysers()[i].ring,geysers_[i][1]);}
            chisels_.resize(objects.chisels().size());
            for(size_t i=0;i<chisels_.size();++i) {
                const auto& pieces=objects.chisels()[i].blow.pieces;chisels_[i].resize(pieces.size());
                for(size_t k=0;k<pieces.size();++k) take(pieces[k].node,chisels_[i][k]);
            }
            rocks_.resize(objects.rocks().size());
            for(size_t i=0;i<rocks_.size();++i) {
                const auto& pieces=objects.rocks()[i].blow.pieces;rocks_[i].resize(pieces.size());
                for(size_t k=0;k<pieces.size();++k) take(pieces[k].node,rocks_[i][k]);
            }
            dicks_.resize(objects.dicks_roads().size());
            for(size_t i=0;i<dicks_.size();++i) for(size_t k=0;k<3;++k) take(objects.dicks_roads()[i].pieces[k],dicks_[i][k]);
            golden_.resize(objects.golden_gates().size());
            for(size_t i=0;i<golden_.size();++i) {
                const GoldenGate& g=objects.golden_gates()[i];
                for(size_t k=0;k<9;++k) take(g.doors[k].node,golden_[i][k]);
                take(g.marker,golden_[i][9]);
            }
            cranes_.resize(objects.cranes().size());
            for(size_t i=0;i<cranes_.size();++i) {
                // The group's batches, by the deepest arm that holds each.
                std::vector<WorldBatch> all;take(objects.cranes()[i].arms[0],all);
                const Node& a0=course.find(objects.cranes()[i].arms[0]);
                std::vector<uint32_t> below1,below2;
                const auto gather=[&](const auto& self,const Node& n,std::vector<uint32_t>& out)->void {out.push_back(n.id);for(const auto& c:n.children) self(self,c,out);};
                (void)a0;
                gather(gather,course.find(objects.cranes()[i].arms[1]),below1);
                gather(gather,course.find(objects.cranes()[i].arms[2]),below2);
                for(WorldBatch& b:all) {
                    const size_t k=std::find(below2.begin(),below2.end(),b.node_id)!=below2.end()?2:(std::find(below1.begin(),below1.end(),b.node_id)!=below1.end()?1:0);
                    cranes_[i][k].push_back(std::move(b));
                }
            }
            shifted_.resize(objects.shifted_roads().size());
            for(size_t i=0;i<shifted_.size();++i) for(size_t k=0;k<6;++k) take(objects.shifted_roads()[i].kids[k],shifted_[i][k]);
            saws_.resize(objects.saws().size());
            for(size_t i=0;i<saws_.size();++i) {
                // The spinner's group holds the sliding owner: its batches move apart.
                std::vector<WorldBatch> all;take(objects.saws()[i].spinner,all);
                for(WorldBatch& b:all) saws_[i][b.node_id==objects.saws()[i].node?1:0].push_back(std::move(b));
            }
            bumps_.resize(objects.bum_steers().size());
            for(size_t i=0;i<bumps_.size();++i) {
                const BumSteer& b=objects.bum_steers()[i];bumps_[i].resize(4);
                for(size_t k=0;k<2;++k) {take(b.gates[k],bumps_[i][k]);take(b.runners[k],bumps_[i][2+k]);}
            }
            spins_.resize(objects.model_spins().size());
            for(size_t i=0;i<spins_.size();++i) take(objects.model_spins()[i].node,spins_[i]);
            jams_.resize(objects.log_jams().size());
            for(size_t i=0;i<jams_.size();++i) {
                const auto& logs=objects.log_jams()[i].logs;jams_[i].resize(logs.size());
                for(size_t k=0;k<logs.size();++k) take(logs[k].node,jams_[i][k]);
            }
            // 0x463e90: UVScroll on a model's vertices. Its batches are found at
            // the first draw, like the lights' below.
            {
                const auto walk=[&](const auto& self,const Node& n)->void {
                    for(const Node& a:n.actions) if(a.class_name=="UVScroll" && a.integer(0xeb)!=0) {
                        Scroll u;u.node=n.id;u.along_u=a.integer(0xea)==1;u.units=a.integer(0xeb);
                        scrolls_.push_back(std::move(u));
                    }
                    for(const Node& c:n.children) self(self,c);
                };
                walk(walk,course.root());
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
            // (WExplosion's GD3dSprites 0x3ac and 0x3af are hidden, flag 0x100:
            // the action draws its own disc, see effects.)
            if(kids[3].class_name=="WKBumm") bumm_frames_=film(kids[3].link(0x35,"FILM"));
            // WackyAutoBomba's m_smokeparticle (0x153): a GDStdBrick whose first
            // child is the flamesmoke GD3DFilm (general.wgm 0x1be, 0x1bf).
            if(const Node* car=find_class(shared.root(),"WackyAuto")) for(const Node& a:car->actions) if(a.class_name=="WackyAutoBomba") {
                const Node& brick=shared.find(uint32_t(a.integer(0x153)));
                if(brick.children.empty() || brick.children[0].class_name!="GD3DFilm") bad("m_smokeparticle has no GD3DFilm");
                hit_smoke_frames_=film(brick.children[0].link(0x35,"FILM"));
                // m_BoomerangHit (0x152): the GDModel types 0x10/0x100 spin.
                const Node& spin=shared.find(uint32_t(a.integer(0x152)));
                WorldData d=load_model_group(archives,spin);upload(d.textures);spin_hit_=std::move(d.batches);
                spin_hit_basis_=node_transform(spin);
                // m_bluepool (0x154): the GDModel of the paint hit (type 4).
                WorldData pool=load_model_group(archives,shared.find(uint32_t(a.integer(0x154))));upload(pool.textures);paint_pool_=std::move(pool.batches);
                // FlyPower's template: vehicle 3's wings are member 0x292's first child.
                if(const Node* fly=[&]()->const Node* {
                       std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                           for(const auto& ac:n.actions) if(ac.class_name=="FlyPower") return &ac;
                           for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                           return nullptr;};
                       return f(shared.root());}()) {
                    const Node& brick=shared.find(uint32_t(fly->integer(0x292)));
                    if(!brick.children.empty()) fly_wings_=load_animated(archives,brick.children[0]);
                    const Node& v4=shared.find(uint32_t(fly->integer(0x290)));
                    if(v4.class_name=="GDHModel") fly_v4_=load_animated(archives,v4);
                    // Vehicle 5 (0x4d5602): member 0x293, a GDHModel cloned itself.
                    const Node& chute=shared.find(uint32_t(fly->integer(0x293)));
                    if(chute.class_name=="GDHModel") {fly_chute_=load_animated(archives,chute);fly_chute_local_=node_transform(chute);}
                    // Vehicle 6 (0x4d523f): member 0x291's brick and its GDHModel child.
                    const Node& prop=shared.find(uint32_t(fly->integer(0x291)));
                    for(const Node& k:prop.children) if(k.class_name=="GDHModel") {
                        fly_prop_=load_animated(archives,k);
                        fly_prop_brick_=node_transform(prop);fly_prop_local_=node_transform(k);
                        for(const Node& a:k.actions) if(a.class_name=="WRot") {
                            if(a.integer(0x1d5)!=2) bad("propeller WRot is not type 2");
                            for(size_t i=0;i<3;++i) {fly_prop_step_[i]=a.real(0x1d2+uint32_t(i));if(!std::isfinite(fly_prop_step_[i])) bad("nonfinite propeller WRot step");}
                        }
                        if(fly_prop_.mesh.segments.size()<4) bad("propeller GDHModel has fewer than 4 segments");
                        break;
                    }
                }
                // m_TankGunHit (0x14f, +0x24): a GDModel whose child GDHModel
                // the type 2 hit poses (0x47e60d).
                const Node& tank=shared.find(uint32_t(a.integer(0x14f)));
                if(tank.children.empty() || tank.children[0].class_name!="GDHModel") bad("m_TankGunHit has no GDHModel");
                tank_hit_=load_animated(archives,tank.children[0]);
                // m_Particle (0x151, +0x34): the GDModel of the tank hit's chunks.
                WorldData chunk=load_model_group(archives,shared.find(uint32_t(a.integer(0x151))));upload(chunk.textures);chunk_model_=std::move(chunk.batches);
                break;
            }
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
        // (0x39a, the breath, is a GD3DFilm, drawn with the films).
        for(const uint32_t id:{0x38cu,0x3a8u,0x3a1u}) {
            const Node& breath=shared.find(id);
            for(const auto& c:breath.children) if(c.class_name=="GDModel") {
                WorldData d=load_model_group(archives,c);upload(d.textures);projectiles_[id]=std::move(d.batches);
                break;
            }
        }
        for(const uint32_t id:{0x38cu,0x39au,0x3a1u,0x3a8u}) {
            const Node& breath=shared.find(id);
            for(const auto& a:breath.actions) if(a.class_name=="WRocketTail")
                tails_[id]={std::clamp(a.integer(0x2ec,16),2,16),a.integer(0x2ed,100),(a.integer(0x2f7)&1)!=0};
            if(id==0x39a) for(const auto& c:breath.children) if(c.class_name=="GD3DFilm") breath_film_=film(c.link(0x35,"FILM"));
        }
        // WPU_Breath's template (the brick holding the action): its GDModel
        // and GDHModel are cloned onto the car while the weapon runs.
        if(const Node* brick=[&]()->const Node* {
               std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                   for(const auto& a:n.actions) if(a.class_name=="WPU_Breath") return &n;
                   for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                   return nullptr;};
               return f(shared.root());}()) {
            for(const auto& c:brick->children) {
                if(c.class_name=="GDModel") {WorldData d=load_model_group(archives,c);upload(d.textures);breath_static_=std::move(d.batches);}
                else if(c.class_name=="GDHModel") breath_head_=load_animated(archives,c);
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
        // MonsterTires (0x469ebc: the wheels are the model root's first four
        // children; 0x4d6380: the template's four tyre GDModels).
        for(int v:vehicles) {
            if(wheel_masks_.count(v)) continue;
            const Node& root=shared.find(templates.vehicles.at(size_t(v)).model_ids[0]);
            std::set<uint32_t> ids;
            std::function<void(const Node&)> all=[&](const Node& n){ids.insert(n.id);for(const auto& c:n.children) all(c);};
            const auto& b=cars_.at(v).batches;
            std::array<Wheel,4> w;
            for(size_t k=0;k<4 && k<root.children.size();++k) {
                ids.clear();all(root.children[k]);
                w[k].mask.assign(b.size(),0);
                for(size_t i=0;i<b.size();++i) if(ids.count(b[i].node_id)) w[k].mask[i]=1;
                w[k].node=node_transform(root.children[k]);w[k].inverse=inverse_affine_matrix(w[k].node);
            }
            std::vector<char> m(b.size(),1);
            for(size_t k=0;k<4;++k) for(size_t i=0;i<w[k].mask.size();++i) if(w[k].mask[i]) m[i]=0;
            wheel_masks_.emplace(v,std::move(m));wheels_.emplace(v,std::move(w));
        }
        if(boot_.batches.empty()) {
            std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                for(const auto& ac:n.actions) if(ac.class_name=="WPU_BootKicker") return &n;
                for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                return nullptr;};
            if(const Node* brick=f(shared.root())) for(const auto& c:brick->children) if(c.class_name=="GDHModel") {
                boot_=load_animated(archives,c);boot_local_=node_transform(c);break;
            }
        }
        if(flipper_.batches.empty()) {
            std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                for(const auto& ac:n.actions) if(ac.class_name=="WPU_Flipper") return &n;
                for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                return nullptr;};
            if(const Node* brick=f(shared.root())) for(const auto& c:brick->children) if(c.class_name=="GDHModel") {flipper_=load_animated(archives,c);break;}
        }
        // WBeeHive's members on the hive brick (0x36c): m_BeeNum 0x129,
        // m_Radius 0x12a, m_Speed 0x12b.
        {
            std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                for(const auto& ac:n.actions) if(ac.class_name=="WBeeHive") return &ac;
                for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                return nullptr;};
            if(const Node* a=f(shared.root())) {
                bee_num_=a->integer(0x129,32);bee_radius_=a->integer(0x12a,150);bee_speed_=a->integer(0x12b,60);
                if(bee_num_<0 || bee_num_>256) bad("implausible WBeeHive m_BeeNum");
            }
        }
        if(tyres_[0].empty()) {
            std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                for(const auto& ac:n.actions) if(ac.class_name=="MonsterTires") return &n;
                for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                return nullptr;};
            if(const Node* brick=f(shared.root())) {
                for(const auto& ac:brick->actions) if(ac.class_name=="MonsterTires") tyre_radius_=ac.real(0x295,50);
                size_t k=0;
                for(const auto& c:brick->children) if(c.class_name=="GDModel" && k<4) {
                    WorldData d=load_model_group(archives,c);upload(d.textures);tyres_[k++]=std::move(d.batches);
                }
            }
        }
        // Controller 0's body (0x440be0 on the car's GDHModel): the group's first
        // GDHModel (vehicle 3: 0x8d), at the transform WorldBuilder gives it
        // (the root's own left out, each child's node transform composed).
        for(int v:vehicles) {
            if(bodies_.count(v)) continue;
            const Node& root=shared.find(templates.vehicles.at(size_t(v)).model_ids[0]);
            std::function<bool(const Node&,const Matrix4&)> find=[&](const Node& n,const Matrix4& at)->bool {
                const auto link=n.properties.find(0x67);
                if(n.class_name=="GDHModel" && link!=n.properties.end() && link->second.size()==8 && !std::memcmp(link->second.data(),"GHMS",4)) {
                    CarBody b;b.node=n.id;b.chain=at;b.model=load_animated(archives,n);
                    const auto& all=cars_.at(v).batches;
                    b.mask.assign(all.size(),1);
                    for(size_t i=0;i<all.size();++i) if(all[i].node_id==n.id) b.mask[i]=0;
                    bodies_.emplace(v,std::move(b));
                    return true;
                }
                for(const auto& c:n.children) if(find(c,multiply_matrix(at,node_transform(c)))) return true;
                return false;
            };
            find(root,identity_matrix());
        }
        // The body's first GDHModel and its segment 1 at rest (no rotation,
        // the SEGS translations along its parents): where WPU_Breath's clone
        // sits (0x4dacba: [car +0x24f] +0x15e -> +0xc is the posed segment
        // array, 0x34 bytes a segment, so +0x34 is segment 1; read under Wine).
        // The body's procedural pose (controller 0) is not ported: rest pose.
        for(int v:vehicles) {
            if(body_segment_.count(v)) continue;
            const Node& root=shared.find(templates.vehicles.at(size_t(v)).model_ids[0]);
            std::function<bool(const Node&,const Matrix4&)> find=[&](const Node& n,const Matrix4& at) {
                const auto link=n.properties.find(0x67);
                if(n.class_name=="GDHModel" && link!=n.properties.end() && link->second.size()==8 && !std::memcmp(link->second.data(),"GHMS",4)) {
                    uint32_t id;std::memcpy(&id,link->second.data()+4,4);
                    const Mesh mesh=load_mesh(archives,"GHMS",id);
                    Matrix4 seg=identity_matrix();
                    for(uint32_t k=1;k<mesh.segments.size() && k!=UINT32_MAX;k=mesh.segments[k].parent) {
                        seg[12]+=mesh.segments[k].x;seg[13]+=mesh.segments[k].y;seg[14]+=mesh.segments[k].z;
                        if(mesh.segments.size()<2) break;
                    }
                    // Wine (cc_t1, vehicle 3): the record at +0x34 holds segment 1's
                    // posed matrix (identity rotation, (12.3, 8.1, 4.9)), and it is
                    // multiplied by car +0x14, the model root (the actor + 20 up),
                    // not by the body's own frame.
                    (void)at;body_segment_[v]=seg;
                    return true;
                }
                for(const auto& c:n.children) if(find(c,multiply_matrix(at,node_transform(c)))) return true;
                return false;
            };
            if(!find(root,identity_matrix())) body_segment_[v]=identity_matrix();
        }
        // Shared WFollowCamera_2 parent camera: FOV 55 degrees, near 50, far 20000.
        const Node& follow=shared.find(templates.camera_ids[0]);
        projection_=camera_projection(follow);
        // 0x4ae060: linear fog to the camera far plane, colored by the viewport
        // background (Glide 0x433930 path likewise). With table and W fog in the
        // device's raster caps (+0x44 bits 0x100, 0x100000; 0x4b1c65) it is
        // table fog from far - 3000 (0x514d7c); otherwise vertex fog from
        // far x 0.5. Wine reports both caps: dr_t2's tunnel, 11000-14000 away,
        // stays black there. The native fog is per pixel by clip w, like W fog.
        fog_end_=follow.real(0x2f);fog_start_=fog_end_-3000.f;
        const uint8_t pixel[]={255,255,255,255};
        white_=wpc_vk_texture_create(window_,1,1,pixel);
        if(!white_) bad("flat material upload failed");
        owned_.push_back(white_);
        // The flake texture (renderer +0x23969 -> +0x18, 0x4b6840) is never
        // written: renderer 1 is a zeroed global (0x26fae78) and no code stores
        // that field, so 0x4b6888 sets no texture (SetTexture(0, NULL)) and the
        // FVF 0x102 vertices carry no colour: plain white squares.
        flake_=white_;

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
        {
            std::function<const Node*(const Node&)> f=[&](const Node& n)->const Node* {
                for(const auto& ac:n.actions) if(ac.class_name=="WPU_FireSiren") return &n;
                for(const auto& c:n.children) if(const Node* r=f(c)) return r;
                return nullptr;};
            if(const Node* brick=f(shared.root())) for(const auto& c:brick->children) if(c.class_name=="GDSprite") {
                siren_=sprite(c.link(0x9e,"GMAT"));siren_centre_={{c.integer(0xa1,16),c.integer(0xa2,16)}};break;
            }
        }
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
        // The objects ported since WRolling: each group of batches drawn by submit
        // must be registered here, or submit's lookup throws.
        for(const auto& c:chisels_) for(const auto& p:c) group(p);
        for(const auto& r:rocks_) for(const auto& p:r) group(p);
        for(const auto& j:jams_) for(const auto& p:j) group(p);
        for(const auto& b:bumps_) for(const auto& p:b) group(p);
        for(const auto& p:spins_) group(p);
        for(const auto& w:saws_) for(const auto& p:w) group(p);
        for(const auto& c:cranes_) for(const auto& p:c) group(p);
        for(const auto& r:shifted_) for(const auto& p:r) group(p);
        for(const auto& g:golden_) for(const auto& p:g) group(p);
        for(const auto& d:dicks_) for(const auto& p:d) group(p);
        for(const auto& r:logs_) for(const auto& l:r) group(l);
        for(const auto& b:bridges_) for(const auto& h:b) group(h);
        for(const auto& g:geysers_) for(const auto& h:g) group(h);
        for(const auto& d:holes_) for(const auto& h:d) group(h);
        for(const auto& [v,c]:cars_) {group(c.batches);group(shadows_.at(v).batches);}
        group(splash_column_);group(splash_ring_);group(ripple_);group(landmine_);group(tnt_);group(detour_);group(oil_);group(wall_);group(tree_);group(hive_);
        for(const auto& [id,b]:projectiles_) group(b);
        // Each batch's sections: those whose enabling (0x4a8680) activates its
        // node (RaceTrack::node_sections); none: always drawn. Its group for the
        // loop seam is that section (one per node on the shipped courses;
        // -1 when none, the start banner: group 0xffff, Wine dr_t3 #728).
        {
            const RaceTrack& track=session.track();
            batch_sections_.assign(world_.batches.size(),0);
            batch_group_.assign(world_.batches.size(),-1);
            for(size_t i=0;i<world_.batches.size();++i) {
                const std::vector<int>& in=track.node_sections(world_.batches[i].node_id);
                for(int s:in) if(s>=0 && s<32) batch_sections_[i]|=1u<<s;
                if(!in.empty()) batch_group_[i]=in.front();
            }
            drawn_.assign(world_.batches.size(),1);
            for(size_t k=0;k<2;++k) seam_[k].assign(world_.batches.size(),0);
        }
        wpc_log("Race view: %zu world batches, %zu world vertices, %zu car models, %zu textures, %zu translucent models",
                world_.batches.size(),world_.vertex_count,cars_.size(),textures_.size(),translucent);
        world_.textures.clear();
    } catch(...) {release();throw;}
}
void RaceView::dim(Image& image) const {
    // The divisors doubled: each colour channel at half (the original then
    // quantises to its 16-bit formats: 255 -> 123 under Wine; native 127).
    if(!rain_dim_) return;
    for(size_t i=0;i+3<image.rgba.size();i+=4) for(size_t k=0;k<3;++k) image.rgba[i+k]=uint8_t(image.rgba[i+k]>>1);
}
void RaceView::upload(std::map<uint32_t,Image>& images) {
    for(auto& [id,image]:images) {
        texture_sizes_[id]={{image.width,image.height}};
        if(textures_.count(id)) continue;
        dim(image);
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
        for(const auto& field:general_.children(general_.find("GMAT",material))) {
            const ByteView b=general_.payload(field);
            if(field.tag=="SMOO") g.smooth=b.size>=4 && word(b,0)!=0;
            if(field.tag=="SILL") g.sill=b.size>=4 && (word(b,0)&0x7fffffffu)!=0; // a float, +0x5c
        }
        g.film=true;
        g.texture=s.texture;g.width=s.width;g.height=s.height;
        frames.push_back(g);
    }
    return frames;
}
RaceView::AnimatedModel RaceView::load_animated(const ArchivePool& archives,const Node& node) {
    AnimatedModel m;
    const auto link=node.properties.find(0x67);
    if(link==node.properties.end() || link->second.size()!=8 || std::memcmp(link->second.data(),"GHMS",4))
        bad("animated model needs a GHMS link: "+std::to_string(node.id));
    uint32_t id;std::memcpy(&id,link->second.data()+4,4);
    m.mesh=load_mesh(archives,"GHMS",id);
    WorldData d=load_model_group(archives,node);upload(d.textures);m.batches=std::move(d.batches);
    // load_model_group's order: per triangle, its material's batch (first
    // seen order), three vertices each.
    std::map<uint32_t,uint32_t> index;std::vector<uint32_t> fill(m.batches.size(),0);
    for(const auto& t:m.mesh.triangles) {
        auto found=index.find(t.material);
        if(found==index.end()) found=index.emplace(t.material,uint32_t(index.size())).first;
        const uint32_t b=found->second;
        if(b>=m.batches.size() || m.batches[b].material_id!=t.material) bad("animated model batches do not follow its triangles");
        for(uint32_t v:{t.a,t.b,t.c}) m.corners.push_back({b,fill[b]++,v});
    }
    for(size_t b=0;b<m.batches.size();++b) if(fill[b]!=m.batches[b].vertices.size()) bad("animated model batch sizes differ");
    const auto motion=node.properties.find(0x65);
    if(motion!=node.properties.end() && motion->second.size()==8 && !std::memcmp(motion->second.data(),"MTNL",4)) {
        uint32_t mid;std::memcpy(&mid,motion->second.data()+4,4);
        auto lib=motions_.find(mid);
        if(lib==motions_.end()) lib=motions_.emplace(mid,parse_motion(archives,mid)).first;
        if(lib->second.segments!=m.mesh.segments.size()) bad("motion segment count differs from its mesh");
        m.lib=&lib->second;
    }
    return m;
}
void RaceView::pose(AnimatedInstance& a) {
    const uint64_t now=session_.updates();
    const int steps=a.posed==0?1:int(std::min<uint64_t>(now-a.posed,5));
    a.posed=now;
    if(steps<=0) return;
    MotionPairs pairs(a.model.mesh.segments.size()*9,{0.f,0.f});
    for(int i=0;i<steps;++i) {
        std::fill(pairs.begin(),pairs.end(),std::array<float,2>{0.f,0.f});
        if(a.controller.active()) a.controller.update(10,&pairs);
    }
    for(const auto& [seg,sc]:a.overrides) if(seg<a.model.mesh.segments.size()) for(size_t i=0;i<3;++i) pairs[seg*9+6+i]={-1.f,sc[i]-1};
    for(const auto& [seg,t]:a.moves) if(seg<a.model.mesh.segments.size()) for(size_t i=0;i<3;++i) pairs[seg*9+i]={-1.f,t[i]};
    apply_pose(a,pairs);
}
void RaceView::apply_pose(AnimatedInstance& a,const MotionPairs& pairs) {
    pose_mesh(a.model.mesh,pairs,&a.segments);
    const Matrix4 w=affine_matrix(a.model.mesh.matrix);
    for(const auto& c:a.model.corners) {
        const MeshVertex& v=a.model.mesh.vertices[c.vertex];
        auto& out=a.model.batches[c.batch].vertices[c.slot];
        out.x=w[0]*v.x+w[4]*v.y+w[8]*v.z+w[12];
        out.y=w[1]*v.x+w[5]*v.y+w[9]*v.z+w[13];
        out.z=w[2]*v.x+w[6]*v.y+w[10]*v.z+w[14];
    }
}
RaceView::Sprite RaceView::sprite(uint32_t material) {
    const uint32_t id=general_.material_texture(material);
    Image image=general_.texture(id);
    Sprite s;s.width=int(image.width);s.height=int(image.height);
    if(image.rgba.empty()) return s;
    auto found=textures_.find(id);
    if(found==textures_.end()) {
        dim(image);
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
void RaceView::submit(const std::vector<WorldBatch>& batches,const Matrix4& vp,const Matrix4& view,const Matrix4& model,const std::vector<char>* mask) {
    const Matrix4 mvp=multiply_matrix(vp,model);
    wpc_vk_set_mvp(window_,mvp.data());
    for(size_t i=0;i<batches.size();++i) if(const auto& b=batches[i];!b.translucent && !b.hidden && (!mask || (*mask)[i]))
        wpc_vk_draw_material_triangles(window_,texture(b.material),b.vertices.data(),uint32_t(b.vertices.size()),
                                       b.material.clamp_axes,b.material.two_sided?1:0);
    const Matrix4 mv=multiply_matrix(view,model);
    // A group not registered at load (a new object's) is registered here rather
    // than failing the frame.
    auto found=translucent_.find(&batches);
    if(found==translucent_.end()) found=translucent_.emplace(&batches,translucent_models(batches)).first;
    for(const auto& m:found->second) {
        if(mask && std::none_of(m.batches.begin(),m.batches.end(),[&](size_t b){return (*mask)[b]!=0;})) continue;
        const auto& o=m.origin;
        queue_.add(m.draw_flags,mv[2]*o[0]+mv[6]*o[1]+mv[10]*o[2]+mv[14],queued_.size());
        queued_.push_back({&batches,&m,mvp,mask});
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
void RaceView::body_gadgets(int car,CarSway& w) {
    // TurboSpeed (init table 0x4d70a8, end table 0x4d7378) and FlyPower (init
    // table 0x4d5354, update 0x4d5b8c, end 0x4d5ba8) on the body [car +0x24f].
    const int effect=session_.gadget_effect(car),fly=session_.flying(car),v=w.vehicle;
    MotionController& clip=w.body->controller;
    const bool clips=w.body->model.lib!=nullptr;
    const auto set_scale=[&](size_t seg,std::array<float,3> s) {   // 0x437f70(seg, s, 1)
        w.locked=true;
        for(auto& l:w.scale_locks) if(l.first==seg) {l.second=s;return;}
        w.scale_locks.push_back({seg,s});
    };
    const auto set_turn=[&](size_t seg,std::array<float,3> r) {    // 0x4383b0(seg, r, 1)
        w.locked=true;
        for(auto& l:w.turn_locks) if(l.first==seg) {l.second=r;return;}
        w.turn_locks.push_back({seg,r});
    };
    const auto unlock=[&]{w.locked=false;w.scale_locks.clear();w.turn_locks.clear();};   // flag -1: +0x17a bit 0 cleared
    if(effect==8 && w.last_effect!=8) {
        if(v==1) set_scale(1,{1,1,1});            // 0x4d6ff4 (and the action's +0x70 = 1, not traced)
        if(v==4 && clips) clip.play(0);           // 0x4d6fcf: 0x440970(0, 0, 1, -1)
    }
    if(w.last_effect==8 && effect!=8 && v==4 && clips) clip.play(2);   // 0x4d7295
    if(fly>0 && w.last_fly==0) {
        if((v==0 || v==1) && clips) clip.play(0); // 0x4d4f26, 0x4d520f
        if(v==5) set_turn(6,{35,0,0});            // 0x4d504b
    }
    if(fly>0 && v==2) set_turn(5,{0,0,153.f*float(fly)});   // 0x4d5309, 0x4d59ff: 153 x +0x64 degrees
    if(fly==0 && w.last_fly>0 && (v==2 || v==5)) unlock();  // 0x4d5b20, 0x4d5aae
    // 0x469a84: vehicle 5's turret, body segment 2 turned about y by +0x3ab
    // in degrees (0x512a20), locked each update (0x4383b0(2, 0, a, 0, 1)).
    if(v==5) set_turn(2,{0,session_.turret(car)*57.29578f,0});
    // WPU_Paintball: body clip 2 at its init (0x4db2cf: 0x440970(2, 0, 1,
    // -1)), frozen at its end when the gun stops (0x4db3cb: command 2).
    const int weapon=session_.running_weapon(car);
    if(weapon==13 && w.last_weapon!=13 && clips) clip.play(2);
    if(w.last_weapon==13 && weapon!=13 && clips) clip.freeze_at_end();
    w.last_effect=effect;w.last_fly=fly;w.last_weapon=weapon;
}
void RaceView::submit_car(int car,int vehicle,const BombHit& hit,const Matrix4& vp,const Matrix4& view,const Matrix4& frame) {
    Matrix4 model=initial_vehicle_model_frame(frame);
    // MonsterTires (0x4d68a6): the model's height +0x92f = (s - 1) x
    // m_tireradius x 0.5 + 20, at least 20.
    const float tyre_scale=session_.monster_scale(car);
    if(tyre_scale>0) {
        const float lift=std::max(0.f,(tyre_scale-1)*tyre_radius_*0.5f);
        for(size_t r=0;r<3;++r) model[12+r]+=lift*frame[4+r];
    }
    if(hit.active() && hit.scale_y!=1) for(size_t i=4;i<8;++i) model[i]*=hit.scale_y; // the local up axis
    const auto& batches=cars_.at(vehicle).batches;
    const float f=hit.active()?hit.tint.f:0;
    // The body GDHModel swayed by controller 0, stepped once per race update
    // (the original steps it per draw, 0x437d00; provisional), from the
    // world matrix it is drawn with.
    const std::vector<char>* mask=nullptr;
    const auto body=bodies_.find(vehicle);
    if(body!=bodies_.end()) {
        CarSway& w=sways_[car];
        static const MotionLib none;
        if(!w.body || w.vehicle!=vehicle) {
            if(w.body) {translucent_.erase(&w.body->model.batches);translucent_.erase(&w.tinted);}
            w=CarSway{};w.vehicle=vehicle;w.sway=SwayController(body_sway_gains(vehicle));
            w.body=std::make_unique<AnimatedInstance>(body->second.model,body->second.model.lib?*body->second.model.lib:none);
        }
        const Matrix4 world=multiply_matrix(model,body->second.chain);
        const uint64_t now=session_.updates();
        if(w.stepped!=now) {
            const int steps=w.stepped==~uint64_t(0)?1:int(std::min<uint64_t>(now-w.stepped,5));
            MotionPairs pairs(w.body->model.mesh.segments.size()*9,{0.f,0.f});
            body_gadgets(car,w);
            for(int i=0;i<steps;++i) {
                std::fill(pairs.begin(),pairs.end(),std::array<float,2>{0.f,0.f});
                w.sway.update(world,pairs);
                if(w.body->controller.active()) w.body->controller.update(10,&pairs);
            }
            if(w.locked) {
                const size_t n=w.body->model.mesh.segments.size();
                for(const auto& [seg,v]:w.scale_locks) if(seg<n) for(size_t k=0;k<3;++k) pairs[seg*9+6+k]={-1.f,v[k]-1};
                for(const auto& [seg,v]:w.turn_locks) if(seg<n) for(size_t k=0;k<3;++k) pairs[seg*9+3+k]={-1.f,v[k]};
            }
            apply_pose(*w.body,pairs);
            w.stepped=now;
        }
        mask=&body->second.mask;
        const std::vector<WorldBatch>* drawn=&w.body->model.batches;
        if(f>0) {
            // 0x47d890 rewrites the body's vertex colours too.
            if(w.tinted.size()!=w.body->model.batches.size()) {translucent_.erase(&w.tinted);w.tinted=w.body->model.batches;}
            for(size_t i=0;i<w.tinted.size();++i) {
                const auto& in=w.body->model.batches[i].vertices;auto& out=w.tinted[i].vertices;
                for(size_t k=0;k<in.size() && k<out.size();++k) {
                    out[k]=in[k];
                    const std::array<float,3> c=tint_colour(hit.tint,{{in[k].r,in[k].g,in[k].b}});
                    out[k].r=c[0];out[k].g=c[1];out[k].b=c[2];
                }
            }
            drawn=&w.tinted;
        }
        submit(*drawn,vp,view,world);
    }
    // The wheels (+0x273..+0x27f) are drawn apart from the rest of the model.
    const std::vector<char>* const mask_in=mask;
    const float spin=session_.wheel_spin(car);
    const auto turn_x=[](Matrix4& m,float a) {   // 0x405e8e -> 0x43b2b0 on the axes
        const float c=std::cos(a),s=std::sin(a);
        for(size_t r=0;r<3;++r) {const float y=m[4+r],z=m[8+r];m[4+r]=c*y-s*z;m[8+r]=s*y+c*z;}
    };
    {
        auto& m=masks_[car];
        m=wheel_masks_.at(vehicle);
        if(mask) for(size_t i=0;i<m.size() && i<mask->size();++i) m[i]=m[i]&&(*mask)[i];
        mask=&m;
    }
    std::array<Matrix4,4> wheel_at{};
    if(tyre_scale>0) {
        // The wheels hidden (flag 0x100, 0x4d648a); the tyres take the car's
        // frame (0x404def) with, each, the scale s turned about x by the
        // wheel spin +0x2a4 x 2.5 (0x51629c) at m_monstertires11/12/21/22's
        // translation (0x4d671d..0x4d68a1).
        const auto at=vehicle_wheels(vehicle);
        for(size_t k=0;k<4;++k) {
            if(tyres_[k].empty()) continue;
            Matrix4 t=identity_matrix();
            for(size_t a=0;a<3;++a) t[a*4+a]=tyre_scale;
            turn_x(t,spin*2.5f);
            t[12]=at[k][0];t[13]=0;t[14]=at[k][1];
            submit(tyres_[k],vp,view,multiply_matrix(frame,t));
        }
    } else {
        // 0x46b15b..: wheel 0 takes 0x43c250(+0x2a4, steer +0x403, 0) at its
        // translation, wheel 1 a copy of wheel 0's axes; wheels 2 and 3 the
        // identity turned about x by +0x2a4 (0x405e8e).
        const float sa=std::sin(spin),ca=std::cos(spin),st=session_.steer(car),sb=std::sin(st),cb=std::cos(st);
        Matrix4 front=identity_matrix();   // 0x43c250(a, b, 0): rows x, y, z
        front[0]=cb;front[1]=0;front[2]=sb;
        front[4]=sb*sa;front[5]=ca;front[6]=-cb*sa;
        front[8]=-sb*ca;front[9]=sa;front[10]=cb*ca;
        Matrix4 rear=identity_matrix();turn_x(rear,spin);
        for(size_t k=0;k<4;++k) wheel_at[k]=k<2?front:rear;
    }
    const auto draw_wheels=[&](const std::vector<WorldBatch>& bs) {
        if(tyre_scale>0) return;
        const auto& w=wheels_.at(vehicle);
        auto& dm=wheel_draw_masks_[car];
        for(size_t k=0;k<4;++k) {
            dm[k]=w[k].mask;
            if(mask_in) for(size_t i=0;i<dm[k].size() && i<mask_in->size();++i) dm[k][i]=dm[k][i]&&(*mask_in)[i];
            Matrix4 l=wheel_at[k];
            for(size_t r=0;r<3;++r) l[12+r]=w[k].node[12+r];
            submit(bs,vp,view,multiply_matrix(model,multiply_matrix(l,w[k].inverse)),&dm[k]);
        }
    };
    if(f<=0) {submit(batches,vp,view,model,mask);draw_wheels(batches);return;}
    // 0x47d890 rewrites every vertex colour of the car model (+0x3e, count
    // +0x42), its translucent parts too (Wine, cc_t1, type 0x80: the whole
    // car turns black). The copy is drawn like the model itself.
    TintedModel& t=tinted_models_[&hit];
    if(t.vehicle!=vehicle || t.batches.size()!=batches.size()) {
        translucent_.erase(&t.batches);
        t.vehicle=vehicle;t.batches=batches;
        translucent_[&t.batches]=translucent_models(t.batches);
    }
    for(size_t i=0;i<batches.size();++i) {
        const auto& in=batches[i].vertices;auto& out=t.batches[i].vertices;
        for(size_t k=0;k<in.size() && k<out.size();++k) {
            const std::array<float,3> c=tint_colour(hit.tint,{{in[k].r,in[k].g,in[k].b}});
            out[k].r=c[0];out[k].g=c[1];out[k].b=c[2];
        }
    }
    submit(t.batches,vp,view,model,mask);
    draw_wheels(t.batches);
}
void RaceView::submit_breath_head(int car,const RaceCar& c,const Matrix4& vp,const Matrix4& view,const Matrix4& placed) {
    // WPU_Breath (init 0x4dab40, update 0x4dac70): while its counter (+0x38,
    // 33 down to 0) runs, the template's clone sits on the body's segment
    // frame times the car model's frame, and its GDHModel plays clip 2
    // (0x440970(2, 0, 1, -1)), chained by the MTNL.
    const bool on=session_.running_weapon(car)==14 && session_.weapon_effect(car)==0xe && session_.weapon_timer(car)>=0;
    auto found=breath_heads_.find(car);
    if(!on) {
        if(found!=breath_heads_.end()) {translucent_.erase(&found->second->model.batches);breath_heads_.erase(found);}
        return;
    }
    if(!breath_head_.lib || breath_head_.batches.empty()) return;
    if(found==breath_heads_.end()) {
        found=breath_heads_.emplace(car,std::make_unique<AnimatedInstance>(breath_head_,*breath_head_.lib)).first;
        found->second->controller.play(2);
    }
    AnimatedInstance& a=*found->second;
    pose(a);
    // 0x4dac70: the body's posed segment 1 ([car +0x24f] +0x15e -> +0xc, +0x34)
    // when its sway instance is drawn, else segment 1 at rest.
    const auto seg=body_segment_.find(c.vehicle);
    Matrix4 at=seg==body_segment_.end()?identity_matrix():seg->second;
    if(const auto w=sways_.find(car);w!=sways_.end() && w->second.body && w->second.vehicle==c.vehicle && w->second.body->segments.size()>1) {
        const auto& s1=w->second.body->segments[1];
        at=identity_matrix();
        for(size_t row=0;row<3;++row) for(size_t k=0;k<3;++k) at[row*4+k]=s1[row*3+k];
        for(size_t k=0;k<3;++k) at[12+k]=s1[9+k];
    }
    Matrix4 frame=multiply_matrix(placed,multiply_matrix(initial_vehicle_model_frame(c.frame()),at));
    // 0x4dacea: with the action's +0x5c (look-back held at the use) the frame
    // turns by pi about its y axis (0x403279), facing backward.
    if(session_.weapon_backward(car)) {Matrix4 r=identity_matrix();r[0]=r[10]=-1;frame=multiply_matrix(frame,r);}
    if(!breath_static_.empty()) submit(breath_static_,vp,view,frame);
    submit(a.model.batches,vp,view,frame);
    if(std::getenv("WPC_DEBUG_BREATH")) {
        float lo[3]={1e9f,1e9f,1e9f},hi[3]={-1e9f,-1e9f,-1e9f};size_t n=0;
        for(const auto& b:a.model.batches) for(const auto& v:b.vertices) {
            const float w[3]={frame[0]*v.x+frame[4]*v.y+frame[8]*v.z+frame[12],frame[1]*v.x+frame[5]*v.y+frame[9]*v.z+frame[13],frame[2]*v.x+frame[6]*v.y+frame[10]*v.z+frame[14]};
            for(int k=0;k<3;++k) {lo[k]=std::min(lo[k],w[k]);hi[k]=std::max(hi[k],w[k]);}++n;
        }
        wpc_log("Breath head car %d timer %d: %zu batches %zu vertices, car (%.0f %.0f %.0f), box x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f",car,session_.weapon_timer(car),
                a.model.batches.size(),n,c.position[0],c.position[1],c.position[2],lo[0],hi[0],lo[1],hi[1],lo[2],hi[2]);
    }
}
void RaceView::submit_spin_hit(const BombHit& hit,const RaceCar& car,const Matrix4& vp,const Matrix4& view,const Matrix4& placed) {
    if(hit.active() && hit.pool.shown && !paint_pool_.empty()) {
        // 0x47ea59: the car model's frame (as submit_car draws it) with its
        // axes scaled.
        Matrix4 m=initial_vehicle_model_frame(Matrix4(hit.pool.car));
        for(size_t a=0;a<3;++a) for(size_t r=0;r<3;++r) m[a*4+r]*=hit.pool.scale[a];
        submit(paint_pool_,vp,view,multiply_matrix(placed,m));
    }
    if(!hit.active() || !hit.spin.shown || spin_hit_.empty()) return;
    // 0x47ecb4..0x47ed05: the basis (template or identity) x scale, turned
    // about y (0x43b4a0: x' = c x + s z, z' = c z - s x), at the car's
    // position plus 120 in y.
    Matrix4 m=hit.spin.from_identity?identity_matrix():spin_hit_basis_;
    for(size_t i=0;i<12;++i) if(i%4!=3) m[i]*=hit.spin.scale;
    const float c=std::cos(hit.spin.turn),sn=std::sin(hit.spin.turn);
    for(size_t r=0;r<3;++r) {const float u=m[r],v=m[8+r];m[r]=c*u+sn*v;m[8+r]=c*v-sn*u;}
    m[12]=car.position[0];m[13]=car.position[1]+120;m[14]=car.position[2];m[15]=1;
    submit(spin_hit_,vp,view,multiply_matrix(placed,m));
}
void RaceView::submit_fly(int car,const RaceCar& c,const Matrix4& vp,const Matrix4& view,const Matrix4& placed) {
    // FlyPower, vehicle 3 (init 0x4d4e19, update 0x4d596f): a clone of the
    // template's +0x4c whose child (GDDoubleHModel 0x1cb) takes the local
    // matrix identity (0x43b090) x 2.5 (0x402027), turned 30 degrees about x
    // (0x43b2b0, 0x3f060a92), at (-30, 150, -10) (0x40150a), and plays clip 0
    // (0x405e1b(0, 0, 1, -1)). Each update the clone takes the car's local
    // matrix (+0x2c) and group. Vehicles 4 and 6: below. Others are not drawn.
    const AnimatedModel& model=c.vehicle==4?fly_v4_:c.vehicle==5?fly_chute_:c.vehicle==6?fly_prop_:fly_wings_;
    const int counter=session_.flying(car);   // FlyPower's +0x64
    const bool on=(c.vehicle==3 || c.vehicle==4 || c.vehicle==6 || (c.vehicle==5 && counter>=10)) && counter>0 && !model.batches.empty();
    auto found=fly_instances_.find(car);
    if(!on) {
        if(found!=fly_instances_.end()) {translucent_.erase(&found->second->model.batches);fly_instances_.erase(found);}
        if(const auto m=fly_mirrors_.find(car);m!=fly_mirrors_.end()) {translucent_.erase(&m->second);fly_mirrors_.erase(m);}
        fly_props_.erase(car);
        return;
    }
    static const MotionLib none;
    if(found==fly_instances_.end()) {
        found=fly_instances_.emplace(car,std::make_unique<AnimatedInstance>(model,model.lib?*model.lib:none)).first;
        AnimatedInstance& a=*found->second;
        if(c.vehicle==6) {
            // 0x4d52bb: segment 1 locked to the scale (0, 1, 0) (0x437f70, flag
            // 1); 0x4d52d5: clip 0 (0x440970(0, 0, 1, -1)); 0x4d52eb: command 2
            // on its controller (0x4409f0 -> 0x440290), frozen at the end.
            a.overrides={{1,{0.f,1.f,0.f}}};
            if(model.lib) {a.controller.play(0);a.controller.freeze_at_end();}
            fly_props_[car]=FlyProp{0,session_.updates(),{}};
        } else if(c.vehicle==5) {
            // 0x4d5660..0x4d568f: clip 0 (0x440970(0, 0, 1, -1)), frozen at
            // its end (0x4409f0(0, 2, -1)).
            if(model.lib) {a.controller.play(0);a.controller.freeze_at_end();}
        } else if(model.lib) a.controller.play(0);
    }
    AnimatedInstance& a=*found->second;
    if(c.vehicle==6) {
        // Update 0x4d56da. Before update 20 (0x4d5783), once clip 0 is frozen
        // (controller 1's +0x28 bit 4): the controller stops (bit 0 cleared),
        // segment 1 shows at full scale and segments 2 and 3 are squashed to
        // 0. At update 70 (0x4d57ef): bit 0 again, segment 1 back to (0, 1, 0),
        // 2 and 3 to 1, clip 2 plays (0x440970(2, 0, 1, -1)) frozen at its end.
        FlyProp& p=fly_props_[car];
        if(p.stage==0 && counter<20 && a.controller.frozen()) {
            a.controller.set_active(false);
            a.overrides={{1,{1.f,1.f,1.f}},{2,{0.f,0.f,0.f}},{3,{0.f,0.f,0.f}}};
            p.stage=1;
        }
        if(p.stage<2 && counter>=70) {
            a.controller.set_active(true);
            a.overrides={{1,{0.f,1.f,0.f}},{2,{1.f,1.f,1.f}},{3,{1.f,1.f,1.f}}};
            if(model.lib) {a.controller.play(2);a.controller.freeze_at_end();}
            p.stage=2;
        }
        // The child's WRot (0x497b00 type 2): m_StepRotDeg{X,Y,Z} added each
        // update, wrapped to [0, 2 pi), turning its saved local frame.
        const uint64_t now=session_.updates();
        for(;p.spun<now;++p.spun) for(size_t k=0;k<3;++k) {
            p.angle[k]+=fly_prop_step_[k];
            if(p.angle[k]>6.2831855f) p.angle[k]-=6.2831855f;
            if(p.angle[k]<0) p.angle[k]+=6.2831855f;
        }
        pose(a);
        Matrix4 l=fly_prop_local_;
        const auto turn=[&](size_t i,size_t j,float t){
            if(t==0) return;
            const float cs=std::cos(t),sn=std::sin(t);
            for(size_t r=0;r<3;++r) {const float u=l[i*4+r],v=l[j*4+r];l[i*4+r]=cs*u+sn*v;l[j*4+r]=cs*v-sn*u;}
        };
        turn(2,1,p.angle[0]);turn(0,2,p.angle[1]);turn(1,0,p.angle[2]);
        // 0x4d56fd..0x4d5741: the brick keeps its own axes, at the car's
        // position (+0x288) plus 100 (0x510414) in y.
        Matrix4 b=fly_prop_brick_;
        b[12]=c.position[0];b[13]=c.position[1]+100;b[14]=c.position[2];
        submit(a.model.batches,vp,view,multiply_matrix(placed,multiply_matrix(b,l)));
        return;
    }
    pose(a);
    if(c.vehicle==5) {
        // Update 0x4d5555: the clone keeps its axes, at the car's position
        // (+0x288) plus 100 (0x510414) in y, as vehicle 6's rotor.
        Matrix4 m=fly_chute_local_;
        m[12]=c.position[0];m[13]=c.position[1]+100;m[14]=c.position[2];
        submit(a.model.batches,vp,view,multiply_matrix(placed,m));
        return;
    }
    if(c.vehicle==4) {
        // 0x4d5172 / 0x4d58a1: the GDHModel itself, clip 0, at the car's local
        // matrix with its translation 160 (0x512a90) higher.
        Matrix4 m=c.frame();m[13]+=160;
        submit(a.model.batches,vp,view,multiply_matrix(placed,m));
        return;
    }
    Matrix4 l=identity_matrix();
    for(size_t ax=0;ax<3;++ax) for(size_t r=0;r<3;++r) l[ax*4+r]*=2.5f;
    {
        const float cs=std::cos(0.5235988f),sn=std::sin(0.5235988f);
        for(size_t r=0;r<3;++r) {const float y=l[4+r],z=l[8+r];l[4+r]=cs*y-sn*z;l[8+r]=sn*y+cs*z;}
    }
    l[12]=-30;l[13]=150;l[14]=-10;
    submit(a.model.batches,vp,view,multiply_matrix(placed,multiply_matrix(c.frame(),l)));
    // A GDDoubleHModel draws a second copy mirrored across the car's x axis
    // (Wine, cc_t1: two symmetric wings; the class's own draw is not traced):
    // x negated in the model, the triangles' order reversed, at x +30.
    auto& mirror=fly_mirrors_[car];
    mirror=a.model.batches;
    for(auto& b:mirror) {
        for(auto& v:b.vertices) v.x=-v.x;
        for(size_t t=0;t+2<b.vertices.size();t+=3) std::swap(b.vertices[t+1],b.vertices[t+2]);
    }
    Matrix4 r=l;r[12]=30;
    submit(mirror,vp,view,multiply_matrix(placed,multiply_matrix(c.frame(),r)));
}
void RaceView::submit_fires(int car,const RaceCar& c,const Matrix4& placed) {
    // Which WFire siblings of the body (car model 0x6b's 0x71/0x72, 0xd7's 0xdd,
    // 0xa0's 0xa9/0xaa, 0x86's 0x8c) are shown, with their members +0x11e
    // (length), +0x122 (width), +0x126 (spread) and local frame: turned about
    // x (0x402626 -> 0x43c250(a, 0, 0)) and moved (0x40150a).
    struct Fire {float x,y,z;int length,width,spread;float turn;};
    static const Fire none[1]={};
    const Fire* fires=none;size_t count=0;
    const int effect=session_.gadget_effect(car);
    if(effect==8) {
        // TurboSpeed's init by vehicle (table 0x4d70a8); its end (table
        // 0x4d7378) hides them again with the effect.
        static const Fire v0[2]={{-40,15,-140,150,18,80,3.1415927f},{37,15,-140,150,18,80,3.1415927f}};  // 0x4d6df5
        static const Fire v2[1]={{0,20,-120,100,40,80,3.1415927f}};                                         // 0x4d7022
        static const Fire v5[2]={{-50,55,-100,70,20,100,3.1415927f},{50,55,-100,70,20,100,3.1415927f}};   // 0x4d6ee2
        static const Fire v6[1]={{0,50,-170,100,40,80,3.1415927f}};                                         // 0x4d6d9e
        switch(c.vehicle) {
            case 0: fires=v0;count=2;break; case 2: fires=v2;count=1;break;
            case 5: fires=v5;count=2;break; case 6: fires=v6;count=1;break;
            default: break;
        }
    } else if(session_.flying(car)>0) {
        // FlyPower: vehicle 0 (0x4d4f26), vehicle 5 (0x4d504b, turned by
        // 0x405f66f3 = 3.4907); hidden by the ends 0x4d5a75 and 0x4d5aae.
        static const Fire v0[2]={{-55,30,-130,100,40,80,3.1415927f},{50,30,-130,100,40,80,3.1415927f}};
        static const Fire v5[2]={{-40,65,-80,100,40,80,3.4906585f},{40,65,-80,100,40,80,3.4906585f}};
        if(c.vehicle==0) {fires=v0;count=2;} else if(c.vehicle==5) {fires=v5;count=2;}
    }
    if(!count) return;
    const Matrix4 model=multiply_matrix(placed,initial_vehicle_model_frame(c.frame()));
    const auto rand15=[&]{fire_random_=fire_random_*214013u+2531011u;return int((fire_random_>>16)&0x7fff);};
    for(size_t k=0;k<count;++k) {
        const Fire& f=fires[k];
        Matrix4 local=identity_matrix();
        const float cs=std::cos(f.turn),sn=std::sin(f.turn);
        local[5]=cs;local[6]=-sn;local[9]=sn;local[10]=cs;   // 0x43c250 with (a, 0, 0)
        local[12]=f.x;local[13]=f.y;local[14]=f.z;
        const Matrix4 m=multiply_matrix(model,local);
        // 0x481390: W = rand % (spread / 16 + 1) + width, L = rand % spread +
        // length + 1; ten points (0x6f2908): the origin, (0, W/2, 0), (W/2, 0,
        // 0), (0, -W/2, 0), (-W/2, 0, 0), the ring (0, W), (W, 0), (0, -W),
        // (-W, 0) at L/4, and the tip (0, 0, L).
        const float W=float(rand15()%(f.spread/16+1)+f.width);
        const float L=float(rand15()%f.spread+f.length+1);
        const std::array<Vec3,10> p{{{{0,0,0}},{{0,W*0.5f,0}},{{W*0.5f,0,0}},{{0,-W*0.5f,0}},{{-W*0.5f,0,0}},
                                     {{0,W,L*0.25f}},{{W,0,L*0.25f}},{{0,-W,L*0.25f}},{{-W,0,L*0.25f}},{{0,0,L}}}};
        // The first corner's colour: red 255, green 128..255 (rand & 0x7f +
        // 0x80), blue 0, alpha 170..249 (rand % 80 + 0xaa); the others 0.
        const float g=float((rand15()&0x7f)+0x80)/255.f;
        const float a=float(rand15()%80+0xaa)/255.f;
        // The triangles (0x52b518): (0, 9, 5..8), (0, 1..4, 5..8).
        static const uint8_t tri[8][3]={{0,9,5},{0,9,6},{0,9,7},{0,9,8},{0,1,5},{0,2,6},{0,3,7},{0,4,8}};
        for(const auto& t:tri) for(size_t i=0;i<3;++i) {
            const Vec3& v=p[t[i]];
            WpcTexturedVertex3D w{};
            w.x=m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12];
            w.y=m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13];
            w.z=m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14];
            if(i==0) {w.r=1;w.g=g;w.b=0;w.a=a;}
            fire_tris_.push_back(w);
        }
    }
}
void RaceView::submit_kicks(const Matrix4& vp,const Matrix4& view,const std::function<Matrix4(int)>& place) {
    // 0x4d8710: the boot's GDHModel turned about y by +0x88 (0x43c380) under
    // the brick; its segment 1's scale locked at 1 (0x4d88b5) and its
    // translation at the extension +0x68 (0x4381a0(1, ext, 1)).
    const auto& kicks=session_.kicks();
    if(boot_.batches.empty()) return;
    static const MotionLib none;
    while(boots_.size()<kicks.size()) boots_.push_back(std::make_unique<AnimatedInstance>(boot_,boot_.lib?*boot_.lib:none));
    for(size_t i=0;i<kicks.size();++i) {
        const auto& k=kicks[i];
        AnimatedInstance& a=*boots_[i];
        a.overrides={{1,{1.f,1.f,1.f}}};a.moves={{1,{k.ext[0],k.ext[1],k.ext[2]}}};
        a.posed=0;pose(a);
        const RaceCar& user=k.owner<0?session_.player():session_.opponents().at(size_t(k.owner)).car;
        Matrix4 g=identity_matrix();   // 0x4d88fc: reset, then turned
        const float c=std::cos(k.angle),s=std::sin(k.angle);
        g[0]=c;g[1]=0;g[2]=s;g[4]=0;g[5]=1;g[6]=0;g[8]=-s;g[9]=0;g[10]=c;
        submit(a.model.batches,vp,view,multiply_matrix(place(user.section),multiply_matrix(k.brick,g)));
    }
}
void RaceView::submit_flips(const Matrix4& vp,const Matrix4& view,const std::function<Matrix4(int)>& place) {
    // 0x4d9a20: clip 0 (0x440970(0, 0, 1, -1)) at time 11 (0x4d9bff); in
    // state 1 the clip is held at 51 and frozen (command 3, 0x4da050), state 2
    // runs it on. 0x4d9890: the GDHModel turned about y by +0x7c (0x43c380);
    // segment 1 at (0, 0, reach) (0x4381a0), segment 2 scaled by min(1,
    // +0x80) and segment 4 by +0x80 (0x437f70).
    const auto& flips=session_.flips();
    if(flipper_.batches.empty()) return;
    static const MotionLib none;
    if(flip_arms_.size()>flips.size()) flip_arms_.resize(flips.size());
    while(flip_arms_.size()<flips.size()) {
        FlipArm arm;arm.a=std::make_unique<AnimatedInstance>(flipper_,flipper_.lib?*flipper_.lib:none);
        if(flipper_.lib) {arm.a->controller.play(0);arm.a->controller.set_time(11);}
        flip_arms_.push_back(std::move(arm));
    }
    for(size_t i=0;i<flips.size();++i) {
        const auto& f=flips[i];
        FlipArm& arm=flip_arms_[i];
        if(f.state==1) {arm.a->controller.set_time(51);if(!arm.frozen) {arm.a->controller.toggle_freeze();arm.frozen=true;}}
        else if(arm.frozen) {arm.a->controller.toggle_freeze();arm.frozen=false;}
        const float s2=f.scale<1?f.scale:1;
        arm.a->overrides={{2,{s2,s2,s2}},{4,{f.scale,f.scale,f.scale}}};
        arm.a->moves={{1,{0,0,f.reach}}};
        arm.a->posed=arm.a->posed?arm.a->posed:session_.updates();
        pose(*arm.a);
        const RaceCar& user=f.owner<0?session_.player():session_.opponents().at(size_t(f.owner)).car;
        Matrix4 g=identity_matrix();
        const float c=std::cos(f.angle),s=std::sin(f.angle);
        g[0]=c;g[2]=s;g[8]=-s;g[10]=c;
        submit(arm.a->model.batches,vp,view,multiply_matrix(place(user.section),multiply_matrix(f.brick,g)));
    }
}
void RaceView::submit_bees(const Matrix4& vp,const std::function<Matrix4(int)>& place) {
    (void)vp;
    std::map<int,bool> live;
    for(const auto& m:session_.mines()) {
        if(!m.hive || m.state!=3 || m.done) continue;
        live[m.victim]=true;
        Swarm& w=swarms_[m.victim];
        const auto rand15=[&]{fire_random_=fire_random_*214013u+2531011u;return int((fire_random_>>16)&0x7fff);};
        if(w.bees.empty()) {
            // 0x474580: each bee's phases rand() & 0xfff, both steps m_Speed.
            for(int i=0;i<bee_num_;++i) {const int a=rand15()&0xfff,b=rand15()&0xfff;w.bees.push_back({a,b,bee_speed_,bee_speed_});}
            w.stepped=session_.updates();
        }
        // 0x474600: each update the phases move on by their steps, wrapped at 0x1000.
        for(;w.stepped<session_.updates();++w.stepped) for(auto& b:w.bees) {b[0]=(b[0]+b[2])&0xfff;b[1]=(b[1]+b[3])&0xfff;}
        // 0x474850 (renderer 1): a bee at R (sin a cos b, -sin b, cos a cos b)
        // (the phases taken as radians), two triangles from it: to (+5, +-5)
        // in (13, 0, 0, 173) and to (-5, +-5) in (255, 255, 0, 173), the
        // centre (0, 0, 1, 255); drawn in the swarm's frame (here the trapped
        // car's, provisional) by 0x4b5550 mode 8.
        const RaceCar& v=m.victim<0?session_.player():session_.opponents().at(size_t(m.victim)).car;
        const Matrix4 f=multiply_matrix(place(v.section),v.frame());
        const float R=float(bee_radius_),h=5;
        const auto vertex=[&](float x,float y,float z,float r,float g,float b,float a) {
            WpcTexturedVertex3D w{};
            w.x=f[0]*x+f[4]*y+f[8]*z+f[12];w.y=f[1]*x+f[5]*y+f[9]*z+f[13];w.z=f[2]*x+f[6]*y+f[10]*z+f[14];
            w.r=r;w.g=g;w.b=b;w.a=a;return w;
        };
        for(const auto& b:w.bees) {
            const float sa=std::sin(float(b[0])),ca=std::cos(float(b[0])),sb=std::sin(float(b[1])),cb=std::cos(float(b[1]));
            const float x=R*sa*cb,y=-R*sb,z=R*ca*cb;
            const WpcTexturedVertex3D c=vertex(x,y,z,0,0,1/255.f,1);
            const float dk=13/255.f,al=173/255.f;
            for(const auto& t:{c,vertex(x+h,y+h,z,dk,0,0,al),vertex(x+h,y-h,z,dk,0,0,al),
                               c,vertex(x-h,y+h,z,1,1,0,al),vertex(x-h,y-h,z,1,1,0,al)}) fire_tris_.push_back(t);
        }
    }
    for(auto it=swarms_.begin();it!=swarms_.end();) it=live.count(it->first)?std::next(it):swarms_.erase(it);
}
void RaceView::submit_tank_hit(int car,const BombHit& hit,const CarEffects& fx,const Matrix4& vp,const Matrix4& view,const Matrix4& placed) {
    // The chunks (WReszecske clones of m_Particle) outlive the hit.
    if(!chunk_model_.empty()) for(const auto& c:fx.chunks()) submit(chunk_model_,vp,view,multiply_matrix(placed,c.frame));
    // Type 2 (0x47e277): the GDHModel at the frame the hit took from the car,
    // its axes scaled (0x402027), segments 1, 3, 4 and 5 scaled by the
    // overrides of stage 2 (0x437f70).
    const bool on=hit.active() && hit.type==2 && hit.tank.shown && !tank_hit_.batches.empty();
    auto found=tank_hits_.find(car);
    if(!on) {if(found!=tank_hits_.end()) tank_hits_.erase(found);return;}
    static const MotionLib none;
    if(found==tank_hits_.end()) found=tank_hits_.emplace(car,std::make_unique<AnimatedInstance>(tank_hit_,tank_hit_.lib?*tank_hit_.lib:none)).first;
    AnimatedInstance& a=*found->second;
    a.overrides.clear();
    if(hit.tank.override) {
        const size_t segs[4]={1,3,4,5};
        for(size_t k=0;k<4;++k) a.overrides.push_back({segs[k],hit.tank.segments[k]});
    }
    a.posed=0;   // the overrides change each update: pose every draw
    pose(a);
    Matrix4 m(hit.tank.frame);
    for(size_t ax=0;ax<3;++ax) for(size_t r=0;r<3;++r) m[ax*4+r]*=hit.tank.scale[ax];
    submit(a.model.batches,vp,view,multiply_matrix(placed,m));
}
bool RaceView::shadow_frame(size_t slot,const RaceCar& car,Matrix4& out,bool frozen) {
    // 0x47c1ad: while the drowning counter +0x8e7 runs, the update is skipped
    // and the shadow keeps its last frame (frozen: the native drowning, provisional).
    // From 403.7 above the car to 1000 below; f is the hit's fraction of that
    // ray. The shadow lies 5 units up the ground normal, right = n x forward,
    // forward = right x n, scaled by the vehicle's sizes (0x47c070) x (1 - f).
    static const float sizes[7][2]={{1.5f,0.7f},{1,1},{3.5f,1.5f},{3.5f,1.5f},{2,2},{2,1},{5,2}};
    const float top=car.position[1]+403.7f,bottom=car.position[1]-1000;
    const TrackSurface g=frozen?TrackSurface{}:session_.track().ground(car.position[0],car.position[2],top,bottom);
    if(!frozen && g.hit && g.y<car.position[1]+50 && car.vehicle>=0 && car.vehicle<7) {
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
void RaceView::effects(const Matrix4& vp,const Matrix4& view,const Matrix4& camera_frame,const std::function<Matrix4(int)>& place,uint32_t enabled) {
    std::vector<const CarEffects*> cars{&session_.effects()};
    std::vector<int> car_sections{session_.player().section};
    for(size_t i=0;i<session_.opponents().size();++i) {cars.push_back(&session_.opponent_effects(i));car_sections.push_back(session_.opponents()[i].car.section);}
    // Groups (+0x104): a car's effects copy its group (WSplash 0x472d00,
    // WalkOnWater 0x47c680); a fired or thrown weapon takes its tracker row's
    // section (0x4a6b30, kept by the manager 0x4a1c40); a dropped one goes
    // into the power-up table of the row whose box holds it (0x4a6d20 ->
    // 0x49c890) and is enabled with that row (0x4a8680). The skid marks
    // (0x47a7c0) take their matrix from 0x4a8e60 for their parent node (+0x10,
    // 0x41b3c0), the car: its group's seam place at draw time, so a car's
    // whole trail moves with the car's current section.
    const auto on=[&](int section) {return section<0 || section>31 || (enabled>>unsigned(section)&1u);};
    const auto at_frame=[&](int section,const Matrix4& m) {return multiply_matrix(place(section),m);};
    // 0x47a7c0 (Direct3D path, 0x6f5958 = 2): for each wheel, two triangles
    // per pair of active segments, untextured (FVF 0x42), vertex colour
    // (fade << 24 | shade x 0x010101), mode 0x18: alpha blended, no culling,
    // no depth write. The software path lifts the points by 5 (0x47aad6);
    // natively too, against z-fighting with the ground (provisional).
    std::vector<WpcTexturedVertex3D> strip;
    for(size_t c=0;c<cars.size();++c) {
    const Matrix4 m=place(car_sections[c]);
    for(const auto& t:cars[c]->tracks()) for(size_t i=0;i+1<t.size();++i) {
        const auto &s0=t[i],&s1=t[i+1];
        if(!s0.active || !s1.active) continue;
        const auto vertex=[&m](const Vec3& p,const CarEffects::TrackSegment& s) {
            WpcTexturedVertex3D v{};
            const float y=p[1]+5;
            v.x=m[0]*p[0]+m[4]*y+m[8]*p[2]+m[12];v.y=m[1]*p[0]+m[5]*y+m[9]*p[2]+m[13];v.z=m[2]*p[0]+m[6]*y+m[10]*p[2]+m[14];
            v.r=v.g=v.b=float(s.shade)/255.f;v.a=float(std::max(s.fade,0))/255.f;
            return v;
        };
        for(const auto& v:{vertex(s0.a,s0),vertex(s0.b,s0),vertex(s1.a,s1),vertex(s1.a,s1),vertex(s0.b,s0),vertex(s1.b,s1)})
            strip.push_back(v);
    }
    }
    if(!strip.empty()) {
        wpc_vk_set_mvp(window_,vp.data());
        wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
        wpc_vk_draw_material_triangles(window_,white_,strip.data(),uint32_t(strip.size()),3,1);
        wpc_vk_set_draw_mode(window_,0);
    }
    for(const Tnt& t:session_.tnts()) {
        RaceCar at;at.position=t.position;at.yaw=std::atan2(-t.velocity[0],t.velocity[2]);
        submit(tnt_,vp,view,at_frame(t.section,at.frame()));
    }
    // WPaintShot's GDModel 0x396, along its velocity.
    if(const auto it=projectiles_.find(0x396);it!=projectiles_.end()) for(const auto& p:session_.paint_shots()) {
        RaceCar at;at.position=p.position;at.yaw=std::atan2(-p.velocity[0],p.velocity[2]);
        submit(it->second,vp,view,at_frame(p.section,at.frame()));
    }
    for(const Projectile& p:session_.projectiles()) {
        const auto it=projectiles_.find(p.object);
        if(it==projectiles_.end()) continue;
        RaceCar at;at.position=p.position;at.yaw=std::atan2(-p.velocity[0],p.velocity[2]);
        submit(it->second,vp,view,at_frame(p.section,at.frame()));
    }
    // WRocketTail's draw (0x4eacf1, Direct3D path): blending on, no culling,
    // no depth write (0x4b5550 mode 9, 0x4b44e0).
    for(const Projectile& p:session_.projectiles()) {
        const auto style=tails_.find(p.object);
        if(style==tails_.end()) continue;
        const Matrix4 m=place(p.section);
        const auto at=[&](const Vec3& v) {return Vec3{{m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12],m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]}};};
        const int length=style->second.length;
        std::vector<WpcTexturedVertex3D> tris;
        const auto vertex=[](const Vec3& v,float r,float g,float b,float a) {
            WpcTexturedVertex3D w{};w.x=v[0];w.y=v[1];w.z=v[2];w.r=r;w.g=g;w.b=b;w.a=a;return w;
        };
        if(style->second.fins) {
            // Points 1..L-1 relative to the head; the taper s starts at 1 and
            // loses 1/L a point (to 0), the corners are c +/- s x up and
            // +/- s x side, each axis x m_Width x k. Four fins (0x4eb260 with
            // corners 1..4) run from the centre line, alpha 255 - (255 / L)
            // per segment, to the corner, alpha 0, in (232, 223, 172)
            // (0xffe8dfac). 0x4b5550 is given L triangles: L / 2 segments.
            std::vector<float> taper(size_t(length),0);
            float s=1;
            for(int i=1;i<length;++i) {taper[size_t(i)]=s;s=s>1.f/float(length)?s-1.f/float(length):0;}
            const int step=255/length;
            for(int fin=0;fin<4;++fin) {
                int alpha=255;
                for(int j=0;j<length/2 && j+2<length;++j) {
                    const auto corner=[&](int i) {
                        const auto& t=p.tail[size_t(i)];
                        const Vec3& axis=fin%2==0?t.up:t.side;
                        const float d=(fin<2?1.f:-1.f)*taper[size_t(i)]*t.k*float(style->second.width);
                        return at({{t.position[0]+axis[0]*d,t.position[1]+axis[1]*d,t.position[2]+axis[2]*d}});
                    };
                    const Vec3 a0=at(p.tail[size_t(j+1)].position),a1=at(p.tail[size_t(j+2)].position),b0=corner(j+1),b1=corner(j+2);
                    const float r=232/255.f,g=223/255.f,bl=172/255.f,al0=float(alpha)/255.f;
                    alpha=std::max(alpha-step,0);
                    const float al1=float(alpha)/255.f;
                    for(const auto& v:{vertex(b0,r,g,bl,0),vertex(b1,r,g,bl,0),vertex(a0,r,g,bl,al0),
                                       vertex(b1,r,g,bl,0),vertex(a1,r,g,bl,al1),vertex(a0,r,g,bl,al0)}) tris.push_back(v);
                }
            }
        }
        // 0x4b44e0: once the tail has run (+0x28 = 12), a disc of +0x6a = 8
        // segments facing the camera at the head, radius +0x68 = 300 + 30 cos,
        // centre colour (240 + 15 sin, 150 + 50 sin, 120, 200 + 50 cos) of the
        // phase, its rim transparent.
        if(p.tail_updates>0) {
            const float c=std::cos(p.tail_phase),sn=std::sin(p.tail_phase);
            const float radius=float(int(300+30*c));
            const float r=float(int(240+15*sn))/255.f,g=float(int(150+50*sn))/255.f,bl=120/255.f,al=float(int(200+50*c))/255.f;
            const Vec3 head=at(p.tail[0].position);
            const Vec3 x{{camera_frame[0],camera_frame[1],camera_frame[2]}},y{{camera_frame[4],camera_frame[5],camera_frame[6]}};
            const auto rim=[&](int k) {
                const float a=float(2*k)*3.14159265f/8.f;
                const float u=std::sin(a)*radius,v=std::cos(a)*radius;
                return vertex({{head[0]+x[0]*u+y[0]*v,head[1]+x[1]*u+y[1]*v,head[2]+x[2]*u+y[2]*v}},r,g,bl,0);
            };
            for(int k=0;k<8;++k) {tris.push_back(vertex(head,r,g,bl,al));tris.push_back(rim(k));tris.push_back(rim(k+1));}
        }
        if(!tris.empty()) {
            wpc_vk_set_mvp(window_,vp.data());
            wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
            wpc_vk_draw_material_triangles(window_,white_,tris.data(),uint32_t(tris.size()),3,1);
            wpc_vk_set_draw_mode(window_,0);
        }
    }
    // WExplosion's draw (0x4806f0): its sprite node is hidden (flag 0x100),
    // and the action draws the disc of its record +0x3c through 0x4b44e0 (as
    // the rocket glow): m_Resolution (10) segments facing the camera, the
    // centre in (m_Red, m_Green, m_Blue) at the alpha +0x3f, the rim
    // transparent, the radius +0x40, at the position moved a fifth of the way
    // to the camera (0x510ef0 = 0.2). m_isRing (0 in both templates) would add
    // the rings +0x43/+0x51/+0x5f (not ported).
    {
        std::vector<WpcTexturedVertex3D> fan;
        const Vec3 x{{camera_frame[0],camera_frame[1],camera_frame[2]}},y{{camera_frame[4],camera_frame[5],camera_frame[6]}};
        for(const auto& b:session_.blasts()) {
            if(b.radius<=0 || b.alpha<=0) continue;
            Vec3 c=b.at;
            for(size_t i=0;i<3;++i) c[i]+=(camera_frame[12+i]-c[i])*0.2f;
            const float r=b.colour[0],g=b.colour[1],bl=b.colour[2],al=float(b.alpha)/255.f,R=float(b.radius);
            const auto vertex=[&](const Vec3& v,float a){WpcTexturedVertex3D w{};w.x=v[0];w.y=v[1];w.z=v[2];w.r=r;w.g=g;w.b=bl;w.a=a;return w;};
            const auto rim=[&](int k){
                const float a=float(2*k)*3.14159265f/float(b.resolution);
                const float u=std::sin(a)*R,v=std::cos(a)*R;
                return vertex({{c[0]+x[0]*u+y[0]*v,c[1]+x[1]*u+y[1]*v,c[2]+x[2]*u+y[2]*v}},0);
            };
            for(int k=0;k<b.resolution;++k) {fan.push_back(vertex(c,al));fan.push_back(rim(k));fan.push_back(rim(k+1));}
        }
        if(!fan.empty()) {
            wpc_vk_set_mvp(window_,vp.data());
            wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
            wpc_vk_draw_material_triangles(window_,white_,fan.data(),uint32_t(fan.size()),3,1);
            wpc_vk_set_draw_mode(window_,0);
        }
    }
    // WFire's draw (0x481390, renderer 1: 0x4b5550 mode 8): the default
    // blend, culling on (no bit 0x10), no depth write.
    if(!fire_tris_.empty()) {
        wpc_vk_set_mvp(window_,vp.data());
        wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE);
        wpc_vk_draw_material_triangles(window_,white_,fire_tris_.data(),uint32_t(fire_tris_.size()),3,0);
        wpc_vk_set_draw_mode(window_,0);
        fire_tris_.clear();
    }
    for(const Landmine& m:session_.mines()) {
        if(!on(m.section)) continue;
        RaceCar at;at.position=m.position;at.yaw=m.yaw;at.up=m.normal;
        submit(m.detour?detour_:m.oil?oil_:m.hive?hive_:landmine_,vp,view,at_frame(m.section,at.frame()));
    }
    // The storm cloud: the sprite at the cloud, faced to the camera
    // (0x419b90, type 0), turned by its +0x152 angle.
    for(const auto& shown:storm_shown_) translucent_.erase(&shown);
    storm_shown_.resize(session_.storms().size());
    for(size_t i=0;i<session_.storms().size();++i) {
        const Storm& st=session_.storms()[i];
        std::vector<WorldBatch>& shown=storm_shown_[i];
        shown=storm_;
        if(st.done) {shown.clear();continue;}
        for(WorldBatch& b:shown) {
            if(!b.billboard) continue;
            const Matrix4 m=place(st.section);  // the anchor moved, then faced
            for(size_t k=0;k<3;++k) b.anchor[k]=m[k]*st.position[0]+m[4+k]*st.position[1]+m[8+k]*st.position[2]+m[12+k];
            // 0x419eef: the quad turns by +0x152 about the view axis
            // (0x43b690: x' = c x - s y, y' = c y + s x), so an offset (ox, oy)
            // on the right/up axes becomes (c ox + s oy, c oy - s ox).
            if(st.angle!=0) {
                const float c=std::cos(st.angle),sn=std::sin(st.angle);
                for(auto& o:b.offsets) o={{c*o[0]+sn*o[1],c*o[1]-sn*o[0]}};
            }
        }
        face_billboards(shown,camera_frame);
        translucent_[&shown]=translucent_models(shown);
        submit(shown,vp,view,identity_matrix());
    }
    for(const Barrier& w:session_.barriers()) {
        if(!on(w.section)) continue;
        RaceCar at;at.position=w.position;at.yaw=w.yaw;
        submit(w.tree?tree_:wall_,vp,view,at_frame(w.section,at.frame()));
    }
    for(size_t c=0;c<cars.size();++c) {
        const CarEffects* fx=cars[c];
        const Matrix4 placed=place(car_sections[c]);
        for(const auto& s:fx->splashes()) {
            if(s.state<=0) continue;
            submit(splash_column_,vp,view,multiply_matrix(placed,s.column()));
            submit(splash_ring_,vp,view,multiply_matrix(placed,s.ring()));
        }
        for(const auto& r:fx->ripples()) {
            Matrix4 m=r.base;
            for(size_t i=0;i<3;++i) {m[i]*=r.scale;m[4+i]*=r.scale;m[8+i]*=r.scale;}
            submit(ripple_,vp,view,multiply_matrix(placed,m));
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
void RaceView::animate_scrolls() {
    // 0x463e90: units per update over the first material's texture size (width for
    // u, height for v), added to every vertex. Driven from the update count,
    // so several draws of an update agree.
    const uint64_t n=session_.updates();
    for(Scroll& u:scrolls_) {
        if(!u.found) {
            u.found=true;
            const auto gather=[&](std::vector<WorldBatch>& list) {
                for(WorldBatch& b:list) if(b.node_id==u.node) {
                    std::vector<float> base;base.reserve(b.vertices.size());
                    for(const auto& v:b.vertices) base.push_back(u.along_u?v.u:v.v);
                    u.batches.emplace_back(&b,std::move(base));
                }
            };
            gather(world_.batches);
            for(auto& list:spins_) gather(list);
        }
        for(auto& [batch,base]:u.batches) {
            const auto size=texture_sizes_.find(batch->material.texture_id);
            if(size==texture_sizes_.end()) continue;
            const float extent=float(u.along_u?size->second[0]:size->second[1]);
            if(!(extent>0)) continue;
            // units x updates wrapped by the texture size in integers, so the
            // shift keeps its precision and never jumps over a long race.
            const int64_t e=int64_t(extent),m=((int64_t(u.units)*int64_t(n%uint64_t(e)))%e+e)%e;
            const float shift=float(m)/extent;
            for(size_t k=0;k<batch->vertices.size() && k<base.size();++k) (u.along_u?batch->vertices[k].u:batch->vertices[k].v)=base[k]+shift;
        }
    }
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
    if(human==0) {animate_lights();animate_scrolls();}
    const int w=int(640*scale_),h=int(480*scale_);
    const bool two=session_.two_players();
    wpc_vk_draw_rect(window_,offset_x_,offset_y_+(two?human*(h/2):0),w,two?h/2:h,world_.clear[0],world_.clear[1],world_.clear[2],1);
    // WFollowCamera_2 (0x478070), updated with the race.
    const Matrix4& camera=session_.camera(human);
    Matrix4 vp=multiply_matrix(projection_,inverse_affine_matrix(camera));
    for(size_t c=0;c<4;++c) {vp[c*4]*=float(w)/width;vp[c*4+1]*=float(h)/height;}
    // A landed wall or tree shakes the viewport (+0x20, +0x24 of the
    // graphics object's +0xb4, rand() % 5 for 30 updates): read here as its
    // origin moved that many 640 x 480 pixels (provisional: the reader of
    // those fields is not traced).
    if(const auto sh=session_.camera_shake();sh[0] || sh[1]) {
        const float dx=2*float(sh[0])*scale_/float(width),dy=-2*float(sh[1])*scale_/float(height);
        for(size_t c=0;c<4;++c) {vp[c*4]+=vp[c*4+3]*dx;vp[c*4+1]+=vp[c*4+3]*dy;}
    }
    // A half viewport. The Direct3D projection (0x4aec30) takes the device
    // viewport (GetViewport, vtable +0x3c): s = sqrt(height / width x ratio
    // x 0.01 (0x512fcc)), m00 = f s, m11 = f / s; camera_projection builds
    // the full screen's (height / width 0.75, the 0.0075 factor). A half
    // (1024 x 384) has height / width 0.375, so its s is the full screen's
    // x sqrt(0.5): x x 0.707 and y x 1.414 onto half the height, as fitted
    // to Wine before (the tower car 205 x 70 pixels at 1024x768 against 290
    // x 100, cc_t1, 2026-11-08). The camera ratio stays 100 in two players
    // (Wine memory read, 2026-11-12).
    if(two) {
        const float k=std::sqrt((float(h)*0.5f/float(w))/(float(h)/float(w))); // sqrt(0.5)
        for(size_t c=0;c<4;++c) {vp[c*4]*=k;vp[c*4+1]=vp[c*4+1]*k+vp[c*4+3]*(human?0.5f:-0.5f)*float(h)/height;}
    }
    // Backdrop around the eye, unfogged, just inside the far plane.
    {
        // Scaled to 93% of the far plane (fitted to the cc_t1 Wine frame, dome
        // radius 2505) about the eye, in the dome node's frame less its
        // translation. The earlier fit lowered it by 262/2505 of the radius:
        // cc_t1's node translation is (0, 262, 0).
        const float k=fog_end_*0.93f/backdrop_radius_;
        Matrix4 place=identity_matrix();
        place[0]=place[5]=place[10]=k;
        for(size_t i=0;i<3;++i) place[12+i]=camera[12+i]-k*backdrop_origin_[i];
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
    // 0x49e290, 0x4a0bbf, 0x4a14b5: on a section change the player's tracker
    // disables its three rows (0x4a8850) and enables the new ones (0x4a8680):
    // its section and the two beside it, in a loop (Wine, dr_t3 on the grid
    // in section 3: brick #303 of section 1 inactive, the others active;
    // robots enable nothing). With two players both sets are drawn
    // (provisional: the original's second tracker is not traced).
    const RaceTrack& track=session_.track();
    const int sections=int(track.sections());
    uint32_t enabled=0;
    const auto enable=[&](int s) {
        if(sections<=0 || sections>32) {enabled=~0u;return;}
        for(int d=-1;d<=1;++d) enabled|=1u<<(((s+d)%sections+sections)%sections);
    };
    enable(session_.player().section);
    if(session_.two_players()) enable(session_.opponents()[0].car.section);
    // The loop seam: each node's group (+0x104, its section; 0xffff when it
    // has none) picks its matrix cache through 0x5472d0 (0x4200e0): +0xcc,
    // under the root's local matrix (the WMOD wrap), for the groups 0x432690
    // marks; otherwise +0x98. Drawing (0x432710) marks the last group when the
    // player is in section 0, and the last two when the player is in the last
    // section (Wine, dr_t3: tables 98 98 98 cc and 98 98 cc cc; root +0xcc =
    // wrap()); groups past 3 never. A group is drawn in one place only. From a
    // camera in the player's group, the marked groups go through wrap(), or the
    // others through unwrap() when the player's own group is marked.
    const int p=human==1 && session_.two_players()?session_.opponents()[0].car.section:session_.player().section;
    const int final_group=sections-1;
    const auto marked=[&](int g) {
        if(g<0 || g>3 || sections<=0) return false;
        if(p==0 && g==final_group) return true;
        return p==final_group && g>=final_group-1;
    };
    const bool camera_marked=marked(p);
    // 0: in place, 1: through unwrap(), 2: through wrap().
    const auto place_of=[&](int g) {return marked(g)==camera_marked?0:camera_marked?1:2;};
    for(size_t i=0;i<batch_sections_.size();++i) {
        const uint32_t m=batch_sections_[i];
        const bool on=!m || (m&enabled);
        const int at=place_of(batch_group_[i]);
        drawn_[i]=on && at==0;
        seam_[0][i]=on && at==1;
        seam_[1][i]=on && at==2;
    }
    submit(world_.batches,vp,view,id,&drawn_);
    submit(world_.batches,vp,view,track.unwrap(),&seam_[0]);
    submit(world_.batches,vp,view,track.wrap(),&seam_[1]);
    const auto seam_place=[&](int section) {
        const int at=place_of(section);
        return at==1?track.unwrap():at==2?track.wrap():id;
    };
    // Course objects are drawn where their group's section is (the group of
    // the node their batches come from).
    const auto submit_object=[&](const std::vector<WorldBatch>& batches,const Matrix4& model) {
        const std::vector<int>& in=batches.empty()?batch_group_none_:track.node_sections(batches.front().node_id);
        submit(batches,vp,view,multiply_matrix(seam_place(in.empty()?-1:in.front()),model));
    };
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
            submit_object(pickups_[i],m);
        }
    }
    {
        const CourseObjects& objects=session_.objects();
        for(size_t i=0;i<crossing_trams_.size();++i) if(objects.crossings()[i].active) submit_object(crossing_trams_[i],objects.crossings()[i].tram_frame());
        for(size_t i=0;i<movers_.size();++i) if(objects.movers()[i].active) submit_object(movers_[i],objects.movers()[i].placed);
        for(size_t i=0;i<logs_.size();++i) for(size_t k=0;k<logs_[i].size();++k) submit_object(logs_[i][k],objects.log_rides()[i].frames[k]);
        for(size_t i=0;i<bridges_.size();++i) for(size_t k=0;k<2;++k) submit_object(bridges_[i][k],objects.bridges()[i].frames[k]);
        for(size_t i=0;i<holes_.size();++i) {
            const DynamiteHole& d=objects.dynamite_holes()[i];
            submit_object(holes_[i][0],d.lid_frame());
            if(d.armed) submit_object(holes_[i][1],hole_fuses_[i]);
        }
        for(size_t i=0;i<chisels_.size();++i) {
            const ChiselCrossover& c=objects.chisels()[i];
            if(c.active) for(size_t k=0;k<chisels_[i].size();++k) submit_object(chisels_[i][k],multiply_matrix(c.log_world,c.blow.pieces[k].local));
        }
        for(size_t i=0;i<rocks_.size();++i) {
            const RollingRock& r=objects.rocks()[i];
            if(r.active) {const Matrix4 w=r.world();for(size_t k=0;k<rocks_[i].size();++k) submit_object(rocks_[i][k],multiply_matrix(w,r.blow.pieces[k].local));}
        }
        for(size_t i=0;i<dicks_.size();++i) {
            const DicksRoad& r=objects.dicks_roads()[i];
            if(r.active) submit_object(dicks_[i][size_t(r.chosen)],r.world[size_t(r.chosen)]);
        }
        for(size_t i=0;i<golden_.size();++i) {
            const GoldenGate& g=objects.golden_gates()[i];
            for(size_t k=0;k<9;++k) submit_object(golden_[i][k],multiply_matrix(g.parent,g.doors[k].frame));
            submit_object(golden_[i][9],multiply_matrix(g.parent,g.marker_frame));
        }
        for(size_t i=0;i<cranes_.size();++i) {
            const Crane& c=objects.cranes()[i];
            if(!c.active) continue;
            const auto w=c.worlds();
            submit_object(cranes_[i][0],w[0]);
            // Batches are in the group's space: each arm's motion from its home.
            const Matrix4 m1=multiply_matrix(c.frame[1],inverse_affine_matrix(c.home[1]));
            submit_object(cranes_[i][1],multiply_matrix(w[0],m1));
            const Matrix4 m2=multiply_matrix(c.frame[2],inverse_affine_matrix(c.home[2]));
            submit_object(cranes_[i][2],multiply_matrix(multiply_matrix(w[0],m1),multiply_matrix(c.home[1],multiply_matrix(m2,inverse_affine_matrix(c.home[1])))));
        }
        for(size_t i=0;i<shifted_.size();++i) {
            const ShiftedRoad& r=objects.shifted_roads()[i];
            for(size_t k=0;k<6;++k) submit_object(shifted_[i][k],multiply_matrix(r.parent,r.frame[k]));
        }
        for(size_t i=0;i<saws_.size();++i) {
            const Saw& w=objects.saws()[i];
            const Matrix4 sp=multiply_matrix(w.world,w.spinner_frame);
            submit_object(saws_[i][0],sp);
            submit_object(saws_[i][1],multiply_matrix(sp,w.owner_motion()));
        }
        for(size_t i=0;i<bumps_.size();++i) {
            const BumSteer& b=objects.bum_steers()[i];
            for(size_t k=0;k<2;++k) {
                submit_object(bumps_[i][k],multiply_matrix(b.parent,b.gate_frame[k]));
                submit_object(bumps_[i][2+k],multiply_matrix(b.parent,b.runner_frame[k]));
            }
        }
        for(size_t i=0;i<spins_.size();++i) if(objects.model_spins()[i].active) submit_object(spins_[i],objects.model_spins()[i].frame());
        for(size_t i=0;i<jams_.size();++i) {
            const LogJam& j=objects.log_jams()[i];
            if(j.active) for(size_t k=0;k<jams_[i].size();++k) submit_object(jams_[i][k],multiply_matrix(j.parent,j.logs[k].frame));
        }
        for(size_t i=0;i<geysers_.size();++i) if(objects.geysers()[i].active) {
            const Geyser& g=objects.geysers()[i];
            if(g.top()>0) submit_object(geysers_[i][0],g.column_frame());
            if(g.ring_scale>0) submit_object(geysers_[i][1],g.ring_frame());
        }
    }
    for(size_t i=0;i<session_.opponents().size();++i) {
        // A hidden robot (flag 0x100, 0x4a27b2) is not drawn.
        if(session_.opponent_drowning(i) || session_.opponent_hidden(i)) continue;
        // Player 2's car in its own near camera (see below).
        if(i==0 && human==1 && session_.two_players() && session_.camera_near(1)) continue;
        const auto& o=session_.opponents()[i];
        // A car's group is its section (+0x104): across the seam it is drawn
        // where its section is.
        const Matrix4 placed=seam_place(o.car.section);
        const Matrix4 frame=multiply_matrix(placed,o.car.frame());
        Matrix4 shadow;
        // 0x47c180 hides the shadow (flag 0x100) in the far mode.
        if(!o.far && shadow_frame(i+1,o.car,shadow,session_.opponent_drowning(i))) submit(shadows_.at(o.vehicle).batches,vp,view,multiply_matrix(placed,shadow));
        // Type 2 hides the car, its wheels and +0x32 (flag 0x100) for stages 0..3.
        if(!session_.bomb(int(i)).tank.car_hidden || session_.bomb(int(i)).type!=2) submit_car(int(i),o.vehicle,session_.bomb(int(i)),vp,view,frame);
        submit_spin_hit(session_.bomb(int(i)),o.car,vp,view,placed);
        submit_tank_hit(int(i),session_.bomb(int(i)),session_.opponent_effects(i),vp,view,placed);
        submit_fly(int(i),o.car,vp,view,placed);
        submit_fires(int(i),o.car,placed);
        submit_breath_head(int(i),o.car,vp,view,placed);
    }
    // 0x4782fb: in the near mode the camera writes itself into +0x24 of the
    // car's body, wheels and template part +0x2e0 (cleared on leaving);
    // read here as those parts not being drawn in that view (provisional).
    if(!session_.drowning() && !(human==0 && session_.camera_near(0))) {
        const Matrix4 placed=seam_place(session_.player().section);  // in place in its own view
        const Matrix4 frame=multiply_matrix(placed,session_.player().frame());
        Matrix4 shadow;
        if(shadow_frame(0,session_.player(),shadow)) submit(shadows_.at(session_.player().vehicle).batches,vp,view,multiply_matrix(placed,shadow));
        if(!session_.bomb(-1).tank.car_hidden || session_.bomb(-1).type!=2) submit_car(-1,session_.player().vehicle,session_.bomb(-1),vp,view,frame);
        submit_spin_hit(session_.bomb(-1),session_.player(),vp,view,placed);
        submit_tank_hit(-1,session_.bomb(-1),session_.effects(),vp,view,placed);
        submit_fly(-1,session_.player(),vp,view,placed);
        submit_fires(-1,session_.player(),placed);
        submit_breath_head(-1,session_.player(),vp,view,placed);
    }
    submit_kicks(vp,view,seam_place);
    submit_flips(vp,view,seam_place);
    submit_bees(vp,seam_place);
    effects(vp,view,camera,seam_place,enabled);
    queue_.flush([&](size_t i) {
        const Queued& q=queued_[i];
        wpc_vk_set_mvp(window_,q.mvp.data());
        wpc_vk_set_draw_mode(window_,draw_mode(q.model->draw_flags));
        for(size_t b:q.model->batches) {
            const WorldBatch& batch=(*q.batches)[b];
            if(batch.hidden || (q.mask && !(*q.mask)[b])) continue;
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
        // A car's films go where its group is across the seam (WKBumm 0x472dc0
        // and the hit smoke 0x47edcc copy the car's +0x104; the dust and
        // POUSSIERE puffs follow the car's position).
        const Matrix4 placed=seam_place(car.section);
        const auto at_car=[&](const Matrix4& base,const Vec3& p) {
            Vec3 w{},at{};
            for(size_t i=0;i<3;++i) w[i]=base[12+i]+base[i]*p[0]+base[4+i]*p[1]+base[8+i]*p[2];
            for(size_t i=0;i<3;++i) at[i]=placed[12+i]+placed[i]*w[0]+placed[4+i]*w[1]+placed[8+i]*w[2];
            return at;
        };
        const Matrix4 car_origin=[&]{Matrix4 m=identity_matrix();for(size_t i=0;i<3;++i) m[12+i]=car.position[i];return m;}();
        for(const auto& p:fx.puffs()) {
            if(!p.age || size_t(p.reel)>=puff_reels_.size() || puff_reels_[size_t(p.reel)].empty()) continue;
            const auto& reel=puff_reels_[size_t(p.reel)];
            add(&reel[size_t(p.frame)%reel.size()],at_car(car_origin,p.offset),puff_scale_,0,{{1,1,1,1}});
        }
        // 0x486c40: WKBumm's pieces, scale 0.2, in the clone's frame.
        if(!bumm_frames_.empty()) for(const auto& b:fx.bumms()) for(const auto& p:b.pieces) {
            if(p.landed) continue;
            add(&bumm_frames_[size_t(p.frame)%bumm_frames_.size()],at_car(b.base,p.position),0.2f,p.angle,{{1,1,1,1}});
        }
        // 0x42d090 on flamesmoke's film: frame 0 (m_AnimType 0 never steps,
        // 0x430110), white (m_ColR/G/B 255), the alpha +0x1ba (SMOO 1 keeps it),
        // the scale +0x152 and the angle +0x14a.
        if(!hit_smoke_frames_.empty()) for(const auto& h:fx.hit_smokes())
            add(&hit_smoke_frames_[0],at_car(h.base,h.position),h.scale,h.angle,{{1,1,1,float(h.alpha)/255.f}});
        // 0x47b9d0: the POUSSIERE puffs, two in three slots, frame age - 1,
        // colour (160, 128, 0), alpha (24 - age) x 3, scale 0.5 + 0.05 age.
        if(!smoke_frames_.empty()) for(size_t i=0;i<fx.smoke().size();++i) {
            const auto& m=fx.smoke()[i];
            if(!m.age || i%3==0) continue;
            add(&smoke_frames_[size_t(m.age-1)%smoke_frames_.size()],at_car(identity_matrix(),m.position),float(m.age)*0.05f+0.5f,0,
                {{160/255.f,128/255.f,0,float((24-m.age)*3)/255.f}});
        }
        }
        // The breath (DragonBreath 0x39a): its GD3DFilm 0x39d at the projectile,
        // m_Scale 1, white, frame trunc(+0x196) (m_AnimType 2, frames 0..7).
        if(!breath_film_.empty()) for(const Projectile& p:session_.projectiles()) if(p.object==0x39a) {
            const Matrix4 m=seam_place(p.section);
            const Vec3& v=p.position;
            add(&breath_film_[size_t(p.film)%breath_film_.size()],{{m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12],m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]}},1,p.spin,{{1,1,1,1}});
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
            // 0x4b5550 (the film's Direct3D draw, renderer 0x6f5958 = 1): no
            // depth write; mode 8, or 9 + SMOO when the frame's SILL (+0x5c)
            // is set (0x42d5c4). Mode 10's bit 0x2 blends INVSRCALPHA/INVSRCCOLOR.
            if(g.film) wpc_vk_set_draw_mode(window_,WPC_DRAW_NO_DEPTH_WRITE|(g.sill && g.smooth?WPC_DRAW_INVERSE:0));
            wpc_vk_draw_material_triangles(window_,g.texture,quad,6,3,1);
            if(g.film) wpc_vk_set_draw_mode(window_,0);
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
    // One player: the gauge and the icons wait until after the pause's dim
    // (late_panel, called from hud()).
    if(human!=0 || session_.two_players()) late_panel(human);
}
void RaceView::late_panel(int human) {
    gauge(human);
    // The gadget (+0x371) at (250,10) and weapon (+0x375) at (290,10); their
    // materials get transparency 0.4 while the charge is 0 (material +0x60 =
    // 0x3ecccccd, else 0; 0x4d2ea8..0x4d2eba). Wine's frames fit an opacity
    // of 0.36 = 0.6^2, not 0.6 (cc_t1 grid, 2026-11-12: before and after the
    // pause's dim); as if the renderer applied the material's alpha twice
    // (provisional: that path is not traced).
    const float alpha=session_.charge_level(human)>=1?1.f:0.36f;
    for(const auto& [index,x]:{std::pair<int,int>{session_.gadget_icon(human),250},std::pair<int,int>{session_.weapon_icon(human),290}}) {
        if(index<0 || size_t(index)>=icons_.size()) continue;
        const Sprite& icon=icons_.at(size_t(index));
        quad(icon.texture,float(2*x-icon_origin_[0]),float(20-icon_origin_[1]),float(icon.width),float(icon.height),false,alpha);
    }
}
void RaceView::gauge(int human) {
    const int nx=human?276:273,ny=human?95:93;
    quad(gauge_.texture,460,136,float(gauge_.width),float(gauge_.height),false,1);
    quad(gauge_needle_.texture,float(2*nx-10),float(2*ny-29),float(gauge_needle_.width),float(gauge_needle_.height),false,1,
         gauge_angle_[size_t(human&1)]*0.0174532925f,10,29);
}
void RaceView::radar(int human) {
    // The battle's radar (WModMngrFilm 0x499b96, mode 1): frame 0 (the disc)
    // at (40, 40) in viewport 0, (32, 32) in viewport 1 (0x499bd9/0x499c0f),
    // and frame 1 (the opponent's dot) at the manager's +0xf94/+0xf9c (or
    // +0xf98/+0xfa0) from 0x4aa120: the opponent in the player's frame
    // (x right, z ahead) x +0xf90 = m_nRadarRadius / m_nRadarRange (20 /
    // 10000 at run time under Wine, 0x49e5af), clamped to the radius 20
    // (0x452e20 normalising), at (centre + x, centre - z), each ftol'd.
    // Positions in 320 units at the window's uniform scale within the half
    // (Wine cc_b: the discs' centres at (130, 128) and (102, 100) pixels,
    // 1024x768), at scale 1 (the strip's heads are 0.65; with 0.65 the disc
    // came out 0.65 of Wine's).
    if(heads_.size()<2) return;
    const RaceCar* self=nullptr;const RaceCar* other=nullptr;
    if(session_.two_players()) {self=human?&session_.opponents()[0].car:&session_.player();other=human?&session_.player():&session_.opponents()[0].car;}
    else if(!session_.opponents().empty()) {self=&session_.player();other=&session_.opponents()[0].car;}
    if(!self || !other) return;
    const float centre=human?32.f:40.f,radius=20,scale=radius/10000.f;
    const Matrix4 f=self->frame();
    const float dx=other->position[0]-f[12],dy=other->position[1]-f[13],dz=other->position[2]-f[14];
    float x=(dx*f[0]+dy*f[1]+dz*f[2])*scale,z=(dx*f[8]+dy*f[9]+dz*f[10])*scale;
    const int ix=int(x),iz=int(z);
    if(ix*ix+iz*iz>int(radius*radius)) {const float l=std::sqrt(x*x+z*z);if(l>0) {x=x/l*radius;z=z/l*radius;}}
    const int px=int(x+centre),py=int(centre-z);
    const float k=1;
    const Glyph& disc=heads_.at(0);const Glyph& dot=heads_.at(1);
    quad(disc.texture,2*centre+disc.x*k,2*centre+disc.y*k,disc.width*k,disc.height*k,false,1);
    quad(dot.texture,2.f*px+dot.x*k,2.f*py+dot.y*k,dot.width*k,dot.height*k,false,1);
}
Colour RaceView::battle_pulse() const {
    const float v=(192+63*std::sin(float(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count()%6283185)*0.01f))/255;
    return Colour{1,v,v};
}
void RaceView::hud2(int human) {
    step_hud();
    const int own=session_.frags(human?0:-1),other=session_.frags(human?-1:0);
    if(session_.finish_state()==10) {
        // 0x4aa440 per viewport (+0x13a): this player's hits received
        // decide TU AS PERDU! (6) or TU AS GAGNE! (5).
        const bool lost=own>=RaceSession::frag_limit;
        const std::string& line=strings_.get(lost?6:5);
        text(line,324,124,4,Colour{0,0,0});text(line,320,120,4,lost?Colour{1,1,1}:battle_pulse());
        text(strings_.get(145),320,200,4);text("CONTINUE - X",320,400,4);
        return;
    }
    if(session_.finish_state()!=0) {
        if(human==0) standings();
        if(session_.finish_counter()<5) text(strings_.get(7),320,200,4); // (160,100) in two-player
        return;
    }
    siren_lights(human?0:-1);
    // Layout rows 1/2 of 0x52bd5c, in the viewport's own frame (the timer at
    // (160,216) and the progress strip fall outside the half and are cut;
    // the dial is hidden): lap (13,30), rank (33,50) and suffix (44,50),
    // message (160,60).
    char buffer[32];
    if(session_.mode()==1) {
        // Battle, rows 1/2: children 13/14 at (14,105) and (306,105); each
        // HUD block holds its own player's hits first (0x6f3168/0x6f316c,
        // 0x6f31bc/0x6f31c0 at 0x4aa6c1).
        // 0x486105: child 13 (this player's own count, +0x20) shows only in
        // one player (0x6f3140 = 1); child 14 (the opponent's hits received,
        // +0x24, 0x4aa68c) always, at (306, 105). Wine shows the right one
        // alone in each half (cc_b, 2026-11-12).
        (void)own;
        std::snprintf(buffer,sizeof buffer,"%d",other);text(buffer,306*2,105*2,0x20000);
        radar(human);
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
void RaceView::siren_lights(int viewer) {
    // WPU_FireSiren's light (0x4e01a2..0x4e01da): the brick shows and hides
    // by turns each time the life (+0x38, after its decrement) is a multiple
    // of 5, shown first. Each viewport places the GDSprite (0x4e00a0) at
    // (160, 30) for its own car's siren, else (276, 110): its top-left is
    // 2 (x, y) - its centre in the 640 x 480 frame.
    if(!siren_.texture) return;
    const auto one=[&](int car) {
        const int life=session_.siren_life(car);
        if(life<=0) return;
        const int toggles=29-(life-1)/5;   // multiples of 5 in [life, 149]
        if(toggles%2) return;
        const bool own=car==viewer;
        const float x=own?160.f:276.f,y=own?30.f:110.f;
        quad(siren_.texture,x*2-float(siren_centre_[0]),y*2-float(siren_centre_[1]),float(siren_.width),float(siren_.height),false,1);
    };
    one(-1);
    for(size_t i=0;i<session_.opponents().size();++i) one(int(i));
}
void RaceView::hud() {
    step_hud();
    if(session_.finish_state()==10) {
        // 0x4aa440, the battle's end (state 10): TU AS PERDU! (string 6)
        // when the player received the limit, else TU AS GAGNE! (5), at
        // (160,60) over a shadow at (162,62); string 145 at (160,100) and
        // the literal "CONTINUE - X" (0x52d210) at (160,200). TU AS GAGNE!
        // pulses (0x4aa497): (255, v, v) with v = sin(timeGetTime x 0.01
        // (0x512fcc)) x 63 (0x514be0) + 192 (0x514bd0); TU AS PERDU! keeps
        // the default colour.
        const bool lost=session_.battle_lost();
        const std::string& line=strings_.get(lost?6:5);
        text(line,324,124,4,Colour{0,0,0});text(line,320,120,4,lost?Colour{1,1,1}:battle_pulse());
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
    siren_lights(-1);
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
        radar(0);
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
    // Provisional (Wine, 2026-11-11): behind the pause the strip and the
    // texts dim to 0.5; the power-up gauge and the speedometer keep 1.0 and
    // the translucent item icons 0.64 (only what shows through them dims),
    // so those are drawn after the pause's wipe (the order inside the
    // original's sprite lists is not traced).
    if(late_dim_) late_dim_();
    late_panel(0);
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
