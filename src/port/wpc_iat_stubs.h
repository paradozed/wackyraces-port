#ifndef WPC_IAT_STUBS_H
#define WPC_IAT_STUBS_H

/*
 * wpc_iat_stubs.h - Import Address Table stubs for the native Linux port.
 *
 * The recovered game code calls Win32 APIs through indirect pointers named
 * wpc_iat_*. On Linux there is no Win32, so these symbols are defined here
 * as function pointers to Linux-compatible stubs. Each stub mirrors the
 * original calling convention and returns a benign default so the recovered
 * code can link and run (albeit with limited functionality) until the
 * corresponding subsystem is ported.
 */

#if !defined(WINAPI)
#define WINAPI
#endif

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- KERNEL32.dll ---- */

typedef uint32_t (WINAPI *Proc_GetLogicalDriveStringsA)(uint32_t, char *);
typedef void *   (WINAPI *Proc_LoadLibraryA)(const char *);
typedef int      (WINAPI *Proc_FreeLibrary)(void *);
typedef int      (WINAPI *Proc_SetCurrentDirectoryA)(const char *);
typedef char *   (WINAPI *Proc_GetCommandLineA)(void);
typedef uint32_t (WINAPI *Proc_GetLastError)(void);
typedef void *   (WINAPI *Proc_CreateFileMappingA)(void *, void *, uint32_t, uint32_t, uint32_t, const char *);
typedef uint32_t (WINAPI *Proc_GetDriveTypeA)(const char *);
typedef int      (WINAPI *Proc_DeviceIoControl)(void *, uint32_t, const void *, uint32_t, void *, uint32_t, uint32_t *, void *);
typedef void *   (WINAPI *Proc_CreateFileA)(const char *, uint32_t, uint32_t, void *, uint32_t, uint32_t, void *);
typedef int      (WINAPI *Proc_FindNextFileA)(void *, void *);
typedef void *   (WINAPI *Proc_GetProcAddress)(void *, const char *);
typedef int      (WINAPI *Proc_CreateDirectoryA)(const char *, void *);
typedef uint32_t (WINAPI *Proc_GetCurrentDirectoryA)(uint32_t, char *);
typedef int      (WINAPI *Proc_DeleteFileA)(const char *);
typedef void     (WINAPI *Proc_OutputDebugStringA)(const char *);
typedef int      (WINAPI *Proc_lstrcatA)(char *, const char *);
typedef void *   (WINAPI *Proc_CreateThread)(void *, uint32_t, void *, void *, uint32_t, uint32_t *);
typedef uint32_t (WINAPI *Proc_WaitForMultipleObjects)(uint32_t, void **, int, uint32_t);
typedef void     (WINAPI *Proc_EnterCriticalSection)(void *);
typedef void     (WINAPI *Proc_LeaveCriticalSection)(void *);
typedef int      (WINAPI *Proc_TerminateThread)(void *, uint32_t);
typedef void     (WINAPI *Proc_DeleteCriticalSection)(void *);
typedef int      (WINAPI *Proc_CloseHandle)(void *);
typedef void *   (WINAPI *Proc_CreateEventA)(void *, int, int, const char *);
typedef void *   (WINAPI *Proc_FindFirstFileA)(const char *, void *);
typedef void     (WINAPI *Proc_GlobalMemoryStatus)(void *);
typedef void     (WINAPI *Proc_InitializeCriticalSection)(void *);
typedef int      (WINAPI *Proc_FindClose)(void *);
typedef uint32_t (WINAPI *Proc_GetTickCount)(void);
typedef char *   (WINAPI *Proc_GetEnvironmentStrings)(void);
typedef int      (WINAPI *Proc_IsBadCodePtr)(void *);
typedef void     (WINAPI *Proc_SetStdHandle)(uint32_t, void *);
typedef int      (WINAPI *Proc_FreeEnvironmentStringsA)(char *);
typedef int      (WINAPI *Proc_UnhandledExceptionFilter)(void *);
typedef int      (WINAPI *Proc_LCMapStringW)(uint32_t, uint32_t, const void *, int, void *, int);
typedef int      (WINAPI *Proc_LCMapStringA)(uint32_t, uint32_t, const char *, int, char *, int);
typedef int      (WINAPI *Proc_MultiByteToWideChar)(uint32_t, uint32_t, const char *, int, void *, int);
typedef int      (WINAPI *Proc_WideCharToMultiByte)(uint32_t, uint32_t, const void *, int, char *, int, const char *, int *);
typedef void *   (WINAPI *Proc_HeapCreate)(uint32_t, uint32_t, uint32_t);
typedef int      (WINAPI *Proc_HeapDestroy)(void *);
typedef int      (WINAPI *Proc_GetVersionExA)(void *);
typedef uint32_t (WINAPI *Proc_GetEnvironmentVariableA)(const char *, char *, uint32_t);
typedef int      (WINAPI *Proc_FreeEnvironmentStringsW)(void *);
typedef void *   (WINAPI *Proc_GetEnvironmentStringsW)(void);
typedef void *   (WINAPI *Proc_VirtualAlloc)(void *, uint32_t, uint32_t, uint32_t);
typedef void *   (WINAPI *Proc_HeapReAlloc)(void *, uint32_t, void *, uint32_t);
typedef void *   (WINAPI *Proc_HeapAlloc)(void *, uint32_t, uint32_t);
typedef uint32_t (WINAPI *Proc_GetFileType)(void *);
typedef int      (WINAPI *Proc_SetHandleCount)(uint32_t);
typedef uint32_t (WINAPI *Proc_SetFilePointer)(void *, int32_t, int32_t *, uint32_t);
typedef int      (WINAPI *Proc_SetUnhandledExceptionFilter)(void *);
typedef int      (WINAPI *Proc_HeapFree)(void *, uint32_t, void *);
typedef int      (WINAPI *Proc_ReadFile)(void *, void *, uint32_t, uint32_t *, void *);
typedef uint32_t (WINAPI *Proc_GetModuleFileNameA)(void *, char *, uint32_t);
typedef int      (WINAPI *Proc_InterlockedIncrement)(volatile int32_t *);
typedef int      (WINAPI *Proc_InterlockedDecrement)(volatile int32_t *);
typedef int      (WINAPI *Proc_WriteFile)(void *, const void *, uint32_t, uint32_t *, void *);
typedef void *   (WINAPI *Proc_GetStdHandle)(uint32_t);
typedef void     (WINAPI *Proc_DebugBreak)(void);
typedef uint32_t (WINAPI *Proc_GetVersion)(void);
typedef int      (WINAPI *Proc_GetStartupInfoA)(void *);
typedef void *   (WINAPI *Proc_GetModuleHandleA)(const char *);
typedef void *   (WINAPI *Proc_GetCurrentProcess)(void);
typedef int      (WINAPI *Proc_TerminateProcess)(void *, uint32_t);
typedef int      (WINAPI *Proc_ExitProcess)(uint32_t);
typedef int      (WINAPI *Proc_SetEndOfFile)(void *);
typedef uint32_t (WINAPI *Proc_GetOEMCP)(void);
typedef uint32_t (WINAPI *Proc_GetACP)(void);
typedef int      (WINAPI *Proc_GetCPInfo)(uint32_t, void *);
typedef int      (WINAPI *Proc_GetStringTypeW)(uint32_t, const void *, int, void *);
typedef int      (WINAPI *Proc_GetStringTypeA)(uint32_t, uint32_t, const char *, int, void *);
typedef int      (WINAPI *Proc_SetConsoleCtrlHandler)(void *, int);
typedef int      (WINAPI *Proc_FlushFileBuffers)(void *);
typedef int      (WINAPI *Proc_VirtualFree)(void *, uint32_t, uint32_t);
typedef int      (WINAPI *Proc_HeapValidate)(void *, uint32_t, const void *);
typedef int      (WINAPI *Proc_IsBadReadPtr)(const void *, uint32_t);
typedef void     (WINAPI *Proc_RtlUnwind)(void *, void *, void *, void *);
typedef int      (WINAPI *Proc_IsBadWritePtr)(void *, uint32_t);
typedef int      (WINAPI *Proc_RaiseException)(uint32_t, uint32_t, uint32_t, const void *);

/* ---- USER32.dll ---- */

typedef int      (WINAPI *Proc_ClientToScreen)(void *, void *);
typedef int      (WINAPI *Proc_MessageBoxA)(void *, const char *, const char *, uint32_t);
typedef void *   (WINAPI *Proc_GetActiveWindow)(void);
typedef int      (WINAPI *Proc_wsprintfA)(char *, const char *, ...);
typedef int      (WINAPI *Proc_UpdateWindow)(void *);
typedef int      (WINAPI *Proc_ShowWindow)(void *, int);
typedef void *   (WINAPI *Proc_CreateWindowExA)(uint32_t, const char *, const char *, uint32_t, int, int, int, int, void *, void *, void *, void *);
typedef int32_t  (WINAPI *Proc_GetWindowLongA)(void *, int);
typedef int      (WINAPI *Proc_MoveWindow)(void *, int, int, int, int, int);
typedef int      (WINAPI *Proc_SendMessageA)(void *, uint32_t, uint32_t, intptr_t);
typedef intptr_t (WINAPI *Proc_DefWindowProcA)(void *, uint32_t, uint32_t, intptr_t);
typedef int16_t  (WINAPI *Proc_GetKeyState)(int);
typedef int      (WINAPI *Proc_EndPaint)(void *, void *);
typedef void *   (WINAPI *Proc_BeginPaint)(void *, void *);
typedef int      (WINAPI *Proc_DrawMenuBar)(void *);
typedef int      (WINAPI *Proc_RedrawWindow)(void *, void *, void *, uint32_t);
typedef int      (WINAPI *Proc_RegisterClassA)(const void *);
typedef void     (WINAPI *Proc_SetRect)(void *, int, int, int, int);
typedef int      (WINAPI *Proc_PeekMessageA)(void *, void *, uint32_t, uint32_t, uint32_t);
typedef int      (WINAPI *Proc_TranslateMessage)(const void *);
typedef intptr_t (WINAPI *Proc_DispatchMessageA)(const void *);
typedef int      (WINAPI *Proc_GetClientRect)(void *, void *);
typedef int      (WINAPI *Proc_UnregisterClassA)(const char *, void *);
typedef int      (WINAPI *Proc_SetFocus)(void *);
typedef int      (WINAPI *Proc_ShowCursor)(int);
typedef void *   (WINAPI *Proc_SetCapture)(void *);
typedef void *   (WINAPI *Proc_SetCursor)(void *);
typedef void     (WINAPI *Proc_PostQuitMessage)(int);
typedef int      (WINAPI *Proc_GetKeyNameTextA)(int32_t, char *, int);
typedef int      (WINAPI *Proc_GetCursorPos)(void *);
typedef int      (WINAPI *Proc_SetCursorPos)(int, int);
typedef int      (WINAPI *Proc_GetSystemMetrics)(int);
typedef void *   (WINAPI *Proc_LoadIconA)(void *, const char *);
typedef void *   (WINAPI *Proc_LoadCursorA)(void *, const char *);
typedef int      (WINAPI *Proc_DestroyWindow)(void *);
typedef int      (WINAPI *Proc_WaitMessage)(void);
typedef int      (WINAPI *Proc_ReleaseCapture)(void);

/* ---- GDI32.dll ---- */

typedef int      (WINAPI *Proc_SetBkMode)(void *, int);
typedef int      (WINAPI *Proc_ExtTextOutA)(void *, int, int, uint32_t, const void *, const char *, uint32_t, const void *);
typedef void *   (WINAPI *Proc_GetStockObject)(int);
typedef int      (WINAPI *Proc_SetTextColor)(void *, uint32_t);

/* ---- ADVAPI32.dll ---- */

typedef int      (WINAPI *Proc_RegDeleteValueA)(void *, const char *);
typedef int      (WINAPI *Proc_RegSetValueExA)(void *, const char *, uint32_t, uint32_t, const void *, uint32_t);
typedef int      (WINAPI *Proc_RegCloseKey)(void *);
typedef int      (WINAPI *Proc_RegDeleteKeyA)(void *, const char *);
typedef int      (WINAPI *Proc_RegCreateKeyExA)(void *, const char *, uint32_t, char *, uint32_t, uint32_t, void *, void **, uint32_t *);
typedef int      (WINAPI *Proc_RegQueryValueExA)(void *, const char *, uint32_t *, uint32_t *, void *, uint32_t *);
typedef int      (WINAPI *Proc_RegOpenKeyExA)(void *, const char *, uint32_t, uint32_t, void **);

/* ---- WINMM.dll ---- */

typedef int      (WINAPI *Proc_mixerSetControlDetails)(void *, void *, uint32_t);
typedef int      (WINAPI *Proc_mixerGetNumDevs)(void);
typedef int      (WINAPI *Proc_mixerGetDevCapsA)(uint32_t, void *, uint32_t);
typedef int      (WINAPI *Proc_mixerGetLineInfoA)(void *, void *);
typedef int      (WINAPI *Proc_mixerGetLineControlsA)(void *, void *, uint32_t);
typedef int      (WINAPI *Proc_timeKillEvent)(int);
typedef int      (WINAPI *Proc_timeSetEvent)(uint32_t, uint32_t, void *, uint32_t, uint32_t);
typedef int      (WINAPI *Proc_mciSendCommandA)(uint32_t, uint32_t, uint32_t, void *);
typedef uint32_t (WINAPI *Proc_timeGetTime)(void);

/* ---- DDRAW.dll ---- */

typedef int      (WINAPI *Proc_DirectDrawCreateEx)(void *, void **, const void *, void *);

/* ---- DINPUT.dll ---- */

typedef int      (WINAPI *Proc_DirectInputCreateEx)(void *, uint32_t, const void *, void **, void *);

/* ---- DSOUND.dll ---- */

typedef int      (WINAPI *Proc_DirectSoundCreate)(void *, void **, void *);

/* ---- AVIFIL32.dll ---- */

typedef int      (WINAPI *Proc_AVIStreamGetFrameClose)(void *);
typedef int      (WINAPI *Proc_AVIStreamReadFormat)(void *, int, void *, int *);
typedef void *   (WINAPI *Proc_AVIStreamGetFrameOpen)(void *, void *);
typedef int      (WINAPI *Proc_AVIStreamInfoA)(void *, void *, uint32_t);
typedef int      (WINAPI *Proc_AVIFileRelease)(void *);
typedef int      (WINAPI *Proc_AVIStreamGetFrame)(void *, int, void *);
typedef int      (WINAPI *Proc_AVIStreamRead)(void *, int, int, void *, int, int *, int *);
typedef int      (WINAPI *Proc_AVIFileExit)(void);
typedef int      (WINAPI *Proc_AVIFileInit)(void);
typedef int      (WINAPI *Proc_AVIFileGetStream)(void *, void **, uint32_t, int);
typedef int      (WINAPI *Proc_AVIFileOpenA)(void **, const char *, uint32_t, void *);
typedef int      (WINAPI *Proc_AVIStreamRelease)(void *);

/* ---- MSVFW32.dll ---- */

typedef void *   (WINAPI *Proc_DrawDibOpen)(void);
typedef int      (WINAPI *Proc_DrawDibDraw)(void *, void *, int, int, void *, void *, int, int, int, int, uint32_t);
typedef int      (WINAPI *Proc_DrawDibClose)(void *);

/* ---- Import address table entries (defined in wpc_iat_stubs.cpp) ---- */

extern Proc_GetLogicalDriveStringsA        wpc_iat_GetLogicalDriveStringsA;
extern Proc_LoadLibraryA                   wpc_iat_LoadLibraryA;
extern Proc_FreeLibrary                    wpc_iat_FreeLibrary;
extern Proc_SetCurrentDirectoryA           wpc_iat_SetCurrentDirectoryA;
extern Proc_GetCommandLineA                wpc_iat_GetCommandLineA;
extern Proc_GetLastError                   wpc_iat_GetLastError;
extern Proc_CreateFileMappingA             wpc_iat_CreateFileMappingA;
extern Proc_GetDriveTypeA                  wpc_iat_GetDriveTypeA;
extern Proc_DeviceIoControl                wpc_iat_DeviceIoControl;
extern Proc_CreateFileA                    wpc_iat_CreateFileA;
extern Proc_FindNextFileA                  wpc_iat_FindNextFileA;
extern Proc_GetProcAddress                 wpc_iat_GetProcAddress;
extern Proc_CreateDirectoryA               wpc_iat_CreateDirectoryA;
extern Proc_GetCurrentDirectoryA           wpc_iat_GetCurrentDirectoryA;
extern Proc_DeleteFileA                    wpc_iat_DeleteFileA;
extern Proc_OutputDebugStringA             wpc_iat_OutputDebugStringA;
extern Proc_lstrcatA                       wpc_iat_lstrcatA;
extern Proc_CreateThread                   wpc_iat_CreateThread;
extern Proc_WaitForMultipleObjects         wpc_iat_WaitForMultipleObjects;
extern Proc_EnterCriticalSection           wpc_iat_EnterCriticalSection;
extern Proc_LeaveCriticalSection           wpc_iat_LeaveCriticalSection;
extern Proc_TerminateThread                wpc_iat_TerminateThread;
extern Proc_DeleteCriticalSection          wpc_iat_DeleteCriticalSection;
extern Proc_CloseHandle                    wpc_iat_CloseHandle;
extern Proc_CreateEventA                   wpc_iat_CreateEventA;
extern Proc_FindFirstFileA                 wpc_iat_FindFirstFileA;
extern Proc_GlobalMemoryStatus             wpc_iat_GlobalMemoryStatus;
extern Proc_InitializeCriticalSection      wpc_iat_InitializeCriticalSection;
extern Proc_FindClose                      wpc_iat_FindClose;
extern Proc_GetTickCount                   wpc_iat_GetTickCount;
extern Proc_GetEnvironmentStrings          wpc_iat_GetEnvironmentStrings;
extern Proc_IsBadCodePtr                   wpc_iat_IsBadCodePtr;
extern Proc_SetStdHandle                   wpc_iat_SetStdHandle;
extern Proc_FreeEnvironmentStringsA        wpc_iat_FreeEnvironmentStringsA;
extern Proc_UnhandledExceptionFilter       wpc_iat_UnhandledExceptionFilter;
extern Proc_LCMapStringW                   wpc_iat_LCMapStringW;
extern Proc_LCMapStringA                   wpc_iat_LCMapStringA;
extern Proc_MultiByteToWideChar            wpc_iat_MultiByteToWideChar;
extern Proc_WideCharToMultiByte            wpc_iat_WideCharToMultiByte;
extern Proc_HeapCreate                     wpc_iat_HeapCreate;
extern Proc_HeapDestroy                    wpc_iat_HeapDestroy;
extern Proc_GetVersionExA                  wpc_iat_GetVersionExA;
extern Proc_GetEnvironmentVariableA        wpc_iat_GetEnvironmentVariableA;
extern Proc_FreeEnvironmentStringsW        wpc_iat_FreeEnvironmentStringsW;
extern Proc_GetEnvironmentStringsW         wpc_iat_GetEnvironmentStringsW;
extern Proc_VirtualAlloc                   wpc_iat_VirtualAlloc;
extern Proc_HeapReAlloc                    wpc_iat_HeapReAlloc;
extern Proc_HeapAlloc                      wpc_iat_HeapAlloc;
extern Proc_GetFileType                    wpc_iat_GetFileType;
extern Proc_SetHandleCount                 wpc_iat_SetHandleCount;
extern Proc_SetFilePointer                 wpc_iat_SetFilePointer;
extern Proc_SetUnhandledExceptionFilter    wpc_iat_SetUnhandledExceptionFilter;
extern Proc_HeapFree                       wpc_iat_HeapFree;
extern Proc_ReadFile                       wpc_iat_ReadFile;
extern Proc_GetModuleFileNameA             wpc_iat_GetModuleFileNameA;
extern Proc_InterlockedIncrement           wpc_iat_InterlockedIncrement;
extern Proc_InterlockedDecrement           wpc_iat_InterlockedDecrement;
extern Proc_WriteFile                      wpc_iat_WriteFile;
extern Proc_GetStdHandle                   wpc_iat_GetStdHandle;
extern Proc_DebugBreak                     wpc_iat_DebugBreak;
extern Proc_GetVersion                     wpc_iat_GetVersion;
extern Proc_GetStartupInfoA                wpc_iat_GetStartupInfoA;
extern Proc_GetModuleHandleA               wpc_iat_GetModuleHandleA;
extern Proc_GetCurrentProcess              wpc_iat_GetCurrentProcess;
extern Proc_TerminateProcess               wpc_iat_TerminateProcess;
extern Proc_ExitProcess                    wpc_iat_ExitProcess;
extern Proc_SetEndOfFile                   wpc_iat_SetEndOfFile;
extern Proc_GetOEMCP                       wpc_iat_GetOEMCP;
extern Proc_GetACP                         wpc_iat_GetACP;
extern Proc_GetCPInfo                      wpc_iat_GetCPInfo;
extern Proc_GetStringTypeW                 wpc_iat_GetStringTypeW;
extern Proc_GetStringTypeA                 wpc_iat_GetStringTypeA;
extern Proc_SetConsoleCtrlHandler          wpc_iat_SetConsoleCtrlHandler;
extern Proc_FlushFileBuffers               wpc_iat_FlushFileBuffers;
extern Proc_VirtualFree                    wpc_iat_VirtualFree;
extern Proc_HeapValidate                   wpc_iat_HeapValidate;
extern Proc_IsBadReadPtr                   wpc_iat_IsBadReadPtr;
extern Proc_RtlUnwind                      wpc_iat_RtlUnwind;
extern Proc_IsBadWritePtr                  wpc_iat_IsBadWritePtr;
extern Proc_RaiseException                 wpc_iat_RaiseException;

extern Proc_ClientToScreen                 wpc_iat_ClientToScreen;
extern Proc_MessageBoxA                    wpc_iat_MessageBoxA;
extern Proc_GetActiveWindow                wpc_iat_GetActiveWindow;
extern Proc_wsprintfA                      wpc_iat_wsprintfA;
extern Proc_UpdateWindow                   wpc_iat_UpdateWindow;
extern Proc_ShowWindow                     wpc_iat_ShowWindow;
extern Proc_CreateWindowExA                wpc_iat_CreateWindowExA;
extern Proc_GetWindowLongA                 wpc_iat_GetWindowLongA;
extern Proc_MoveWindow                     wpc_iat_MoveWindow;
extern Proc_SendMessageA                   wpc_iat_SendMessageA;
extern Proc_DefWindowProcA                 wpc_iat_DefWindowProcA;
extern Proc_GetKeyState                    wpc_iat_GetKeyState;
extern Proc_EndPaint                       wpc_iat_EndPaint;
extern Proc_BeginPaint                     wpc_iat_BeginPaint;
extern Proc_DrawMenuBar                    wpc_iat_DrawMenuBar;
extern Proc_RedrawWindow                   wpc_iat_RedrawWindow;
extern Proc_RegisterClassA                 wpc_iat_RegisterClassA;
extern Proc_SetRect                        wpc_iat_SetRect;
extern Proc_PeekMessageA                   wpc_iat_PeekMessageA;
extern Proc_TranslateMessage               wpc_iat_TranslateMessage;
extern Proc_DispatchMessageA               wpc_iat_DispatchMessageA;
extern Proc_GetClientRect                  wpc_iat_GetClientRect;
extern Proc_UnregisterClassA               wpc_iat_UnregisterClassA;
extern Proc_SetFocus                       wpc_iat_SetFocus;
extern Proc_ShowCursor                     wpc_iat_ShowCursor;
extern Proc_SetCapture                     wpc_iat_SetCapture;
extern Proc_SetCursor                      wpc_iat_SetCursor;
extern Proc_PostQuitMessage                wpc_iat_PostQuitMessage;
extern Proc_GetKeyNameTextA                wpc_iat_GetKeyNameTextA;
extern Proc_GetCursorPos                   wpc_iat_GetCursorPos;
extern Proc_SetCursorPos                   wpc_iat_SetCursorPos;
extern Proc_GetSystemMetrics               wpc_iat_GetSystemMetrics;
extern Proc_LoadIconA                      wpc_iat_LoadIconA;
extern Proc_LoadCursorA                    wpc_iat_LoadCursorA;
extern Proc_DestroyWindow                  wpc_iat_DestroyWindow;
extern Proc_WaitMessage                    wpc_iat_WaitMessage;
extern Proc_ReleaseCapture                 wpc_iat_ReleaseCapture;

extern Proc_SetBkMode                      wpc_iat_SetBkMode;
extern Proc_ExtTextOutA                    wpc_iat_ExtTextOutA;
extern Proc_GetStockObject                 wpc_iat_GetStockObject;
extern Proc_SetTextColor                   wpc_iat_SetTextColor;

extern Proc_RegDeleteValueA                wpc_iat_RegDeleteValueA;
extern Proc_RegSetValueExA                 wpc_iat_RegSetValueExA;
extern Proc_RegCloseKey                    wpc_iat_RegCloseKey;
extern Proc_RegDeleteKeyA                  wpc_iat_RegDeleteKeyA;
extern Proc_RegCreateKeyExA                wpc_iat_RegCreateKeyExA;
extern Proc_RegQueryValueExA               wpc_iat_RegQueryValueExA;
extern Proc_RegOpenKeyExA                  wpc_iat_RegOpenKeyExA;

extern Proc_mixerSetControlDetails         wpc_iat_mixerSetControlDetails;
extern Proc_mixerGetNumDevs                wpc_iat_mixerGetNumDevs;
extern Proc_mixerGetDevCapsA               wpc_iat_mixerGetDevCapsA;
extern Proc_mixerGetLineInfoA              wpc_iat_mixerGetLineInfoA;
extern Proc_mixerGetLineControlsA          wpc_iat_mixerGetLineControlsA;
extern Proc_timeKillEvent                  wpc_iat_timeKillEvent;
extern Proc_timeSetEvent                   wpc_iat_timeSetEvent;
extern Proc_mciSendCommandA                wpc_iat_mciSendCommandA;
extern Proc_timeGetTime                    wpc_iat_timeGetTime;

extern Proc_DirectDrawCreateEx             wpc_iat_DirectDrawCreateEx;
extern Proc_DirectInputCreateEx            wpc_iat_DirectInputCreateEx;
extern Proc_DirectSoundCreate              wpc_iat_DirectSoundCreate;

extern Proc_AVIStreamGetFrameClose         wpc_iat_AVIStreamGetFrameClose;
extern Proc_AVIStreamReadFormat            wpc_iat_AVIStreamReadFormat;
extern Proc_AVIStreamGetFrameOpen          wpc_iat_AVIStreamGetFrameOpen;
extern Proc_AVIStreamInfoA                 wpc_iat_AVIStreamInfoA;
extern Proc_AVIFileRelease                 wpc_iat_AVIFileRelease;
extern Proc_AVIStreamGetFrame              wpc_iat_AVIStreamGetFrame;
extern Proc_AVIStreamRead                  wpc_iat_AVIStreamRead;
extern Proc_AVIFileExit                    wpc_iat_AVIFileExit;
extern Proc_AVIFileInit                    wpc_iat_AVIFileInit;
extern Proc_AVIFileGetStream               wpc_iat_AVIFileGetStream;
extern Proc_AVIFileOpenA                   wpc_iat_AVIFileOpenA;
extern Proc_AVIStreamRelease               wpc_iat_AVIStreamRelease;

extern Proc_DrawDibOpen                    wpc_iat_DrawDibOpen;
extern Proc_DrawDibDraw                    wpc_iat_DrawDibDraw;
extern Proc_DrawDibClose                   wpc_iat_DrawDibClose;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WPC_IAT_STUBS_H */
