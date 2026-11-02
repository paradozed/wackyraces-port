/*
 * wpc_field_readers.cpp - C++ stubs for the stream field reader routines.
 *
 * The original field readers (src/field_readers.cpp) use i386-specific
 * instruction matching. These are replaced with minimal stubs that return
 * zero / false for all field reads.
 */

#include <cstdint>

extern "C" {

/* Stub field readers - all return zero/false. */
uint32_t wpc_field_read_u8(const void *, uint32_t) { return 0; }
uint32_t wpc_field_read_u16(const void *, uint32_t) { return 0; }
uint32_t wpc_field_read_u32(const void *, uint32_t) { return 0; }
uint32_t wpc_field_read_f32(const void *, uint32_t) { return 0; }
int32_t  wpc_field_read_s32(const void *, uint32_t) { return 0; }

} /* extern "C" */
