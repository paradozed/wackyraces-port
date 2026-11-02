#ifndef WPC_GAME_GROUND_H
#define WPC_GAME_GROUND_H
#include "game_assets.h"
#include <array>

namespace wpc {
struct GroundPoint {float x=0,y=0,z=0;};
struct GroundHit {
    float fraction=0;
    GroundPoint point,normal;
    uint32_t resource_id=0,record_index=0,record_flags=0;
    // The polygon's offset in its record's geometry, in 4-byte words: the
    // grid link's second half, and what WCtrlRoboCar keeps as its module
    // (WackyAuto +0x2c5, from the query result +0x9a).
    uint32_t polygon=0;
};
// The route data of a route polygon (record flag 0x80000000, the secondary
// layer), after its vertex indices (WCtrlRoboCar 0x4e9610/0x4e9750):
// +0x10 route flags (byte 1, +0x11, the remaining fraction of the section in
// 1/255; byte 3 a speed class, 0x52e600); +0x16 counts (bits 4..7 forward
// links, 0..3 backward ones; bits 8..15 not read); then 12-byte links
// {u16 target polygon, u16 weight, s16 x, y, z, u16}, forward links first.
struct RouteLink {uint16_t target=0xffff,weight=0;GroundPoint point;};
struct RouteNode {
    uint32_t polygon=0,flags=0;
    // The counts word's high byte (+0x17 on a quad): the remaining fraction of
    // the section in 1/256 that ranks racers (0x4a3937).
    uint8_t remaining=0;
    std::array<uint16_t,4> vertices{};
    size_t vertex_count=0;
    std::vector<RouteLink> forward,backward;
};
struct GroundQueryResult {
    bool hit=false;
    bool complete=true;
    GroundHit contact;
    bool secondary_hit=false;
    GroundHit secondary;
    // Eligible candidates requiring an unported runtime context/path are
    // reported explicitly; they are not silently treated as empty space.
    std::vector<std::string> unsupported;
};
class GroundResource {
public:
    void load(const Archive& archive,uint32_t id);
    // Complete WGNR resource payload: ID, version, blob length, blob bytes.
    void load_bytes(std::vector<uint8_t> payload);
    // Coordinates are in this resource's active route/object frame. The
    // caller supplies that frame; scanning every WGNR in world coordinates
    // does not reproduce the original WModManager spatial query.
    GroundQueryResult query_local(GroundPoint start,GroundPoint end,uint32_t mask=0x018000ff) const;
    uint32_t id() const {return id_;}
    // A route polygon of a record; throws when the data does not fit.
    RouteNode route_node(uint32_t record,uint32_t polygon) const;
    GroundPoint vertex_point(uint32_t record,uint16_t index) const;
    size_t record_count() const {return records_.size();}
    size_t geometry_count() const {return geometries_.size();}
    std::array<uint16_t,2> grid_size() const {return {{grids_[0].width,grids_[0].height}};}
private:
    struct Geometry {size_t polygons=0,vertices=0,planes=0,end=0;};
    struct Record {size_t geometry=0;uint32_t flags=0;uint16_t matrix=0;};
    struct Grid {uint8_t shift_x=0,shift_z=0;int32_t origin_x=0,origin_z=0;uint16_t width=0,height=0;size_t cells=0,links=0;};
    uint32_t id_=0;
    std::vector<uint8_t> bytes_;
    std::vector<Geometry> geometries_;
    std::vector<Record> records_;
    std::array<Grid,2> grids_{};
};
}
#endif
