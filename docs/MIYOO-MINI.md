# The Miyoo Mini platform

What this device is, what its software stack can and cannot do, and why the
engine is shaped the way it is here. Written 2026-07-28 after the first runs on
real hardware, so that the next person does not have to rediscover it — and with
sources, because almost none of this is documented anywhere official.

Everything below is either quoted from source that is linked, or measured on a
Miyoo Mini Plus running OnionOS. Where something is inferred rather than
observed, it says so.

**Companion documents:** [TARGETS.md](TARGETS.md) for the constraints across all
five targets, [planning/2026-07-27-onion-bundle](../planning/2026-07-27-onion-bundle/)
for the bundle and its decisions, and
[planning/2026-07-25-target-validation/results.md](../planning/2026-07-25-target-validation/results.md)
for the raw device output.

---

## 1. The hardware

| | |
|---|---|
| SoC | SigmaStar SSD202D |
| CPU | 2 × Cortex-A7 @ 1.2 GHz, NEON + VFPv4, ARMv7-A hard-float |
| RAM | 128 MB DDR3, shared with the OS |
| GPU | **none** — no 3D block, no GL, no GLES, no EGL |
| 2D | `MI_GFX`, a vendor blitter: copy with scale and 90° rotation |
| Display | fbdev, **no DRM/KMS device** |
| Kernel | 4.9.84 (observed: `Linux (none) 4.9.84 #1133 SMP PREEMPT Fri May 5 21:30:37 PDT 2023 armv7l`) |
| Panels | Mini and Mini Plus 640×480; Mini Flip 752×560 |

The Flip's 752×560 is from the driver's own runtime detection (§4.1), not from a
spec sheet; `docs/TARGETS.md` previously recorded 750×560.

**The toolchain sysroot is glibc 2.28**, and carries the complete MI SDK —
`mi_gfx.h`, `mi_ao.h`, `mi_sys.h` and friends, with both `.so` and `.a`. Anything
written directly against the vendor APIs can be built without extra downloads:

```console
$ ls <sysroot>/usr/lib | grep -c '^libmi_'
12
$ ls <sysroot>/usr/lib | grep -i sdl
libSDL-1.2.so.0   libSDL_image-1.2.so.0   libSDL_mixer-1.2.so.0   libSDL_ttf-2.0.so.0
```

Note the second command: **the sysroot has SDL 1.2 and no SDL2.**

---

## 2. Why upstream SDL2 cannot work here

This is the root of everything else on this page, and it needs no device to
verify.

SDL2 has **no framebuffer video backend**. SDL 1.2 had `fbcon`; it has no SDL2
successor. The 2.32.10 tree offers `kmsdrm`, `x11`, `wayland`, `vivante`,
`raspberry`, `directfb`, `offscreen` and `dummy`
([libsdl-org/SDL, release-2.32.10, src/video/](https://github.com/libsdl-org/SDL/tree/release-2.32.10/src/video)),
and this device has a framebuffer and no DRM node. Building the pinned SDL2 for
this target produces exactly what that implies:

```console
$ grep -E "define SDL_VIDEO_DRIVER_" build/miyoomini/_deps/sdl2-build/include-config-release/SDL2/SDL_config.h
#define SDL_VIDEO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_OFFSCREEN 1
#define SDL_VIDEO_DRIVER_WAYLAND 1
```

No display path, and no ALSA either — audio comes out as `OSS`, `PULSEAUDIO`,
`SNDIO`, `DISK` and `DUMMY`. So `WREEL_USE_SYSTEM_SDL2=ON` is **mandatory** on
this target, against an SDL2 that someone has ported to the vendor APIs.

---

## 3. Which SDL2, and the important finding about all of them

Three builds of `libSDL2-2.0.so.0` for this device have been examined:

| Build | Source | Video driver | Render driver | Notes |
|---|---|---|---|---|
| Onion `parasyte` | ships in OnionOS at `/mnt/SDCARD/.tmp_update/lib/parasyte/` | `mmiyoo` | `software`, plus its own | Mesa/X11/libdrm dependency cascade, own loader and libc |
| steward-fu prebuilt | [steward-fu/sdl2](https://github.com/steward-fu/sdl2) `prebuilt/640x480/` | `mini` | `Miyoo Mini` | Self-contained. **What this project vendors** |
| XK9274 fork | [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo) | `mmiyoo` | `MMIYOO` | Actively maintained; 800px textures, threaded present |

All are based on **SDL 2.0.20**, LGPL-2.1. Bracketed by symbol presence rather
than a version string: `SDL_SoftStretchLinear` (2.0.16) present,
`SDL_RenderGetWindow` (2.0.22) absent.

### 3.1 The finding that matters

**Two independent forks, written years apart, implement the same one operation
and stub the rest.** steward-fu's
[`SDL_render_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/render/mini/SDL_render_mini.c)
and XK9274's
[`SDL_render_mmiyoo.c`](https://github.com/XK9274/sdl2_miyoo/blob/main/sdl2/src/render/mmiyoo/SDL_render_mmiyoo.c)
both read:

```c
QueueDrawPoints  → return 0;      /* no-op */
QueueGeometry    → return 0;      /* no-op */
QueueFillRects   → return 0;      /* no-op */
RenderReadPixels → SDL_Unsupported();
```

Only `QueueCopy` does anything.

**So this is not a quality problem in one person's port.** `MI_GFX` blits a
surface to the framebuffer; it has no primitive for filling a rectangle, drawing
geometry, or reading back. Everyone who ports SDL2 here wraps the one thing
there is. Choosing a different fork buys a larger maximum texture and a threaded
present — not capability.

---

## 4. Anatomy of the vendor stack

Read from
[steward-fu/sdl2](https://github.com/steward-fu/sdl2), which is what this project
ships. XK9274's differs in detail, not in shape.

### 4.1 Video driver — `src/video/mini/`

`Mini_VideoInit` adds ten display modes and then **never sets the display's
desktop mode**:

```c
SDL_VideoDisplay display = {0};      /* desktop_mode and current_mode zeroed */
... SDL_AddDisplayMode(&display, &mode) × 10 ...
SDL_AddVideoDisplay(&display, SDL_FALSE);
```

Consequences, all observed on device:

- `SDL_GetCurrentDisplayMode` returns `0x0 @ 0 Hz`.
- **`SDL_WINDOW_FULLSCREEN_DESKTOP` produces a 0×0 window**, because it sizes
  itself from that zeroed mode. Plain `SDL_WINDOW_FULLSCREEN` works, because it
  sizes from the closest *listed* mode and `SetDisplayMode` is a no-op returning
  success. This is defect D24.
- `SDL_GetRendererOutputSize` then also returns `0x0` **and reports success**
  (D22).

Panel geometry is compile-time in steward-fu's build — `DEF_FB_W`/`DEF_FB_H`,
hence separate `prebuilt/320x240/` and `prebuilt/640x480/` — with a runtime
override for one case only:

```c
fd = popen("fbset | grep \"mode \"", "r");
if (strstr(buf, "752")) { FB_W = 752; FB_H = 560; }   /* the Flip */
```

The device table lists **no** `SetWindowFullscreen`, `SetWindowSize`,
`GetDisplayBounds` or `GetDisplayModes`, and sets `device->is_dummy = SDL_TRUE`.
There is no window manager and no window concept: whatever the window holds is
scaled to the whole panel.

### 4.2 Presentation — a double buffer with a hardware blitter

```c
gfx.vinfo.yres_virtual = gfx.vinfo.yres * 2;                       /* two pages */
FB_SIZE = FB_W * FB_H * FB_BPP * 2;
dst.phyAddr = gfx.fb.phyAddr + (FB_W * gfx.vinfo.yoffset * FB_BPP); /* back page */
MI_GFX_BitBlit(&src.surf, &src.rt, &dst.surf, &dst.rt, &opt, &fence);
MI_GFX_WaitAllDone(TRUE, fence);
ioctl(gfx.fb_dev, FBIOPAN_DISPLAY, &gfx.vinfo);                    /* flip */
gfx.vinfo.yoffset ^= FB_H;
```

This is page-flipped double buffering, with the composite done by hardware and
the flip done by panning the display start — the DOS technique without the
banking, and with a blitter. It is also why `present` costs 11–13 ms on device:
that is the wait for the flip at 60 Hz, not work.

### 4.3 Render backend — one operation, and it is not neutral

```c
dst.x = (vid_win->w - (dstrect->x + dstrect->w)) * scale;   /* x mirrored */
dst.y = dstrect->y * scale;                                 /* y not */
GFX_Copy(pixels, src, dst, pitch, 0, E_MI_GFX_ROTATE_180);  /* always rotated */
```

Every `SDL_RenderCopy` is rotated 180° with its x placement mirrored. A
**full-screen** copy survives this — `dst.x` works out to 0 and a rotated field
of horizontal bars still looks like one. A **sub-rectangle** copy does not: each
glyph and HUD line lands rotated and displaced. That is defect D25, and it is
what "the text is broken" looked like on device.

`max_texture_width/height` is **640×480** — the panel exactly. Any texture wider
or taller simply fails to upload.

### 4.4 SDL's own software renderer is present, and cannot present

It is compiled in and registered (`SW_RenderDriver` is in the binary's symbol
table), and `SDL_CreateRenderer` honours `SDL_HINT_RENDER_DRIVER`, so
`SDL_RENDER_DRIVER=software` selects it successfully. It then draws into a
window surface that is never shown:

```c
int Mini_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    return 0;
}
```

Measured on device: 965 correct frames, a **4 µs** present where the `mini`
backend spends 11–13 ms, and a black panel. **The only route to the display is
`Mini_QueueCopy`.**

---

## 5. Capability summary

| Capability | Status on this device |
|---|---|
| Full-screen texture copy | **works** — the only thing that does |
| Sub-rectangle copy (`Context::draw` with a source rect) | draws, wrongly: rotated and mirrored |
| `SDL_RenderFillRect` | silent no-op |
| Points, lines, geometry | silent no-ops |
| `SDL_RenderCopyEx` (rotation, flip) | silent no-op |
| `SDL_RenderReadPixels` | `SDL_Unsupported` |
| Render-to-texture | documented by the fork as not working |
| Max texture | 640×480 (800×480 on XK9274's) |
| Display metadata | none: no desktop mode, no bounds |
| Keyboard/gamepad | **keyboard only** — see §6.3 |
| Audio | works, once `audioserver` releases MI_AO — see §6.2 |

---

## 6. Firmware: OnionOS

### 6.1 Bundle layout

Apps live at `App/<Name>/` with `config.json` and `launch.sh`; ports use three
parallel trees under `Roms/PORTS/`. Taken from real packages in
[OnionUI/Onion](https://github.com/OnionUI/Onion/tree/main/static/packages/App)
and [OnionUI/Ports-Collection](https://github.com/OnionUI/Ports-Collection), not
from a guide. Native SDL2 ports vendor their runtime in the app's own `lib/` —
Sonic Mania ships `libSDL2-2.0.so.0`, `libEGL.so` and `libjson-c.so.5` that way.

**Do not borrow Onion's `parasyte` SDL2.** Its EGL is Mesa and pulls in gbm,
glapi, X11, xcb and libdrm, plus a matched loader and libc: it is an alternate
userland for binaries built against a newer glibc, not a library.

### 6.2 Audio is single-owner

`MI_AO` is held by Onion's `audioserver`. With it running, opening audio fails
inside the vendor layer before SDL sees it:

```
[MI ERR ]: MI_AO_SetPubAttr[3364]: Dev0 failed to set pub attr!!! error number:0xa0052009!!!
```

The firmware ships the remedy at
`/mnt/SDCARD/.tmp_update/script/stop_audioserver.sh` and its own ports launcher
sources it. **That script expects `$miyoodir`** (`/mnt/SDCARD/miyoo`, set in
Onion's `runtime.sh`); without it, one branch kills `wpa_supplicant` and
`udhcpc` and cannot restart them, taking the network down.

Onion also **preloads `libpadsp.so`**, an audio shim, into launched programs.
Its ports launcher unsets `LD_PRELOAD` before exec'ing; so should anything that
talks to MI_AO directly.

### 6.3 The pad is a keyboard

The video driver reads `/dev/input` and translates Linux `KEY_*` codes straight
to SDL keysyms, so **no joystick is ever enumerated**. The bindings are Onion's
own
[`src/common/system/keymap_sw.h`](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_sw.h):

| A | B | X | Y | L1 | R1 | L2 | R2 | Select | Start | Menu |
|---|---|---|---|---|---|---|---|---|---|---|
| `SPACE` | `LCTRL` | `LSHIFT` | `LALT` | `e` | `t` | `TAB` | `BACKSPACE` | `RCTRL` | `RETURN` | `ESCAPE` |

A program binding letter keys will appear to take the D-pad and Escape and
ignore every face button, because arrows and Escape coincide and nothing else
does.

### 6.4 Other observations

- `HOME` is `/mnt/SDCARD/RetroArch/` when launched from the Apps menu, so
  `SDL_GetPrefPath` lands somewhere surprising unless overridden.
- The clock is not set: logs show 1970 dates.

---

## 7. What this measured

From a Miyoo Mini Plus, `coppers`, 60 Hz vsync-capped:

| | 640×480 layer | 320×240 layer |
|---|---|---|
| plot | 1.05 ms | 0.84 ms |
| blit | 2.51 ms | 4.30–5.37 ms |
| present | 11.2–13.1 ms (vsync wait) | 11.2 ms |
| CPU scroller | — | 493–728 µs |

**Plotting a full-screen field costs about 1 ms of a 16.7 ms frame.** Two
Cortex-A7 cores can compose the whole screen every frame with roughly 5×
headroom. The fill-rate anxiety that motivated much of the planning was
misplaced by an order of magnitude; the binding constraint was never bandwidth,
it was what the driver would accept.

Note the blit column: a **scaled** blit costs more than a 1:1 one even with
MI_GFX doing the scaling, which is the same shape the dev box showed for SDL's
software blitter. These figures were taken with the demo being toggled
interactively, so treat them as indicative until a single-configuration run is
taken.

---

## 8. The engineering conclusion

The only model this device supports is: **compose the whole frame yourself into
one full-screen buffer, and hand it over once.** That is what
`gfx::renderer::Layer` is, it is what the hardware is built around, and it is
comfortable at this resolution.

Consequences carried into the codebase:

- `coppers` detects the driver by name and defaults to CPU composition — the CPU
  scroller and a HUD plotted into the layer.
- `Layer::set_readback()` exists because it is the only way to capture a frame
  where `SDL_RenderReadPixels` is unsupported.
- `Context` probes four sources for its output size and refuses to run with
  none, and picks its fullscreen flag from what the driver can actually answer.
- **`software-2d-sprites-tiling` cannot assume `Context::draw()` here.** `Atlas`,
  `AnimatedSprite` and `TileMap` are source-rect blits by definition; on this
  target they must composite into a layer.

### Alternatives, and why not

| Path | Gets you | Costs |
|---|---|---|
| **SDL2 + layer model** (chosen) | One codebase across five targets and Steam | CPU composition on this target |
| SDL 1.2 | A mature, vendor-supported stack — what most Onion ports use | A different API; forks the codebase for one device; dead upstream; irrelevant to Steam |
| A different SDL2 fork | 800px textures, threaded present | Same stubs (§3.1) |
| Own platform layer (fbdev + MI_GFX + evdev + MI_AO) | Total control, and the SDK is in the sysroot | Reimplements input, audio and present per device — a platform port, not a fix |

Worth noting how close the chosen path already is to the last one: SDL2 is
providing window creation, input translation, audio and timing, all adequately.
What it is not providing here is a renderer — and the engine has stopped needing
one.

---

## 9. Sources

Vendor SDL2 ports:

- [steward-fu/sdl2](https://github.com/steward-fu/sdl2) — the port this project
  vendors. LGPL-2.1, SDL 2.0.20.
  [`SDL_video_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/video/mini/SDL_video_mini.c) ·
  [`SDL_render_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/render/mini/SDL_render_mini.c) ·
  [`SDL_fb_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/video/mini/SDL_fb_mini.c) ·
  [`SDL_event_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/video/mini/SDL_event_mini.c)
- [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo) — actively
  maintained fork; same architecture
- [Luspin/sdl2-MMP](https://github.com/Luspin/sdl2-MMP) — another descendant

Firmware:

- [OnionUI/Onion](https://github.com/OnionUI/Onion) — `static/packages/App/` for
  the app layout, `static/build/.tmp_update/lib/parasyte/` for the runtime set,
  `static/build/.tmp_update/script/stop_audioserver.sh`,
  [`src/common/system/keymap_sw.h`](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_sw.h)
- [OnionUI/Ports-Collection](https://github.com/OnionUI/Ports-Collection) —
  Sonic Mania is the native-SDL2 reference bundle
- [Onion documentation](https://onionui.github.io/docs) — Tweaks, services

Toolchain:

- [shauninman/union-miyoomini-toolchain](https://github.com/shauninman/union-miyoomini-toolchain)
  — GCC 8.3.0, glibc 2.28 sysroot, MI SDK included
- [shauninman/miyoomini-toolchain-buildroot](https://github.com/shauninman/miyoomini-toolchain-buildroot)
  — how that toolchain is built

Upstream:

- [libsdl-org/SDL, release-2.32.10](https://github.com/libsdl-org/SDL/tree/release-2.32.10/src/video)
  — the video backend list, for the absence of a framebuffer driver

Local verification — most claims here can be re-checked without a device:

```sh
# what the pinned SDL2 builds for this target
grep -E "define SDL_VIDEO_DRIVER_" \
  build/miyoomini/_deps/sdl2-build/include-config-release/SDL2/SDL_config.h

# what the vendored runtime exports, and what our binary needs from it
readelf --dyn-syms -W lib/libSDL2-2.0.so.0 | awk '$4=="FUNC" && $7!="UND"{print $8}' | sort -u
nm -D --undefined-only build/gcc83-sdl2/bin/coppers | awk '{print $2}' | grep -E '^(SDL_|IMG_|TTF_|Mix_)'
```
