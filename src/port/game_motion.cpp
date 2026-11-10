#include "game_motion.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("game motion: "+text);}
uint32_t u32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float real(const uint8_t* p) {const uint32_t bits=u32(p);float v;std::memcpy(&v,&bits,4);return v;}
constexpr uint32_t version=0x1310988;
}
MotionLib parse_motion(const Archive& archive,uint32_t id) {
    const ByteView b=archive.payload(archive.find("MTNL",id));
    if(b.size<28) bad("truncated MTNL header");
    if(u32(b.data)!=id) bad("MTNL identity mismatch");
    if(u32(b.data+4)!=version) bad("invalid GDMotion version");
    const uint8_t* p=b.data+8;
    MotionLib lib;
    lib.opaque=u32(p);
    const uint32_t clips=u32(p+4),indices=u32(p+8),keys=u32(p+12);
    lib.segments=u32(p+16);
    if(clips>4096 || indices>1u<<20 || keys>1u<<20 || lib.segments>4096) bad("implausible MTNL counts");
    const size_t need=28+size_t(clips)*20+size_t(indices)*4+size_t(keys)*16;
    if(b.size!=need) bad("MTNL size does not match its counts");
    p+=20;
    for(uint32_t i=0;i<clips;++i,p+=20) {
        // Serialized order: controller, next, duration, channels, first index.
        MotionClip c{int32_t(u32(p)),int32_t(u32(p+4)),int32_t(u32(p+8)),u32(p+12),u32(p+16)};
        if(c.controller<-1 || c.controller>255) bad("MTNL clip controller out of range");
        if(c.next<-1 || c.next>=int32_t(clips)) bad("MTNL clip chain out of range");
        if(c.first_index>indices || c.channels>indices-c.first_index) bad("MTNL clip channels outside the index array");
        lib.clips.push_back(c);
    }
    for(uint32_t i=0;i<indices;++i,p+=4) {
        lib.indices.push_back(u32(p));
        // MTNL 0x7f9's second clip (duration -570) points channels at the
        // key count itself: one past the end, accepted but never read.
        if(lib.indices.back()>keys) bad("MTNL channel index outside the keys");
    }
    for(uint32_t i=0;i<keys;++i,p+=16) {
        MotionKey k{int32_t(u32(p)),real(p+4),real(p+8),real(p+12)};
        if(!std::isfinite(k.x)||!std::isfinite(k.y)||!std::isfinite(k.z)) bad("nonfinite MTNL key");
        lib.keys.push_back(k);
    }
    // 0x43fd10 sizes every controller from clip 0's channel count.
    for(const auto& c:lib.clips) if(c.channels!=lib.clips[0].channels) bad("MTNL clips differ in channel count");
    return lib;
}
MotionLib parse_motion(const ArchivePool& archives,uint32_t id) {return parse_motion(resolve_resource(archives,"MTNL",id),id);}

float motion_value(const std::array<float,2>& p) {
    float b=p[1];
    if(p[0]!=-1.f) {
        if(b>=0.f) {if(p[0]>0.f) b=p[0];}
        else if(p[0]>0.f) b+=p[0];
    }
    return b;
}
namespace {
void merge(std::array<float,2>& p,float v) {
    if(p[0]==-1.f) return;
    if(v<=0.f) {if(v<p[1]) p[1]=v;}
    else if(p[0]<v) p[0]=v;
}
}
MotionController::MotionController(const MotionLib& lib):lib_(lib) {
    // 0x43fda0 reset: speed 1, time 0, channels cleared.
    if(!lib.clips.empty()) channels_.resize(lib.clips[0].channels);
}
void MotionController::play(size_t clip,int32_t offset,float weight) {
    if(clip>=lib_.clips.size()) bad("motion clip out of range");
    weight_=weight;
    next_=&lib_.clips[clip];end_=time_-1;next_start_=time_+offset;
    flags_=(flags_&~0xeu)|1;
    update(0,nullptr);
}
void MotionController::accumulate(MotionPairs& pairs) const {
    // 0x43fe10 and the tail of 0x43feb0: channel c fills pairs 3c..3c+2.
    for(size_t c=0;c<channels_.size();++c) for(size_t i=0;i<3;++i) {
        const size_t slot=c*3+i;
        if(slot<pairs.size()) merge(pairs[slot],channels_[c].value[i]);
    }
}
void MotionController::update(int32_t dt,MotionPairs* pairs) {
    if(!(flags_&1)) return;
    if(flags_&4) {if(pairs) accumulate(*pairs);return;}
    const int32_t step=int32_t(float(dt)*speed_);
    time_+=step;
    if(time_>end_) {
        if(flags_&2) {flags_&=~3u;return;}
        int32_t at=next_start_;
        const MotionClip* clip=nullptr;
        do {
            clip=next_;start_=at;
            if(!clip) {flags_&=~1u;return;}
            at+=clip->duration;
            next_=clip->next>=0?&lib_.clips[size_t(clip->next)]:nullptr;
            end_=next_start_=at;
        } while(at<time_);
        for(size_t c=0;c<channels_.size();++c) {
            Channel& ch=channels_[c];
            const uint32_t index=lib_.indices[clip->first_index+c];
            ch.key=index<lib_.keys.size()?&lib_.keys[index]:nullptr;
            ch.next_time=std::numeric_limits<int32_t>::min();ch.now=time_-1;
        }
        if(flags_&8) {
            flags_=(flags_&~8u)|4;
            if(pairs) accumulate(*pairs);
            time_-=step;
            return;
        }
    }
    if(!pairs) return;
    const int32_t local=time_-start_;
    const MotionKey* last=lib_.keys.data()+lib_.keys.size()-1;
    for(Channel& ch:channels_) {
        if(!ch.key) continue; // past the keys: the original reads beyond them
        if(local>ch.next_time) {
            // Step to the first key at or after the local time. The original
            // reads past a channel's keys if the clip outlasts them; the port
            // stops at the library's last key.
            const MotionKey* k;
            do {k=ch.key;if(ch.key<last) ++ch.key;} while(k->time<local && k<last);
            ch.next_time=k->time;
            ch.from=ch.now;
            ch.interval=k->time+start_-ch.from;
            const float rate=ch.interval?1.f/float(ch.interval):0.f;
            const float key[3]={k->x,k->y,k->z};
            for(size_t i=0;i<3;++i) {
                ch.rate[i]=rate*ch.value[i];
                ch.target[i]=weight_==0.f?0.f:rate*weight_*key[i];
            }
        }
        // Linear from the value at the segment start to weight x key.
        const int32_t since=time_-ch.from;
        for(size_t i=0;i<3;++i) ch.value[i]=float(since)*ch.target[i]+float(ch.interval-since)*ch.rate[i];
        ch.now=time_;
    }
    accumulate(*pairs);
}

void pose_mesh(Mesh& mesh,const MotionPairs& pairs,std::vector<std::array<float,12>>* segments) {
    const size_t n=mesh.segments.size();
    if(pairs.size()!=n*9) bad("motion pairs do not match the segments");
    if(mesh.rest.size()!=mesh.vertices.size() || mesh.vertex_segment.size()!=mesh.vertices.size()) bad("mesh has no segment rest pose");
    // Row-vector 3x3 basis plus translation, as WPC's 0x34-byte matrices.
    struct M {std::array<float,9> r;std::array<float,3> t;};
    std::vector<M> world(n);
    constexpr float degrees=0.0174533f; // 0x511518
    for(size_t s=0;s<n;++s) {
        float v[9];
        for(size_t i=0;i<9;++i) v[i]=motion_value(pairs[s*9+i]);
        M m{{1,0,0,0,1,0,0,0,1},{0,0,0}};
        if(v[6]!=0 || v[7]!=0 || v[8]!=0) // 0x43b190 scales the basis rows
            for(size_t row=0;row<3;++row) for(size_t c=0;c<3;++c) m.r[row*3+c]*=1+v[6+row];
        const auto turn=[&](float angle,size_t a,size_t b,float sign) {
            // 0x43b2b0 (rows 1,2), 0x43b4a0 (rows 0,2), 0x43b690 (rows 0,1).
            if(angle==0) return;
            const float c=std::cos(angle*degrees),sn=std::sin(angle*degrees);
            for(size_t i=0;i<3;++i) {
                const float x=m.r[a*3+i],y=m.r[b*3+i];
                m.r[a*3+i]=c*x-sign*sn*y;m.r[b*3+i]=sign*sn*x+c*y;
            }
        };
        turn(v[3],1,2,1);turn(v[4],0,2,-1);turn(v[5],0,1,1);
        const MeshSegment& seg=mesh.segments[s];
        m.t={seg.x+v[0],seg.y+v[1],seg.z+v[2]};
        if(seg.parent!=UINT32_MAX) {
            // 0x43c620: this x parent.
            const M& p=world[seg.parent];
            M out{};
            for(size_t row=0;row<3;++row) for(size_t c=0;c<3;++c)
                out.r[row*3+c]=m.r[row*3]*p.r[c]+m.r[row*3+1]*p.r[3+c]+m.r[row*3+2]*p.r[6+c];
            for(size_t c=0;c<3;++c) out.t[c]=m.t[0]*p.r[c]+m.t[1]*p.r[3+c]+m.t[2]*p.r[6+c]+p.t[c];
            m=out;
        }
        world[s]=m;
    }
    if(segments) {
        segments->resize(n);
        for(size_t s=0;s<n;++s) {for(size_t i=0;i<9;++i) (*segments)[s][i]=world[s].r[i];for(size_t i=0;i<3;++i) (*segments)[s][9+i]=world[s].t[i];}
    }
    for(size_t i=0;i<mesh.vertices.size();++i) {
        // 0x43b900: v x basis + translation.
        const M& m=world[mesh.vertex_segment[i]];
        const auto& r=mesh.rest[i];
        auto& out=mesh.vertices[i];
        out.x=r[0]*m.r[0]+r[1]*m.r[3]+r[2]*m.r[6]+m.t[0];
        out.y=r[0]*m.r[1]+r[1]*m.r[4]+r[2]*m.r[7]+m.t[1];
        out.z=r[0]*m.r[2]+r[1]*m.r[5]+r[2]*m.r[8]+m.t[2];
    }
}

std::array<float,3> body_sway_gains(int vehicle) {
    // Wine (cc_t1, vehicle 3): [car +0x24f] +0x17e = (0.7, 0.25, 0.25).
    static const std::array<float,3> table[7]={{{0.6f,0.25f,0.3f}},{{0.7f,0.25f,0.4f}},{{0.7f,0.25f,0.4f}},{{0.7f,0.25f,0.25f}},
                                               {{0.7f,0.25f,0.6f}},{{0.7f,0.25f,0.4f}},{{0.7f,0.25f,0.3f}}};
    if(vehicle<0 || vehicle>=7) return {{0.25f,0.25f,0.5f}};
    return table[vehicle];
}
SwayController::Biquad::Biquad(float gain,float alpha,float omega) {
    // 0x440ab0: c0 = 2 a cos w, c1 = -a^2, b0 = (1 - c0 - c1) x gain x
    // 0.28985509, b1 = 1.8 b0, b2 = 0.65 b0 (unit gain at rest x gain).
    c0=2*alpha*std::cos(omega);c1=-alpha*alpha;
    b0=(1-c0-c1)*gain*0.28985509f;b1=b0*1.8f;b2=b0*0.65f;
}
float SwayController::Biquad::step(float x) {
    // 0x440b40: w = c1 s1 + c0 s0 + x; y = b1 s0 + b2 s1 + b0 w.
    const float w=c1*s1+c0*s0+x,y=b1*s0+b2*s1+b0*w;
    s1=s0;s0=w;return y;
}
SwayController::SwayController(const std::array<float,3>& r) {
    // 0x440d40 with the gains of 0x440be0: (gain, alpha, omega) per filter.
    filters_={{Biquad(0.1f,0.2f,0.35f),Biquad(0.8f,0.6f,0.3f),Biquad(0.1f,0.3f,0.3f),
               Biquad(r[0],0.7f,0.4f),Biquad(r[1],0.9f,0.5f),Biquad(r[2],0.9f,0.4f),
               Biquad(0.1f,0.9f,0.6f)}};
}
void SwayController::update(const std::array<float,16>& m,MotionPairs& pairs) {
    const std::array<float,3> p{{m[12],m[13],m[14]}};
    if(!started_) {position_=p;started_=true;}   // 0x440be0 takes the position, velocity 0
    std::array<float,3> v{},a{};
    for(size_t i=0;i<3;++i) {v[i]=p[i]-position_[i];a[i]=v[i]-velocity_[i];}
    position_=p;velocity_=v;
    // 0x43ba80: into model space.
    const std::array<float,3> l{{m[0]*a[0]+m[1]*a[1]+m[2]*a[2],m[4]*a[0]+m[5]*a[1]+m[6]*a[2],m[8]*a[0]+m[9]*a[1]+m[10]*a[2]}};
    // 0x452ed0: (0, 1, 0.2) x l.
    const std::array<float,3> r{{1*l[2]-0.2f*l[1],0.2f*l[0]-0*l[2],0*l[1]-1*l[0]}};
    float in[6]={-l[0],-l[1],-l[2],r[0],r[1],r[2]};
    for(size_t i=0;i<6;++i) {in[i]=std::clamp(in[i],-15.f,15.f);values_[i]=filters_[i].step(in[i]);}
    values_[6]=0;values_[8]=0;
    values_[7]=filters_[6].step(std::clamp(l[1]*-0.01f,-20.f,20.f));
    // 0x4410f1: every segment, weight +0x20 = 1.
    for(size_t seg=0;seg+1<=pairs.size()/9;++seg) for(size_t k=0;k<9;++k) {
        auto& pr=pairs[seg*9+k];const float x=values_[k];
        if(pr[0]==-1.f) continue;
        if(x>0) {if(x>pr[0]) pr[0]=x;}
        else if(x<pr[1]) pr[1]=x;
    }
}
}
