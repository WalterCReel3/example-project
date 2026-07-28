# Development environment

Setting up a Debian-based machine to build for desktop, Steam on Linux, and
Linux retro handhelds.

See [Status](#status) at the bottom for exactly which targets have been built and
run, and which have not. That distinction is maintained deliberately — treat
anything not listed as verified with suspicion.

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
| GL | `libgl1-mesa-dev libegl1-mesa-dev libgles-dev libegl-dev` | desktop GL, GLES, EGL |
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
  audio/  gfx/  loaders/  math/  posix/  util/
posix/              libposix   — errno → typed C++ exceptions
util/               libutil    — file I/O, logging, string/tokenizing
  posix/  mswin/      compile-time platform backends (pimpl, not virtual)
audio/              libaudio   — sound effects and music (SDL2_mixer)
gfx/                libgfx     — SDL2 + rendering
  software/           the GPU-less backend
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

# Native build with both renderers, so the skratch demo builds
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

The first configure of any preset clones and builds eight pinned dependencies
from source and takes a couple of minutes. Subsequent configures are ~2 seconds.
`ccache` is picked up automatically if installed, which matters a lot when
building the same dependencies for several targets.

### Configure-time options

| Option | Default | Meaning |
|---|---|---|
| `WREEL_ENABLE_GLES2` | follows `WREEL_TARGET_HAS_GPU` | build `gfx::gles2`. Rejected on a target with no GPU. Empty means auto; set `ON`/`OFF` to override |
| `WREEL_USE_SYSTEM_SDL2` | `OFF` | link the sysroot's SDL2 instead of building the pinned copy |
| `WREEL_BUILD_TESTS` | `ON` | build the doctest suite |
| `WREEL_BUILD_DEMOS` | `ON` | build the demo applications. Gated per demo: `skratch` is skipped where `gfx::gles2` is unavailable, but a `gfx::renderer` demo builds on every target |
| `WREEL_BUILD_PROBE` | `ON` | build `wreel-probe` |
| `WREEL_WERROR` | `ON` | treat warnings as errors. The tree is at zero warnings on all five configured presets, verified 2026-07-27. **Changing this default does not change an existing build directory** — `option()` will not overwrite a cache entry, so a directory configured before 2026-07-26 kept `OFF` and reported it in the configure summary. Pass `-DWREEL_WERROR=ON` once, or check the summary. Recorded as D19 |
| `WREEL_STATIC_CXX` | `OFF` | static-link libstdc++/libgcc. Forced `ON` by every device toolchain |
| `WREEL_AUDIO_CODECS` | per-target | `minimal` \| `standard` \| `full`. Affects **binary size only** — see below |
| `WREEL_AUDIO_RATE` | 44100 / 22050 | mixer sample rate. Affects **per-frame CPU** |
| `WREEL_AUDIO_BUFFER` | 1024 / 2048 | mixer buffer in samples |
| `WREEL_AUDIO_CHANNELS` | `2` | 1 mono, 2 stereo |
| `WREEL_AUDIO_VOICES` | 16 / 8 | simultaneous sound effect voices |

Audio is a base requirement — `wreel::audio` always builds. The codec tier and the
mixer profile are independent knobs and cost different things: extra codecs cost
bytes on disk (`full` is ~282 KB over `minimal`), while the mixer profile costs
cycles every callback. So a FLAC-capable audio player on a handheld is one flag,
not a tradeoff:

```sh
cmake --preset miyoomini -DWREEL_AUDIO_CODECS=full
```

Full reasoning in [TARGETS.md § Audio](TARGETS.md#audio).

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

### Miyoo Mini

The `miyoomini` preset has two modes, like the aarch64 ones.

**Compile-check (no download, not shippable).** With no device toolchain present
it falls back to Debian's `arm-linux-gnueabihf` cross-GCC automatically:

```sh
sudo apt install crossbuild-essential-armhf qemu-user-static binfmt-support
cmake --preset miyoomini && cmake --build --preset miyoomini
ctest --preset miyoomini          # runs under qemu-arm
```

This is genuinely useful — it exercises 32-bit ARM codegen, the Cortex-A7 flags
and `off_t` width in a couple of minutes rather than after a 279 MB toolchain
download. **Two things it does not prove:**

- **The compiler.** It uses GCC 12, not the device toolchain's GCC 8.3, so C++17
  library gaps stay invisible.
- **The display path.** Debian's armhf cross has no target libdrm/libgbm, so SDL2
  silently builds *without KMSDRM* — the probe reports `wayland, offscreen,
  dummy, evdev`. Since KMSDRM is how a handheld actually reaches its panel, video
  is untested by this mode.

**Device toolchain (shippable).** GCC 8.3 and a matched sysroot, from
[union-miyoomini-toolchain](https://github.com/shauninman/union-miyoomini-toolchain):

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
| `scripts/bootstrap-debian.sh` | **verified** — full `--all` install on Debian 12, 50/50 packages, shellcheck clean |
| `.clang-format` | **verified** — config parses under clang-format 14; all authored files conform |
| [TARGETS.md](TARGETS.md) constraints | researched and verified upstream |
| Dependency choices | settled — SDL2, nlohmann/json, pugixml, glm, doctest |
| Modern CMake build | **verified** on system CMake 3.25 |
| `CMakePresets.json` | **verified** — 7 presets enumerate and configure |
| `gfx::renderer` (SDL_Renderer) | **verified** — builds on x86_64, aarch64 and armv7. `test_renderer` pins driver selection: `PreferAccelerated` degrades to software, `Accelerated` refuses to |
| Accelerated 2D on Mali | **build verified only** — `rk3326`/`h700` now compile SDL with the `opengles2` render driver (D18). Whether the vendor blobs expose it is a hardware question |
| `audio` module | **verified** — opens on pulseaudio and dummy; 3 codec tiers build |
| `wreel-probe` | **verified** — runs on x86_64 and as an aarch64 binary under qemu; reports audio |
| `rig::Pad` | **verified on the dev box, keyboard path only** — `SDL_GameController` when SDL recognises the device, raw `SDL_Joystick` otherwise, keyboard always. No pad has been enumerated on a handheld yet, which is the point: it logs name, GUID, axis/button/hat counts and whether a mapping existed, so a device answers the question instead of being guessed at. **The raw fallback's button order is unverified and says so in the log** |
| `coppers::Playlist` | **verified** — `test_playlist`, 6 cases stepping the three real `.mod` files both ways, and skipping a missing file, a non-module and an all-dead playlist without throwing. Asserts `playing()` at each step, not just the track name: the first version checked only the name and passed straight through D20, which left the mixer silent |
| `audio::Music` lifetime | **verified** — two tracks alive at once, releasing the superseded one leaves the current one playing (D20). Both regression tests were confirmed to fail against the old destructor before the fix was kept |
| `gfx::renderer::Texture` | **verified** — owning, movable, colour-modulated. `Context::draw()` takes source and destination rects, so an atlas cell is expressible for the first time; `coppers`' scroller is the first consumer and `draw_surface` is now built on it. This is what [software-2d-sprites-tiling](../planning/2026-07-25-software-2d-sprites-tiling/) was blocked on |
| `gfx::renderer::Layer` | **verified** — CPU-plotted streaming texture with a scope-guarded lock. `coppers` draws its bar field through it on x86-64 and armv7; the two produce byte-identical screenshots. This is the locked-pixels view two snapshots were holding open as a design question |
| `coppers` demo | **verified on the dev box** — builds on all five presets including `miyoomini`, renders copper bars through `gfx::renderer` on both the software and `opengl` drivers, and its `--screenshot`/`--seconds` make a fill-rate run a command. Never run on a device |
| Fill rate | **measured on the dev box only** — [target-validation/results.md](../planning/2026-07-25-target-validation/results.md). A lower internal resolution is a net *loss* on the software driver and a 2.8× win on an accelerated one; the answer inverts, so there is no single default |
| doctest suite | **verified** — 15 executables, 15/15 on all five configured presets: both desktop, plus `rk3326`, `h700` and `miyoomini` under qemu. Re-run 2026-07-27 with `-DWREEL_WERROR=ON` after D19 |
| `util::ascii` predicates | **verified** — `test_ascii`, 12 cases / 1017 assertions; replaces the `<ctype.h>` predicates in `string.hpp` |
| `util::logging` | **verified** — `test_logging`, 11 cases / 38 assertions. printf-style, no iostreams; armv7 `wreel-probe` dropped 865 KB (28%) |
| `util::xml` | **verified** — `test_xml`, 19 cases / 116 assertions against the real Sparrow atlas in `data/`. pugixml `v1.16`, XPath compiled out, and confirmed private to `util/xml.cc`: no consumer of `wreel::util` gets pugixml's include path |
| `util::from_string` | **verified** — `test_number`, 11 cases / 77 assertions. Strict whole-string conversion; `include/util/number.hpp` |
| `rig::FrameTiming` | **verified** — `test_timing`, 10 cases. Clamped delta, monotonicity, fps seeding and smoothing, all over supplied timestamps so nothing sleeps. `rig::FrameClock` is deliberately untested: it adds a `steady_clock` read and an `SDL_Delay` |
| `rig::asset_path` | **verified for the shipped layout** — `test_assets` covers the `WREEL_DATA_DIR` override, caching and separator handling; the "beside the executable" rule was confirmed against a real `cmake --install` tree run from `/tmp`, which resolved `bin/data/` and rendered. That invocation failed before |
| `rk3326` / `h700` toolchains | **verified** — cross-build plus `ctest` under qemu |
| `miyoomini` toolchain | **verified in compile-check mode** — armv7 build + `ctest` under qemu-arm. Device toolchain (GCC 8.3) still untried |
| `gfx::gles2` | **verified on the dev box** — `skratch` renders a 20×20 grid through it, confirmed with `--screenshot`. ES 2.0 profile from Mesa. No GL library linked; entry points come from `SDL_GL_GetProcAddress`. Never run on a device |
| 2016 fixed-function backend | **deleted** 2026-07-26 with GLEW and GLU, once `skratch` was ported |
| `steam` preset | not run — needs the sniper container |
| `docker/miyoomini.Dockerfile` | not built — needs Docker plus the upstream base image |
| `gfx::gles2` | not started — stage 3 of [the renderer snapshot](../planning/2026-07-26-gfx-renderer-and-gles2/) |
| C++17 cleanup of 2016 sources | **done.** From 167 warnings to **zero on all five configured presets**, with `WREEL_WERROR=ON` since 2026-07-26. The last 30 went with the 2016 fixed-function sources rather than being polished in place, which is what [the renderer snapshot](../planning/2026-07-26-gfx-renderer-and-gles2/) was for |

### What has actually been run

On Debian 12 / GCC 12.2 / CMake 3.25 / clang-format 14, after a full
`./scripts/bootstrap-debian.sh --all`:

| Check | Result |
|---|---|
| All five presets rebuilt with `-DWREEL_WERROR=ON` | 2026-07-27: **15/15 tests and zero warnings on every one.** This is the first run in which `-Werror` was actually in effect — see D19 |
| `coppers` on all five presets | builds everywhere, including `miyoomini` where no demo could be built before. Renders through the software driver and through `opengl` |
| `coppers` armv7 under qemu-arm | runs the full path — layer, lock, plot, scaled blit, present, BMP. Its *timings* are meaningless under qemu and must not be quoted as Cortex-A7 figures |
| `skratch` from an installed bundle, launched from `/tmp` | resolves `bin/data/` via `rig::asset_path`, loads font and model, screenshots correctly. The relative-path landmine is gone |
| `desktop-software` cold configure → build → test | pass, zero errors, zero warnings |
| `rk3326` cross-build → `ctest` under qemu | pass |
| `h700` cross-build → `ctest` under qemu | pass, `-mcpu=cortex-a53` confirmed |
| `miyoomini` armv7 build → `ctest` under qemu-arm | pass, `-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4` confirmed. `util/ascii.hpp` and the new logger compile warning-free on both ARM cross compilers, where `char` is unsigned. This preset caught a `long`-width assumption in `test_number` that both 64-bit presets accepted |
| `desktop-debug` build → test | pass, zero warnings under `-Werror`; `skratch` renders and screenshots |
| `wreel-probe` as an aarch64 binary under qemu | runs, reports correctly |
| `shellcheck scripts/bootstrap-debian.sh` | clean |
| `clang-format --dump-config` | parses; authored files conform |
| Configure guards ×4 | all reject correctly with actionable messages |

Two things this pass found and fixed, both of which had been asserted as working
without being tested:

- **`.clang-format` did not parse at all.** It used `ConstructorInitializerIndentation`,
  which is not a clang-format key. Every invocation errored out, so "the tree is
  formatted" was meaningless. Now `ConstructorInitializerIndentWidth`.
- **Cross-built tests could not run under qemu.** The binaries are dynamically
  linked against `/lib/ld-linux-aarch64.so.1`, which does not exist on an x86_64
  host, so every test failed with `Could not open`. The toolchain files now pass
  `-L <sysroot>` to qemu so it finds the target loader.

### The `gles2` renderer

```sh
cmake --preset desktop-debug && cmake --build --preset desktop-debug
./build/desktop-debug/bin/wreel-probe        # what does this machine offer?
```

**Verified working on the dev box.** The probe asks for an ES 2.0 profile and Mesa
returns a real one — `OpenGL ES 3.2 Mesa 22.3.6`, GLSL ES 3.20, max texture 16384 —
which is what makes GLES2 developable natively rather than only on a device. It
also reports a default-profile context separately, so a driver that offers desktop
GL but no ES profile is visible rather than silently absent.

No GL library is linked anywhere in the tree. `gfx/gles2/api.cc` resolves every
entry point through `SDL_GL_GetProcAddress`, and `wreel-probe` does the same for
the two it reports with. Three reasons, in
[include/gfx/gles2/api.hpp](../include/gfx/gles2/api.hpp); the decisive one is that
both renderers live in one library, so a `DT_NEEDED` on `libGLESv2` would stop a
**2D-only** binary from starting on a device whose firmware has no GLES blob — the
dynamic loader resolves that before `main()` and there is no degrading from it.

`GLEW` and `GLU` are gone with the 2016 backend, and so is the `--legacy` bootstrap
group.

### Running the `skratch` demo

`skratch` is not the game. It is the worked example of how GL is structured now
versus how it was structured in 2016 — explicit matrices instead of `glRotatef`,
shaders instead of the fixed-function pipeline, object lifetimes instead of global
state. The game path is `gfx::renderer`, whose own demo is scoped in
[planning/2026-07-25-software-2d-sprites-tiling](../planning/2026-07-25-software-2d-sprites-tiling/).

Read this before the first run — it goes fullscreen and takes over the display,
which is the intended presentation, not an accident.

```sh
cd /path/to/example-project          # MUST be the repo root
./build/desktop-debug/bin/skratch
```

**Or don't take over the display at all:**

```sh
./build/desktop-debug/bin/skratch --screenshot /tmp/frame.bmp
```

That renders two frames, writes the second to a BMP and exits 0. It is the only
automated evidence that this renderer draws anything — there is no headless GL, so
`gfx::gles2` has no unit tests, and "the process did not crash" is not the same as
"geometry appeared". It is also how a handheld gets checked over SSH, where nobody
can see the panel.

**Three things that will bite otherwise:**

- **Working directory must be the repo root.** It opens `data/Speedy.fon` and
  `data/ico.obj` by relative path. Running it from `build/` fails immediately.
- **It is fullscreen at the panel's native mode**, via
  `SDL_WINDOW_FULLSCREEN_DESKTOP`, with the cursor hidden.
- **The log is not in the working directory.** It goes to
  `SDL_GetPrefPath("wreel", "skratch")` — on Linux
  `~/.local/share/wreel/skratch/skratch.log`. A silent instant exit means *read
  that file*. It used to be `runlog.txt` beside the binary, which fails silently on
  a read-only mount.

**Controls** (from `skratch/input.cc`):

| | |
|---|---|
| `Escape` | quit |
| `W` / `S` | forward / back |
| `A` / `D` | strafe left / right |
| `Space` / `Left Ctrl` | up / down |
| Arrow keys | pitch and yaw |
| Mouse | pitch and yaw (relative) |
| Joystick axes 0/1, 3/4, hat | move and look, if a pad is attached |

Roll is wired into `InputState` but has **no** keyboard binding, so it is
unreachable. Joystick axis mapping is hardcoded for an Xbox 360 pad.

**If input does not work**, `Escape` is not the only way out:

- `Ctrl+C` in the launching terminal — SDL2 turns `SIGINT` into `SDL_QUIT`, which
  `translate_input()` maps to `EXIT`, so this exits cleanly.
- `pkill -x skratch` from another terminal or over SSH.
- `Ctrl+Alt+F3` to switch VT, then `pkill`.

**What you should see**, and what was observed 2026-07-26 via `--screenshot`: a
20×20 grid of icosahedra receding with correct perspective on a dark blue
background, each shaded by interpolated vertex colours, with a white HUD line at
top-left showing camera position, orientation and joystick axis values. No
lighting — vertex colours are faked from position by the OBJ loader — and the far
plane is 100 units, so the far end of the grid clips out.

The camera now starts at `(25, 25, 30)` looking into the grid. The 2016 demo
started at the origin, which is exactly where its first instance sits, so it opened
from *inside* an icosahedron with one model's gradient filling the screen. The port
reproduced that faithfully before the starting position was changed.

### Still not run

- **The device toolchain build.** Needs Docker group membership plus the upstream
  base image and a 279 MB toolchain download. This is the only way to find out
  whether **GCC 8.3 compiles this codebase**, which is the single largest
  remaining unknown.
- **`steam`** — needs the sniper container.
- **Any video or audio output on real hardware.** Both the armv7 and aarch64
  compile-check builds lack KMSDRM, so the display path is entirely untested.

Pre-flight checks that *were* done, so the above should not surprise anyone:
the toolchain tarball resolves (HTTP 200, 279 MB), `archive.debian.org` is
reachable for the EOL buster base, and the CMake/Ninja versions pinned in
`docker/miyoomini.Dockerfile` both download.

## See also

- [TARGETS.md](TARGETS.md) — device matrix, graphics backends, dependency pins
- [../README.md](../README.md) — project overview
