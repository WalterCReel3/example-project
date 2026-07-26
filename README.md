# example-project

A small C++ engine scaffold for SDL2 toy games and live graphics, built to run
on Linux handhelds and ship on Steam for Linux.

Originally written in early 2016 and dormant since. This is a revival: the
organisation, tooling discipline, and output shape of the original are kept
deliberately; the language level, build system, and rendering path are being
brought forward.

## What it's for

**Primary goal** — build small SDL2 / SDL2+OpenGL games that run on Linux-based
retro handhelds (Anbernic, Miyoo Mini, and friends) *and* package cleanly as a
plain Linux application suitable for Steam.

**Secondary goal** — a fun toolkit for live multimedia: MIDI-controlled,
old-school demo-style graphics driven from a hardware controller.

The two goals pull in useful opposite directions. Handhelds force a tight,
portable, CPU-cheap core with no GPU assumptions. Live visuals want fast
iteration and expressive rendering. Keeping both honest is the point.

## Targets

| Preset | Devices | `gfx::renderer` driver | `gfx::gles2` |
|---|---|---|---|
| `desktop` | your dev box | `opengl` | yes |
| `steam` | Steam, Steam Deck | `opengl` | yes |
| `miyoomini` | Miyoo Mini, Mini Plus | `software` — **no GPU at all** | never |
| `rk3326` | RG351, RG353 | `opengles2` | yes |
| `h700` | RG35XX Plus/H/SP, RG40XX | `opengles2` | yes |

Two renderers, both compiled where the hardware allows: `gfx::renderer` over
`SDL_Renderer` draws the game on every target, hardware-accelerated wherever a GPU
exists, and `gfx::gles2` owns a GL context for shaders and 3D.

Full detail, and the three constraints that drive every design decision here, in
[docs/TARGETS.md](docs/TARGETS.md).

The short version of those constraints:

1. **C++17 is the ceiling** — the Miyoo Mini toolchain is GCC 8.3.
2. **Never ship Debian's cross-GCC output** — glibc 2.36 binaries won't load on
   a handheld. Device SDK containers exist for this.
3. **Miyoo Mini has no GPU at all** — the software renderer is the baseline, not
   a fallback.

## Layout

Flat top-level module directories, each producing one static library, with all
public headers hoisted into a single `include/<module>/` tree. Dependencies run
one way: `skratch` → `loaders` → `util` → `posix`.

| Directory | Output | Role |
|---|---|---|
| [posix/](posix/) | `libposix` | errno → typed C++ exceptions |
| [util/](util/) | `libutil` | file I/O, logging, string/tokenizing |
| [gfx/](gfx/) | `libgfx` | SDL2 windowing and rendering |
| [audio/](audio/) | `libaudio` | sound effects and music — SDL2_mixer |
| [loaders/](loaders/) | `libloaders` | OBJ models, images, texture atlases |
| [skratch/](skratch/) | executable | demo app — free camera over a model grid, and the worked example of modern GL structuring |
| `probe/` | executable | `wreel-probe` — device capability diagnostic |
| [tests/](tests/) | CTest suite | doctest, run from repo root against `data/` |

Two patterns from the original that are worth keeping, and are being kept:

- **Compile-time platform pimpl.** `util::File` swaps
  [util/posix/fileimpl.cc](util/posix/fileimpl.cc) for
  [util/mswin/fileimpl.cc](util/mswin/fileimpl.cc) at build time rather than
  through virtual dispatch — zero overhead, no vtable, no runtime branch.
- **Errors as types.** [include/posix/errors.hpp](include/posix/errors.hpp)
  macro-generates an exception class per errno value, and `posix::wrap()` turns
  any failing syscall into the matching throw.

## Getting started

Debian 12 or a derivative (Ubuntu 22.04+, Mint 21+, Pop!_OS 22.04+):

```sh
./scripts/bootstrap-debian.sh --dry-run   # inspect first
./scripts/bootstrap-debian.sh             # install

cmake --preset desktop-software
cmake --build --preset desktop-software
ctest --preset desktop-software

./build/desktop-software/bin/wreel-probe  # what does this machine offer?
```

`desktop-software` builds the same renderer the Miyoo Mini uses, natively — so
you can work on the handheld code path without a device or a cross-compiler.
Full detail in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Status

Revival in progress. The 2016 code is now fully modernized: both renderers work,
the fixed-function backend is gone, and the tree builds warning-free under
`-Werror` on all five configured presets. What is missing is hardware — none of
this has run on a device — and the 2D sprite path the handheld game needs.

- [x] Debian dev environment, scripted and documented
- [x] Target matrix, toolchain, and constraint research
- [x] Editor/format configuration
- [x] Dependencies settled — SDL2, nlohmann/json, pugixml, glm, doctest ([why](docs/TARGETS.md#pinned-dependencies))
- [x] Modern CMake build with pinned `FetchContent` dependencies
- [x] Cross-compile toolchain files and `CMakePresets.json` (7 presets)
- [x] `probe/` → `wreel-probe`, replacing `project1/`
- [x] `gfx::renderer` — the SDL_Renderer path, on every target. Software driver on
      the Miyoo Mini, `opengles2` on the Mali handhelds
- [x] doctest suite — 10 executables, 114 cases, 1482 assertions, passing on all five presets
- [x] Audio as a base requirement — SDL2_mixer, tiered codecs, tracker-first ([why](docs/TARGETS.md#audio))
- [ ] Verify the cross and Steam presets on real toolchains and hardware
- [x] C++17 cleanup of the 2016 sources — **167 warnings to zero**, `WREEL_WERROR=ON`
- [x] `util::xml` — pugixml behind a facade, so Sparrow atlases and TMX are read as
      authored
- [x] `gfx::gles2` for shaders and 3D; the 2016 fixed-function backend retired, with
      `skratch` ported onto it as the modern-GL reference
- [ ] Packaging: handheld bundles and a Steam depot layout

Five of the seven presets are built and tested — both desktop ones natively, and
`rk3326`, `h700` and `miyoomini` cross-built with their tests run under qemu. **No
part of this has run on real hardware**, and neither the Miyoo Mini device
toolchain nor the Steam runtime container has been tried. See
[docs/DEVELOPMENT.md § Status](docs/DEVELOPMENT.md#status) for exactly what is
verified and what isn't.

## Documentation

- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) — host setup, toolchains, building
- [docs/TARGETS.md](docs/TARGETS.md) — device matrix, backends, dependency pins
