#include "race_effects.h"
#include <algorithm>

namespace wpc {
namespace {
constexpr uint32_t deep_water=0x800000;
Matrix4 scaled(const Matrix4& base,float x,float y,float z) {
    // A local diag(x,y,z) under the base frame (row scales in WPC's layout).
    Matrix4 m=base;
    for(size_t i=0;i<3;++i) {m[i]*=x;m[4+i]*=y;m[8+i]*=z;}
    return m;
}
}
int CarEffects::puff_reel(uint32_t surface) {
    switch(surface) {
        case 0x20: case 0x80: return 0;
        case deep_water: return 1;
        case 4: return 2;
        case 0x10: return 3;
        default: return -1;
    }
}
float CarEffects::track_width(int vehicle) {
    static const float widths[7]={24,8,6,6,7,16,8};
    return vehicle>=0 && vehicle<7?widths[vehicle]:24;
}
void CarEffects::update_tracks(const Matrix4& frame,const std::array<WheelContact,4>& wheels,bool skid) {
    const Vec3 right{{frame[0],frame[1],frame[2]}};
    for(size_t w=0;w<4;++w) {
        TrackStrip& t=tracks_[w];
        // 0x47a5e3: from segment 14 up, an active segment moves down one and
        // fades by 6; an inactive one clears the next.
        for(size_t i=15;i-->0;) {
            if(t[i].active) {t[i+1]=t[i];t[i+1].fade-=6;}
            else t[i+1].active=false;
        }
        // 0x47a609: no contact (123.456) or another surface leaves a gap;
        // bits 4 and 0xa0 mark with 0x30, bit 2 only while skidding with 0x5c.
        const WheelContact& c=wheels[w];
        TrackSegment& s=t[0];
        int shade=0;
        if(c.hit) {
            if(c.surface&0xa4) shade=0x30;
            else if((c.surface&2) && skid) shade=0x5c;
        }
        if(!shade) {s.active=false;continue;}
        // ftol truncates each coordinate.
        for(size_t i=0;i<3;++i) {
            s.a[i]=std::trunc(c.position[i]-track_width_*right[i]);
            s.b[i]=std::trunc(c.position[i]+track_width_*right[i]);
        }
        s.shade=shade;s.fade=100;s.active=true;
    }
    // 0x47a974 (in the draw): when wheel 0's newest segment lies more than
    // 500 along +X, +Y or +Z from the next one (a respawn), segments 1..15 of
    // every wheel are cleared (0x47b320). Only positive differences count.
    if(tracks_[0][0].a[0]-tracks_[0][1].a[0]>500 || tracks_[0][0].a[1]-tracks_[0][1].a[1]>500 ||
       tracks_[0][0].a[2]-tracks_[0][1].a[2]>500)
        for(auto& t:tracks_) for(size_t i=1;i<16;++i) t[i].active=false;
}
Matrix4 CarEffects::Splash::column() const {return scaled(base,a*0.2f,height*0.2f,a*0.2f);}
Matrix4 CarEffects::Splash::ring() const {return scaled(base,b*0.2f,0.01f*0.2f,b*0.2f);}

void CarEffects::bumm(const Matrix4& frame,float speed) {
    // 0x486960: piece i leaves at 45i degrees, spinning (7i - 30) degrees
    // per update, frame rand() % 5; forward speed adds the car's x 100.
    Bumm b;b.base=frame;
    constexpr float degrees=0.0174533f;
    for(int i=0;i<8;++i) {
        Piece& p=b.pieces[size_t(i)];
        p.position={{0,unit()*100+1,0}};
        const float a=float(45*i)*degrees;
        p.spin=float(7*i-30)*degrees;
        p.velocity[0]=std::sin(a)*(unit()*8+3);
        p.velocity[2]=speed*100+std::cos(a)*(unit()*8+3);
        p.velocity[1]=unit()*18+20;
        p.frame=random_.next()%5;
    }
    bumms_.push_back(b);
}
void CarEffects::update(const Matrix4& frame,float speed,const std::array<WheelContact,4>& wheels,bool splash,
                        const Matrix4& splash_frame,bool skid) {
    ++updates_;
    // 0x47a460: the tracks, then the POUSSIERE puffs, dust and ripples.
    update_tracks(frame,wheels,skid);
    for(auto& b:bumms_) for(auto& p:b.pieces) {
        // 0x486b20: a piece below local height 1 has landed and is hidden.
        if(p.landed) continue;
        if(p.position[1]<1) {p.landed=true;continue;}
        for(size_t i=0;i<3;++i) p.position[i]+=p.velocity[i];
        p.velocity[1]-=2;p.angle+=p.spin;
    }
    // Once every piece has landed the clone waits for its sound, then ends.
    bumms_.erase(std::remove_if(bumms_.begin(),bumms_.end(),[](const Bumm& b){
        return std::all_of(b.pieces.begin(),b.pieces.end(),[](const Piece& p){return p.landed;});}),bumms_.end());
    // Existing clones update first; those spawned below start next update
    // (provisional: the original's object list order is not traced).
    for(auto& r:ripples_) {
        // 0x47c7d0: 15 updates growing by 0.4, then the clone destroys itself.
        if(r.counter==15) r.counter=-1;
        else {r.scale+=0.4f;++r.counter;}
    }
    ripples_.erase(std::remove_if(ripples_.begin(),ripples_.end(),[](const Ripple& r){return r.counter<0;}),ripples_.end());
    for(auto& s:splashes_) {
        // 0x473df0 states: 0 sets up, 1..10 widen the ring, 11 raises and
        // drops the column, 12..30 widen both, 31 ends (sound stopped).
        switch(s.state) {
            case 0: s.height=0;s.a=0.1f;s.b=0.1f;s.rise=0.25f;s.grow_a=0.05f;s.grow_b=0.07f;s.state=1;break;
            case 11:
                s.height+=s.rise;
                if(s.height<0) {s.height=0;s.state=12;}
                else {s.a+=s.grow_a;s.b+=s.grow_b;s.rise-=s.rise<=0?0.01f:0.03f;}
                break;
            case 31: s.state=-1;continue;
            default:
                if(s.state>11) s.a+=s.grow_a;
                ++s.state;s.b+=s.grow_b;
                break;
        }
        s.grow_a-=0.0015f;s.grow_b-=0.0015f;
    }
    splashes_.erase(std::remove_if(splashes_.begin(),splashes_.end(),[](const Splash& s){return s.state<0;}),splashes_.end());
    if(splash) {Splash s;s.base=splash_frame;splashes_.push_back(s);}

    if(smoke_on_) {
        // 0x47b750: the first free slot takes the rear wheel of the update's
        // parity (even 3, odd 2) above speed 0.25, thrown by rand() % 20 - 10
        // along the car's up and right axes; 23 updates.
        const Vec3 up{{frame[4],frame[5],frame[6]}},right{{frame[0],frame[1],frame[2]}};
        bool tried=false;
        for(Smoke& m:smoke_) {
            if(m.age) {
                for(size_t i=0;i<3;++i) m.position[i]+=m.velocity[i];
                if(++m.age==23) m.age=0;
                continue;
            }
            if(tried) continue;
            tried=true;
            if(!(speed>0.25f || speed< -0.25f)) continue;
            const WheelContact& c=wheels[(updates_&1)?2:3];
            if(!c.hit) continue;
            m.position=c.position;
            const float a=float(random_.next()%20-10);
            for(size_t i=0;i<3;++i) m.velocity[i]=a*up[i];
            const float b=float(random_.next()%20-10);
            for(size_t i=0;i<3;++i) m.velocity[i]+=b*right[i];
            m.age=1;
        }
    }
    // 0x47bb10. On even updates a random front wheel on deep water leaves a
    // WalkOnWater ripple at its contact (0x47c680), in the car's frame.
    const int ripple_wheel=random_.next()&1;
    if(!(updates_&1) && wheels[size_t(ripple_wheel)].hit && wheels[size_t(ripple_wheel)].surface==deep_water) {
        Ripple r;r.base=frame;
        for(size_t i=0;i<3;++i) r.base[12+i]=wheels[size_t(ripple_wheel)].position[i];
        ripples_.push_back(r);
    }
    const Vec3 position{{frame[12],frame[13],frame[14]}},right{{frame[0],frame[1],frame[2]}};
    bool spawned=false;
    for(Puff& p:puffs_) {
        if(p.age) {
            // 23 updates; one integration step per update (0x5299b4 / 2).
            if(++p.age==23) {p.age=0;continue;}
            for(size_t i=0;i<3;++i) p.offset[i]+=p.velocity[i];
            p.velocity[1]-=0.3f;
            continue;
        }
        if(spawned) continue;
        // Rear wheels above 0.3, front wheels below -0.1; one puff per update.
        int w;
        if(speed>0.3f) w=(random_.next()&1)+2;
        else if(speed< -0.1f) w=random_.next()&1;
        else continue;
        const WheelContact& c=wheels[size_t(w)];
        if(!c.hit || puff_reel(c.surface)<0) continue;
        spawned=true;
        p.age=1;
        for(size_t i=0;i<3;++i) p.offset[i]=c.position[i]-position[i];
        // Thrown outwards: left wheels (0, 2) along -X, right ones along +X.
        const float f=unit()*3+2,side=(w==0 || w==2)?-1.f:1.f;
        for(size_t i=0;i<3;++i) p.velocity[i]=right[i]*f*side;
        const float up=unit();
        p.velocity[1]=up+1+up+1;
        p.reel=puff_reel(c.surface);
        p.frame=random_.next()&3;
    }
}
}
