# Graphics backends: `gles2`, `gl33`, and retiring `gl_legacy`

**Status:** `superseded`
**Written:** 2026-07-25
**Superseded by:** [2026-07-26-gfx-renderer-and-gles2](../2026-07-26-gfx-renderer-and-gles2/)
**Blocked by:** [cxx17-modernization](../2026-07-25-cxx17-modernization/)

> **Superseded 2026-07-26.** The plan below — write `gles2`, then retire
> `gl_legacy` — treats a hand-written GLES2 backend as what the Mali handhelds
> need. Applying this document's *own* 2D-only decision to that ordering shows it
> is not: SDL_Renderer has a `GLES2_RenderDriver`, so the Mali devices get
> hardware-accelerated 2D from the existing `software` backend with no new
> rendering code. Accelerating the handhelds and writing GLES2 are separate jobs
> with separate consumers. Both are being done, in that order, and the mutually
> exclusive `WREEL_GFX_BACKEND` switch is retired because the two renderers
> coexist.
>
> Measured while working that out: `rk3326` and `h700` currently build SDL2 with
> GL, GLES and EGL all disabled, so both Mali devices are compiled as though they
> had no GPU. Recorded as D18.
>
> This document is kept for the structural analysis in *The structural problem*,
> which still holds, and for the 2D-only decision, which is unchanged.

## Where things stand

Two backends exist. `software` is complete enough to be the baseline;
`gl_legacy` is the 2016 fixed-function code, kept only so `skratch` keeps
running.

| Backend | State |
|---|---|
| `software` | implemented — window, renderer, blit, TTF text |
| `gl_legacy` | implemented (inherited), desktop only, to be deleted |
| `gles2` | not started |
| `gl33` | not started |

`rk3326` and `h700` currently build `software` even though both have Mali GPUs.
That is the gap this snapshot closes.

## The structural problem

`gl_legacy` is not portable forward, and it is not confined to `gfx/`. Two
couplings leak GL into modules that have no business knowing about it:

**`gfx::ObjModel` is a GPU object wearing a model's name.** It holds
`GLuint vertex_buffer`, `color_buffer` and `index_buffer` alongside its vertex
data, so `loaders/obj.cc` — a *text parser* — transitively depends on OpenGL.
That is why `loaders/CMakeLists.txt` has to exclude `obj.cc` under the `software`
backend, and why a software build currently cannot load a model at all.

**`skratch/application.cc` drives GL directly.** `glClear`, `glLoadIdentity`,
`glRotatef`, `glTranslatef` are called from application code, so the demo is
welded to the fixed-function pipeline and cannot follow any new backend.

Neither is fixable inside a backend. Both need the seam moved.

## Proposed shape

Split model data from GPU residency:

```
loaders::load_obj(path) -> gfx::Mesh        // plain data: vertices, colours,
                                            // indices. No GL. No SDL.
gfx::<backend>::MeshBuffer(const Mesh&)     // per-backend GPU residency
```

`gfx::Mesh` belongs in `include/gfx/types.hpp` next to the existing `Vertices` /
`Colors` / `Indexes` typedefs, which are already backend-neutral. Once that
exists, `loaders` drops its `gfx` dependency to a data-only one and `obj.cc`
builds on every target.

> **Still worth doing, for a narrower reason than originally written.** With the
> software backend now 2D only, nothing on a handheld will *render* an OBJ. The
> case for the seam is that `loaders/obj.cc` is a text parser and has no business
> including `SDL_opengl.h`, and that decoupling lets `test_obj.cc` run on every
> preset — including the cross builds under qemu — instead of only where a GL
> backend is configured. That is real but modest, so it is no longer urgent. If it
> is cut, `obj.cc` simply stays GL-gated and untested on software builds.

Then unify the context types. `gfx::Context` (gl_legacy) and
`gfx::software::Context` should converge on one interface. Follow the pattern the
codebase already uses twice — `util::File` over `posix`/`mswin`, selected at
compile time, no virtual dispatch:

```cpp
// include/gfx/context.hpp
#if defined(WREEL_GFX_BACKEND_SOFTWARE)
#include <gfx/software/context.hpp>
#elif defined(WREEL_GFX_BACKEND_GLES2)
#include <gfx/gles2/context.hpp>
#endif
```

The build already defines `WREEL_GFX_BACKEND_<NAME>` for exactly this.

## Why `gles2` before `gl33`

One shader pipeline can serve both. GLES 2.0 is a near-subset of desktop GL 2.1+,
and desktop Mesa exposes GLES 2 directly, so `gles2` runs on the Mali handhelds
*and* on a dev box. Writing `gl33` first would produce something that runs on
nothing you are targeting except Steam.

Practical consequence: shaders should be written to GLSL ES 1.00 (`#version 100`)
with the precision qualifiers, and desktop builds should request a GLES context
rather than assuming core profile.

## Tasks

- [x] Decide whether the software backend needs 3D — **no**, see above
- [ ] Unify `Context` behind a compile-time-selected header
- [ ] `gles2` backend: context creation, shader loading, `MeshBuffer`, text
- [ ] Port `skratch` off direct GL calls onto the unified interface
- [ ] Add `gles2` to `WREEL_GFX_BACKEND_VALUES`; point `rk3326`/`h700` at it
- [ ] `gl33` only if Steam actually needs something `gles2` cannot give
- [ ] Delete `gl_legacy`, `gfx/context.cc`, `gfx/obj.cc`, `gfx/utils.cc`, GLEW/GLU

Optional, and no longer on the critical path:

- [ ] Introduce `gfx::Mesh` as backend-neutral model data
- [ ] Rewrite `loaders::load_obj_model` to produce `gfx::Mesh`; drop the GL dep
- [ ] Build `loaders/obj.cc` on all backends in `loaders/CMakeLists.txt`
- [ ] Add `test_obj.cc` against `data/cube.obj`, `ico.obj`, `teapot.obj`

~~Software `MeshBuffer` — a CPU rasteriser, or triangle-free 2D fallback~~ —
dropped by the 2D-only decision.

## Decided: the software backend is 2D only

**Settled 2026-07-25.** `software` exposes sprites, tiling and text. No CPU
triangle rasteriser.

A basic handheld game here will be 2D, or mocked/basic 3D with bespoke rendering.
Software-rendered 3D is a much larger lift and is deferred to its own snapshot —
see [software-3d-rasteriser](../2026-07-25-software-3d-rasteriser/). A CPU
rasteriser on two Cortex-A7 cores inside 128 MB is a project in its own right, and
committing to it would have set the scope of this one an order of magnitude higher.

Consequences, which are what shrink this snapshot:

- **No software `MeshBuffer`.** Dropped from the task list entirely.
- **`skratch`'s spinning-model demo stays desktop-only.** It is a GL demo; it does
  not need to follow the software backend.
- **`loaders/obj.cc` stays gated to the GL backends** as far as *rendering* goes.
  The `gfx::Mesh` seam below is still worth doing, but for a different reason than
  originally written — see the note there.
- The real content of the software backend becomes sprite and tile rendering,
  which is scoped separately in
  [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

## Open questions
- Is `glm` worth taking as a dependency for matrix maths, versus extending
  `include/math/vector.hpp` (see defect D7)? `libglm-dev` is header-only and
  already in the bootstrap script's `math` group.
- Do the Mali handhelds want GLES 2.0 or 3.x? Mali-G31 supports GLES 3.2, but
  vendor blobs on older firmware sometimes only expose 2.0 reliably.
  `wreel-probe` on real hardware answers this — see
  [target-validation](../2026-07-25-target-validation/).

## References

- [docs/TARGETS.md § Graphics backends](../../docs/TARGETS.md)
- `gfx/CMakeLists.txt` — the existing gate
- `include/gfx/software/` — the pattern a `gles2` backend should mirror
