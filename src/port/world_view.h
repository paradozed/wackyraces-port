#ifndef WPC_WORLD_VIEW_H
#define WPC_WORLD_VIEW_H
#include "game_world.h"

namespace wpc {
class WorldView {
public:
    WorldView(WpcWindow* window,WorldData data);
    ~WorldView();
    WorldView(const WorldView&)=delete;
    WorldView& operator=(const WorldView&)=delete;
    void draw();
private:
    WpcWindow* window_;
    WorldData data_;
    std::map<uint32_t,WpcTexture*> textures_;
    WpcTexture* white_=nullptr;
    void release();
};
}
#endif
