#ifndef WPC_GAME_RANDOM_H
#define WPC_GAME_RANDOM_H
#include <cstdint>

namespace wpc {
// MSVC rand() (0x430e40); the scene loader seeds it with 0x1312d6e (0x431fc3).
class MsvcRandom {
public:
    explicit MsvcRandom(uint32_t seed=0x1312d6e):state_(seed) {}
    int next() {state_=state_*214013u+2531011u;return int((state_>>16)&0x7fff);}
    // 0x430fa0(x): rand() * 2^-15 (0x430ec0) times x.
    float unit(float scale=1) {return float(next())*(1.f/32768.f)*scale;}
private:
    uint32_t state_;
};
}
#endif
