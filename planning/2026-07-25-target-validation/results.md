# Results

Real output, per preset, rather than a summary. The deliverable this snapshot's
checklist asks for.

~~**Nothing here has run on a device.**~~ **A Miyoo Mini Plus ran this from
2026-07-28** — see the entries from that date, which are the only ones taken on
hardware. Everything dated 2026-07-27 comes from the dev box or from qemu, and the
one thing qemu cannot tell you is how fast anything is. Read the caveats before
quoting a figure, and check which machine produced it.

---

## 2026-07-30 — the bundle loses 21.8 MB of GL, and runs on two firmwares

The staged bundle, before and after `bundle-onion` learned to drop the unused
`libGLESv2.so` dependency. Container build, GCC 8.3 `coppers`, steward-fu's
`prebuilt/640x480/` runtime:

```console
$ du -sb App/Coppers                       # before: 30,946,413   after:
9170485
$ ls -l App/Coppers/lib/
-rw-r--r-- 1   55100 libEGL.so
-rw-r--r-- 1   50884 libjson-c.so.5
-rw-r--r-- 1 5736116 libSDL2-2.0.so.0      # libGLESv2.so, 21,775,928, is gone
$ stat -c %s pkg/coppers-0.2.0-onion.tar.gz
3617572                                    # was ~13 MB
```

**Staged 29.5 MiB → 8.7 MiB; tarball ~13 MB → 3.6 MB.** The three checks in
[gles-free-runtime § Verification](../2026-07-29-gles-free-runtime/README.md)
all pass: nothing references the removed library, it is no longer in `NEEDED`,
and `coppers` still resolves every SDL symbol it imports from the staged copy.

### It runs, and on stock firmware as well as OnionOS

From `App/Coppers/stdout.log` off the stock card. This is the first run of this
project on a firmware other than Onion, and the first that got all the way
through with the display working:

```
Linux (none) 4.9.84 #1136 SMP PREEMPT Wed Jun 28 21:28:40 HKT 2023 armv7l
pwd=/mnt/SDCARD/App/Coppers
HOME=/
LD_PRELOAD=/mnt/SDCARD/miyoo354/app/../lib/libpadsp.so
--- /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh not found; audio will likely fail ---
--- library resolution ---
	libSDL2-2.0.so.0 => /mnt/SDCARD/App/Coppers/lib/libSDL2-2.0.so.0
	libEGL.so        => /mnt/SDCARD/App/Coppers/lib/libEGL.so
	libjson-c.so.5   => /mnt/SDCARD/App/Coppers/lib/libjson-c.so.5
	libmi_gfx.so     => /config/lib/libmi_gfx.so
	libz.so.1        => /customer/lib/libz.so.1
	libstdc++.so.6   => /lib/libstdc++.so.6
	libgcc_s.so.1    => /lib/libgcc_s.so.1
[I] gfx system initialised, video driver Mini
[I] input: no pad attached, keyboard only
[W] no desktop mode reported; using exclusive fullscreen against the driver's own mode list (10 modes, first 800x600)
[I] output size: renderer 640x480 (reported success)
[I] renderer context 640x480 via Miyoo Mini (accelerated)
[I] assets: /mnt/SDCARD/App/Coppers/data/ (beside the executable)
[W] Miyoo Mini draws sub-rectangles wrongly; composing everything into the layer
[I] coppers: Miyoo Mini 640x480 layer, 640x480 window, 863 frames, 59.7 fps,
             plot 2.813 blit 4.428 present 9.450 ms, scroller cpu 1035 us
--- exited 0 ---
```

**Sixteen libraries resolved and no `libGLESv2.so` among them.** That is stage 2
of [gles-free-runtime](../2026-07-29-gles-free-runtime/) settled on hardware: the
loader never wanted it, exactly as the symbol table said.

Three of this document's own fixes are visible working: `output size: renderer
640x480 (reported success)` where the 2026-07-28 run got `0x0` (D22), the
fullscreen fallback against the driver's mode list (D24), and the layer-composition
path that D25 forced. `exited 0`.

| Question | Answer |
|---|---|
| Does the bundle run on non-Onion firmware? | **Yes**, unmodified, from the stock Apps menu |
| Is `libGLESv2.so` needed at load? | **No** — 16 libraries resolved, it is not one |
| Does the GL-free bundle draw? | Yes. 863 frames, 59.7 fps, vsync-bound |
| Are the three base libraries present on stock? | Yes — `libstdc++.so.6`, `libz.so.1`, `libgcc_s.so.1` all resolved, the last two out of `/customer/lib` |
| `HOME` on stock | `/` — unusable, and a *different* wrong value from Onion's `/mnt/SDCARD/RetroArch/` |
| Is `libpadsp.so` an Onion thing? | **No.** Stock preloads it too, from `miyoo354/lib/` |
| Audio | Fails. Same MI_AO contention as Onion, and stock ships no remedy |

The stock-firmware result is the surprising one. Every firmware-specific
decision in [onion-bundle](../2026-07-27-onion-bundle/) was made for Onion, and
`docs/MIYOO-MINI.md § 6` was written as if Onion were the only firmware this
bundle targets.

**It launched the same way**: the same `App/Coppers/` directory at the same
path, `config.json` unmodified, listed and started by the stock Apps menu. So
the App layout is stock MainUI's and Onion inherited it — the bundle is not
Onion-specific, and `WREEL_TARGET_FIRMWARE=onion` is a narrower name than what
it builds. Recorded in `docs/MIYOO-MINI.md § 6.1` and in the layout's comment in
`cmake/Packaging.cmake`; the firmware value is left alone until a second
firmware needs a layout that actually differs.

### The audio: same fault, no remedy shipped

The HUD's `silent` is `Playlist::current()` being empty, which
[playlist.cc](../../coppers/playlist.cc) reaches three ways. The log picks one:

```
[MI ERR ]: MI_AO_SetPubAttr[3364]: Dev0 failed to set pub attr!!! error number:0xa0052009!!!
[MI ERR ]: MI_AO_DisableChn[3667]: Dev0 has not been enabled.
[W] audio: Mix_OpenAudio failed (); continuing without sound
[W] music: no audio device, running in silence
```

**The device never opened, and the error number is identical to Onion's.** So it
is not a stock-specific fault; it is the same single-owner MI_AO contention, on a
firmware that ships no `stop_audioserver.sh` to release it.

One hypothesis is disposed of without a device trip. `libpadsp.so` is preloaded
on stock as well as Onion, so "keep the shim and let it proxy the audio" is the
obvious thing to try — and it cannot work. The vendored SDL2 compiles in exactly
one audio driver and has no OSS path for a dsp shim to intercept:

```console
$ strings -a libSDL2-2.0.so.0 | grep -xiE 'mmiyoo|mini|Miyoo Mini|dsp|oss|alsa|pulseaudio|dummy|disk' | sort -u
mini
Mini
Miyoo Mini
$ strings -a libSDL2-2.0.so.0 | grep -iE '/dev/dsp|/dev/audio|soundcard\.h|SDL_PATH_DSP'
(nothing)
```

Unsetting `LD_PRELOAD` is therefore right on both firmwares, and the remaining
question is narrow: **which process holds MI_AO on stock, and is stopping it
advisable?** The process table answers the first and nothing on a dev box can.
Onion's script is firmware-supplied and MainUI restarts what it stops; a
hand-rolled kill on stock would be neither, which is why this is not simply
"port the Onion branch".

Also found here, and recorded as **D26**: `Mix_OpenAudio failed ()` — the vendor
driver fails without calling `SDL_SetError`, so the logger has nothing to print
and the only description of the failure went to stdout. The launcher capturing
stdout is what preserved it.

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
the image depends only on the frame index. That makes it comparable across builds —
**with `--no-hud`, which the recipe originally recorded here omitted**:

```sh
# armv7 under qemu, and native x86-64, both at frame 3
cmp /tmp/coppers-arm.bmp /tmp/coppers-x86.bmp   # -> identical, 1,228,922 bytes
```

Byte-identical. So the bar arithmetic, the ARGB packing and the blit produce the
same result on both architectures, which matters more here than it looks: `char`
signedness differs between them (D10), and a pixel format written as a packed
`Uint32` would break on a big-endian target. This is also usable as a regression
fixture — a future change that alters the picture will change the bytes.

> **Corrected 2026-07-27, when the fixture was re-run against a third build.** The
> conclusion stands and is now stronger; the *recipe* above was not reproducible.
>
> The HUD reports measured microseconds, so with it on — which is the default —
> **the same binary does not agree with itself between two runs**:
>
> ```sh
> coppers --screenshot a.bmp --frames 3     # HUD on, the default
> coppers --screenshot b.bmp --frames 3
> cmp a.bmp b.bmp     # -> differ at byte 1196455
>
> coppers --screenshot a.bmp --frames 3 --no-hud
> coppers --screenshot b.bmp --frames 3 --no-hud
> cmp a.bmp b.bmp     # -> identical
> ```
>
> So `--no-hud` is not optional for this comparison, it is what makes it a
> comparison. Whether the original run passed it and failed to record it, or
> compared file sizes, cannot be recovered — but as written the check fails.
>
> With it, the equality now covers **three** builds rather than two, all
> byte-identical at 1,228,922 bytes:
>
> | Build | Compiler | Flags |
> |---|---|---|
> | x86-64 | GCC 12.2 | `-g` |
> | armv7 (Debian cross) | GCC 12.2 | `-Os` |
> | armv7 (**device toolchain**) | **GCC 8.3** | `-Os -mcpu=cortex-a7 -mfpu=neon-vfpv4` |
>
> The third row is the one worth having. Same pixels out of a compiler four major
> versions older, at `-Os`, with NEON and hard-float enabled — so nothing in the
> bar arithmetic depends on GCC 12's codegen or on x86 floating point, which was an
> assumption nobody had tested until there was a device toolchain to test it with.

---

## 2026-07-28 — step 4: it runs on a Miyoo Mini Plus

**The first time any part of this project has executed on hardware.** OnionOS, via
the `App/Coppers` bundle, launched from the Apps menu with no arguments.

What worked on the first real attempt: the GCC 8.3 binary loaded, the vendored
SDL2 resolved, assets resolved beside the executable, the mixer opened at the
handheld profile, **tracker music played**, and **input responded**. What did not:
nothing appeared on the panel.

```
Linux (none) 4.9.84 #1133 SMP PREEMPT Fri May  5 21:30:37 PDT 2023 armv7l
libSDL2-2.0.so.0 => /mnt/SDCARD/App/Coppers/lib/libSDL2-2.0.so.0
libmi_gfx.so     => /config/lib/libmi_gfx.so
...
[I] gfx system initialised, video driver Mini
[I] renderer context 0x0 via Miyoo Mini (accelerated)
[I] assets: /mnt/SDCARD/App/Coppers/data/ (beside the executable)
[I] audio: 22050 Hz, 2 ch, 2048 sample buffer, 8 voices, driver Miyoo Mini
[I] music: playing complications.mod
[I] layer: 1x1
```

**`0x0`.** `SDL_GetRendererOutputSize` returns a zero size on this driver *and
reports success*, and the return value was not checked, so the layer clamped
itself to 1x1 and every frame drew nothing (D22). The black panel was not the
display path failing — the display path was never asked to show anything.

Underneath it, a second independent fault: 2437 frames each produced

```
[E] draw_surface: could not upload texture: Texture dimensions are limited to 640x480
```

The device caps textures at the panel size, and the HUD's single line rasterises
to ~735px (D23). It would have been missing even with the size right.

### What this run settles

| Question, and where it was asked | Answer |
|---|---|
| Does upstream SDL2 work here? | No, and the vendored `mmiyoo` SDL2 does. `video driver Mini` |
| Is `WREEL_USE_SYSTEM_SDL2` the right call? | Yes. Vendored runtime loaded from the bundle's `lib/`; `libmi_*` came from `/config/lib` |
| Does `rig::asset_path()` work on a device? | **Yes** — `(beside the executable)`, first try, from a firmware launcher |
| Does the mixer open? | Yes, at the handheld profile, once `audioserver` is stopped |
| Is the audio contention real? | Yes — `MI_AO_SetPubAttr ... 0xa0052009` before the fix, silent after |
| Max texture size | **640x480**, the panel exactly. New constraint, `docs/TARGETS.md § 3a` |
| Render driver | Reports as `Miyoo Mini (accelerated)` — *not* `software`, on a device with no GPU |
| Fill rate | **Still unmeasured.** Nothing was drawn |

The last row is the point: the measurement this whole snapshot exists for did not
happen, because the instrument was reporting a 1x1 layer. Both faults are fixed
and the bundle rebuilt; the numbers need another trip.

### Two notes worth keeping

**`HOME` was `/mnt/SDCARD/RetroArch/`** before `launch.sh` overrode it — so
decision 4 of the bundle snapshot was load-bearing rather than tidy. Without it
the run log would have gone into RetroArch's directory.

**Onion preloads `libpadsp.so`** into launched programs (`LD_PRELOAD` was set at
entry). The launcher unsets it, as Onion's own ports launcher does.

---

## 2026-07-27 — step 3: GCC 8.3 compiles this codebase, after one fix

**The device toolchain build, which this document has called "the single most
valuable step" since it was written.** Run inside `union-miyoomini-toolchain` via
`docker/miyoomini.Dockerfile`, which layers CMake 3.31.6 and Ninja over the base
image's Debian 10 CMake 3.13.

```console
$ arm-linux-gnueabihf-g++ --version
arm-linux-gnueabihf-g++ (GNU Toolchain for the A-profile Architecture 8.3-2019.03
                         (arm-rel-8.36)) 8.3.0
```

**First build: 53 errors, one cause.** `POSIX_ERROR_DECL` ends in a semicolon and
so does every use of it, making each an empty declaration at namespace scope.
Legal since C++11, silent on GCC 12 under `-Wpedantic`, an error on GCC 8.3.
Recorded as [D21](../2026-07-25-cxx17-modernization/defects.md) and fixed by
dropping the semicolon from the macro.

**Second build: zero errors, zero warnings, with `WREEL_WERROR=ON`.**

Everything this document predicted would bite did not:

| Predicted risk | Outcome on GCC 8.3 |
|---|---|
| `nlohmann/json` 3.12 | compiles |
| doctest under `DOCTEST_CONFIG_SUPER_FAST_ASSERTS` | compiles, and all 15 suites pass |
| SDL2 2.32 against a 2019 sysroot | compiles |
| `std::from_chars` for integers | present, `util::from_string` works |
| `inline constexpr` callables in `util/ascii.hpp` | compiles |
| `if constexpr` in `util/number.hpp` | compiles |
| pugixml 1.16, glm 1.0.3 | compile |
| `std::filesystem` / `-lstdc++fs` link | resolves |
| `-Os` codegen | see the pixel-equality note below — identical output |

So the C++17 ceiling recorded in `docs/TARGETS.md` is accurate: the language level
this project restricted itself to is the language level this compiler has. The one
failure was not a C++17 gap at all, it was a pedantic diagnostic that four newer
compilers had been letting through.

### The binary, and why it is the first shippable one

```console
$ readelf -d build/gcc83/bin/coppers | grep NEEDED
  libdl.so.2   libm.so.6   libpthread.so.0   libc.so.6   ld-linux-armhf.so.3

$ objdump -T build/gcc83/bin/coppers | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1
GLIBC_2.28
```

`GLIBC_2.28`, against the `GLIBC_2.36` the Debian cross-GCC build requires — which
is the whole of `docs/TARGETS.md § 2` in two lines. The toolchain's sysroot is
glibc 2.28, so that is the floor these binaries carry, and Onion's own SDL2 needs
at most 2.27, which is consistent with a device at or above 2.28.

### It runs

15/15 doctest suites under `qemu-arm-static` against the toolchain's sysroot:

```sh
qemu-arm-static -L <sysroot> build/gcc83/bin/test_*      # 15 passed, 0 failed
```

The usual caveat applies and is worth repeating: **qemu says nothing about speed.**
It says the code executes.

**Note what this build is not.** It links the pinned static SDL2, which has no
video driver for this device, so on hardware it will run headless and draw
nothing. That is still a useful first artefact — `SDL_VIDEODRIVER=dummy coppers
--screenshot` over SSH proves a GCC 8.3 binary loads and executes on the device
without needing the display question answered first. The display needs the
firmware's SDL2; see
[onion-bundle](../2026-07-27-onion-bundle/).

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

---

## 2026-07-28 — the software renderer cannot present, and that settles the model

The open question from the bundle work was whether the Miyoo Mini's crippled
`mini` render backend could be sidestepped by asking for SDL's own software
renderer, which is compiled into the same library. **It can be selected, and it
cannot present.**

```
--- local.env ---
SDL_RENDER_DRIVER=software
[I] renderer context 640x480 via software (software)
[I] coppers: software 320x240 layer, 640x480 window, 965 frames, 59.4 fps,
             plot 0.838 blit 5.371 present 0.004 ms, scroller cpu 493 us
```

Black screen, 965 frames, and a **present cost of 4 microseconds** where the
`mini` backend spends 11-13 ms waiting on the flip. Nothing reached the panel.
The reason is three lines of the vendor driver:

```c
int Mini_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    debug("%s\n", __func__);
    return 0;
}
```

SDL's software renderer composites into a window surface and presents it with
`SDL_UpdateWindowSurface`, which routes to that function. It is a no-op. The
surface is created correctly (`Mini_CreateWindowFramebuffer` really does make
one), drawn into correctly, and then discarded. The only route to the panel in
this library is `Mini_QueueCopy` -> `GFX_Copy` -> `GFX_Flip`, which belongs to
the `mini` render backend.

**So the choice that looked like a choice is not one.** On this device:

- The panel can only be reached through the `mini` backend.
- That backend implements one drawing operation, `RenderCopy`, always rotated
  180 degrees with x mirrored (D25). Fills, points, geometry, `CopyEx` and
  `ReadPixels` do nothing.
- A full-screen copy survives the rotation; a sub-rectangle copy does not.

Which leaves exactly one usable model, and it is the one the hardware was asking
for all along: **compose the whole frame yourself into a single full-screen
streaming texture, and hand it over once.** A software double buffer with a
hardware blitter and a page flip underneath it — `yres_virtual = yres * 2`,
`MI_GFX_BitBlit` into the back page, `FBIOPAN_DISPLAY` to flip.

That is not a workaround. It is what `gfx::renderer::Layer` already is, and the
measurements say it is comfortable: **0.84 ms to plot a 320x240 field, 1.05 ms
for 640x480**, against a 16.7 ms frame. The fill-rate anxiety that motivated
this whole snapshot was misplaced by a wide margin; the constraint was never
bandwidth, it was what the driver would accept.

### Two false starts worth recording, because both looked like driver bugs

The first `SDL_RENDER_DRIVER=software` run had the file named `locals.env`, so it
was never sourced. The second was sourced and still had no effect, because a
sourced `FOO=bar` sets a **shell** variable and a child process receives the
**environment** — the launcher echoed `software` from its own shell while
`coppers` was never told. Fixed with `set -a` around the source.

Both failures presented identically to "SDL ignored the hint", and the second
sent this investigation into the library's symbol table to check whether
`SW_RenderDriver` was even registered. It was. The lesson is narrow and worth
keeping: **a diagnostic that prints a value the program was never given is worse
than no diagnostic**, and the launcher was doing exactly that.

---

## 2026-08-01 — our own SDL2 on the device, and the conformance baseline

Two runs, both from `pkg/coppers-0.2.0-onion.tar.gz` built with
`WREEL_MINI_SDL2=ON` — pinned upstream SDL 2.32.10 with the SSD202D drivers
compiled in. Reasoning and the full account are in
[miyoo-sdl2-fork § 8](../2026-07-31-miyoo-sdl2-fork/); this is the raw output,
kept because it is the baseline every subsequent driver patch is diffed against.

### `coppers` — the gate

```
[I] gfx system initialised, video driver Mini
[W] no desktop mode reported; using exclusive fullscreen against the driver's own mode list (10 modes, first 800x600)
[I] output size: renderer 640x480 (reported success)
[I] renderer context 640x480 via Miyoo Mini (accelerated)
[W] Miyoo Mini draws sub-rectangles wrongly; composing everything into the layer
[I] audio: 22050 Hz, 2 ch, 2048 sample buffer, 8 voices, driver Miyoo Mini
[I] coppers: Miyoo Mini 640x480 layer, 640x480 window, 859 frames, 59.7 fps,
    plot 2.920 blit 4.425 present 9.348 ms, scroller cpu 1054 us
```

Indistinguishable from the prebuilt it replaces. The loader resolves six
non-system libraries and **none of them is EGL, GLESv2 or json-c**:

```
libSDL2-2.0.so.0 => /mnt/SDCARD/App/Coppers/lib/libSDL2-2.0.so.0
libmi_gfx.so, libmi_ao.so, libmi_sys.so, libmi_common.so => /config/lib/
libshmvar.so => /customer/lib/
```

Note `/config/lib`, which is not a path `launch.sh` adds — it is already on the
default search path on this firmware.

### `wreel-diag` — the conformance baseline

Machine: `INFINITY2M SSC011A-S01A-S`, Linux 4.9.84 armv7l, 2 cores, 100 MB RAM
reported, `/dev/fb0` 640x480 at 32 bpp, stride 2560.

```
== Orientation ==
  drawing size             640x480
  readback via             /dev/fb0
  expected                 TL=red TR=green BL=blue BR=white
  observed                 TL=ffffff TR=0000e0 BL=00e000 BR=e00000
  screen transform         info      content reaches the panel rotated 180

== Renderer conformance ==
  SDL_UpdateTexture copies WRONG     the driver kept the caller's pointer and read it at draw time
  SDL_RenderClear          IGNORED   screen still holds the previous frame
  SDL_RenderCopy full      OK
  SDL_RenderCopy sub-rect  WRONG     read past the rows that were staged, returned the previous blit's leftovers
  sub-rect, RGB565         WRONG     80x60 sub-rect of solid green arrived as e007e0; pitch/rect.w is 16
  requested dst            80,60 160x120
  in the framebuffer       400,60 160x120
  as seen on the panel     80,300 160x120
  partial destination      WRONG     x is right and y is mirrored — expected y=60, got y=300
  SDL_SetTextureBlendMode  IGNORED   sprite is opaque (00ff00); mode accepted (mode=1), never applied
  SDL_SetTextureColorMod   IGNORED   texture arrived unmodulated
  SDL_RenderFillRect       IGNORED   returned success and drew nothing

== Texture limits ==
  advertised max           640x480
  create at the limit      OK
  create over the limit    OK        refused, as advertised
  render to texture        IGNORED   the target texture is empty; TARGETTEXTURE is advertised anyway

== Audio ==
  driver                   Miyoo Mini
  SDL_OpenAudioDevice      OK        22050 Hz, 2 ch, 2048 samples
```

**Every row above matches what § 1 and § 2 of the fork snapshot predicted from
source.** The two that read `OK` are the two that should: a full-screen copy is
the one operation this backend implements, and the texture cap is both advertised
and enforced.

`SDL_GetDesktopDisplayMode` and `SDL_GetDisplayBounds` both returned **success
with zeroed structures** — D22 and D24, unfixed, on our build as much as on the
prebuilt.

### How to reproduce, and what to compare against

```sh
cmake --build build/miyoomini --target bundle-onion
# copy pkg/*.tar.gz over the SD card root; run "Wreel Diagnostics" from Apps
# read App/WreelDiag/diag.txt
```

The control is the same binary on `desktop-software`, which runs against SDL's
own software renderer and returns `OK` on all eleven checks. A line that reads
`OK` there and `IGNORED` here is a gap in this driver; `IGNORED` in both is a bug
in the check — and three of them were, first time round. See § 8.4.
