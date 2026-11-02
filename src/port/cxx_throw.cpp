/*
 * cxx_throw.cpp - C++ replacement for the i386-only C++ throw helper (0x004eeaf0).
 *
 * The original builds an exception record on the stack and calls the Windows
 * RaiseException vectored dispatch. On the native port we throw a standard
 * C++ exception instead.
 */

#include <cstdint>
#include <stdexcept>

extern "C" void wpc_cxx_throw_004eeaf0(void *object, const void *throw_info)
{
    (void)object;
    (void)throw_info;
    throw std::runtime_error("wpc_cxx_throw: C++ exception thrown");
}
