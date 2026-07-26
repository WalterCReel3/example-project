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

#### `legacy` — GLEW and GLU, for `gl_legacy` only

`libglew-dev libglu1-mesa-dev`

**Not in the default set.** These are needed by exactly one thing: the
`gl_legacy` backend, where `gfx/context.cc` calls `glewInit()` and
`gluPerspective()`. SDL2's own CMakeLists references neither, the `software`
backend needs neither, and no handheld target can use `gl_legacy` at all — the
Miyoo Mini has no GPU.

Kept as its own group so that when `gl_legacy` is retired, the packages go with
it. Buried among twenty GL entries in the `sdl` group they would quietly outlive
the code that needed them, and the next person would have no way to tell which
were still load-bearing.

```sh
./scripts/bootstrap-debian.sh --legacy   # just these two
```

`desktop-software` — the working preset — needs none of it, and if you do want
`gl_legacy` the configure error names the package.

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
| `WREEL_ENABLE_GLES2` | `OFF` | build `gfx::gles2`. Rejected on a target with no GPU. Currently always `OFF` — the sources land in stage 3 of [the renderer snapshot](../planning/2026-07-26-gfx-renderer-and-gles2/) |
| `WREEL_ENABLE_GL_LEGACY` | `OFF` | build the 2016 fixed-function backend. Being retired; only `skratch` needs it |
| `WREEL_USE_SYSTEM_SDL2` | `OFF` | link the sysroot's SDL2 instead of building the pinned copy |
| `WREEL_BUILD_TESTS` | `ON` | build the doctest suite |
| `WREEL_BUILD_DEMOS` | `ON` | build `skratch` — forced `OFF` unless `WREEL_ENABLE_GL_LEGACY` is on |
| `WREEL_BUILD_PROBE` | `ON` | build `wreel-probe` |
| `WREEL_WERROR` | `OFF` | treat warnings as errors. Off because the 2016 GL sources do not survive the full warning set yet — 30 remain on `desktop-debug`, down from 167. `desktop-software` is already clean and could be gated today. See [planning/2026-07-25-cxx17-modernization](../planning/2026-07-25-cxx17-modernization/) |
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
| doctest suite | **verified** — 10 executables, 114 cases / 1482 assertions, 10/10 on all five configured presets: both desktop, plus `rk3326`, `h700` and `miyoomini` under qemu |
| `util::ascii` predicates | **verified** — `test_ascii`, 12 cases / 1017 assertions; replaces the `<ctype.h>` predicates in `string.hpp` |
| `util::logging` | **verified** — `test_logging`, 11 cases / 38 assertions. printf-style, no iostreams; armv7 `wreel-probe` dropped 865 KB (28%) |
| `util::xml` | **verified** — `test_xml`, 19 cases / 116 assertions against the real Sparrow atlas in `data/`. pugixml `v1.16`, XPath compiled out, and confirmed private to `util/xml.cc`: no consumer of `wreel::util` gets pugixml's include path |
| `util::from_string` | **verified** — `test_number`, 11 cases / 77 assertions. Strict whole-string conversion; `include/util/number.hpp` |
| `rk3326` / `h700` toolchains | **verified** — cross-build plus `ctest` under qemu |
| `miyoomini` toolchain | **verified in compile-check mode** — armv7 build + `ctest` under qemu-arm. Device toolchain (GCC 8.3) still untried |
| `gl_legacy` backend | **verified** — `desktop-debug` builds and links, `skratch` included, 6/6 tests |
| `steam` preset | not run — needs the sniper container |
| `docker/miyoomini.Dockerfile` | not built — needs Docker plus the upstream base image |
| `gfx::gles2` | not started — stage 3 of [the renderer snapshot](../planning/2026-07-26-gfx-renderer-and-gles2/) |
| C++17 cleanup of 2016 sources | **in progress.** `string.hpp` done — the `ptr_fun`/`not1`/`unary_function` cluster is gone and character classification moved to `util/ascii.hpp`. `desktop-software` is at **zero warnings**; `desktop-debug` is at 30, down from 167, all in `gl_legacy`/`skratch` files that [the renderer snapshot](../planning/2026-07-26-gfx-renderer-and-gles2/) deletes or rewrites. `WREEL_WERROR` stays `OFF` until those clear |

### What has actually been run

On Debian 12 / GCC 12.2 / CMake 3.25 / clang-format 14, after a full
`./scripts/bootstrap-debian.sh --all`:

| Check | Result |
|---|---|
| `desktop-software` cold configure → build → test | pass, 10/10, zero errors, zero warnings |
| `rk3326` cross-build → `ctest` under qemu | pass, 10/10 |
| `h700` cross-build → `ctest` under qemu | pass, 10/10, `-mcpu=cortex-a53` confirmed |
| `miyoomini` armv7 build → `ctest` under qemu-arm | pass, 10/10, `-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4` confirmed. `util/ascii.hpp` and the new logger compile warning-free on both ARM cross compilers, where `char` is unsigned. This preset caught a `long`-width assumption in `test_number` that both 64-bit presets accepted |
| `desktop-debug` (`gl_legacy`) build → test | pass, 10/10; `skratch` links; probe reports Mesa 22.3.6 / AMD |
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

### The `gl_legacy` backend

```sh
./scripts/bootstrap-debian.sh --legacy    # GLEW + GLU
cmake --preset desktop-debug && cmake --build --preset desktop-debug
```

**Verified working.** The 2016 fixed-function sources compile and link clean under
C++17 with the full warning set, `skratch` links, and `wreel-probe` reports a real
GL context (Mesa 22.3.6 / AMD Radeon, compatibility profile 4.6).

That is a better result than expected — this had been flagged as the most likely
thing in the tree to be broken. The only failure the build surfaced was mine, not
the 2016 code's: `wreel-probe` calls `glGetString()` directly, and
`find_package(OpenGL)` was scoped inside the `gl_legacy` block, so probe compiled
and then failed to link. OpenGL is now looked for whenever the target could have a
GPU, and probe links it only when both `WREEL_TARGET_HAS_GPU` and
`WREEL_HAVE_OPENGL` hold.

### Running the `skratch` demo

Read this before the first run — it goes fullscreen and takes over the display,
which is the intended presentation, not an accident.

```sh
cd /path/to/example-project          # MUST be the repo root
./build/desktop-debug/bin/skratch
```

**Four things that will bite otherwise:**

- **Working directory must be the repo root.** It opens `data/Speedy.fon` and
  `data/ico.obj` by relative path and writes `runlog.txt` to the current
  directory. Running it from `build/` fails immediately.
- **It is fullscreen at the panel's native mode**, via
  `SDL_WINDOW_FULLSCREEN_DESKTOP`. For debugging, pass `false` to
  `System::create_context()` — the `fullscreen` parameter is honoured now, and a
  windowed run gets three quarters of the desktop, centred.
- **The cursor is hidden and the mouse is grabbed** via relative mouse mode. Both
  are conditional on `fullscreen`, so a windowed run leaves your pointer alone.
- **Errors go to `runlog.txt`, not stderr.** `skratch/main.cc` catches everything
  and writes `e.what()` to that file, so a silent instant exit means *check the
  log*, not "nothing happened".

`SDL_VIDEODRIVER=x11` is no longer needed — the mode-change request that made this
X11-shaped is gone. It is still a useful thing to try first if the window does not
appear on your compositor, since GLEW needs GLX and Wayland reaches it through
XWayland.

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

For a first run, a watchdog costs nothing — it is fullscreen and grabs the mouse:

```sh
( sleep 30; pkill -x skratch ) &
./build/desktop-debug/bin/skratch
cat runlog.txt
```

**What you should see:** a 20×20 grid of icosahedra on a dark blue background,
with a white HUD line at top-left showing camera position, orientation and
joystick axis values. No lighting — vertex colours are faked from position — and
the far plane is 100 units, so distant models clip out.

**Already verified, so these are not the likely failure:** the assets load
(`ico.obj` parses to 42 vertices / 240 indices, `teapot.obj` to 3644 / 18960), the
binary links against real GLEW and GLU, and `wreel-probe` from the same build gets
a working GL context. What is untested is whether the window actually appears on
your compositor — no run of this demo has been observed on a real display.

> A dry run under `SDL_VIDEODRIVER=offscreen` is **not** a valid substitute:
> `glewInit()` fails there because GLEW needs GLX, and `runlog.txt` just says
> `Unknown error`.

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
