// Integration test: verifies actual presented X11 pixels, including resize.
#include "wpc_platform.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

static Window find(Display *d) {
    Window root, parent, *children = nullptr; unsigned count = 0;
    XQueryTree(d, DefaultRootWindow(d), &root, &parent, &children, &count);
    Window result = 0;
    for (unsigned i = 0; i < count; ++i) {
        char *name = nullptr; XFetchName(d, children[i], &name);
        if (name && std::string(name) == "WPC render test") result = children[i];
        if (name) XFree(name);
    }
    if (children) XFree(children);
    return result;
}
static unsigned component(unsigned long pixel, unsigned long mask) {
    while (mask && !(mask & 1)) { mask >>= 1; pixel >>= 1; }
    return mask ? unsigned((pixel & mask) * 255 / mask) : 0;
}
static void pixel(XImage *image, int x, int y, int r, int g, int b) {
    auto p = XGetPixel(image,x,y);
    int actual[] = {int(component(p,image->red_mask)),int(component(p,image->green_mask)),int(component(p,image->blue_mask))};
    int expected[] = {r,g,b};
    for (int i=0;i<3;++i) if (abs(actual[i]-expected[i])>2) {
        fprintf(stderr,"Pixel (%d,%d): got %d,%d,%d expected %d,%d,%d\n",x,y,actual[0],actual[1],actual[2],r,g,b);
        throw std::runtime_error("Presented image mismatch");
    }
}
int main() {
    WpcWindow *w = wpc_window_create("WPC render test",320,240);
    if (!w) return 1;
    Display *d = XOpenDisplay(nullptr);
    try {
        if (!d) throw std::runtime_error("Test XOpenDisplay failed");
        const uint8_t rgba[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,128};
        WpcTexture *texture = wpc_vk_texture_create(w,2,2,rgba);
        if (!texture) throw std::runtime_error("Texture creation failed");
        Window native = find(d);
        if (!native) throw std::runtime_error("Test window not found");
        for (int phase = 0; phase < 2; ++phase) {
            if (phase) { XResizeWindow(d,native,480,360); XSync(d,False); }
            int width=0,height=0;
            for (int i=0;i<30;++i) {
                if (wpc_window_poll_events(w)) throw std::runtime_error("Renderer closed unexpectedly");
                wpc_window_get_size(w,&width,&height);
                int status=wpc_vk_begin_frame(w);
                if (status<0) throw std::runtime_error("Frame start failed");
                if (status>0) {
                    wpc_vk_clear(w,0,0,0);
                    wpc_vk_draw_texture(w,0,0,width,height,texture);
                    wpc_vk_draw_rect(w,0,0,20,20,1,1,0,1);
                    wpc_vk_draw_rect(w,width-20,0,20,20,0,1,1,1);
                    wpc_vk_set_clip_rect(w,30,30,10,10);
                    wpc_vk_draw_rect(w,0,0,width,height,1,0,1,1);
                    if (!wpc_vk_end_frame(w)) throw std::runtime_error("Frame submission failed");
                }
                wpc_sleep_ms(5);
            }
            if (width != (phase?480:320) || height != (phase?360:240)) throw std::runtime_error("Resize not applied");
            XSync(d,False);
            XImage *capture=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
            if (!capture) throw std::runtime_error("No presented image");
            pixel(capture,5,5,255,255,0); pixel(capture,width-5,5,0,255,255);
            pixel(capture,35,35,255,0,255); pixel(capture,45,35,255,0,0);
            pixel(capture,width/4,height/4,255,0,0); pixel(capture,3*width/4,height/4,0,255,0);
            pixel(capture,width/4,3*height/4,0,0,255); pixel(capture,3*width/4,3*height/4,128,128,128);
            XDestroyImage(capture);
        }
        const uint8_t white_pixel[]={255,255,255,255},zero_pixel[]={255,255,255,0};
        WpcTexture *white=wpc_vk_texture_create(w,1,1,white_pixel);
        WpcTexture *zero=wpc_vk_texture_create(w,1,1,zero_pixel);
        if (!white || !zero) throw std::runtime_error("3D test texture upload failed");
        int width,height; wpc_window_get_size(w,&width,&height);
        WpcTexturedVertex3D triangle[]={{-1,-1,0,0,0,1,1,1,1},{3,-1,0,0,0,1,1,1,1},{-1,3,0,0,0,1,1,1,1}};
        for (int i=0;i<5;++i) {
            if (wpc_vk_begin_frame(w)!=1) throw std::runtime_error("3D test frame not ready");
            wpc_vk_clear(w,0,0,0);
            // An alpha-zero foreground must not write depth; a far blue
            // triangle drawn after the near red triangle must not replace it.
            wpc_vk_draw_textured_triangles(w,zero,triangle,3);
            for (auto &v:triangle) { v.z=.25f; v.r=1; v.g=0; v.b=0; }
            wpc_vk_draw_textured_triangles(w,white,triangle,3);
            for (auto &v:triangle) { v.z=.75f; v.r=0; v.g=0; v.b=1; }
            wpc_vk_draw_textured_triangles(w,white,triangle,3);
            for (auto &v:triangle) { v.z=0; v.r=v.g=v.b=1; }
            wpc_vk_draw_rect(w,0,0,20,20,1,1,0,1);
            if (!wpc_vk_end_frame(w)) throw std::runtime_error("3D test submission failed");
            wpc_sleep_ms(10);
        }
        XSync(d,False);
        XImage *depth_capture=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
        if (!depth_capture) throw std::runtime_error("3D test capture failed");
        pixel(depth_capture,width/2,height/2,255,0,0); pixel(depth_capture,5,5,255,255,0);
        XDestroyImage(depth_capture);
        // WPC draw flags (0x4b3630): 0x8000 blends ONE/ONE whatever the alpha;
        // 0x2000000 keeps the depth test but writes no depth.
        const float identity[]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const auto mode_frame=[&](uint32_t mode,float r0,float g0,float b0,float a0,float z0,float r1,float g1,float b1,float z1) {
            for (int i=0;i<5;++i) {
                if (wpc_vk_begin_frame(w)!=1) throw std::runtime_error("Draw-mode frame not ready");
                wpc_vk_clear(w,0,0,0);wpc_vk_set_mvp(w,identity);
                WpcTexturedVertex3D t[]={{-1,-1,z0,0,0,r0,g0,b0,a0},{3,-1,z0,0,0,r0,g0,b0,a0},{-1,3,z0,0,0,r0,g0,b0,a0}};
                wpc_vk_draw_material_triangles(w,white,t,3,3,1);
                wpc_vk_set_draw_mode(w,mode);
                for (auto &v:t) {v.z=z1;v.r=r1;v.g=g1;v.b=b1;v.a=.5f;}
                wpc_vk_draw_material_triangles(w,white,t,3,3,1);
                wpc_vk_set_draw_mode(w,0);
                if (!wpc_vk_end_frame(w)) throw std::runtime_error("Draw-mode submission failed");
                wpc_sleep_ms(10);
            }
            XSync(d,False);
            XImage *capture=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
            if (!capture) throw std::runtime_error("Draw-mode capture failed");
            return capture;
        };
        XImage *additive=mode_frame(WPC_DRAW_ADDITIVE,1,0,0,1,.5f,0,0,1,.25f);
        pixel(additive,width/2,height/2,255,0,255);XDestroyImage(additive);
        // A no-depth-write layer at .25 must not hide a later one at .5.
        XImage *no_depth=nullptr;
        for (int i=0;i<5;++i) {
            if (wpc_vk_begin_frame(w)!=1) throw std::runtime_error("Depth-write frame not ready");
            wpc_vk_clear(w,0,0,0);wpc_vk_set_mvp(w,identity);
            WpcTexturedVertex3D t[]={{-1,-1,.25f,0,0,0,1,0,1},{3,-1,.25f,0,0,0,1,0,1},{-1,3,.25f,0,0,0,1,0,1}};
            wpc_vk_set_draw_mode(w,WPC_DRAW_NO_DEPTH_WRITE);
            wpc_vk_draw_material_triangles(w,white,t,3,3,1);
            wpc_vk_set_draw_mode(w,0);
            for (auto &v:t) {v.z=.5f;v.g=0;v.b=1;}
            wpc_vk_draw_material_triangles(w,white,t,3,3,1);
            if (!wpc_vk_end_frame(w)) throw std::runtime_error("Depth-write submission failed");
            wpc_sleep_ms(10);
        }
        XSync(d,False);
        no_depth=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
        if (!no_depth) throw std::runtime_error("Depth-write capture failed");
        pixel(no_depth,width/2,height/2,0,0,255);XDestroyImage(no_depth);
        wpc_vk_texture_destroy(w,white); wpc_vk_texture_destroy(w,zero);
        for (int i=0;i<5;++i) {
            if (wpc_vk_begin_frame(w)!=1) throw std::runtime_error("Wrap test frame not ready");
            wpc_vk_clear(w,0,0,0);
            for (uint32_t mask=0;mask<4;++mask) {
                float x0=mask&1 ? 0.f : -1.f, y0=mask&2 ? 0.f : -1.f;
                float x1=x0+1,y1=y0+1;
                // Out-of-range UVs distinguish all four independent U/V
                // addressing combinations on the same shared GPU image.
                WpcTexturedVertex3D quad[]={
                    {x0,y0,.5f,1.25f,1.25f,1,1,1,1},{x1,y0,.5f,1.25f,1.25f,1,1,1,1},{x1,y1,.5f,1.25f,1.25f,1,1,1,1},
                    {x0,y0,.5f,1.25f,1.25f,1,1,1,1},{x1,y1,.5f,1.25f,1.25f,1,1,1,1},{x0,y1,.5f,1.25f,1.25f,1,1,1,1}};
                wpc_vk_draw_material_triangles(w,texture,quad,6,mask,1);
            }
            if (!wpc_vk_end_frame(w)) throw std::runtime_error("Wrap test submission failed");
            wpc_sleep_ms(10);
        }
        XSync(d,False);
        XImage *wrap_capture=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
        if (!wrap_capture) throw std::runtime_error("Wrap test capture failed");
        pixel(wrap_capture,width/4,height/4,255,0,0); pixel(wrap_capture,3*width/4,height/4,0,255,0);
        pixel(wrap_capture,width/4,3*height/4,0,0,255); pixel(wrap_capture,3*width/4,3*height/4,128,128,128);
        XDestroyImage(wrap_capture);
        // Grow while the preceding submission may still be in flight. Submit
        // more than the old 4 MiB limit and check a final visible triangle.
        if(!wpc_vk_reserve_vertices(w,150003) || wpc_vk_reserve_vertices(w,UINT32_MAX))
            throw std::runtime_error("Vertex storage growth/bounds failed");
        std::vector<WpcTexturedVertex3D> large(150000,{2,2,.5f,0,0,1,1,1,1});
        large.insert(large.end(),{{-1,-1,.25f,0,0,1,1,1,1},{3,-1,.25f,0,0,1,1,1,1},{-1,3,.25f,0,0,1,1,1,1}});
        if(wpc_vk_begin_frame(w)!=1 || wpc_vk_reserve_vertices(w,200000))
            throw std::runtime_error("In-frame vertex growth was accepted");
        wpc_vk_clear(w,0,0,0);
        wpc_vk_draw_material_triangles(w,texture,large.data(),uint32_t(large.size()),3,1);
        if(!wpc_vk_end_frame(w)) throw std::runtime_error("Large draw submission failed");
        wpc_sleep_ms(30);XSync(d,False);
        XImage* large_capture=XGetImage(d,native,0,0,width,height,AllPlanes,ZPixmap);
        if(!large_capture) throw std::runtime_error("Large draw capture failed");
        pixel(large_capture,width/2,height/2,255,0,0);XDestroyImage(large_capture);
        XEvent key_event{};
        key_event.xkey.type=KeyPress; key_event.xkey.display=d; key_event.xkey.window=native;
        key_event.xkey.keycode=XKeysymToKeycode(d,XK_Down);
        XSendEvent(d,native,False,KeyPressMask,&key_event); XSync(d,False);
        wpc_window_poll_events(w);
        if (!wpc_input_key_down(w,WPC_KEY_DOWN)) throw std::runtime_error("Arrow key press was lost");
        key_event.xkey.type=KeyRelease;
        XSendEvent(d,native,False,KeyReleaseMask,&key_event); XSync(d,False);
        wpc_window_poll_events(w);
        if (wpc_input_key_down(w,WPC_KEY_DOWN)) throw std::runtime_error("Arrow key release was lost");
        key_event.xkey.type=KeyPress;
        XSendEvent(d,native,False,KeyPressMask,&key_event); XSync(d,False);
        wpc_window_poll_events(w);
        XEvent focus{}; focus.xfocus.type=FocusOut; focus.xfocus.display=d; focus.xfocus.window=native;
        XSendEvent(d,native,False,FocusChangeMask,&focus); XSync(d,False);
        wpc_window_poll_events(w);
        if (wpc_input_key_down(w,WPC_KEY_DOWN)) throw std::runtime_error("Focus loss left a stuck key");
        wpc_vk_texture_destroy(w,texture);
        XEvent close{}; close.xclient.type=ClientMessage; close.xclient.display=d; close.xclient.window=native;
        close.xclient.message_type=XInternAtom(d,"WM_PROTOCOLS",False); close.xclient.format=32;
        close.xclient.data.l[0]=long(XInternAtom(d,"WM_DELETE_WINDOW",False));
        XSendEvent(d,native,False,NoEventMask,&close); XSync(d,False);
        if (!wpc_window_poll_events(w)) throw std::runtime_error("Window close event was lost");
        wpc_window_destroy(w); XCloseDisplay(d);
        puts("Native renderer: pixels, alpha, clipping, multiple draws, 71 frames, resize, textured 3D, depth, alpha-zero discard, additive and no-depth-write modes, U/V addressing, vertex storage growth, input and close passed");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr,"%s\n",error.what());
        wpc_window_destroy(w); if (d) XCloseDisplay(d); return 1;
    }
}
