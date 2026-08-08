# Maintaining our own SDL2 for the Miyoo Mini

**Status:** `in-progress` — stage 0 passed 2026-08-01; stage 1 items 1, 21 and
20 landed and verified on hardware 2026-08-02
**Written:** 2026-07-31
**Blocked by:** nothing — but stage 0 is a gate, and everything after it is
conditional on stage 0 succeeding
**Serves:** [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/),
[packaging-distribution](../2026-07-25-packaging-distribution/), the Mini Flip,
and defects D22, D24, D25, D26, D27
**Decision:** taken 2026-07-31 and **revised the same day** — the drivers rebased
onto the already-pinned SDL 2.32.10 (option **E**), tier 1 plus a reproducible
build. Tier 2 is no longer decided here, because the engine-side alternative
serves five targets where a driver patch serves one. Still gated on stage 0.
See §5

> **Why a same-day revision.** The first decision was taken on a reading of the
> port's *sources*. It was not taken on a reading of the *build*, of the SDK in
> our own toolchain, or of what upstream SDL2's backend interface looks like now
> — and all three turned out to matter. §1.7, §3 and §5 carry what changed; the
> §1 and §2 findings that motivated the work are unaffected and were re-checked
> against the same checkout.

> **Stage 0 passed 2026-08-01.** The drivers are grafted onto the pinned SDL
> 2.32.10, the library builds in the toolchain container, and `coppers` ran on
> the device against it: **859 frames at 59.7 fps, audio at 22050 Hz**, video
> driver `Mini`, render driver `Miyoo Mini (accelerated)` — indistinguishable
> from the prebuilt. §8 is the full record, including what the accompanying
> diagnostics run measured and what it got wrong twice before it was
> trustworthy.

## Motivation

We ship a prebuilt `libSDL2-2.0.so.0` we did not build, whose renderer is
[341 lines](https://github.com/steward-fu/sdl2/blob/master/sdl2/src/render/mini/SDL_render_mini.c)
and implements one drawing operation. Until 2026-07-31 the recorded explanation
was that the hardware offers one drawing operation. It does not — `MI_GFX`
fills, blends, colour-keys, mirrors and rotates, per call
([docs/MIYOO-MINI.md § 4.6](../../docs/MIYOO-MINI.md)).

That alone would be a features argument, and a weak one, because the engine has
already routed around the gap: `gfx::renderer::Layer` composes on the CPU for
about 1 ms of a 16.7 ms frame. **Reading the port's actual source is what changes
the question**, and it was read for the first time on 2026-07-31, from a local
checkout rather than through a web view — see §7.

Two things came out of that reading, and the second is the larger:

- **The port has correctness bugs**, including a use-after-free on our own
  `draw_surface()` path — §1.
- **It discards SDL2's state model wholesale**, so `SDL_RenderClear`,
  `SDL_SetTextureBlendMode`, `SDL_SetTextureColorMod` and `SDL_SetTextureAlphaMod`
  are silent no-ops. Our engine already calls all four — §2.

Both land on the module this project is trying to build next. An atlas cannot
render correctly here (§1.2, §1.3, D25), and even once it does, every sprite
would be an opaque rectangle (§2.1).

---

## 1. What is actually wrong, read from source

Line references are `sdl2/src/{video,render}/mini/` at head `0631abc8`.

### 1.1 `SDL_UpdateTexture` stores a borrowed pointer and never copies

> **Fixed 2026-08-02 (item 1).** Kept as written because it is the reasoning that
> found the defect and the record of how it was proven. The driver now copies
> into `t->data`; the verdict below reads OK.

`SDL_render_mini.c:121` —

```c
static int Mini_UpdateTexture(SDL_Renderer *renderer, SDL_Texture *texture,
                              const SDL_Rect *rect, const void *pixels, int pitch)
{
    update_texture(texture, texture, pixels, pitch);   /* stores the pointer */
    return 0;
}
```

`update_texture` writes `pixels` into a table and returns. **The caller's buffer
is never copied**, and `Mini_QueueCopy` dereferences it at draw time via
`get_pixels()`. The driver allocates `t->data` in `Mini_CreateTexture` and then
uses it only on the lock/unlock path.

`SDL_CreateTextureFromSurface` reaches this directly — `texture->native` is null
whenever the format came from `renderer->info.texture_formats`, which it always
does. When the surface format does not match the renderer's, SDL converts into a
temporary and **frees it inside the same function**:

```c
temp = SDL_ConvertSurface(surface, dst_fmt, 0);
SDL_UpdateTexture(texture, NULL, temp->pixels, temp->pitch);
SDL_FreeSurface(temp);                    /* the driver still holds temp->pixels */
```

The `mini` driver advertises two formats, `RGB565` and `ARGB8888`;
`loaders::load_image` produces `ABGR8888`. So this project takes the converting
path, and the stored pointer is dangling **before
`SDL_CreateTextureFromSurface` returns** — not merely after the caller cleans up.

That is the path `gfx::renderer::Context::draw_surface()` takes. **Confirmed on
hardware 2026-08-01** — see §8.3. It took `wreel-diag` down with a SIGSEGV when
that tool's own upload buffer went out of scope, and was then measured directly
without touching freed memory. Before that run this section said it "has not
visibly failed on device, which is what a read of recently-freed heap normally
looks like"; it fails very visibly once the freed page is reused.

`Layer` is unaffected: it locks and unlocks, so the registered pointer is
`t->data`, which the driver owns.

### 1.2 The staging copy ignores the source rect's `y`

`SDL_video_mini.c:117` —

```c
int GFX_Copy(const void *pixels, SDL_Rect srt, SDL_Rect drt, int pitch, ...)
{
    memcpy(gfx.tmp.virAddr, pixels, srt.h * pitch);      /* from the TOP of the texture */
    ...
    gfx.hw.src.rt.s32Ypos = srt.y;                       /* but read from row srt.y */
```

`srt.h * pitch` bytes are copied starting at `pixels` — row 0 — and the blitter
is then told to read `srt.h` rows beginning at row `srt.y`. For any source rect
with `srt.y > 0`, rows `[srt.h, srt.y + srt.h)` were never copied and the blitter
reads whatever the staging buffer last held.

**An atlas is a source rect with a non-zero `y` by definition.**

### 1.3 The pixel format is inferred from the source rect's width

Same function, one line up:

```c
int rgb565 = (pitch / srt.w) == 2 ? 1 : 0;
```

`pitch` is the whole texture's pitch; `srt.w` is the sub-rectangle's width. The
ratio is only the bytes-per-pixel when the source rect spans the full texture.
A 64-pixel-wide sub-rect of a 640-wide RGB565 texture gives `1280 / 64 = 20`, so
the surface is handed to the blitter as `ARGB8888` and renders as noise.

### 1.4 `max_texture_width/height` do not follow the Flip's runtime geometry

`Mini_VideoInit` detects the Flip at runtime and adjusts the framebuffer
(`SDL_video_mini.c:333`):

```c
if (strstr(buf, "752")) { FB_W = 752; FB_H = 560; ... }
```

but the renderer's limits are literals (`SDL_render_mini.c:335`):

```c
.max_texture_width  = 640,
.max_texture_height = 480,
```

So on a Mini Flip this binary drives a 752×560 panel **and refuses to create a
texture wider than 640**. A full-screen `Layer` is impossible there. This settles
a contradiction between our own documents — [TARGETS.md § 3a](../../docs/TARGETS.md)
said the Flip needs its own build because the geometry is compile-time, and
[MIYOO-MINI.md § 4.1](../../docs/MIYOO-MINI.md) said there is a runtime override.
Both were half right: the *framebuffer* is detected, the *texture cap* is not.

### 1.5 `SDL_RENDERER_TARGETTEXTURE` is advertised and does nothing

`Mini_RenderDriver.info.flags` includes it; `Mini_SetRenderTarget` is
`return 0`. A program that asks for render-to-texture is told it succeeded and
draws to the screen instead.

### 1.6 The rest, briefly

- `RunCommandQueue` returns 0 without processing anything — drawing happens
  directly inside `QueueCopy`, so the SDL command queue is bypassed entirely.
  Any new operation has to be added the same way.
- `GFX_Copy` sets `eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE`, `eDstDfbBldOp = 0`,
  `eDFBBlendFlag = 0` — blending explicitly off. This is where blend modes would
  be plumbed, and it is three assignments.
- `int c0 = FB_W / vid_win->w` is integer division; a window wider than the panel
  yields scale 0 and a zero-area blit.
- The `overlay` MMA buffer is allocated and mapped in `Mini_InitGFX` and never
  used — `FB_W * FB_H * 4` bytes, 1.2 MB, on a 128 MB device.
- Only `RGB565` and `ARGB8888` are advertised. `loaders::load_image` converts to
  `ABGR8888`, so SDL converts again on every upload.

**Note what this list is not.** None of it is `MI_GFX` being weak. §1.1–1.3 are
ordinary C bugs, §1.4–1.5 are unmaintained constants, and the blitter underneath
supports everything §1.6 says is switched off.

### 1.7 And one claim of our own that is not true: `libEGL.so` is never called

Three documents — [THIRD-PARTY.md](../../THIRD-PARTY.md),
[MIYOO-MINI.md § 4.5](../../docs/MIYOO-MINI.md) and
[gles-free-runtime § 2](../2026-07-29-gles-free-runtime/) — record that
`Mini_CreateWindow` calls `eglUpdateBufferSettings` to register the `GFX_CB`
presentation callback, and therefore that one symbol of the unknown-licence
`libEGL.so` is load-bearing before any GL context exists.

It does not. It calls `glUpdateBufferSettings`, which is a static function in the
port's own `SDL_gles_mini.c:136`:

```c
int glUpdateBufferSettings(void *cb)
{
    fb_cb = cb;                      /* stores it in a file-scope static */
    debug("%s, callback=%p\n", __func__, cb);
    return 0;
}
```

The libEGL import `eglUpdateBufferSettings` is called from exactly one place,
`glCreateContext` (`SDL_gles_mini.c:124`), reached only through
`SDL_GL_CreateContext`. Nothing on this target creates a GL context, and nothing
*can*: `gfx::gles2` is not compiled and `WREEL_ENABLE_GLES2` is rejected here.

**Two symbols one letter apart, and the wrong one was attributed.** The eleven-symbol
import list in gles-free-runtime § 2 is correct; the sentence naming which of them
matters is not.

What it changes, and it is the largest single item in this revision:

- **`libEGL.so` is a `DT_NEEDED` and nothing else on our path.** No symbol of it
  executes. The stub of gles-free-runtime option C — eleven functions that return
  failure — is now provably safe rather than hopeful, and it removes the shipped
  binary of unknown licence in an afternoon **without any fork**. That is
  [packaging-distribution](../2026-07-25-packaging-distribution/)'s blocker for
  anything that counts as distribution.
- **In a source build the dependency is one line, not a patch.**
  `-lEGL -lGLESv2` is hardcoded in `configure.ac`'s `CheckMiniVideo()`:

  ```
  EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L. -lEGL -lGLESv2"
  ```

  which is why `--disable-video-opengl`, `--disable-video-opengles` and
  `--disable-video-opengles2` never removed it, and why the prebuilt links a GL
  stack it never calls. gles-free-runtime's "removing them means patching the
  driver: dropping the `device->GL_*` assignments and the
  `eglUpdateBufferSettings` call" overstates it: dropping the two `-l` flags and
  not compiling `SDL_gles_mini.c` is the whole of it, and the `device->GL_*`
  assignments then have nothing to point at.

---

## 2. The conformance gap: one cause, not six symptoms

This is the part that matters most for deciding *what* to change, and it was the
last thing found. The driver does not merely omit operations — **it inverts
SDL2's architecture**, and every state-related symptom falls out of that.

SDL2's contract is that `Queue*` functions append to a command buffer and
`RunCommandQueue` executes it. The frontend deliberately relies on this;
`PrepQueueCmdDraw` in `src/render/SDL_render.c` says so:

```c
/* Set the viewport and clip rect directly before draws, so the backends
   don't have to worry about that state. */
if (retval == 0 && !renderer->viewport_queued) { retval = QueueCmdSetViewport(renderer); }
if (retval == 0 && !renderer->cliprect_queued) { retval = QueueCmdSetClipRect(renderer); }
```

The `mini` driver draws *during queueing* and discards *execution*:

```c
static int Mini_RunCommandQueue(...) { debug(...); return 0; }
renderer->QueueSetViewport  = Mini_QueueSetViewport;   /* return 0 */
renderer->QueueSetDrawColor = Mini_QueueSetViewport;   /* same no-op, aliased */
```

So the entire command stream — clear, viewport, clip rect, draw colour, blend
state — is built by SDL and thrown away. Only `QueueCopy`'s side effect survives.

### 2.1 What that costs us, in our own code

Traced against the engine's actual call sites, not hypothetically:

| Our call | Where | Honoured on this target |
|---|---|---|
| `SDL_SetRenderDrawColor` + `SDL_RenderClear` | `Context::clear()` [context.cc:261](../../gfx/renderer/context.cc#L261) | **No.** `SDL_RenderClear` only queues; `RunCommandQueue` drops it. There is no other path |
| `SDL_SetTextureBlendMode(BLENDMODE_BLEND)` | `Texture::set_blend()` [texture.cc:112](../../gfx/renderer/texture.cc#L112) | **No.** `GFX_Copy` hardcodes `eSrcDfbBldOp = BLD_ONE`, `eDstDfbBldOp = 0` |
| `SDL_SetTextureColorMod` | `Texture::set_color_mod()` [texture.cc:90](../../gfx/renderer/texture.cc#L90) | **No.** Never read by the driver |
| `SDL_SetTextureAlphaMod` | `Texture::set_alpha_mod()` [texture.cc:97](../../gfx/renderer/texture.cc#L97) | **No.** Never read |
| `SDL_SetTextureScaleMode` | `Texture`, `Layer` | **No.** `Mini_SetTextureScaleMode` is a no-op |
| `SDL_CreateTextureFromSurface` | `Texture(surface)` [texture.cc:24](../../gfx/renderer/texture.cc#L24) | ~~**Use-after-free**~~ **yes, since item 1 landed 2026-08-02** — §1.1 |
| `SDL_LockTexture` / `SDL_UnlockTexture` | `Layer::lock()` | yes, and safe |
| `SDL_UpdateTexture` | `LayerLock::~LayerLock` | yes, and safe — `Layer` owns `_pixels` for its lifetime |
| `SDL_RenderCopy`, full-screen | `Context::draw()` | yes |
| `SDL_RenderCopy`, sub-rect | `Context::draw(src, dst)` | no — §1.2, §1.3, D25 |

**`Texture::set_blend(true)` is a lie on this target, and that is the biggest
forward-looking item in this document.** A sprite with an alpha channel renders as
an opaque rectangle. So does a tile. `set_color_mod` and `set_alpha_mod` going
nowhere kills fades, tints and damage-flashes — the ordinary vocabulary of a 2D
game. None of this is recorded anywhere in the project, because nothing has drawn
a blended sprite on the device yet.

Weighed against §1: the sub-rect bugs stop an atlas from rendering *correctly*;
the blend gap stops it from rendering *usefully even when correct*. Both block
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

### 2.2 The good news: five of the six map onto fields already being zeroed

`MI_GFX_Opt_t` is `memset` to 0 in `Mini_InitGFX` and then partly filled per
call. The state SDL is discarding has a home in the struct that already exists:

| SDL state | `MI_GFX` mechanism |
|---|---|
| `SDL_BLENDMODE_BLEND` | `eSrcDfbBldOp = SRCALPHA`, `eDstDfbBldOp = INVSRCALPHA`, `eDFBBlendFlag \|= ALPHACHANNEL` |
| `SDL_BLENDMODE_ADD` / `MOD` | the same operand pair, different constants — eleven are available |
| `SDL_SetTextureColorMod` | `E_MI_GFX_DFB_BLEND_COLORIZE` + `u32GlobalSrcConstColor` |
| `SDL_SetTextureAlphaMod` | `E_MI_GFX_DFB_BLEND_COLORALPHA` + `u32GlobalSrcConstColor` |
| `SDL_RenderSetClipRect` | `stClipRect` |
| `SDL_RenderClear` + draw colour | `MI_GFX_QuickFill` over the destination |
| `SDL_RenderSetViewport` | an offset applied to destination rects, plus the clip rect |
| **`SDL_SetTextureScaleMode`** | **nothing** — there is no filter field. A genuine limit |

Scale mode is the one honest gap, and the right response is to report it rather
than accept it silently: `SDL_SetTextureScaleMode` should be a no-op that the
docs record, which is what `Layer` already assumes by defaulting to nearest.

### 2.3 The design constraint this imposes

**Implement `RunCommandQueue` properly or leave it alone entirely — do not
half-do it.** Draw ordering is currently correct *by accident*: every operation
that draws does so at queue time, so queue order equals draw order. Wiring
`QueueFillRects` to `MI_GFX_QuickFill` the same immediate way preserves that.
But implementing execution for *some* commands while others still draw during
queueing inverts their order, and the result is a fill painted over the copy it
was meant to sit behind.

That makes the state work one commit, not a drip of small ones — and it is the
strongest argument for treating this as a real port rather than a patch pile.

---

## 3. What we would own

The Miyoo-specific surface is **1,480 lines across 11 files**:

```
sdl2/src/video/mini/    SDL_video_mini.c  365   SDL_event_mini.c  169
                        SDL_gles_mini.c   160   SDL_fb_mini.c      61   + 4 headers
sdl2/src/render/mini/   SDL_render_mini.c 341
sdl2/src/audio/mini/    SDL_audio_mini.c  240                          + 1 header
```

**Corrected 2026-07-31, same day.** The first count was 1,208 lines across 8
files and covered video and render only. There is a third driver, and leaving it
out of the inventory understated both the surface and the opportunity:

- `src/audio/mini/` is where MI_AO is opened, so **D26** — SDL having no error
  string to report the single-owner audio contention with — lives in this file
  and is fixable here rather than worked around in `launch.sh`.
- It is also **the only reason the bundle needs `libjson-c.so.5`**. The driver
  reads `/appconfigs/system.json` for the initial volume, through four json-c
  calls in one function. That is what makes the bundle's `lib/` come from two
  upstreams ([THIRD-PARTY.md](../../THIRD-PARTY.md)), and it is a link-time
  problem for any rebuild — see the stage 0 tasks.

That is the whole of it. This is not "maintain SDL2" — the other ~200k lines are
upstream and we would not touch them.

A patch set in three tiers. The tiers matter more than the ordering within them,
because they have different justifications and should be decided separately.

**Tier 1 — correctness.** Bugs in code we already ship and run. Nothing here
depends on wanting new capability, and each is small and self-contained.

| # | Change | Fixes | Confidence |
|---|---|---|---|
| 1 | ~~Copy pixels in `Mini_UpdateTexture` into `t->data`~~ **landed 2026-08-02** | §1.1 use-after-free | **done, verified** |
| 2 | Offset the staging copy by `srt.y * pitch` | §1.2 atlas corruption | **certain** |
| 3 | Derive the format from `texture->format`, not `pitch / srt.w` | §1.3 | **certain** |
| 4 | Set `max_texture_*` from `FB_W`/`FB_H` after detection | §1.4, the Flip | **certain** |
| 5 | Drop `TARGETTEXTURE` from the advertised flags | §1.5 | **certain** |
| 6 | Bounds-check `update_texture`'s 100-entry table | silent missing sprites (D27) | **certain** |
| 13 | Advertise `ABGR8888` (`E_MI_GFX_FMT_ABGR8888` is native) | removes a conversion per upload, and §1.1's converting path | high |
| 19 | `MI_SYS_Munmap(gfx.fb.virAddr, FB_SIZE)`, not `TMP_SIZE` | under-unmaps the framebuffer on teardown | **certain** — it is upstream's own later fix (§7.1) |
| 20 | ~~Mirror `dst.y` the way `dst.x` already is~~ **landed 2026-08-02** | sub-rect destinations land vertically mirrored (§8.2) | **done, verified** |
| 21 | ~~Implement `Mini_UpdateWindowFramebuffer`~~ **landed 2026-08-02** | makes SDL's own software renderer work here — §8.5 | **done, verified** — and it retires most of tier 2's justification |
| 10 | Blend mode → `eSrcDfbBldOp`/`eDstDfbBldOp`/`eDFBBlendFlag` | `SDL_SetTextureBlendMode` accepted and ignored — SDL requires all four modes of every renderer | high — **moved from tier 2 2026-08-08**, §8.7 |
| 11 | Colour/alpha mod → `COLORIZE`/`COLORALPHA` + `u32GlobalSrcConstColor` | `SDL_SetTextureColorMod`, `SetAlphaMod` accepted and ignored | medium — the flags are documented, the combination is untried |

**Numbers are append-only** from 2026-07-31 on, because the rest of this document
cross-references them. Item 13 moved up from tier 2 in the revised decision: it
has nothing to do with the state model, and it closes §1.1's converting path.
Item 19 is new, and came from the vendor's own `9eff61a4` rather than from
reading — worth noting as the one tier-1 item this document would not have found
by inspection.

**Tier 2 — conformance.** Make the driver honour SDL2's state model, so ordinary
SDL2 code stops silently lying (§2). This is **one commit, not six** — see §2.3.

**Revised 2026-07-31: this tier is contested, not scheduled.** Everything in it is
also expressible in `gfx::renderer::Layer`, where it serves five targets instead
of one and needs no device to verify. The decision in §5 explains why that is now
a comparison to make in
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) rather
than a plan to execute here.

| # | Change | Restores | Confidence |
|---|---|---|---|
| 7 | Implement `RunCommandQueue` as an execution loop over the command stream | the architecture everything below hangs off | high — the loop is ~60 lines and every other backend is a model for it |
| 8 | `SDL_RENDERCMD_CLEAR` → `MI_GFX_QuickFill` with the queued draw colour | `SDL_RenderClear`, `SDL_SetRenderDrawColor` | high — direct API match |
| 9 | `SDL_RENDERCMD_SETCLIPRECT` → `stClipRect`; `SETVIEWPORT` → a destination offset | `SDL_RenderSetClipRect`, `SetViewport`, `SetLogicalSize` | high |
| ~~10~~ | — | *moved to tier 1, 2026-08-08* — §8.7 | |
| ~~11~~ | — | *moved to tier 1, 2026-08-08* — §8.7 | |
| 12 | `Mini_QueueFillRects` → `MI_GFX_QuickFill` | `SDL_RenderFillRect` | high |
| ~~13~~ | — | *moved to tier 1* | |

> **Items 10 and 11 are not behind item 7 — read 2026-08-05, from the 2.32
> headers rather than from this document's own §2.3.** That section says to
> implement `RunCommandQueue` properly or leave it alone, and the tier is
> presented as one commit. That holds for the state SDL *queues* — clear,
> viewport, clip rect, fill, draw colour — and not for these two.
>
> `SDL_Texture` in `SDL_sysrender.h` carries `blendMode`, `modMode` and `color`
> as plain fields, and there is **no `SetTextureBlendMode` backend hook**: the
> frontend stores them and nothing else. So `Mini_QueueCopy` already has the
> texture in hand and can read all three at draw time, with the command queue
> untouched and draw ordering unaffected. It is about twenty lines, and
> `GFX_Copy` already takes the `alpha` parameter it would use.
>
> Worth recording alongside: `IsSupportedBlendMode` treats `BLENDMODE_BLEND`,
> `ADD`, `MOD` and `MUL` as **required of every renderer** and returns true
> without consulting the backend. The driver cannot honestly refuse them, which
> is why the no-op is undetectable by any conforming program rather than merely
> unimplemented.

**Tier 3 — capability and performance.** Genuinely optional; none of it is needed
by anything the project has today.

| # | Change | Gets | Confidence |
|---|---|---|---|
| 14 | Compose the caller's rotation with the panel's in `QueueCopy`/`QueueCopyEx` | D25, `SDL_RenderCopyEx` for 90° steps and flips | **needs the §6 experiment first** |
| 15 | Set `desktop_mode` in `Mini_VideoInit` | D22, D24 at source | high |
| 16 | Batch blits behind one `MI_GFX_WaitAllDone` instead of waiting per copy | the vendor API is async with fences; the port waits every time | medium — real only once more than one blit per frame exists |
| 17 | `MI_SYS_MMA_Alloc` texture memory, blit without staging | a per-frame `memcpy` | **speculative** — lock/unlock contract and `MI_SYS_FlushInvCache` coherency both need care |
| 18 | `RenderReadPixels` from `t->data` / the mapped framebuffer | `save_screenshot()` without `set_readback()` | medium — plausible, untried |

**One thing that cannot be fixed:** `SDL_SetTextureScaleMode`. `MI_GFX_Opt_t` has
no filter field, so nearest is all there is. The right treatment is to document
it, not to keep accepting the call silently.

---

## 4. What it costs

Stated at full weight, because the line count above makes it look cheaper than it
is. Three of these were written assuming the 2.0.20 base; §5's option E is the
response to them, so each says what survives it.

- **The base is dead — and this was the strongest cost, until it was checked.**
  SDL 2.0.20 is from 2022; upstream is 2.32.x. Owning a fork of an abandoned
  snapshot is a real cost, and this document originally priced the alternative at
  "a much larger job than the line count suggests, because `SDL_sysrender.h` has
  changed". **The header changed; the backend interface barely did** — see §5,
  option E. *Does not survive E.*
- **The build is unproven and awkward.** `steward-fu/sdl2` has `configure.ac` and
  no committed `configure`; the buster toolchain container has no autoconf (nor
  does the host), and this project deliberately does not `apt` against buster's
  archive. So it needs `autoconf` in a modern container and `configure`/`make` in
  the union one. Already recorded in
  [gles-free-runtime § What B would actually involve](../2026-07-29-gles-free-runtime/).
  *Does not survive E, which is a CMake build of a tree this project already
  configures and compiles with this toolchain.*
- **Every change needs a device to verify.** There is no CI for this target and
  no emulator that exercises `MI_GFX`. The verification loop is an SD card.
  **This one is unchanged by every option and is the real recurring cost.**
- **LGPL obligations get heavier**, in the good sense: a modified library means
  publishing the modified source and keeping relinking possible.
  [THIRD-PARTY.md](../../THIRD-PARTY.md) already carries the declaration
  machinery for the one `patchelf` edit; this would extend it, not invent it.
  Note where the obligation actually comes from: `sdl2/LICENSE.txt` is **zlib**,
  and it is steward-fu's ~1,480 driver lines that carry `// LGPL-2.1 License` in
  their headers. So the obligation follows the drivers through any rebase, and
  only a clean-room rewrite of them would change it. Ship `libSDL2` shared,
  as now.
- **`libEGL.so` is resolved, and not by this work.** §1.7: no symbol of it is
  called on our path, so the unknown-licence binary can go behind an eleven-symbol
  stub without a fork at all. A source build drops it outright by removing two
  `-l` flags. *This stops being a fork argument and becomes a
  [gles-free-runtime](../2026-07-29-gles-free-runtime/) task.*

---

## 5. Options

| | Approach | Gets | Costs | |
|---|---|---|---|---|
| **A** | Ship the prebuilt, compose in `Layer` | works today | the bugs in §1 stay; no Flip | status quo |
| **B** | Fork 2.0.20 and maintain | everything in §3 | §4, in full | |
| **C** | Send the patches to [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo), adopt their build | same fixes, someone else maintains | their timeline, their driver name (`mmiyoo`), a runtime swap with its own device trip | rejected |
| **D** | **Prove the build; decide after** | the gate every other option needs | a container and an afternoon | the gate |
| **E** | **Rebase the drivers onto the pinned SDL 2.32.10** | everything in §3, on a live base, built by machinery we already have | one device run that tests base and build together | **chosen** |

### 5.1 Option E, and why the reasoning that excluded it did not hold

The first version of this section rejected rebasing in one sentence:
`SDL_sysrender.h` has changed since 2.0.20, so it is a port rather than a rebase.
The header has indeed changed — by 132 lines. **The interface a backend actually
implements has not.**

Method: enumerate every `device->`, `renderer->` and `impl->` member the three
mini drivers assign, then look each one up in 2.32.10's `SDL_sysvideo.h`,
`SDL_sysrender.h` and `SDL_sysaudio.h`. All 50 still exist. Ignoring the
whitespace of upstream's 2.24-era formatting pass, **three have changed
signature**:

| Member | 2.0.20 | 2.32.10 |
|---|---|---|
| `RenderPresent` | `void (*)(SDL_Renderer*)` | `int (*)(SDL_Renderer*)` |
| `SDL_RenderDriver.CreateRenderer` | `SDL_Renderer *(*)(SDL_Window*, Uint32)` | `int (*)(SDL_Renderer*, SDL_Window*, Uint32)` |
| audio `OpenDevice` | `(_THIS, void*, const char*, int)` | `(_THIS, const char*)` |

Registering a driver in the CMake build is a seven-line block modelled on
`SDL_OFFSCREEN`, one `#cmakedefine` in `SDL_config.h.cmake`, and one entry each
in the `bootstrap[]` / `render_drivers[]` arrays of `SDL_video.c`,
`SDL_render.c` and `SDL_audio.c`.

**And the base build is the half already proven.** `cmake --preset miyoomini`
fetches and builds SDL 2.32.10 with the union toolchain's GCC 8.3 today — that
build is where [TARGETS.md](../../docs/TARGETS.md)'s "upstream SDL2 has no video
driver that can reach that panel" evidence comes from. Option B's build is the
half that has never been run.

What E buys beyond the §3 patch set, none of which B gets:

- **No autoconf, no 2.3 GB clone, no SwiftShader.** The vendor tree's own
  `Makefile` symlinks SwiftShader's `libEGL.so`/`libGLESv2.so` into `sdl2/` to
  satisfy the hardcoded `-lEGL -lGLESv2` (§1.7). Remove those two flags and the
  GPU half of that repository is never needed.
- **One SDL2 version across all five targets**, which is the stated point of
  [§ Pinned dependencies](../../docs/TARGETS.md) and the one target it does not
  currently hold for.
- **It deletes `WREEL_SDL2_ROOT` and the hand-assembled prefix.** Today
  `miyoomini` compiles against SDL2 headers of unrecorded provenance and links a
  2.0.20 runtime; the symbol check in TARGETS.md verifies the *symbols* resolve,
  not that the headers match. A source build emits headers and library together.
  **This is the strongest argument in the whole document and it is not about
  capability at all** — it is the one thing no engine-side mitigation can fix.
- **The satellites stop straddling versions.** SDL2_image, SDL2_ttf and
  SDL2_mixer are pinned at releases built against 2.32 and are statically linked
  into a process whose `libSDL2` is 2.0.20.

E's honest cost: **it gives up the clean control experiment.** B's stage 0 —
rebuild unpatched, confirm identical behaviour — isolates the build pipeline from
every later change. E's equivalent run tests a new base and a new build at once.
Accepted, because the base is the part with independent evidence behind it and
the vendor autoconf build is the part with none.

### 5.2 The maintenance mechanism: not a vendored tree, and not a GitHub fork

Separate question from *which base*, and it has its own answer.

| Mechanism | Verdict |
|---|---|
| Copy the SDL tree into this repository | **no** — ~200k lines of upstream in a deliberately curated tree, to own 1,480 of them. Nothing requires it: LGPL source availability is discharged by a pin plus published patches |
| A GitHub fork of `steward-fu/sdl2` | **only if the answer had been B.** It is the right home for patches on a 2.0.20 base — real commits on upstream history, a URL that discharges the licence obligation by itself, and the artefact to hand XK9274 later at no cost. It also inherits the 2.3 GB, the autoconf build and the dead base |
| Patch series against a fetched pin | workable with either base, and the shape B should take if E fails stage 0: the ~10 patches are the entire intellectual content and stay reviewable in *this* repository's history |
| **Driver sources in-tree, upstream fetched and patched** | **chosen.** The 1,480 driver lines live here as source we own and format; upstream SDL 2.32.10 keeps arriving through `FetchContent` at its existing pin, with a small patch registering the drivers in its CMake build |

The last row is what "vendored fork" should have meant: **we vendor the drivers,
not SDL2.** The drivers are the part with our changes in it, the part that needs
review, and the part the licence obligation attaches to. Upstream stays a pin
like the other eight dependencies.

### Decision, revised 2026-07-31: D as a gate, then E for tier 1

**Taken rather than proposed.** What changed from the first decision, and what
did not.

**Unchanged: tier 1 is worth doing.** Those are defects in a binary we ship.
§1.1 is a use-after-free on our own `draw_surface` path, §1.2 and §1.3 mean
`Atlas`, `AnimatedSprite` and `TileMap` cannot render correctly on this device
however the engine is written, and §1.4 leaves the Flip with no working
configuration.

**Changed: E rather than B**, on §5.1 — and structured as §5.2 rather than as a
fork of someone's repository.

**Changed: tier 2 is no longer decided here.** The first decision took it on the
grounds that an atlas rendering correctly but opaque is no more useful than one
rendering as noise. That is still true, and it is an argument for *the capability
existing*, not for where it lives. Blend, colour mod and alpha mod implemented in
`gfx::renderer::Layer` work on all five targets, are testable on the desk without
an SD card, and are needed by
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) anyway on
the reasoning [MIYOO-MINI § 8](../../docs/MIYOO-MINI.md) already records —
sprites composite into a layer here regardless. Implemented in the driver they
work on one target and get written twice.

So tier 2 is **contested, not scheduled**, and the comparison belongs in the
sprites snapshot rather than this one. Item 13 (advertise `ABGR8888`) is the
exception and moves to tier 1: it removes a conversion per upload and closes
§1.1's converting path, and it has nothing to do with the state model.

**Tier 3 is not in scope**, unchanged, and specifically not the exciting parts.
Item 14 stays in view because D25 is live, and waits on §6 either way.

**Everything is still gated on D.** If the drivers cannot be built into a working
library, E is void, B is void, and A is the answer — in which case the
mitigations below are the whole plan.

**C is rejected**, unchanged. Sending patches to an external project uninvited
spends our time on someone else's timeline, and adopting their build means a
runtime swap (`mmiyoo` rather than `mini`, different texture cap, different bugs)
that costs a device trip on its own. If XK9274 asks, revisit — the patches will
exist by then and offering them costs nothing at that point. Note that E makes
this *less* portable to them, not more: patches against 2.32 do not apply to a
2.0.20 tree. That is a real cost of E and it is accepted, because C is rejected
on its own terms rather than being kept warm.

### If the answer stays A

The engine-side mitigations, none of which need a fork:

- Keep composing into `Layer` and keep `set_readback()` for screenshots. Both
  already exist and both already work.
- **Treat `Context::draw()` with a source rect as unavailable on this target**,
  which D25 already says and §1.2/§1.3 now independently confirm.
- Avoid `SDL_CreateTextureFromSurface` on this target, or re-upload every frame
  so the borrowed pointer is never stale. §1.1 makes this a live hazard rather
  than a theoretical one.
- The Flip stays unsupported, and that should be said plainly rather than left
  as a panel we intend to reach.

---

## 6. The experiment that should happen first — ANSWERED 2026-08-01

**Is the fixed `E_MI_GFX_ROTATE_180` a bug or correct panel compensation?**

> **Compensation.** The panel is mounted inverted. `wreel-diag` established that
> the framebuffer holds the image rotated 180°, and the ambiguity that leaves —
> a rotated framebuffer displays upright on an inverted panel — was closed by
> looking at the screen: `coppers`' HUD text reads left-to-right from the top
> left. **Item 14 must compose with the rotation, not remove it.**
>
> The rest of this section is kept as written, because the second axis it
> identified turned out to be the real defect and the reasoning is what found it
> — see §8.2 and item 20.

Nothing recorded so far distinguishes them, because `coppers`' full-screen
content is a field of horizontal bars that looks the same rotated. One run
settles it:

```sh
./launch.sh --screenshot frame.bmp     # with visibly asymmetric full-screen content
```

If the panel shows the content upright, the rotation is compensation and patch
item 6 must *compose* with it. If inverted, it is a bug and item 6 removes it.
Getting this backwards would turn a working full-screen path into a broken one,
which is the single most expensive mistake available in this snapshot.

`coppers` currently has no asymmetric mode; the cheapest vehicle is probably a
one-off test image rather than a demo change.

**A second axis, found 2026-07-31 in the same function.** `Mini_QueueCopy`
mirrors the *destination placement* horizontally before handing it over:

```c
dst.x = (vid_win->w - (dstrect->x + dstrect->w)) * scale;
dst.y = dstrect->y * scale;                       /* y is not mirrored */
```

For a full-screen `dstrect` that reduces to `dst.x = 0`, so **the path this
project actually uses cannot observe it** — which is a second reason the current
runtime tells us nothing about either transform. The two together (content
rotated 180°, destination mirrored in x but not y) are consistent with
compensating a panel mounted upside down while keeping a left-handed destination
axis, and that is a guess. The experiment above should therefore use content that
is asymmetric in **both** axes, and a `dstrect` that is not full-screen, or it
will answer half the question.

---

## 7. Research setup

The port is checked out at **`/home/wreel/Source/sdl2-steward-fu`**, beside
`union-miyoomini-toolchain`, deliberately outside this repository. Head
`0631abc8` (2026-04-19), 108 commits, 2.3 GB — it vendors SwiftShader and the
prebuilt binaries as well as the source.

**This should have been done before any of the earlier conclusions were drawn.**
Everything this project recorded about the port before 2026-07-31 came from
reading files through a web view, which summarises rather than reproducing — and
every finding in §1 is invisible unless you have the exact bytes. The vendor SDK
is likewise readable only inside the toolchain image:

```sh
docker run --rm wreel-miyoomini \
  cat /opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/mi_gfx.h
```

The pin that matters for the shipped binary is commit `68ce3172`
([THIRD-PARTY.md](../../THIRD-PARTY.md)); note that it is the last commit to
touch `prebuilt/640x480/`.

### 7.1 Correspondence: partly established, and it moves the pin

This document originally recorded the binary-to-source correspondence as
"assumed, not verified". Two checks, both cheap, done 2026-07-31:

**The shipped binary does contain the Flip detection.** §1.4's runtime probe is a
`popen`, so it leaves its command line in the binary:

```console
$ strings -a prebuilt/640x480/libSDL2-2.0.so.0 | grep fbset
fbset | grep "mode "
```

That is the blob `7dba96fb` THIRD-PARTY.md pins. So §1.4 is a statement about the
library we ship, not only about the source — the framebuffer adapts to a Flip and
the texture cap does not.

**But the drivers moved one commit after the pin, and that commit is not
cosmetic.** Exactly one commit touches `src/{video,render,audio}/mini` after
`68ce3172`:

```console
$ git log --oneline 68ce3172..HEAD -- sdl2/src/video/mini sdl2/src/render/mini sdl2/src/audio/mini
9eff61a4 Fixed 320x2480 resolution issue.
```

Committed two minutes after the pin, and it **changes the `eglUpdateBufferSettings`
ABI** — a sixth parameter, with SwiftShader's `libEGL` rebuilt in the same commit
to match:

```c
-EGLBoolean eglUpdateBufferSettings(EGLDisplay, EGLSurface, void *, void *, void *);
+EGLBoolean eglUpdateBufferSettings(EGLDisplay, EGLSurface, void *, void *, void *, int);
```

So **building from head against the pinned `libEGL.so` is an ABI mismatch**,
harmless only because §1.7 shows the call is never reached. It also fixes a
`MI_SYS_Munmap(gfx.fb.virAddr, TMP_SIZE)` that should have been `FB_SIZE` — an
under-unmap of the framebuffer, on the teardown path, present in the binary we
ship.

**Consequence for stage 0: build at `68ce3172`, not at head**, and treat the
delta to `9eff61a4` as the first patch to consider rather than as part of the
control.

---

## 8. Stage 0 on hardware — what it settled, 2026-08-01

The build landed as §5.2 described: driver sources in
[platform/miyoomini/sdl2/](../../platform/miyoomini/sdl2/), upstream fetched at
its existing pin and patched with 46 additive lines across 8 files, one
`WREEL_MINI_SDL2` option, `WREEL_SDL2_LINKAGE` forced to SHARED for the LGPL
reason. Provenance and the complete list of modifications are in that
directory's `PROVENANCE.md`.

### 8.1 The gate

`coppers`, unmodified, against the rebuilt library:

```
[I] gfx system initialised, video driver Mini
[I] renderer context 640x480 via Miyoo Mini (accelerated)
[I] audio: 22050 Hz, 2 ch, 2048 sample buffer, 8 voices, driver Miyoo Mini
[I] coppers: 859 frames, 59.7 fps, plot 2.920 blit 4.425 present 9.348 ms
```

**The drivers behave the same on a 2.32 base as on 2.0.20.** That is what §5.1
predicted from the signature survey and what §4 said would be the risk of taking
E over B, and it is now measured rather than argued.

Two things fall out of the loader trace:

```
libSDL2-2.0.so.0 => /mnt/SDCARD/App/Coppers/lib/libSDL2-2.0.so.0
libmi_gfx.so     => /config/lib/libmi_gfx.so
libmi_ao.so, libmi_sys.so, libmi_common.so, libshmvar.so
```

- **No `libEGL.so`, no `libGLESv2.so`, no `libjson-c.so.5`.** §1.7 and §3's
  json-c removal, realised. The bundle's `lib/` is one file, and
  [THIRD-PARTY.md](../../THIRD-PARTY.md)'s unknown-licence item is gone rather
  than mitigated.
- **The MI libraries resolve from `/config/lib`**, not `/customer/lib`, which is
  the only path `launch.sh` adds. `/config/lib` is already on the default search
  path on this firmware.

Audio also opened and played three tracks **without the driver touching the
system volume**, which is the removal recorded in `PROVENANCE.md` and was the
one part of it that could not be argued from source.

### 8.2 The rotation question, closed

§6 asked whether the fixed `E_MI_GFX_ROTATE_180` is a bug or panel compensation,
and called getting it backwards the most expensive mistake available here.

`wreel-diag` drew four coloured quadrants and read `/dev/fb0`: the framebuffer
holds the image **rotated 180°**. That is still ambiguous on its own — a rotated
framebuffer displays upright on an inverted panel — so it was closed by looking
at the screen. `coppers`' HUD text reads left-to-right from the top left.

**The panel is mounted inverted and the rotation compensates for it.** Item 14
must compose with the rotation; removing it would break the only path that
works. D25 is corrected accordingly.

And the correction turns §6's second axis into a defect with a one-line fix. For
the viewer to see content at `dstrect`, the framebuffer rect must be mirrored in
*both* axes. `Mini_QueueCopy` mirrors x and not y:

```c
dst.x = (vid_win->w - (dstrect->x + dstrect->w)) * scale;   /* correct */
dst.y = dstrect->y * scale;                                 /* missing the same */
```

Invisible full-screen, wrong for every sprite. **New tier 1 item 20.**

**Confirmed 2026-08-01**, and the arithmetic held exactly. A 160×120 block asked
for at y=60 on a 480-tall panel landed at y=300, which is `480 − 60 − 120`; x was
correct. The check reports it as "x is right and y is mirrored", which is the
shape the derivation predicted rather than a general "it is in the wrong
place".

### 8.3 What the diagnostics measured

Complete after the third run, 2026-08-01. **Every §1 and §2 finding that could be
reached from user code is now measured rather than read, and none of them
surprised.**

| | Verdict | |
|---|---|---|
| `SDL_UpdateTexture` copies | **WRONG** | keeps the caller's pointer — §1.1 / D27, proven directly, and it crashed the tool before it was written to survive it |
| `SDL_RenderClear` | **IGNORED** | "screen still holds the previous frame; the clear was queued and never executed" — §2 exactly |
| `SDL_RenderCopy` full-screen | **OK** | the one operation that works, and the whole reason `Layer` is the house pattern here |
| `SDL_RenderCopy` sub-rect | **WRONG** | read *past the staged rows* and returned the previous blit's leftovers — §1.2, and the magenta poison is what proves it rather than a lucky colour match |
| sub-rect, RGB565 | **WRONG** | `pitch/rect.w` is 16, the driver reads that as bytes-per-pixel — §1.3 |
| partial destination | **WRONG** | requested y=60, landed at y=300 on a 480-tall panel. `480 − 60 − 120 = 300` — §8.2's arithmetic, to the pixel |
| `SDL_SetTextureBlendMode` | **IGNORED** | sprite opaque over red; mode accepted and never applied |
| `SDL_SetTextureColorMod` | **IGNORED** | no tints, fades or damage flashes |
| `SDL_RenderFillRect` | **IGNORED** | success, nothing drawn |
| render to texture | **IGNORED** | "the target texture is empty… and TARGETTEXTURE is advertised anyway" — §1.5 |
| texture cap | 640×480 advertised **and enforced** | over-limit correctly refused |
| desktop mode, display bounds | **success, all zeros** | D22/D24, on our build too — item 15 is unfixed |
| audio | **OK** | 22050 Hz once `audioserver` is stopped |

Two of these needed the checks rewritten before they were worth anything —
`SDL_RenderClear` and the sub-rect — and both had reported the *opposite* answer
first. §8.4 is why.

Incidental, from the same runs: the framebuffer's virtual height was 960 on one
run and 1440 on another. `Mini_InitGFX` sets `yres_virtual = yres * 2`, and this
tool probes `/dev/fb0` before `SDL_Init`, so what it reports there is the
firmware's own buffering rather than SDL's. Worth knowing before reading anything
into it.

### 8.4 What the tool got wrong, which is worth more than the table

Three of the first run's verdicts were the diagnostic's fault, not the driver's,
and each is a trap that will catch the next person testing on this device.

- **A window sized from the driver's mode list is 800×600 on a 640×480 panel.**
  `SDL_GetDesktopDisplayMode` returns *success* with a zeroed mode, and SDL sorts
  the driver's ten fixed modes largest-first. Every full-size texture was then
  refused by the 640×480 cap. `gfx::renderer::Context` probes four sources for
  exactly this reason; the tool now reads `/dev/fb0`.
- **A no-op `SDL_RenderClear` cannot be measured across two presents.** Present
  is an fbdev pan between halves of a 640×960 framebuffer, so the second present
  flips to the frame from *two* presents ago. Background and operation have to
  be in one frame.
- **A sub-rect check can pass by accident.** §1.2 says the blitter reads whatever
  the staging buffer last held; the previous check had filled staging with the
  same green the check expected, so it reported OK about stale memory. The
  staging buffer has to be poisoned with a colour that appears nowhere else.

A fourth was a check that tested nothing: `SDL_GetRenderTarget` returns the
frontend's own bookkeeping, so it reports success whatever
`Mini_SetRenderTarget` does.

**The transferable part:** on this driver, a check that does not read the
framebuffer back is measuring its own assumptions, and a check that reads it
back can still be measuring the previous check's leftovers.

### 8.5 The item that changes tier 2

`Mini_UpdateWindowFramebuffer` is `return 0` — recorded in D25 on 2026-07-28,
with a device measurement of 965 correct frames, a 4 µs present and a black
panel. That was filed as architectural because the library was not ours.

**It is ours now**, and implementing it is about ten lines: `GFX_Copy` the window
surface, `GFX_Flip`. It makes SDL's own software renderer work here — the
reference implementation of correct sub-rectangles, blend modes, colour
modulation, fills and render-to-texture — at the CPU cost this project already
pays for `Layer`, with `MI_GFX` still carrying the full-screen blit.

That is one function against tier 2's six items, and it is why tier 2 stays
contested rather than scheduled. **New item 21**, and the first thing to try.

> **Tried, 2026-08-02, and it works.** Present 9.487 ms against the 4 µs of the
> `return 0`, an upright picture with legible text, and `wreel-diag` returning
> **OK on every conformance check** — clear, blend, colour mod, fill and
> render-to-texture included. `coppers` drew its HUD through per-glyph sub-rect
> copies at frame rate with no `_layer_only` workaround.
>
> Caveat that must travel with that table: under the software renderer the tool
> reads back through `SDL_RenderReadPixels` rather than `/dev/fb0`, so it
> measured what SDL composited, not what reached the panel. The panel evidence is
> the present cost and the demo run. Item 18 (`RenderReadPixels` on the `mini`
> backend) or a `--readback fb0` flag would remove the asymmetry.

---

## Tasks

**Stage 0 — the gate. DONE 2026-08-01.** Kept in full rather than deleted,
because what each item turned into is the useful part.

- [x] **Settle the json-c link.** Taken further than this task proposed: the
      whole system-volume mechanism went, not just its dependency. Upstream read
      `/appconfigs/system.json` on every open and wrote the result to the
      *system* volume through an ioctl on `/dev/mi_ao` — a different knob from
      the `MI_AO_SetVolume` that stays. Re-asserting a level the firmware
      already holds is not an audio backend's job, and its fallback of `0` drove
      the level to `MIN_RAW_VALUE` and muted the device. An intermediate design
      that had the host read the file and pass `SDL_MINI_VOLUME` was built and
      withdrawn: it moved the coupling instead of removing it
- [x] Add `src/{video,render,audio}/mini` to the pinned SDL 2.32.10 build. 42
      inserted lines across 8 upstream files, nothing removed or changed, applied
      by `graft.cmake` from FetchContent's `PATCH_COMMAND`
- [x] ~~Fix the three changed signatures~~ **Six, not three, and the sixth was
      found by the compiler rather than by the survey.** `QueueCopyEx` gained
      `scale_x, scale_y`; `AudioBootStrap.init` went `int` to `SDL_bool`;
      `VideoBootStrap.create` lost its `devindex`. §5.1's method — grep the
      member names, diff the declarations — missed the first because its
      declaration spans two lines and the second and third because they are not
      assigned through a `->`. Plus one real bug rather than a signature: from
      2.24 the frontend owns the `SDL_Renderer` allocation, so upstream's
      `SDL_free(renderer)` in `Mini_DestroyRenderer` would have been a double
      free
- [x] Compile it. The SDK headers needed no work, as predicted — sysroot and
      vendor copies are byte-identical. The link needed `-lmi_ao`, which the
      first patch omitted
- [x] Compare exported symbols against the shipped prebuilt's. **839 against
      798, a strict superset**; nothing the prebuilt exports is missing, and all
      176 SDL-family symbols our binaries import resolve
- [x] Run on the device unmodified. **The gate: passed** — §8.1
- [x] Drop `-lEGL -lGLESv2` and `SDL_gles_mini.c` (§1.7). Confirmed on the
      device: `lib/` is one file and the loader maps no EGL and no GL

**What stage 0 cost that was not on the list:** two device runs of the
diagnostics tool before its own results could be trusted, for the reasons in
§8.4. The library was right on the first run; the instrument was not.

**Stage 0-B — the fallback control**, only if stage 0 fails

- [ ] `autoconf` the vendor tree **at `68ce3172`, not head** (§7.1) in a modern
      container — `autogen.sh` needs `autoconf` only, no automake and no libtool
      — then `configure`/`make` in `wreel-miyoomini`, and produce a
      `libSDL2-2.0.so.0`
- [ ] Run it unmodified. An unpatched 2.0.20 rebuild that behaves identically to
      the prebuilt separates "the drivers do not build here" from "2.32 is the
      problem", which is the one thing stage 0 cannot tell you on its own

**Stage 1 — tier 1, the correctness patches.** Stage 0 passed, so this is next.
Ordered, because the order is not obvious and two items were added after the
device runs.

> **Items 1, 21 and 20 landed 2026-08-02**, in one device trip as planned, and
> the diff against the 2026-08-01 baseline moved exactly the two verdicts they
> aimed at. Full record in
> [target-validation/results.md](../2026-07-25-target-validation/results.md).
> The rest of stage 1 is unstarted.

- [x] **Item 1 — copy the pixels in `Mini_UpdateTexture`.** Done 2026-08-02.
      `SDL_UpdateTexture copies` WRONG → OK on the device. **D27 withdrawn** and
      `Context::draw_surface()` is usable here. One thing the task did not
      anticipate: `Mini_UnlockTexture` fed `t->data` back through
      `Mini_UpdateTexture`, which after the fix is a `memcpy` onto itself, so it
      registers the buffer directly instead
- [x] **Item 21 — implement `Mini_UpdateWindowFramebuffer`.** Done 2026-08-02,
      and it is the item that pays. `SDL_RENDER_DRIVER=software` gives an upright
      picture with legible text, a present of 9.487 ms against the 4 µs of the
      `return 0`, and **every `wreel-diag` conformance check OK**. Nearer 30
      lines than ten: the staging buffer is one panel's worth of pixels and this
      driver advertises modes larger than the panel, so both bounds are real
      rather than defensive
- [x] **Item 20 — mirror `dst.y`.** Done 2026-08-02. `partial destination`
      WRONG → OK, framebuffer box at y=300 un-rotating to the requested y=60
- [ ] **Items 2, 3, 10 and 11 — the source rect's `y`, the format inference, and
      blending.** One device trip: all four are in `Mini_QueueCopy` and
      `GFX_Copy`, and each has its own verdict — `SDL_RenderCopy sub-rect`,
      `sub-rect, RGB565`, `SDL_SetTextureBlendMode`, `SDL_SetTextureColorMod`.
      2 and 3 are the atlas blockers; 10 and 11 move up from tier 2 on the
      reasoning in §8.7, and item 10 carries the free guard that `coppers` must
      be unchanged. Item 11 lands separately, being the one with medium
      confidence
- [ ] **Items 5, 6, 13 and 19** — drop `TARGETTEXTURE` from the advertised
      flags, bounds-check `update_texture`'s 100-entry table, advertise
      `ABGR8888`, and the `MI_SYS_Munmap` size from `9eff61a4` (§7.1). Small,
      self-contained, no capability change
- [ ] **Item 4 — `max_texture_*` from `FB_W`/`FB_H`.** Correct, and
      **unverifiable without a Mini Flip in hand**. Land it saying so rather
      than implying it was tested
- [x] Revise D25 to whatever survives, and withdraw D27 when item 1 lands. Done
      2026-08-02, and revisited 2026-08-05 when §8.6 found two of D25's
      consequence bullets were wrong about *why* they were true

**How each one is verified.** Run `wreel-diag` before and after and diff the two
reports — it is a regression suite, not just a survey, and § 8.4 is the record of
how easily it produced confident wrong answers before its own checks were right.
A patch that changes a verdict it was not aiming at is the interesting case.

**One device trip can carry items 1, 21 and 20** — they are independent and have
separate checks. Item 1 still wants to be its own commit.

**Stage 2 — tier 2, and it is a comparison rather than a task list**

Not scheduled here any more; the decision above moved it. What stage 2 owes
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) is the
comparison, made once with tier 1 landed:

- [ ] Blend, colour mod and alpha mod in `gfx::renderer::Layer` — five targets,
      no device loop, needed by that snapshot anyway — against items 7–12 in the
      driver, one target, device loop, and correct for any SDL2 program rather
      than only ours

> **Item 21 added a third option and weakened the second, 2026-08-02.** SDL's own
> software renderer now works on this device and returns OK on every conformance
> check, so the capability tier 2 was to build already exists here — through code
> shared with the other four targets rather than written for this one. Measured
> cost against the `mini` backend, same device, same demo: blit 6.551 ms against
> 3.977, present 9.487 against 10.680, frame rate identical at the demo's cap.
>
> That does not decide it. `Layer` composites once and blits full-screen through
> `MI_GFX`, which the software renderer does not, and the phases those numbers
> came from are not aligned. But **items 7–12 now have to beat a working
> implementation rather than an absent one**, and that is a materially harder
> case than the one this section was written to frame.
- [ ] If the driver wins, item 7 goes first and alone: `RunCommandQueue` as a
      real execution loop with `QueueCopy`'s behaviour moved into it unchanged.
      **The device run after that commit must look identical to the one before
      it** — §2.3 is why, and it is what makes 8–12 safe to add

### 8.6 The comparison, worked through — 2026-08-05

Prompted by a smaller question: `coppers` forces its HUD into the layer on this
driver (`Demo::_layer_only`), and whether that can go is the same decision in
miniature. Two things had to be corrected before the options were even right.

**Blending in `Layer` is not one of the options.** `_layer_only` chooses between
*plotting the HUD into the layer* and *drawing it as a texture over the layer*.
Blending inside `Layer` only helps what is already plotted. Removing the branch
needs the **renderer** to blend. `Layer`-side blending is still wanted by
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/); it is
not a route here.

**And the reason the HUD needs it is blend, not sub-rectangles.** `draw_hud()`
calls `draw(texture, nullptr, &target)` — a *full* source rect with a partial
destination — so items 2 and 3 never bore on it. What did were item 1 (the
upload) and item 20 (the destination mirror), both landed. What remains is that
`draw_text` rasterises with `TTF_RenderUTF8_Blended` and uploads through
`SDL_CreateTextureFromSurface`, which sets `SDL_BLENDMODE_BLEND` — so the text's
whole rectangle would arrive opaque over the copper bars.

#### A — blend in the `mini` backend, items 10 and 11

| | |
|---|---|
| Cost | ~20 lines, one device trip. **Not** the queue rework — see the note under tier 2 |
| Unlocks | alpha sprites, tints, fades, on the accelerated path |
| Leaves | items 2, 3 and 4 — an atlas still cannot blit correctly, and is still capped at 640×480 |

- **The only option that keeps `MI_GFX` in the drawing path.** Item 16's batching
  is meaningless unless sprites reach the blitter at all, so B forecloses in
  practice what A keeps open.
- **The only option that removes `_layer_only` without changing what `coppers`
  measures** — see the trap under B.
- Fixes it for any SDL2 program on the device, which matters only if these
  drivers are ever offered outward. §5 rejects that for now.
- Grows the surface we maintain and the LGPL obligation follows it, and every
  change costs an SD card. Item 11's confidence stays medium: the `MI_GFX` flags
  are documented, the combination is untried.

#### B — `Driver::Software` on this target

| | |
|---|---|
| Cost | **nothing to build.** [context.hpp](../../include/gfx/renderer/context.hpp)'s `Driver::Software` already exists, and the `mini` backend advertises `ACCELERATED` so it will not match |
| Unlocks | everything, measured: 11/11 conformance, sub-rects, blend, colour mod, fills, render-to-texture |
| Costs | compositing moves to two Cortex-A7 cores — blit 6.551 ms against 3.977, before any sprites exist |

- **It removes four driver items from the sprites module's critical path** —
  2, 3, 10, 11 — and the texture cap with them. The software renderer advertises
  `max 0x0`, unlimited, against the backend's 640×480. That also disposes of
  item 4 and the Mini Flip, on the one target where the cap is wrong and cannot
  be tested.
- **The device loop mostly disappears for engine work.** `desktop-software`
  exercises the identical renderer, so correctness becomes a desk question and
  the SD card is for integration.
- **Retires `Layer::set_readback()` as a necessity.** `SDL_RenderReadPixels`
  works there, so `save_screenshot()` needs no workaround — D25's 2026-07-28
  mitigation becomes optional rather than load-bearing.
- **It changes what `coppers` is, and that is the trap.** The demo exists to
  measure the hardware scale-and-blit; under the software renderer the
  layer-to-window scale is a CPU blit and the comparison is gone. Present stays
  hardware either way, because item 21 blits the window surface through
  `GFX_Copy`. **Switching the engine default and switching the demo default are
  not the same decision and should not be taken together.**
- Leaves the `mini` backend unused by default, which invites rot — though it
  also stays the control every conformance diff is taken against.

#### C — plot the HUD into the layer everywhere

Delete `draw_hud()` and the branch outright. Zero dependencies, and it is the
only option available today. Rejected: it breaks decision 2 of the
[coppers snapshot](../2026-07-26-coppers-cracktro/) deliberately — the HUD would
scale with the layer, so at `--layer-height 240` the instrument moves with the
thing it measures. Recorded because "just always use the layer" is the obvious
question.

#### The split

They are not exclusive, and B costs nothing today. Divide by what the code is
*for* rather than by target:

| | Engine and sprites work | `coppers` |
|---|---|---|
| Renderer | **B** — `Driver::Software` | stay on `mini` |
| Why | every capability now, desk-testable, no texture cap | keep measuring the hardware path |
| `_layer_only` | not applicable — blending works | **stays until A lands** |

So B unblocks the sprites module without a single driver item, and A becomes
worth doing on its own merits rather than as a blocker — cheaper than this
document priced it, and the only thing that retires `_layer_only` while leaving
`coppers` measuring what it was built to measure.

**What to avoid:** making B the global default and calling `_layer_only` solved.
That trades a visible workaround for an invisible 2.6 ms and quietly retires the
only instrument pointed at the hardware path.

### 8.7 The split, weakened by its own premise — 2026-08-08

§8.6 priced A as a thing you do *instead of* the atlas work, and that was wrong.
Items 2, 3, 10 and 11 are all in `Mini_QueueCopy` and `GFX_Copy` — the same two
functions, one device trip, and four separate `wreel-diag` verdicts to guard
them. Together they give the `mini` backend correct source rectangles *and*
blending, which is the whole of what `Atlas`, `AnimatedSprite` and `TileMap`
need from a renderer.

That leaves B with a narrower and more honest case than §8.6 gave it:

- **Desk-testability**, which is untouched by any of this and is the strongest
  single argument — engine correctness verified on `desktop-software` instead of
  through an SD card.
- **No texture cap**, which matters for an atlas larger than the panel and for
  the Flip.

Everything else B was buying, A now buys with the blitter doing the work instead
of two Cortex-A7 cores.

#### Why item 10 is arguably tier 1, not tier 2

Read from `mi_gfx_datatype.h` in the toolchain sysroot rather than from
[MIYOO-MINI.md § 4.6](../../docs/MIYOO-MINI.md), and it reframes the item.

**`GFX_Copy`'s hardcode is exactly `SDL_BLENDMODE_NONE`**: `eSrcDfbBldOp =
BLD_ONE` with `eDstDfbBldOp = 0`, which is `BLD_ZERO`, gives `src*1 + dst*0`. So
the driver does not have blending switched off — it implements **one of the four
modes SDL requires of every renderer** and silently substitutes it for the other
three. With `IsSupportedBlendMode` returning true without consulting the
backend, no conforming program can detect the substitution.

That is the same shape as item 1: code that lies to its caller. Tier 1 is
defined here as "bugs in code we already ship and run", and this qualifies. It
was sorted into tier 2 because the original reading framed it as a missing
capability.

All four modes map onto operands the header already has:

| SDL mode | `eSrcDfbBldOp` | `eDstDfbBldOp` | `eDFBBlendFlag` |
|---|---|---|---|
| `NONE` | `BLD_ONE` | `BLD_ZERO` | — *(today's hardcode)* |
| `BLEND` | `BLD_SRCALPHA` | `BLD_INVSRCALPHA` | `ALPHACHANNEL` |
| `ADD` | `BLD_SRCALPHA` | `BLD_ONE` | `ALPHACHANNEL` |
| `MOD` | `BLD_DESTCOLOR` | `BLD_ZERO` | — |
| `MUL` | `BLD_DESTCOLOR` | `BLD_INVSRCALPHA` | `ALPHACHANNEL` |

with `E_MI_GFX_DFB_BLEND_COLORIZE` and `COLORALPHA` for item 11's modulation.

#### The guard, which is free

`gfx::renderer::Layer` sets `SDL_BLENDMODE_NONE` deliberately, so a correct
mapping produces byte-identical operands on the path `coppers` actually uses.
**The device run after item 10 must look exactly like the one before it** — the
same cheap check §2.3 demands for item 7, available here without the queue
rework.

#### What is still unknown

- **Whether blending composes with the fixed `E_MI_GFX_ROTATE_180` in one
  `BitBlit`.** Both live in `MI_GFX_Opt_t` and nothing suggests they interact,
  but nothing has exercised them together either — and §6 is this project's
  standing reminder about assuming how that rotation behaves.
- **Item 11's confidence stays medium.** `COLORIZE`/`COLORALPHA` with a global
  const colour is documented and untried, which is why it lands as its own
  commit after item 10 rather than with it.

**Stage 3 — reconsider tier 3**

- [ ] The §6 experiment, in both axes, then item 14 if it says so
- [ ] Revisit 15–18 with stage 1's device evidence in hand

**Not scheduled, and deliberately so**

- Tier 3 items 16–18. Recorded so they are not forgotten, not because they are
  proposed.
- A GitHub fork of `steward-fu/sdl2`. §5.2 — it is the right mechanism for a
  2.0.20 base and we are not taking that base. Kept here because "shouldn't this
  be a fork?" is the obvious question and deserves a standing answer.
- Offering the patches upstream. Rejected in §5 as an uninvited cost, and made
  harder by E; revisit only if asked.

---

## Risks

**Patching a driver we cannot test except on hardware.** Mitigated only by
sequencing. **Option E weakens this mitigation and that is its main risk**: its
stage 0 changes the base and the build together, so the first device run has two
possible causes of failure rather than one. Stage 0-B exists to localise it after
the fact, at the cost of a second device trip.

**The 2.32 render frontend may not drive this backend the way 2.0.20 did.** The
vtable survey in §5.1 is a survey of *signatures*, and signatures are not
semantics. A backend that draws during queueing and ignores `RunCommandQueue`
(§2) is relying on frontend behaviour it never asked for, and twelve years of
batching work sit between the two versions. This is the specific thing stage 0's
device run tests, and the specific reason it must happen before any patch.

**The rotation experiment gets skipped.** It is the cheapest item here and the
one whose absence has already caused a wrong conclusion to be recorded twice.

**Scope creep into a platform port.** [MIYOO-MINI.md § 8](../../docs/MIYOO-MINI.md)
notes how close the chosen path already is to writing our own platform layer.
Work that starts by fixing `Mini_UpdateTexture` and ends up owning input, audio
and present has become that, without the decision ever being taken. The patch
list in §3 is the boundary — and note that E moves that boundary, because owning
the drivers as source in-tree makes each next line easier to write than it was
as a patch against someone else's file.

**The prebuilt may not correspond to the source.** Partly settled in §7.1: it
carries the Flip detection, and it is one commit behind a change to the driver
sources. Enough to build the control against `68ce3172` rather than head, not
enough to call the correspondence proven.

---

## Open questions

- **Does XK9274's fork have the same bugs?** §1.1–1.3 are in code both forks
  descend from, and [MIYOO-MINI.md § 3.1](../../docs/MIYOO-MINI.md) found they
  converge. Worth a read now that a checkout is cheap — **as a reference, not as
  a route**: if they have already solved something, the solution is worth reading
  before writing our own. Option C stays rejected either way.
- **Is `Context::clear()` load-bearing anywhere on this target?** It is a no-op
  today (§2.1) and nothing has broken, which means either nothing depends on it
  or something is depending on the full-screen `Layer` copy to cover for it. The
  distinction decides whether item 8 is a fix or a formality.
- ~~**Does `eglUpdateBufferSettings` matter to our path at all?**~~ **Answered
  2026-07-31: no.** §1.7 — it is called only from `glCreateContext`, and the call
  in `Mini_CreateWindow` is to a same-named local static. Carried unanswered from
  [gles-free-runtime](../2026-07-29-gles-free-runtime/) since 2026-07-29, where it
  is now closed too.
- **Is `Mini_UpdateTexture`'s borrowed pointer why anything already observed
  looked odd?** Nothing is currently attributed to it. Worth re-reading the device
  logs against it rather than assuming it has been harmless.
- **What does 2.32's render frontend expect that 2.0.20's did not?** The signature
  survey (§5.1) does not answer it, and it is the question stage 0 is really
  testing. Reading `SDL_render.c`'s `QueueCmd*` path in both trees would answer it
  on the desk rather than on the device, and is the cheapest de-risking available
  for option E.
- **Can the audio driver's json-c dependency simply go?** It reads one integer,
  the initial volume, from `/appconfigs/system.json`. If the default is acceptable
  the dependency disappears — and with it a shipped binary whose version
  [THIRD-PARTY.md](../../THIRD-PARTY.md) records as unidentifiable. What is not
  known is whether the firmware expects SDL to honour that file, i.e. whether
  ignoring it makes the system volume control stop working inside the demo.

## References

- [docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md) — § 3.1 for what the ports omit,
  § 4.6 for the vendor API, § 5 for which limits are whose
- [gles-free-runtime](../2026-07-29-gles-free-runtime/) — option B, whose cost
  this snapshot revises, and the build notes that are its starting point. §1.7
  here closes its last open question and re-prices its option C
- [defects.md](../2026-07-25-cxx17-modernization/defects.md) — D22, D24, D25, D26
- [THIRD-PARTY.md](../../THIRD-PARTY.md) — the pin, and the LGPL obligations,
  which follow the drivers rather than SDL2 (§4)
- [TARGETS.md § Pinned dependencies](../../docs/TARGETS.md) — the machinery
  option E puts this target back inside
- [steward-fu/sdl2](https://github.com/steward-fu/sdl2) — LGPL-2.1, SDL 2.0.20
- [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo) — the maintained fork,
  and option C
