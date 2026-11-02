/*
 * wpc_iat_stubs.cpp - Import Address Table stub implementations for Linux.
 *
 * Each stub mirrors a Win32 API and returns a benign default value so the
 * recovered game code can link and start. Functionality is intentionally
 * limited: the goal is to get a running shell, not a full Win32
 * compatibility layer. Stubs that need real behavior (files, time, windowing)
 * delegate to wpc_platform.h where possible.
 */

#include "wpc_iat_stubs.h"
#include "wpc_platform.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <pthread.h>

/* ------------------------------------------------------------------ */
/* Helper: no-op function pointer initializers                        */
/* ------------------------------------------------------------------ */

#define WPC_STUB(name, rettype, ...) \
    static rettype wpc_stub_##name(__VA_ARGS__) { return (rettype)0; }

/* ------------------------------------------------------------------ */
/* KERNEL32 stubs                                                     */
/* ------------------------------------------------------------------ */

static uint32_t stub_GetLogicalDriveStringsA(uint32_t, char *) { return 0; }
static void *   stub_LoadLibraryA(const char *) { return NULL; }
static int      stub_FreeLibrary(void *) { return 0; }
static int      stub_SetCurrentDirectoryA(const char *) { return 0; }
static char *   stub_GetCommandLineA(void) { return (char *)""; }
static uint32_t stub_GetLastError(void) { return 0; }
static void *   stub_CreateFileMappingA(void *, void *, uint32_t, uint32_t, uint32_t, const char *) { return NULL; }
static uint32_t stub_GetDriveTypeA(const char *) { return 1; } /* DRIVE_NO_ROOT_DIR */
static int      stub_DeviceIoControl(void *, uint32_t, const void *, uint32_t, void *, uint32_t, uint32_t *, void *) { return 0; }
static void *   stub_CreateFileA(const char *, uint32_t, uint32_t, void *, uint32_t, uint32_t, void *) { return (void *)-1; } /* INVALID_HANDLE_VALUE */
static int      stub_FindNextFileA(void *, void *) { return 0; }
static void *   stub_GetProcAddress(void *, const char *) { return NULL; }
static int      stub_CreateDirectoryA(const char *, void *) { return 0; }
static uint32_t stub_GetCurrentDirectoryA(uint32_t n, char *buf) { if (buf && n) buf[0] = 0; return 0; }
static int      stub_DeleteFileA(const char *) { return 0; }
static void     stub_OutputDebugStringA(const char *) {}
static int      stub_lstrcatA(char *, const char *) { return 0; }
static void *   stub_CreateThread(void *, uint32_t, void *, void *, uint32_t, uint32_t *) { return NULL; }
static uint32_t stub_WaitForMultipleObjects(uint32_t, void **, int, uint32_t) { return 0; }
static void     stub_EnterCriticalSection(void *) {}
static void     stub_LeaveCriticalSection(void *) {}
static int      stub_TerminateThread(void *, uint32_t) { return 0; }
static void     stub_DeleteCriticalSection(void *) {}
static int      stub_CloseHandle(void *) { return 0; }
static void *   stub_CreateEventA(void *, int, int, const char *) { return NULL; }
static void *   stub_FindFirstFileA(const char *, void *) { return (void *)-1; }
static void     stub_GlobalMemoryStatus(void *) {}
static void     stub_InitializeCriticalSection(void *) {}
static int      stub_FindClose(void *) { return 0; }
static uint32_t stub_GetTickCount(void) { return (uint32_t)(wpc_time_now() * 1000.0); }
static char *   stub_GetEnvironmentStrings(void) { return (char *)""; }
static int      stub_IsBadCodePtr(void *) { return 0; }
static void     stub_SetStdHandle(uint32_t, void *) {}
static int      stub_FreeEnvironmentStringsA(char *) { return 0; }
static int      stub_UnhandledExceptionFilter(void *) { return 0; }
static int      stub_LCMapStringW(uint32_t, uint32_t, const void *, int, void *, int) { return 0; }
static int      stub_LCMapStringA(uint32_t, uint32_t, const char *, int, char *, int) { return 0; }
static int      stub_MultiByteToWideChar(uint32_t, uint32_t, const char *, int, void *, int) { return 0; }
static int      stub_WideCharToMultiByte(uint32_t, uint32_t, const void *, int, char *, int, const char *, int *) { return 0; }
static void *   stub_HeapCreate(uint32_t, uint32_t, uint32_t) { return (void *)0x1; }
static int      stub_HeapDestroy(void *) { return 0; }
static int      stub_GetVersionExA(void *) { return 0; }
static uint32_t stub_GetEnvironmentVariableA(const char *, char *, uint32_t) { return 0; }
static int      stub_FreeEnvironmentStringsW(void *) { return 0; }
static void *   stub_GetEnvironmentStringsW(void) { return NULL; }
static void *   stub_VirtualAlloc(void *, uint32_t, uint32_t, uint32_t) { return NULL; }
static void *   stub_HeapReAlloc(void *, uint32_t, void *, uint32_t) { return NULL; }
static void *   stub_HeapAlloc(void *, uint32_t, uint32_t) { return NULL; }
static uint32_t stub_GetFileType(void *) { return 0; }
static int      stub_SetHandleCount(uint32_t) { return 0; }
static uint32_t stub_SetFilePointer(void *, int32_t, int32_t *, uint32_t) { return 0xFFFFFFFF; }
static int      stub_SetUnhandledExceptionFilter(void *) { return 0; }
static int      stub_HeapFree(void *, uint32_t, void *) { return 0; }
static int      stub_ReadFile(void *, void *, uint32_t, uint32_t *, void *) { return 0; }
static uint32_t stub_GetModuleFileNameA(void *, char *, uint32_t) { return 0; }
static int      stub_InterlockedIncrement(volatile int32_t *) { return 0; }
static int      stub_InterlockedDecrement(volatile int32_t *) { return 0; }
static int      stub_WriteFile(void *, const void *, uint32_t, uint32_t *, void *) { return 0; }
static void *   stub_GetStdHandle(uint32_t) { return NULL; }
static void     stub_DebugBreak(void) {}
static uint32_t stub_GetVersion(void) { return 0; }
static int      stub_GetStartupInfoA(void *) { return 0; }
static void *   stub_GetModuleHandleA(const char *) { return NULL; }
static void *   stub_GetCurrentProcess(void) { return (void *)-1; }
static int      stub_TerminateProcess(void *, uint32_t) { return 0; }
static int      stub_ExitProcess(uint32_t) { _exit(0); return 0; }
static int      stub_SetEndOfFile(void *) { return 0; }
static uint32_t stub_GetOEMCP(void) { return 437; }
static uint32_t stub_GetACP(void) { return 1252; }
static int      stub_GetCPInfo(uint32_t, void *) { return 0; }
static int      stub_GetStringTypeW(uint32_t, const void *, int, void *) { return 0; }
static int      stub_GetStringTypeA(uint32_t, uint32_t, const char *, int, void *) { return 0; }
static int      stub_SetConsoleCtrlHandler(void *, int) { return 0; }
static int      stub_FlushFileBuffers(void *) { return 0; }
static int      stub_VirtualFree(void *, uint32_t, uint32_t) { return 0; }
static int      stub_HeapValidate(void *, uint32_t, const void *) { return 0; }
static int      stub_IsBadReadPtr(const void *, uint32_t) { return 0; }
static void     stub_RtlUnwind(void *, void *, void *, void *) {}
static int      stub_IsBadWritePtr(void *, uint32_t) { return 0; }
static int      stub_RaiseException(uint32_t, uint32_t, uint32_t, const void *) { return 0; }

/* ------------------------------------------------------------------ */
/* USER32 stubs                                                       */
/* ------------------------------------------------------------------ */

static int      stub_ClientToScreen(void *, void *) { return 0; }
static int      stub_MessageBoxA(void *, const char *, const char *, uint32_t) { return 0; }
static void *   stub_GetActiveWindow(void) { return NULL; }
static int      stub_wsprintfA(char *, const char *, ...) { return 0; }
static int      stub_UpdateWindow(void *) { return 0; }
static int      stub_ShowWindow(void *, int) { return 0; }
static void *   stub_CreateWindowExA(uint32_t, const char *, const char *, uint32_t, int, int, int, int, void *, void *, void *, void *) { return NULL; }
static int32_t  stub_GetWindowLongA(void *, int) { return 0; }
static int      stub_MoveWindow(void *, int, int, int, int, int) { return 0; }
static int      stub_SendMessageA(void *, uint32_t, uint32_t, intptr_t) { return 0; }
static intptr_t stub_DefWindowProcA(void *, uint32_t, uint32_t, intptr_t) { return 0; }
static int16_t  stub_GetKeyState(int) { return 0; }
static int      stub_EndPaint(void *, void *) { return 0; }
static void *   stub_BeginPaint(void *, void *) { return NULL; }
static int      stub_DrawMenuBar(void *) { return 0; }
static int      stub_RedrawWindow(void *, void *, void *, uint32_t) { return 0; }
static int      stub_RegisterClassA(const void *) { return 0; }
static void     stub_SetRect(void *, int, int, int, int) {}
static int      stub_PeekMessageA(void *, void *, uint32_t, uint32_t, uint32_t) { return 0; }
static int      stub_TranslateMessage(const void *) { return 0; }
static intptr_t stub_DispatchMessageA(const void *) { return 0; }
static int      stub_GetClientRect(void *, void *) { return 0; }
static int      stub_UnregisterClassA(const char *, void *) { return 0; }
static int      stub_SetFocus(void *) { return 0; }
static int      stub_ShowCursor(int) { return 0; }
static void *   stub_SetCapture(void *) { return NULL; }
static void *   stub_SetCursor(void *) { return NULL; }
static void     stub_PostQuitMessage(int) {}
static int      stub_GetKeyNameTextA(int32_t, char *, int) { return 0; }
static int      stub_GetCursorPos(void *) { return 0; }
static int      stub_SetCursorPos(int, int) { return 0; }
static int      stub_GetSystemMetrics(int) { return 0; }
static void *   stub_LoadIconA(void *, const char *) { return NULL; }
static void *   stub_LoadCursorA(void *, const char *) { return NULL; }
static int      stub_DestroyWindow(void *) { return 0; }
static int      stub_WaitMessage(void) { return 0; }
static int      stub_ReleaseCapture(void) { return 0; }

/* ------------------------------------------------------------------ */
/* GDI32 stubs                                                        */
/* ------------------------------------------------------------------ */

static int      stub_SetBkMode(void *, int) { return 0; }
static int      stub_ExtTextOutA(void *, int, int, uint32_t, const void *, const char *, uint32_t, const void *) { return 0; }
static void *   stub_GetStockObject(int) { return NULL; }
static int      stub_SetTextColor(void *, uint32_t) { return 0; }

/* ------------------------------------------------------------------ */
/* ADVAPI32 stubs                                                     */
/* ------------------------------------------------------------------ */

static int      stub_RegDeleteValueA(void *, const char *) { return 0; }
static int      stub_RegSetValueExA(void *, const char *, uint32_t, uint32_t, const void *, uint32_t) { return 0; }
static int      stub_RegCloseKey(void *) { return 0; }
static int      stub_RegDeleteKeyA(void *, const char *) { return 0; }
static int      stub_RegCreateKeyExA(void *, const char *, uint32_t, char *, uint32_t, uint32_t, void *, void **, uint32_t *) { return 0; }
static int      stub_RegQueryValueExA(void *, const char *, uint32_t *, uint32_t *, void *, uint32_t *) { return 0; }
static int      stub_RegOpenKeyExA(void *, const char *, uint32_t, uint32_t, void **) { return 0; }

/* ------------------------------------------------------------------ */
/* WINMM stubs                                                        */
/* ------------------------------------------------------------------ */

static int      stub_mixerSetControlDetails(void *, void *, uint32_t) { return 0; }
static int      stub_mixerGetNumDevs(void) { return 0; }
static int      stub_mixerGetDevCapsA(uint32_t, void *, uint32_t) { return 0; }
static int      stub_mixerGetLineInfoA(void *, void *) { return 0; }
static int      stub_mixerGetLineControlsA(void *, void *, uint32_t) { return 0; }
static int      stub_timeKillEvent(int) { return 0; }
static int      stub_timeSetEvent(uint32_t, uint32_t, void *, uint32_t, uint32_t) { return 0; }
static int      stub_mciSendCommandA(uint32_t, uint32_t, uint32_t, void *) { return 0; }
static uint32_t stub_timeGetTime(void) { return (uint32_t)(wpc_time_now() * 1000.0); }

/* ------------------------------------------------------------------ */
/* DDRAW stubs                                                        */
/* ------------------------------------------------------------------ */

static int      stub_DirectDrawCreateEx(void *, void **, const void *, void *) { return 0; }

/* ------------------------------------------------------------------ */
/* DINPUT stubs                                                       */
/* ------------------------------------------------------------------ */

static int      stub_DirectInputCreateEx(void *, uint32_t, const void *, void **, void *) { return 0; }

/* ------------------------------------------------------------------ */
/* DSOUND stubs                                                       */
/* ------------------------------------------------------------------ */

static int      stub_DirectSoundCreate(void *, void **, void *) { return 0; }

/* ------------------------------------------------------------------ */
/* AVIFIL32 stubs                                                     */
/* ------------------------------------------------------------------ */

static int      stub_AVIStreamGetFrameClose(void *) { return 0; }
static int      stub_AVIStreamReadFormat(void *, int, void *, int *) { return 0; }
static void *   stub_AVIStreamGetFrameOpen(void *, void *) { return NULL; }
static int      stub_AVIStreamInfoA(void *, void *, uint32_t) { return 0; }
static int      stub_AVIFileRelease(void *) { return 0; }
static int      stub_AVIStreamGetFrame(void *, int, void *) { return 0; }
static int      stub_AVIStreamRead(void *, int, int, void *, int, int *, int *) { return 0; }
static int      stub_AVIFileExit(void) { return 0; }
static int      stub_AVIFileInit(void) { return 0; }
static int      stub_AVIFileGetStream(void *, void **, uint32_t, int) { return 0; }
static int      stub_AVIFileOpenA(void **, const char *, uint32_t, void *) { return 0; }
static int      stub_AVIStreamRelease(void *) { return 0; }

/* ------------------------------------------------------------------ */
/* MSVFW32 stubs                                                      */
/* ------------------------------------------------------------------ */

static void *   stub_DrawDibOpen(void) { return NULL; }
static int      stub_DrawDibDraw(void *, void *, int, int, void *, void *, int, int, int, int, uint32_t) { return 0; }
static int      stub_DrawDibClose(void *) { return 0; }

/* ------------------------------------------------------------------ */
/* Import Address Table definitions                                   */
/* ------------------------------------------------------------------ */

Proc_GetLogicalDriveStringsA        wpc_iat_GetLogicalDriveStringsA        = stub_GetLogicalDriveStringsA;
Proc_LoadLibraryA                   wpc_iat_LoadLibraryA                   = stub_LoadLibraryA;
Proc_FreeLibrary                    wpc_iat_FreeLibrary                    = stub_FreeLibrary;
Proc_SetCurrentDirectoryA           wpc_iat_SetCurrentDirectoryA           = stub_SetCurrentDirectoryA;
Proc_GetCommandLineA                wpc_iat_GetCommandLineA                = stub_GetCommandLineA;
Proc_GetLastError                   wpc_iat_GetLastError                   = stub_GetLastError;
Proc_CreateFileMappingA             wpc_iat_CreateFileMappingA             = stub_CreateFileMappingA;
Proc_GetDriveTypeA                  wpc_iat_GetDriveTypeA                  = stub_GetDriveTypeA;
Proc_DeviceIoControl                wpc_iat_DeviceIoControl                = stub_DeviceIoControl;
Proc_CreateFileA                    wpc_iat_CreateFileA                    = stub_CreateFileA;
Proc_FindNextFileA                  wpc_iat_FindNextFileA                  = stub_FindNextFileA;
Proc_GetProcAddress                 wpc_iat_GetProcAddress                 = stub_GetProcAddress;
Proc_CreateDirectoryA               wpc_iat_CreateDirectoryA               = stub_CreateDirectoryA;
Proc_GetCurrentDirectoryA           wpc_iat_GetCurrentDirectoryA           = stub_GetCurrentDirectoryA;
Proc_DeleteFileA                    wpc_iat_DeleteFileA                    = stub_DeleteFileA;
Proc_OutputDebugStringA             wpc_iat_OutputDebugStringA             = stub_OutputDebugStringA;
Proc_lstrcatA                       wpc_iat_lstrcatA                       = stub_lstrcatA;
Proc_CreateThread                   wpc_iat_CreateThread                   = stub_CreateThread;
Proc_WaitForMultipleObjects         wpc_iat_WaitForMultipleObjects         = stub_WaitForMultipleObjects;
Proc_EnterCriticalSection           wpc_iat_EnterCriticalSection           = stub_EnterCriticalSection;
Proc_LeaveCriticalSection           wpc_iat_LeaveCriticalSection           = stub_LeaveCriticalSection;
Proc_TerminateThread                wpc_iat_TerminateThread                = stub_TerminateThread;
Proc_DeleteCriticalSection          wpc_iat_DeleteCriticalSection          = stub_DeleteCriticalSection;
Proc_CloseHandle                    wpc_iat_CloseHandle                    = stub_CloseHandle;
Proc_CreateEventA                   wpc_iat_CreateEventA                   = stub_CreateEventA;
Proc_FindFirstFileA                 wpc_iat_FindFirstFileA                 = stub_FindFirstFileA;
Proc_GlobalMemoryStatus             wpc_iat_GlobalMemoryStatus             = stub_GlobalMemoryStatus;
Proc_InitializeCriticalSection      wpc_iat_InitializeCriticalSection      = stub_InitializeCriticalSection;
Proc_FindClose                      wpc_iat_FindClose                      = stub_FindClose;
Proc_GetTickCount                   wpc_iat_GetTickCount                   = stub_GetTickCount;
Proc_GetEnvironmentStrings          wpc_iat_GetEnvironmentStrings          = stub_GetEnvironmentStrings;
Proc_IsBadCodePtr                   wpc_iat_IsBadCodePtr                   = stub_IsBadCodePtr;
Proc_SetStdHandle                   wpc_iat_SetStdHandle                   = stub_SetStdHandle;
Proc_FreeEnvironmentStringsA        wpc_iat_FreeEnvironmentStringsA        = stub_FreeEnvironmentStringsA;
Proc_UnhandledExceptionFilter       wpc_iat_UnhandledExceptionFilter       = stub_UnhandledExceptionFilter;
Proc_LCMapStringW                   wpc_iat_LCMapStringW                   = stub_LCMapStringW;
Proc_LCMapStringA                   wpc_iat_LCMapStringA                   = stub_LCMapStringA;
Proc_MultiByteToWideChar            wpc_iat_MultiByteToWideChar            = stub_MultiByteToWideChar;
Proc_WideCharToMultiByte            wpc_iat_WideCharToMultiByte            = stub_WideCharToMultiByte;
Proc_HeapCreate                     wpc_iat_HeapCreate                     = stub_HeapCreate;
Proc_HeapDestroy                    wpc_iat_HeapDestroy                    = stub_HeapDestroy;
Proc_GetVersionExA                  wpc_iat_GetVersionExA                  = stub_GetVersionExA;
Proc_GetEnvironmentVariableA        wpc_iat_GetEnvironmentVariableA        = stub_GetEnvironmentVariableA;
Proc_FreeEnvironmentStringsW        wpc_iat_FreeEnvironmentStringsW        = stub_FreeEnvironmentStringsW;
Proc_GetEnvironmentStringsW         wpc_iat_GetEnvironmentStringsW         = stub_GetEnvironmentStringsW;
Proc_VirtualAlloc                   wpc_iat_VirtualAlloc                   = stub_VirtualAlloc;
Proc_HeapReAlloc                    wpc_iat_HeapReAlloc                    = stub_HeapReAlloc;
Proc_HeapAlloc                      wpc_iat_HeapAlloc                      = stub_HeapAlloc;
Proc_GetFileType                    wpc_iat_GetFileType                    = stub_GetFileType;
Proc_SetHandleCount                 wpc_iat_SetHandleCount                 = stub_SetHandleCount;
Proc_SetFilePointer                 wpc_iat_SetFilePointer                 = stub_SetFilePointer;
Proc_SetUnhandledExceptionFilter    wpc_iat_SetUnhandledExceptionFilter    = stub_SetUnhandledExceptionFilter;
Proc_HeapFree                       wpc_iat_HeapFree                       = stub_HeapFree;
Proc_ReadFile                       wpc_iat_ReadFile                       = stub_ReadFile;
Proc_GetModuleFileNameA             wpc_iat_GetModuleFileNameA             = stub_GetModuleFileNameA;
Proc_InterlockedIncrement           wpc_iat_InterlockedIncrement           = stub_InterlockedIncrement;
Proc_InterlockedDecrement           wpc_iat_InterlockedDecrement           = stub_InterlockedDecrement;
Proc_WriteFile                      wpc_iat_WriteFile                      = stub_WriteFile;
Proc_GetStdHandle                   wpc_iat_GetStdHandle                   = stub_GetStdHandle;
Proc_DebugBreak                     wpc_iat_DebugBreak                     = stub_DebugBreak;
Proc_GetVersion                     wpc_iat_GetVersion                     = stub_GetVersion;
Proc_GetStartupInfoA                wpc_iat_GetStartupInfoA                = stub_GetStartupInfoA;
Proc_GetModuleHandleA               wpc_iat_GetModuleHandleA               = stub_GetModuleHandleA;
Proc_GetCurrentProcess              wpc_iat_GetCurrentProcess              = stub_GetCurrentProcess;
Proc_TerminateProcess               wpc_iat_TerminateProcess               = stub_TerminateProcess;
Proc_ExitProcess                    wpc_iat_ExitProcess                    = stub_ExitProcess;
Proc_SetEndOfFile                   wpc_iat_SetEndOfFile                   = stub_SetEndOfFile;
Proc_GetOEMCP                       wpc_iat_GetOEMCP                       = stub_GetOEMCP;
Proc_GetACP                         wpc_iat_GetACP                         = stub_GetACP;
Proc_GetCPInfo                      wpc_iat_GetCPInfo                      = stub_GetCPInfo;
Proc_GetStringTypeW                 wpc_iat_GetStringTypeW                 = stub_GetStringTypeW;
Proc_GetStringTypeA                 wpc_iat_GetStringTypeA                 = stub_GetStringTypeA;
Proc_SetConsoleCtrlHandler          wpc_iat_SetConsoleCtrlHandler          = stub_SetConsoleCtrlHandler;
Proc_FlushFileBuffers               wpc_iat_FlushFileBuffers               = stub_FlushFileBuffers;
Proc_VirtualFree                    wpc_iat_VirtualFree                    = stub_VirtualFree;
Proc_HeapValidate                   wpc_iat_HeapValidate                   = stub_HeapValidate;
Proc_IsBadReadPtr                   wpc_iat_IsBadReadPtr                   = stub_IsBadReadPtr;
Proc_RtlUnwind                      wpc_iat_RtlUnwind                      = stub_RtlUnwind;
Proc_IsBadWritePtr                  wpc_iat_IsBadWritePtr                  = stub_IsBadWritePtr;
Proc_RaiseException                 wpc_iat_RaiseException                 = stub_RaiseException;

Proc_ClientToScreen                 wpc_iat_ClientToScreen                 = stub_ClientToScreen;
Proc_MessageBoxA                    wpc_iat_MessageBoxA                    = stub_MessageBoxA;
Proc_GetActiveWindow                wpc_iat_GetActiveWindow                = stub_GetActiveWindow;
Proc_wsprintfA                      wpc_iat_wsprintfA                      = stub_wsprintfA;
Proc_UpdateWindow                   wpc_iat_UpdateWindow                   = stub_UpdateWindow;
Proc_ShowWindow                     wpc_iat_ShowWindow                     = stub_ShowWindow;
Proc_CreateWindowExA                wpc_iat_CreateWindowExA                = stub_CreateWindowExA;
Proc_GetWindowLongA                 wpc_iat_GetWindowLongA                 = stub_GetWindowLongA;
Proc_MoveWindow                     wpc_iat_MoveWindow                     = stub_MoveWindow;
Proc_SendMessageA                   wpc_iat_SendMessageA                   = stub_SendMessageA;
Proc_DefWindowProcA                 wpc_iat_DefWindowProcA                 = stub_DefWindowProcA;
Proc_GetKeyState                    wpc_iat_GetKeyState                    = stub_GetKeyState;
Proc_EndPaint                       wpc_iat_EndPaint                       = stub_EndPaint;
Proc_BeginPaint                     wpc_iat_BeginPaint                     = stub_BeginPaint;
Proc_DrawMenuBar                    wpc_iat_DrawMenuBar                    = stub_DrawMenuBar;
Proc_RedrawWindow                   wpc_iat_RedrawWindow                   = stub_RedrawWindow;
Proc_RegisterClassA                 wpc_iat_RegisterClassA                 = stub_RegisterClassA;
Proc_SetRect                        wpc_iat_SetRect                        = stub_SetRect;
Proc_PeekMessageA                   wpc_iat_PeekMessageA                   = stub_PeekMessageA;
Proc_TranslateMessage               wpc_iat_TranslateMessage               = stub_TranslateMessage;
Proc_DispatchMessageA               wpc_iat_DispatchMessageA               = stub_DispatchMessageA;
Proc_GetClientRect                  wpc_iat_GetClientRect                  = stub_GetClientRect;
Proc_UnregisterClassA               wpc_iat_UnregisterClassA               = stub_UnregisterClassA;
Proc_SetFocus                       wpc_iat_SetFocus                       = stub_SetFocus;
Proc_ShowCursor                     wpc_iat_ShowCursor                     = stub_ShowCursor;
Proc_SetCapture                     wpc_iat_SetCapture                     = stub_SetCapture;
Proc_SetCursor                      wpc_iat_SetCursor                      = stub_SetCursor;
Proc_PostQuitMessage                wpc_iat_PostQuitMessage                = stub_PostQuitMessage;
Proc_GetKeyNameTextA                wpc_iat_GetKeyNameTextA                = stub_GetKeyNameTextA;
Proc_GetCursorPos                   wpc_iat_GetCursorPos                   = stub_GetCursorPos;
Proc_SetCursorPos                   wpc_iat_SetCursorPos                   = stub_SetCursorPos;
Proc_GetSystemMetrics               wpc_iat_GetSystemMetrics               = stub_GetSystemMetrics;
Proc_LoadIconA                      wpc_iat_LoadIconA                      = stub_LoadIconA;
Proc_LoadCursorA                    wpc_iat_LoadCursorA                    = stub_LoadCursorA;
Proc_DestroyWindow                  wpc_iat_DestroyWindow                  = stub_DestroyWindow;
Proc_WaitMessage                    wpc_iat_WaitMessage                    = stub_WaitMessage;
Proc_ReleaseCapture                 wpc_iat_ReleaseCapture                 = stub_ReleaseCapture;

Proc_SetBkMode                      wpc_iat_SetBkMode                      = stub_SetBkMode;
Proc_ExtTextOutA                    wpc_iat_ExtTextOutA                    = stub_ExtTextOutA;
Proc_GetStockObject                 wpc_iat_GetStockObject                 = stub_GetStockObject;
Proc_SetTextColor                   wpc_iat_SetTextColor                   = stub_SetTextColor;

Proc_RegDeleteValueA                wpc_iat_RegDeleteValueA                = stub_RegDeleteValueA;
Proc_RegSetValueExA                 wpc_iat_RegSetValueExA                 = stub_RegSetValueExA;
Proc_RegCloseKey                    wpc_iat_RegCloseKey                    = stub_RegCloseKey;
Proc_RegDeleteKeyA                  wpc_iat_RegDeleteKeyA                  = stub_RegDeleteKeyA;
Proc_RegCreateKeyExA                wpc_iat_RegCreateKeyExA                = stub_RegCreateKeyExA;
Proc_RegQueryValueExA               wpc_iat_RegQueryValueExA               = stub_RegQueryValueExA;
Proc_RegOpenKeyExA                  wpc_iat_RegOpenKeyExA                  = stub_RegOpenKeyExA;

Proc_mixerSetControlDetails         wpc_iat_mixerSetControlDetails         = stub_mixerSetControlDetails;
Proc_mixerGetNumDevs                wpc_iat_mixerGetNumDevs                = stub_mixerGetNumDevs;
Proc_mixerGetDevCapsA               wpc_iat_mixerGetDevCapsA               = stub_mixerGetDevCapsA;
Proc_mixerGetLineInfoA              wpc_iat_mixerGetLineInfoA              = stub_mixerGetLineInfoA;
Proc_mixerGetLineControlsA          wpc_iat_mixerGetLineControlsA          = stub_mixerGetLineControlsA;
Proc_timeKillEvent                  wpc_iat_timeKillEvent                  = stub_timeKillEvent;
Proc_timeSetEvent                   wpc_iat_timeSetEvent                   = stub_timeSetEvent;
Proc_mciSendCommandA                wpc_iat_mciSendCommandA                = stub_mciSendCommandA;
Proc_timeGetTime                    wpc_iat_timeGetTime                    = stub_timeGetTime;

Proc_DirectDrawCreateEx             wpc_iat_DirectDrawCreateEx             = stub_DirectDrawCreateEx;
Proc_DirectInputCreateEx            wpc_iat_DirectInputCreateEx            = stub_DirectInputCreateEx;
Proc_DirectSoundCreate              wpc_iat_DirectSoundCreate              = stub_DirectSoundCreate;

Proc_AVIStreamGetFrameClose         wpc_iat_AVIStreamGetFrameClose         = stub_AVIStreamGetFrameClose;
Proc_AVIStreamReadFormat            wpc_iat_AVIStreamReadFormat            = stub_AVIStreamReadFormat;
Proc_AVIStreamGetFrameOpen          wpc_iat_AVIStreamGetFrameOpen          = stub_AVIStreamGetFrameOpen;
Proc_AVIStreamInfoA                 wpc_iat_AVIStreamInfoA                 = stub_AVIStreamInfoA;
Proc_AVIFileRelease                 wpc_iat_AVIFileRelease                 = stub_AVIFileRelease;
Proc_AVIStreamGetFrame              wpc_iat_AVIStreamGetFrame              = stub_AVIStreamGetFrame;
Proc_AVIStreamRead                  wpc_iat_AVIStreamRead                  = stub_AVIStreamRead;
Proc_AVIFileExit                    wpc_iat_AVIFileExit                    = stub_AVIFileExit;
Proc_AVIFileInit                    wpc_iat_AVIFileInit                    = stub_AVIFileInit;
Proc_AVIFileGetStream               wpc_iat_AVIFileGetStream               = stub_AVIFileGetStream;
Proc_AVIFileOpenA                   wpc_iat_AVIFileOpenA                   = stub_AVIFileOpenA;
Proc_AVIStreamRelease               wpc_iat_AVIStreamRelease               = stub_AVIStreamRelease;

Proc_DrawDibOpen                    wpc_iat_DrawDibOpen                    = stub_DrawDibOpen;
Proc_DrawDibDraw                    wpc_iat_DrawDibDraw                    = stub_DrawDibDraw;
Proc_DrawDibClose                   wpc_iat_DrawDibClose                   = stub_DrawDibClose;
