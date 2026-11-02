// Vehicle preview regression check (TODO: silhouette, placement and rendered
// materials across the cars). Takes the original menu.WGM; projects each car
// item of the car selection page (page 28, WMenuNodeCarSelection) through the
// page camera at frame 0 and compares the box, counts and mean vertex colour
// with values recorded from the native port (2026-11-01). The numbers are
// derived measurements, not game content.
#include "menu_models.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {
size_t checks=0;
void require(bool value,const std::string& message) { ++checks; if(!value) throw std::runtime_error(message); }
const wpc::Node* find(const wpc::Node& n,uint32_t id) {
    if(n.id==id) return &n;
    for(const auto& c:n.children) if(const auto* r=find(c,id)) return r;
    return nullptr;
}
struct Expected { size_t vertices,batches,translucent; float x0,y0,x1,y1,r,g,b; };
// Recorded from the native port; regenerate with --print after an intended change.
const Expected expected[7]={
#include "native_vehicle_expected.inc"
};
}

int main(int argc,char** argv) {
    try {
        if(argc<2) { std::printf("native vehicle: no menu.WGM given, skipped\n"); return 0; }
        const bool print=argc>2 && std::string(argv[2])=="--print";
        wpc::Archive archive;archive.load(argv[1]);
        wpc::Scene scene;scene.load(archive);
        // Textures are not uploaded: any non-null handle stands in.
        static int dummy;
        wpc::MenuModels models(nullptr,archive,scene,[](uint32_t){return reinterpret_cast<WpcTexture*>(&dummy);});
        const wpc::Node* page=find(scene.root(),28);
        require(page && page->class_name=="WMenuPage","car selection page 28");
        models.prepare(*page);
        for(int v=0;v<7;++v) {
            const auto s=models.silhouette(28,v);
            if(print) {
                std::printf("    {%zu,%zu,%zu,%.1ff,%.1ff,%.1ff,%.1ff,%.3ff,%.3ff,%.3ff},\n",s.vertices,s.batches,s.translucent,
                            s.x0,s.y0,s.x1,s.y1,s.color[0],s.color[1],s.color[2]);
                continue;
            }
            const Expected& e=expected[v];
            const std::string car="vehicle "+std::to_string(v);
            require(s.vertices==e.vertices && s.batches==e.batches && s.translucent==e.translucent,car+": geometry and material batches");
            // Half a pixel of the 640x480 frame for the placement and silhouette box.
            require(std::abs(s.x0-e.x0)<0.5f && std::abs(s.y0-e.y0)<0.5f && std::abs(s.x1-e.x1)<0.5f && std::abs(s.y1-e.y1)<0.5f,car+": projected box");
            require(std::abs(s.color[0]-e.r)<0.002f && std::abs(s.color[1]-e.g)<0.002f && std::abs(s.color[2]-e.b)<0.002f,car+": mean vertex colour");
            require(s.x0<s.x1 && s.y0<s.y1 && s.x0>=0 && s.x1<=640 && s.y0>=0 && s.y1<=480,car+": inside the frame");
        }
        if(!print) std::printf("native vehicle: %zu checks passed\n",checks);
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"native vehicle: %s\n",e.what());
        return 1;
    }
}
