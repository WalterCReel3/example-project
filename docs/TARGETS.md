# Targets

The project builds one codebase for three quite different worlds: a desktop dev
box, Steam on Linux, and Linux-based retro handhelds. The handhelds are what
constrain everything — this page records those constraints so decisions
elsewhere don't have to re-derive them.

## Target matrix

| Preset | Devices | SoC | Arch | GPU | `gfx::renderer` driver | Toolchain |
|---|---|---|---|---|---|---|
| `desktop-debug` / `-release` | your dev box | any | `x86_64` | any | `opengl` | host GCC/Clang |
| `desktop-software` | your dev box | any | `x86_64` | any | `software` (forced) | host GCC/Clang |
| `steam` | Steam / Steam Deck | any | `x86_64` | any | `opengl` | Steam Runtime **sniper** container |
| `miyoomini` | Miyoo Mini, Mini Plus, Mini Flip | SigmaStar SSD202D | `armv7-a` | **none** | `software` **only** | `union-miyoomini-toolchain` (GCC 8.3) |
| `rk3326` | RG351P/M/V, RG353P/M/V | Rockchip RK3326 | `aarch64` | Mali-G31 | `opengles2` | device sysroot or Debian cross |
| `h700` | RG35XX Plus/H/SP, RG40XX | Allwinner H700 | `aarch64` | Mali-G31 MP2 | `opengles2` | device sysroot or Debian cross |

Every preset builds `gfx::renderer`; the driver column is what SDL selects
underneath it. The two Mali rows were `software` until 2026-07-26, when D18 was
fixed — they had been compiled as though they had no GPU. Those two are
**build-verified only**: no part of this has run on hardware.

**One preset, three panels.** All three Miyoo devices are the same SSD202D with two
Cortex-A7 cores at 1.2 GHz and 128 MB of DDR3, so one toolchain row covers them —
but their displays differ, and the newest is the most expensive:

| Device | Panel | Pixels |
|---|---|---|
| Miyoo Mini | 2.8", 640×480 | 307,200 |
| Miyoo Mini Plus | 3.5", 640×480 | 307,200 |
| Miyoo Mini Flip | 2.8", **752×560** | **421,120** |

The Flip is therefore the binding target for anything fill-rate bound: 37% more
pixels than the other two on identical silicon. 752×560 is also a non-standard
mode, so whether the firmware reports it to SDL or interposes a scaler is a real
unknown — ask `wreel-probe` on the device rather than assuming. Note that several
planning documents pose cost questions at "320×240", which matches no device here;
see [planning/2026-07-26-coppers-cracktro](../planning/2026-07-26-coppers-cracktro/).

`desktop-software` is how you exercise the Miyoo Mini code path without a device.
It sets `WREEL_TARGET_HAS_GPU=OFF`, so SDL is built with no GL at all and the
software driver is the only one available — the same situation as the SSD202D,
at native speed and without a cross-compiler.

### Out of scope

- **Original RG35XX (2022)** — Allwinner F1C100s, ARM926EJ-S, `armv5te`, no FPU.
  Soft-float only. Supporting it would mean a second ABI and a no-FPU math path;
  not worth it.
- **Windows / macOS.** The 2016 tree had a Win32 file backend
  ([util/mswin/fileimpl.cc](../util/mswin/fileimpl.cc)) and scattered `__APPLE__`
  branches. Those stay in the tree because the platform-pimpl pattern is worth
  keeping, but neither is a build target and neither is tested.

## The three constraints that actually matter

### 1. C++17 is the ceiling, and GCC 8.3 sets it

The Miyoo Mini toolchain
([union-miyoomini-toolchain](https://github.com/shauninman/union-miyoomini-toolchain))
bundles the **GNU Toolchain for A-profile 8.3-2019.03** — that is **GCC 8.3**.
It is the oldest compiler in the matrix, so it defines the language floor for
all shared code.

GCC 8.3 has good C++17 language support but an incomplete C++17 library. What
this rules out:

| Not available | Needs | Use instead |
|---|---|---|
| `std::from_chars` / `to_chars` for **float/double** | GCC 11 | `strtod` / `strtof` / `snprintf` |
| `std::span` | GCC 10 (C++20) | pointer + length, or a small local `span` |
| Ranges, concepts, `<=>`, `consteval` | GCC 10+ (C++20) | — |
| Parallel STL (`std::execution`) | needs TBB | — |
| `std::filesystem` **without** `-lstdc++fs` | GCC 10 | link `stdc++fs` explicitly on this target |
| `<ascii>` — ASCII character classification | C++26 ([P3688](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3688r6.html)) | `include/util/ascii.hpp`, which borrows P3688's names |

Integer `std::from_chars` **is** available in GCC 8. `std::optional`,
`std::variant`, `std::string_view`, structured bindings, `if constexpr`,
`std::not_fn`, and fold expressions are all fine. Inline variables are available,
which is what lets `util/ascii.hpp` expose `inline constexpr` predicate objects.

> This is why [util/number.hpp](../include/util/number.hpp) dispatches on type:
> integers go through `std::from_chars`, which GCC 8 has and which is
> locale-independent, while floating point falls back to the `strtod` family. It is
> the one place in the tree that has to care, and `util::from_string` is what
> everything else — the OBJ loader, `util::xml`, TMX — calls instead.

> **Do not use `<cctype>` for parsing.** `::isspace` and friends are
> locale-dependent, take `int`, and are undefined for negative `char` — and `char`
> is signed on the x86-64 dev box but **unsigned** on both ARM targets, so the same
> asset byte takes a different path on the dev box than on any device. Use
> `util::ascii_*`. `SDL_isspace` is not an alternative: SDL forwards it to
> `::isspace` whenever `HAVE_CTYPE_H` is set, which is every target here. Full
> reasoning in
> [planning/2026-07-25-cxx17-modernization § Decisions](../planning/2026-07-25-cxx17-modernization/README.md).

### 1a. No iostreams in shipped code

`<iostream>`, `<fstream>`, `<sstream>` and `<iomanip>` are not to be included by
anything that ships. They cost far more than they look like they do, and the cost
lands on the most constrained target.

Measured on armv7, statically linked at `-Os`:

| Program | Stripped size |
|---|---|
| `<cstdio>` only, including `%f` conversions | 366,948 |
| + iostreams (one `std::cerr <<`) | 963,240 |
| + fmt 12.2.0 instead (header-only) | 901,576 |

So iostreams add **596 KB**, roughly 1.6× the entire `<cstdio>`-only C++ runtime
floor. The real saving is larger than that in practice: removing them from
`util/logging.hpp` took the armv7 `wreel-probe` from 3,014,624 to 2,148,792
bytes — **865 KB, 28%** — because the locale transliteration tables and the C++
demangler went with them.

`util::logging` is printf-style for this reason. Format strings are still checked
at compile time, because each function carries
`__attribute__((format(printf, 1, 2)))` and the build enables `-Wformat=2`.

Two related findings worth not re-deriving:

- **`std::print` is not an option.** It is C++23 and needs GCC 14; `std::format`
  needs GCC 13. The *host* GCC 12.2 has neither header, so this is not merely a
  GCC 8.3 limitation.
- **fmt is not the escape either.** As the reference implementation of both, it
  looks like the obvious modern answer, but measured above it costs 522 KB over
  plain `printf` and is only ~60 KB better than the iostreams it would replace —
  it reaches the same glibc locale and demangler machinery through its exception
  and RTTI paths. `-fno-exceptions` recovered about 4 KB. That is fmt's default
  configuration, not a tuned one.

### 2. The glibc forward-compatibility trap

This is the single most common way handheld builds fail, and it fails at
*runtime*, not build time.

Debian 12 has glibc 2.36. If you cross-compile with Debian's
`arm-linux-gnueabihf-g++`, the resulting binary carries `GLIBC_2.36` symbol
requirements. Copy it to a handheld running an older glibc and you get:

```
./mygame: /lib/libc.so.6: version `GLIBC_2.36' not found
```

Debian's cross-GCC is therefore **compile-check only**. It is genuinely useful
for that — catching `-Werror` breakage and ABI mistakes fast without spinning a
container — but never ship its output.

To find a device's actual glibc, on the device:

```sh
ldd --version | head -1
# or, if ldd is a busybox stub:
strings /lib/libc.so.6 | grep -oE 'GLIBC_2\.[0-9]+' | sort -Vu | tail -1
```

Shippable device binaries come from the device SDK container, which is built
against a matching (old) glibc. Static-linking `libstdc++` and `libgcc`
(`-static-libstdc++ -static-libgcc`) removes the *C++* runtime half of the
problem but not the libc half.

> **The Miyoo Mini has its own reference document.** Everything learned about
> that platform from running on it — the vendor SDL2 forks and what they can and
> cannot do, the presentation pipeline, the firmware's audio and input
> behaviour, the measured costs, and the alternatives that were considered and
> rejected — is in [MIYOO-MINI.md](MIYOO-MINI.md), with sources. The two
> constraints below are the ones that change decisions elsewhere in this file.

### 3a. On the Miyoo Mini, no texture may be larger than the panel

Measured on hardware 2026-07-27, not inferred. The device's `SDL_Renderer`
reports a **maximum texture size of 640x480** — the panel exactly. Every desktop
driver in this matrix allows 16384, so nothing in the tree had ever come near it.

This is a design constraint, not a curiosity, and it is invisible everywhere else:

- Any atlas, tilemap page or pre-rendered background wider than 640 or taller
  than 480 **cannot be uploaded on this target**. `data/glyphs-16x16.png` is
  320x48 and safe; a 1024-wide sheet would fail.
- A single line of rasterised text can exceed it. `coppers`' HUD did, at ~735px,
  and failed to upload on all 2437 frames of the first device run (D23).
- `SDL_CreateTexture` fails with `Texture dimensions are limited to 640x480`
  rather than degrading, so the symptom is a missing element rather than a
  scaled one.

The related trap is the Mini Flip, and the shape of it was **corrected 2026-07-31**
by reading the driver source rather than a view of it. This section used to say
the geometry is compiled in "with no runtime override", in the `mmiyoo` driver —
wrong on both counts. The shipped driver is `mini`, and it *does* detect the Flip:

```c
if (strstr(buf, "752")) { FB_W = 752; FB_H = 560; ... }   /* SDL_video_mini.c */
```

**What does not follow is the texture cap.** `max_texture_width/height` are
literals of 640 and 480 in `SDL_render_mini.c`, unrelated to `FB_W`/`FB_H`. So on
a Flip this binary drives the 752×560 panel and then refuses to create any
texture wider than 640 — a full-screen `Layer` is impossible, and the failure is
`SDL_CreateTexture` returning an error rather than anything visibly geometric.

The conclusion is unchanged and firmer: **the Flip needs a patched build**, not
merely a differently-configured one. Note also 752, not the 750 this document
carried — the number comes from the driver's own detection. See
[MIYOO-MINI.md § 4.1](MIYOO-MINI.md) and
[planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/).

### 3. Miyoo Mini has no GPU

The SSD202D is two Cortex-A7 cores and **no 3D block**. There is no OpenGL, no
GLES, no EGL. Everything is CPU blitting through `SDL_Renderer`, which the vendor
fork routes to its own `MI_GFX` layer.

There *is* a 2D block, and it is better than this project uses. `MI_GFX` does
scaled and rotated blits, rectangle fills, colour-keying and alpha blending in
hardware — the SDL2 port exposes one full-screen copy and stubs the rest, which
is a fact about the port. See [MIYOO-MINI.md § 4.6](MIYOO-MINI.md). "No GPU"
means no programmable 3D pipeline, not no acceleration.

Consequences:

- `gfx::renderer` with its software driver is not a fallback here, it is the
  **only** thing that runs, so it has to be good enough to be the baseline
  everywhere. `WREEL_ENABLE_GLES2` is rejected outright on this target.
- `WREEL_ENABLE_GLES2` is **rejected** here rather than silently downgraded, and
  `WREEL_BUILD_DEMOS` turns itself off because `skratch` needs that renderer. The
  2016 fixed-function code that could never compile for this target is gone
  entirely.
- RAM is **128 MB total**, shared with the OS. Asset budgets are tight and
  unbounded caches are not an option.

## Modules

Dependencies run one way. `wreel::rig` is the newest and the one whose boundary is
easiest to get wrong:

| Module | For | Links SDL? |
|---|---|---|
| `posix` | typed errno exceptions | no |
| `util` | tokenizers, number conversion, logging, file I/O, XML | **no, deliberately** |
| `rig` | what a realtime program needs from its surroundings: asset and preference paths, frame timing, input mapping | yes |
| `audio` | sound and music over SDL2_mixer | yes, and hides it |
| `gfx` | `renderer` always, `gles2` where there is a GPU | yes |
| `loaders` | asset formats to plain data | yes |

**Why `rig` exists rather than putting this in `util`.** `util` is generic to any
application on any platform and links no SDL. Asset resolution wants
`SDL_GetBasePath()` and frame pacing wants a sleep, so putting either there would
have put the tokenizer and number tests downstream of a windowing toolkit for the
sake of two functions. The split is by what code is *for*, not by how generic it
looks: `rig` is realtime-application services, and its charter — including what
does **not** belong in it — is stated in [rig/CMakeLists.txt](../rig/CMakeLists.txt),
because a name that broad invites a dumping ground.

## Graphics renderers

**Renderers are capabilities, not alternatives.** There is no
`-DWREEL_GFX_BACKEND=<name>` any more: it selected one of two mutually exclusive
implementations of one interface, which was the right model while both were ways
to put pixels on the same screen. The two that survive are not interchangeable, so
a build compiles both and each executable chooses. Full reasoning in
[planning/2026-07-26-gfx-renderer-and-gles2](../planning/2026-07-26-gfx-renderer-and-gles2/).

| Renderer | Built | API | Draws | Status |
|---|---|---|---|---|
| `gfx::renderer` | **always** | `SDL_Renderer` | textures, atlases, tilemaps, text — the game | **implemented** — the baseline |
| `gfx::gles2` | `WREEL_ENABLE_GLES2` | GLES 2.0 context we own | anything a shader can express; 3D | **implemented**, and `skratch` renders through it. Never run on a device |
| `gl33` | — | GL 3.3 core | — | only if Steam needs something `gles2` cannot give |

A window is driven by one renderer or the other. `SDL_Renderer` owns its window's
GL context internally, and mixing our own GL calls into it is possible in
SDL ≥ 2.0.10 but not worth the state-restoration discipline.

### `gfx::renderer`'s driver is not always software

This is the point the old `software` name obscured, and why the namespace was
renamed. SDL picks a **render driver** underneath `SDL_Renderer`:

| Target | Driver | Consequence |
|---|---|---|
| Miyoo Mini | `software` | two Cortex-A7 cores doing the blitting |
| RK3326, H700 | `opengles2` | **hardware accelerated**, same source |
| desktop | `opengl` | whatever Mesa offers |

So the 2D game path is GPU-accelerated on the Mali handhelds without a line of GL
in this project. `gfx::renderer::Driver` selects among `PreferAccelerated` (the
default), `Accelerated` and `Software`; `Context::driver_name()` and
`accelerated()` report what SDL actually gave, and `tests/test_renderer.cc`
asserts the resolution rather than trusting the log line.

`PreferAccelerated` degrades to software rather than failing, for the same reason
`audio::Device` tolerates a missing audio device: a firmware with broken vendor
blobs should still boot into a playable game.

**`WREEL_TARGET_HAS_GPU` means device capability and nothing else.** It is consumed
by [Dependencies.cmake](../cmake/Dependencies.cmake) to decide whether SDL2 is
built with GL/GLES/EGL at all, so using it to record which *renderer* is ready
silently disables the accelerated driver — which is exactly what had happened to
`rk3326` and `h700` (D18). Renderer readiness is `WREEL_ENABLE_GLES2`.

**The 2016 backend was not ported forward, and could not have been.** GL 3.3 core
removed the entire fixed-function pipeline it was built on, so `skratch` was
rewritten against `gfx::gles2` rather than migrated: explicit matrices instead of
`glRotatef` on a driver-side stack, `glm::perspective` instead of `gluPerspective`,
shaders instead of immediate-mode state. It is kept deliberately as the worked
example of that difference. A `gl33` renderer, if Steam ever needs something
`gles2` cannot give, would be a rewrite for the same reason.

### What actually gets built

The gates are in [gfx/CMakeLists.txt](../gfx/CMakeLists.txt), and they reach
further than `gfx` itself:

| | always | `+ WREEL_ENABLE_GLES2` |
|---|---|---|
| `gfx` | `spritesheet.cc`, `system.cc`, `renderer/` | plus `gles2/` |
| `loaders` | `image.cc`, `obj.cc`, `sparrow.cc` | — |
| `rig` | `assets.cc`, `timing.cc` | — |
| `skratch` demo | **not built** | built |
| `wreel-probe` | built | built |
| tests | built | built |

`skratch` is the only thing here that needs `gles2`, and until 2026-07-27 its
requirement was expressed as `WREEL_BUILD_DEMOS=OFF` — which switched off *all*
demos on exactly the targets where a `gfx::renderer` demo is most wanted. The
requirement now sits on `WREEL_BUILD_SKRATCH`, and `WREEL_BUILD_DEMOS` is the
umbrella.

Nothing in `loaders` is gated any more. `obj.cc` used to be, because it filled a
`gfx::ObjModel` holding `GLuint` buffer handles — a text parser that transitively
included `SDL_opengl.h`. It produces `gfx::Mesh` now, so it builds and is tested on
every target including the GPU-less one.

`skratch` needs `gfx::gles2`, so the configure step disables the demo on a target
without a GPU rather than failing.

## Audio

Audio is a **base requirement**: `wreel::audio` is built unconditionally on every
target, and there is no option to disable it. What varies is the codec set and the
mixer profile.

### The cost model, which is easy to get backwards

Two independent things, often conflated:

| | Controlled by | Costs |
|---|---|---|
| **Codec set** | `WREEL_AUDIO_CODECS` | **binary size only** |
| **Mixer profile** | `WREEL_AUDIO_RATE` / `_BUFFER` / `_CHANNELS` / `_VOICES` | **per-frame CPU** |

SDL2_mixer picks a decoder from the file's contents at load time, so a decoder
that never sees a matching file never executes. A FLAC-capable build does not slow
down a game that only plays WAV. Per-frame cost comes from mixing work — rate,
channel count, voice count — which happens in every audio callback regardless of
what is playing.

Measured, `wreel-probe` Release on x86_64:

| Tier | Binary | `libSDL2_mixer.a` | Adds |
|---|---|---|---|
| `minimal` | 2.95 MB | 208 KB | — |
| `standard` | 3.05 MB | 319 KB | Ogg Vorbis |
| `full` | 3.23 MB | 552 KB | + MP3, FLAC |

**`full` costs ~282 KB over `minimal` and zero per-frame CPU.** On a 128 MB device
that is negligible, which is why a FLAC/MP3-capable audio-player build is
essentially free — it is one flag, not a fork.

### Codec tiers

Every decoder here is header-only or vendored, so **no tier adds an external
dependency**: `stb_vorbis`, `minimp3`, `dr_flac`, and libxmp which SDL2_mixer
vendors itself.

| Tier | Formats | Default for |
|---|---|---|
| `minimal` | WAV, MOD/XM/IT/S3M | — |
| `standard` | + Ogg Vorbis | handheld targets |
| `full` | + MP3, FLAC | desktop, Steam |

```sh
# A FLAC-capable player build for a handheld:
cmake --preset miyoomini -DWREEL_AUDIO_CODECS=full
```

**Deliberately excluded:** Opus and WavPack (need libogg and friends — real
external dependencies), GME, and MIDI. Ask `audio::compiled_codecs()` at runtime
rather than assuming; `wreel-probe` prints it.

> **`SDL2MIXER_MIDI` is not the project's MIDI goal.** It means *playing* MIDI
> files through a synthesiser — FluidSynth or Timidity, plus a soundfont of tens
> of megabytes. The secondary goal is MIDI *input* from a hardware controller via
> RtMidi, a different subsystem entirely. `SDL2MIXER_MIDI` is `OFF`.

### Mixer profile

| | Desktop / Steam | Handhelds |
|---|---|---|
| Sample rate | 44100 Hz | 22050 Hz |
| Buffer | 1024 samples (~23 ms) | 2048 samples (~93 ms) |
| Channels | 2 | 2 |
| Voices | 16 | 8 |

22050 Hz halves mixing work and is ample for tracker music. Smaller buffers
underrun on two Cortex-A7 cores that are also software-rasterising.

### Prefer tracker formats for music

On handhelds, `.mod` / `.xm` / `.it` / `.s3m` are the right default: a song is
tens of kilobytes rather than megabytes, and playback is sample mixing rather than
transform decoding.

One caveat worth recording, because it is a natural assumption. libxmp can report
tracker row/pattern/tick, which would give sample-accurate visual sync — but
**SDL2_mixer does not expose it.** `Mix_GetMusicPosition()` returns seconds only.
Row-level sync would mean driving libxmp directly and bypassing the mixer, noted
as an option in
[planning/2026-07-25-midi-live-visuals](../planning/2026-07-25-midi-live-visuals/).

### No device is not an error

Some handheld firmwares expose no audio output at all. `audio::Device` reports
`available() == false` and `Sound`/`Music` become no-ops rather than throwing, so
a game stays playable in silence instead of refusing to start. A program that
constructs no `Device` never initialises the audio subsystem and pays nothing.

## Pinned dependencies

Fetched and built from source per target, so every target gets identical
library versions. Tags verified upstream.

| Dependency | Pin | Notes |
|---|---|---|
| SDL2 | `release-2.32.10` | SDL2, not SDL3 — SDL2 is what every handheld firmware ships. Static (`SDL2::SDL2-static`) |
| SDL2_image | `release-2.8.12` | **not** vendored — decodes PNG/JPEG via bundled `stb_image`, so needs no libpng/libjpeg anywhere |
| SDL2_ttf | `release-2.24.0` | vendored FreeType on **every** target; HarfBuzz off |
| SDL2_mixer | `release-2.8.2` | codec set per `WREEL_AUDIO_CODECS`; vendored libxmp, no external deps |
| nlohmann/json | `v3.12.0` | JSON config and data; replaces RapidJSON |
| pugixml | `v1.16` | Sparrow texture atlases and Tiled TMX maps |
| glm | `1.0.3` | vector and matrix maths; replaces `include/math/vector.hpp`. Debian 12 ships `0.9.9.8` |
| doctest | `v2.5.3` | test framework; single header, no per-target build |

Three things about this that are easy to get wrong, and are settled here:

- **The satellites link static everywhere; SDL2 itself is a per-target choice.**
  `BUILD_SHARED_LIBS=OFF` is forced before the SDL satellites are populated. Left
  to their defaults they build shared and link `SDL2::SDL2` while a static build
  links `SDL2::SDL2-static`, which CMake rejects at generate time as a
  `COMPATIBLE_INTERFACE_BOOL` conflict on `SDL2_SHARED`. Static builds also
  rename the targets to `SDL2_ttf::SDL2_ttf-static` and
  `SDL2_image::SDL2_image-static`, so [Dependencies.cmake](../cmake/Dependencies.cmake)
  resolves the names rather than hard-coding them.

  **`WREEL_SDL2_LINKAGE`** (added 2026-08-01) carries the SDL2 half, and the two
  values are not the same kind of decision:

  | | | |
  |---|---|---|
  | four targets | `STATIC` | a **preference**. One binary is easier to put in a Steam depot or on an SD card, and it keeps us off whatever library set a firmware ships while we hold a glibc floor |
  | `miyoomini` | `SHARED`, forced | an **obligation**. The SSD202D drivers grafted into that build carry steward-fu's LGPL-2.1 header, and the licence requires that a user can relink against a modified copy. It does not relax because packaging tooling improves |

  The satellites are unaffected either way: SDL2_image, SDL2_ttf and SDL2_mixer
  are zlib-licensed and consume the SDL2 API rather than the display, so none of
  that reasoning reaches them. They link SDL2 `PRIVATE` and behind
  `$<BUILD_INTERFACE:>`, and a static satellite asserts nothing about SDL2's
  linkage — which is what lets one vary under the other.
- **FreeType is vendored on all targets, not just cross builds.** There is no
  `stb` fallback for font rasterising, so keying vendoring on
  `CMAKE_CROSSCOMPILING` would make desktop builds require `libfreetype-dev` and
  every device sysroot carry a matching FreeType — defeating the point of pinning.
  `GIT_SUBMODULES` is narrowed to `external/freetype` so SDL2_ttf's ~30 MB
  HarfBuzz submodule is never cloned.
- **CMake 4.x needs a policy floor for dependencies.** CMake 4.0 removed support
  for `cmake_minimum_required(VERSION < 3.5)`, and SDL2_ttf's bundled FreeType
  still declares one. `CMAKE_POLICY_VERSION_MINIMUM` is set to 3.5 for dependency
  builds only. Debian 12's CMake 3.25 never hits this; newer hosts do.

### JSON: why nlohmann/json

RapidJSON is out. Its only tag is `v1.1.0` from **2016**, and upstream has been
dormant since **February 2025**. The original build made this worse by pulling
`master` *unpinned* from `miloyip/rapidjson`, an org that has since moved to
`Tencent/rapidjson`.

nlohmann/json was chosen on support breadth, which is the metric that matters for
a dependency this project will lean on for years:

| | Stars | Last push | Compiler matrix | Debian |
|---|---|---|---|---|
| **nlohmann/json** | 50k | active | **GCC 4.8 – 14.2**, documented | `nlohmann-json3-dev` |
| yyjson | 3.8k | active | — | none |
| picojson | 1.2k | 2024-07 | — | none |

It is the only candidate whose published support matrix explicitly spans this
project's **GCC 8.3 floor** (see
[§ C++17 is the ceiling](#1-c17-is-the-ceiling-and-gcc-83-sets-it)), and being
packaged as `nlohmann-json3-dev` on bookworm means host builds can skip the
fetch entirely.

**Comments in config files.** JSON has none, which is normally a real problem for
hand-edited settings. nlohmann/json sidesteps it — since 3.9.0, `parse()` takes
an `ignore_comments` flag:

```cpp
auto cfg = nlohmann::json::parse(text, nullptr, true, /*ignore_comments=*/true);
```

That removed the main reason to add a second config format (TOML) alongside
JSON, so the project stays on one format and one library.

**Rejected, and why:**

| Candidate | Reason |
|---|---|
| glaze | requires **C++23** / GCC 13+ — unreachable on the GCC 8.3 floor |
| simdjson | compiles fine, wrong shape: its SIMD advantage doesn't exist on armv7 Cortex-A7, and it is tuned for multi-megabyte documents, not 45-byte config |
| Boost.JSON | standalone mode removed in Boost 1.81; pulling Boost onto a 128 MB device is against the grain |
| cJSON / jsoncpp | viable but no advantage over the above |

**Wrap it, don't spread it.** JSON access goes behind a `util::json` facade
rather than letting `nlohmann::json` into module signatures — the same pattern
[`util::File`](../include/util/file.hpp) uses over POSIX `open`/`read` and
[`posix::wrap`](../include/posix/errors.hpp) uses over `errno`. That keeps a
future swap contained and matches how the rest of the tree is built.

**What this unlocks.** JSON remains the format for configuration and for the MIDI
controller mapping files.

> **Superseded, 2026-07-25.** This section used to argue that converting the
> Sparrow atlases to JSON would revive `loaders/sparrow.cc` "without needing an
> XML parser at all, avoiding a second dependency decision". That is no longer the
> direction: XML is supported natively and JSON support is unaffected. See
> *XML: why pugixml* below.

### XML: why pugixml

Two asset formats in the 2D pipeline are XML and are not ours to redefine:
**Sparrow texture atlases** (what TexturePacker and ShoeBox export, and what
`data/jetpackdude.xml` already is) and **Tiled TMX** maps. Supporting them as
authored means reading XML rather than asking artists to convert on the way in.

[loaders/sparrow.cc](../loaders/sparrow.cc) is entirely commented out today,
blocked on a `util/xml.hpp` that was never written.

**Why not hand-roll it.** Sparrow's subset is trivially regular — a tag name and
quoted attributes — and could be read in under 200 lines over the existing
tokenizers. TMX is not: external `.tsx` tileset references, object layers,
properties, and multiple layer-data encodings. Hand-rolling that is a parser
project with a long tail of malformed-input handling, and it would be ours to
maintain.

**Why pugixml over tinyxml2.** Both are MIT, both clear the GCC 8.3 floor, both
cross-compile with no further dependencies. Static footprint was measured on
armv7 rather than assumed, with iostreams already linked as they are here:

| | armv7 marginal cost |
|---|---|
| tinyxml2 11.0.0 | 16,488 bytes |
| pugixml v1.16 (`PUGIXML_NO_XPATH`) | 40,968 bytes |

24 KB apart on a 128 MB device, so size does not decide it. pugixml wins on
ergonomics: its range-based `doc.child("TextureAtlas").children("SubTexture")`
composes with range-`for` and the standard algorithms, which is the style this
codebase already leans into, and its named-child ranges read better than
`FirstChildElement`/`NextSiblingElement` walks. XPath is compiled out
(`PUGIXML_NO_XPATH`); nothing here needs it, and `PUGIXML_INSTALL` is forced
`OFF` because pugixml's install rules default on, unlike nlohmann/json's, and
would otherwise put its headers in this project's bundles.

**Wrapped, like JSON.** Access goes behind the
[`util::xml`](../include/util/xml.hpp) facade for the same reason
`nlohmann::json` does — see *Wrap it, don't spread it*. `pugi::xml_node` does not
belong in a `loaders::` signature. Containment is verified rather than intended:
`util/xml.cc` is the only translation unit compiled with pugixml's include path,
which is asserted by pugixml being a `PRIVATE` link dependency of `wreel_util`.
The header names `pugi` exactly once, forward-declaring `xml_node_struct` so a
`Node` can hold the internal handle without pulling in `pugixml.hpp`.

**One thing the facade does *not* re-export**, found while implementing it: the
attribute accessors' documented contract is wrong. `as_int(def)` says it returns
the default "if conversion did not succeed or attribute is empty", but measured
against v1.16 the default applies only when the attribute is **absent** — a
present but non-numeric value yields `0`, and `"12px"` yields `12`. For an asset
that is the worst possible answer, because a broken dimension becomes a
plausible-looking sprite. `util::xml` therefore converts through
[`util::from_string`](../include/util/number.hpp), so malformed and absent both
mean "fallback", and offers `require_attribute_*` forms that throw instead.
Recorded as part of D17.

**One TMX constraint worth knowing before authoring maps.** Tiled can write tile
layer data as XML, CSV, or base64 with optional gzip/zlib/zstd compression. This
project reads **CSV**, which is a documented user-selectable option in Tiled.
Supporting base64+zlib would need a base64 decoder and zlib — and while zlib
symbols do appear in the linked binary transitively through SDL_image, no zlib
target or header is exposed to the project, so it would mean declaring another
dependency. Save maps as CSV.

### Testing: why doctest

All three serious candidates clear the GCC 8.3 floor, so compatibility didn't
decide it — **shape** did.

| | Stars | Latest | Std | Shape |
|---|---|---|---|---|
| **doctest** | 6.8k | `v2.5.3` (2026-07) | C++11 | **single header, 9.1k lines** |
| Catch2 | 21.3k | `v3.15.2` (2026-07) | C++14 | compiled static lib |
| googletest | 38.9k | `v1.17.0` (2025-04) | C++17 | compiled static lib |

Catch2 v3 and googletest are **compiled libraries**, so each would have to be
built for all five targets, inside each toolchain container — including the
Debian 10 Miyoo image. doctest is a header that works unchanged everywhere, and
it is the fastest of the three to compile, which compounds across five build
trees.

`boost-ext/ut` and `snitch` both require **C++20** — unreachable on GCC 8.3.

Debian packages `doctest-dev` (2.4.9), which lags `v2.5.3`; the pin above is
fetched so all targets agree.

**Where the test surface actually is.** This matters more than the framework
choice: [include/util/string.hpp](../include/util/string.hpp) is 688 lines of
tokenizers, escape handling, and line iteration — the most intricate code in the
repo, one fifth of the entire codebase, and the piece that *must* change (its
`std::ptr_fun` usage is removed in C++17). Refactoring it without tests is how
the OBJ loader breaks silently, because
[loaders/obj.cc](../loaders/obj.cc) is its only consumer.

Test priority, in order:

1. `util/string.hpp` tokenizers — **before** the `ptr_fun` removal, not after
2. OBJ parsing against the existing `data/*.obj` fixtures
3. `util::File` POSIX/Win32 backends
4. `posix::wrap` errno → exception dispatch
5. the `util::json` facade

Two conventions from the original build are kept: tests run with the
**repository root** as working directory so `data/` fixture paths resolve, and
each test is registered individually with CTest. Cross-built test binaries run
directly on the dev box via `qemu-user-static` — see
[DEVELOPMENT.md](DEVELOPMENT.md#cross--cross-compilers).

### `wreel-probe`: the device capability tool

`project1/` — a "Lesson 0" SDL/GLEW smoke test — is replaced by `probe/`,
building **`wreel-probe`**.

The original was abandoned tutorial scaffolding rather than unfinished work: it
calls `SDL_GL_SetAttribute` for the GL context version *after*
`SDL_GL_CreateContext`, where it has no effect, and it renders nothing.

`wreel-probe` reports what the current device actually offers:

- SDL video and audio drivers available
- `SDL_Renderer` backends, and which is selected
- GL / GLES version and vendor, where a GL context is possible at all
- display modes and refresh rates
- attached gamepads and their axis/button counts

This exists because bringing up five device classes otherwise means guessing.
SSH into an Anbernic, run `wreel-probe`, and the firmware tells you what it has
instead of you inferring it from a crash. It also keeps the two-executable
output shape of the original tree: one demo, one tool.

### Why source builds instead of `libsdl2-dev`

Cross-compiling is the whole point. There is no `libsdl2-dev:armhf` that matches
a Miyoo Mini's glibc and video backend, so the SDL2 that ships on the device
would have to be matched by hand per firmware. Building pinned SDL2 sources per
target with the target's own toolchain gives one reproducible answer for all
five presets.

The escape hatch is `-DWREEL_USE_SYSTEM_SDL2=ON`, for the case where a device's
firmware ships a *patched* SDL2 that upstream cannot replace — which is exactly
the Miyoo Mini situation if you target stock firmware rather than KMSDRM.

Note that "system SDL2" cannot mean "found in the sysroot" on that target. The
union toolchain predates any SDL2 on this platform and carries **SDL 1.2 only**,
so `find_package(SDL2)` under `CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY` finds
nothing. It means headers we supply plus the device's runtime copy, which is what
`WREEL_SDL2_ROOT` exists for.

### The Miyoo Mini exception, which is not hypothetical — 2026-07-27

`WREEL_USE_SYSTEM_SDL2=ON` is **mandatory** on `miyoomini`, not a contingency. The
pinned upstream SDL2 has no video driver that can reach that panel, and this is
checkable without a device:

```console
$ grep -E "define SDL_VIDEO_DRIVER_" \
    build/miyoomini/_deps/sdl2-build/include-config-release/SDL2/SDL_config.h
#define SDL_VIDEO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_OFFSCREEN 1
#define SDL_VIDEO_DRIVER_WAYLAND 1
```

Nor is it a detection failure to be fixed by turning something on. **SDL2 has no
framebuffer backend at all** — SDL 1.2's `fbcon` has no SDL2 successor, and the
2.32 tree's `src/video/` offers `kmsdrm`, `x11`, `wayland`, `vivante`,
`raspberry`, `directfb` and nothing that fits. The SSD202D exposes SigmaStar's
`MI_GFX` and a framebuffer, and no DRM device. Audio is the same story: the same
config has no ALSA, only `OSS`, `PULSEAUDIO`, `SNDIO`, `DISK` and `DUMMY`.

What runs there is an SDL2 ported to the vendor APIs, driving `libmi_gfx` and
`libmi_ao`.

> **Revised 2026-08-01: we build that SDL2 now.** This section used to say the
> project vendors steward-fu's prebuilt binary. It vendors his ~1,290 lines of
> *driver source* instead, compiled into the same pinned upstream SDL2 every
> other target gets — 2.32.10, not the prebuilt's 2.0.20.
>
> `WREEL_MINI_SDL2` is defaulted ON by the miyoomini toolchain file. It grafts
> `src/{video,render,audio}/mini` into the FetchContent'd tree and applies a
> 42-line additive patch registering them. Provenance and the complete list of
> our modifications are in
> [platform/miyoomini/sdl2/PROVENANCE.md](../platform/miyoomini/sdl2/PROVENANCE.md);
> the decision is § 5 of
> [planning/2026-07-31-miyoo-sdl2-fork](../planning/2026-07-31-miyoo-sdl2-fork/),
> and it passed its device gate — `coppers` ran 859 frames at 59.7 fps against
> it, indistinguishable from the prebuilt.
>
> What that changes, beyond provenance:
>
> - **The bundle's `lib/` is one file.** No `libEGL.so` of unknown licence, no
>   21.8 MB `libGLESv2.so`, no `libjson-c.so.5` of unidentifiable version — the
>   first two because the driver's GL path is not compiled, the third because
>   the audio driver no longer reads a firmware config file. 8.8 MiB staged
>   becomes 4.7 MB.
> - **The headers match the library.** The old arrangement compiled against
>   headers of unrecorded provenance and linked a 2.0.20 runtime, verifying only
>   that the symbols resolved.
> - **`WREEL_USE_SYSTEM_SDL2` is the escape hatch now**, not the route. It stays
>   for a firmware whose SDL2 genuinely cannot be replaced.
>
> The table below still describes the two prebuilts, and is why we do not borrow
> the firmware's own copy.

The reasoning against borrowing is unchanged and is in decision 3 of
[planning/2026-07-27-onion-bundle](../planning/2026-07-27-onion-bundle/):
OnionOS's `parasyte` copy has a Mesa EGL that drags in gbm, glapi, X11, xcb and
libdrm plus its own loader and libc.

| | Vendored `prebuilt/640x480/` | Onion's `parasyte` |
|---|---|---|
| Video driver | **`mini`** | `mmiyoo` |
| Render driver | **`Miyoo Mini`** | `software`, plus its own |
| In this project | shipped | not used |

Both are SDL 2.0.20 and both are steward-fu's work, from two different source
trees. **The names are not interchangeable** — an earlier version of this section
described the shipped library as registering `mmiyoo`, which is the other one, and
setting `SDL_VIDEODRIVER` to it cost a device run. `launch.sh` now names neither.

Three consequences worth knowing before building against it:

- **The render driver reports as `Miyoo Mini (accelerated)`** on a device with no
  GPU, so a driver name is not evidence of a GPU. What it accelerates is a
  full-screen blit through `MI_GFX`, and it exposes almost nothing else — see
  [MIYOO-MINI.md § 4.3](MIYOO-MINI.md) and defect D25. **Note the verb:** the
  blitter itself also fills, blends, colour-keys, mirrors and rotates, and the
  port plumbs none of that through. Corrected 2026-07-31 from "implements almost
  nothing else", which attributed the port's omissions to the hardware;
  [MIYOO-MINI.md § 4.6](MIYOO-MINI.md) is the API as the vendor SDK declares it.
- **Only the core SDL2 goes shared.** SDL2_image, SDL2_ttf and SDL2_mixer stay
  pinned and statically linked, because they consume the SDL2 API rather than the
  display. All 107 SDL symbols they import exist in that 2.0.20 runtime, as do
  all 87 this codebase calls — checked by comparing the dynamic symbol table
  against `nm -u` on the static archives, not assumed.
- **The shipped copy is modified.** One unused `DT_NEEDED` is removed at bundle
  time, dropping 21.8 MB of SwiftShader the library references no symbol from.
  Declared in [THIRD-PARTY.md](../THIRD-PARTY.md), because an LGPL binary we alter
  may not be altered silently.

Note also that `SDL_KMSDRM ON` in [Dependencies.cmake](../cmake/Dependencies.cmake)
produces no KMSDRM driver in the cross build, because libdrm and gbm are absent
from that environment and the probe fails silently. Moot on this device, which has
no DRM node — but a flag whose failure is invisible is worth knowing about.

Full reasoning, and the bundle that carries the shared object, in
[planning/2026-07-27-onion-bundle](../planning/2026-07-27-onion-bundle/).

## See also

- [DEVELOPMENT.md](DEVELOPMENT.md) — host setup, toolchain install, build commands
- [MIYOO-MINI.md](MIYOO-MINI.md) — that platform in detail: what its SDL2 can and
  cannot do, and what was measured on the hardware
- [../THIRD-PARTY.md](../THIRD-PARTY.md) — shipped binaries we did not build,
  their pins and their licences
- [../data/PROVENANCE.md](../data/PROVENANCE.md) — where the assets came from
- [../README.md](../README.md) — project overview
