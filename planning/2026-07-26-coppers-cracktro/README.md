# `coppers` — a copper-bar cracktro as the first thing to run on hardware

**Status:** `in-progress`
**Written:** 2026-07-26
**Blocked by:** nothing
**Started:** 2026-07-27 — stage 0 landed
**Serves:** [target-validation](../2026-07-25-target-validation/),
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/)

## Motivation

An oldskool copper-bar cracktro: horizontal raster bars sweeping on sine phases,
a scrolling message, tracker music from `data/*.mod`, palette and song changes on
the face buttons. Full screen on the device, windowed on the dev box.

It is chosen deliberately rather than for fun alone. Two snapshots are each
carrying an unanswered question that only running code on a device settles, and a
copper-bar demo is the natural instrument for both:

- **[target-validation](../2026-07-25-target-validation/) asks what the software
  driver's fill rate is.** A full-screen raster field is the fill-rate stress case,
  so the measurement falls out of the demo rather than needing a synthetic
  benchmark beside it. That document also records that `gfx::renderer` has no
  screenshot path and that this is "worth adding when the fill-rate measurement
  below is taken" — same piece of work.
- **[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) is
  blocked on `Texture` and a source-rect blit.** A per-frame scroller cannot exist
  without them, because `Context::draw_text` rasterises and uploads a texture on
  every call. So the demo's second task *is* that snapshot's first task.

It is also the first thing in this project that will have run on hardware at all.
Six snapshots' worth of work has been verified by compiling and by tests.

## Targets

| Device | SoC | CPU | RAM | Panel | Pixels |
|---|---|---|---|---|---|
| **Miyoo Mini Plus** | SigmaStar SSD202D | Cortex-A7 ×2 @ 1.2 GHz | 128 MB DDR3 | 3.5", **640×480** | 307,200 |
| **Miyoo Mini Flip** | SigmaStar SSD202D | Cortex-A7 ×2 @ 1.2 GHz | 128 MB DDR3 | 2.8", **750×560** | 420,000 |

"Emulator" mode is `desktop-software`, windowed — the same code path, since that
preset sets `WREEL_TARGET_HAS_GPU=OFF` and forces the software driver.

Both devices are GPU-less, so `gfx::gles2` is excluded by construction and this is
a `gfx::renderer` program end to end. It shares nothing with `skratch`, which is
the opposite showcase — see
[gfx-renderer-and-gles2 § 4](../2026-07-26-gfx-renderer-and-gles2/README.md).

**Checked rather than assumed, and the second row is the surprise.** Both devices
are the *same SoC*, so the existing `miyoomini` preset covers both and no new
toolchain row is needed — `docs/TARGETS.md` only needs its device list widened.
But the Flip's panel is **750×560**, not 640×480: a non-standard mode with **37%
more pixels than the Mini Plus, on identical silicon**.

That inverts the intuition. The Flip is the smaller screen and the newer device,
and it is the **binding target for anything fill-rate bound** — which this demo
is by construction. Design against 750×560 and the Plus has headroom; design
against 640×480 and the Flip is 37% over budget.

750×560 is unusual enough that the firmware may well present SDL a different
logical mode than the physical panel, which several of these devices do. That is
exactly what `wreel-probe`'s display-mode section answers, and it makes running the
probe on a Flip a higher priority than on a Plus.

## Verified while writing this

Four claims that were going to be assumed, checked instead. Two changed the plan.

**1. `miyoomini` builds no demo at all today.**
[`cmake/ProjectOptions.cmake:164`](../../cmake/ProjectOptions.cmake) force-disables
`WREEL_BUILD_DEMOS` whenever `WREEL_ENABLE_GLES2` is off, with a comment saying
why — `skratch` renders through `gles2` and has nothing to draw with on a GPU-less
target. Correct when `skratch` was the only demo, wrong now: it disables the demo
on the one preset this demo is *for*. Splitting the option is a prerequisite, not
a nicety.

**2. The bitmap glyph source already exists and already works.** `data/Speedy.fon`
is a genuine NE-format Windows 3.x font DLL (`file(1)`: "MS-DOS executable, NE for
MS Windows 3.x (3.0) (DLL or font)"). The vendored FreeType keeps the default
`modules.cfg`, so `winfonts` is in `FONT_MODULES` and both
`src/winfonts/winfnt.c.o` and `src/base/ftwinfnt.c.o` are in the build tree. And
[`skratch/application.cc:74`](../../skratch/application.cc) already loads it with
`TTF_OpenFontIndex("data/Speedy.fon", 10, 0)` and renders its HUD from it.

This was the original basis for decision 5 — authentic bitmap glyphs with no new
asset and no new dependency, which is what made shipping two glyph paths cheap.
Decision 5 now prefers a PNG sheet for reasons unrelated to this, but the finding
stands and is why the fallback is a genuine fallback rather than a hope.

**3. The Miyoo Mini build uses SDL's generic C blitters.** `SDL_ARM_SIMD_BLITTERS`
and `SDL_ARM_NEON_BLITTERS` are both `#undef` in the generated
`build/miyoomini/_deps/sdl2-build/.../SDL_config.h`. Upstream defaults both `OFF`
(`sdl2-src/CMakeLists.txt:422-423`), gated on `SDL_ASSEMBLY;SDL_CPU_ARM32` — both
of which hold for this preset, on a Cortex-A7 that has NEON
([`cmake/toolchains/miyoomini.cmake:89`](../../cmake/toolchains/miyoomini.cmake)).
Nothing in `cmake/` sets them.

So the one device doing 100% of its pixel work on the CPU is leaving SDL's NEON
assembly blitters unbuilt, one flag away. Stated precisely rather than oversold:
`pixman-arm-neon-asm.S` covers specific blit paths, **not `FillRect`**, so this is
a sprite-and-alpha win rather than a fill win, and it is ARM32-only so it never
applies to the aarch64 rows. It is a separate measurement from this demo's, and it
should be taken separately or it will contaminate the fill-rate number.

**4. The `320×240` in the existing docs is wrong, and no single number replaces
it.** [target-validation](../2026-07-25-target-validation/README.md) and
[software-3d-rasteriser](../2026-07-25-software-3d-rasteriser/README.md) both pose
their cost questions at 320×240, while
[`gfx/gles2/context.cc:40`](../../gfx/gles2/context.cc) budgets its depth buffer at
640×480. Neither describes the hardware: the Mini Plus is 640×480 and the Mini Flip
is **750×560** (sourced in Targets above). So the fill-cost questions those
documents ask are out by **4×** and **5.5×** respectively, and the two target
devices do not even agree with each other.

That is the direct argument for decision 6 — the layer's resolution becomes a
parameter and gets measured, because there is no single figure to hard-code and
picking one would silently mean picking a device.

## Decisions

Settled 2026-07-26, before implementation.

### 1. Hybrid: `SDL_Renderer` is the engine, a locked-pixels layer is the escape hatch

The question that decided this was not "which is more authentic" or "which is
faster" but **which code survives into a game**, since this project's next real
consumer is 2D game work.

| Path | What you end up owning | Does SDL already ship it? |
|---|---|---|
| `SDL_Renderer` | `Texture`, source-rect blit, `Atlas`, `AnimatedSprite`, `TileMap` | **no — this is the gap** |
| CPU framebuffer | a locked-pixels view | **no — this is the other gap** |
| | glyph and sprite blitting | yes |
| | clipping | yes |
| | alpha compositing | yes |
| | scaling, rotation, flip | yes |

Roughly 80% of a full CPU-framebuffer engine is re-implementing what SDL already
ships, tested, on every target — and every item in the `SDL_Renderer` column is
already a line on
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/README.md)'s
task list. Taking that path would also make the Mali rows perform like the Miyoo
Mini by construction, since `SDL_Renderer` is hardware accelerated there and a
hand-plotted framebuffer is not.

But the remaining 20% is not optional for a game either, and this is the half the
pragmatic argument usually drops. **On the SSD202D there are no shaders.** A fade
to black, a palette flash on impact, a wipe between levels, a plasma background,
heat distortion — every one of them needs pixel access, and `SDL_Renderer` offers
no route to any of them. A platform that cannot fade to black is missing
something real, and retrofitting pixel access after the API has set is the
expensive ordering.

So both gaps get closed and neither engine gets rebuilt:

- Bars are plotted per scanline into a `STREAMING` texture that `Context` can lock.
- Glyphs, sprites and HUD go through `Texture` and source-rect blits.

**This is not two renderers.** The streaming texture is an ordinary `SDL_Texture`
blitted like any other, so there is no state-restoration discipline to maintain —
that problem is specific to mixing our own GL calls into a window `SDL_Renderer`
owns, which is why `gfx::gles2` is a separate renderer and not this.

It also closes the locked-pixels design question that
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/README.md)
and [midi-live-visuals](../2026-07-25-midi-live-visuals/README.md) are both holding
open, for roughly sixty lines on `Context`.

### 2. Both glyph mechanisms ship, switchable at runtime, with their costs on screen

The demo draws the scroller two ways — glyph sheet as a `Texture` with source-rect
blits, and a hand-written CPU blitter into the locked layer — and a button
switches between them live while the per-path cost is displayed.

This is the same reasoning that keeps `skratch` in the tree: it is retained
deliberately as the worked example where
[the contrast is the useful part](../2026-07-26-gfx-renderer-and-gles2/README.md).
A tech demo that *shows* the difference between a driver blit and a hand blitter,
in microseconds, on the actual device, is worth more than either path alone and
more than a paragraph asserting which is faster.

Three consequences worth recording, because each is the kind of thing that gets
"corrected" later by someone reading only the code:

- **Runtime virtual dispatch is correct here.** A small abstract scroller base
  with two implementations, selected by a button press. `CLAUDE.md` prefers
  compile-time polymorphism *where the choice is fixed at build time* — which is
  why `util::File` selects its `FileImpl` by `#ifdef`. This choice is not fixed at
  build time, so a vtable is the right tool and traits dispatch would be the
  wrong one. Do not "fix" this into an `#ifdef`.
- **One asset feeds both.** The glyph sheet loads once as an `SDL_Surface`. The
  texture path uploads it to a `Texture`; the CPU path reads from the surface
  directly. No duplicated fixture, and the two paths are drawing provably
  identical glyphs, which is what makes the comparison mean anything.
- **The HUD does not switch with the scroller.** It is pinned to the texture path
  always. Otherwise the instrument moves with the thing being measured.

A per-scanline rubber scroller stays available on *both* paths: on the texture path
it is a stack of 1px-high source-rect blits, which is cheap.

### 3. Free-running wall clock; the visuals do not listen to the music

Bar phase and scroll speed derive from elapsed time, independent of playback.
Authentic to the era, no new machinery, and it cannot drift or stall if the mixer
substitutes a rate — which
[`docs/TARGETS.md § Mixer profile`](../../docs/TARGETS.md) records as a real
per-device behaviour.

Beat sync was considered and rejected for now. `Mix_GetMusicPosition()` returns
**seconds only**; libxmp can report pattern, row and tick but SDL2_mixer does not
surface it, so real beat locking means driving libxmp directly and bypassing the
`audio::Music` facade. That is a subsystem change, not a demo feature, and
`docs/TARGETS.md` already records it as such.

### 4. Capability text is collected in the demo, not extracted from `wreel-probe`

The greetz are assembled from live SDL queries inside the demo. `probe/main.cc` is
left alone.

Stated as the knowing trade it is: `wreel-probe`'s output is a committed
deliverable of [target-validation](../2026-07-25-target-validation/), and
refactoring a working validation tool to serve a demo risks the tool for the
demo's convenience. The cost is that two code paths will report the same facts and
can drift. Accepted; revisit if a third consumer appears, which is the point at
which a shared module would be drawn in the right place rather than guessed at.

### 5. The glyph source is a fixed-grid PNG sheet, with `Speedy.fon` as the fallback

Revised the same day this was written, after
[ianhan/BitmapFonts](https://github.com/ianhan/BitmapFonts) was raised as a
reference — ~700 oldskool sheets, and stylistically exactly right for this.

A PNG sheet is the better engineering answer regardless of where the art comes
from, for four reasons:

- **`loaders::load_image` already returns exactly what decision 2 needs** — one
  `SDL_Surface*`, converted to `ABGR8888`. The texture path uploads it; the CPU
  path reads it. That is the "one asset feeds both" requirement satisfied by code
  that already exists, with no rasterising step at all.
- **It takes SDL_ttf out of the demo entirely**, along with the risk that
  `Speedy.fon` is stuck at the sizes baked into the file. A PNG sheet
  integer-scales to any chunkiness a cracktro wants and stays crisp.
- **A fixed grid needs no atlas metadata.** Sheet names like `08X08-F1.png` and
  `16X16-F1.png` imply a uniform cell, so glyph index to source rect is
  arithmetic — no Sparrow XML, no TMX. That makes it the *simplest possible* first
  consumer of the source-rect blit, which is the point of building it here.
- It gives `Texture` a real fixture. Note that
  [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/README.md)
  is carrying an open task to source a sheet, because `data/jetpackdude.xml`
  references a `JetPackDude.png` that is not in the repository.

**The licensing has to be handled, not assumed.** That repository has **no LICENSE
file**, and the README states plainly "I do not claim rights to any of these works"
and "I don't remember where much of this collection came from". So:

- Fine to develop and measure against. It is a dev-box asset in a demo nobody has
  shipped.
- **Not fine to ship.** [packaging-distribution](../2026-07-25-packaging-distribution/)
  has Steam as a real channel, and an asset of unknown provenance in a store build
  is an actual problem rather than a theoretical one. Before anything ships, either
  author a sheet or substitute a license-clear pixel font (CC0 or OFL), and record
  the choice in `docs/TARGETS.md` the way every pinned dependency is.

`data/Speedy.fon` stays as the fallback and keeps its value as the *proven* path:
it is already in the repository, already loads in this build, and its licence
situation is no worse than it was before. If the PNG route hits trouble, nothing is
blocked.

**One thing to verify per sheet, not assume:** the grid and the transparency
convention. Whether the charset starts at ASCII 32, how many columns, and — the
one that will actually bite — whether transparency is an alpha channel or a key
colour. A sheet with an opaque black background needs `SDL_SetColorKey` before it
will composite over copper bars, and it will look correct on a black test screen
right up until the bars are behind it.

### 6. Internal resolution is a parameter, and gets measured

Rather than resolving the 320×240-vs-640×480 discrepancy by argument, the locked
layer's size is a runtime option and both get timed on the device. The demo is a
measurement instrument; let the number settle it.

One nuance that must shape how the timing is reported, or the measurement will be
misread. A 320×240 layer scaled 2× quarters **our plotting** cost but does not
remove the full-screen write: the scaling blit still writes 640×480 into the back
buffer, and the present after it may copy that again. So the HUD must attribute
cost **per stage** — plot, scale-blit, present, scroller — not just report total
frame time. Reporting one number would make a 4× reduction in plotting look like
no improvement at all.

> **Measured 2026-07-27, and the nuance turned out to be the whole story.** Full
> numbers in
> [target-validation/results.md](../2026-07-25-target-validation/results.md).
> The plot scales exactly with layer pixels — 4.2× for a quarter of them — but on
> the **software driver** SDL's scaling blit costs 2.5× what a 1:1 blit costs, so
> the total goes *up*: 0.93 ms against 0.78 ms. Scaling down is a **net loss on
> the Miyoo Mini's driver**. On the **accelerated** driver the upscale is free and
> the same change is a **2.8× speedup**.
>
> So this decision was right for a reason better than the one given: there is no
> single correct internal resolution, because the answer **inverts between the two
> drivers this project ships on**. Had a constant been picked it would have been
> picked for one target and against the other. The wording above still assumes a
> lower resolution is a win pending measurement — it is, on `rk3326` and `h700`,
> and it is not on `miyoomini`.

### 7. Realtime services live in `wreel::rig`, not `wreel::util`

Added 2026-07-27, when stage 0 tried to put asset resolution in `util` and that
turned out to be wrong.

`util` is generic to any application on any platform — tokenizers, number
conversion, logging, file I/O — and it links **no SDL**, deliberately. Asset
resolution wants `SDL_GetBasePath()` and frame pacing wants a sleep, so taking
either into `util` would have put `test_string`, `test_number` and `test_ascii`
downstream of a windowing toolkit for the sake of two functions.

The rejected workaround is worth recording, because it looked reasonable:
implement "where is the executable" with `readlink("/proc/self/exe")` through the
existing `util/{posix,mswin}/` split, keeping `util` SDL-free. That works on all
five targets and is the same compile-time platform selection `util::File` uses.
It was still the wrong answer — it pays a real cost, reimplementing SDL's platform
handling, to preserve a boundary that was in the wrong place. The boundary should
move instead.

So `wreel::rig` holds what a realtime program needs from its surroundings: asset
and preference paths, frame timing, and next the `SDL_GameController` mapping.
The split is by what code is **for**, not by how generic it looks. Its charter,
including what does *not* belong in it, is in
[rig/CMakeLists.txt](../../rig/CMakeLists.txt) — a name that broad invites a
dumping ground, so the boundary is written down where someone adding a file will
see it.

### 8. Fullscreen stays the default; `--windowed` is the emulator mode

`Context` already takes a `fullscreen` parameter defaulting to `true`, and
handhelds have no window manager. The desktop mode is an added option, not a
changed default — `CLAUDE.md § Developer convenience is not a correctness
criterion`.

## Proposed shape

```
rig::asset_path(name)               resolved once: $WREEL_DATA_DIR, then beside
                                    the executable, then the working directory
rig::FrameClock                     clamped delta, optional cap, per-stage timing

gfx::renderer::Texture              owns an SDL_Texture; movable, not copyable
Context::draw(const Texture&, const Rect* src, const Rect* dst)
Context::lock() / unlock()          a STREAMING layer, plotted per scanline
Context::save_bmp(path)             SDL_RenderReadPixels — the missing screenshot

coppers/main.cc                     args; log to SDL_GetPrefPath("wreel","coppers")
coppers/bars.{hpp,cc}               ramps, sine phases, plot into the locked layer
coppers/scroller.hpp                abstract: draw(text, x, y), last_cost_us()
coppers/scroller_texture.cc         glyph sheet -> Texture, source-rect blits
coppers/scroller_cpu.cc             glyph surface -> locked layer, hand blitter
coppers/glyphs.{hpp,cc}             PNG sheet via loaders::load_image; fixed grid,
                                    so glyph index -> source rect is arithmetic
coppers/playlist.{hpp,cc}           the three data/*.mod over audio::Music
coppers/greetz.{hpp,cc}             capability strings, assembled for scrolling
coppers/input.{hpp,cc}              SDL_GameController + keyboard
```

`coppers/input.cc` is written against `SDL_GameController` from the start rather
than raw axis indices. `skratch/input.cc` hard-codes Xbox 360 axis numbers and
[target-validation](../2026-07-25-target-validation/README.md) already predicts
that "certainly gets [the handheld] wrong". The mapping this demo discovers is
also one of the answers step 4 of that document is looking for, so enumeration
gets **logged**, not assumed.

### Controls

| Input | Action |
|---|---|
| D-pad left / right | previous / next `.mod` |
| A | cycle copper palette ramp |
| B | toggle scroller path — texture blits ↔ CPU blitter |
| X | toggle internal resolution — 320×240 scaled ↔ native |
| Y | toggle the timing HUD |
| Start / Esc | quit |

Keyboard equivalents throughout, so the desktop window is a full substitute.

### What the message actually says

The greetz are the capability report, in demo voice — SoC and target id, render
driver actually selected, panel mode and refresh, RAM and core count from
`SDL_GetSystemRAM`/`SDL_GetCPUCount`, granted audio spec and codec tier, what the
gamepad enumerated as — plus the live cost of the path currently drawing the text,
which is the joke and the measurement at the same time.

## Tasks

Ordered so the tree builds and tests pass at every step, and so something is on
screen as early as possible.

**Stage 0 — prerequisites. Nothing visible yet — LANDED 2026-07-27**

- [x] Split the demo gating so a GPU-less target can build a demo. `skratch` now
      follows `WREEL_BUILD_SKRATCH`, derived from `WREEL_ENABLE_GLES2`;
      `WREEL_BUILD_DEMOS` is the umbrella and no longer disables itself. The
      `miyoomini` preset reports `demos ON` for the first time
- [x] Asset resolution — landed as **`rig::asset_path()`**, not `util::`. See the
      module decision below; `test_assets` covers the override, caching and
      separator handling, and the shipped layout was verified against a real
      `cmake --install` tree launched from `/tmp`
- [x] A real frame clock — `rig::FrameTiming` (pure arithmetic, 10 cases, no clock
      and no sleep) plus a thin `rig::FrameClock`. `skratch` is off `SDL_Delay(10)`
      and its camera is now rate-based, scaled against the 60 Hz it was implicitly
      tuned at so the motion is unchanged at 60 fps and correct at any other rate

**What stage 0 cost and found.** All five presets: 12/12 tests, zero warnings.
Three things it turned up that were not in this plan:

- **`WREEL_WERROR=ON` was in effect nowhere** — `option()` does not overwrite a
  cache entry, so the 2026-07-26 flip reached a fresh clone and no existing build
  directory. Every "zero warnings under `-Werror`" claim in the status table had
  been produced with `-Werror` off. Recorded as **D19**, and it is D18's twin: a
  changed default is not a changed setting. Rebuilt with it forced on — the tree
  was as clean as claimed, so the outcome is fine and the verification was not.
- **`skratch`'s `_last_tick` was dead** — assigned once in the constructor, never
  read. It is what the frame clock replaces.
- **Mouse deltas must not be scaled by frame time**, unlike held keys and stick
  axes. `mouse_rel_*` is a displacement already accumulated over the frame, not a
  velocity; scaling it would have made look sensitivity frame-rate dependent,
  which is the exact bug the clock exists to remove.

**Stage 1 — bars on screen — LANDED 2026-07-27**

- [x] The locked-pixels layer, as `gfx::renderer::Layer` plus a `LayerLock` guard
      that unlocks on scope exit. Sized independently of the window; nearest
      scaling by default, since a linear 2× upscale looks soft-focused rather than
      pixelated. This is what closes the open design question in
      [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) and
      [midi-live-visuals](../2026-07-25-midi-live-visuals/)
- [x] `coppers/bars.cc` — palettes, six bars on staggered centres, per-scanline
      resolve. Kept free of any rendering call, so `test_bars` covers it with 9
      cases and no window
- [x] `--windowed`, `--layer-height`, `--screenshot`, `--frames`, `--fps`,
      `--no-hud`, and `--seconds`
- [x] `Context::save_screenshot()` via `SDL_RenderReadPixels` — named to match
      `gles2::Context` so the two renderers stay symmetric. The check
      [target-validation](../2026-07-25-target-validation/README.md) recorded as
      missing now exists for both
- [x] Per-stage timing HUD: plot, blit, present, separately

**What stage 1 found.** Four things, two of which changed the plan:

- **The internal-resolution answer inverts between drivers** — see decision 6.
  This is the fill-rate measurement the project has been carrying as an open
  question, and it says the assumption everyone was working from is wrong on the
  target it matters most for.
- **A per-stage measurement of a batching API measures the batching.** The first
  run reported `blit 0.000` and a present cost that *rose* as the layer shrank.
  SDL queues render commands and executes them at present, so the blit was timed
  as zero and its cost charged to the swap — which reads as "scaling is free".
  `SDL_RenderFlush()` inside the timed region fixes it, at the cost of defeating
  batching, which is a distortion worth accepting in an instrument.
- **`--seconds` was not in the plan and is needed.** The loop could only be left
  by a keypress, and a fill-rate run on a device happens over SSH where there is
  no keyboard. A fixed duration is also repeatable in a way that "however long I
  left it running" is not.
- **armv7 and x86-64 produce byte-identical screenshots.** `--screenshot` steps a
  fixed 1/60 rather than wall-clock time, so the image depends only on the frame
  index. That makes it a cross-architecture regression fixture, which is worth
  more than the smoke test it was added as — `char` signedness differs between
  those two targets (D10).

**Also settled while here:** `DISALLOW_COPY_AND_ASSIGN` is not used in any of the
new code. It is the pre-C++11 idiom — declare the copy operations private and
never define them — so it fails at *link* time, and fails not at all from inside
the class. `= delete` is a compile error with a diagnostic that says what happened.
`CLAUDE.md` already required this; the first draft of `Layer` and `Demo` copied the
surrounding legacy pattern instead.

**Stage 2 — the scroller, both ways — LANDED 2026-07-27**

- [x] `gfx::renderer::Texture` — owning, movable not copyable, with colour and
      alpha modulation. **`software-2d-sprites-tiling`'s blocking task is done**;
      `Atlas`, `AnimatedSprite` and `TileMap` are now expressible
- [x] `Context::draw()` taking source and destination rects
- [x] Glyph sheet chosen and its conventions verified before any code was written
      against it — `data/glyphs-16x16.png`, 320×48, sixty 16×16 cells, ASCII
      32–91. **Transparency is a key colour, not alpha**, exactly as the risk
      predicted, so it is read as a 1-bit mask instead
- [x] `coppers/glyphs.cc` — loaded once through `loaders::load_image`, glyph index
      to source rect by arithmetic, integer scaled. `test_glyphs`, 8 cases
      against the real file
- [x] Both scrollers behind the abstract base; B toggles; per-path µs on screen
- [x] `draw_surface` re-expressed over `Texture`, so there is one upload path
      rather than two that can drift

**What stage 2 found.**

- **The two mechanisms invert with the driver, by 4.3× in each direction.** The
  hand-written blitter costs 169 µs on the software driver and 168 µs on the GPU
  — identical, because it never touches the driver. The texture path costs 734 µs
  and 39 µs, a 19× swing. So the Miyoo Mini should plot text by hand and the Mali
  devices should not, which is the same shape as the internal-resolution finding
  and has the same cause. Numbers in
  [results.md](../2026-07-25-target-validation/results.md).
- **The instrument was wrong again, the same way, in a new place.** The first
  reading was `texture 4 µs` against `cpu 167 µs` — the texture path apparently
  forty times faster, when it is four times slower. Four microseconds was the cost
  of *queueing* eighty `SDL_RenderCopy` calls; the execution was being charged to
  the next stage. Fixing the layer blit's attribution in stage 1 did not cover
  this, because it was a different unflushed region. Every timed region now
  flushes, and the stages no longer nest so they can be summed.
- **Reading the sheet as a 1-bit mask rather than colour-keying it** was worth
  more than expected. It is what lets the palette switch recolour the text along
  with the bars, from one upload, on both paths.
- **The text was invisible at first**, because it followed the palette exactly and
  so did the bars it crosses. Fixed with a drop shadow and by pushing the text
  colour halfway to white — period correct, and applied identically to both paths
  so the comparison stays honest.

**Also recorded:** `data/PROVENANCE.md` now tracks where every asset came from and
what is known about its licence, including the honest "unknown" rows for the 2016
inheritance. Written on the first commit that adds a traceable asset rather than
later, which is the only time it is cheap.

**Stage 3 — music and input — LANDED 2026-07-27**

- [x] **Commit the music** — done 2026-07-27. `data/complications ii.mod` and
      `data/her bloody weekend.mod` had been sitting untracked in `data/` since
      before this work, so a fresh clone had one of the three songs. They landed
      with the `~Music` fix, whose regression test needs them
- [x] `coppers/playlist.cc` over `audio::Music`; left and right step the three
      `.mod`s, wrapping both ways. `test_playlist`, 6 cases against the real
      files, including the two whose names contain spaces
- [x] Palette ramps on A — landed in stage 1, and they now recolour the scroller
      as well as the bars
- [x] `rig::Pad` — `SDL_GameController` where SDL recognises the device, a raw
      `SDL_Joystick` fallback where it does not, keyboard equivalents always, and
      full enumeration logged
- [x] X toggles internal resolution at runtime, so decision 6's finding can be
      checked on a device without rebuilding

**Where the input work landed, and why it is in `rig`.** `rig::Pad`, not
`coppers/input.cc`, because
[packaging-distribution](../2026-07-25-packaging-distribution/) already wanted
`skratch/input.cc`'s hard-coded Xbox 360 axis indices replaced and a second demo
needs the same thing. `skratch` is **not** ported onto it yet — that is a change
to a working demo and wants its own commit.

**The raw-joystick mapping is a guess, and says so.** There is no way to know what
button 3 is on a device SDL does not recognise. The fallback assigns buttons in
the conventional retro-handheld order so the demo is controllable at all, and logs
a warning naming the device, its GUID, its axis/button/hat counts and the fact
that the mapping is unverified. That log line **is** the target-validation
deliverable: "what does the gamepad enumerate as" stops being a question and
becomes output.

**What stage 3 found.**

- **`build_greetz()` was constructing its own `audio::Device`** to read the mixer
  spec — and a Device opens the mixer in its constructor and calls
  `Mix_CloseAudio` in its destructor. Since the message is rebuilt whenever X
  changes resolution, that would close the mixer underneath music that is already
  playing. **Checked before claiming it**: SDL_mixer reference-counts
  (`audio_opened` in `mixer.c`), so it survives — *but only while both Devices
  request the same format*. With differing formats `Mix_OpenAudio` tears the first
  one down outright, and nothing in `audio::Device`'s interface says a second
  instance is safe. Not a bug that fired; a trap that was armed. The device is
  passed in now.
- **`-Werror` earned its keep again.** `-Wvexing-parse` caught
  `Playlist playlist(std::vector<std::string>())` declaring a function rather than
  a variable, in a test that would otherwise have silently tested nothing.
- **libxmp prints loader diagnostics to stdout in Debug builds** — 65 lines a run
  on `desktop-software`. Zero on `miyoomini`, which is Release, so this is dev-box
  noise and not something a device console would see. Recorded rather than fixed.

**Stage 4 — the measurement, which is the point**

- [ ] Fill rate at both internal resolutions on a Mini Plus, per stage
- [ ] `SDL_ARMNEON`/`SDL_ARMSIMD` measured **separately**, so it does not
      contaminate the fill-rate number
- [ ] `wreel-probe` output and the demo's numbers into
      `../2026-07-25-target-validation/results.md`, with real command output

## Risks

**Nothing here has run on hardware, and this is the thing that will.** Every
number below is arithmetic, not evidence, and the demo exists to replace it.

**The frame budget is the whole question, and the Flip sets it.** At 32 bpp the bar
field alone writes 1.2 MB per frame on the Mini Plus (640×480) and **1.7 MB on the
Flip** (750×560), so 60 fps needs roughly 74 MB/s and **101 MB/s** of write
bandwidth respectively — before the scale-blit, before the present, before a single
glyph. Add those and it is plausibly 150–200 MB/s on two Cortex-A7 cores that are
also mixing audio. A 320×240 internal layer cuts *our plotting* to ~18 MB/s but the
full-screen write does not disappear, it moves into SDL's blitter.

None of that is a prediction of failure — it is the arithmetic that says why the
measurement is the deliverable. If the numbers come back bad, the levers in rough
order of cost are: a 16-bit layer (`RGB565` halves the bytes and is closer to what
these panels want anyway), plot every other scanline and double it, cap at 30 fps,
or shrink the bar band and leave borders — which is period-correct rather than a
compromise.

**Stock firmware may not take the same path SDL thinks it does.** The Miyoo
firmware routes `SDL_Renderer` through its own `MI_GFX` layer, so present cost may
not resemble the desktop's. This is also where
[target-validation](../2026-07-25-target-validation/README.md)'s largest open
question lands — whether upstream SDL2 is viable at all or
`WREEL_USE_SYSTEM_SDL2=ON` is mandatory. If it is mandatory, the timing numbers
are against a different SDL than the one measured here and must say so.

**The assets are unlicensed, and that is a shipping blocker rather than a
development one.** Developing against
[ianhan/BitmapFonts](https://github.com/ianhan/BitmapFonts) is fine; putting it in
a Steam depot is not, since the collection carries no LICENSE and its curator
states they do not know its provenance.

**The same applies to the music, and more sharply.** Tracker modules from the scene
normally have a named author who retains rights, so three `.mod` files are a
clearer attribution question than an anonymous glyph sheet, not a vaguer one. This
demo's whole premise is scene material, so the point is not to avoid it — it is
that "we'll sort the licences later" is how it gets forgotten and ships.

Mitigation is cheap while it is still cheap: record each asset's origin and author
next to it from the first commit, and put "every shipped asset has a known licence"
on [packaging-distribution](../2026-07-25-packaging-distribution/)'s prerequisite
list beside `SDL_GetBasePath()`. Substituting a license-clear glyph sheet is a
half-hour change if it is done before the demo is built around a specific grid, and
an annoying one afterwards.

**Two glyph paths is real duplicated work** for a comparison rather than a
feature. Held to be worth it on the same grounds `skratch` is kept, but it is the
first thing to cut if stage 2 overruns — ship the texture path, leave the CPU
blitter as a follow-up, and say so rather than quietly dropping the comparison the
demo advertises.

## Open questions

- ~~**What SoC and panel does the Miyoo Mini Flip have?**~~ **Answered while
  writing this**, see Targets: same SSD202D, so one preset covers both, but
  750×560 rather than 640×480. Still open is which firmware the Flip ships and
  whether OnionOS layouts apply to it, which
  [packaging-distribution](../2026-07-25-packaging-distribution/) will need.
- **Does the Flip's firmware report 750×560 to SDL, or a scaled logical mode?** A
  non-standard panel is exactly where a vendor scaler tends to sit. `wreel-probe`
  on a Flip answers it, and the answer decides whether the demo sizes itself from
  `SDL_GetDesktopDisplayMode` or is told.
- ~~**Is `320×240` in the older snapshots wrong?**~~ **Yes.** No device in the
  matrix has a 320×240 panel; the two this demo targets are 640×480 and 750×560.
  Those documents should be corrected to say "an internal render resolution" if
  that is what was meant, because as written they read as panel claims and the cost
  questions built on them are out by 4× and 5.5× respectively.
- **Does `save_bmp()` belong on `Context` or in the demo?** Leaning `Context`:
  target-validation wants a screenshot from *anything* using this renderer, not
  just from here.
- **Computed ramps, or a real indexed layer?** Ramps computed per frame need no
  new machinery. A genuine `INDEX8` layer with a palette LUT is a quarter of the
  bytes to fill and gives authentic palette rotation for free, but costs a LUT
  expand on upload. Measure the fill first; this is cheap to adopt early and
  annoying to retrofit.
- **Is `coppers` the name?** A coin toss against `cracktro` and `copperbars`.

## References

- [target-validation](../2026-07-25-target-validation/) — the fill-rate question,
  the missing `gfx::renderer` screenshot, and the gamepad unknown this answers
- [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) —
  `Texture` and the source-rect blit are shared, not duplicated
- [midi-live-visuals](../2026-07-25-midi-live-visuals/) — the other consumer of a
  locked-pixels view, and where the effects catalogue lives
- [gfx-renderer-and-gles2](../2026-07-26-gfx-renderer-and-gles2/) — why there are
  two renderers and why this one is not `gles2`
- [packaging-distribution](../2026-07-25-packaging-distribution/) —
  `SDL_GetBasePath()` and `SDL_GameController`, both prerequisites here
- [`docs/TARGETS.md`](../../docs/TARGETS.md) — codec tiers, mixer profile, and why
  tracker formats
- [ianhan/BitmapFonts](https://github.com/ianhan/BitmapFonts) — ~700 oldskool PNG
  glyph sheets. Reference and development asset; **no LICENSE, provenance
  disclaimed by the curator**, so see decision 5 before it goes anywhere near a
  shipped build
