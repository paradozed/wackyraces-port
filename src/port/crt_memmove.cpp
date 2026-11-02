/*
 * crt_memmove.cpp - C++ replacement for the i386-only CRT memmove (0x004fcc00).
 *
 * The original is hand-written x86 assembly with biased jump tables. On the
 * native port we simply delegate to the C library memmove, which has the same
 * semantics (safe for overlapping buffers).
 */

#include <cstring>

extern "C" void *wpc_crt_memmove_004fcc00(void *dst, const void *src, unsigned int count)
{
    return memmove(dst, src, count);
}
