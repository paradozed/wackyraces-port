#include "race_weapons.h"
#include <algorithm>
#include <cmath>

namespace wpc {
namespace {
// The types WackyAutoBomba's update knows; others end at once (0x47e264).
bool held(uint16_t t) {
    switch(t) {case 2:case 4:case 8:case 0x10:case 0x40:case 0x80:case 0x100:case 0x200:case 0x400: return true;}
    return false;
}
}
namespace {
int to_byte(float c) {return std::clamp(int(std::lround(c*255)),0,255);}
// 0x47d906: (target - c) / 13, truncated toward zero.
int tint_step(int target,int c) {const int d=target-c;return d>=0?d/13:-((-d)/13);}
}
bool step_tint(Tint& t,const std::array<float,3>& lo,const std::array<float,3>& hi,int n) {
    std::array<int,3> target{};
    for(size_t i=0;i<3;++i) target[i]=to_byte(t.target[i]);
    if(!t.started) {
        // The fade starts from the colours the vertices have: the model's own,
        // or the target already written (type 2's stage 4, f = 1).
        for(size_t i=0;i<3;++i) {
            t.rest[i]={{to_byte(lo[i]),to_byte(hi[i])}};
            t.cur[i]=t.f>=1?std::array<int,2>{{target[i],target[i]}}:t.rest[i];
        }
        t.from_target=t.f>=1;t.started=true;
    }
    const auto fade=[&](bool back) {
        // One step of every vertex; true once every channel's largest step
        // was below 2.
        bool small=true;
        for(size_t i=0;i<3;++i) for(size_t e=0;e<2;++e) {
            const int d=tint_step(back?t.rest[i][e]:target[i],t.cur[i][e]);
            t.cur[i][e]+=d;if(std::abs(d)>=2) small=false;
        }
        ++t.steps;
        float far=0,span=0;
        for(size_t i=0;i<3;++i) for(size_t e=0;e<2;++e) {
            far=std::max(far,float(std::abs(target[i]-t.cur[i][e])));span=std::max(span,float(std::abs(target[i]-t.rest[i][e])));
        }
        t.f=span>0?std::clamp(1-far/span,0.f,1.f):1;
        if(back && t.f<=0) t.f=1e-6f;   // still tinted until mode 4
        if(!back && t.f<=0) t.f=1e-6f;
        return small;
    };
    switch(t.mode) {
        case 0: if(fade(false)) t.mode=1;return false;
        case 1: for(size_t i=0;i<3;++i) t.cur[i]={{target[i],target[i]}};t.f=1;t.hold=0;t.steps=0;t.mode=2;return false;
        case 2: if(++t.hold>n) t.mode=3;return false;
        case 3: if(fade(true)) t.mode=4;return false;
        case 4: t.cur=t.rest;t.f=0;t.steps=0;t.mode=5;return false;
        default: return true;
    }
}
std::array<float,3> tint_colour(const Tint& t,const std::array<float,3>& rest) {
    std::array<float,3> out=rest;
    // A colour written directly (type 2's stage 4, type 0x400): the target.
    if(!t.started) return t.f>=1?t.target:out;
    // Modes 1 and 4 show the last fade's colours until their write.
    if(t.mode>=5) return out;
    for(size_t i=0;i<3;++i) {
        const int target=to_byte(t.target[i]),home=to_byte(rest[i]);
        int c;
        if(t.mode==2) c=target;
        else if(t.mode<=1) {c=t.from_target?target:home;for(int k=0;k<t.steps;++k) c+=tint_step(target,c);}
        else {c=target;for(int k=0;k<t.steps;++k) c+=tint_step(home,c);}
        out[i]=float(c)/255.f;
    }
    return out;
}
namespace {
Tint tint_to(int r,int g,int b,float f=0) {Tint t;t.target={{r/255.f,g/255.f,b/255.f}};t.f=f;return t;}

}
BombStep update_bomb(BombHit& h) {
    BombStep s;
    if(!h.active()) return s;
    // 0x47e234: the update after +0x36 is set ends the action (0x41ba90 ->
    // vtable +0x14 0x47f690 clears +0x2c) and does nothing else.
    if(h.done) {const BombHit kept=h;h=BombHit{};h.guard=kept.guard;h.colour_lo=kept.colour_lo;h.colour_hi=kept.colour_hi;return s;}
    const int c=h.counter;
    switch(h.type) {
        case 8:case 0x40:case 0x80:
            ++h.sub;s.smoke=h.sub%5==0;s.flip=h.sub%10==0;
            if(c==0) h.tint=tint_to(10,10,0);
            h.done=step_tint(h.tint,h.colour_lo,h.colour_hi) && c>=bomb_hold_updates;
            break;
        case 4: {
            // 0x47f254's stages: 0 the car model's frame x 0.2, shown (flag
            // 0x100 cleared); 1 x (1.15, 1, 1.15) an update until +0x82 passes
            // 16; 2 waits until +0x28 passes 50; 3 x (0.8, 1, 0.8) until +0x28
            // passes 6; 4 hidden. 0x47d890(20, 20, 255, 10) done at stage 5
            // ends the hit (counter 78).
            BombHit::PaintPool& p=h.pool;
            switch(p.stage) {
                case 0: p.scale={{0.2f,0.2f,0.2f}};p.shown=true;s.pool=true;p.stage=1;break;
                case 1: p.scale[0]*=1.15f;p.scale[2]*=1.15f;if(c>16) {h.sub=0;p.stage=2;} break;
                case 2: if(h.sub++>50) {h.sub=0;p.stage=3;} break;
                case 3: p.scale[0]*=0.8f;p.scale[2]*=0.8f;if(h.sub++>6) p.stage=4;break;
                case 4: p.shown=false;p.stage=5;break;
                default: break;
            }
            if(c==0) h.tint=tint_to(20,20,255);
            h.done=step_tint(h.tint,h.colour_lo,h.colour_hi) && p.stage==5;
            break;
        }
        case 2: {
            // 0x47e277: the stages of the tank gun's hit (jump table 0x47f230;
            // +0x28 counts within a stage).
            BombHit::TankHit& k=h.tank;
            switch(k.stage) {
                case 0: // 0x47e2c8: at the car's frame, x 2 (0x402027), shown; the car hidden
                    k.shown=k.car_hidden=true;k.scale={{2,2,2}};s.tank_axes=s.tank_position=true;k.stage=1;break;
                case 1: // 0x47e49b: follows the car's frame; at +0x28 = 5 the next stage
                    s.tank_axes=s.tank_position=true;
                    if(++h.sub==5) {h.sub=0;k.stage=2;}
                    break;
                case 2: { // 0x47e60d: follows the car's position; t = +0x28 x 20 degrees
                    s.tank_position=true;++h.sub;
                    const float t=float(h.sub)*20*0.017453292f;
                    const float a=std::sin(2*t)*0.5f+1,b=std::sin(t)*0.5f+1,c4=std::sin(4*t)*0.5f+1;
                    k.override=true;k.segments={{{{1,b,1}},{{c4,1,1}},{{a,1,1}},{{1,1,a}}}}; // segments 1, 3, 4, 5
                    if(h.sub>35) {k.override=false;h.sub=0;k.stage=3;} // the overrides back to 1, flag 0
                    break;
                }
                case 3: // 0x47e79c..0x47e9a2: stages 3, 4 and 5 in one update
                    ++h.sub;
                    // 3: eleven clones of m_Particle (0x394, WReszecske).
                    s.tank_chunks=true;
                    // 4: the car shown again, its colours (100, 100, 0) (0x3ec8c8c9).
                    k.car_hidden=false;h.tint=tint_to(100,100,0,1);
                    // 5: the model's axes x (0.7, 0.1, 0.7), +0x28 = 0.
                    k.scale[0]*=0.7f;k.scale[1]*=0.1f;k.scale[2]*=0.7f;h.sub=0;k.stage=6;
                    break;
                case 6: // 0x47e9a2: x (1.1, 1, 1.1) an update; past +0x28 = 5 the next stage
                    k.scale[0]*=1.1f;k.scale[2]*=1.1f;
                    if(++h.sub>5) {h.sub=0;k.stage=7;}
                    break;
                case 7: if(c>25) k.stage=8;break; // 0x47e9de
                default: // 0x47e9f0: hidden; 0x47d890(100, 100, 0, 10) ends the hit
                    k.shown=false;h.done=step_tint(h.tint,h.colour_lo,h.colour_hi);break;
            }
            break;
        }
        case 0x400:
            // Stage 0 writes (10, 10, 0), stage 2 (at 27) the saved colours.
            h.tint=tint_to(10,10,0,c<27?1.f:0.f);
            h.done=c>=27;
            break;
        case 0x200:
            // 0x47eea0: +0x66 0.1 toward 1, a spring past +0x28 = 20.
            if(c==0) {h.scale_y=0.1f;h.squash_v=0.2f;h.sub=0;}
            if(h.sub<=23) {
                if(h.sub>20) {
                    const float v=(1-h.scale_y)/((66-float(h.sub))*0.05f+0.3f);
                    h.squash_v=(v+h.squash_v)*0.87f;h.scale_y+=h.squash_v;
                }
                ++h.sub;
            } else h.scale_y=1;
            h.done=c>=25;
            break;
        case 0x10:case 0x100: {
            // 0x47ebf5: shown (+0xc bit 2). Stage 0 scales the basis by 0.1
            // (0x43b190); stage 1 by 1.3 an update, and once +0x28 passes 8 the
            // basis becomes the identity (0x43b090) times 1.5. Then every update
            // the model sits 120 (0x512bf4) above the car and turns 20 degrees
            // (0x3eb2b8c2). Past +0x82 = 45 (0x47ed4d) it is hidden again.
            BombHit::SpinModel& m=h.spin;
            m.shown=true;
            if(m.stage==0) {m.scale*=0.1f;m.stage=1;}
            else if(m.stage==1) {
                m.scale*=1.3f;
                if(h.sub>8) {m.stage=2;m.from_identity=true;m.scale=1.5f;m.turn=0;}
            }
            m.turn+=0.34906585f;
            ++h.sub; // 0x47ed13
            if(c>0x2d) m.shown=false;
            h.done=c>=0x2e;
            break;
        }
        default: h.done=true;break;
    }
    // 0x47ef88/0x47ed1c below 0x19, the +0x8db write at 0x19, then the
    // counter increments at 0x47f1e8.
    if(held(h.type)) {
        if(h.counter<bomb_hold_updates) s.hold=true;
        else if(h.counter==bomb_hold_updates) s.boost=true;
    }
    ++h.counter;
    return s;
}
bool start_bomb(BombHit& h,uint16_t type) {
    if(h.active()) return false;
    if(h.guard==8) return false;
    if(h.guard==2) {h.guard=type!=4?1:8;return false;}
    h.type=type;h.counter=0;h.done=false;h.tint=Tint{};h.scale_y=1;h.squash_v=0;h.sub=0;h.spin=BombHit::SpinModel{};h.pool=BombHit::PaintPool{};
    return true;
}
ItemAims default_item_aims() {
    constexpr float full=6.2831855f,d20=0.34906585f,d30=0.5235988f,d40=0.6981317f,d60=1.0471976f,d70=1.2217305f,back=-2.0943952f;
    return {{{0,full},{0,full},{0,full},{0,full},{300,full},{0,full},{0,full},{0,full},{0,full},{5000,full},{5000,full},
             {2000,d40},{2000,d40},{3000,d30},{10000,d20},{8000,d70},{10000,d20},{2000,back},{2000,back},{2000,back},
             {5000,d60},{10000,d60},{5000,d60},{5000,d60},{3000,d20},{10000,d20},{0,full}}};
}
void update_tail(Projectile& p,const Vec3& up,const Vec3& side) {
    for(size_t i=p.tail.size()-1;i>0;--i) p.tail[i]=p.tail[i-1];
    p.tail[0]={p.position,up,side,3};
    p.tail_phase+=0.3f;++p.tail_updates;
    // 0x430110 mode 2: past m_LastFrame (7) the accumulator returns to 0.
    p.film+=1;if(p.film>7) p.film=0;
}
Shield start_shield(BombHit& h,int time) {
    Shield s;s.timer=time;h.guard=2;
    return s;
}
void update_shield(Shield& s,BombHit& h) {
    if(s.timer==0 || h.guard==1) s.state=std::max(s.state,1);
    switch(s.state) {
        case 0:
            if(s.timer>30 && h.guard==8) s.timer=30;
            if(s.timer==0) s.state=1;
            else --s.timer;
            break;
        case 1: h.guard=4;s.timer=6;s.state=2;break;
        case 2:
            if(s.timer<2) s.state=3; // the model hides
            else --s.timer;
            break;
        default: h.guard=1;s.done=true;break;
    }
}
namespace {
float length(const Vec3& v) {return std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);}
}
bool breath_weapon(int icon,Projectile& out) {
    Projectile p;
    switch(icon) {
        case 25: p.type=8;p.object=0x38c;break;
        case 14: p.type=8;p.object=0x39a;p.p.maneuver=20;p.radius=151;break;
        case 16: p.type=2;p.object=0x3a8;break;
        // WPU_Paintball's WPaintShot 0x396 (class 0x4991c0, not traced):
        // provisionally a WBreath with the paint hit type 4.
        case 13: p.type=4;p.object=0x396;break;
        case 15:
            p.type=0x10;p.object=0x3a1;
            p.p.maneuver=50;p.p.angle=2.1f;p.p.range=10000;p.p.travel=10000;
            break;
        default: return false;
    }
    out=p;
    return true;
}
void aim_projectile(Projectile& p,const Vec3& target) {
    Vec3 d{{target[0]-p.position[0],target[1]-p.position[1],target[2]-p.position[2]}};
    const float l=length(d);
    if(l<=0) return;
    for(size_t i=0;i<3;++i) p.velocity[i]=d[i]/l*p.p.speed;
}
int64_t target_distance(const Vec3& d,float heading,float angle,int range) {
    const float r=float(range);
    if(!(std::abs(d[0])<r && std::abs(d[1])<r && std::abs(d[2])<r)) return -1;
    const int64_t x=int64_t(d[0]),y=int64_t(d[1]),z=int64_t(d[2]);
    const int64_t d2=x*x+y*y+z*z;
    if(d2>int64_t(range)*range) return -1;
    float a=std::atan2(-d[0],d[2])-heading;
    a=std::fmod(a,6.2831855f);
    if(a>3.1415927f) a-=6.2831855f;
    else if(a<-3.1415927f) a+=6.2831855f;
    if(!(a<angle && -angle<a)) return -1;
    return d2;
}
void steer_projectile(Projectile& p,const Vec3* target,bool ground,float ground_y,float reach) {
    bool close=false;
    if(target && p.p.maneuver!=0) {
        Vec3 d{{(*target)[0]-p.position[0],(*target)[1]-p.position[1],(*target)[2]-p.position[2]}};
        const float l=length(d);
        close=l<p.p.aim;
        Vec3 steer{};
        if(l>0) for(size_t i=0;i<3;++i) steer[i]=d[i]/l*p.p.speed-p.velocity[i];
        if(!close) steer[1]=0;
        const float sl=length(steer);
        if(sl>0) for(size_t i=0;i<3;++i) p.velocity[i]+=steer[i]/sl*p.p.maneuver;
    }
    if(!close) {
        // Reach H above the ground over the updates it takes to get there
        // (2 H below and 1 update when no ground is found): lowering scaled
        // by loweringspeed / H, climbing at most climb.
        const float floor=ground?ground_y:p.position[1]-2*p.p.height;
        const float t=ground?std::max(1.f,reach):1.f;
        const float e=floor+p.p.height-p.position[1];
        p.velocity[1]=e<0?std::max(e*p.p.lowering/p.p.height/t,-p.p.lowering):std::min(e/t,p.p.climb);
    }
    // 0x475507: the vertical speed is set aside while the horizontal one is
    // capped at ABSspd, then restored.
    const float h=std::sqrt(p.velocity[0]*p.velocity[0]+p.velocity[2]*p.velocity[2]);
    if(h>p.p.speed) {p.velocity[0]*=p.p.speed/h;p.velocity[2]*=p.p.speed/h;}
}
Tnt throw_tnt(const Vec3& position,const Vec3& forward,int owner,const Vec3* target) {
    Tnt t;t.position=position;t.position[1]+=100;t.owner=owner;
    if(target) {
        Vec3 d{{(*target)[0]-t.position[0],(*target)[1]-t.position[1],(*target)[2]-t.position[2]}};
        const float l=length(d);
        for(size_t i=0;i<3;++i) t.velocity[i]=l>0?d[i]/l*tnt_forward:forward[i]*tnt_forward;
        t.velocity[1]+=tnt_rise;
    } else {
        for(size_t i=0;i<3;++i) t.velocity[i]=forward[i]*tnt_forward;
        t.velocity[1]+=tnt_rise;
    }
    return t;
}
bool bounce_tnt(Tnt& t,const Vec3& hit,const Vec3& n,const Vec3* target) {
    if(++t.bounces==3) return false;
    for(size_t i=0;i<3;++i) t.position[i]=hit[i]+n[i]*10;
    t.speed*=tnt_reducer;
    if(target) {
        Vec3 d{{(*target)[0]-t.position[0],(*target)[1]-t.position[1],(*target)[2]-t.position[2]}};
        const float l=length(d);
        if(l>0) for(size_t i=0;i<3;++i) t.velocity[i]=d[i]/l*t.speed;
        t.velocity[1]+=tnt_rise;
    } else {
        const float dn=t.velocity[0]*n[0]+t.velocity[1]*n[1]+t.velocity[2]*n[2];
        for(size_t i=0;i<3;++i) t.velocity[i]-=2*dn*n[i];
        t.velocity[0]*=tnt_reducer;t.velocity[2]*=tnt_reducer;
    }
    return true;
}
BombKick bomb_kick(uint16_t type) {
    BombKick k;
    k.hop=(type&0xda)!=0;
    if(type&0x3fc) k.spin=12.566371f;
    if(type&0x1000) k.spin=-3.1415927f;
    if(type&0x800) k.spin=3.1415927f;
    return k;
}
void request_spin(Spin& s,float v) {
    const float total=s.left+s.done;
    if((v<0 && total>v) || (v>0 && total<v)) s.left=v-s.done;
}
void decay_spin(Spin& s) {
    if(s.left==0) {s=Spin{};return;} // 0x472680
    s.step=std::abs(s.left)<0.05235988f?s.left:s.left*0.11111111f;
    s.left-=s.step;s.done+=s.step;
}
Landmine drop_detour(const Vec3& position,float yaw,int owner,bool left) {
    Landmine m;
    m.position=position;
    m.position[0]+=std::sin(yaw)*270;m.position[2]-=std::cos(yaw)*270;
    m.yaw=yaw;m.owner=owner;m.life=m.lifetime=150;m.gravity=100;m.trap=150;
    m.type=left?0x1000:0x800;m.detour=true;
    return m;
}
Barrier drop_barrier(const Vec3& position,float yaw,int owner,bool tree) {
    Barrier b;
    b.position=position;
    b.position[0]-=std::sin(yaw)*2000;b.position[2]+=std::cos(yaw)*2000;b.position[1]+=1000;
    b.yaw=yaw;b.owner=owner;b.tree=tree;b.scale_num=tree?5:3;
    return b;
}
Landmine drop_oil(const Vec3& position,float yaw,int owner) {
    Landmine m;
    m.position=position;m.position[1]+=10;
    m.yaw=yaw;m.owner=owner;m.oil=true;
    return m;
}
Landmine drop_hive(const Vec3& position,float yaw,int owner) {
    Landmine m=drop_oil(position,yaw,owner);
    m.oil=false;m.hive=true;
    return m;
}
Landmine drop_landmine(const Vec3& position,float yaw,int owner) {
    Landmine m;
    m.position=position;m.position[1]+=10;
    m.yaw=yaw;m.owner=owner;
    return m;
}
}
