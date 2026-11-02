/*
 * wpc_tga.cpp - C++ stubs for the TGA header and pointer helpers.
 *
 * The original TGA code (src/tga.cpp, src/tga.h) requires 32-bit x86 due to
 * WPC_THISCALL and inline assembly. These are replaced with minimal stubs.
 */

#include <cstdint>
#include <cstring>

#ifndef WPC_THISCALL
#define WPC_THISCALL
#endif

extern "C" {

typedef unsigned char WpcByte;

struct TgaHeaderView {
    WpcByte *header;
    int WPC_THISCALL validate() { return 0; }
    int WPC_THISCALL palette_entries() { return 0; }
    WpcByte *WPC_THISCALL palette_data() { return nullptr; }
    WpcByte *WPC_THISCALL pixel_data() { return nullptr; }
};

} /* extern "C" */
