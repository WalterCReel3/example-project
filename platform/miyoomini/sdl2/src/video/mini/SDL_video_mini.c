// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MINI

#include <time.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>

#include "../../events/SDL_events_c.h"
#include "../SDL_sysvideo.h"
#include "../SDL_pixels_c.h"

#include "SDL_version.h"
#include "SDL_syswm.h"
#include "SDL_loadso.h"
#include "SDL_events.h"
#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_video_mini.h"
#include "SDL_event_mini.h"
#include "SDL_fb_mini.h"

GFX gfx = { 0 };

int FB_W = 0;
int FB_H = 0;
int FB_SIZE = 0;
int TMP_SIZE = 0;
SDL_Window *vid_win = NULL;

static int Mini_VideoInit(_THIS);
static int Mini_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);
static void Mini_VideoQuit(_THIS);

int Mini_InitGFX(void)
{
    debug("%s\n", __func__);
    MI_SYS_Init();
    MI_GFX_Open();

    gfx.fb_dev = open("/dev/fb0", O_RDWR);
    ioctl(gfx.fb_dev, FBIOGET_FSCREENINFO, &gfx.finfo);
    ioctl(gfx.fb_dev, FBIOGET_VSCREENINFO, &gfx.vinfo);
    gfx.vinfo.yoffset = 0;
    gfx.vinfo.yres_virtual = gfx.vinfo.yres * 2;
    ioctl(gfx.fb_dev, FBIOPUT_VSCREENINFO, &gfx.vinfo);

    gfx.fb.phyAddr = gfx.finfo.smem_start;
    MI_SYS_MemsetPa(gfx.fb.phyAddr, 0, FB_SIZE);
    MI_SYS_Mmap(gfx.fb.phyAddr, gfx.finfo.smem_len, &gfx.fb.virAddr, TRUE);
    memset(&gfx.hw.opt, 0, sizeof(gfx.hw.opt));

    MI_SYS_MMA_Alloc(NULL, TMP_SIZE, &gfx.tmp.phyAddr);
    MI_SYS_Mmap(gfx.tmp.phyAddr, TMP_SIZE, &gfx.tmp.virAddr, TRUE);

    MI_SYS_MMA_Alloc(NULL, TMP_SIZE, &gfx.overlay.phyAddr);
    MI_SYS_Mmap(gfx.overlay.phyAddr, TMP_SIZE, &gfx.overlay.virAddr, TRUE);
    return 0;
}

int Mini_QuitGFX(void)
{
    debug("%s\n", __func__);
    MI_SYS_Munmap(gfx.fb.virAddr, TMP_SIZE);
    MI_SYS_Munmap(gfx.tmp.virAddr, TMP_SIZE);
    MI_SYS_MMA_Free(gfx.tmp.phyAddr);
    MI_SYS_Munmap(gfx.overlay.virAddr, TMP_SIZE);
    MI_SYS_MMA_Free(gfx.overlay.phyAddr);

    MI_GFX_Close();
    MI_SYS_Exit();
    return 0;
}

void GFX_Init(void)
{
    debug("%s\n", __func__);
    Mini_InitGFX();
}

void GFX_Quit(void)
{
    debug("%s\n", __func__);

    GFX_Clear();
    Mini_QuitGFX();
    gfx.vinfo.yoffset = 0;
    ioctl(gfx.fb_dev, FBIOPUT_VSCREENINFO, &gfx.vinfo);
    close(gfx.fb_dev);
    gfx.fb_dev = 0;
}

void GFX_Clear(void)
{
    debug("%s\n", __func__);
    MI_SYS_MemsetPa(gfx.fb.phyAddr, 0, FB_SIZE);
    MI_SYS_MemsetPa(gfx.tmp.phyAddr, 0, TMP_SIZE);
}

/* The blitter's format, from SDL's. Only the formats this driver advertises,
   plus the RGB888 the window framebuffer uses — SDL never hands the backend
   anything else, because the frontend converts through a native texture first. */
static MI_GFX_ColorFmt_e mi_color_format(uint32_t sdl_format)
{
    switch (sdl_format) {
    case SDL_PIXELFORMAT_RGB565:
        return E_MI_GFX_FMT_RGB565;
    case SDL_PIXELFORMAT_ABGR8888:
        return E_MI_GFX_FMT_ABGR8888;
    case SDL_PIXELFORMAT_RGB888:
        /* XRGB8888: the alpha byte is unused rather than absent, and every
           blend that reads it is off for this surface. */
    case SDL_PIXELFORMAT_ARGB8888:
    default:
        return E_MI_GFX_FMT_ARGB8888;
    }
}

/* SDL's blend modes onto MI_GFX's DirectFB-derived operands.
 *
 * All four are mandatory: SDL_render.c's IsSupportedBlendMode returns true for
 * BLEND, ADD, MOD and MUL without consulting the backend, so a driver cannot
 * refuse one — it can only apply it or lie about having done so.
 *
 * NONE is what this function used to hardcode: BLD_ONE with BLD_ZERO is
 * src*1 + dst*0. The path gfx::renderer::Layer takes therefore produces the
 * same operands as before, which is what makes an unchanged device run the
 * check on this change.
 */
static void mi_blend(MI_GFX_Opt_t *opt, SDL_BlendMode blend)
{
    opt->eDFBBlendFlag = E_MI_GFX_DFB_BLEND_NOFX;

    switch (blend) {
    case SDL_BLENDMODE_BLEND:
        opt->eSrcDfbBldOp = E_MI_GFX_DFB_BLD_SRCALPHA;
        opt->eDstDfbBldOp = E_MI_GFX_DFB_BLD_INVSRCALPHA;
        opt->eDFBBlendFlag = E_MI_GFX_DFB_BLEND_ALPHACHANNEL;
        break;
    case SDL_BLENDMODE_ADD:
        opt->eSrcDfbBldOp = E_MI_GFX_DFB_BLD_SRCALPHA;
        opt->eDstDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
        opt->eDFBBlendFlag = E_MI_GFX_DFB_BLEND_ALPHACHANNEL;
        break;
    case SDL_BLENDMODE_MOD:
        opt->eSrcDfbBldOp = E_MI_GFX_DFB_BLD_DESTCOLOR;
        opt->eDstDfbBldOp = E_MI_GFX_DFB_BLD_ZERO;
        break;
    case SDL_BLENDMODE_MUL:
        opt->eSrcDfbBldOp = E_MI_GFX_DFB_BLD_DESTCOLOR;
        opt->eDstDfbBldOp = E_MI_GFX_DFB_BLD_INVSRCALPHA;
        opt->eDFBBlendFlag = E_MI_GFX_DFB_BLEND_ALPHACHANNEL;
        break;
    case SDL_BLENDMODE_NONE:
    default:
        opt->eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
        opt->eDstDfbBldOp = E_MI_GFX_DFB_BLD_ZERO;
        break;
    }
}

int GFX_Copy(const void *pixels, SDL_Rect src_rt, SDL_Rect dst_rt, int pitch, uint32_t format, SDL_BlendMode blend, int rotate)
{
    MI_U16 u16Fence = 0;
    const int bpp = SDL_BYTESPERPIXEL(format);
    const int staged = src_rt.h * pitch;

    debug("%s, pixels=%p, format=%08x\n", __func__, pixels, format);

    if ((staged <= 0) || (staged > TMP_SIZE)) {
        debug("%s, %d bytes will not fit the %d byte staging buffer\n",
            __func__, staged, TMP_SIZE);
        return -1;
    }

    /* From the rect's first row, not the texture's. The blitter is told to read
       from row 0 of what was staged, so the two agree — copying from the top
       while reading from src_rt.y is what left an atlas showing the previous
       blit's leftovers. */
    memcpy(gfx.tmp.virAddr, (const uint8_t *)pixels + (src_rt.y * pitch), staged);

    gfx.hw.opt.u32GlobalSrcConstColor = 0;
    gfx.hw.opt.eRotate = rotate;
    mi_blend(&gfx.hw.opt, blend);

    /* The staged surface is as wide as the source texture and as tall as the
       rect, so x offsets still index into it and y has already been consumed by
       the copy above. Deriving the width from the pitch rather than from the
       rect is what makes a sub-rectangle narrower than its texture work. */
    gfx.hw.src.rt.s32Xpos = src_rt.x;
    gfx.hw.src.rt.s32Ypos = 0;
    gfx.hw.src.rt.u32Width = src_rt.w;
    gfx.hw.src.rt.u32Height = src_rt.h;
    gfx.hw.src.surf.u32Width = bpp ? (uint32_t)(pitch / bpp) : (uint32_t)src_rt.w;
    gfx.hw.src.surf.u32Height = src_rt.h;
    gfx.hw.src.surf.u32Stride = pitch;
    gfx.hw.src.surf.eColorFmt = mi_color_format(format);
    gfx.hw.src.surf.phyAddr = gfx.tmp.phyAddr;

    gfx.hw.dst.rt.s32Xpos = dst_rt.x;
    gfx.hw.dst.rt.s32Ypos = dst_rt.y;
    gfx.hw.dst.rt.u32Width = dst_rt.w;
    gfx.hw.dst.rt.u32Height = dst_rt.h;
    gfx.hw.dst.surf.u32Width = FB_W;
    gfx.hw.dst.surf.u32Height = FB_H;
    gfx.hw.dst.surf.u32Stride = FB_W * FB_BPP;
    gfx.hw.dst.surf.eColorFmt = E_MI_GFX_FMT_ARGB8888;
    gfx.hw.dst.surf.phyAddr = gfx.fb.phyAddr + (FB_W * gfx.vinfo.yoffset * FB_BPP);

    MI_SYS_FlushInvCache(gfx.tmp.virAddr, pitch * src_rt.h);
    MI_GFX_BitBlit(&gfx.hw.src.surf, &gfx.hw.src.rt, &gfx.hw.dst.surf, &gfx.hw.dst.rt, &gfx.hw.opt, &u16Fence);
    MI_GFX_WaitAllDone(TRUE, u16Fence);
    return 0;
}

void GFX_Flip(void)
{
    debug("%s\n", __func__);
    ioctl(gfx.fb_dev, FBIOPAN_DISPLAY, &gfx.vinfo);
    gfx.vinfo.yoffset ^= FB_H;
}

static void Mini_DeleteDevice(SDL_VideoDevice *device)
{
    debug("%s\n", __func__);
    SDL_free(device);
}

void Mini_DestroyWindow(_THIS, SDL_Window *window)
{
    debug("%s\n", __func__);
    GFX_Quit();
}

int Mini_CreateWindow(_THIS, SDL_Window *window)
{
    vid_win = window;
    SDL_SetMouseFocus(window);
    debug("%s, win=%p, w=%d, h=%d\n", __func__, window, window->w, window->h);
    return 0;
}

int Mini_CreateWindowFrom(_THIS, SDL_Window *window, const void *data)
{
    debug("%s\n", __func__);
    return SDL_Unsupported();
}

/* SDL 2.24 dropped VideoBootStrap's devindex: a backend that registers one
   device never used it, and no backend registered more than one. */
static SDL_VideoDevice *Mini_CreateDevice(void)
{
    SDL_VideoDevice *device = NULL;

    debug("%s\n", __func__);

    device = (SDL_VideoDevice *) SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    device->is_dummy = SDL_TRUE;
    device->VideoInit = Mini_VideoInit;
    device->VideoQuit = Mini_VideoQuit;
    device->SetDisplayMode = Mini_SetDisplayMode;
    device->PumpEvents = Mini_PumpEvents;
    device->CreateSDLWindow = Mini_CreateWindow;
    device->CreateSDLWindowFrom = Mini_CreateWindowFrom;
    device->CreateWindowFramebuffer = Mini_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = Mini_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = Mini_DestroyWindowFramebuffer;
    device->DestroyWindow = Mini_DestroyWindow;

    /* No GL entry points are registered. The SSD202D has no 3D block, so the
       only GL this port could offer is SwiftShader through libEGL — 21.8 MB of
       software rasteriser reached by dlopen. Leaving these null makes
       SDL_GL_CreateContext fail with "No OpenGL support in video driver"
       instead of succeeding into a stack that cannot exist. */

    device->free = Mini_DeleteDevice;
    return device;
}

VideoBootStrap Mini_VideoDriver = { "Mini", "Miyoo Mini Video Driver", Mini_CreateDevice };

int Mini_VideoInit(_THIS)
{
    FILE *fd = NULL;
    char buf[MAX_PATH] = {0};
    SDL_DisplayMode mode = {0};
    SDL_VideoDisplay display = {0};

    debug("%s\n", __func__);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 640;
    mode.h = 480;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 640;
    mode.h = 480;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 800;
    mode.h = 480;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 800;
    mode.h = 480;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);
    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 800;
    mode.h = 600;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 800;
    mode.h = 600;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 320;
    mode.h = 240;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 320;
    mode.h = 240;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = 480;
    mode.h = 272;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_ARGB8888;
    mode.w = 480;
    mode.h = 272;
    mode.refresh_rate = 60;
    SDL_AddDisplayMode(&display, &mode);
    SDL_AddVideoDisplay(&display, SDL_FALSE);

    FB_W = DEF_FB_W;
    FB_H = DEF_FB_H;
    FB_SIZE = (FB_W * FB_H * FB_BPP * 2);
    TMP_SIZE = (FB_W * FB_H * FB_BPP);
    fd = popen("fbset | grep \"mode \"", "r");
    if (fd) {
        fgets(buf, sizeof(buf), fd);
        pclose(fd);

        if (strstr(buf, "752")) {
            FB_W = 752;
            FB_H = 560;
            FB_SIZE = (FB_W * FB_H * FB_BPP * 2);
            TMP_SIZE = (FB_W * FB_H * FB_BPP);
        }
    }

    debug("%s, screen=%dx%d\n", __func__, FB_W, FB_H);
    GFX_Init();
    Mini_EventInit();
    return 0;
}

static int Mini_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    debug("%s\n", __func__);
    return 0;
}

void Mini_VideoQuit(_THIS)
{
    debug("%s\n", __func__);
    Mini_EventQuit();
}

#endif

