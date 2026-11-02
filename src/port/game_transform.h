#ifndef WPC_GAME_TRANSFORM_H
#define WPC_GAME_TRANSFORM_H
#include "game_scene.h"
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace wpc {
using Matrix4=std::array<float,16>;
inline Matrix4 identity_matrix() {return {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};}
inline Matrix4 multiply_matrix(const Matrix4& a,const Matrix4& b) {
    Matrix4 out{};
    for(size_t c=0;c<4;++c) for(size_t r=0;r<4;++r) for(size_t k=0;k<4;++k)
        out[c*4+r]+=a[k*4+r]*b[c*4+k];
    for(float v:out) if(!std::isfinite(v)) throw std::runtime_error("Nonfinite composed transform");
    return out;
}
inline Matrix4 affine_matrix(const std::array<float,12>& raw) {
    Matrix4 out=identity_matrix();
    for(size_t c=0;c<3;++c) for(size_t r=0;r<3;++r) out[c*4+r]=raw[c*3+r];
    for(size_t r=0;r<3;++r) out[12+r]=raw[9+r];
    for(float v:out) if(!std::isfinite(v)) throw std::runtime_error("Nonfinite affine transform");
    return out;
}
inline Matrix4 node_transform(const Node& node) {
    const auto found=node.properties.find(0x1b);
    if(found==node.properties.end()) return identity_matrix();
    if(found->second.size()!=48) throw std::runtime_error("Invalid node affine length");
    std::array<float,12> raw{};
    for(size_t i=0;i<12;++i) {
        const uint8_t* p=found->second.data()+4*i;
        const uint32_t bits=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
        std::memcpy(&raw[i],&bits,4);
    }
    return affine_matrix(raw);
}
// Original 0x420110 constructs root+0x98 as identity and keeps the root's
// local matrix for the alternate/wrapped +0xcc record. Initial group-zero
// marker/ground queries use the ordinary hierarchy below that root.
inline Matrix4 unwrapped_scene_transform(const Node& root,uint32_t id) {
    Matrix4 result=identity_matrix();bool found=root.id==id;
    const auto visit=[&](const auto& self,const Node& n,const Matrix4& parent)->void {
        if(found) return;
        const Matrix4 world=multiply_matrix(parent,node_transform(n));
        if(n.id==id) {result=world;found=true;return;}
        for(const auto& c:n.children) self(self,c,world);
    };
    for(const auto& c:root.children) visit(visit,c,identity_matrix());
    if(!found) throw std::runtime_error("Missing scene transform for node "+std::to_string(id));
    return result;
}
inline Matrix4 inverse_affine_matrix(const Matrix4& m) {
    const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
    const double det=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
    if(!std::isfinite(det) || std::abs(det)<1e-12) throw std::runtime_error("Singular camera transform");
    Matrix4 out=identity_matrix();
    out[0]=float((e*i-f*h)/det);out[4]=float((c*h-b*i)/det);out[8]=float((b*f-c*e)/det);
    out[1]=float((f*g-d*i)/det);out[5]=float((a*i-c*g)/det);out[9]=float((c*d-a*f)/det);
    out[2]=float((d*h-e*g)/det);out[6]=float((b*g-a*h)/det);out[10]=float((a*e-b*d)/det);
    for(size_t r=0;r<3;++r) out[12+r]=-(out[r]*m[12]+out[4+r]*m[13]+out[8+r]*m[14]);
    for(float v:out) if(!std::isfinite(v)) throw std::runtime_error("Nonfinite camera inverse");
    return out;
}
// Recovered 0x435410, with Vulkan Y inversion and [0,1] depth.
inline Matrix4 camera_projection(const Node& camera) {
    const float fov=camera.real(0x2c),ratio=float(camera.integer(0x2d));
    const float near=camera.real(0x2e),far=camera.real(0x2f);
    if(!std::isfinite(fov)||!std::isfinite(ratio)||!std::isfinite(near)||!std::isfinite(far)||
       fov<=0||fov>=3.141592654f||ratio<=0||near<=0||far<=near)
        throw std::runtime_error("Invalid serialized camera projection");
    const float root=std::sqrt(ratio*.0075f),tangent=std::tan(fov*.5f);
    Matrix4 out{};out[0]=root/tangent;out[5]=-1/(root*tangent);
    out[10]=far/(far-near);out[11]=1;out[14]=-far*near/(far-near);
    return out;
}
}
#endif
