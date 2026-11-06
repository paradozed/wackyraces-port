#include "race_track.h"
#include "game_scene.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("race track: "+text);}
Vec3 transform_point(const Matrix4& m,const Vec3& p) {
    return {{m[0]*p[0]+m[4]*p[1]+m[8]*p[2]+m[12],m[1]*p[0]+m[5]*p[1]+m[9]*p[2]+m[13],m[2]*p[0]+m[6]*p[1]+m[10]*p[2]+m[14]}};
}
Vec3 rotate(const Matrix4& m,const Vec3& v) {
    return {{m[0]*v[0]+m[4]*v[1]+m[8]*v[2],m[1]*v[0]+m[5]*v[1]+m[9]*v[2],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]}};
}
GroundPoint point(const Vec3& v) {return {v[0],v[1],v[2]};}
Vec3 vec(const GroundPoint& p) {return {{p.x,p.y,p.z}};}
Vec3 position(const Matrix4& m) {return {{m[12],m[13],m[14]}};}
float distance(const Vec3& a,const Vec3& b) {
    return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));
}
Vec3 unit(Vec3 v) {
    const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(length>0) for(float& c:v) c/=length;
    return v;
}
}
RaceTrack::RaceTrack(const Archive& course,const Scene& scene,bool battle) {
    const ModeData mode=load_mode(course);
    {
        // 0x456050 collects the top-level Zartter bricks; 0x455d40 orders
        // each axis's min and max.
        const auto visit=[&](const auto& self,const Node& n)->void {
            if(n.class_name=="Zartter") {
                if(n.integer(0xd5,0)) bad("Zartter with a geometry link (0x459570 branch not ported)");
                Shelter z;z.frame=unwrapped_scene_transform(scene.root(),n.id);
                const float v[6]={n.real(0x8e,0),n.real(0x8f,0),n.real(0x90,0),n.real(0x91,1000),n.real(0x92,1000),n.real(0x93,1000)};
                for(size_t a=0;a<3;++a) {
                    if(!std::isfinite(v[a]) || !std::isfinite(v[a+3]) || std::abs(v[a])>1e6f || std::abs(v[a+3])>1e6f) bad("implausible Zartter box");
                    z.box[a]=std::min(v[a],v[a+3]);z.box[a+3]=std::max(v[a],v[a+3]);
                }
                shelters_.push_back(z);
            }
            for(const auto& c:n.children) self(self,c);
        };
        visit(visit,scene.root());
    }
    std::vector<std::pair<ModeVector,ModeVector>> bounds;
    // Sections in route-table order: route(k) is definitions[route_indices[k]]
    // (the table is a permutation on cc_t4, dr_t2 and sh_t2), each exit
    // meeting the next entry, the last one through the loop transform.
    for(size_t index=0;index<mode.route_indices.size();++index) {
        const ModeRoute& route=mode.route(index);
        if(route.rows.empty()) bad("route has no rows");
        // ct_b, sh_b and dr_b add routes with no ground brick (0): no section.
        if(!route.rows[0].ground_object_id) continue;
        Section section;
        const Node& brick=scene.find(route.rows[0].ground_object_id);
        if(brick.class_name!="WGndBrick") bad("route ground object is not a WGndBrick");
        section.ground.load(course,brick.link(0x174,"WGNR"));
        section.transform=unwrapped_scene_transform(scene.root(),brick.id);
        section.inverse=inverse_affine_matrix(section.transform);
        // WMOD route +28/+30: entry and exit WPortal of this section.
        section.entry_frame=unwrapped_scene_transform(scene.root(),route.object_ids[0]);
        section.entry=position(section.entry_frame);
        // Row +0x34: the row's route length (0x4a3a7a); one row per section
        // on the shipped courses, so the first row's.
        section.row_length=int(route.rows[0].word34);
        section.exit=position(unwrapped_scene_transform(scene.root(),route.object_ids[1]));
        // Every kind-zero link of the section's rows (0x4a8680 enables
        // those nodes for the player's neighbouring rows).
        for(const ModeRow& row:route.rows) for(const uint32_t link:row.object_links) {
            if(!(link&~uint32_t(3)) || (link&3)) continue;
            auto& s=links_[link>>2];
            if(std::find(s.begin(),s.end(),int(sections_.size()))==s.end()) s.push_back(int(sections_.size()));
        }
        // Kind-zero grid links (low two bits 0) to WBendSign objects.
        for(const uint32_t link:route.rows[0].object_links) {
            if(!(link&~uint32_t(3)) || (link&3)) continue;
            const Node* node=nullptr;
            try {node=&scene.find(link>>2);} catch(const std::runtime_error&) {continue;}
            if(node->class_name!="WBendSign") continue;
            // A sign is linked from every grid cell it touches.
            if(std::any_of(section.signs.begin(),section.signs.end(),[&](const BendSign& s){return s.id==node->id;})) continue;
            const Matrix4 m=unwrapped_scene_transform(scene.root(),node->id);
            BendSign sign;
            sign.position=position(m);sign.forward={{m[8],m[9],m[10]}};
            sign.radius=float(node->integer(0x1e0));sign.type=node->integer(0x1e1);sign.id=node->id;
            if(sign.type<0 || sign.type>4 || !(sign.radius>0)) continue;
            section.signs.push_back(sign);
        }
        sections_.push_back(std::move(section));
        bounds.push_back({route.rows[0].vectors[2],route.rows[0].vectors[3]});
    }
    if(sections_.empty()) bad("course has no WMOD routes");
    // Fields need every section's ground loaded (queries span all bricks).
    for(size_t i=0;i<sections_.size();++i) {
        const auto& [a,b]=bounds[i];
        build_field(sections_[i],i,{{std::min(a[0],b[0]),0,std::min(a[2],b[2])}},{{std::max(a[0],b[0]),0,std::max(a[2],b[2])}});
    }
    const auto start=mode.first_start_line_id(scene);
    if(!battle) {
        if(!start) bad("route zero links no WStartLine");
        start_line_=unwrapped_scene_transform(scene.root(),*start);
        start_radius_=scene.find(*start).real(0x1df,5000);
        if(!(start_radius_>0)) bad("invalid WStartLine radius");
    } else {
        // Route 0's first row (the manager's +0x8d0 for route +0x8c4 = 0,
        // provisional: the route choice is not traced).
        const ModeRow& row=mode.route(0).rows.at(0);
        if(!row.object_ids[0] || !row.object_ids[1]) bad("route zero links no WStartLine and no battle spawns");
        battle_=true;
        Matrix4 a=unwrapped_scene_transform(scene.root(),row.object_ids[0]);
        for(size_t r=0;r<3;++r) a[12+r]+=300*a[8+r];
        Matrix4 b=unwrapped_scene_transform(scene.root(),row.object_ids[1]);
        for(size_t r=0;r<3;++r) {b[12+r]-=300*b[8+r];b[r]=-b[r];b[8+r]=-b[8+r];}
        battle_frames_={a,b};start_line_=a;
    }
    // Select the serialized WMOD transform that closes the loop: the one
    // mapping the last exit portal onto the first entry portal.
    float best=std::numeric_limits<float>::max();
    for(const auto& t:mode.transforms) {
        const Matrix4 m=affine_matrix(t);
        const float error=distance(transform_point(m,sections_.back().exit),sections_.front().entry);
        if(error<best) {best=error;wrap_=m;}
    }
    if(battle_ && !(best<1000)) {wrap_=identity_matrix();best=0;} // arenas need no loop
    if(!(best<1000)) bad("no WMOD transform closes the route loop");
    unwrap_=inverse_affine_matrix(wrap_);
    // A node's rows: its own links, else its nearest linked ancestor's.
    std::vector<int> linked;
    std::function<void(const Node&)> collect=[&](const Node& n) {
        const std::vector<int> outer=linked;
        if(const auto l=links_.find(n.id);l!=links_.end()) linked=l->second;
        if(n.class_name=="WPowerUp") {
            PowerUpSpot spot;spot.node=n.id;spot.type=n.integer(0x28b);
            const Matrix4 m=unwrapped_scene_transform(scene.root(),n.id);
            spot.position=position(m);
            for(const Node& c:n.children) if(c.class_name=="GDModel") {spot.model=c.id;break;}
            // The object query (0x4a5730) scales the primitive's radius by the
            // owner's X axis and moves its centre into world space.
            for(const ScenePrim& prim:node_prims(n)) if(prim.sensor_group&0x8000) {
                spot.centre=transform_point(m,{{prim.centre[0],prim.centre[1],prim.centre[2]}});
                spot.radius=prim.radius*std::sqrt(m[0]*m[0]+m[1]*m[1]+m[2]*m[2]);
                break;
            }
            spot.sections=linked;
            if(spot.model && spot.type>=0 && spot.type<=2 && spot.radius>0) power_ups_.push_back(spot);
        }
        if(n.class_name=="WGndBrick") {
            bool any=false;Matrix4 m{};
            for(const ScenePrim& prim:node_prims(n)) {
                if(!prim.cylinder || !(prim.sensor_group&0x50000)) continue;
                if(!any) {m=unwrapped_scene_transform(scene.root(),n.id);any=true;}
                TrackCylinder c;
                c.base=transform_point(m,{{prim.centre[0],prim.centre[1],prim.centre[2]}});
                c.radius=prim.radius;c.height=prim.height;c.sensor_group=prim.sensor_group;c.object=prim.object>>16;
                cylinders_.push_back(c);
            }
        }
        for(const Node& c:n.children) collect(c);
        linked=outer;
    };
    collect(scene.root());
    objects_.load(scene);
    objects_.load_log_meshes(course,scene);
    objects_.link([this](uint32_t id) {const auto i=links_.find(id);return i==links_.end()?std::vector<int>{}:i->second;});
    // 0x4c5962..0x4c59c3: the WMisty row's route nodes 14 and 40 (0x4856f0
    // walks the nodes, each (links + 2) x 3 words long). The section is the
    // one the first sensor stands on (provisional: +0x104 is not traced).
    // 0x4d0089..0x4d011e: a WChiselCrossover's route node and its two next
    // ones (section under the brick: provisional, as for WMisty).
    for(ChiselCrossover& c:objects_.chisels()) {
        if(c.route_node<0) continue;
        const Route r=route(c.centre);
        if(!r.hit) continue;
        const auto node=[&](int n) {
            uint32_t polygon=0;
            for(int i=0;i<n;++i) {const RouteNode k=route_node(r.section,r.record,polygon);polygon+=uint32_t(k.forward.size()+k.backward.size()+2)*3;}
            return polygon;
        };
        c.section=r.section;c.polygon=node(c.route_node);
        const RouteNode here=route_node(r.section,r.record,c.polygon);
        std::vector<RouteLink> links=here.forward;links.insert(links.end(),here.backward.begin(),here.backward.end());
        const uint32_t one=node(c.route_node+1),two=node(c.route_node+2);
        if(links.size()>0 && links[0].target==one) c.bits|=1;
        if(links.size()>0 && links[0].target==two) c.bits|=2;
        if(links.size()>1 && links[1].target==one) c.bits|=4;
        if(links.size()>1 && links[1].target==two) c.bits|=8;
    }
    for(MistyBridge& b:objects_.bridges()) {
        if(b.sensors.empty()) continue;
        const Route r=route(b.sensors[0]);
        if(!r.hit) continue;
        const auto node=[&](int n) {
            uint32_t polygon=0;
            for(int i=0;i<n;++i) {const RouteNode k=route_node(r.section,r.record,polygon);polygon+=uint32_t(k.forward.size()+k.backward.size()+2)*3;}
            return polygon;
        };
        b.fork_section=r.section;b.fork_polygon=node(14);
        const RouteNode fork=route_node(r.section,r.record,b.fork_polygon);
        b.fork_to_40=!fork.forward.empty() && fork.forward[0].target==node(40);
    }
    const int laps=scene.root().integer(0x25,3); // GDWorld m_nLapNum
    if(laps<1 || laps>99) bad("invalid GDWorld lap count");
    laps_=laps;
}
TrackSurface RaceTrack::ground(float x,float z,float top,float bottom) const {
    TrackSurface best;
    for(size_t i=0;i<sections_.size();++i) {
        const auto& s=sections_[i];
        const Vec3 a=transform_point(s.inverse,{{x,top,z}}),b=transform_point(s.inverse,{{x,bottom,z}});
        const GroundQueryResult q=s.ground.query_local(point(a),point(b),0x018000ff);
        if(!q.hit) continue;
        const Vec3 hit=transform_point(s.transform,vec(q.contact.point));
        if(best.hit && hit[1]<=best.y) continue;
        best.hit=true;best.y=hit[1];best.flags=q.contact.record_flags;best.section=int(i);
        best.normal=unit(rotate(s.transform,vec(q.contact.normal)));
        if(best.normal[1]<0) for(float& c:best.normal) c=-c;
    }
    return best;
}
bool RaceTrack::sheltered(const Vec3& p) const {
    // 0x45a270: six planes through the frame origin moved along one world
    // axis only by axis[i] x bound (an original simplification; exact for
    // unrotated boxes); outside when a plane gives n.p + d < 0 (0x510124).
    const auto inside=[&](const Matrix4& m,const std::array<float,6>& box) {
        for(size_t a=0;a<3;++a) {
            const Vec3 axis{{m[a*4],m[a*4+1],m[a*4+2]}};
            const float l=std::sqrt(axis[0]*axis[0]+axis[1]*axis[1]+axis[2]*axis[2]);
            if(!(l>0)) return false;
            Vec3 n{{axis[0]/l,axis[1]/l,axis[2]/l}};
            for(int side=0;side<2;++side) {
                Vec3 at{{m[12],m[13],m[14]}};at[a]+=axis[a]*box[a+3*size_t(side)];
                if(side) for(float& f:n) f=-f;
                if(n[0]*(p[0]-at[0])+n[1]*(p[1]-at[1])+n[2]*(p[2]-at[2])<0) return false;
            }
        }
        return true;
    };
    // The original also tests each box in the wrapped frame (0x420110(1))
    // for the sections across the seam (0x6abb40); native: both frames.
    for(const Shelter& z:shelters_)
        if(inside(z.frame,z.box) || inside(multiply_matrix(wrap_,z.frame),z.box) || inside(multiply_matrix(unwrap_,z.frame),z.box)) return true;
    return false;
}
TrackSurface RaceTrack::ground_in(int section,float x,float z,float top,float bottom) const {
    TrackSurface out;
    if(section<0 || size_t(section)>=sections_.size()) return out;
    const auto& s=sections_[size_t(section)];
    const GroundQueryResult q=s.ground.query_local(point(transform_point(s.inverse,{{x,top,z}})),point(transform_point(s.inverse,{{x,bottom,z}})),0x018000ff);
    if(!q.hit) return out;
    out.hit=true;out.y=transform_point(s.transform,vec(q.contact.point))[1];out.flags=q.contact.record_flags;out.section=section;
    out.normal=unit(rotate(s.transform,vec(q.contact.normal)));
    if(out.normal[1]<0) for(float& c:out.normal) c=-c;
    return out;
}
TrackSurface RaceTrack::ray(const Vec3& from,const Vec3& to) const {
    TrackSurface best;float nearest=2;
    for(size_t i=0;i<sections_.size();++i) {
        const auto& s=sections_[i];
        const GroundQueryResult q=s.ground.query_local(point(transform_point(s.inverse,from)),point(transform_point(s.inverse,to)),0x018000ff);
        if(!q.hit || q.contact.fraction>=nearest) continue;
        nearest=q.contact.fraction;
        best.hit=true;best.point=transform_point(s.transform,vec(q.contact.point));best.y=best.point[1];
        best.flags=q.contact.record_flags;best.section=int(i);
        best.normal=unit(rotate(s.transform,vec(q.contact.normal)));
    }
    return best;
}
RaceTrack::Route RaceTrack::route(const Vec3& p,int near) const {
    Route best;
    const int n=int(sections_.size());
    for(size_t i=0;i<sections_.size();++i) {
        if(near>=0 && n>3 && int(i)!=near && int(i)!=(near+1)%n && int(i)!=(near+n-1)%n) continue;
        const auto& s=sections_[i];
        const Vec3 a=transform_point(s.inverse,{{p[0],p[1]+403.7f,p[2]}}),b=transform_point(s.inverse,{{p[0],p[1]-1000,p[2]}});
        const GroundQueryResult q=s.ground.query_local(point(a),point(b),0x818000ff);
        if(!q.secondary_hit) continue;
        const float y=transform_point(s.transform,vec(q.secondary.point))[1];
        if(best.hit && y<=best.y) continue;
        best={true,int(i),q.secondary.record_index,q.secondary.polygon,y};
    }
    return best;
}
RouteNode RaceTrack::route_node(int section,uint32_t record,uint32_t polygon) const {
    return sections_.at(size_t(section)).ground.route_node(record,polygon);
}
Vec3 RaceTrack::route_point(int section,const GroundPoint& local) const {
    return transform_point(sections_.at(size_t(section)).transform,vec(local));
}
Vec3 RaceTrack::route_vertex(int section,uint32_t record,uint16_t index) const {
    return route_point(section,sections_.at(size_t(section)).ground.vertex_point(record,index));
}
TrackWall RaceTrack::wall(const Vec3& from,const Vec3& to) const {
    TrackWall best;
    for(const auto& s:sections_) {
        const GroundQueryResult q=s.ground.query_local(point(transform_point(s.inverse,from)),point(transform_point(s.inverse,to)),0x01e000ff);
        if(!q.hit || q.contact.fraction>=best.fraction) continue;
        Vec3 n=unit(rotate(s.transform,vec(q.contact.normal)));
        if(std::abs(n[1])>0.6f) continue;
        n[1]=0;n=unit(n);
        // Face the normal against the movement.
        const float along=n[0]*(to[0]-from[0])+n[2]*(to[2]-from[2]);
        if(along>0) {n[0]=-n[0];n[2]=-n[2];}
        best.hit=true;best.fraction=q.contact.fraction;best.normal=n;
    }
    return best;
}
TrackContact RaceTrack::contact(const Vec3& from,const Vec3& to) const {
    TrackContact best;
    for(const auto& s:sections_) {
        const GroundQueryResult q=s.ground.query_local(point(transform_point(s.inverse,from)),point(transform_point(s.inverse,to)),0x01e000ff);
        if(!q.hit || q.contact.fraction>=best.fraction) continue;
        Vec3 n=unit(rotate(s.transform,vec(q.contact.normal)));
        // Every surface of the mask counts, ramp faces and floors included
        // (sh_t1, Wine: one ramp-face hit an update entering the ramp at
        // (-15415, -2332), matched within 5% with the tilt slot). The
        // normal is the polygon's, in full (0x46bdd3).
        const float along=n[0]*(to[0]-from[0])+n[1]*(to[1]-from[1])+n[2]*(to[2]-from[2]);
        if(along>0) for(float& c:n) c=-c;
        best.hit=true;best.fraction=q.contact.fraction;best.normal=n;best.flags=q.contact.record_flags;
    }
    return best;
}
void RaceTrack::build_field(Section& s,size_t index,const Vec3& low,const Vec3& high) {
    constexpr float step=150;
    s.x0=low[0];s.z0=low[2];
    s.width=int((high[0]-low[0])/step)+1;s.height=int((high[2]-low[2])/step)+1;
    if(s.width<=1 || s.height<=1 || size_t(s.width)*size_t(s.height)>4000000) bad("invalid WMOD route bounds");
    // Every surface of this section in a column is a node, scanned downward
    // from the top, so bridges and tunnels get their own layer.
    const size_t columns=size_t(s.width)*s.height;
    s.first.assign(columns+1,0);s.node_y.clear();
    std::vector<uint8_t> deep;
    for(size_t c=0;c<columns;++c) {
        s.first[c]=uint32_t(s.node_y.size());
        const float x=s.x0+float(c%size_t(s.width))*step,z=s.z0+float(c/size_t(s.width))*step;
        float top=8000;
        for(int layer=0;layer<6;++layer) {
            const TrackSurface g=ground(x,z,top,-8000);
            if(!g.hit) break;
            // Deep water joins the field at a high cost, bridging the jumps
            // over it (sh_t1's pyramid platforms) without routing through it.
            if(g.section==int(index) && ((g.flags&0xff) || drowning_surface(g.flags))) {
                s.node_y.push_back(g.y);deep.push_back(drowning_surface(g.flags));
            }
            top=g.y-100;
        }
    }
    s.first[columns]=uint32_t(s.node_y.size());
    const auto column_of=[&](size_t node) {
        return size_t(std::upper_bound(s.first.begin(),s.first.end(),uint32_t(node))-s.first.begin())-1;
    };
    const auto position_of=[&](size_t node) {
        const size_t c=column_of(node);
        return Vec3{{s.x0+float(c%size_t(s.width))*step,s.node_y[node],s.z0+float(c/size_t(s.width))*step}};
    };
    // Stepping across a wall costs as much as a long detour: thin walls
    // (sh_t1's tunnel walls, ct_t1's banks) fall between 150-unit samples. A
    // fixed cost rather than a block keeps kerbs and banks from cutting the
    // field apart (cc_t1's Seadman section).
    constexpr float wall_cost=4000;
    const auto crossing=[&](size_t a,size_t b) {
        Vec3 from=position_of(a),to=position_of(b);from[1]+=40;to[1]+=40;
        return wall(from,to).hit?wall_cost:0.f;
    };
    s.distance.assign(s.node_y.size(),-1);
    const auto nearest=[&](const Vec3& p)->long {
        long best=-1;float best_d=std::numeric_limits<float>::max();
        for(size_t n=0;n<s.node_y.size();++n) {
            if(deep[n]) continue;
            const float d=distance(position_of(n),p);
            if(d<best_d) {best_d=d;best=long(n);}
        }
        return best;
    };
    const long start=nearest(s.entry),finish=nearest(s.exit);
    if(start<0 || finish<0) {s.distance.clear();s.width=s.height=0;return;}
    // Dijkstra from the exit over nodes of 8-connected columns within one
    // step of height. Distance to go has no false peaks, unlike distance
    // travelled: sh_t1's meadow has dead-end corners far from its entry.
    // A second pass, when the entry stays apart, also joins steep ramps and
    // gaps of up to three columns at a high cost (ct_t4's climb to y 1200).
    const auto run=[&](float max_dy,int reach,float gap_cost) {
        s.distance.assign(s.node_y.size(),-1);
        std::vector<std::pair<float,size_t>> heap{{0.f,size_t(finish)}};
        s.distance[size_t(finish)]=0;
        const auto later=[](const std::pair<float,size_t>& a,const std::pair<float,size_t>& b){return a.first>b.first;};
        while(!heap.empty()) {
            std::pop_heap(heap.begin(),heap.end(),later);
            const auto [d,node]=heap.back();heap.pop_back();
            if(d>s.distance[node]) continue;
            const size_t c=column_of(node);
            const int cx=int(c%size_t(s.width)),cz=int(c/size_t(s.width));
            for(int dz=-reach;dz<=reach;++dz) for(int dx=-reach;dx<=reach;++dx) {
                const int nx=cx+dx,nz=cz+dz;
                if((!dx && !dz) || nx<0 || nz<0 || nx>=s.width || nz>=s.height) continue;
                const bool far=std::abs(dx)>1 || std::abs(dz)>1;
                const size_t column=size_t(nz)*size_t(s.width)+size_t(nx);
                for(size_t n=s.first[column];n<s.first[column+1];++n) {
                    const float dy=std::abs(s.node_y[n]-s.node_y[node]);
                    if(dy>max_dy) continue;
                    float nd=d+std::sqrt(float(dx*dx+dz*dz))*step*(deep[n] || deep[node]?8.f:1.f);
                    if(far || dy>step) nd+=gap_cost;
                    if(s.distance[n]>=0 && s.distance[n]<=nd) continue;
                    nd+=crossing(node,n);
                    if(s.distance[n]>=0 && s.distance[n]<=nd) continue;
                    s.distance[n]=nd;heap.push_back({nd,n});std::push_heap(heap.begin(),heap.end(),later);
                }
            }
        }
        // The entry's node: the nearest reached one (a portal can stand
        // over an isolated sliver, sh_t2's log ride).
        long best=s.distance[size_t(start)]>0 ? start : -1;float best_d=600;
        if(best<0) for(size_t n=0;n<s.node_y.size();++n) {
            if(s.distance[n]<0 || deep[n]) continue;
            const float dd=distance(position_of(n),s.entry);
            if(dd<best_d) {best_d=dd;best=long(n);}
        }
        s.entry_distance=best<0?-1.f:s.distance[size_t(best)];
    };
    run(step,1,0);
    if(!(s.entry_distance>0)) run(4*step,3,2000);
    if(!(s.entry_distance>0)) {
        // Sampling gaps can split a section; keep the straight-line portal
        // fraction for it instead of failing the race.
        s.distance.clear();s.width=s.height=0;
    }
}
long RaceTrack::cell(const Section& s,const Vec3& p) const {
    if(s.distance.empty()) return -1;
    const int cx=int(std::lround((p[0]-s.x0)/150)),cz=int(std::lround((p[2]-s.z0)/150));
    long best=-1;float best_score=std::numeric_limits<float>::max();
    for(int dz=-3;dz<=3;++dz) for(int dx=-3;dx<=3;++dx) {
        const int x=cx+dx,z=cz+dz;
        if(x<0 || z<0 || x>=s.width || z>=s.height) continue;
        const size_t column=size_t(z)*size_t(s.width)+size_t(x);
        for(size_t n=s.first[column];n<s.first[column+1];++n) {
            // The layer within reach of the car's height, then the closest column.
            const float dy=std::abs(s.node_y[n]-p[1]);
            if(s.distance[n]<0 || dy>250) continue;
            const float score=float(dx*dx+dz*dz)*1000+dy;
            if(score<best_score) {best_score=score;best=long(n);}
        }
    }
    return best;
}
float RaceTrack::section_fraction(int section,const Vec3& p) const {
    const auto& s=sections_.at(size_t(section));
    const long c=cell(s,p);
    if(c<0) {
        const float a=distance(p,s.entry),b=distance(p,s.exit);
        return a+b>0?a/(a+b):0;
    }
    return std::clamp(1-s.distance[size_t(c)]/s.entry_distance,0.f,1.f);
}
Vec3 RaceTrack::section_direction(int section,const Vec3& p) const {
    const auto& s=sections_.at(size_t(section));
    const long node=cell(s,p);
    if(node<0) return {};
    const size_t c=size_t(std::upper_bound(s.first.begin(),s.first.end(),uint32_t(node))-s.first.begin())-1;
    const int cx=int(c%size_t(s.width)),cz=int(c/size_t(s.width));
    const float y=s.node_y[size_t(node)],here=s.distance[size_t(node)];
    Vec3 gradient{};
    for(int dz=-2;dz<=2;++dz) for(int dx=-2;dx<=2;++dx) {
        const int x=cx+dx,z=cz+dz;
        if((!dx && !dz) || x<0 || z<0 || x>=s.width || z>=s.height) continue;
        const size_t column=size_t(z)*size_t(s.width)+size_t(x);
        for(size_t n=s.first[column];n<s.first[column+1];++n) {
            if(s.distance[n]<0 || std::abs(s.node_y[n]-y)>250) continue;
            const float w=(here-s.distance[n])/float(dx*dx+dz*dz);
            gradient[0]+=w*dx;gradient[2]+=w*dz;
        }
    }
    return unit(gradient);
}
}
