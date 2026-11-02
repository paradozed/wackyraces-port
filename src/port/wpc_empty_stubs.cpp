/*
 * wpc_empty_stubs.cpp - C++ stubs for the empty leaf routines.
 *
 * The original empty stubs (src/empty_stubs.cpp) use __stdcall, which is not
 * available on x86-64 GCC. These are replaced with plain C++ no-op functions.
 */

extern "C" {

void wpc_empty_0041bf20(void) {}
void wpc_empty_00426ad0(void) {}
void wpc_empty_0042db70(void) {}
void wpc_empty_0045c3b0(void) {}
void wpc_empty_0045cf50(void) {}
void wpc_empty_0045d0d0(void) {}
void wpc_empty_0045d250(void) {}
void wpc_empty_0045de10(void) {}
void wpc_empty_004b96d0(unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int) {}
void wpc_empty_004b96f0(unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int) {}
void wpc_empty_004bc200(unsigned int) {}

} /* extern "C" */
