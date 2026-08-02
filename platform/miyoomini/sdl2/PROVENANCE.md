# Miyoo Mini SDL2 drivers — origin, licence and modifications

The SSD202D video, render and audio drivers, compiled into the pinned upstream
SDL2 rather than taken as a prebuilt binary. Companion to
[THIRD-PARTY.md](../../../THIRD-PARTY.md), which covers binaries we did *not*
build; this file covers source we did not write.

**Written 2026-08-01**, when the graft first produced a working library.
Reasoning and alternatives in
[planning/2026-07-31-miyoo-sdl2-fork](../../../planning/2026-07-31-miyoo-sdl2-fork/).

## Origin

| | |
|---|---|
| Upstream | [steward-fu/sdl2](https://github.com/steward-fu/sdl2) |
| Commit | `68ce3172` (2025-12-17) |
| Licence | **LGPL-2.1**, per the header on every file |
| Base SDL | 2.0.20 there; **release-2.32.10** here |

`68ce3172` rather than the repository head, deliberately: it is the last commit
to touch `prebuilt/640x480/`, so it is the source corresponding to the binary
this project shipped until now. That makes the shipped library a control for
anything these drivers do differently. See § 7.1 of the planning snapshot for
the one later commit that touches them and why it is not taken wholesale.

**The upstream files are recoverable byte-for-byte**, which is what makes the
"modifications" list below checkable rather than a claim:

```sh
git -C /path/to/sdl2-steward-fu show 68ce3172:sdl2/src/video/mini/SDL_video_mini.c
```

Blob hashes as imported, before any edit of ours — `git hash-object` on the
pristine file gives the same SHA GitHub reports:

| File | Blob |
|---|---|
| `src/video/mini/SDL_video_mini.c` | `38c4d8c5` |
| `src/video/mini/SDL_video_mini.h` | `30c0fbf4` |
| `src/video/mini/SDL_event_mini.c` | `bd954195` |
| `src/video/mini/SDL_event_mini.h` | `d2d80a66` |
| `src/video/mini/SDL_fb_mini.c` | `fb081dc7` |
| `src/video/mini/SDL_fb_mini.h` | `ae37036b` |
| `src/render/mini/SDL_render_mini.c` | `0797628d` |
| `src/audio/mini/SDL_audio_mini.c` | `09fca522` |
| `src/audio/mini/SDL_audio_mini.h` | `9239b523` |

## Licence obligations

**LGPL-2.1 comes from these files, not from SDL2.** Upstream SDL2 is zlib
(`sdl2/LICENSE.txt`, "Copyright (C) 1997-2022 Sam Lantinga"); it is steward-fu's
~1,290 lines that carry `// LGPL-2.1 License`. Two consequences worth stating
plainly, because both are easy to get backwards:

- **The obligation survives the rebase.** Putting these drivers on a newer SDL2
  changes nothing about their licence. Only replacing them — a clean-room
  driver written against `mi_gfx.h` — would.
- **It is why `libSDL2` is linked shared on this target and only this one.** The
  licence requires that a user can relink against a modified copy; dynamic
  linking meets that without shipping object files with every release.
  `WREEL_SDL2_LINKAGE` in [cmake/ProjectOptions.cmake](../../../cmake/ProjectOptions.cmake)
  forces SHARED whenever these drivers are in the build, and says so there.

**This is the declaration of modification.** The list below is complete.

## What we changed, and why

Three kinds of change, kept apart because they carry different risk: what SDL
2.32.10 requires to compile, what removes an unjustifiable dependency, and — from
2026-08-02 — deliberate correctness fixes to the drivers themselves.

The correctness fixes came **after** the unmodified graft had run on hardware,
not with it. The first device run had to compare against the shipped binary, and
that comparison would have been worthless if the drivers had changed at the same
time. That run is § 8.1 of the planning snapshot; the fixes below are stage 1.

### Required by SDL 2.32.10

Six backend interface changes between 2.0.20 and 2.32.10. The first five were
found by survey; the sixth by the compiler, which is recorded because it says
what the survey was worth.

| Where | 2.0.20 | 2.32.10 |
|---|---|---|
| `SDL_RenderDriver.CreateRenderer` | `SDL_Renderer *(*)(SDL_Window*, Uint32)` | `int (*)(SDL_Renderer*, SDL_Window*, Uint32)` — SDL allocates and frees the renderer |
| `RenderPresent` | returns `void` | returns `int` |
| `QueueCopyEx` | … `SDL_RendererFlip flip` | … `flip, float scale_x, float scale_y` |
| audio `OpenDevice` | `(_THIS, void*, const char*, int)` | `(_THIS, const char*)` |
| `AudioBootStrap.init` | `int (*)(SDL_AudioDriverImpl*)` | `SDL_bool (*)(SDL_AudioDriverImpl*)` |
| `VideoBootStrap.create` | `SDL_VideoDevice *(*)(int devindex)` | `SDL_VideoDevice *(*)(void)` |

Two consequences beyond the signatures:

- **`Mini_DestroyRenderer` no longer frees the renderer.** From 2.24 the frontend
  owns that allocation; leaving upstream's `SDL_free(renderer)` in place would be
  a double free. This is the one change here that fixes a real bug rather than
  satisfying a compiler.
- **`_THIS` is `#undef`'d before the audio driver redefines it.** The audio file
  includes the video driver's header, so both of SDL's definitions are in scope
  and the redefinition warns without it.

### Dependencies removed

- **No GL.** `SDL_gles_mini.{c,h}` is not imported, the `device->GL_*`
  assignments and the `SDL_GLDriverData` allocation are gone, and `GFX_CB` — the
  EGL presentation callback, reachable only from `glCreateContext` — with them.
  The SSD202D has no 3D block; the only GL this could offer is SwiftShader
  through a `libEGL.so` whose licence THIRD-PARTY.md records as unknown.
  **Result: neither `libEGL.so` nor `libGLESv2.so` is a `DT_NEEDED` of the
  library we build.**
- **No json-c, and no system-volume handling at all.** Upstream read
  `/appconfigs/system.json` with json-c on every `OpenDevice` and wrote the
  result to the **system** volume through an ioctl on `/dev/mi_ao`. That is the
  only reason the bundle ever carried a `libjson-c.so.5` of unidentifiable
  version.

  The whole mechanism is gone — `set_volume`, `set_volume_raw`, the exported
  and never-called `volume_inc`/`volume_dec`, the three ioctl numbers, and the
  config read. Note it was a *different knob* from the `MI_AO_SetVolume` that
  remains: that one is the AO device's own gain, set to unity as part of
  bringing the device up, and it stays.

  **Re-asserting the system volume from a config file is the firmware's job.**
  The user set that level with the volume buttons and it should survive a
  program starting. Removing it also removes upstream's worst behaviour: the
  fallback when the file was missing was `0`, and `0` drove the level to
  `MIN_RAW_VALUE` and *muted the device* — so a firmware without that file got
  silence.

  An intermediate design — the host reading the file and passing the value in
  as `SDL_MINI_VOLUME` — was built and then withdrawn on 2026-08-01. It moved
  the coupling rather than removing it: whichever module read the file learned a
  firmware path, a config format and a private env-var contract with this
  driver, to re-apply a value the system already held.
- **`#include "SDL_image.h"` dropped** from the video driver. Nothing in the file
  uses `IMG_*`; it is why upstream's `configure.ac` hardcodes an SDL2 include
  path from a buildroot toolchain that has nothing to do with this build.

### Correctness fixes, 2026-08-02

Three, each measured on hardware by `wreel-diag` before and after. Numbered as
the planning snapshot's § 3 numbers them.

- **Item 1 — `Mini_UpdateTexture` copies the pixels.** Upstream recorded the
  caller's pointer in a table and dereferenced it at draw time, so any texture
  whose upload buffer had been freed or reused was a use-after-free — and
  `SDL_CreateTextureFromSurface` frees its converted surface before it returns.
  The driver already allocated `t->data` and used it only on the lock path; the
  update path now copies into it, honouring the rect SDL passes.

  `Mini_UnlockTexture` registers `t->data` directly rather than routing through
  `Mini_UpdateTexture`, which after the fix would have been a `memcpy` onto
  itself. Verdict `SDL_UpdateTexture copies`: WRONG → OK.

- **Item 21 — `Mini_UpdateWindowFramebuffer` is implemented.** It was `return 0`,
  so SDL's own software renderer composited correct frames that never reached the
  panel. It now stages the window surface through `GFX_Copy` and flips, with two
  guards: the surface must fit the staging buffer, and the window must not be
  larger than the panel. Both are reachable — this driver advertises 800x600 on a
  640x480 panel. Confirmed by running the demo with `SDL_RENDER_DRIVER=software`.

- **Item 20 — `Mini_QueueCopy` mirrors the destination `y`.** It mirrored `x`
  only. The panel is mounted inverted and `GFX_Copy` compensates with a fixed
  180-degree rotation, so a destination rect has to be reflected through the
  centre of the window in *both* axes; mirroring one cancels the rotation there
  and leaves the other doubled. Invisible full-screen, wrong for every sprite.
  Verdict `partial destination`: WRONG → OK.

## What is NOT changed

The remaining bugs. `GFX_Copy` still copies from row 0 while telling the blitter
to read from `srt.y`, and still infers the pixel format from `pitch / srt.w`;
`update_texture`'s 100-entry table is still unbounded;
`max_texture_width/height` are still literals of 640x480 that the Flip detection
does not reach; `SDL_RENDERER_TARGETTEXTURE` is still advertised and still does
nothing; `RunCommandQueue` still returns 0 without executing anything.

All of it is inventoried, with fixes, in § 1 and § 3 of the planning snapshot.

## Rebuilding the registration patch

`patches/0001-register-mini-drivers.patch` is additive only — 46 lines inserted
across 8 upstream files, none removed or changed — and it is applied by
[graft.cmake](graft.cmake) from FetchContent's `PATCH_COMMAND`. If the SDL2 pin
moves and it stops applying, regenerate rather than force it:

```sh
# in a populated, unpatched SDL2 source tree at the new tag
#   1. redo the eight edits (the patch itself is the specification)
#   2. then:
git -c safe.directory="$PWD" diff > 0001-register-mini-drivers.patch
```

The tree under `_deps/` is created by the container and is owned by root; the
`safe.directory` override is why that command reads the way it does.
