# A game layer, and a small game to define it

**Status:** `snapshot`
**Written:** 2026-08-10
**Blocked by:** nothing. Device verification runs in parallel and is
[target-validation](../2026-07-25-target-validation/)'s

> This is the successor
> [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) named on
> 2026-07-25, when it scoped itself to "rendering plus a minimal entity store" and
> put the rest behind a line: *"Not in scope: collision, camera, scene graph,
> physics, input mapping. Those are a game layer and belong in their own
> snapshot."* This is that snapshot.

## Motivation

The 2D module is complete and every piece of it has been exercised by a test or a
demo. What none of it has been exercised by is **a game**, and that is a different
kind of pressure: a test asserts that a component does what its author said, and a
game finds out whether the components fit together.

Three things already recorded are waiting on exactly this, and none of them can be
settled by writing another test:

- **`sprites::Entities` is waiting for a second consumer to define it.**
  `entities.hpp` says promotion out of the demo happens "when a second consumer
  turns up and says what the interface actually needs to be — not before, on a
  guess about what that will be." There has been one consumer since it was
  written. A game is the second, and the useful part is that it will *disagree*
  with the first.
- **The fill-rate work has a lever and nothing pulling it.**
  [results.md](../2026-07-25-target-validation/results.md) killed the tile-cache
  idea — 1295 small blits beat 315 large ones by 2.6× — and redirected effort at
  **overdraw** and at **drawing 1:1**. `data/sunnyland.tmx` is deliberately the
  worst case: a backdrop layer covering every cell, built to make the meter move.
  A real level is where "remove a layer, remove pixels" gets tested on content
  rather than on a stress fixture.
- **`rig::Pad`'s raw-joystick path is a documented guess.** It logs that it is
  one. Nothing has yet needed a button to mean something at a moment that
  matters, which is when a wrong mapping stops being a log line.

## What exists today

Surveyed 2026-08-10 by reading the headers, not from recollection of them.

| Piece | State | What the game layer still needs from it |
|---|---|---|
| `gfx::Atlas` / `AtlasFrame` | done, trim-aware, `test_atlas` 10 cases / 64 assertions | nothing |
| `gfx::Animation` / `AnimatedSprite` | done, shared definition + per-entity state, 27 / 230 | nothing |
| `gfx::TileSet` / `TileMap` | done, arithmetic source rects, `draw()` culls and honours `visible` | nothing |
| `loaders::load_tmx` | done, CSV layers, external `.tsx`, object layers | **properties — see below** |
| `gfx::MapObject` | carries `name`, `type`, `id`, `x`, `y`, `width`, `height` | nothing; `type` is enough to spawn from |
| `sprites::Entities` | done, generational handles, `Entity` has position, velocity, sprite, scale, layer | **a collision box and per-entity behaviour** |
| `rig::Pad` | done, named `Button`s, three input paths, `down()` and `pressed()` | an action layer above it |
| `rig::FrameClock` / `FrameTiming` | done, clamped delta | a fixed-step mode for reproducible screenshots |
| `tools/pack_atlas.py` | done, trims, verifies by rebuilding each frame, seeds an animation sidecar | nothing |
| `tools/make_tilemap.py` | done, writes `.tsx` + `.tmx`, three tile layers and one object layer | a level rather than a stress fixture |

### The one real gap

**`loaders::load_tmx` reads no properties.** Not tile properties in the `.tsx`, not
object properties in the `.tmx` — `grep -n propert loaders/tmx.cc
include/loaders/tmx.hpp include/gfx/tilemap.hpp` returns nothing at all. `TileLayer`
carries `name`, `width`, `height`, `tiles`, `visible` and an offset; `MapObject`
carries `name`, `type`, `id` and a rect.

This is not a defect — nothing has needed them — but it decides how collision gets
authored, and it is cheaper to decide than to discover. See the proposal below.

## The game

**Foxy forages in an orchard. Three animals wander it. Touching one costs you
your last pickup and sits you down dizzy for a moment. Collect everything and the
level is done.**

Avoidance, not combat, as asked. Nothing in the game kills, is killed, or is
struck; there is no health, no weapon, no score multiplier, and no fail state
beyond losing progress you can re-earn. The animals are not hostile — they follow
fixed paths and ignore the player entirely, which is *why* it works as avoidance:
routing around a patrol is a spatial puzzle, whereas an animal that chases you is
a fight with the aggression filed off.

This is also the cheapest thing that stresses every part of the layer. Collision
against tiles and against entities, a camera that follows and clamps, an input
mapping that has to feel right, a state that persists across a frame — and no
scenes, no menus, no save, no dialogue.

### Naming, and why it is worth a paragraph

The art pack calls two of its animations things this game is not.
`Misc/Sunnyland FX/Sprites/enemy-death/` (6 frames, 32×32) is a generic dust puff,
and `Characters/eagle/Sprites/attack/` (4 frames, 40×41) is a wing-down glide.
`tools/pack_atlas.py` takes frame names from directory names, so whatever those
directories are called at pack time is what lands in `data/*.xml` and gets quoted
in every call site and test that follows.

So they get renamed on ingest — `puff` and `swoop` — and the rename happens in the
copy into `data/sunny-land/`, not by patching the atlas afterward. Renaming a
frame later means touching the atlas, the sidecar, the code and the tests at once.

The demo executable is proposed as **`orchard`**, one word, matching `skratch`,
`coppers` and `sprites`. Open to a different one; it is cheap now and annoying
after `orchard/` exists in five CMake files.

### The art, measured

Counted and dimensioned on 2026-08-10 from
`data/Sunny-land-files/Assets/`, which is already in the tree.

| Source | Frames | Authored size | Role |
|---|---|---|---|
| `Characters/Foxy/*` | 36 across 12 animations | 33×32 | the player, already packed as `data/foxy.*` |
| `Characters/frog/Sprites/{idle,jump}` | 4 + 2 | 35×32 | a hopper; vertical timing |
| `Characters/Opossum/opossum/` | 6 | 36×28 | a ground patroller |
| `Characters/eagle/Sprites/attack` → `swoop` | 4 | 40×41 | an air patroller, ignores terrain |
| `Misc/Sunnyland items/Sprites/cherry` | 7 | 21×21 | the common pickup |
| `Misc/Sunnyland items/Sprites/gem` | 5 | 15×13 | the rare pickup |
| `Misc/Sunnyland FX/Sprites/item-feedback` | 4 | 40×41 | pickup sparkle |
| `Misc/Sunnyland FX/Sprites/enemy-death` → `puff` | 6 | 32×32 | the bump reaction |

38 new frames. All CC0 1.0 under the same Ansimuz grant `data/PROVENANCE.md`
already records for Foxy, so this adds art without adding a licence question —
which is not true of `glyphs-16x16.png` and is the reason this pack keeps earning
its place.

Foxy already has what the game needs and no more: `idle`, `run`, `jump`,
`crouch`, `dizzy`, `victory`. `climb`, `roll`, `wall-grab`, `lookup`, `hurt` and
`hurt2` go unused, and that is fine — they cost 18 frames of sheet and nothing
else.

**The bump reaction is `dizzy`, not `hurt`.** Both exist; `hurt` is named for
something this game does not have, and at 2 frames playing once it is a flinch.
`dizzy` is 6 frames on a repeat loop, which reads as sat-down-seeing-stars and —
the practical half — means the **recovery window sets the duration rather than
the art doing it**. A once-through animation would have to be re-timed every time
the recovery period was tuned.

## Decisions

Settled 2026-08-10, before any code is written. Decisions 6 and 7 were open
questions in the first draft of this document and were answered the same day;
what is left genuinely open is at the bottom.

### 1. A second atlas, not a bigger one

`critters.{png,xml}` holds the frog, opossum, eagle, cherry, gem, puff and
sparkle. `foxy.{png,xml}` is left exactly as it is.

The tempting move is one combined sheet — one texture, one upload, one bind — and
it would fit: 74 frames of untrimmed area totals ~74,000 px, so a packed sheet
lands near 280×270, comfortably inside the Miyoo Mini's 640×480 texture cap.
(Arithmetic on measured frame sizes, not a pack; the real number comes from
running the packer.)

It is still the wrong trade. `data/foxy.xml` is the fixture for `test_sparrow`
(18 cases / 189 assertions) and half of `test_atlas` (10 / 64), and repacking it
churns both to save one texture bind per frame — against a renderer measured at
3.5 ns per pixel, where binds were shown not to be what is billed for. Two
atlases cost nothing here.

### 2. Collision comes from a tile layer named `collision`, not from tile properties

Two ways to author solid tiles in Tiled: a `collidable` property on each tile in
the `.tsx`, or a dedicated tile layer whose non-empty cells are solid.

The layer wins on three counts. It needs **no loader change** — `TileMap::find()`
already resolves a layer by name and `TileLayer::at()` already returns
`empty_tile` out of bounds. It costs **no fill rate**, because `draw_layer()`
returns early on `!layer.visible` (`gfx/tilemap.cc:70`) and the layer ships
`visible="false"`. And it is **visible in the editor**, which is where the person
authoring a level wants to see it.

The property route also drags in a typed-value parser — Tiled properties are
`string|int|float|bool|color|file` — for one boolean.

This decision is reversible and does not preclude adding property support later
if something genuinely needs per-tile data. It just should not be paid for now.

### 3. Spawns come from the object layer's `type`, which already works

`MapObject::type` is parsed today. A `spawns` object layer with objects typed
`player`, `frog`, `opossum`, `eagle`, `cherry` and `gem` is enough to place
everything, with no properties and no loader change. Tiled writes objects at
map-origin pixels, which is the coordinate space `Entity::x`/`y` already use.

### 4. The level is authored against overdraw, deliberately

`data/sunnyland.tmx` was built as the *worst* case — a backdrop covering every
cell — to make the fill-rate meter move, and it did its job. The game's level is
built the other way:

- **Sky is `Context::clear()`, not a tile layer.** A cleared colour is one fill;
  a covering tile layer is a full-screen blit that the layers above it then paint
  over. This is the overdraw lever results.md identified, applied.
- **The camera is integer pixels.** A fractional camera puts every tile blit on
  SDL's stretch path, measured at several times the 1:1 cost. Entity positions
  stay `double` — they integrate velocity and rounding every frame makes slow
  movement stutter, which is why `Entity` is already `double` — and get rounded
  once, at draw.
- **Two visible tile layers, not three**, plus the invisible collision layer.

The result should be a level that is *cheaper* than the fixture while looking
like more. That is a claim, and `orchard --seconds N` reporting frame cost the
way `sprites --tilemap` does is how it gets checked rather than asserted.

### 5. The entity store is promoted and extended, not templated

`sprites::Entities` becomes `game::Entities` in a new `game/` module, and
`Entity` grows a collision box and a behaviour tag. `sprites` uses the promoted
store and ignores the fields it does not read.

This is the trigger `entities.hpp` recorded firing exactly as written — "a file
move plus a namespace" — so the names do not change with it. `game::Entity`, not
`game::Actor`: renaming a type at the same moment as moving it makes the diff
unreadable and buys nothing.

The alternative considered and rejected was templating the storage over a payload
(`game::Store<T>`, with the generational handles and free list generic and each
demo supplying its own actor type). That is the house compile-time-polymorphism
pattern and would need no `concept` — a documented traits requirement, as
`util::tokenizer_traits` already is. It was rejected as generalising ahead of a
third consumer: two known users do not tell you where the seam goes, and the
version that guesses is the one that has to be undone. The cost of being wrong
the other way is a few unused bytes across tens of entities.

**Two consequences worth naming now**, because both are the kind of thing that
turns a "file move" into an afternoon:

- `tests/CMakeLists.txt:81` compiles `../sprites/entities.cc` directly into
  `test_entities`, the arrangement `coppers/bars.cc` established for code living
  inside an executable. Once this is a module the test links `wreel::game`
  instead, and that is the change that proves the promotion actually happened.
- `sprites/CMakeLists.txt` drops `entities.cc` from its sources and gains a
  `wreel::game` link. Its `WREEL_BUNDLE_ASSETS` list is unaffected.

### 6. Sound and music, with the effects generated by a tool

Yes to both, and it is cheaper than it looks: **the engine work is zero.**

- `audio::Sound` already exists — a facade over `Mix_Chunk`, decoded fully into
  memory at load, `play(loops)` and `set_volume`. Its constructor "succeeds even
  with no audio device open", so it is testable headless.
- `audio::Music` already streams one track at a time, and its lifetime bug (D20)
  is fixed and regression-tested.
- **`WREEL_AUDIO_CODECS=minimal` is "WAV + tracker (MOD/XM/IT)"**
  (`cmake/ProjectOptions.cmake:130`). Generated WAV effects and a tracker module
  are *both* in the smallest tier, so this adds no codec to any target. The
  handheld builds are `standard` and desktop is `full`, so there is headroom that
  will not be used.
- Audio is proven on the device: `coppers` ran 859 frames at 59.7 fps **with
  audio** on the Miyoo Mini on 2026-08-01.

`include/audio/device.hpp` states the cost model that makes this safe, and it is
easy to get backwards: codecs cost **binary size, not CPU** — SDL2_mixer picks a
decoder from file contents at load and one that never matches never executes. The
per-frame cost is the mixer profile, fixed at `Device` construction: 22050 Hz /
2048 samples / 8 voices on a handheld, 44100 / 1024 / 16 on desktop.

**Effects are generated offline by `tools/make_sfx.py` into WAV, not synthesised
at runtime.** The runtime synth is the more tempting design and the wrong one
here: it puts buffer generation next to an audio callback on a device whose SDL
port this project has already had to fix seven times, to save a few kilobytes of
WAV. Offline output is also *listenable*, which is the only real review method for
a sound, and byte-identical regeneration is checkable by sha256 — the precedent
`tools/pack_atlas.py` already set.

**One chunk per event, and the pitch movement lives inside the sample.** The jump
cue is a glissando — a sweep from root to a 5th above over its own 60–80 ms — and
it is rendered that way, once. There is no streak, no ladder, and no variant set.

This is worth stating as a decision rather than an omission, because
**SDL2_mixer cannot pitch-shift a chunk**: anything that varies pitch across
successive events has to be pre-rendered as separate chunks, and that machinery
is the thing being declined here. If a streak is wanted later it costs about 2 KB
per variant at 0.1 s mono 22050 Hz — cheap, but not free in the code that has to
pick between them, which is the part not worth having yet.

Generate at **22050 Hz mono** to match the handheld mixer exactly. `Mix_LoadWAV`
converts to the mixer format once at load, so the desktop's upsample is paid once
and costs nothing; the point is that the target that can least afford resampling
does none.

#### What makes it sound 8-bit rather than merely synthesised

Four details worth writing down, because three of them are the difference between
"chiptune" and "a beep", and the fourth is the bug everyone writes first:

- **Duty cycle is the timbre knob, not the waveform.** The NES 2A03 offers
  12.5 / 25 / 50 / 75 %. 12.5 % is thin and reedy, 50 % is hollow and full. A
  square generator without a duty parameter can only make one sound.
- **Noise should be an LFSR, not `random()`.** The 2A03 noise channel is a 15-bit
  linear-feedback shift register clocked at one of 16 periods, and "stepping the
  pitch down" means stepping that period. It is about ten lines of Python and it
  is *why* the descending-noise dizzy cue will read as 8-bit; white noise stepped
  through a filter does not sound the same.
- **Do not band-limit.** A naive square aliases, and at 22050 Hz it aliases
  audibly. That aliasing is part of the sound being asked for — reaching for
  PolyBLEP here makes it cleaner and worse.
- **1–2 ms linear fade at both ends.** A square starting mid-cycle at full
  amplitude pops, and the pop is louder than the effect. This is the single most
  common defect in generated blips and it is invisible until it is played through
  something with bass response.

#### The authored artifact is the description, not the WAV

`data/sfx.xml` describes each effect — waveform, duty, note or LFSR period list,
envelope, duration — and `tools/make_sfx.py` renders it to WAVs. Seeded with
defaults, then hand-tuned, and the tool **merges rather than overwrites** so
tuning survives a regenerate. That is exactly the arrangement `foxy.anim.xml`
already uses and it is the shape the tooling rule asks for: the tool's output is
derived, its input is the thing a human edits.

One distinction to state so nobody wires it up wrong: unlike `foxy.anim.xml`,
**`sfx.xml` is not read at runtime.** It is a tool input. The game loads WAVs.

#### The music is `data/emerald-droplets.xm`

By **malmen**, freely available under a copyleft licence requiring artist
attribution. Settled 2026-08-10. It gets a `PROVENANCE.md` row with that
attribution, which is what the licence asks for and what the three existing
`.mod` files — all still in that document's *unknown* section — cannot be given.
They are not referenced by this demo.

Inspected rather than taken on the filename: MilkyTracker, XM 1.04, 24 channels,
46 patterns, tempo 4 / 161 BPM, 17 instruments across 19 samples, 174 KB. Most of
those samples are 30–60 byte single-cycle waveforms, which is both why it is that
small and why it sits well next to generated square-wave effects.

**No codec change**: `minimal` is already WAV + tracker (MOD/XM/IT), so XM costs
nothing on any target. The one thing it does change is mixing load — see Risks.

### 7. Screenshots are deterministic by construction

`coppers` learned this the expensive way: its cross-architecture byte-identical
fixture requires `--no-hud`, because the HUD draws measured microseconds, so with
it on *one binary does not reproduce itself*. The recipe originally omitted that.

So `orchard --screenshot` drives a **fixed delta**, reads no wall clock, and if
anything is random it takes a seed. Built in from the first commit, because
retrofitting determinism means finding every place that read a clock.

## Proposed shape

```
game::Entity         the promoted struct + an AABB + a Behaviour tag
game::Entities       sprites::Entities, moved and extended (decision 5)
game::Level          TileMap + the collision layer index + the spawn table
game::collide        AABB vs. the collision layer; AABB vs. AABB
game::Camera         follow with a deadzone, clamped to level bounds, integer out
game::Action         Pickup/Jump/Left/Right over rig::Button
game::Sfx            named audio::Sound handles, loaded once through asset_path()
orchard/             the demo: rules, HUD, --screenshot, --seconds, --mute
tools/make_sfx.py    data/sfx.xml -> the WAVs (decision 6)
```

`game::collide` is free functions over plain data, not a class — it needs no
state, and that keeps it testable without a renderer, the way `load_sparrow` and
`load_obj` are.

## Tasks

Ordered so something is on screen early, and so the parts that can be tested
headless are written before the parts that cannot.

- [ ] Ingest the art. Copy frog, opossum, eagle, cherry, gem, puff and sparkle
      into `data/sunny-land/`, renaming `enemy-death` → `puff` and `attack` →
      `swoop` **in the copy**. Pack as `critters.{png,xml}` with
      `tools/pack_atlas.py --verify`; hand-tune `critters.anim.xml`. Record the
      new rows in `data/PROVENANCE.md` under the existing CC0 grant
- [ ] `game::collide` — AABB against a tile layer, AABB against AABB, with the
      swept/resolve behaviour spelled out. Headless tests first; this is the part
      most likely to be subtly wrong and the easiest to pin. `util::floor_div`
      and `floor_mod` already exist for exactly the world-to-tile conversion this
      needs, and exist because C division truncates toward zero
- [ ] `game::Camera` — follow with a deadzone, clamp to level bounds, integer
      output. Headless; it is arithmetic over supplied positions
- [ ] `game::Level` — wraps a `TileMap`, resolves the `collision` layer index
      once at load, reads the `spawns` object layer into a typed table. Fails
      loudly if either is missing, rather than silently producing a level with no
      floor
- [ ] Promote the entity store to `wreel::game` per decision 5 — the move and the
      namespace first, with `test_entities` relinked and passing unchanged, and
      the AABB and behaviour tag added only after that is green. Two commits, so
      that a regression in the second one has a working first to bisect to
- [ ] `game::Action` over `rig::Pad`. One indirection, justified by the raw pad
      mapping being a documented guess: when a handheld reports the wrong button,
      the fix should be one table, not a search for `Button::A`
- [ ] A level, by extending `tools/make_tilemap.py` or a sibling — Tiled is still
      not available here, and `data/PROVENANCE.md` should keep saying so plainly
- [ ] `orchard` — the rules, the HUD, `--screenshot`, `--seconds`, `--mute`, and
      the fill-rate line. Builds on all five presets; no GPU requirement, same as
      `sprites`. Its `WREEL_BUNDLE_ASSETS` list names every file it opens,
      including the WAVs, since that list is what a firmware bundle carries
- [ ] `tools/make_sfx.py` + `data/sfx.xml` — square with a duty parameter, an
      LFSR noise channel, an envelope, a 1–2 ms fade at both ends, 22050 Hz mono
      WAV out. Merge-not-overwrite on the description, as `pack_atlas.py` does
      for the animation sidecar, and a sha256 regeneration check
- [ ] Wire the cues: jump (glissando), land, pickup, bump, and the
      level-complete flourish. One chunk each. `--mute` exists so a timed run and
      a screenshot run are not also an audio test
- [ ] Add the `data/emerald-droplets.xm` row to `data/PROVENANCE.md` — malmen,
      copyleft with artist attribution — and name it in the demo's
      `WREEL_BUNDLE_ASSETS`
- [ ] Add it to the Onion bundle as a fourth entry, and to `docs/DEVELOPMENT.md
      § Status` with an honest verification state

## Risks

**The promoted entity store makes a demo depend on a module for fields it does
not use.** Decision 5 takes this knowingly, and the residual risk is that
`game::Entity` becomes the place every later need gets appended to, one field at
a time, until it is a struct nobody can describe. The guard is that a third
consumer is the trigger to revisit the templated storage — not a fourth field.

**The music is 24 channels where everything measured so far was four.** All three
existing `.mod` files are `M.K.`, four channels; `emerald-droplets.xm` is 24.
`ProjectOptions.cmake` is explicit that the mixer profile — not the codec — is
where per-frame CPU lives, and the handheld runs 22050 Hz / 2048 samples /
8 voices on two Cortex-A7 cores that are also software-rasterising. `coppers`'
59.7 fps on the device was measured against a 4-channel module, so it is not
evidence for this one. Cheap to settle rather than argue about: `orchard
--seconds` with and without music, on the device.

**Generated audio is easy to get subtly wrong in ways that only show up on the
device.** The dev box is 44100 Hz with 16 voices and a forgiving buffer; the
Miyoo Mini is 22050 with 8 and 2048 samples. A cue that fires every frame while a
button is held, or eight overlapping pickups, will sound fine here and starve
there. Effects are one-shot and triggered on edges (`pressed()`, not `down()`),
and the fill-rate run reports voice count alongside frame cost.

**"Simple game" is the least reliable estimate in software.** The scope above is
deliberately small and will still try to grow a title screen, a pause menu, a
second level and sound. The guard is that none of those is in Tasks, and adding
one means saying so here first.

**The fill-rate claim in decision 4 is a prediction.** A level built against
overdraw *should* cost less than the fixture. It has not been measured and might
not, particularly if a parallax layer sneaks in. `orchard --seconds` is the
instrument; the number goes in results.md next to the others.

**Still nothing has run on a device.** Unchanged from every snapshot before this
one, and this one adds code on top of a fill-rate figure taken only on a dev box —
1.162 ms at 640×480, times a guessed 10–20× for two Cortex-A7 cores, which
straddles the 60 fps budget. The mitigation is that this work does not *depend* on
that answer: a wrong guess changes how much fits on screen, not whether the
collision code is correct. But it is the reason not to build a second level before
the first one runs on hardware.

## Open questions

- ~~Does the level-complete flourish use `victory` (1 frame) or a still?~~
  **Answered 2026-08-10: `victory`.** `foxy.anim.xml` declares it `fps="6"
  loop="repeat" frames="1"`, so it holds the pose until the game moves on —
  the same property that decided the bump cue, and for the same reason: **the
  game state sets the duration, not the art.** Nothing in the code has to
  special-case a still, and `AnimatedSprite` plays it like any other animation.

  The consequence stands unchanged: with one frame of art doing the work, the
  flourish has to come from the sound cue and the HUD.

No open questions remain. Everything above is decided; what is unverified is
listed under Risks and is a measurement rather than a choice.

## References

- [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) — the
  predecessor, and the source of the scope line this snapshot picks up
- [target-validation/results.md](../2026-07-25-target-validation/results.md) — the
  fill-rate measurements decision 4 is built on
- [packaging-distribution](../2026-07-25-packaging-distribution/) — where a fourth
  bundle entry lands
- `data/PROVENANCE.md` — the CC0 grant this art arrives under, and the unknown
  section the three `.mod` files are still in
- `include/audio/device.hpp` — the codec cost model decision 6 rests on
- `cmake/ProjectOptions.cmake:130` — the codec tiers; `minimal` is WAV + tracker
- `include/gfx/tilemap.hpp`, `include/rig/input.hpp`, `sprites/entities.hpp`
