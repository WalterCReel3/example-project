# The Miyoo Mini platform

What this device is, what its software stack can and cannot do, and why the
engine is shaped the way it is here. Written 2026-07-28 after the first runs on
real hardware, so that the next person does not have to rediscover it — and with
sources, because almost none of this is documented anywhere official.

Everything below is either quoted from source that is linked, or measured on a
Miyoo Mini Plus — running OnionOS for everything dated to 2026-07-28, and on
stock firmware as well from 2026-07-30. Where something is inferred rather than
observed, it says so, and where it holds on only one firmware, §6 says which.

**Revised 2026-07-31, and it is worth reading §3.1 before anything else.** This
document described what `MI_GFX` can do by inferring it from what the SDL2 ports
call. The vendor SDK is in the toolchain sysroot and says something different:
the blitter fills rectangles, blends, colour-keys and rotates per call, and the
ports use almost none of it. §4.6 is new and is read from the header;
§3.1, §4.3, §5 and §8 are corrected where they had it backwards. **The shipping
decision is unchanged** — what changes is that its reason is a measurement rather
than a hardware limit, and that a fork becomes a real option rather than a
pointless one.

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
| 2D | `MI_GFX`, a vendor blitter: copy with scale, 90° rotation, mirror, colour-key, alpha blend and rectangle fill — see §4.6 |
| Display | fbdev, **no DRM/KMS device** |
| Kernel | 4.9.84 (observed: `Linux (none) 4.9.84 #1133 SMP PREEMPT Fri May 5 21:30:37 PDT 2023 armv7l`) |
| Panels | Mini and Mini Plus 640×480; Mini Flip 752×560 |

The Flip's 752×560 is from the driver's own runtime detection (§4.1), not from a
spec sheet. Every document in this repository recorded **750**×560 until
2026-07-31, from the spec sheets; the driver tests for `"752"` and sets 752×560,
and that is the number the panel is actually driven at. Swept repository-wide the
same day — the derived figures did not move, because 750×560 and 752×560 are
0.3% apart and every calculation that used them was quoted to two significant
figures.

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
`SNDIO`, `DISK` and `DUMMY`. **Unmodified upstream SDL2 cannot run here**, and
that has not changed.

What changed on 2026-08-01 is the answer to it. This section used to conclude
that `WREEL_USE_SYSTEM_SDL2=ON` is mandatory — link a library someone else
ported. The project now compiles the vendor drivers into the same pinned
upstream SDL2 every other target gets: `WREEL_MINI_SDL2`, defaulted ON by the
miyoomini toolchain file. The `SDL_config.h` above is what upstream produces
*without* those drivers; with them it also carries `SDL_VIDEO_DRIVER_MINI`,
`SDL_VIDEO_RENDER_MINI` and `SDL_AUDIO_DRIVER_MINI`. See §3 and
[platform/miyoomini/sdl2/](../platform/miyoomini/sdl2/).

---

## 3. Which SDL2, and the important finding about all of them

Three builds of `libSDL2-2.0.so.0` for this device have been examined:

| Build | Source | Video driver | Render driver | Notes |
|---|---|---|---|---|
| Onion `parasyte` | ships in OnionOS at `/mnt/SDCARD/.tmp_update/lib/parasyte/` | `mmiyoo` | `software`, plus its own | Mesa/X11/libdrm dependency cascade, own loader and libc |
| steward-fu prebuilt | [steward-fu/sdl2](https://github.com/steward-fu/sdl2) `prebuilt/640x480/` | `mini` | `Miyoo Mini` | Self-contained. What this project vendored until 2026-08-01 |
| XK9274 fork | [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo) | `mmiyoo` | `MMIYOO` | Actively maintained; 800px textures, threaded present |
| **ours** | pinned upstream **2.32.10** + steward-fu's drivers, [platform/miyoomini/sdl2/](../platform/miyoomini/sdl2/) | `Mini` | `Miyoo Mini` | **What this project ships since 2026-08-01.** Built from source, no EGL, no GLESv2, no json-c |

The first three are **SDL 2.0.20**. Ours is 2.32.10 carrying the same driver
sources, which is why it registers the same names and why `coppers` could not
tell the difference. All four are LGPL-2.1 — the licence comes from steward-fu's
driver files, not from SDL2, which is zlib.

For the three prebuilts, the base version is bracketed by symbol presence rather
than a version string: `SDL_SoftStretchLinear` (2.0.16) present,
`SDL_RenderGetWindow` (2.0.22) absent. Ours answers `SDL_GetVersion` honestly.

### 3.1 The finding that matters, and the reason it is not what it looked like

**Two independent forks, written years apart, implement the same one operation
and stub the rest.** steward-fu's
[`SDL_render_mini.c`](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/render/mini/SDL_render_mini.c)
— 371 lines in total — and XK9274's
[`SDL_render_mmiyoo.c`](https://github.com/XK9274/sdl2_miyoo/blob/main/sdl2/src/render/mmiyoo/SDL_render_mmiyoo.c)
both read:

```c
QueueDrawPoints  → return 0;      /* no-op */
QueueGeometry    → return 0;      /* no-op */
QueueFillRects   → return 0;      /* no-op */
QueueCopyEx      → return 0;      /* no-op */
RenderReadPixels → SDL_Unsupported();
```

Only `QueueCopy` does anything.

**Corrected 2026-07-31.** This section used to explain that by saying `MI_GFX`
"has no primitive for filling a rectangle, drawing geometry, or reading back",
and that "everyone who ports SDL2 here wraps the one thing there is". That was
wrong, in the direction that makes the device look less capable than it is. It
had been inferred from the ports rather than read from the SDK — which is in the
toolchain sysroot and answers the question directly:

```console
$ readelf --dyn-syms -W $SYSROOT/usr/lib/libmi_gfx.so | awk '$7!="UND"{print $8}' \
    | grep ^MI_GFX | sort -u
MI_GFX_BitBlit   MI_GFX_DrawLine                MI_GFX_QuickFill  MI_GFX_SetPalette
MI_GFX_Close     MI_GFX_GetAlphaThresholdValue  MI_GFX_Open       MI_GFX_WaitAllDone
                 MI_GFX_SetAlphaThresholdValue
```

Nine entry points. The library this project ships imports four:

```console
$ readelf --dyn-syms -W lib/libSDL2-2.0.so.0 | awk '$7=="UND"{print $8}' | grep ^MI_GFX
MI_GFX_BitBlit  MI_GFX_Close  MI_GFX_Open  MI_GFX_WaitAllDone
```

**`MI_GFX_QuickFill` is a hardware rectangle fill, and neither port calls it.**
So `QueueFillRects` returning 0 is an omission, not a limit. Two of the stubs are
honest — there is no triangle primitive behind `QueueGeometry`, and nothing in
GFX reads a surface back — and `QueueCopyEx` is somewhere in between, since
rotation and mirroring are per-call fields of `BitBlit`'s options (§4.6).

Choosing a different *existing* fork therefore still buys a larger maximum
texture and a threaded present rather than capability, because both made the same
omissions. Building one is a different proposition, and it is scoped in
[planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/).

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

**And the texture cap does not follow it — so the Flip has no working
configuration.** Added 2026-07-31 after reading the source rather than a view of
it. `Mini_VideoInit` adjusts `FB_W`/`FB_H`/`FB_SIZE`/`TMP_SIZE` above, but the
renderer's limits are literals in a different file:

```c
.max_texture_width  = 640,       /* SDL_render_mini.c, not derived from FB_W */
.max_texture_height = 480,
```

On a Mini Flip this binary drives a 752×560 panel and refuses any texture wider
than 640, so a full-screen `Layer` cannot be created at all. Two of this
project's documents disagreed about why the Flip needs its own build —
[TARGETS.md § 3a](TARGETS.md) said the geometry is compile-time with no runtime
override, this section said there is one. **Both were half right**: the
framebuffer is detected, the texture cap is not. It needs a patched build rather
than merely a differently-configured one.

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

**The rotation is not the only thing wrong with a sub-rectangle copy, and it may
not be the worst.** Added 2026-07-31 from the source. `GFX_Copy` stages the
texture into MMA memory before blitting (§4.6), and the staging copy starts at
row 0 while the blitter is told to read from row `srt.y`:

```c
memcpy(gfx.tmp.virAddr, pixels, srt.h * pitch);   /* copies from the TOP */
...
gfx.hw.src.rt.s32Ypos = srt.y;                    /* reads from row srt.y */
int rgb565 = (pitch / srt.w) == 2 ? 1 : 0;        /* format from the SUB-RECT's width */
```

So for any source rect with `srt.y > 0` the last `srt.y` rows were never copied,
and the pixel format is misidentified whenever the sub-rect is narrower than the
texture — `1280 / 64 = 20`, not 2, so an RGB565 atlas is handed over as
`ARGB8888`. **An atlas frame is a source rect with a non-zero `y` and a width
smaller than the sheet**, which is to say both bugs fire on exactly the use this
project has next. Neither is visible in `coppers`, whose only sub-rect draws are
full-width HUD lines at `y = 0`.

**That rotation is a constant in the port, not a property of the blitter.**
`eRotate` and `eMirror` are per-call fields of `MI_GFX_Opt_t` (§4.6), so the
value is chosen once per `GFX_Copy` and could as easily be composed with a
caller's.

> **Settled on hardware 2026-08-01: the rotation is correct compensation.** This
> paragraph used to end by saying two readings were possible — compensation for
> an inverted panel, or simply wrong — and that no run distinguished them.
>
> `wreel-diag` drew four coloured quadrants and read `/dev/fb0`: the framebuffer
> holds the image rotated 180°. That is still ambiguous on its own, so it was
> closed by looking at the panel — `coppers`' HUD text reads left-to-right from
> the top left. **The panel is mounted inverted and `E_MI_GFX_ROTATE_180`
> compensates.** Removing it would break the only path that works.
>
> **The `dst.y` line above is the defect.** For content to appear at `dstrect`
> on an inverted panel the framebuffer rect must be mirrored in *both* axes.
> Mirroring x and not y is invisible full-screen — both work out to 0 — and
> puts every sub-rectangle at a vertically mirrored position. So the paragraph
> above this one is right that sub-rect copies land "rotated and displaced", and
> wrong about which half is the bug: the rotation is correct and the
> displacement is not.
>
> **Measured, not just derived.** A 160×120 block requested at y=60 landed at
> y=300 on the 480-tall panel, `480 − 60 − 120`, with x correct.
>
> Scoped as item 20 in
> [miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/), which is a
> one-line fix.

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

> **Still true, and no longer permanent — 2026-08-01.** That last sentence was
> written about a library we did not build. We build it now
> ([platform/miyoomini/sdl2/](../platform/miyoomini/sdl2/)), so this is a
> function we own that returns 0, not a property of the device.
>
> Filling it in is roughly ten lines — `GFX_Copy` the window surface, then
> `GFX_Flip` — and the measurement above says everything else on that path
> already works: 965 frames rendered *correctly* before being dropped on the
> floor. What it would buy is SDL's own software renderer, which is the
> reference implementation of the contract § 5 is a list of gaps in: correct
> sub-rectangles, blend modes, colour modulation, fills and render-to-texture,
> at CPU cost this project already pays for `Layer`.
>
> Scoped as item 21 in
> [miyoo-sdl2-fork § 8.5](../planning/2026-07-31-miyoo-sdl2-fork/). Until it
> lands, `SDL_RENDER_DRIVER=software` on this device produces a black screen
> rather than a slow one, which is worth knowing before reaching for it as a
> comparison.

### 4.5 What the runtime drags in, and the 21.8 MB that it does not

`libSDL2-2.0.so.0` declares four non-system dependencies. Only two of them are
real:

| `DT_NEEDED` | Size | Symbols actually referenced |
|---|---|---|
| `libEGL.so` | 55 KB | **11** — the EGL entry points |
| `libGLESv2.so` | 21.8 MB | **0** |
| `libjson-c.so.5` | 51 KB | 4 |
| `libmi_*`, `libshmvar` | firmware's | the vendor SDK, and mandatory |

`libGLESv2.so` is SwiftShader, a software OpenGL implementation, and it is three
quarters of a staged bundle on a device with no GPU. The library references not
one symbol from it — the `DT_NEEDED` is a link-time artefact of `-lGLESv2`
passed without `--as-needed`, not a dependency. Re-checkable in one command:

```console
$ comm -12 <(readelf --dyn-syms -W libSDL2-2.0.so.0 | awk '$7=="UND"{print $8}' | sed 's/@.*//' | sort -u) \
           <(readelf --dyn-syms -W libGLESv2.so     | awk '$7!="UND"{print $8}' | sed 's/@.*//' | sort -u)
(nothing)
```

So the bundle drops it — `scripts/drop-unused-needed.sh`, run by the
`bundle-onion` target, which performs exactly the comparison above and refuses
to patch anything if it comes back non-empty. **30 MB staged becomes 8.8 MB, and
13 MB compressed becomes 3.5 MB.**

Removing it does not break EGL. `libEGL.so` does not list the GL library as
`DT_NEEDED`; it carries the strings `libGLESv2.so`, `libGLESv2.so.2` and
`libGLESv2_swiftshader`, and opens one of them on demand. Dropping the entry
defers the GL library to the moment something asks for a GL context rather than
removing the possibility — and on this target nothing can ask, because
`gfx::gles2` is not compiled and `WREEL_ENABLE_GLES2` is rejected for a device
with no GPU. Should something ask anyway, the `dlopen` fails at that point, which
is loud.

The 55 KB `libEGL.so` stays, but **not because anything calls it**. All eleven of
its symbols are imported and none is reached: ten need a GL context, and the
eleventh, `eglUpdateBufferSettings`, is called from `glCreateContext` alone. What
keeps the file present is the `DT_NEEDED` itself — the loader resolves those
whether or not the symbols are used, and unlike `libGLESv2.so` these symbols
really are imported, so the `--as-needed` argument that justifies dropping the GL
library does not apply. A stub exporting the eleven, or a source build that does
not pass `-lEGL`, are the two ways it goes.

> **Corrected 2026-07-31.** This paragraph used to say that one of the eleven
> symbols is why the file stays: "SDL calls `eglUpdateBufferSettings` from
> `Mini_CreateWindow` to register the `GFX_CB` presentation callback,
> unconditionally and before any GL context exists."
>
> `Mini_CreateWindow` calls `glUpdateBufferSettings` — a static function in the
> port's own `SDL_gles_mini.c`, which stores the callback in a file-scope
> variable. The EGL symbol is one letter away and is not it. Read from source in
> [planning/2026-07-31-miyoo-sdl2-fork § 1.7](../planning/2026-07-31-miyoo-sdl2-fork/).
> Same shape as the § 4.3 correction below it: a conclusion drawn from a symbol
> table where the call site was the thing to read.

**The two builds spell the soname differently** — steward-fu's declares
`libGLESv2.so`, Onion's `parasyte` copy declares `libGLESv2.so.2` — and
`patchelf --remove-needed` exits 0 on a name that is not there. The script
checks the entry exists before patching for that reason; it is the same shape as
the `mmiyoo`/`mini` driver-name confusion in §3.

Reasoning and options in
[planning/2026-07-29-gles-free-runtime](../planning/2026-07-29-gles-free-runtime/).

### 4.6 What `MI_GFX` actually offers, and what it demands in return

Read 2026-07-31 from the SDK in the toolchain sysroot, which is the authority for
this chip. Everything above this section was inferred from the ports; this is the
header.

```sh
docker run --rm wreel-miyoomini \
  cat /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/mi_gfx.h
```

**Do not code against the published SigmaStar page instead.** The
[SSD201 GFX doc](https://wx.comake.online/doc/doc/SigmaStarDocs-SSD201-SIGMASTAR-202305231828/platform/MI/V2/gfx_en.html)
is the nearest thing to official documentation and is worth reading, but it is a
*different revision* of the V2 API: it gives `MI_GFX_BitBlit` a leading
`MI_GFX_DEV GfxDevId` parameter, and ours has none. Our header declares
`GFX_MAJOR_VERSION 2`, `GFX_SUB_VERSION 5`. The port's call in §4.2 matches the
sysroot, not the page.

**Nine entry points are exported; eight are declared.** `MI_GFX_DrawLine` is in
`libmi_gfx.so` but *not* in `mi_gfx.h`, so reaching it means writing your own
`extern` declaration against an undocumented signature — treat it as unsupported
surface rather than as an available feature. The page above documents an
`MI_GFX_Line_t` for it, with the notable semantic that "the end point of the
straight line will not be drawn", where `SDL_RenderDrawLine` does draw its
endpoint.

**`BitBlit` carries far more than a copy.** The options struct is applied per
call:

```c
typedef struct MI_GFX_Opt_s {
    MI_GFX_Rect_t          stClipRect;
    MI_GFX_ColorKeyInfo_t  stSrcColorKeyInfo, stDstColorKeyInfo;
    MI_GFX_DfbBldOp_e      eSrcDfbBldOp, eDstDfbBldOp;
    MI_GFX_Mirror_e        eMirror;      /* NONE / HORIZONTAL / VERTICAL / BOTH */
    MI_GFX_Rotate_e        eRotate;      /* 0 / 90 / 180 / 270 */
    MI_Gfx_DfbBlendFlags_e eDFBBlendFlag;
    MI_U32                 u32GlobalSrcConstColor, u32GlobalDstConstColor;
} MI_GFX_Opt_t;
```

The blend operands are DirectFB's eleven (`SRCALPHA`, `INVSRCALPHA`,
`DESTALPHA`, `SRCALPHASAT` and the rest), and the flags add `COLORALPHA`,
`COLORIZE`, `SRC_PREMULTIPLY`, `XOR` and source/destination colour-keying. **All
of `SDL_BLENDMODE_BLEND`, `ADD` and `MOD` are expressible in hardware here, and
none is wired up.** Twenty-two colour formats are supported including I1–I8 with
a palette; `E_MI_GFX_FMT_ABGR8888` is among them, which is what
`loaders::load_image` already converts to, so that path costs no conversion.

**The demand: every surface is a physical address.**

```c
typedef struct MI_GFX_Surface_s {
    MI_PHY  phyAddr;                     /* physical, not virtual */
    MI_GFX_ColorFmt_e eColorFmt;
    MI_U32  u32Width, u32Height, u32Stride;
} MI_GFX_Surface_t;
```

A `malloc`'d buffer cannot be blitted. Memory has to come from
`MI_SYS_MMA_Alloc`, which is why the port keeps a staging buffer and why
`GFX_Copy` takes `pixels` and copies them into `gfx.tmp.virAddr` before handing
over a surface. **There is a CPU `memcpy` of the frame on every present**, and it
is inside the 2.51 ms blit figure in §7 rather than beside it. A layer that
allocated MMA memory directly would not pay it — the most concrete performance
item this section turns up.

**The error enum names the constraints**, and they are worth knowing before
designing against any of the above:

| Code | Means |
|---|---|
| `NON_ALIGN_ADDRESS`, `NON_ALIGN_PITCH` | physical address and stride must align to the pixel size |
| `DRV_FAIL_OVERLAP` | source and destination may not overlap |
| `DRV_FAIL_STRETCH` | scaling is a distinct path that can fail on its own |
| `DRV_FAIL_FORMAT`, `DRV_FAIL_LOCKED` | unsupported format; surface held elsewhere |

`DRV_FAIL_STRETCH` existing as its own failure is the likely explanation for §7's
otherwise odd measurement that a **scaled** blit costs more than a 1:1 one even
with the hardware doing the scaling.

---

## 5. Capability summary

Third column added 2026-07-31, and it is the point of the table: **most of these
are the port's limits, not the device's.** Anything marked *port* is reachable
from `MI_GFX` and simply not wired up — see §4.6 and
[planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/).

**The state rows have one shared cause**, and it is worth knowing before reading
them as five separate omissions. SDL2 backends are supposed to append to a
command buffer in `Queue*` and execute it in `RunCommandQueue`; this driver draws
during queueing and its `RunCommandQueue` is `return 0`. So clear, viewport, clip
rect, draw colour and blend state are all built by SDL and discarded unread.
`SDL_RenderClear` has no other path — it *only* queues.

**This bites code that looks correct.** `Context::clear()`,
`Texture::set_blend()`, `set_color_mod()` and `set_alpha_mod()` are ordinary
SDL2 and do nothing here, silently. Nothing has caught it because nothing has yet
drawn a blended sprite on the device.

> **Measured on hardware 2026-08-01**, and the table below is no longer inferred
> from source. `wreel-diag` draws known content and reads `/dev/fb0` back; the
> run is in
> [miyoo-sdl2-fork § 8.3](../planning/2026-07-31-miyoo-sdl2-fork/). Blend mode,
> colour mod and `SDL_RenderFillRect` all report IGNORED exactly as this table
> predicted. Two rows are now *more* than the table said:
>
> - **`SDL_UpdateTexture` does not copy.** Not a missing feature — a
>   use-after-free, D27, confirmed by a SIGSEGV and then measured deliberately.
> - **The rotation is correct compensation, not a defect.** The panel is mounted
>   inverted. What *is* wrong is that `Mini_QueueCopy` mirrors destination x and
>   not destination y, so sub-rectangle *placement* is vertically mirrored while
>   full-screen output is right. See § 4.3.
>
> Also confirmed here: this project builds its own copy of this library now, and
> the rows below hold for it as well as for the prebuilt — they are the same
> driver sources. See [TARGETS.md](TARGETS.md) § The Miyoo Mini exception.

| Capability | Status through the shipped SDL2 | Whose limit |
|---|---|---|
| Full-screen texture copy | **works** — the only thing that does | — |
| Sub-rectangle copy (`Context::draw` with a source rect) | draws, wrongly: rotated and mirrored | **port** — `eRotate` is per call |
| `SDL_RenderFillRect` | silent no-op | **port** — `MI_GFX_QuickFill` exists |
| Lines | silent no-op | **port**, but `MI_GFX_DrawLine` is undeclared |
| Points | silent no-op | hardware — no point primitive |
| Geometry / triangles | silent no-op | **hardware** — no triangle primitive |
| `SDL_RenderCopyEx` (rotation, flip) | silent no-op | **port** for 90° steps and flips; hardware for arbitrary angles |
| `SDL_RenderClear` + draw colour | **silent no-op** | **port** — `RenderClear` only queues a command, and `RunCommandQueue` returns 0 |
| Blend modes (`BLEND`, `ADD`, `MOD`) | **silent no-op** — alpha sprites render opaque | **port** — eleven blend ops in `MI_GFX_Opt_t` |
| `SDL_SetTextureColorMod` / `AlphaMod` | **silent no-op** — no fades, tints or flashes | **port** — `COLORIZE`/`COLORALPHA` + `u32GlobalSrcConstColor` |
| `SDL_SetTextureScaleMode` | silent no-op — nearest always | **hardware** — no filter field in `MI_GFX_Opt_t` |
| Viewport, clip rect, logical size | **silent no-ops** | **port** — SDL queues them as commands the driver drops; `stClipRect` exists |
| `SDL_RenderReadPixels` | `SDL_Unsupported` | **unclear** — nothing in GFX reads back, but textures are plain CPU buffers and the framebuffer is `MI_SYS_Mmap`-able, so a port could plausibly do it without GFX at all |
| Render-to-texture | documented by the fork as not working | not investigated |
| Max texture | 640×480 (800×480 on XK9274's) | **port** — compile-time `DEF_FB_W/H` |
| Display metadata | none: no desktop mode, no bounds | **port** — §4.1 |
| Keyboard/gamepad | **keyboard only** — see §6.3 | port |
| Audio | works on Onion once `audioserver` releases MI_AO; **not yet up on stock** — see §6.2 | firmware |

---

## 6. Firmware: OnionOS, and stock

Both have run this project's bundle. Where an observation below holds on only
one of them, it says which — the distinction is newer than the rest of this
document and should not be assumed to have been checked everywhere.

### 6.1 Bundle layout

Apps live at `App/<Name>/` with `config.json` and `launch.sh`; ports use three
parallel trees under `Roms/PORTS/`. Taken from real packages in
[OnionUI/Onion](https://github.com/OnionUI/Onion/tree/main/static/packages/App)
and [OnionUI/Ports-Collection](https://github.com/OnionUI/Ports-Collection), not
from a guide. Native SDL2 ports vendor their runtime in the app's own `lib/` —
Sonic Mania ships `libSDL2-2.0.so.0`, `libEGL.so` and `libjson-c.so.5` that way.

**The App layout is stock MainUI's, and Onion inherited it.** Observed
2026-07-30: the same `App/Coppers/` directory, copied to the same path with an
unmodified `config.json`, was listed and launched by the **stock** firmware's
Apps menu. So the four keys below are not an Onion convention, and a bundle
built for Onion is not thereby Onion-only:

```json
{ "label": "Coppers", "icon": "...", "launch": "launch.sh", "description": "..." }
```

Two qualifications, because one run is one run. The `icon` path this project
generates is theme-relative in Onion's style (`../../Icons/Default/app/...`) and
no icon is installed either way, so a stock menu resolving it differently would
be invisible — a missing icon is cosmetic and was never the test. And `Roms/PORTS/`
above remains an Onion structure; nothing here has looked for a stock equivalent.

What this does *not* mean is that the two firmwares are interchangeable
underneath. §6.2 is where they demonstrably differ.

**Do not borrow Onion's `parasyte` SDL2.** Its EGL is Mesa and pulls in gbm,
glapi, X11, xcb and libdrm, plus a matched loader and libc: it is an alternate
userland for binaries built against a newer glibc, not a library.

### 6.2 Audio is single-owner, and this is where the firmwares diverge

`MI_AO` admits one owner. That much is the vendor layer's, not a firmware's, and
it is the one capability in §5 that is not settled on both firmwares.

**On OnionOS** the owner is `audioserver`. With it running, opening audio fails
inside the vendor layer before SDL sees it:

```
[MI ERR ]: MI_AO_SetPubAttr[3364]: Dev0 failed to set pub attr!!! error number:0xa0052009!!!
```

The firmware ships the remedy at
`/mnt/SDCARD/.tmp_update/script/stop_audioserver.sh` and its own ports launcher
sources it. **That script expects `$miyoodir`** (`/mnt/SDCARD/miyoo`, set in
Onion's `runtime.sh`); without it, one branch kills `wpa_supplicant` and
`udhcpc` and cannot restart them, taking the network down. With that handled,
audio works: the mixer opens at the handheld profile and tracker music plays.

**`libpadsp.so` is preloaded by both firmwares**, not only Onion. Stock sets it
too, out of its own versioned directory:

```
LD_PRELOAD=/mnt/SDCARD/miyoo354/app/../lib/libpadsp.so
```

Onion's ports launcher unsets it before exec'ing and `launch.sh` does the same
on both. That is right here for a reason worth stating, because "keep the shim
and let it proxy" is the obvious thing to reach for when audio fails: **this
SDL2 compiles in one audio driver and has no OSS path at all.**

```console
$ strings -a libSDL2-2.0.so.0 | grep -xiE 'mmiyoo|mini|Miyoo Mini|dsp|oss|alsa|pulseaudio|dummy|disk' | sort -u
mini
Mini
Miyoo Mini
$ strings -a libSDL2-2.0.so.0 | grep -iE '/dev/dsp|/dev/audio|soundcard\.h|SDL_PATH_DSP'
(nothing)
```

There is no path through the library that a dsp shim could serve, so keeping
`LD_PRELOAD` would not have produced sound on either firmware.

**On stock, audio does not come up — and it is the same contention.** Observed
2026-07-30, in the run where everything else worked:

```
--- /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh not found; audio will likely fail ---
...
[MI ERR ]: MI_AO_SetPubAttr[3364]: Dev0 failed to set pub attr!!! error number:0xa0052009!!!
[MI ERR ]: MI_AO_DisableChn[3667]: Dev0 has not been enabled.
[W] audio: Mix_OpenAudio failed (); continuing without sound
[W] music: no audio device, running in silence
```

Same error number as Onion, same point of failure. What differs is not the fault
but the remedy: **stock ships no `stop_audioserver.sh`**, so there is no
firmware-supplied way to release the device, and the run proceeded with whatever
already held it.

**Who holds it on stock is not established.** The process table names it and
nothing on a dev box can, so it is a device question. Whether to stop it there
is a second question and not a foregone one — Onion's script is firmware-supplied
and MainUI restarts what it stops; a hand-rolled kill on stock would be neither.

**Note the empty parenthesis** in `Mix_OpenAudio failed ()`. The vendor audio
driver fails without calling `SDL_SetError`, so SDL has nothing to report and the
only description of the failure is the `MI ERR` lines the vendor layer writes
straight to stdout. That is defect D26, and it is why the launcher redirects the
whole script rather than reading back a log file — on this device the failure
that matters most did not go through the logger at all.

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

The table is sourced from Onion, but the mechanism is not Onion's: the driver
reads the kernel's input device directly, and the same binary took input
correctly on stock firmware (2026-07-30) with the same bindings compiled in.
That is consistent with the keymap belonging to the kernel input driver both
firmwares share, and it is one run — enough to stop treating the bindings as
Onion-specific, not enough to have verified every key on stock.

### 6.4 Other observations

- **`HOME` is wrong in a different way on each firmware**, and neither is
  usable: `/mnt/SDCARD/RetroArch/` from Onion's Apps menu, and `/` from stock's.
  `SDL_GetPrefPath` would put the run log under either. `launch.sh` overrides it
  to the bundle directory on both, which is what keeps the log on the SD card.
- The clock is not set: logs show 1970 dates. Both firmwares.
- The two firmwares are not the same kernel build — `#1133 SMP PREEMPT Fri May 5
  2023` on the Onion device, `#1136 SMP PREEMPT Wed Jun 28 2023` on the stock
  one. Same 4.9.84.
- Stock keeps its userland under a version-stamped directory, `miyoo354/` on the
  unit tested, where Onion uses `.tmp_update/`. Anything reaching into firmware
  paths by name should expect the stock one to move between releases.

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

The only model **the SDL2 we ship** supports is: **compose the whole frame
yourself into one full-screen buffer, and hand it over once.** That is what
`gfx::renderer::Layer` is, and it is comfortable at this resolution.

**Revised 2026-07-31.** This used to say "the only model this device supports",
and that it is "what the hardware is built around". §4.6 withdraws both: the
blitter fills, blends, colour-keys, mirrors and rotates per call, and it is the
port that exposes none of it. The conclusion is unchanged as a *shipping*
decision, because we ship a prebuilt binary we do not build — but the argument
that carries it is §7's measurement, not a hardware limit. Plotting a full-screen
field costs about 1 ms of a 16.7 ms frame, so composing on the CPU is cheap here
whether or not the alternative exists.

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
| A different *existing* SDL2 fork | 800px textures, threaded present | Same omissions (§3.1) |
| **Our own SDL2 fork** | `FillRect`, correct sub-rect placement, hardware blending, a bigger texture cap, possibly the staging copy removed | A vendored patch set to maintain against a dead 2.0.20 base, and a build nobody else reproduces — scoped in [planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/) |
| Own platform layer (fbdev + MI_GFX + evdev + MI_AO) | Total control, and the SDK is in the sysroot | Reimplements input, audio and present per device — a platform port, not a fix |

Worth noting how close the chosen path already is to the last one: SDL2 is
providing window creation, input translation, audio and timing, all adequately.
What it is not providing here is a renderer — and the engine has stopped needing
one. That is what makes the fourth row a genuine option rather than an obvious
win: it would restore capability the engine has already routed around.

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

Vendor graphics API:

- `mi_gfx.h` and `mi_gfx_datatype.h` in the toolchain sysroot at
  `/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/` — **the
  authority for this chip**, GFX v2.5, and what §4.6 is read from
- [SigmaStar SSD201 MI_GFX documentation](https://wx.comake.online/doc/doc/SigmaStarDocs-SSD201-SIGMASTAR-202305231828/platform/MI/V2/gfx_en.html)
  — the nearest thing to official prose, and the only description of
  `MI_GFX_DrawLine` there is. A **different revision** of V2: its `BitBlit` takes
  a device id and ours does not, so read it for semantics and the header for
  signatures

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

# §3.1 and §4.6: what MI_GFX offers against what the shipped SDL2 asks of it
SYS=/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr
docker run --rm wreel-miyoomini sh -c \
  "readelf --dyn-syms -W $SYS/lib/libmi_gfx.so | awk '\$7!=\"UND\"{print \$8}'" \
  | grep ^MI_GFX | sort -u
readelf --dyn-syms -W lib/libSDL2-2.0.so.0 | awk '$7=="UND"{print $8}' | grep ^MI_GFX
```

The difference between those last two lists is the subject of
[planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/).
