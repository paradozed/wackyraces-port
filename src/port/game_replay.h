#ifndef WPC_GAME_REPLAY_H
#define WPC_GAME_REPLAY_H
#include "game_drive.h"
#include <array>
#include <cstdint>
#include <vector>

namespace wpc {
// The replay buffer 0x6abee0 (0x382f9 bytes) that demo1..3.pc and .WSR
// files hold raw (0x45afb0 / 0x4b7a30 read it, 0x45aff0 restores it).
struct Replay {
    std::array<uint32_t,4> session{}; // app +0x1a60..+0x1a6c: game mode, players, vehicle, second vehicle
    int environment=0,course=0;       // 0x6f41d8, 0x6f41dc
    uint8_t flag=0;                   // +0x18 -> METEO rain (0x45b132, 0x6f464c)
    // The first controller stream from +0x49: (updates, input word) runs
    // of three bytes, 0x45ba58; the word less bit 0x800 is the input
    // (0x6e42a0) for that many updates.
    std::vector<std::pair<uint8_t,uint16_t>> controller;
    int frames=0;                     // +0x382f7
    static Replay parse(const std::vector<uint8_t>& bytes);
    // The raw buffer as 0x4b7920 writes it; the regions the port does not
    // record (second controller, channels, the per-update floats and 2-bit
    // states) stay zero.
    std::vector<uint8_t> serialize() const;
    // 0x45b530's controller part: one input word an update, runs of at
    // most 255 updates.
    void record(uint16_t word);
    // The inverse of ReplayCursor::input.
    static uint16_t word(const DriveInput& input);
};
// Plays the first controller's runs, one input word an update.
class ReplayCursor {
public:
    explicit ReplayCursor(const Replay& replay):replay_(replay) {}
    bool done() const {return run_==0 && index_>=replay_.controller.size();}
    uint16_t next();
    // 0x439a..: 0x40 accelerate, 0x80 brake, 0x8000 left (stick x < -64),
    // 0x2000 right (x > 64).
    static DriveInput input(uint16_t word);
private:
    const Replay& replay_;
    size_t index_=0;
    int run_=0;
    uint16_t word_=0;
};
}
#endif
