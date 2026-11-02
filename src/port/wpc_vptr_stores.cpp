/*
 * wpc_vptr_stores.cpp - C++ stubs for vtable pointer initialization.
 *
 * The original (src/vptr_stores.cpp) stores 32-bit pointers at hardcoded
 * addresses, which is not valid on x86-64. These are replaced with no-op
 * stubs.
 */

#include <cstdint>

extern "C" {

void wpc_vptr_00439280(uint32_t *) {}
void wpc_vptr_00439350(uint32_t *) {}
void wpc_vptr_00439440(uint32_t *) {}
void wpc_vptr_00439570(uint32_t *) {}
void wpc_vptr_0040d7f0(uint32_t *) {}

void wpc_init_vptr_00455710(uint32_t *self)
{
    (void)self;
}

} /* extern "C" */
