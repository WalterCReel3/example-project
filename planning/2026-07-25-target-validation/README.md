# Target validation

**Status:** `snapshot`
**Written:** 2026-07-25
**Blocks:** [packaging-distribution](../2026-07-25-packaging-distribution/)

## Motivation

Six of the seven presets have never been run. The build system was written from
verified documentation — the Miyoo toolchain's own `setup-env.sh`, upstream SDL
tags, Debian package metadata — but *documented* and *working* are different
claims, and only `desktop-software` currently supports the second one.

This is the cheapest high-value work available. Every hour here removes guesses
from the foundation the rest of the project sits on.

## What is verified today

On Debian 12 / GCC 12.2 / CMake 4.4:

- `desktop-software` — cold configure, build, and `ctest` (3/3, 71 assertions)
- `wreel-probe` — runs headless under `SDL_VIDEODRIVER=dummy`
- Configure-time guards — unknown backend rejected; `gl_legacy` on a GPU-less
  target rejected; both missing-cross-toolchain messages fire correctly

## What is not

| Preset | Blocker | Risk if wrong |
|---|---|---|
| `desktop-debug` / `-release` | no GLEW/GLU on the dev box | **highest.** `find_package(GLEW)` never ran, and the 2016 GL sources have never compiled under the new build |
| `miyoomini` | needs the toolchain container | toolchain paths, GCC 8.3 C++17 gaps, `-Os` codegen |
| `rk3326` / `h700` | needs `crossbuild-essential-arm64` | cross flags, qemu test execution |
| `steam` | needs the sniper container | glibc targeting, static libstdc++ |
| `docker/miyoomini.Dockerfile` | needs Docker + upstream base image | CMake/Ninja layering |

## Order of attack

Cheapest and most informative first.

### 1. `gl_legacy` on the dev box

```sh
sudo apt install libglew-dev libglu1-mesa-dev
cmake --preset desktop-debug && cmake --build --preset desktop-debug
```

This is first because it is one `apt install` away and it is the most likely
thing to be broken — it is the only path that exercises `find_package(GLEW)`,
`OpenGL::GLU`, and the 2016 renderer under C++17. Expect `-Wold-style-cast` noise
from `gfx/obj.cc` and `gfx/utils.cc`; expect possible real errors.

### 2. aarch64 compile-check + qemu

```sh
sudo apt install crossbuild-essential-arm64 qemu-user-static binfmt-support
cmake --preset rk3326 && cmake --build --preset rk3326
ctest --preset rk3326
```

Validates the shared `aarch64-handheld.cmake` base, `-mcpu` flags, and — most
interestingly — whether `WREEL_TEST_EMULATOR` actually wires qemu into ctest
correctly. This produces unshippable binaries by design; the point is the
toolchain plumbing.

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

### 5. Steam Runtime

```sh
docker pull registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest
```

Lowest urgency — desktop Linux is a forgiving target and the risk is confined to
glibc symbol versioning, which `objdump -T | grep GLIBC_` can confirm directly.

## Deliverables

- [ ] A `results.md` in this directory recording what happened per preset, with
      real command output rather than a summary
- [ ] `docs/DEVELOPMENT.md § Status` updated to move rows from "not run" to
      verified
- [ ] `wreel-probe` output from at least one real handheld, committed as a
      reference for what these devices report
- [ ] Fixes for whatever breaks, ideally as separate commits from this snapshot

## Open questions

- Is upstream SDL2 viable on Miyoo Mini stock firmware, or is
  `WREEL_USE_SYSTEM_SDL2=ON` mandatory there? This is the largest single unknown
  in the project, and it decides whether the hermetic-FetchContent approach holds
  for the hardest target or needs a documented exception.
- Should CI be added now? A GitHub Actions matrix over `desktop-software`,
  `desktop-debug` and the two aarch64 compile-checks would catch regressions
  cheaply. The container-based targets are harder and can come later.

## References

- [docs/DEVELOPMENT.md § Cross-compiling](../../docs/DEVELOPMENT.md)
- [docs/TARGETS.md § 2 — the glibc trap](../../docs/TARGETS.md)
