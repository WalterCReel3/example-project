# Software 2D: sprites, atlases, tiling, entities

**Status:** `in-progress`
**Written:** 2026-07-25
**Blocked by:** nothing

> **Started 2026-07-26 with the XML dependency**, which is the first task below
> and the one `loaders/sparrow.cc` has been blocked on since 2016. pugixml is
> pinned, `util::xml` exists with 19 cases against the real Sparrow atlas in
> `data/`, and the strict number conversion it needed became
> `include/util/number.hpp`. Nothing about the rendering path has moved yet —
> `gfx::software::Texture` is still the piece everything waits on.

## Motivation

This is where the handheld work actually goes. With the software backend settled
as [2D only](../2026-07-25-graphics-backends/README.md), the content of that
backend is sprite and tile rendering — and right now it cannot draw a sprite at
all.

A basic handheld game here will be some form of 2D, or mocked/basic 3D done with
bespoke rendering. Neither needs a triangle pipeline; both need atlases, animated
sprites, a tilemap, and something to hold entity state.

## What exists today

Surveyed 2026-07-25, because the gap is smaller than it looks in some places and
larger in others.

| Piece | State |
|---|---|
| `gfx::software::Context` | window, renderer, `clear`, `present`, `draw_surface`, `draw_text` |
| `gfx::Spritesheet` / `SpritesheetFrame` | backend-neutral frame list; `SpritesheetFrame` is a plain aggregate. **No consumers** |
| `loaders::load_image` | `IMG_Load` plus a convert to `ABGR8888`, returns `SDL_Surface*` |
| `util::xml` | **done 2026-07-26** — pugixml `v1.16` behind `include/util/xml.hpp`; `test_xml`, 19 cases / 116 assertions |
| `loaders/sparrow.cc` | **entirely commented out** — every line. No longer blocked: `util/xml.hpp` now exists |
| Tilemap | nothing. No code, no assets, no format |
| Entities | nothing |

Assets already in `data/`: `cavernes.png`, `darknes.png`, `jetpackdude.xml`
(a Sparrow atlas, 8 frames of `reddude.NNN` at 24×34), `test2.xml`. Note that
`jetpackdude.xml` names `imagePath="JetPackDude.png"`, **which is not in the
repository** — the atlas is orphaned from its sheet.

### The two real gaps

**`Context` cannot blit part of a texture.** `draw_surface` takes a whole
`SDL_Surface` and blits it at a point. Atlas and tile rendering need a source
rectangle — `SDL_RenderCopy` with both src and dst. There is no way to express
that through the current interface.

**`draw_surface` creates and destroys an `SDL_Texture` per call.** Acceptable for
the occasional HUD string it was written for; ruinous for a tilemap, which would
upload every tile every frame. A sprite path needs a texture that outlives the
frame, which means an owning texture type that does not exist yet.

## Decisions already made

Settled 2026-07-25, before implementation.

- **XML is supported natively; JSON support is unaffected.** Sparrow atlases and
  Tiled TMX are read as authored. pugixml `v1.16` is pinned for it, behind a
  `util::xml` facade. Full reasoning and the measurements in
  [docs/TARGETS.md § XML: why pugixml](../../docs/TARGETS.md).
- **Tilemaps are Tiled TMX.** Tiled is the de-facto editor for this kind of game,
  so maps get a real editor rather than hand-authored arrays. **Layer data must be
  saved as CSV** — base64+zlib would pull in a zlib dependency the project does
  not currently expose.
- **Scope is rendering plus a minimal entity store.** Texture, atlas, animated
  sprite, tilemap rendering, and a flat entity store with position, sprite
  reference and an update tick — enough to drive several animated things
  independently. *Not* in scope: collision, camera, scene graph, physics, input
  mapping. Those are a game layer and belong in their own snapshot.

## Proposed shape

```
util::xml                        facade over pugixml; no pugi:: in signatures

gfx::software::Texture           owns an SDL_Texture, created once
Context::draw(const Texture&, const Rect* src, const Rect* dst)

gfx::Atlas                       named frames over one Texture
                                 (gfx::Spritesheet, given a Texture and an owner)
loaders::load_sparrow(path)      -> gfx::Atlas          (revives sparrow.cc)

gfx::AnimatedSprite              frame sequence + timing over an Atlas
gfx::TileMap                     tilesets, tile layers, object layers
loaders::load_tmx(path)          -> gfx::TileMap

game::Entities                   flat store: position, sprite ref, update tick
```

`Texture` is the piece everything else waits on, because it is what makes a
source-rect blit expressible and what stops the per-call upload.

## Tasks

Ordered so that something is visible on screen as early as possible.

- [x] Pin pugixml in `cmake/Dependencies.cmake`; add `util::xml` facade + tests.
      Done 2026-07-26. Two things worth knowing before the loaders are written:
      pugixml's `as_int()` does not honour its documented default-on-failure
      contract, so the facade converts through the new `util::from_string`
      instead and offers `require_attribute_*` forms that throw (D17); and
      `Node` is a non-owning view, so it dangles once its `Document` goes —
      `load_sparrow` must not return `Node`s, which it was never going to
- [ ] `gfx::software::Texture` — owning, created once, movable not copyable
- [ ] `Context::draw()` taking source and destination rects
- [ ] Retire or re-express `draw_surface` in terms of `Texture` so the per-call
      upload has exactly one remaining caller (`draw_text`) rather than two
- [ ] `gfx::Atlas` over `Texture`, reusing `SpritesheetFrame`
- [ ] `loaders::load_sparrow` — uncomment and rewrite against `util::xml`;
      `test_sparrow.cc` against `data/jetpackdude.xml`
- [ ] Source a sheet for `jetpackdude.xml`, or replace the atlas fixture with one
      whose image is actually present
- [ ] `gfx::AnimatedSprite` — frame sequence, frame duration, looping
- [ ] Frame timing: the demo loop currently `SDL_Delay(10)`s unconditionally,
      which is neither a frame cap nor vsync. Shared with
      [midi-live-visuals](../2026-07-25-midi-live-visuals/)
- [ ] `gfx::TileMap` + `loaders::load_tmx`, CSV layer data, external `.tsx`
- [ ] A tilemap fixture in `data/` authored in Tiled
- [ ] `game::Entities` — flat store, update tick, draw ordering
- [ ] A `sprites` demo executable, separate from `skratch`

## Risks

**The software renderer's fill rate is unmeasured on real hardware.** Everything
here is affordable on a dev box and nothing about that is evidence. A full-screen
tilemap at 320×240 on two Cortex-A7 cores through `SDL_Renderer`'s software path
has never been timed, and the whole design assumes it is fast enough. This is the
same gap [target-validation](../2026-07-25-target-validation/) has been carrying:
no part of this codebase has run on a device. Measure before building the tilemap
on the assumption that per-tile blitting is viable; dirty-rectangle or
tile-caching strategies are much cheaper to adopt early than to retrofit.

**`SDL_Renderer` may not be the right layer.** It gives texture blitting with
scaling and rotation for free, which is most of a 2D engine. But the classic
demoscene effects in [midi-live-visuals](../2026-07-25-midi-live-visuals/) —
plasma, rotozoom, feedback — want direct pixel access, which means
`SDL_LockTexture` or a surface the CPU plots into. The two paths coexist, but
whether `Context` should expose a locked-pixels view is a real design question and
it is better answered before the API sets.

## Open questions

- Does `gfx::Spritesheet` survive as the atlas type, or become an implementation
  detail of `gfx::Atlas`? It currently holds a non-owning `SDL_Surface*` and has
  no consumers, so there is nothing to preserve except the shape.
- How are assets addressed? Paths are relative today, which
  [packaging-distribution](../2026-07-25-packaging-distribution/) has already
  flagged as broken the moment a launcher changes directory. `SDL_GetBasePath()`
  is the fix and this is the work that will first care.
- Where does the entity store live — a new `game/` module, or inside the demo
  executable until a second consumer exists? Leaning toward the latter, since a
  module boundary drawn before there are two users tends to be drawn wrong.

## References

- [docs/TARGETS.md § XML: why pugixml](../../docs/TARGETS.md)
- [graphics-backends § Decided: the software backend is 2D only](../2026-07-25-graphics-backends/README.md)
- [software-3d-rasteriser](../2026-07-25-software-3d-rasteriser/) — what was deferred
- `include/gfx/software/context.hpp`, `include/gfx/spritesheet.hpp`,
  `loaders/sparrow.cc`
