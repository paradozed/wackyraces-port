#ifndef WPC_PLATFORM_H
#define WPC_PLATFORM_H

/*
 * wpc_platform.h - Thin platform abstraction for the native Wacky Races port.
 *
 * The recovered game source was written for Windows/MSVC and uses Win32
 * APIs extensively. This header defines the interface the native Linux port
 * implements so the recovered code can be compiled with minimal changes.
 *
 * Platform-specific implementations live in src/port/linux_platform.{cpp,h}.
 * Rendering uses Vulkan instead of OpenGL.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Window / display                                                   */
/* ------------------------------------------------------------------ */

/* Opaque handle to a native window. */
typedef struct WpcWindow WpcWindow;

/* Create a window with the given title and size. Returns NULL on failure. */
WpcWindow *wpc_window_create(const char *title, int width, int height);

/* Destroy a window. */
void wpc_window_destroy(WpcWindow *window);

/* Pump the event queue. Returns 0 while the app should keep running,
 * non-zero when it should exit. */
int wpc_window_poll_events(WpcWindow *window);

/* Swap buffers / present the rendered frame. */
void wpc_window_present(WpcWindow *window);

/* Get window size. */
void wpc_window_get_size(WpcWindow *window, int *width, int *height);

/* ------------------------------------------------------------------ */
/* Rendering (Vulkan backend)                                         */
/* ------------------------------------------------------------------ */

/* Make the Vulkan context current for the given window. */
int wpc_vk_make_current(WpcWindow *window);

/* Begin a frame: 1 ready, 0 skipped during resize, -1 on renderer failure. */
int wpc_vk_begin_frame(WpcWindow *window);

/* Grow frame vertex storage outside a begun frame; 1 success, 0 failure. */
int wpc_vk_reserve_vertices(WpcWindow *window, uint32_t count);

/* Clip subsequent draws to a client-pixel rectangle. Reset at begin_frame. */
void wpc_vk_set_clip_rect(WpcWindow *window, int x, int y, int w, int h);

/* Clear the screen with the given RGB color (0..1). */
void wpc_vk_clear(WpcWindow *window, float r, float g, float b);

/* Draw a filled rectangle in screen-space pixels. */
void wpc_vk_draw_rect(WpcWindow *window,
                      int x, int y, int w, int h,
                      float r, float g, float b, float a);

/* LUMINOSITE 0..100 (50 neutral): end_frame applies the original's linear
 * DirectDraw gamma ramp (0x4abf50) over the whole frame. */
void wpc_vk_set_brightness(WpcWindow *window, int value);

/* End the current frame (end render pass, prepare for present). */
int wpc_vk_end_frame(WpcWindow *window);

/* RGBA8 texture; create/destroy outside a begun frame. Owned by the window. */
typedef struct WpcTexture WpcTexture;
WpcTexture *wpc_vk_texture_create(WpcWindow *window, uint32_t width,
                                 uint32_t height, const uint8_t *rgba);
void wpc_vk_texture_destroy(WpcWindow *window, WpcTexture *texture);
/* Replace all pixels of an existing texture (same size), outside frame recording. */
int wpc_vk_texture_update(WpcWindow *window, WpcTexture *texture, const uint8_t *rgba);

/* Draw a textured rectangle with straight-alpha blending. */
void wpc_vk_draw_texture(WpcWindow *window,
                         int x, int y, int w, int h,
                         void *texture_id);
void wpc_vk_draw_texture_tinted(WpcWindow *window, int x, int y, int w, int h,
                                WpcTexture *texture, float r, float g, float b, float a);

/* ------------------------------------------------------------------ */
/* 3D rendering (Vulkan pipeline)                                     */
/* ------------------------------------------------------------------ */

/* Set of transformed vertices for 3D rendering. */
typedef struct {
    float x, y, z;   /* position */
    float r, g, b;   /* color */
} WpcVertex3D;

/* Push constant block: 4x4 column-major model-view-projection matrix. */
typedef struct {
    float mvp[16];
} WpcPushConstants3D;

/* Submit a batch of 3D vertices for rendering (line list). */
void wpc_vk_draw_3d(WpcWindow *window, const WpcVertex3D *vertices,
                    uint32_t vertex_count);

/* Set the model-view-projection matrix (column-major, 16 floats). */
void wpc_vk_set_mvp(WpcWindow *window, const float mvp[16]);
/* Linear fog for material triangles, by view depth (clip w); end<=start disables. */
void wpc_vk_set_fog(WpcWindow *window, float r, float g, float b, float start, float end);

typedef struct {
    float x, y, z, u, v, r, g, b, a;
} WpcTexturedVertex3D;
/* Triangle list with perspective-correct UVs and depth testing/writes. */
void wpc_vk_draw_textured_triangles(WpcWindow *window, WpcTexture *texture,
                                    const WpcTexturedVertex3D *vertices, uint32_t count);
/* Blend/depth state for later material triangles (WPC 0x4b3630): additive is
 * ONE/ONE (draw flag 0x8000) instead of SRCALPHA/INVSRCALPHA; no depth write
 * is draw flag 0x2000000. Depth testing stays on. */
#define WPC_DRAW_ADDITIVE 2u
#define WPC_DRAW_NO_DEPTH_WRITE 4u
/* 0x4b5550's mode bit 0x2 (a film frame whose material has SILL and SMOO, mode
 * 10): INVSRCALPHA/INVSRCCOLOR, dst x (1 - src) + src x (1 - alpha). */
#define WPC_DRAW_INVERSE 8u
void wpc_vk_set_draw_mode(WpcWindow *window, uint32_t mode);
/* Original CLAM bits: bit 0 clamps U, bit 1 clamps V; cleared bits repeat.
 * two_sided: when false, back-face culling is enabled for this draw. */
void wpc_vk_draw_material_triangles(WpcWindow *window, WpcTexture *texture,
                                   const WpcTexturedVertex3D *vertices, uint32_t count, uint32_t clamp_axes, int two_sided);

/* ------------------------------------------------------------------ */
/* Input                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    WPC_KEY_UNKNOWN = 0,
    WPC_KEY_ESCAPE,
    WPC_KEY_RETURN,
    WPC_KEY_UP,
    WPC_KEY_DOWN,
    WPC_KEY_LEFT,
    WPC_KEY_RIGHT,
    WPC_KEY_SPACE,
    WPC_KEY_CONTROL,
    WPC_KEY_COUNT
} WpcKey;

/* Ask the window manager for full screen (_NET_WM_STATE_FULLSCREEN); the
   swapchain follows the new size. */
void wpc_window_set_fullscreen(WpcWindow *window, int on);

/* Query whether a key is currently held down. */
int wpc_input_key_down(WpcWindow *window, WpcKey key);
/* Linux input codes (X keycode - 8, the PC set-1 scan code below 0x59):
   whether one is held, and the last one pressed (0 once read). */
int wpc_input_code_down(WpcWindow *window, int code);
int wpc_input_last_code(WpcWindow *window);
/* Drain the text typed since the last call (Latin-1 bytes from KeyPress,
   '\b' for BackSpace); returns the bytes written. */
size_t wpc_input_text(WpcWindow *window, char *out, size_t capacity);

/* ------------------------------------------------------------------ */
/* Time                                                               */
/* ------------------------------------------------------------------ */

/* Seconds since an arbitrary point, as a double. */
double wpc_time_now(void);

/* Sleep for approximately `milliseconds`. */
void wpc_sleep_ms(uint32_t milliseconds);

/* ------------------------------------------------------------------ */
/* File I/O                                                           */
/* ------------------------------------------------------------------ */

/* Open a game data file for reading. Returns NULL on failure. */
void *wpc_file_open(const char *path, const char *mode);

/* Read up to `size` bytes into `buf`. Returns bytes read. */
size_t wpc_file_read(void *file, void *buf, size_t size);

/* Close a file opened with wpc_file_open. */
void wpc_file_close(void *file);

/* ------------------------------------------------------------------ */
/* Logging                                                            */
/* ------------------------------------------------------------------ */

void wpc_log(const char *format, ...)
    __attribute__((format (printf, 1, 2)));

void wpc_log_error(const char *format, ...)
    __attribute__((format (printf, 1, 2)));

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* WPC_PLATFORM_H */
