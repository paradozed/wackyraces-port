#include "game_race.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("game race: "+text);}
bool model_class(const Node& n) {return n.class_name=="GDModel" || n.class_name=="GDHModel";}
using Vec3=std::array<float,3>;
Vec3 cross(const Vec3& a,const Vec3& b) {
    return {{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]}};
}
Vec3 normalized(const Vec3& v) {
    const float length=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if(!std::isfinite(length)||length<=0) bad("degenerate follow-camera basis");
    return {{v[0]/length,v[1]/length,v[2]/length}};
}
void finite(const Matrix4& m) {
    for(float f:m) if(!std::isfinite(f)) bad("nonfinite actor transform");
    if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1) bad("actor transform is not affine");
}
}
RaceTemplates load_race_templates(const Node& general_root) {
    RaceTemplates result;
    size_t cameras=0;
    std::map<uint32_t,const Node*> nodes;
    std::function<void(const Node&)> index;
    index=[&](const Node& n) {
        if(!nodes.emplace(n.id,&n).second) bad("duplicate node ID in shared scene");
        for(const auto& c:n.children) index(c);
    };
    index(general_root);
    std::function<void(const Node&)> visit;
    visit=[&](const Node& parent) {
        for(size_t i=0;i<parent.children.size();++i) {
            const Node& n=parent.children[i];
            if(n.class_name=="WackyAuto") {
                const int vehicle=n.integer(0xed,-1);
                if(vehicle<0 || vehicle>=7) bad("unsupported shared vehicle enum");
                auto& entry=result.vehicles[size_t(vehicle)];
                if(entry.actor_id) bad("duplicate shared vehicle template");
                if(i+2>=parent.children.size() || !model_class(parent.children[i+1]) || !model_class(parent.children[i+2]))
                    bad("actor lacks the two following model siblings used by 0x469bf0");
                const int shadow=n.integer(0x120,-1);
                const auto found=nodes.find(uint32_t(shadow));
                if(shadow<0 || found==nodes.end() || !model_class(*found->second)) bad("actor has invalid shadow-model link");
                entry={n.id,uint32_t(shadow),{{parent.children[i+1].id,parent.children[i+2].id}}};
            }
            if(n.class_name=="GDCamera") for(const auto& action:n.actions) if(action.class_name=="WFollowCamera_2") {
                if(cameras>=2) bad("more than two shared follow-camera templates");
                result.camera_ids[cameras]=n.id;result.follow_action_ids[cameras]=action.id;++cameras;
            }
            visit(n);
        }
    };
    visit(general_root);
    for(const auto& entry:result.vehicles) if(!entry.actor_id) bad("missing shared vehicle template");
    if(cameras!=2) bad("missing shared follow cameras");
    return result;
}
uint32_t race_random(uint32_t& state) {
    state=state*0x343fdu+0x269ec3u;
    return (state>>16)&0x7fff;
}
std::vector<Racer> race_roster(int mode,int vehicle,uint32_t& random_state) {
    if(mode<0 || mode>3 || vehicle<0 || vehicle>=7) bad("invalid race selection");
    if(mode==2) return {{vehicle,2,true}};
    if(mode==1) return {{vehicle,0,true},{(vehicle+1)%7,1,false}};
    std::vector<int> opponents;
    for(int i=0;i<7;++i) if(i!=vehicle) opponents.push_back(i);
    // 0x49d580 performs count/2 random pair swaps, with an explicit next-index
    // correction if both random draws select the same opponent.
    for(size_t i=0;i<opponents.size()/2;++i) {
        const size_t a=race_random(random_state)%opponents.size();
        size_t b=race_random(random_state)%opponents.size();
        if(a==b) b=(b+1)%opponents.size();
        std::swap(opponents[a],opponents[b]);
    }
    std::vector<Racer> result{{vehicle,6,true}};
    for(size_t i=0;i<opponents.size();++i) result.push_back({opponents[i],unsigned(i),false});
    return result;
}
Matrix4 race_grid_frame(const Matrix4& start_line,unsigned slot) {
    static const float lateral[]={-450,-150,150,450,-300,0,300};
    static const float forward[]={-300,-300,-300,-300,-600,-600,-600};
    finite(start_line);
    if(slot>=7) bad("grid index outside original offset tables");
    Matrix4 frame=start_line;
    for(size_t r=0;r<3;++r) frame[12+r]=start_line[12+r]+lateral[slot]*start_line[r]+forward[slot]*start_line[8+r];
    finite(frame);return frame;
}
Matrix4 initial_follow_camera(const Matrix4& actor,bool two_players) {
    finite(actor);
    const float eye_height=two_players?350.f:400.f,look=two_players?75.f:150.f;
    Vec3 eye{},direction{},up{{actor[4],actor[5],actor[6]}};
    for(size_t r=0;r<3;++r) {
        eye[r]=actor[12+r]+eye_height*actor[4+r]-900.f*actor[8+r];
        const float aim=actor[12+r]+look*(up[r]+(r==1?1.f:0.f));
        direction[r]=aim-eye[r];
    }
    const Vec3 forward=normalized(direction);
    const Vec3 right=normalized(cross(up,forward));
    const Vec3 corrected_up=normalized(cross(forward,right));
    Matrix4 result=identity_matrix();
    for(size_t r=0;r<3;++r) {
        result[r]=right[r];result[4+r]=corrected_up[r];result[8+r]=forward[r];result[12+r]=eye[r];
    }
    finite(result);return result;
}
Matrix4 initial_vehicle_model_frame(const Matrix4& actor) {
    finite(actor);Matrix4 result=actor;
    for(size_t r=0;r<3;++r) result[12+r]+=20.f*actor[4+r];
    finite(result);return result;
}
RaceGrid place_race_grid(const std::vector<Racer>& roster,const Matrix4& marker,const SpawnGroundQuery& query) {
    if(!query || roster.empty() || roster.size()>7) bad("invalid grid initialization context");
    RaceGrid result;
    for(const auto& racer:roster) {
        PlacedRacer placed;placed.racer=racer;
        placed.candidate=race_grid_frame(marker,racer.grid_slot);placed.frame=placed.candidate;
        GroundPoint end{placed.candidate[12],placed.candidate[13],placed.candidate[14]};
        GroundPoint start=end;start.y+=150;
        placed.ground=query(start,end,0x018000ff);
        result.ground_complete&=placed.ground.complete;
        if(placed.ground.complete && placed.ground.hit) {
            placed.frame[12]=placed.ground.contact.point.x;
            placed.frame[13]=placed.ground.contact.point.y;
            placed.frame[14]=placed.ground.contact.point.z;
            finite(placed.frame);
        }
        result.racers.push_back(std::move(placed));
    }
    return result;
}
}
