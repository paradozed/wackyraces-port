/*
 * wpc_exception_metadata.cpp - C++ stubs for C++ exception metadata.
 *
 * The original (src/cxx_exception_metadata.cpp) uses 32-bit-specific type
 * layouts and static assertions that fail on x86-64. These are replaced with
 * minimal stubs providing the same symbols.
 */

#include <cstdint>

struct WpcCatchableType32 {
    uint32_t attributes;
    void (*unwind_function)();
    void (*forward_compatibility)();
    const void *catchable_types;
};
struct WpcCatchableArray3 {
    uint32_t count;
    const WpcCatchableType32 *types[3];
};
struct WpcThrowInformation32 {
    uint32_t options;
    uint32_t unused1;
    uint32_t unused2;
    const WpcCatchableArray3 *catchable_types;
};

extern "C" {

const WpcCatchableType32 wpc_catchable_void_pointer = {
    0, nullptr, nullptr, nullptr
};

const WpcCatchableType32 wpc_catchable_gd_error_pointer = {
    0, nullptr, nullptr, nullptr
};

const WpcCatchableType32 wpc_catchable_gd_load_error_pointer = {
    0, nullptr, nullptr, nullptr
};

const WpcCatchableArray3 wpc_load_error_catchable_types = {
    3, {&wpc_catchable_gd_load_error_pointer,
        &wpc_catchable_gd_error_pointer,
        &wpc_catchable_void_pointer}
};

const WpcThrowInformation32 wpc_load_error_throw_information = {
    0, 0, 0, &wpc_load_error_catchable_types
};

uint32_t wpc_type_info_delete_004ee9e0(void *, uint32_t)
{
    return 0;
}

} /* extern "C" */
