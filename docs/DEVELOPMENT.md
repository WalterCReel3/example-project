# Development environment

Setting up a Debian-based machine to build for desktop, Steam on Linux, and
Linux retro handhelds.

Everything in the "Host setup" section works today. Sections marked
**⏳ not yet wired** describe the intended interface for build machinery that is
still landing — see [Status](#status) at the bottom for exactly where the line
is.

**Assumed audience:** Debian 12 (bookworm) or a derivative. Ubuntu 22.04+,
Linux Mint 21+, and Pop!_OS 22.04+ use identical package names. Package names
below were verified against bookworm's apt metadata, not written from memory.

---

## Host setup

### Quick start

```sh
git clone <your-fork> example-project
cd example-project

# See what would be installed, change nothing:
./scripts/bootstrap-debian.sh --dry-run

# Install the default set (core + SDL deps + codecs + tools):
./scripts/bootstrap-debian.sh

# Or everything, including cross-compilers, Docker, and MIDI:
./scripts/bootstrap-debian.sh --all
```

The script is idempotent, uses `--no-install-recommends`, and finishes with a
verification pass over the installed toolchain. `--list` shows the groups.

### What gets installed, and why

Run `./scripts/bootstrap-debian.sh --list` for the summary. The reasoning:

#### `core` — build tooling

`build-essential cmake ninja-build git pkg-config ccache file`

Bookworm ships **CMake 3.25.1**, comfortably past the 3.21 minimum this project
needs for `CMakePresets.json` schema v3. `ccache` matters more than usual here:
building SDL2 from source for five targets means a lot of repeated compilation.

#### `sdl` — SDL2's *build* dependencies

This is the group people get wrong. Because SDL2 is built **from source** (see
[TARGETS.md § Why source builds](TARGETS.md#why-source-builds-instead-of-libsdl2-dev)),
what's needed is what *SDL* needs to compile, not `libsdl2-dev`.

SDL2's configure step is permissive: if a backend's headers are missing it
**silently omits that backend** and the build still succeeds. You find out later,
at runtime, as `SDL_Init` failing with *"no available video device"*. So install
all of them:

| Sub-group | Packages | Backend enabled |
|---|---|---|
| Audio | `libasound2-dev libpulse-dev libjack-dev libsndio-dev` | ALSA, PulseAudio, JACK, sndio |
| X11 | `libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev libxkbcommon-dev` | X11 video |
| Wayland | `libwayland-dev wayland-protocols libdecor-0-dev` | Wayland video |
| KMSDRM | `libdrm-dev libgbm-dev` | direct-to-display video |
| GL | `libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev libgles-dev libegl-dev` | desktop GL, GLES, EGL |
| Input | `libudev-dev libdbus-1-dev libibus-1.0-dev` | hotplug, gamepads, IME |

Two notes worth keeping:

- **KMSDRM is not optional even on desktop.** It is the backend the handhelds
  actually use. Keeping it buildable on your dev box is how you notice device
  breakage without flashing an SD card.
- **`libgles2-mesa-dev` is a transitional dummy package** on bookworm. The real
  one is **`libgles-dev`**. Older guides still name the dummy.

#### `codecs` — image and font codecs

`libpng-dev libjpeg-dev libfreetype-dev libharfbuzz-dev`

Optional. SDL2_image and SDL2_ttf can both vendor their own copies
(`SDLIMAGE_VENDORED=ON`, `SDLTTF_VENDORED=ON`), which is what the cross builds
do so they don't need per-arch codec libraries. On the host, using the system
copies builds faster.

#### `tools` — analysis and debugging

`clang-format clang-tidy gdb cppcheck shellcheck`

`clang-format` 14 on bookworm, which is what [.clang-format](../.clang-format)
targets.

#### `cross` — cross compilers

`crossbuild-essential-armhf crossbuild-essential-arm64 qemu-user-static binfmt-support patchelf`

Read [TARGETS.md § the glibc forward-compatibility trap](TARGETS.md#2-the-glibc-forward-compatibility-trap)
before using these. Short version: **Debian's cross-GCC is for compile-checking
only.** It links against bookworm's glibc 2.36 and its output will not run on any
handheld. It is still worth having — it catches `-Werror` and ABI problems in
seconds rather than minutes.

`qemu-user-static` + `binfmt-support` register ARM binary handlers with the
kernel, which means cross-built **test binaries execute directly on your dev
box**:

```sh
$ file build/miyoomini/bin/test_file
... ELF 32-bit LSB pie executable, ARM, EABI5 ...
$ ./build/miyoomini/bin/test_file      # just works, via qemu
```

Verify registration:

```sh
ls /proc/sys/fs/binfmt_misc/ | grep qemu
# if empty:
sudo systemctl restart systemd-binfmt
```

#### `container` — device SDKs

`docker.io`

The device toolchains and the Steam Runtime are distributed as container images.
Podman works too if you prefer it — substitute `podman` for `docker` throughout.

Add yourself to the `docker` group to avoid `sudo` on every invocation:

```sh
sudo usermod -aG docker "$USER"
newgrp docker      # or log out and back in
```

#### `midi` and `math` — optional

`librtmidi-dev alsa-utils` and `libglm-dev`.

RtMidi 5.0 covers the MIDI-driven live-visuals goal. `aconnect -l` (from
`alsa-utils`) lists ALSA sequencer ports, which is how you find a controller.
GLM is header-only matrix/vector math for when the shader backends need it.

### Older Debian / Ubuntu

CMake older than 3.21 cannot read this project's presets:

| Distro | CMake | OK? |
|---|---|---|
| Debian 12 bookworm | 3.25.1 | yes |
| Debian 11 bullseye | 3.18.4 | **no** |
| Ubuntu 24.04 | 3.28 | yes |
| Ubuntu 22.04 | 3.22 | yes |
| Ubuntu 20.04 | 3.16 | **no** |

Easiest fix without touching system packages:

```sh
python3 -m venv ~/.venv/cmake
~/.venv/cmake/bin/pip install cmake ninja
export PATH="$HOME/.venv/cmake/bin:$PATH"    # add to ~/.bashrc
```

Or use [Kitware's APT repository](https://apt.kitware.com/) for a system-wide
current CMake.

---

## Repository layout

The 2016 organisation is deliberately preserved: flat top-level module
directories, each building one static library, with all public headers hoisted
into a single `include/<module>/` tree.

```
include/            public headers, one directory per module
  gfx/  loaders/  math/  posix/  util/
posix/              libposix   — errno → typed C++ exceptions
util/               libutil    — file I/O, logging, string/tokenizing
  posix/  mswin/      compile-time platform backends (pimpl, not virtual)
gfx/                libgfx     — SDL2 + rendering
loaders/            libloaders — OBJ, image, texture atlas
skratch/            demo application
probe/              wreel-probe — device capability diagnostic
tests/              doctest suite, run from the repo root against data/
data/               fixtures and assets
cmake/              ProjectOptions, Dependencies, Testing, Packaging
  toolchains/         one file per cross target
docker/             build environments for device SDKs
scripts/            developer tooling
docs/               this
```

Dependencies run one way only: `skratch` → `loaders` → `util` → `posix`.

Tests run with the **repository root** as their working directory, so fixture
paths like `data/test.json` resolve. That is intentional and worth preserving.

---

## Building

```sh
# List available configurations
cmake --list-presets

# Native build with the software renderer — this is the Miyoo Mini code path,
# at native speed, with no cross-compile. Verified working.
cmake --preset desktop-software
cmake --build --preset desktop-software
ctest --preset desktop-software

# Native build with the legacy GL renderer (needs GLEW + GLU installed)
cmake --preset desktop-debug
cmake --build --preset desktop-debug

# Handhelds and Steam — see "Cross-compiling" below for prerequisites
cmake --preset miyoomini
cmake --preset rk3326
cmake --preset h700
cmake --preset steam
```

Each preset gets its own build tree under `build/<preset>/` and its own install
prefix under `dist/<preset>/`, so switching targets never triggers a reconfigure
storm. Binaries land in `build/<preset>/bin/`, static libraries in `lib/`.

The first configure of any preset clones and builds five pinned dependencies
from source and takes a couple of minutes. Subsequent configures are ~2 seconds.
`ccache` is picked up automatically if installed, which matters a lot when
building the same dependencies for several targets.

### Configure-time options

| Option | Default | Meaning |
|---|---|---|
| `WREEL_GFX_BACKEND` | per-target | `software` or `gl_legacy`. Invalid values are rejected at configure time |
| `WREEL_USE_SYSTEM_SDL2` | `OFF` | link the sysroot's SDL2 instead of building the pinned copy |
| `WREEL_BUILD_TESTS` | `ON` | build the doctest suite |
| `WREEL_BUILD_DEMOS` | `ON` | build `skratch` — forced `OFF` unless the backend is `gl_legacy` |
| `WREEL_BUILD_PROBE` | `ON` | build `wreel-probe` |
| `WREEL_WERROR` | `OFF` | treat warnings as errors. Off because the 2016 sources do not survive `-Wall -Wextra` yet; flips to `ON` with the C++17 cleanup |
| `WREEL_STATIC_CXX` | `OFF` | static-link libstdc++/libgcc. Forced `ON` by every device toolchain |

Configuring prints a summary of all of these, so you can confirm what you got:

```
-- === wreel 0.2.0 ===
--   target id .......... desktop
--   system ............. Linux / x86_64
--   compiler ........... GNU 12.2.0
--   gfx backend ........ software
--   target has GPU ..... OFF
...
```

### A note on CMake 4.x

CMake 4.0 removed support for `cmake_minimum_required(VERSION < 3.5)`, and
SDL2_ttf's bundled FreeType still declares one. The build sets
`CMAKE_POLICY_VERSION_MINIMUM=3.5` for dependency builds to work around this, so
CMake 3.21 through 4.x all configure. Debian 12's 3.25 never hits it.

---

## Cross-compiling

### Compile-check pass (fast, not shippable)

The `rk3326` and `h700` presets default to Debian's cross-GCC, which catches
breakage in seconds. They print a warning saying the output is not shippable, and
[TARGETS.md § 2](TARGETS.md#2-the-glibc-forward-compatibility-trap) explains why.

```sh
sudo apt install crossbuild-essential-arm64 qemu-user-static binfmt-support

cmake --preset rk3326
cmake --build --preset rk3326
ctest --preset rk3326      # test binaries run under qemu-user-static
```

For a shippable build, add a device sysroot:

```sh
cmake --preset rk3326 -DWREEL_SYSROOT=/path/to/device/rootfs \
                      -DWREEL_USE_SYSTEM_SDL2=ON
```

### Miyoo Mini (shippable)

The community toolchain is a Docker image providing GCC 8.3 and a matched
sysroot. From [union-miyoomini-toolchain](https://github.com/shauninman/union-miyoomini-toolchain):

```sh
git clone https://github.com/shauninman/union-miyoomini-toolchain.git
cd union-miyoomini-toolchain
make shell        # builds the image on first run, then drops into a shell
```

The container binds its `~/workspace` to `./workspace` on the host, so clone or
symlink this repo into `union-miyoomini-toolchain/workspace/` and build from
inside the container shell.

What the image sets up:

| Thing | Value |
|---|---|
| Toolchain root | `/opt/miyoomini-toolchain` |
| Compiler prefix | `/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-` |
| Target triple | `arm-linux-gnueabihf` |
| Sysroot | `/opt/miyoomini-toolchain/usr/arm-linux-gnueabihf/sysroot` |
| `$CROSS_COMPILE` | set to the compiler prefix |
| `$PREFIX` | `<sysroot>/usr` |
| GCC | **8.3** (ARM A-profile 8.3-2019.03) |

**One gotcha, already handled:** the upstream image is Debian 10 buster, whose
CMake is 3.13 — too old for this project's presets, which need 3.21.
[docker/miyoomini.Dockerfile](../docker/miyoomini.Dockerfile) layers a pinned
current CMake and Ninja on top so no manual setup is needed:

```sh
# 1. Build the upstream base image (not published to any registry)
git clone https://github.com/shauninman/union-miyoomini-toolchain.git
cd union-miyoomini-toolchain && make .build     # tags 'miyoomini-toolchain'

# 2. Layer a modern CMake over it, from this repository
cd /path/to/example-project
docker build -f docker/miyoomini.Dockerfile -t wreel-miyoomini .

# 3. Build
docker run --rm -it -v "$PWD":/src -w /src wreel-miyoomini bash
cmake --preset miyoomini && cmake --build --preset miyoomini
```

The Dockerfile sets `MIYOOMINI_TOOLCHAIN_ROOT` so the toolchain file finds the
compiler without further configuration. To use the upstream image directly
instead, install a current CMake into it yourself and set that variable by hand.

### Steam on Linux

Steam games should be built against the **Steam Runtime**, not your host, so
they run on any user's distribution. The `sniper` runtime is Debian 11 based
(glibc 2.31).

```sh
docker pull registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest

docker run --rm -it \
  -v "$PWD":/src -w /src \
  registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest \
  bash
```

Same CMake-version caveat applies inside that container.

### Anbernic (RK3326 / H700)

These run real Linux distributions — ArkOS, ROCKNIX, muOS, Batocera — with Mesa
or vendor Mali drivers and a distro SDL2. Two approaches:

1. **Device sysroot** (recommended for shipping): copy `/usr` off a device image
   and point CMake at it with `-DCMAKE_SYSROOT=`. Set
   `-DWREEL_USE_SYSTEM_SDL2=ON` to link the firmware's SDL2.
2. **Debian cross-GCC** for compile-checking, per above.

Check the target's glibc before shipping — see
[TARGETS.md](TARGETS.md#2-the-glibc-forward-compatibility-trap).

---

## Code style

[.clang-format](../.clang-format) and [.editorconfig](../.editorconfig) codify
the style already in the tree: 4-space indent, 80 columns, brace-on-next-line
for functions and types, attached braces for control flow, `Type* name` pointer
binding.

```sh
# Format a file in place
clang-format -i gfx/context.cc

# Check the whole tree without modifying it
find . -name '*.cc' -o -name '*.hpp' | grep -v '^./build' \
  | xargs clang-format --dry-run --Werror
```

Two inherited quirks these files intentionally settle:

- The old sources carry `vim: set sts=2 sw=2 expandtab` modelines that
  **contradict** their own 4-space indentation. The modelines are wrong; drop
  them as you touch files.
- Include guards are `__NAME_HPP__`. Identifiers with leading double
  underscores are reserved for the implementation; new headers should use
  `#pragma once` or an unreserved guard.

---

## Status

| Thing | State |
|---|---|
| `scripts/bootstrap-debian.sh` | working, verified on Debian 12 |
| Package list | verified against bookworm apt metadata |
| `.clang-format` / `.editorconfig` / `.gitignore` | in place |
| [TARGETS.md](TARGETS.md) constraints | researched and verified upstream |
| Dependency choices | settled — SDL2, nlohmann/json, doctest |
| Modern CMake build | **working** |
| `CMakePresets.json` | **working** — 7 presets |
| `cmake/toolchains/*.cmake` | written; error paths verified, real cross builds **not** yet run |
| `docker/miyoomini.Dockerfile` | written, **not** yet built |
| `software` graphics backend | **working** — window, renderer, blit, text |
| `wreel-probe` | **working** |
| doctest suite | **working** — 28 cases, 71 assertions passing |
| `gles2` / `gl33` backends | not started |
| C++17 cleanup of 2016 sources | not started (`WREEL_WERROR` stays `OFF` until then) |

### What has actually been run

Verified end to end on Debian 12 / GCC 12.2 / CMake 4.4:

- `cmake --preset desktop-software` — configures, all five dependencies fetched
  and built
- `cmake --build --preset desktop-software` — clean build, zero errors
- `ctest --preset desktop-software` — 3/3 tests pass, 71 assertions
- `wreel-probe` — runs headless under `SDL_VIDEODRIVER=dummy` and correctly
  reports the software renderer with GL compiled out
- Configure-time guards — rejecting an unknown backend, rejecting `gl_legacy` on
  a GPU-less target, and both missing-cross-toolchain messages

### What has NOT been run

Be appropriately sceptical of these until someone tries them:

- **`desktop-debug` / `desktop-release`** (the `gl_legacy` backend) — this box has
  no GLEW or GLU installed, so `find_package(GLEW)` was never exercised. The 2016
  GL sources have not been compiled under the new build.
- **`miyoomini`, `rk3326`, `h700`** — no cross-compilers or device SDK present.
  Only the not-found error paths were checked.
- **`steam`** — needs the sniper container.
- **`docker/miyoomini.Dockerfile`** — needs Docker plus the upstream base image.
- **Anything on real hardware.**

## See also

- [TARGETS.md](TARGETS.md) — device matrix, graphics backends, dependency pins
- [../README.md](../README.md) — project overview
