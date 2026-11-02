#include "game_mesh.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace wpc {
namespace {
[[noreturn]] void bad(const std::string& text) { throw std::runtime_error("game mesh: " + text); }
uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
float real(const uint8_t* p) {
    const uint32_t bits = u32(p); float result;
    std::memcpy(&result, &bits, 4);
    if (!std::isfinite(result)) bad("nonfinite geometry component");
    return result;
}
void extent(size_t size, size_t start, size_t count, const char* name) {
    if (start > size || count > size-start) bad(std::string("out-of-range ")+name);
}
void counted(ByteView data, uint32_t count, size_t stride, size_t prefix, const char* name) {
    if (data.size < prefix || count > (data.size-prefix)/stride || data.size-prefix != size_t(count)*stride)
        bad(std::string("invalid ")+name+" count/length");
}
uint32_t count(ByteView data, const char* name) {
    if (data.size < 4) bad(std::string("truncated ")+name+" count");
    return u32(data.data);
}
using ChunkMap = std::unordered_map<std::string, Chunk>;
ChunkMap index(const std::vector<Chunk>& children) {
    ChunkMap result;
    for (const auto& child : children)
        if (!result.emplace(child.tag,child).second) bad("duplicate mesh chunk "+child.tag);
    return result;
}
const Chunk& required(const ChunkMap& chunks, const char* name) {
    const auto found = chunks.find(name);
    if (found == chunks.end()) bad(std::string("missing mesh chunk ")+name);
    return found->second;
}
struct Set { uint32_t material, vertex_start, vertex_count, triangle_start, triangle_count; };
struct Segment { uint32_t parent, set_count, set_start, vertex_count, vertex_start, map_start; float x,y,z; };
} // namespace

Mesh load_mesh(const Archive& archive, const std::string& tag, uint32_t id) {
    return load_mesh(ArchivePool{&archive},tag,id);
}

Mesh load_mesh(const ArchivePool& archives, const std::string& tag, uint32_t id) {
    if (tag != "GMDL" && tag != "GHMS") bad("unsupported mesh resource tag");
    const Archive& archive=resolve_resource(archives,tag,id);
    const auto chunks = index(archive.children(archive.find(tag,id)));
    Mesh mesh;
    const auto matrix = archive.payload(required(chunks,"MATR"));
    if (matrix.size != 52) bad("invalid MATR length");
    mesh.matrix_flags = u32(matrix.data);
    for (size_t i=0;i<12;++i) mesh.matrix[i] = real(matrix.data+4+i*4);

    const auto mats = archive.payload(required(chunks,"MATS"));
    const uint32_t material_count = count(mats,"MATS");
    counted(mats, material_count, 8, 4,"MATS");
    std::vector<uint32_t> materials;
    for (uint32_t i=0;i<material_count;++i) {
        const uint8_t* entry = mats.data+4+size_t(i)*8;
        if (std::memcmp(entry,"GMAT",4)) bad("unexpected MATS resource type");
        const uint32_t material = u32(entry+4);
        resolve_resource(archives,"GMAT",material);
        materials.push_back(material);
    }

    const Chunk& vert_chunk = required(chunks,"VERT");
    const auto vert = archive.payload(vert_chunk);
    const uint32_t vertex_count = count(vert,"VERT");
    if (vertex_count > 4*1024*1024) bad("vertex count exceeds decoder limit");
    const auto vertex_fields = index(archive.children(vert_chunk,4));
    std::vector<MeshVertex> source;
    source.resize(vertex_count);
    const auto positions = vertex_fields.find("POSI"), uvs = vertex_fields.find("UVUV");
    if (vertex_count && positions == vertex_fields.end()) bad("missing vertex positions");
    if (positions != vertex_fields.end()) {
        const auto data = archive.payload(positions->second);
        counted(data,vertex_count,12,0,"POSI");
        for (uint32_t i=0;i<vertex_count;++i) {
            const uint8_t* p = data.data+size_t(i)*12;
            source[i].x=real(p); source[i].y=real(p+4); source[i].z=real(p+8);
        }
    }
    if (uvs != vertex_fields.end()) {
        const auto data = archive.payload(uvs->second);
        counted(data,vertex_count,8,0,"UVUV");
        for (uint32_t i=0;i<vertex_count;++i) {
            const uint8_t* p = data.data+size_t(i)*8;
            // Original 0x43da50 -> 0x4297c0 copies UVUV bytes literally.
            // Five dt_t4 meshes contain finite CDCDCDCD values in referenced,
            // textured vertices. Preserve those values; do not generate UVs.
            if (u32(p)==0xcdcdcdcd || u32(p+4)==0xcdcdcdcd)
                ++mesh.uninitialized_uv_vertices;
            source[i].u=real(p); source[i].v=real(p+4);
        }
    }
    // Missing UVUV is meaningful for untextured meshes, not an unbounded read.
    for (const auto& field : vertex_fields) {
        if (field.first == "NORM" || field.first == "VCOL")
            counted(archive.payload(field.second),vertex_count,12,0,field.first.c_str());
    }
    const auto colors = vertex_fields.find("VCOL");
    if (colors != vertex_fields.end()) {
        // 0x43dd67: the colours stay (mesh +0x30) unless every vertex's
        // r + g + b is <= 0; the renderer then uses white.
        const auto data = archive.payload(colors->second);
        bool lit = false;
        for (uint32_t i=0;i<vertex_count;++i) {
            const uint8_t* p = data.data+size_t(i)*12;
            if (real(p)+real(p+4)+real(p+8)>0) lit=true;
        }
        mesh.vertex_colors=lit;
        if (lit) for (uint32_t i=0;i<vertex_count;++i) {
            const uint8_t* p = data.data+size_t(i)*12;
            source[i].r=real(p); source[i].g=real(p+4); source[i].b=real(p+8);
        }
    }

    const Chunk& tang_chunk = required(chunks,"TANG");
    const auto tang = archive.payload(tang_chunk);
    const uint32_t triangle_count = count(tang,"TANG");
    if (triangle_count > 4*1024*1024) bad("triangle count exceeds decoder limit");
    const auto triangle_fields = index(archive.children(tang_chunk,4));
    const auto indices = triangle_fields.find("VIND");
    if (triangle_count && indices == triangle_fields.end()) bad("missing triangle vertex indices");
    std::vector<std::array<uint32_t,3>> triangles(triangle_count);
    if (indices != triangle_fields.end()) {
        const auto data = archive.payload(indices->second);
        counted(data,triangle_count,12,0,"VIND");
        for (uint32_t i=0;i<triangle_count;++i)
            for (size_t c=0;c<3;++c) triangles[i][c]=u32(data.data+size_t(i)*12+c*4);
    }
    for (const auto& field : triangle_fields) {
        if (field.first=="TNOR") counted(archive.payload(field.second),triangle_count,12,0,"TNOR");
        if (field.first=="TFLG") counted(archive.payload(field.second),triangle_count,8,0,"TFLG");
    }
    const auto tset = archive.payload(required(chunks,"TSET"));
    const uint32_t set_count = count(tset,"TSET");
    counted(tset,set_count,20,4,"TSET");
    std::vector<Set> sets;
    std::vector<uint8_t> covered(triangle_count,0);
    mesh.triangles.resize(triangle_count);
    for (uint32_t i=0;i<set_count;++i) {
        const uint8_t* p=tset.data+4+size_t(i)*20;
        Set set{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16)};
        if (set.material>=material_count) bad("TSET material index outside MATS");
        extent(vertex_count,set.vertex_start,set.vertex_count,"TSET vertex span");
        extent(triangle_count,set.triangle_start,set.triangle_count,"TSET triangle span");
        for (uint32_t j=0;j<set.triangle_count;++j) {
            const uint32_t triangle=set.triangle_start+j;
            if (covered[triangle]++) bad("overlapping TSET triangle spans");
            mesh.triangles[triangle].material=materials[set.material];
        }
        sets.push_back(set);
    }
    for (uint8_t present : covered) if (!present) bad("triangle has no TSET material");

    if (tag=="GMDL") {
        mesh.vertices=std::move(source);
        for (uint32_t i=0;i<triangle_count;++i) {
            for (uint32_t vertex : triangles[i]) if (vertex>=vertex_count) bad("VIND outside VERT");
            mesh.triangles[i].a=triangles[i][0]; mesh.triangles[i].b=triangles[i][1]; mesh.triangles[i].c=triangles[i][2];
        }
        return mesh;
    }

    const auto hmts=archive.payload(required(chunks,"HMTS"));
    if (hmts.size<8 || (hmts.size-8)%8) bad("invalid HMTS framing");
    const uint32_t map_count=u32(hmts.data+4);
    if (map_count>4*1024*1024) bad("HMTS map exceeds decoder limit");
    const auto segs=archive.payload(required(chunks,"SEGS"));
    const uint32_t segment_count=count(segs,"SEGS");
    counted(segs,segment_count,36,4,"SEGS");
    std::vector<Segment> segments;
    std::vector<uint8_t> set_owners(set_count,0);
    for (uint32_t i=0;i<segment_count;++i) {
        const uint8_t* p=segs.data+4+size_t(i)*36;
        Segment s{u32(p),u32(p+4),u32(p+8),u32(p+12),u32(p+16),u32(p+20),real(p+24),real(p+28),real(p+32)};
        mesh.segments.push_back({s.parent,s.x,s.y,s.z});
        if (s.parent!=UINT32_MAX && s.parent>=i) bad("SEGS parent is not an earlier segment");
        extent(set_count,s.set_start,s.set_count,"SEGS TSET span");
        extent(vertex_count,s.vertex_start,s.vertex_count,"SEGS source vertices");
        extent(map_count,s.map_start,s.vertex_count,"SEGS map span");
        if (s.parent!=UINT32_MAX) {
            s.x+=segments[s.parent].x; s.y+=segments[s.parent].y; s.z+=segments[s.parent].z;
            if (!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z)) bad("segment transform overflow");
        }
        for (uint32_t j=0;j<s.set_count;++j)
            if (set_owners[s.set_start+j]++) bad("TSET assigned to multiple segments");
        segments.push_back(s);
    }
    for (uint8_t owner : set_owners) if (!owner) bad("TSET has no segment owner");
    const uint32_t missing=UINT32_MAX;
    std::vector<uint32_t> map_source(map_count,missing), map_segment(map_count,missing);
    // The original 0x437040 loop descends and, within one iteration, fills
    // that segment's HMTS span then rewrites its TSETs. A segment therefore
    // sees its own vertices in its span; only slots outside it resolve to the
    // higher-numbered segments written earlier (seams such as GHMS 0x7ae's
    // seg0 skin). Lower segments overwrite the overlap only afterwards.
    for (size_t n=segments.size();n>0;--n) {
        const auto& segment=segments[n-1];
        for (uint32_t j=0;j<segment.vertex_count;++j) {
            map_source[segment.map_start+j]=segment.vertex_start+j;
            map_segment[segment.map_start+j]=uint32_t(n-1);
        }
        // Match 0x436ff0: sharing is per TSET, keyed only by source vertex.
        // The first occurrence supplies that vertex's segment.
        for (uint32_t s=0;s<segment.set_count;++s) {
            const auto& set=sets[segment.set_start+s];
            std::unordered_map<uint32_t,uint32_t> remapped;
            for (uint32_t j=0;j<set.triangle_count;++j) {
                const uint32_t triangle=set.triangle_start+j;
                uint32_t rewritten[3];
                for (size_t c=0;c<3;++c) {
                    const uint32_t map_id=triangles[triangle][c];
                    if (map_id>=map_count) bad("GHMS VIND outside HMTS map");
                    const uint32_t source_id=map_source[map_id];
                    if (source_id==missing) bad("GHMS VIND references an unfilled HMTS slot");
                    auto found=remapped.find(source_id);
                    if (found==remapped.end()) {
                        if (mesh.vertices.size()>=4*1024*1024) bad("posed vertex count exceeds decoder limit");
                        MeshVertex vertex=source[source_id];
                        mesh.vertex_segment.push_back(map_segment[map_id]);
                        mesh.rest.push_back({{vertex.x,vertex.y,vertex.z}});
                        const auto& pose=segments[map_segment[map_id]];
                        vertex.x+=pose.x; vertex.y+=pose.y; vertex.z+=pose.z;
                        if (!std::isfinite(vertex.x)||!std::isfinite(vertex.y)||!std::isfinite(vertex.z)) bad("posed vertex overflow");
                        const uint32_t new_id=uint32_t(mesh.vertices.size());
                        mesh.vertices.push_back(vertex); found=remapped.emplace(source_id,new_id).first;
                    }
                    rewritten[c]=found->second;
                }
                mesh.triangles[triangle].a=rewritten[0]; mesh.triangles[triangle].b=rewritten[1]; mesh.triangles[triangle].c=rewritten[2];
            }
        }
    }
    return mesh;
}
} // namespace wpc
