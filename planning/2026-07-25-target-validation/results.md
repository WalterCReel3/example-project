# Results

Real output, per preset, rather than a summary. The deliverable this snapshot's
checklist asks for.

**Nothing here has run on a device.** Every number below comes from the dev box or
from qemu, and the one thing qemu cannot tell you is how fast anything is. Read the
caveats before quoting a figure.

---

## 2026-07-27 — fill rate, and the internal-resolution lever

Taken with `coppers`, the demo added for exactly this
([planning/2026-07-26-coppers-cracktro](../2026-07-26-coppers-cracktro/)). It plots
a full-screen copper field into a `gfx::renderer::Layer` and reports the cost of
each stage separately.

Method, so the numbers can be reproduced or disputed:

```sh
WREEL_DATA_DIR=$PWD/data SDL_VIDEODRIVER=dummy \
  ./build/desktop-software/bin/coppers \
  --windowed --no-hud --fps 0 --seconds 2 --layer-height <h>
```

Uncapped so the frame cap cannot hide the cost, `--no-hud` so per-frame text
rasterising is not counted, and `SDL_VIDEODRIVER=dummy` for the software runs so a
compositor is not in the measurement. Costs are exponentially smoothed over ~2
seconds of frames. Debian 12, x86-64, Ryzen with Radeon integrated graphics.

### The software driver — the Miyoo Mini code path

`desktop-software`, which forces the software driver and builds SDL with no GL at
all, so this is the same path the SSD202D takes.

| Layer | Output | plot | blit | total | frames in 2 s |
|---|---|---|---|---|---|
| 640×480 (1:1) | 640×480 | **0.450 ms** | 0.331 ms | 0.781 ms | 2463 |
| 320×240 (2× up) | 640×480 | **0.108 ms** | 0.819 ms | 0.927 ms | 2200 |
| 160×120 (4× up) | 640×480 | **0.025 ms** | 0.797 ms | 0.822 ms | 2393 |

**The plot scales exactly with the layer's pixel count** — 4.2× and 18× for 4× and
16× fewer pixels. That part of the plan was right.

**The conclusion drawn from it was wrong.** Both this snapshot and
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) assume a
lower internal resolution is a cheap win. It is not, on this driver: SDL's
*scaling* blit costs **2.5× what a 1:1 blit costs** (0.82 ms against 0.33 ms), which
more than gives back everything the smaller plot saved. Scaling down is a **net
loss** here — 0.93 ms against 0.78 ms.

The mechanism is that a 1:1 blit is a straight row copy while a scaled one is a
strided read plus a write, and the output side is the same size either way. So the
saving is real but it lands in the wrong place: you remove work from your own loop
and hand more of it to SDL's.

### The accelerated driver — and the answer inverts

`desktop-debug`, same binary, `opengl` driver, real X11 display.

| Layer | Output | plot | blit | present | fps |
|---|---|---|---|---|---|
| 640×480 (1:1) | 640×480 | 0.507 ms | 0.020 ms | 0.096 ms | 1569 |
| 320×240 (2× up) | 640×480 | 0.134 ms | 0.013 ms | 0.078 ms | **4384** |

On a GPU the upscale is free, so the plot is the whole cost and cutting it 4× is a
**2.8× overall speedup**. The same option that loses on the software driver wins
substantially on an accelerated one.

**So `--layer-height` has no single correct default, and that is the finding.** It
is driver-dependent and it inverts. Decision 6 of the coppers snapshot made it a
measured parameter rather than a constant; this is why that was the right call, and
it was made before any of these numbers existed.

### What this does and does not say about the devices

Does not say: anything about absolute speed on a Cortex-A7. This is a desktop CPU
with a large cache measuring a memory-bandwidth-bound loop. The SSD202D has two
Cortex-A7 cores at 1.2 GHz sharing 128 MB with the OS, and the plot is a pure
sequential write while the scaled blit has much worse source locality — the ratio
between them could move a long way in either direction.

Does say, and this is the useful part:

- The instrument works, and its stages are attributable.
- The naive assumption is false on at least one real driver, so it must be measured
  on the device rather than assumed there either.
- **`rk3326` and `h700` should prefer a low internal resolution and the Miyoo Mini
  probably should not.** That is now a hypothesis with a mechanism, which is a
  better thing to take to hardware than a guess.

### One correction to the method, worth not repeating

The first run of this reported `blit 0.000` and a *present* cost that went **up**
as the layer got smaller, which reads as "scaling is free and presenting is
mysterious". Both were artefacts: SDL batches render commands and executes them at
present, so the blit was being timed as zero and its cost attributed to the swap.
`SDL_RenderFlush()` inside the timed region fixes the attribution. A per-stage
measurement of a batching API measures the batching unless it is told not to.

---

## 2026-07-27 — driver blit versus hand-written blitter

The scroller comparison, taken the same way. `coppers` draws its message two ways
and a button switches between them; both draw the same glyphs from the same sheet,
both with a drop shadow, so the only variable is the mechanism.

| Driver | Scroller | Cost per frame | Overall |
|---|---|---|---|
| software | texture blits | **734 us** | 910 fps |
| software | hand-written | **169 us** | 1117 fps |
| opengl | texture blits | **39 us** | 1153 fps |
| opengl | hand-written | **168 us** | 961 fps |

**The hand-written blitter costs the same on both drivers** — 169 and 168 us — which
it must, being CPU work that never touches the driver. **The texture path swings 19x**,
from 734 us on the software driver to 39 us on the GPU.

So which one wins inverts, by a factor of about 4.3 in each direction, and the
crossover is exactly the presence of a GPU:

- **Miyoo Mini (no GPU): plot the text by hand.** SDL's per-glyph blit path is
  4.3x more expensive than writing the pixels directly.
- **RK3326 / H700 (Mali): use the texture path.** It is 4.3x cheaper there, and it
  also stays crisp when the layer is scaled.

This is the same shape as the internal-resolution finding above, and for the same
underlying reason: anything that hands work to SDL's blitter is cheap when that
blitter is a GPU and expensive when it is a Cortex-A7.

**One asymmetry worth stating, because it flatters the CPU path.** The two are not
doing identical work per pixel. The texture path alpha-blends the whole 16x16 quad
including its transparent margins; the hand blitter tests the 1-bit mask and stores
only where there is ink, with no read-modify-write. That is inherent to the
mechanisms rather than a rigged comparison — SDL_Renderer has no alpha *test*, so
transparency there costs a blend — but the gap is partly "less work", not purely
"less overhead".

### The measurement was wrong twice before it was right

Both times for the same reason, and the second time in a place the first fix did
not cover. **SDL batches render commands and executes them at present**, so a timer
that stops before a flush measures the cost of *queueing* work.

The first version of this table read `texture 4 us` against `cpu 167 us` — the
texture path looking forty times faster, when it is in fact 4.3x slower. Four
microseconds was the time to push ~80 `SDL_RenderCopy` calls into a queue. The real
cost surfaced in the next stage, where it was invisible because it had been folded
in with the layer blit.

The fix is `SDL_RenderFlush()` inside every timed region, and keeping the stages
from nesting so they can be summed. The general lesson is worth more than the
numbers: **an instrument pointed at a batching API measures the batching** unless it
forces execution, and it will do so silently and plausibly.

---

## 2026-07-27 — armv7 runs, and produces identical pixels

`miyoomini`, built in compile-check mode, run under `qemu-arm-static`:

```sh
WREEL_DATA_DIR=$PWD/data SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  qemu-arm-static -L /usr/arm-linux-gnueabihf \
  build/miyoomini/bin/coppers --windowed --no-hud --fps 0 --seconds 2
```

```
coppers: software 640x480 layer, 640x480 window, 1794 frames, 922.3 fps,
         plot 0.366 blit 0.493 present 0.004 ms
```

**The timings are meaningless** — qemu-user is x86 executing ARM instructions with
no cache or memory model at all, so this is not a slow device, it is a fast one
wearing a costume. Do not quote these as a Cortex-A7 figure.

What it does establish is that the whole path executes on armv7: texture creation,
lock, the plotting loop, the scaled blit, present, and the BMP writer.

### Cross-architecture pixel equality

`--screenshot` steps a fixed 1/60 per frame rather than using wall-clock time, so
the image depends only on the frame index. That makes it comparable across builds:

```sh
# armv7 under qemu, and native x86-64, both at frame 3
cmp /tmp/coppers-arm.bmp /tmp/coppers-x86.bmp   # -> identical, 1,228,922 bytes
```

Byte-identical. So the bar arithmetic, the ARGB packing and the blit produce the
same result on both architectures, which matters more here than it looks: `char`
signedness differs between them (D10), and a pixel format written as a packed
`Uint32` would break on a big-endian target. This is also usable as a regression
fixture — a future change that alters the picture will change the bytes.

---

## 2026-07-27 — gamepad enumeration is now output, not a question

Step 4 of this document asks "what the gamepad enumerates as, which
`skratch/input.cc`'s hard-coded Xbox 360 axis mapping certainly gets wrong".
`rig::Pad` turns that into a log line. On any device, `coppers` now records for
every attached pad:

```
input: pad 0 '<name>' guid <32 hex chars>, sdl mapping <yes|NO>
```

and, when SDL has no mapping for it:

```
input: <name> has no SDL mapping — N axes, N buttons, N hats.
       Button order is a GUESS; verify on hardware and add a mapping.
```

On the dev box that reads `input: no pad attached, keyboard only`, which is all
this can establish here — **the keyboard path is verified and the pad path is
not**. The value is that running the demo on a Miyoo Mini now produces the answer
without anyone having to instrument anything first.

The fallback's button order is the conventional retro-handheld one and is very
likely wrong somewhere. It is deliberately not presented as correct: it exists so
the demo is controllable on an unrecognised pad, and the warning says it is
unverified. Once a device reports its GUID, the right fix is an
`SDL_GameControllerAddMapping` string rather than more guessing.

---

## 2026-07-27 — `WREEL_WERROR` was off everywhere

Not a device finding, but it invalidates the verification of everything above it in
this file's history. `option()` does not overwrite an existing cache entry, so the
2026-07-26 flip of `WREEL_WERROR` to `ON` reached a fresh clone and no configured
build directory. All five presets still had `OFF` cached.

Rebuilt with `-DWREEL_WERROR=ON`: **zero warnings and 13/13 tests on all five
presets.** The tree was as clean as claimed; the claim had simply not been tested.
Recorded as D19 in
[the defect inventory](../2026-07-25-cxx17-modernization/defects.md).

---

## Still outstanding

The steps this snapshot lists that no amount of dev-box work can close:

| Step | Needs |
|---|---|
| 3 — Miyoo Mini device toolchain (GCC 8.3) | the toolchain container |
| 4 — on-device run | a Miyoo Mini Plus or Flip |
| 5 — Steam Runtime | the sniper container |
| Mali GLES2 | an RK3326 or H700 device |

`coppers` is now the thing to run for step 4, alongside `wreel-probe`. Between them
they answer the display path, the video driver, the gamepad enumeration, the audio
spec and the fill rate — and `--screenshot`, `--seconds` and `--layer-height` make
each one a command rather than a judgement, which is what makes them usable over
SSH.

A first pass on a device would be:

```sh
wreel-probe > probe.txt
coppers --screenshot frame.bmp --frames 3          # does it draw at all
coppers --seconds 20 --no-hud                      # fill rate, native layer
coppers --seconds 20 --no-hud --layer-height 240   # and scaled
coppers --seconds 20 --no-hud --cpu-scroller       # the other scroller path
cat ~/.local/share/wreel/coppers/coppers.log       # or the firmware's pref path
```

That covers every open question in step 4 except whether upstream SDL2 runs on
stock firmware at all, which is answered by whether the first command works.
