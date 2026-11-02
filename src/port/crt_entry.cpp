/*
 * crt_entry.cpp - C++ stub for the i386-only CRT entry point (0x004f18a0).
 *
 * On Linux, the entry point is _start (provided by crt0 / the toolchain), so
 * this symbol is only needed if other recovered code references it. It is a
 * no-op stub for now.
 */

extern "C" void wpc_crt_entry_004f18a0(void)
{
    /* No-op on Linux native port. */
}
