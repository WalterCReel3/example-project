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
| `desktop-debug`, `desktop-release` | your dev box | `opengl` | yes |
| `desktop-software` | your dev box, as a Miyoo Mini stand-in | `software` (forced) | no |
| `steam` | Steam, Steam Deck | `opengl` | yes |
| `miyoomini` | Miyoo Mini, Mini Plus, Mini Flip | `Miyoo Mini` or `software` — **no GPU at all** | never |
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
3. **Miyoo Mini has no GPU at all** — `gfx::renderer` without a GPU is the
   baseline, not a fallback.

## Layout

Flat top-level module directories, each producing one static library, with all
public headers hoisted into a single `include/<module>/` tree. Dependencies run
one way: `loaders` and `game` → `gfx`; `gfx`, `rig` and `audio` → `util` →
`posix`; executables on top. Module charters in
[docs/TARGETS.md § Modules](docs/TARGETS.md#modules).

| Directory | Output | Role |
|---|---|---|
| [posix/](posix/) | `libposix` | errno → typed C++ exceptions |
| [util/](util/) | `libutil` | file I/O, logging, string/tokenizing, XML; links no SDL |
| [rig/](rig/) | `librig` | asset paths, frame timing, input mapping |
| [audio/](audio/) | `libaudio` | sound effects and music — SDL2_mixer |
| [gfx/](gfx/) | `libgfx` | `gfx::renderer` (SDL_Renderer), `gfx::gles2`, atlases, animation, tilemaps |
| [loaders/](loaders/) | `libloaders` | OBJ, images, Sparrow atlases, animation sidecars, Tiled TMX |
| [game/](game/) | `libgame` | entity store, collision, camera, level spawn tables |
| [orchard/](orchard/) | executable | the small game the `game` layer is shaped around |
| [sprites/](sprites/) | executable | 2D game-path demo — atlas, animation, tilemap |
| [coppers/](coppers/) | executable | copper-bar cracktro over `gfx::renderer`; also a fill-rate instrument |
| [skratch/](skratch/) | executable | free camera over a model grid through `gfx::gles2`; needs a GPU |
| [probe/](probe/) | executable | `wreel-probe` — what the device's SDL says it offers |
| [diag/](diag/) | executable | `wreel-diag` — what the device's SDL actually draws, read back |
| [tests/](tests/) | CTest suite | doctest, run from repo root against `data/` |
| [tools/](tools/) | Python scripts | offline asset generators; not part of the build |
| [platform/miyoomini/sdl2/](platform/miyoomini/sdl2/) | — | SSD202D SDL2 drivers grafted onto the pinned SDL2 |

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

Revival in progress. The 2016 code is modernized, the 2D game path (atlases,
animation, tilemaps, a `game` layer) exists, and a small game, `orchard`, runs
on a Miyoo Mini. Validation is currently narrowed to desktop and `miyoomini`
([why](docs/TARGETS.md#validation-scope-desktop-and-miyoomini)).

- [x] Debian dev environment, scripted and documented
- [x] Target matrix, toolchain, and constraint research
- [x] Editor/format configuration
- [x] Dependencies settled — SDL2, nlohmann/json, pugixml, glm, doctest ([why](docs/TARGETS.md#pinned-dependencies))
- [x] Modern CMake build with pinned `FetchContent` dependencies
- [x] Cross-compile toolchain files and `CMakePresets.json` (7 presets)
- [x] `probe/` → `wreel-probe`, replacing `project1/`
- [x] `gfx::renderer` — the SDL_Renderer path, on every target. `software` or
      `Miyoo Mini` driver on the Miyoo Mini, `opengles2` on the Mali handhelds
- [x] doctest suite — 26 suites
- [x] Audio as a base requirement — SDL2_mixer, tiered codecs, tracker-first ([why](docs/TARGETS.md#audio))
- [x] C++17 cleanup of the 2016 sources — **167 warnings to zero**, `WREEL_WERROR=ON`
- [x] `util::xml` — pugixml behind a facade, so Sparrow atlases and TMX are read as
      authored
- [x] `gfx::gles2` for shaders and 3D; the 2016 fixed-function backend retired, with
      `skratch` ported onto it as the modern-GL reference
- [x] Miyoo Mini: GCC 8.3 device toolchain, SDL2 built from source with the
      SSD202D drivers, and runs on hardware
- [x] 2D game path — `gfx::Atlas`, `gfx::AnimatedSprite`, `gfx::TileMap`, and
      the `game` layer, exercised by `orchard`
- [ ] Verify the Mali handhelds and Steam on real toolchains and hardware
- [ ] Packaging: the OnionOS bundle exists and runs; other firmwares and a Steam
      depot layout do not

What has been run, and where:

- **Dev box:** `desktop-software` (full compile, ccache disabled) and
  `desktop-debug` both build with zero warnings under `WREEL_WERROR` and pass
  26/26 suites, 2026-10-16, on the tree since committed through `9921511`.
- **GCC 8.3 (`miyoomini` device toolchain), 2026-10-16:** the same tree
  does **not** build under `-Werror`: two `-Wcast-align` errors in the
  mirroring tests, committed in `1b811fd` (D34 in
  [defects.md](planning/2026-07-25-cxx17-modernization/defects.md), open).
  Incremental build; library and demo code compiled.
- **Miyoo Mini hardware**, from the GCC 8.3 bundle: `coppers` and `wreel-diag`
  (2026-07-28 to 2026-08-08; `coppers` on stock firmware as well as OnionOS),
  and `orchard`, played to the goal at 59.7 fps on the `software` driver under
  OnionOS ([results.md](planning/2026-07-25-target-validation/results.md),
  2026-10-06 entry).
- **Under qemu only:** the `rk3326` and `h700` cross builds and their tests,
  last on 2026-07-27 when the suite was 15. The GCC 8.3 `miyoomini` build
  under qemu, 2026-10-16: 20 of the 24 suites that build pass; `test_audio`,
  `test_playlist` and `test_renderer` fail on that target's premises, and
  `test_entities` segfaults (D33, open).
- **Never run:** anything on the Mali handhelds (`rk3326`, `h700`), the `steam`
  preset, the Mini Flip, and `sprites` and `wreel-probe` on any device.

[docs/DEVELOPMENT.md § Status](docs/DEVELOPMENT.md#status) has the per-component
detail.

## Documentation

- [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) — host setup, toolchains, building
- [docs/TARGETS.md](docs/TARGETS.md) — device matrix, backends, dependency pins
- [docs/MIYOO-MINI.md](docs/MIYOO-MINI.md) — the SSD202D, its firmware, and its SDL2
- [planning/README.md](planning/README.md) — index of dated planning snapshots
- [tools/README.md](tools/README.md) — offline asset tooling
- [THIRD-PARTY.md](THIRD-PARTY.md) — shipped libraries this project did not build
- [data/PROVENANCE.md](data/PROVENANCE.md) — where the assets came from
