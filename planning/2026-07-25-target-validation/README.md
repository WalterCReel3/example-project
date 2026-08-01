# Target validation

**Status:** `in-progress`
**Written:** 2026-07-25
**Blocks:** [packaging-distribution](../2026-07-25-packaging-distribution/)

> **Steps 1 and 2 below are already done.** This document was written before the
> validation pass recorded in
> [docs/DEVELOPMENT.md § Status](../../docs/DEVELOPMENT.md#status), and its
> "what is not verified" table is stale. Re-checked 2026-07-25: `libglew-dev`,
> `libglu1-mesa-dev`, `aarch64-linux-gnu-g++` and `arm-linux-gnueabihf-g++` are
> all installed, and the `desktop-debug`, `rk3326`, `h700` and `miyoomini` build
> directories are configured. `desktop-debug` builds and links with **zero
> errors** and 4/4 tests. What remains is genuinely container- and
> hardware-dependent: steps 3, 4 and 5.

## Motivation

Six of the seven presets have never been run. The build system was written from
verified documentation — the Miyoo toolchain's own `setup-env.sh`, upstream SDL
tags, Debian package metadata — but *documented* and *working* are different
claims, and only `desktop-software` currently supports the second one.

This is the cheapest high-value work available. Every hour here removes guesses
from the foundation the rest of the project sits on.

## What is verified today

On Debian 12 / GCC 12.2. Superseded by
[docs/DEVELOPMENT.md § Status](../../docs/DEVELOPMENT.md#status), which is the
authoritative record — this list is kept for the reasoning, not the checklist.

- `desktop-software` — cold configure, build, and `ctest` (4/4, not 3/3 as
  originally written here)
- `desktop-debug` — builds and links clean, `skratch` included, 4/4 tests
- `rk3326` / `h700` — cross-build plus `ctest` under qemu
- `miyoomini` — armv7 compile-check build plus `ctest` under qemu-arm. The
  **device** toolchain (GCC 8.3) is still untried, which is the distinction that
  matters
- `wreel-probe` — runs headless under `SDL_VIDEODRIVER=dummy`, and as an aarch64
  binary under qemu
- Configure-time guards — unknown backend rejected; `gl_legacy` on a GPU-less
  target rejected; both missing-cross-toolchain messages fire correctly

## What is not

| Preset | Blocker | Risk if wrong |
|---|---|---|
| ~~`miyoomini` **device** toolchain~~ | ~~needs the toolchain container~~ | **DONE 2026-07-27.** GCC 8.3.0 builds the tree with zero errors and zero warnings after one fix (D21), 15/15 under qemu, `GLIBC_2.28` floor. None of the predicted C++17 library gaps materialised — see [results.md](results.md) |
| `steam` | needs the sniper container | glibc targeting, static libstdc++ |
| ~~`docker/miyoomini.Dockerfile`~~ | ~~needs Docker + upstream base image~~ | **DONE 2026-07-27.** Both images build; the CMake/Ninja layering works and is needed exactly as predicted, since buster's CMake 3.13 cannot read the presets. One wrinkle worth knowing: run commands with `bash -c`, not `bash -lc` — a login shell re-reads `/etc/profile` and discards the image's `PATH`, so the layered CMake loses to buster's |
| any real hardware | needs a device | display path, SDL video driver, gamepad enumeration |
| Mali GLES2 on `rk3326` / `h700` | needs a device | **new since 2026-07-26.** Both now request an accelerated render driver and can build `gfx::gles2`. If a firmware's vendor blobs do not expose a working GLES2 context, `gfx::renderer` degrades to software and the game still runs — that is what `PreferAccelerated` is for — but `gfx::gles2` cannot, and would throw at context creation |

Resolved since this was written:

| Preset | Was | Now |
|---|---|---|
| `desktop-debug` / `-release` | "**highest** risk — `find_package(GLEW)` never ran" | **moot.** GLEW and GLU are gone with the 2016 backend; nothing links a GL library at all now. The preset builds, links and renders |
| `rk3326` / `h700` | "needs `crossbuild-essential-arm64`" | **verified.** Cross-build plus qemu `ctest`; `WREEL_TEST_EMULATOR` wiring confirmed |
| `miyoomini` (compile-check) | "needs the toolchain container" | **verified** in armv7 compile-check mode under qemu-arm |

## Order of attack

Cheapest and most informative first.

### 1. `gl_legacy` on the dev box — **done**

```sh
sudo apt install libglew-dev libglu1-mesa-dev
cmake --preset desktop-debug && cmake --build --preset desktop-debug
```

Predicted here as "the most likely thing to be broken". It was not: zero errors,
`skratch` links, `wreel-probe` reports a real GL context. The only failure was in
the new build glue, not the 2016 code — see
[docs/DEVELOPMENT.md § The `gl_legacy` backend](../../docs/DEVELOPMENT.md#the-gl_legacy-backend).

The `-Wold-style-cast` prediction was also wrong, and measurably so: `gfx/obj.cc`
and `gfx/utils.cc` produce **one** old-style-cast warning between them, not the
expected noise. See
[cxx17-modernization § Measured warning inventory](../2026-07-25-cxx17-modernization/README.md).

### 2. aarch64 compile-check + qemu — **done**

```sh
sudo apt install crossbuild-essential-arm64 qemu-user-static binfmt-support
cmake --preset rk3326 && cmake --build --preset rk3326
ctest --preset rk3326
```

Validated the shared `aarch64-handheld.cmake` base, the `-mcpu` flags, and
`WREEL_TEST_EMULATOR`'s qemu wiring. 4/4 under qemu on both `rk3326` and `h700`.
The one real defect it surfaced: cross-built tests could not run at all until the
toolchain files passed `-L <sysroot>` to qemu so it could find the target loader.

Also useful for the C++17 work: `char` is **unsigned** on both
`aarch64-linux-gnu-g++` and `arm-linux-gnueabihf-g++`, and signed on the host.
That divergence is defect D10.

### 3. The Miyoo Mini container — **the real test**

```sh
git clone https://github.com/shauninman/union-miyoomini-toolchain.git
cd union-miyoomini-toolchain && make .build
cd -; docker build -f docker/miyoomini.Dockerfile -t wreel-miyoomini .
docker run --rm -it -v "$PWD":/src -w /src wreel-miyoomini bash
cmake --preset miyoomini && cmake --build --preset miyoomini
```

The single most valuable step in this document. It is the only way to find out
whether **GCC 8.3 actually compiles this codebase**. Specific things expected to
bite:

- `nlohmann/json` 3.12 on GCC 8.3 — the support matrix says 4.8–14.2, but the
  matrix is not the same as having tried it
- doctest's assertion macros under `DOCTEST_CONFIG_SUPER_FAST_ASSERTS`
- SDL2 2.32 cross-building against a 2019-vintage sysroot
- `std::filesystem` — nothing uses it yet, but `wreel_options` links `stdc++fs`
  on GCC < 10, and that link should be confirmed to resolve

Added since this was written, and all of it unverified on GCC 8.3 — the list has
grown considerably, which is the argument for doing this step sooner rather than
later:

- **`std::from_chars` for integers**, which `util::from_string` depends on. The
  C++17 library gap table in docs/TARGETS.md says GCC 8 has the integer overload
  and not the floating-point one, and the whole traits-dispatch design in
  `include/util/number.hpp` rests on that being true
- **`inline constexpr` callable objects** — `util/ascii.hpp`'s thirteen predicates,
  and `std::not_fn`, both documented as GCC 7+ and neither actually compiled by 8.3
  here
- **pugixml v1.16 and glm 1.0.3**, two dependencies added after this list
- **`decltype(&::glFoo)` over SDL's bundled Khronos headers**, which is how
  `gfx/gles2/api.hpp` declares its entry points. It is plain C++11 and should be
  fine, but it is unusual enough to be worth confirming rather than assuming — and
  `WREEL_ENABLE_GLES2` is off for this target anyway, so a failure here would only
  show up on the aarch64 device SDKs
- **`if constexpr`** in `util/number.hpp`'s floating-point path and `-Os` codegen
  across all of it

### 4. On-device

Copy `wreel-probe` to a Miyoo Mini and run it over SSH or from the firmware's
file manager. Its output answers questions that are currently assumptions:

- which SDL video driver the firmware actually offers, and whether it is upstream
  KMSDRM or the vendor `MI_GFX` path
- whether the pinned upstream SDL2 works at all, or whether
  `WREEL_USE_SYSTEM_SDL2=ON` against the firmware's patched copy is required
- real display mode and refresh rate
- what the gamepad enumerates as, which `skratch/input.cc`'s hard-coded Xbox 360
  axis mapping certainly gets wrong

Two tools exist specifically to make this a command rather than a judgement, both
added 2026-07-26:

- **`wreel-probe` reports GLES** as well as GL. It attempts an ES 2.0 request and a
  default request separately and prints version, vendor, renderer, GLSL version and
  max texture size for each. On an aarch64 handheld this is the answer to "do the
  vendor blobs work". It had been compiled *out* on `rk3326` and `h700` until the
  `find_package(OpenGL)` requirement was removed — the two targets it matters most
  for could not produce a report at all.
- **`skratch --screenshot <path.bmp>`** renders two frames, writes a BMP and exits.
  Nobody can watch a handheld's panel over SSH, and there is no headless GL to test
  against, so this is the only way to confirm the renderer draws on a device. It
  needs `gfx::gles2`, so it applies to the two Mali targets and not the Miyoo Mini.

~~For the Miyoo Mini the equivalent check is still missing.~~ **Added 2026-07-27**
as `gfx::renderer::Context::save_screenshot()`, via `SDL_RenderReadPixels` and
named to match the `gles2` one. Both renderers can now be checked over SSH. It
arrived with the fill-rate measurement exactly as predicted here, because the demo
that takes the measurement is also the thing that needed the screenshot —
[coppers](../2026-07-26-coppers-cracktro/).

### 5. Steam Runtime

```sh
docker pull registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest
```

Lowest urgency — desktop Linux is a forgiving target and the risk is confined to
glibc symbol versioning, which `objdump -T | grep GLIBC_` can confirm directly.

## Deliverables

- [x] A [`results.md`](results.md) in this directory recording what happened per
      preset, with real command output rather than a summary. Started 2026-07-27
      with the fill-rate measurement
- [x] `docs/DEVELOPMENT.md § Status` updated to move rows from "not run" to
      verified — done for steps 1 and 2; that table is now the authoritative
      record and this document defers to it
- [ ] `wreel-probe` output from at least one real handheld, committed as a
      reference for what these devices report
- [ ] Fixes for whatever breaks, ideally as separate commits from this snapshot

## Open questions

- ~~Is upstream SDL2 viable on Miyoo Mini stock firmware, or is
  `WREEL_USE_SYSTEM_SDL2=ON` mandatory there? This is the largest single unknown
  in the project.~~ **Answered 2026-07-27: mandatory.** It is not viable, and no
  device was needed to establish it — the generated `SDL_config.h` for this preset
  builds `dummy`, `offscreen` and `wayland`, and SDL2 has no framebuffer backend
  anywhere in its tree to build instead. The hermetic-FetchContent approach holds
  for four of five presets and takes a documented exception on the fifth, which is
  what the escape hatch was put there for. What the *device* still has to say is
  narrower and more practical: whether the firmware's `mmiyoo` SDL2 gets the display
  away from MainUI, and whether the audio server yields the mixer. See
  [onion-bundle](../2026-07-27-onion-bundle/).
- ~~**What is the software driver's fill rate at 320×240 on two Cortex-A7 cores?**~~
  **Method built and measured on the dev box, 2026-07-27** — see
  [results.md](results.md). Still open on hardware, but the question was posed
  wrongly: 320×240 is not any device's resolution (the Mini Plus is 640×480 and the
  Flip is 752×560), and a lower *internal* resolution turns out to be a **net loss**
  on the software driver, because SDL's scaling blit costs more than the plotting it
  saves. It is a 2.8× win on an accelerated driver. So the answer inverts between
  the two drivers this project ships on, and `coppers --seconds` is how it gets
  taken on a device.
- Should CI be added now? A GitHub Actions matrix over `desktop-software`,
  `desktop-debug` and the two aarch64 compile-checks would catch regressions
  cheaply. The container-based targets are harder and can come later.

## References

- [docs/DEVELOPMENT.md § Cross-compiling](../../docs/DEVELOPMENT.md)
- [docs/TARGETS.md § 2 — the glibc trap](../../docs/TARGETS.md)
