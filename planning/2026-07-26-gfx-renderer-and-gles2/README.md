# Two renderers: `gfx::renderer` everywhere, `gfx::gles2` where there is a GPU

**Status:** `in-progress`
**Written:** 2026-07-26
**Supersedes:** [2026-07-25-graphics-backends](../2026-07-25-graphics-backends/)
**Blocks:** nothing. Unblocks `WREEL_WERROR`

## Why this supersedes the earlier snapshot

[graphics-backends](../2026-07-25-graphics-backends/) planned one thing: write a
`gles2` backend, then retire `gl_legacy`. That ordering was written *before* the
software backend was settled as
[2D only](../2026-07-25-graphics-backends/README.md#decided-the-software-backend-is-2d-only),
and it was never re-examined afterwards. Applying the 2D decision to it changes
the answer.

The old snapshot assumed a hand-written GLES2 backend was what the Mali handhelds
needed. It is not. Once the GPU targets only have to draw sprites, atlases,
tilemaps and text, **SDL_Renderer already implements all of it** — and the pinned
SDL 2.32.10 ships a `GLES2_RenderDriver` (`src/render/opengles2/`, listed in
`render_drivers[]` behind `SDL_VIDEO_RENDER_OGL_ES2`). Pointing `rk3326` and
`h700` at that driver makes the *existing* `gfx::software::Context` hardware
accelerated on Mali, from the same source that CPU-blits on the Miyoo Mini, with
no new rendering code at all.

So the two things are decoupled. Accelerating the handhelds is a build-and-driver
question. Writing GLES2 is about **shaders and 3D**, which is a different job with
different consumers — and both are wanted, so both are being done.

## What was measured first

### Every preset but one compiles SDL with no GPU support whatsoever

Read out of each preset's generated `SDL_config.h`, 2026-07-26:

| Preset | `OPENGL` | `OPENGL_ES2` | `OPENGL_EGL` | `RENDER_OGL` | `RENDER_OGL_ES2` |
|---|---|---|---|---|---|
| `desktop-software` | off | off | off | off | off |
| `desktop-debug` | **on** | **on** | **on** | **on** | **on** |
| `rk3326` | off | off | off | off | off |
| `h700` | off | off | off | off | off |
| `miyoomini` | off | off | off | off | off |

`miyoomini` and `desktop-software` are correct — the SSD202D has no 3D block, and
the software preset asks for none. **`rk3326` and `h700` are wrong.** Both have
Mali GPUs and both are currently built as though they did not, so even the
accelerated *render driver* is compiled out. Nothing detects this; the build
succeeds and silently produces a CPU-blitting binary for a device with a GPU.

The cause is one line — [`cmake/toolchains/aarch64-handheld.cmake:29`](../../cmake/toolchains/aarch64-handheld.cmake):

```cmake
# Mali GPUs: GLES 2.0 / 3.x is available. The gles2 backend is not written yet,
# so these presets currently build with the software backend; the flag records
# device capability, not backend readiness.
set(WREEL_TARGET_HAS_GPU OFF)
```

The comment states the intent exactly and then the code does the opposite of it.
`WREEL_TARGET_HAS_GPU` is consumed by
[`cmake/Dependencies.cmake`](../../cmake/Dependencies.cmake) to decide whether
SDL2 gets GL/GLES/EGL at all, so setting it from *backend readiness* rather than
*device capability* propagates into the dependency build. This is the first thing
to fix, and it is a one-line change with a table-shaped consequence.

Recorded as **D18**.

### The cross sysroot has no GLES headers

`libgles-dev` and `libegl-dev` are installed for the host, so
`/usr/include/GLES2/gl2.h` and `/usr/include/EGL/egl.h` exist and desktop GLES2
work is possible today. The aarch64 sysroot has neither, and `arm64` is not an
enabled foreign architecture:

```
$ dpkg --print-foreign-architectures      # (nothing)
$ ls /usr/aarch64-linux-gnu/include/GLES2/
ls: cannot access ...: No such file or directory
```

So flipping `WREEL_TARGET_HAS_GPU ON` for the aarch64 presets is necessary but
not sufficient: SDL's configure will still find no EGL and disable it. Fixing it
needs `dpkg --add-architecture arm64` plus `libgles-dev:arm64 libegl-dev:arm64`,
or a device SDK sysroot that carries them. That is a
[bootstrap-debian.sh](../../scripts/bootstrap-debian.sh) change, and it belongs
with this work rather than with `target-validation`.

Worth being clear about what this does and does not prove: headers let the build
*compile* GLES support. Whether the vendor Mali blobs on a given firmware
actually expose a working GLES2 context is a hardware question that only
[target-validation](../2026-07-25-target-validation/) step 4 answers.

## Decisions

Settled 2026-07-26, before implementation.

### 1. The renderers coexist. `WREEL_GFX_BACKEND` stops being a switch

This is the structural change, and it is the part the old snapshot got wrong by
assumption rather than by measurement.

`WREEL_GFX_BACKEND=software|gl_legacy` modelled two **mutually exclusive
implementations of one interface**, selected at compile time — the same pattern
`util::File` uses over `posix`/`mswin`. That was the right model when the two were
alternative ways to put pixels on the same screen.

It is the wrong model now, because the two surviving renderers are not
alternatives:

| | `gfx::renderer` | `gfx::gles2` |
|---|---|---|
| Built on | `SDL_Renderer` | an SDL GL context we own |
| Available on | every target | targets with a GPU |
| Draws | textures, atlases, tilemaps, text | anything a shader can express |
| For | the game path | shader effects, 3D, the modern-GL showcase |
| Miyoo Mini | yes, software driver | never |

A build wants **both** compiled, with each executable choosing. So the switch
becomes a capability flag:

- `gfx::renderer` is unconditional. There is nothing to gate; it works everywhere.
- `WREEL_ENABLE_GLES2` defaults to `WREEL_TARGET_HAS_GPU` and compiles
  `gfx/gles2/`. Off on `miyoomini` always.

`WREEL_GFX_BACKEND` is retired rather than redefined. Keeping the name for a
concept it no longer describes is how the `__GFX_VIEW_HPP__`-in-`context.hpp`
kind of confusion starts.

**One window, one renderer.** SDL_Renderer owns the window's GL context
internally. Mixing our GL calls into a window it manages is possible in SDL
≥ 2.0.10 via `SDL_RenderFlush` and full state restoration, and it is not worth
it. A window is driven by one or the other, chosen at construction.

### 2. `gfx::software` is renamed `gfx::renderer`

`software` named the *driver* it happened to get on the weakest device, not the
abstraction. The moment `rk3326` selects `opengles2` the name is actively
misleading — the same code, hardware accelerated, in a namespace that says it
isn't. `gfx::renderer::Context` says what it is: a context over `SDL_Renderer`,
whichever driver that resolves to. `driver_name()` already reports the actual
choice, and that reporting becomes load-bearing rather than informational.

It also leaves `gfx::` free of a "default" renderer, which matters now there are
two: `gfx::renderer::Context` and `gfx::gles2::Context` are symmetric, and neither
is silently the one you get.

### 3. Driver selection is explicit, and reported

`gfx::renderer::Context` takes a driver preference and honours it:

- `Accelerated` — require a hardware driver, fail if unavailable
- `Prefer accelerated` — hardware if present, software otherwise. The default
- `Software` — force the software driver, which is also what Miyoo Mini gets

"Prefer" rather than "require" as the default because a firmware whose GLES blobs
are broken should still boot into a playable game, which is the same reasoning
`audio::Device` uses for a missing audio device — see
[docs/TARGETS.md § No device is not an error](../../docs/TARGETS.md).

### 4. `skratch` is kept, and becomes the modern-GL reference

Not retired. Ported onto `gfx::gles2` and kept deliberately as the worked example
of how GL is structured *now* versus how it was structured in 2016, since the tree
happens to contain both and the contrast is the useful part:

| 2016 `gl_legacy` | ported `gles2` |
|---|---|
| `glRotatef` / `glTranslatef` on a driver-side matrix stack | matrices built explicitly, uploaded as a uniform |
| `glBegin`-era immediate mode and fixed-function pipeline | vertex + fragment shaders, GLSL ES 1.00 |
| `gluPerspective` from GLU | projection built in code |
| `GLuint` handles inside `gfx::ObjModel` | `gfx::Mesh` data, `gles2::MeshBuffer` residency |
| state set globally, implicitly | state owned by an object with a lifetime |

That is what `skratch` is *for* now. It is not the game, and the game does not
depend on it — the game path is `gfx::renderer` and its demo is the separate
`sprites` executable in
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

### 5. `gl_legacy` goes once `skratch` is ported

`gfx/context.cc`, `gfx/system.cc`, `gfx/obj.cc`, `gfx/utils.cc`,
`include/gfx/utils.hpp`, and the GLEW and GLU dependencies. That is the 11
deleted-file warnings and, with the `skratch` rewrite, the other 22 — which is
what finally makes `WREEL_WERROR=ON` reachable tree-wide. See
[cxx17-modernization § the remaining 33](../2026-07-25-cxx17-modernization/README.md).

`gfx::System`'s leaked singleton (D8) is deleted with it rather than fixed, as
that snapshot already decided.

## Open question, and it gates the `gles2` API shape

**Matrix maths: glm, or extend `include/math/vector.hpp`?**

The earlier snapshot lists this as open and it now has to be answered, because it
decides whether `MeshBuffer::draw()` takes a `glm::mat4` or a `math::Matrix4`, and
whether `gfx::Mesh` holds `glm::vec3` or `math::Vector3`.

Recommendation: **take glm, pin `1.0.3`, and retire `math::Vector3`.**

- `math::Vector3::operator+` and `operator*` mutate their left operand and return
  a reference (D7), so `a + b` modifies `a`. They are not exercised today, which
  means the type has to be fixed *or* replaced before anything uses it seriously.
  Replacing it deletes the defect instead of repairing it.
- The header has no `Matrix4`, no `dot`, `cross`, `length` or `normalize`. A
  modern GL path needs all of them, so the choice is write them or take them.
- glm is header-only and already in the bootstrap script's `math` group. Latest
  tag is **1.0.3**, verified upstream — note Debian 12 ships `0.9.9.8`, so this
  gets pinned via FetchContent like every other dependency rather than taken from
  the system.
- Cost, stated plainly: it is a fifth pinned dependency, it appears in
  `gfx::Mesh`'s definition and therefore in `loaders::load_obj`'s output, and
  `-Wdouble-promotion`-clean use of it needs care. It is also a vendor type in a
  module signature, which
  [docs/TARGETS.md § Wrap it, don't spread it](../../docs/TARGETS.md) argues
  against — the counter-argument being that a maths vector is a value type, not a
  subsystem, and wrapping `vec3` buys nothing but indirection.

The alternative — extend `math::Vector3` with `Matrix4` and the missing operations
— keeps the dependency count and the "no vendor types in signatures" rule intact,
and costs perhaps 200 lines of maths plus the tests to trust it.

## Tasks

Ordered so the tree builds and tests pass at every step, and so the cheap
correctness fix lands before the large feature.

**Stage 1 — fix what is measurably wrong, no new rendering code**

- [ ] D18: `WREEL_TARGET_HAS_GPU` records device capability. `ON` for the aarch64
      handhelds
- [ ] `libgles-dev:arm64` / `libegl-dev:arm64` in `bootstrap-debian.sh`, plus the
      `dpkg --add-architecture arm64` step
- [ ] Confirm from the generated `SDL_config.h` that `rk3326`/`h700` pick up
      `RENDER_OGL_ES2`, and that `miyoomini` still does not
- [ ] Rename `gfx::software` → `gfx::renderer`; `gfx/software/` → `gfx/renderer/`
- [ ] Driver preference on `renderer::Context`, with `driver_name()` asserted in a
      test rather than logged and forgotten
- [ ] Retire `WREEL_GFX_BACKEND`; add `WREEL_ENABLE_GLES2`. Update the seven
      presets, the configure guards and `wreel-probe`

**Stage 2 — the data seam, which `gles2` needs and `loaders` wants anyway**

- [ ] Decide the maths question above
- [ ] `gfx::Mesh` — backend-neutral vertex/colour/index data, no GL
- [ ] `loaders::load_obj` produces a `gfx::Mesh`; `loaders/obj.cc` builds on every
      target and stops including `SDL_opengl.h`
- [ ] `tests/test_obj.cc` — `cube.obj`, `ico.obj`, `teapot.obj`, pinning the
      counts already recorded in `docs/DEVELOPMENT.md` (ico 42/240, teapot
      3644/18960)

**Stage 3 — the GLES2 renderer**

- [ ] `gfx::gles2::Context` — GL context creation, `SDL_GL_SetAttribute` with
      `CONTEXT_PROFILE_ES`, version 2.0, swap
- [ ] `gfx::gles2::Shader` / `Program` — compile, link, uniforms, and *report* the
      info log on failure rather than silently producing a black screen
- [ ] `gfx::gles2::MeshBuffer` — VBO residency for a `gfx::Mesh`
- [ ] `gfx::gles2::Texture` + textured-quad draw, so text and sprites work
- [ ] Text: SDL_ttf surface → texture → quad

**Stage 4 — port `skratch`, then delete `gl_legacy`**

- [ ] `skratch` on `gles2`: explicit projection and model-view matrices, shaders,
      `MeshBuffer`. No `gl*` calls in application code
- [ ] `skratch/main.cc` off its own `ofstream logging` onto `util::log_*`, and
      `runlog.txt` to `SDL_GetPrefPath()` — it is a shipped executable including
      `<fstream>`, against
      [docs/TARGETS.md § 1a](../../docs/TARGETS.md)
- [ ] Delete `gl_legacy`: `gfx/context.cc`, `gfx/system.cc`, `gfx/obj.cc`,
      `gfx/utils.cc`, `include/gfx/utils.hpp`, GLEW and GLU
- [ ] Flip `WREEL_WERROR` to `ON` and clear whatever the flip reveals
- [ ] `docs/DEVELOPMENT.md` § Status, and the `WREEL_WERROR` row

## Risks

**The Mali handhelds have never run this code, and now the build will assume more
about them.** Stage 1 makes `rk3326`/`h700` request an accelerated driver. If a
firmware's GLES blobs are missing or broken, `Prefer accelerated` degrades to
software and the game still runs — which is why that is the default rather than
`Accelerated`. Verifying the good path still needs hardware:
[target-validation](../2026-07-25-target-validation/) step 4.

**A GLES2 context cannot be tested headlessly.** `SDL_VIDEODRIVER=dummy` gives no
GL, so `gfx::gles2` cannot have a test that draws. What *can* be tested without a
display: shader compilation diagnostics as string handling, `gfx::Mesh` and the
OBJ loader as data, and matrix maths. Those are where the bugs actually hide, and
they are the parts to cover — but "the tests pass" will not mean "it renders", and
the status table must not imply otherwise.

**Deleting `gl_legacy` deletes the only code proven to link against a real GPU
stack.** `desktop-debug` builds, links and reports Mesa 22.3.6 today. Between the
deletion and a working `gles2`, nothing in the tree exercises a GL driver, so
stage 4's order matters: port `skratch` and see it run *before* the delete
commit, not after.

## References

- [2026-07-25-graphics-backends](../2026-07-25-graphics-backends/) — superseded by
  this document; still the record of why `gl_legacy` is not portable forward
- [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) — the
  game path, which is `gfx::renderer`'s consumer
- [midi-live-visuals](../2026-07-25-midi-live-visuals/) — the other `gles2`
  consumer, and the reason shaders are wanted at all
- [cxx17-modernization](../2026-07-25-cxx17-modernization/) — the 33 warnings this
  clears, and D7/D8/D18
- `src/render/opengles2/` in the pinned SDL2 tree — what `gfx::renderer` gets for
  free on Mali
