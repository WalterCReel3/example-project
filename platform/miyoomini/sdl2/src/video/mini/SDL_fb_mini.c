// LGPL-2.1 License
// (C) 2025 Steward Fu <steward.fu@gmail.com>

#include "../../SDL_internal.h"

#if SDL_VIDEO_DRIVER_MINI

#include "../SDL_sysvideo.h"

#include "SDL_video_mini.h"
#include "SDL_fb_mini.h"

#define MINI_SURFACE "SDL_MiniSurface"

extern int FB_W;
extern int FB_H;
extern int TMP_SIZE;

int Mini_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    int w = 0;
    int h = 0;
    int bpp = 0;
    uint32_t Rmask = 0;
    uint32_t Gmask = 0;
    uint32_t Bmask = 0;
    uint32_t Amask = 0;
    SDL_Surface *surface = NULL;
    const uint32_t surface_format = SDL_PIXELFORMAT_RGB888;

    debug("%s\n", __func__);
    surface = (SDL_Surface *) SDL_GetWindowData(window, MINI_SURFACE);
    SDL_FreeSurface(surface);

    SDL_PixelFormatEnumToMasks(surface_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask);
    SDL_GetWindowSize(window, &w, &h);
    surface = SDL_CreateRGBSurface(0, w, h, bpp, Rmask, Gmask, Bmask, Amask);
    if (!surface) {
        debug("%s, failed to create window surface\n", __func__);
        return -1;
    }

    SDL_SetWindowData(window, MINI_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return 0;
}

int Mini_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_Surface *surface = (SDL_Surface *) SDL_GetWindowData(window, MINI_SURFACE);
    SDL_Rect src = {0};
    SDL_Rect dst = {0};
    int c0 = 0;
    int c1 = 0;
    int scale = 0;

    debug("%s\n", __func__);
    if (!surface) {
        return SDL_SetError("no window surface to present");
    }

    /* GFX_Copy stages through gfx.tmp, which holds one panel's worth of
       pixels. A window sized from this driver's mode list can be larger than
       the panel — 800x600 is in the list and 640x480 is the panel — so a
       surface that does not fit is ordinary rather than exceptional, and
       staging it would run off the end of the DMA buffer. */
    if ((surface->h * surface->pitch) > TMP_SIZE) {
        return SDL_SetError("window surface %dx%d exceeds the %dx%d staging buffer",
                            surface->w, surface->h, FB_W, FB_H);
    }

    c0 = FB_W / surface->w;
    c1 = FB_H / surface->h;
    scale = c0 > c1 ? c1 : c0;
    if (scale < 1) {
        return SDL_SetError("window %dx%d is wider or taller than the %dx%d panel",
                            surface->w, surface->h, FB_W, FB_H);
    }

    /* The dirty rects are deliberately ignored. GFX_Flip pans between two
       halves of the framebuffer, so the half this update lands in holds the
       frame from two presents ago; anything outside the dirty region would be
       stale rather than merely unchanged. */
    src.w = surface->w;
    src.h = surface->h;

    dst.w = surface->w * scale;
    dst.h = surface->h * scale;

    /* Centred, which needs no mirror term: the panel is mounted inverted and
       GFX_Copy compensates with a 180-degree rotation, and that maps a centred
       rect onto itself. A rect placed anywhere else has to be mirrored in both
       axes, which is what Mini_QueueCopy does. */
    dst.x = (FB_W - dst.w) / 2;
    dst.y = (FB_H - dst.h) / 2;

    GFX_Copy(surface->pixels, src, dst, surface->pitch, surface->format->format,
             E_MI_GFX_ROTATE_180);
    GFX_Flip();
    return 0;
}

void Mini_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_Surface *surface = (SDL_Surface *) SDL_SetWindowData(window, MINI_SURFACE, NULL);

    debug("%s\n", __func__);
    SDL_FreeSurface(surface);
}

#endif

