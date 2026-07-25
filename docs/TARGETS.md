# Targets

The project builds one codebase for three quite different worlds: a desktop dev
box, Steam on Linux, and Linux-based retro handhelds. The handhelds are what
constrain everything — this page records those constraints so decisions
elsewhere don't have to re-derive them.

## Target matrix

| Preset | Devices | SoC | Arch | GPU | Backend today | Toolchain |
|---|---|---|---|---|---|---|
| `desktop-debug` / `-release` | your dev box | any | `x86_64` | any | `gl_legacy` | host GCC/Clang |
| `desktop-software` | your dev box | any | `x86_64` | any | `software` | host GCC/Clang |
| `steam` | Steam / Steam Deck | any | `x86_64` | any | `gl_legacy` | Steam Runtime **sniper** container |
| `miyoomini` | Miyoo Mini, Mini Plus | SigmaStar SSD202D | `armv7-a` | **none** | `software` **only** | `union-miyoomini-toolchain` (GCC 8.3) |
| `rk3326` | RG351P/M/V, RG353P/M/V | Rockchip RK3326 | `aarch64` | Mali-G31 | `software` (awaiting `gles2`) | device sysroot or Debian cross |
| `h700` | RG35XX Plus/H/SP, RG40XX | Allwinner H700 | `aarch64` | Mali-G31 MP2 | `software` (awaiting `gles2`) | device sysroot or Debian cross |

`desktop-software` is how you exercise the Miyoo Mini code path without a device:
same backend, native speed, no cross-compile.

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

Integer `std::from_chars` **is** available in GCC 8. `std::optional`,
`std::variant`, `std::string_view`, structured bindings, `if constexpr`, and
fold expressions are all fine.

> This is why [loaders/obj.cc](../loaders/obj.cc) keeps using `strtod`/`strtol`
> rather than moving to `<charconv>` — floating-point `from_chars` simply is not
> there.

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

### 3. Miyoo Mini has no GPU

The SSD202D is two Cortex-A7 cores and **no 3D block**. There is no OpenGL, no
GLES, no EGL. Everything is CPU blitting through `SDL_Renderer`'s software path
(the vendor firmware routes this through its own `MI_GFX` layer).

Consequences:

- The `software` backend is not a fallback, it is the **only** backend on this
  device, so it has to be good enough to be the baseline everywhere.
- The existing [gfx/](../gfx/) code — `glBegin`/`glEnd`, `glMatrixMode`,
  `gluPerspective`, GLEW — cannot compile for this target at all. It is gated
  behind the desktop-only `gl_legacy` backend.
- RAM is **128 MB total**, shared with the OS. Asset budgets are tight and
  unbounded caches are not an option.

## Graphics backends

Selected at configure time with `-DWREEL_GFX_BACKEND=<name>`.

**The option currently accepts `software` and `gl_legacy` only.** Anything else
is rejected at configure time rather than failing later in the build.

| Backend | API | Runs on | Status |
|---|---|---|---|
| `software` | `SDL_Renderer` software | everything, incl. Miyoo Mini | **implemented** — the baseline |
| `gl_legacy` | fixed-function GL + GLU + GLEW | desktop only | **implemented** — the 2016 code, kept so [skratch](../skratch/) keeps running during the port; will be retired |
| `gles2` | OpenGL ES 2.0 | Mali handhelds, desktop via Mesa | **not written yet** |
| `gl33` | OpenGL 3.3 core | desktop, Steam | **not written yet** |

Until `gles2` exists, the `rk3326` and `h700` presets build with the `software`
backend. Those devices do have Mali GPUs — `WREEL_TARGET_HAS_GPU` records device
capability, not backend readiness.

`gl_legacy` cannot be ported forward: GL 3.3 core removed the entire
fixed-function pipeline it is built on. The `gl33` backend is a rewrite, not a
migration.

### What each backend actually builds

The gate is in [gfx/CMakeLists.txt](../gfx/CMakeLists.txt), and it reaches
further than `gfx` itself:

| | `software` | `gl_legacy` |
|---|---|---|
| `gfx` | `spritesheet.cc` + `software/` | `spritesheet.cc` + the 2016 GL sources |
| `loaders` | `image.cc`, `sparrow.cc` | plus `obj.cc` |
| `skratch` demo | **not built** | built |
| `wreel-probe` | built | built |
| tests | built | built |

`loaders/obj.cc` is excluded under `software` because it populates
`gfx::ObjModel`, which holds `GLuint` buffer handles and is therefore inherently
OpenGL. `skratch` is excluded because
[skratch/application.cc](../skratch/application.cc) calls `glClear`, `glRotatef`
and `glTranslatef` directly. Decoupling model data from GPU buffers is part of
the `gles2` work; the configure step disables the demo automatically rather than
failing.

## Pinned dependencies

Fetched and built from source per target, so every target gets identical
library versions. Tags verified upstream.

| Dependency | Pin | Notes |
|---|---|---|
| SDL2 | `release-2.32.10` | SDL2, not SDL3 — SDL2 is what every handheld firmware ships. Static (`SDL2::SDL2-static`) |
| SDL2_image | `release-2.8.12` | **not** vendored — decodes PNG/JPEG via bundled `stb_image`, so needs no libpng/libjpeg anywhere |
| SDL2_ttf | `release-2.24.0` | vendored FreeType on **every** target; HarfBuzz off |
| nlohmann/json | `v3.12.0` | JSON config and data; replaces RapidJSON |
| doctest | `v2.5.3` | test framework; single header, no per-target build |

Three things about this that are easy to get wrong, and are settled here:

- **Everything links static.** `BUILD_SHARED_LIBS=OFF` is forced before the SDL
  satellites are populated. Left to their defaults they build shared and link
  `SDL2::SDL2` while we link `SDL2::SDL2-static`, which CMake rejects at generate
  time as a `COMPATIBLE_INTERFACE_BOOL` conflict on `SDL2_SHARED`. Static builds
  also rename the targets to `SDL2_ttf::SDL2_ttf-static` and
  `SDL2_image::SDL2_image-static`, so [Dependencies.cmake](../cmake/Dependencies.cmake)
  resolves the names rather than hard-coding them.
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

**What this unlocks.** [loaders/sparrow.cc](../loaders/sparrow.cc) is entirely
commented out, blocked on a `util/xml.hpp` that was never written, with
`data/jetpackdude.xml` and friends orphaned beside it. Converting those Sparrow
atlases to JSON revives that loader **without needing an XML parser at all**,
avoiding a second dependency decision.

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
the Miyoo Mini situation if you target stock firmware rather than KMSDRM. Prefer
the SDK's SDL2 there and let CMake find it in the sysroot.

## See also

- [DEVELOPMENT.md](DEVELOPMENT.md) — host setup, toolchain install, build commands
- [../README.md](../README.md) — project overview
