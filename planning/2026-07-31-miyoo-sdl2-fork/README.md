# Maintaining our own SDL2 for the Miyoo Mini

**Status:** `snapshot`
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

That is the path `gfx::renderer::Context::draw_surface()` takes. It has not
visibly failed on device, which is what a read of recently-freed heap normally
looks like.

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
| `SDL_CreateTextureFromSurface` | `Texture(surface)` [texture.cc:24](../../gfx/renderer/texture.cc#L24) | **Use-after-free** — §1.1 |
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
| 1 | Copy pixels in `Mini_UpdateTexture` into `t->data` | §1.1 use-after-free | **certain** — it is a `memcpy` |
| 2 | Offset the staging copy by `srt.y * pitch` | §1.2 atlas corruption | **certain** |
| 3 | Derive the format from `texture->format`, not `pitch / srt.w` | §1.3 | **certain** |
| 4 | Set `max_texture_*` from `FB_W`/`FB_H` after detection | §1.4, the Flip | **certain** |
| 5 | Drop `TARGETTEXTURE` from the advertised flags | §1.5 | **certain** |
| 6 | Bounds-check `update_texture`'s 100-entry table | silent missing sprites (D27) | **certain** |
| 13 | Advertise `ABGR8888` (`E_MI_GFX_FMT_ABGR8888` is native) | removes a conversion per upload, and §1.1's converting path | high |
| 19 | `MI_SYS_Munmap(gfx.fb.virAddr, FB_SIZE)`, not `TMP_SIZE` | under-unmaps the framebuffer on teardown | **certain** — it is upstream's own later fix (§7.1) |

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
| 10 | Blend mode → `eSrcDfbBldOp`/`eDstDfbBldOp`/`eDFBBlendFlag` | `SDL_SetTextureBlendMode` — **alpha sprites** | high |
| 11 | Colour/alpha mod → `COLORIZE`/`COLORALPHA` + `u32GlobalSrcConstColor` | `SDL_SetTextureColorMod`, `SetAlphaMod` — fades and tints | medium — the flags are documented, the combination is untried |
| 12 | `Mini_QueueFillRects` → `MI_GFX_QuickFill` | `SDL_RenderFillRect` | high |
| ~~13~~ | — | *moved to tier 1* | |

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

## 6. The experiment that should happen first, fork or not

**Is the fixed `E_MI_GFX_ROTATE_180` a bug or correct panel compensation?**

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

## Tasks

**Stage 0 — the gate.** Rewritten for option E; the B form is kept below it
because B is the fallback if E fails, and its tasks are not the same.

- [ ] **Settle the json-c link before anything else.** `--enable-audio-mini`
      links `-ljson-c`, and the union sysroot **does not have it** — it ships
      `libcjson`, a different library with a different API. Three ways out, and
      the choice wants making rather than discovering at link time: supply json-c
      headers and link the `libjson-c.so.5` the bundle already carries; link
      `-l:libjson-c.so.5` against the staged bundle copy; or drop the dependency
      by replacing the driver's four json-c calls, which exist only to read an
      initial volume out of `/appconfigs/system.json`. **The third is the one to
      take** if the volume default is acceptable, because it removes a shipped
      binary of unrecorded version (THIRD-PARTY.md) as a side effect
- [ ] Add `src/{video,render,audio}/mini` to the pinned SDL 2.32.10 build: the
      `SDL_OFFSCREEN`-shaped CMake block, the `#cmakedefine` entries, and the
      three `bootstrap[]`/`render_drivers[]` registrations (§5.1)
- [ ] Fix the three changed signatures — `RenderPresent`,
      `SDL_RenderDriver.CreateRenderer`, audio `OpenDevice` — and **nothing
      else**. Anything beyond them is a finding, not a task: it means the §5.1
      survey missed something and the estimate needs revisiting
- [ ] Compile it. The SDK headers need no work: the toolchain sysroot's
      `mi_gfx.h`, `mi_sys.h`, `mi_common.h` and `mi_ao.h` are **byte-identical**
      to the vendor tree's `mini/inc/` copies, verified 2026-07-31, and the
      sysroot carries the matching `libmi_*.so`. There is no need to vendor
      SigmaStar's headers or blobs
- [ ] Compare exported symbols against the shipped prebuilt's — 107 imported by
      our satellites plus 87 by our own code, the comparison
      [TARGETS.md](../../docs/TARGETS.md) already records. Expect *more* symbols,
      not the same set: 2.32 exports what 2.0.20 did plus twelve years of
      additions. **The check is that nothing is missing**, not that the sets match
- [ ] Point the bundle at it (`WREEL_ONION_SDL2_RUNTIME`) and run on the device
      unmodified. **This is the gate.** It tests base and build together (§5.1),
      so a failure needs the B control below to localise
- [ ] Drop `-lEGL -lGLESv2` and `SDL_gles_mini.c` (§1.7) and confirm the bundle's
      `lib/` is `libSDL2` alone

**Stage 0-B — the fallback control**, only if stage 0 fails

- [ ] `autoconf` the vendor tree **at `68ce3172`, not head** (§7.1) in a modern
      container — `autogen.sh` needs `autoconf` only, no automake and no libtool
      — then `configure`/`make` in `wreel-miyoomini`, and produce a
      `libSDL2-2.0.so.0`
- [ ] Run it unmodified. An unpatched 2.0.20 rebuild that behaves identically to
      the prebuilt separates "the drivers do not build here" from "2.32 is the
      problem", which is the one thing stage 0 cannot tell you on its own

**Stage 1 — tier 1, the correctness patches**, only if stage 0 passes

- [ ] Items 1–6 from §3 plus item 13, each as its own commit naming the defect it
      fixes
- [ ] The `MI_SYS_Munmap` under-unmap from `9eff61a4` (§7.1), which is a seventh
      tier-1 item the source found rather than the reading
- [ ] A device run per item, or per pair where one cannot mask the other
- [ ] Withdraw D27, and revise D25 to whatever survives

**Stage 2 — tier 2, and it is a comparison rather than a task list**

Not scheduled here any more; the decision above moved it. What stage 2 owes
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) is the
comparison, made once with tier 1 landed:

- [ ] Blend, colour mod and alpha mod in `gfx::renderer::Layer` — five targets,
      no device loop, needed by that snapshot anyway — against items 7–12 in the
      driver, one target, device loop, and correct for any SDL2 program rather
      than only ours
- [ ] If the driver wins, item 7 goes first and alone: `RunCommandQueue` as a
      real execution loop with `QueueCopy`'s behaviour moved into it unchanged.
      **The device run after that commit must look identical to the one before
      it** — §2.3 is why, and it is what makes 8–12 safe to add

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
