# Graphics backends: `gles2`, `gl33`, and retiring `gl_legacy`

**Status:** `snapshot`
**Written:** 2026-07-25
**Blocked by:** [cxx17-modernization](../2026-07-25-cxx17-modernization/)

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

- [ ] Introduce `gfx::Mesh` as backend-neutral model data
- [ ] Rewrite `loaders::load_obj_model` to produce `gfx::Mesh`; drop the GL dep
- [ ] Re-enable `loaders/obj.cc` on all backends in `loaders/CMakeLists.txt`
- [ ] Add `test_obj.cc` against `data/cube.obj`, `ico.obj`, `teapot.obj`
- [ ] Unify `Context` behind a compile-time-selected header
- [ ] Software `MeshBuffer` — a CPU rasteriser, or triangle-free 2D fallback
- [ ] `gles2` backend: context creation, shader loading, `MeshBuffer`, text
- [ ] Port `skratch` off direct GL calls onto the unified interface
- [ ] Add `gles2` to `WREEL_GFX_BACKEND_VALUES`; point `rk3326`/`h700` at it
- [ ] `gl33` only if Steam actually needs something `gles2` cannot give
- [ ] Delete `gl_legacy`, `gfx/context.cc`, `gfx/obj.cc`, `gfx/utils.cc`, GLEW/GLU

## Open questions

- **Does the software backend need 3D at all?** A CPU triangle rasteriser on two
  Cortex-A7 cores at 128 MB is a real project. If the handheld target is 2D
  sprite games — which is what these devices are actually used for — then
  `software` should expose sprites and text only, `gfx::Mesh` stays a GL-side
  concern, and `skratch`'s spinning-model demo simply stays desktop-only. **This
  decision should be made before any rasteriser work starts**, because it changes
  the scope by an order of magnitude.
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
