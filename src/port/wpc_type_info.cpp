/*
 * wpc_type_info.cpp - C++ stubs for C++ type-info methods.
 *
 * The original (src/type_info.cpp) uses i386-specific inline assembly.
 * These are replaced with minimal stubs.
 */

#include <cstdint>

extern "C" {

uint32_t wpc_type_info_destruct_004ee9b0(void *)
{
    return 0;
}

uint32_t wpc_type_info_equal_004eea10(const void *, const void *)
{
    return 0;
}

uint32_t wpc_type_info_not_equal_004eea40(const void *, const void *)
{
    return 0;
}

uint32_t wpc_type_info_before_004eea70(const void *, const void *)
{
    return 0;
}

const char *wpc_type_info_raw_name_004eeaa0(const void *)
{
    return "";
}

void *wpc_type_info_initialize_004eeac0(void *, uint32_t)
{
    return nullptr;
}

void *wpc_type_info_passthrough_004eeae0(void *, uint32_t)
{
    return nullptr;
}

} /* extern "C" */
