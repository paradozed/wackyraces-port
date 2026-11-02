#include "game_material.h"

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) { throw std::runtime_error("game material: "+text); }
uint32_t word(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
float real(const uint8_t* p) {
    const uint32_t bits=word(p);float value;std::memcpy(&value,&bits,4);
    if(!std::isfinite(value)) bad("nonfinite material component");
    return value;
}
ByteView required(const Archive& archive,const std::unordered_map<std::string,Chunk>& chunks,
                   const char* tag,size_t size) {
    const auto found=chunks.find(tag);
    if(found==chunks.end()) bad(std::string("missing ")+tag+" chunk");
    const auto value=archive.payload(found->second);
    if(value.size!=size) bad(std::string("invalid ")+tag+" length");
    return value;
}
} // namespace

const Archive& resolve_resource(const ArchivePool& archives,const std::string& tag,uint32_t id) {
    if(tag.size()!=4) bad("resource tag must contain four bytes");
    const Archive* result=nullptr;
    for(const Archive* archive:archives) {
        if(!archive) bad("null archive in resource pool");
        for(const auto& resource:archive->resources()) {
            if(resource.tag!=tag || resource.id!=id) continue;
            if(result && result!=archive) bad("ambiguous "+tag+" resource "+std::to_string(id));
            result=archive;
        }
    }
    if(!result) bad("missing "+tag+" resource "+std::to_string(id));
    return *result;
}

Material parse_material(const Archive& archive,uint32_t id) {
    const auto& resource=archive.find("GMAT",id);
    const auto header=archive.payload(resource);
    if(header.size<8 || word(header.data+4)!=0x0130e079) bad("unsupported GDMaterial version");
    std::unordered_map<std::string,Chunk> chunks;
    for(const auto& chunk:archive.children(resource))
        if(!chunks.emplace(chunk.tag,chunk).second) bad("duplicate material chunk "+chunk.tag);

    const auto texture_chunk=chunks.find("TEXP");
    if(texture_chunk==chunks.end()) bad("missing TEXP chunk");
    const auto texture=archive.payload(texture_chunk->second);
    if(texture.size<4) bad("truncated TEXP count");
    const uint32_t count=word(texture.data);
    if(count>(texture.size-4)/8 || texture.size-4!=size_t(count)*8) bad("invalid TEXP count/length");
    if(count>1) bad("multiple material texture slots are not implemented");
    Material material;
    if(count==1) {
        const uint8_t* link=texture.data+4;
        // 0x44ac30 first clears its resource link, then only resolves it when
        // the serialized tag is nonzero. An ID alone does not make it textured.
        if(word(link)!=0) {
            if(std::memcmp(link,"GTEX",4)) bad("unsupported material texture reference type");
            material.textured=true;
            material.texture_id=word(link+4);
        }
    }

    const auto colors=required(archive,chunks,"COLS",36);
    for(size_t i=0;i<9;++i) {
        const float value=real(colors.data+i*4);
        if(i>=3 && i<6) material.diffuse[i-3]=value;
    }
    material.transparency=real(required(archive,chunks,"TRAN",4).data);
    material.clamp_axes=word(required(archive,chunks,"CLAM",4).data)&3u;
    material.two_sided=(word(required(archive,chunks,"TWOS",4).data)&1u)!=0;
    // Preserve stored values instead of silently normalizing/clamping them.
    // Glide's constant alpha is (1-TRAN); how that constant participates in
    // every backend/draw-flag mode is a separate renderer-state question.
    return material;
}

Material parse_material(const ArchivePool& archives,uint32_t id) {
    Material material=parse_material(resolve_resource(archives,"GMAT",id),id);
    if(material.textured) resolve_resource(archives,"GTEX",material.texture_id);
    return material;
}
} // namespace wpc
