#include "game_ground.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) {throw std::runtime_error("game ground: "+text);}
void span(size_t size,size_t at,size_t count) {if(at>size || count>size-at) bad("serialized range exceeds WGNR blob");}
uint16_t half(const std::vector<uint8_t>& b,size_t at) {span(b.size(),at,2);return uint16_t(b[at])|uint16_t(b[at+1])<<8;}
uint32_t word(const std::vector<uint8_t>& b,size_t at) {span(b.size(),at,4);return uint32_t(b[at])|uint32_t(b[at+1])<<8|uint32_t(b[at+2])<<16|uint32_t(b[at+3])<<24;}
int16_t signed_half(const std::vector<uint8_t>& b,size_t at) {const auto bits=half(b,at);int16_t out;std::memcpy(&out,&bits,2);return out;}
int32_t signed_word(const std::vector<uint8_t>& b,size_t at) {const auto bits=word(b,at);int32_t out;std::memcpy(&out,&bits,4);return out;}
size_t pointer(const std::vector<uint8_t>& b,uint32_t value,size_t length=4) {
    const size_t at=size_t(value&0x3fffffff)*4;span(b.size(),at,length);return at;
}
float dot(GroundPoint a,GroundPoint b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
GroundPoint difference(GroundPoint a,GroundPoint b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
GroundPoint interpolate(GroundPoint a,GroundPoint b,float t) {return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
int32_t ten(uint32_t bits) {bits&=1023;return bits&512?int32_t(bits)-1024:int32_t(bits);}
GroundPoint unpack_normal(uint32_t n) {return {ten(n>>20)/256.f,ten(n>>10)/256.f,ten(n)/256.f};}
GroundPoint vertex(const std::vector<uint8_t>& b,size_t at) {return {float(signed_half(b,at)),float(signed_half(b,at+2)),float(signed_half(b,at+4))};}
int32_t integer(float f) {
    if(!std::isfinite(f) || double(f)<std::numeric_limits<int32_t>::min() || double(f)>std::numeric_limits<int32_t>::max())
        bad("query coordinate cannot be represented by original integer conversion");
    return int32_t(std::trunc(f));
}
int16_t quantize(float f) {const uint16_t bits=uint16_t(uint32_t(integer(f)));int16_t out;std::memcpy(&out,&bits,2);return out;}
struct Pair {int16_t x,y;};
Pair project(GroundPoint v,unsigned axis) {
    if(axis==0) return {quantize(v.y),quantize(v.z)};
    if(axis==1) return {quantize(v.z),quantize(v.x)};
    return {quantize(v.x),quantize(v.y)};
}
int32_t determinant(int32_t ax,int32_t ay,int32_t bx,int32_t by) {
    // FUN_004833a0 uses 32-bit integer multiplication/subtraction.
    const uint32_t bits=uint32_t(ax)*uint32_t(by)-uint32_t(ay)*uint32_t(bx);
    int32_t out;std::memcpy(&out,&bits,4);return out;
}
bool triangle_contains(const std::array<Pair,3>& v,Pair p) {
    // Original dominant-axis projection, short-coordinate truncation,
    // quadrant table at 0x52b547 and 0x4833a0 determinant classification.
    std::array<uint8_t,3> code{};
    for(size_t i=0;i<3;++i) code[i]=uint8_t((1u<<(v[i].x<p.x?3:2))+(1u<<(v[i].y<p.y?1:0)));
    if(code[0]&code[1]&code[2]) return false;
    const bool shares=(code[1]&code[0])!=0;
    const unsigned a=shares&&(code[2]&code[0])?1:0,b=shares?2:1,c=3-a-b;
    constexpr uint8_t table[16]={0,0,0,0,0,9,5,0,0,10,6,0,0,0,0,0};
    auto det=[&](unsigned i,unsigned j){return determinant(int(v[i].x)-p.x,int(v[i].y)-p.y,int(v[j].x)-p.x,int(v[j].y)-p.y);};
    uint8_t first=table[code[a]];
    if(det(a,b)<0) first^=15;
    uint8_t second=code[c];
    if(second==code[a]) {second=table[code[b]];if(det(b,c)<0) second^=15;}
    else if(second==code[b]) {second=table[second];if(det(c,a)<0) second^=15;}
    return first!=second;
}
int side(float ax,float az,float bx,float bz) {
    const float scale=std::max({std::abs(ax),std::abs(az),std::abs(bx),std::abs(bz)});
    if(scale!=0) {ax/=scale;az/=scale;bx/=scale;bz/=scale;}
    const float d=az*bx-ax*bz;
    if(d>.006f) return 1;
    if(d<-.006f) return -1;
    return 0;
}
}

void GroundResource::load(const Archive& archive,uint32_t id) {
    const auto data=archive.payload(archive.find("WGNR",id));
    load_bytes(std::vector<uint8_t>(data.data,data.data+data.size));
    if(id_!=id) bad("resource ID mismatch");
}
void GroundResource::load_bytes(std::vector<uint8_t> payload) {
    if(payload.size()<12 || word(payload,4)!=0x0130e39a) bad("invalid Wacky Ground version/header");
    const size_t length=word(payload,8);
    if(length>256*1024*1024 || payload.size()-12!=length) bad("invalid WGNR blob length");
    GroundResource next;next.id_=word(payload,0);next.bytes_.assign(payload.begin()+12,payload.end());
    const auto& bytes=next.bytes_;
    span(bytes.size(),0,16);
    if(word(bytes,4)!=0) bad("WGNR blob contains already-relocated pointers");
    const size_t primary=pointer(bytes,word(bytes,8),32);
    size_t cursor=primary+4;
    const uint32_t row_count=word(bytes,primary);
    if(row_count>bytes.size()/4) bad("invalid primary row count");
    for(uint32_t i=0;i<row_count;++i) {
        const size_t count=word(bytes,cursor);cursor+=4;
        if(count>(bytes.size()-cursor)/4) bad("truncated primary row");
        for(size_t j=0;j<count;++j) {const uint32_t value=word(bytes,cursor+j*4);if(value&0x80000000) pointer(bytes,value);}
        cursor+=count*4;
    }
    const size_t linked_count=word(bytes,12);
    if(linked_count>(bytes.size()-16)/4) bad("invalid linked row count");
    for(size_t i=0;i<linked_count;++i) {
        uint32_t link=word(bytes,16+i*4);
        if(!link) continue;
        std::set<size_t> visited;
        for(;;) {
            const size_t row=pointer(bytes,link,8);
            if(!visited.insert(row).second) bad("cyclic linked row");
            pointer(bytes,word(bytes,row+4));
            link=word(bytes,row);
            if(link==0xffffffff) break;
        }
    }
    const size_t count=half(bytes,primary+8),records=pointer(bytes,word(bytes,primary+12),count*28);
    std::map<size_t,size_t> geometries;
    for(size_t i=0;i<count;++i) {
        const size_t at=records+i*28,raw=pointer(bytes,word(bytes,at),20);
        auto found=geometries.find(raw);
        if(found==geometries.end()) {
            Geometry g{pointer(bytes,word(bytes,raw+4),0),pointer(bytes,word(bytes,raw+8),0),
                       pointer(bytes,word(bytes,raw+12),0),pointer(bytes,word(bytes,raw+16),0)};
            if(!(g.polygons<=g.vertices && g.vertices<=g.planes && g.planes<=g.end) ||
               (g.planes-g.vertices)%8 || (g.end-g.planes)%4) bad("invalid geometry stream boundaries");
            const size_t index=next.geometries_.size();next.geometries_.push_back(g);found=geometries.emplace(raw,index).first;
        }
        next.records_.push_back({found->second,word(bytes,at+8),half(bytes,at+4)});
    }
    for(size_t grid_index=0;grid_index<2;++grid_index) {
        // 0x482560 reaches the first linked row's +4 pointer, which points
        // four bytes past the primary-table prefix. Its +0x18 grid is thus
        // primary+0x1c. The other descriptor has a different consumer.
        const size_t at=pointer(bytes,word(bytes,primary+28-grid_index*4),24);
        Grid g{bytes[at],bytes[at+2],signed_word(bytes,at+4),signed_word(bytes,at+8),half(bytes,at+12),half(bytes,at+14),word(bytes,at+16),word(bytes,at+20)};
        if(g.shift_x>31 || g.shift_z>31) bad("invalid spatial-grid shift");
        const size_t cells=size_t(g.width)*g.height;
        if(cells>4*1024*1024) bad("spatial grid exceeds limit");
        span(bytes.size(),g.cells,cells*2);span(bytes.size(),g.links,0);
        next.grids_[grid_index]=g;
        if(grid_index!=0) continue;
        std::set<size_t> validated;
        for(size_t cell=0;cell<cells;++cell) {
            const uint16_t entry=half(bytes,g.cells+cell*2);
            if(!entry) continue;
            size_t link=g.links+size_t(entry>>1)*4;
            if(!validated.insert(link).second) continue;
            for(;;link+=4) {
                const uint16_t packed=half(bytes,link),poly=half(bytes,link+2);
                const size_t index=packed>>6;
                if(index>=next.records_.size()) bad("grid link references missing record");
                const Geometry& geometry=next.geometries_[next.records_[index].geometry];
                const size_t polygon=geometry.polygons+size_t(poly)*4;
                span(geometry.vertices,polygon,6);
                const uint32_t flags=word(bytes,polygon);
                const size_t vertices=flags&0x08000000?4:3;
                span(geometry.vertices,polygon,6+vertices*2);
                const size_t plane=half(bytes,polygon+4)&0xfff;
                if(plane>=(geometry.end-geometry.planes)/4) bad("polygon references missing plane");
                if((flags&0x28000000)==0x28000000 && plane+1>=(geometry.end-geometry.planes)/4) bad("quad references missing second plane");
                for(size_t v=0;v<vertices;++v) if(half(bytes,polygon+6+v*2)>=(geometry.planes-geometry.vertices)/8) bad("polygon references missing vertex");
                if(!(packed&1)) break;
            }
        }
    }
    *this=std::move(next);
}

RouteNode GroundResource::route_node(uint32_t record_index,uint32_t polygon) const {
    if(record_index>=records_.size()) bad("route node in a missing record");
    const Geometry& g=geometries_[records_[record_index].geometry];
    const size_t at=g.polygons+size_t(polygon)*4;
    span(g.vertices,at,8);
    RouteNode n;n.polygon=polygon;
    const uint32_t flags=word(bytes_,at);
    n.vertex_count=flags&0x08000000?4:3;
    const size_t header=(6+n.vertex_count*2+3)&~size_t(3);
    span(g.vertices,at,header+8);
    for(size_t v=0;v<n.vertex_count;++v) {
        n.vertices[v]=half(bytes_,at+6+v*2);
        if(n.vertices[v]>=(g.planes-g.vertices)/8) bad("route node references a missing vertex");
    }
    n.flags=word(bytes_,at+header);
    const uint16_t counts=half(bytes_,at+header+6);
    n.remaining=uint8_t(counts>>8);
    const size_t forward=(counts>>4)&15,backward=counts&15;
    span(g.vertices,at+header+8,(forward+backward)*12);
    for(size_t i=0;i<forward+backward;++i) {
        const size_t l=at+header+8+i*12;
        RouteLink link{half(bytes_,l),half(bytes_,l+2),{float(signed_half(bytes_,l+4)),float(signed_half(bytes_,l+6)),float(signed_half(bytes_,l+8))}};
        (i<forward?n.forward:n.backward).push_back(link);
    }
    return n;
}
GroundPoint GroundResource::vertex_point(uint32_t record_index,uint16_t index) const {
    if(record_index>=records_.size()) bad("vertex in a missing record");
    const Geometry& g=geometries_[records_[record_index].geometry];
    if(index>=(g.planes-g.vertices)/8) bad("missing route vertex");
    return vertex(bytes_,g.vertices+size_t(index)*8);
}
GroundQueryResult GroundResource::query_local(GroundPoint start,GroundPoint end,uint32_t mask) const {
    if(bytes_.empty()) bad("query requires a loaded resource");
    for(float coordinate:{start.x,start.y,start.z,end.x,end.y,end.z}) integer(coordinate);
    GroundQueryResult out;
    const Grid& g=grids_[0];
    auto grid_coordinate=[](int32_t coordinate,int32_t origin,uint8_t shift)->int64_t {
        // Arithmetic right shift, including negative cells, without relying
        // on implementation-defined signed shift behavior.
        const int64_t delta=int64_t(coordinate)-origin,divisor=int64_t(1)<<shift;
        return delta>=0?delta/divisor:-((-delta+divisor-1)/divisor);
    };
    int64_t x0=grid_coordinate(integer(start.x),g.origin_x,g.shift_x),x1=grid_coordinate(integer(end.x),g.origin_x,g.shift_x);
    int64_t z0=grid_coordinate(integer(start.z),g.origin_z,g.shift_z),z1=grid_coordinate(integer(end.z),g.origin_z,g.shift_z);
    if(x0>x1) std::swap(x0,x1);
    if(z0>z1) std::swap(z0,z1);
    if(x1<0 || z1<0 || x0>=g.width || z0>=g.height) return out;
    x0=std::max<int64_t>(0,x0);x1=std::min<int64_t>(g.width-1,x1);
    z0=std::max<int64_t>(0,z0);z1=std::min<int64_t>(g.height-1,z1);
    const int32_t min_y=std::min(integer(start.y),integer(end.y)),max_y=std::max(integer(start.y),integer(end.y));
    auto unsupported=[&](const std::string& reason){out.complete=false;if(std::find(out.unsupported.begin(),out.unsupported.end(),reason)==out.unsupported.end()) out.unsupported.push_back(reason);};
    for(int64_t z=z0;z<=z1;++z) for(int64_t x=x0;x<=x1;++x) {
        const uint16_t entry=half(bytes_,g.cells+(size_t(z)*g.width+size_t(x))*2);
        if(!entry) continue;
        for(size_t link=g.links+size_t(entry>>1)*4;;link+=4) {
            const uint16_t packed=half(bytes_,link);
            const uint32_t record_index=packed>>6;
            const Record& record=records_[record_index];
            const Geometry& geometry=geometries_[record.geometry];
            bool accepted=false;
            if(record.flags&mask) {
                const size_t polygon=geometry.polygons+size_t(half(bytes_,link+2))*4;
                const uint32_t flags=word(bytes_,polygon);
                const bool quad=(flags&0x08000000)!=0;
                auto point=[&](unsigned i){return vertex(bytes_,geometry.vertices+size_t(half(bytes_,polygon+6+i*2))*8);};
                const bool special=(record.flags&0x00600000)!=0;
                if(special || (max_y>=point((packed>>3)&3).y && min_y<=point((packed>>1)&3).y)) {
                    const uint32_t plane=half(bytes_,polygon+4)&0xfff;
                    // 0x482b30: a quad with polygon bit 0x20000000 is tested as two
                    // sub-polygons with consecutive planes; otherwise one plane.
                    const bool split=!special && quad && (flags&0x20000000);
                    for(unsigned part=0;part<(split?2u:1u);++part) {
                    GroundPoint normal=unpack_normal(word(bytes_,geometry.planes+size_t(plane+part)*4));
                    GroundPoint surface{},hit_point{};
                    float denominator=0,numerator=0,fraction=0;
                    bool possible=true;
                    if(special) {
                        if(!quad || (start.x==end.x && start.z==end.z)) possible=false;
                        else if(record.matrix) {unsupported("directional polygon requires runtime matrix index "+std::to_string(record.matrix));possible=false;}
                        else {
                            surface=point((packed>>3)&3);const GroundPoint other=point(3-((packed>>3)&3));
                            int a=side(other.x-surface.x,other.z-surface.z,start.x-surface.x,start.z-surface.z);
                            int b=side(other.x-surface.x,other.z-surface.z,end.x-surface.x,end.z-surface.z);
                            if(a && a==b) possible=false;
                            a=side(end.x-start.x,end.z-start.z,surface.x-start.x,surface.z-start.z);
                            b=side(end.x-start.x,end.z-start.z,other.x-start.x,other.z-start.z);
                            if(a && a==b) possible=false;
                            denominator=(start.x-end.x)*normal.x+(start.z-end.z)*normal.z;
                            numerator=(start.x-surface.x)*normal.x+(start.z-surface.z)*normal.z;
                            if(denominator<0 && !(record.flags&0x00200000)) possible=false;
                        }
                    } else {
                        surface=point(1);denominator=dot(difference(start,end),normal);numerator=dot(difference(start,surface),normal);
                        if(denominator<0 && !(record.flags&0x01000000)) possible=false;
                    }
                    if(denominator==0 || (denominator>0 && (numerator<0 || numerator>denominator)) ||
                       (denominator<0 && (numerator>0 || numerator<denominator))) possible=false;
                    if(possible) {
                        fraction=numerator/denominator;hit_point=interpolate(start,end,fraction);
                        const bool secondary=(record.flags&0x80000000)!=0;
                        const bool existing=secondary?out.secondary_hit:out.hit;
                        const float limit=existing?(secondary?out.secondary.fraction:out.contact.fraction):10.f;
                        if(fraction>=limit) possible=false;
                        if(special) {
                            if((record.flags&0x00100000) && hit_point.y<surface.y) possible=false;
                            if(denominator<0 && (record.flags&0x00200000)) normal={-normal.x,-normal.y,-normal.z};
                        } else {
                            const float ax=std::abs(normal.x),ay=std::abs(normal.y),az=std::abs(normal.z);
                            const unsigned axis=ax>ay&&ax>az?0:ay>az?1:2;
                            const Pair p=project(hit_point,axis);
                            bool inside=false;
                            if(!split || part==0) inside=triangle_contains({{project(point(0),axis),project(point(1),axis),project(point(2),axis)}},p);
                            if(!inside && quad && (!split || part==1))
                                inside=triangle_contains({{project(point(3),axis),project(point(1),axis),project(point(2),axis)}},p);
                            if(!inside) possible=false;
                        }
                        if(possible) {
                            GroundHit hit{fraction,hit_point,normal,id_,record_index,record.flags,half(bytes_,link+2)};
                            if(secondary) {out.secondary_hit=true;out.secondary=hit;} else {out.hit=true;out.contact=hit;}
                            accepted=true;
                        }
                    }
                    }
                }
            }
            if((accepted && !(mask&0x80000000)) || !(packed&1)) break;
        }
    }
    return out;
}
}
