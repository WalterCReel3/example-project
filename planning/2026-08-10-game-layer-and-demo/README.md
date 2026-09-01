# A game layer, and a small game to define it

**Status:** `in progress` — the game layer landed 2026-08-22 and was finished
2026-08-30 with `game::Level` and `game::Action`; the game has not started. See
[Where this stands](#where-this-stands-2026-08-30).
**Written:** 2026-08-10
**Blocked by:** nothing, to continue. The art ingest and everything downstream
of it wait on the frame-name question in
[Open questions](#one-open-question-found-2026-08-14-while-checking-the-first-task).
Device verification runs in parallel and is
[target-validation](../2026-07-25-target-validation/)'s

> This is the successor
> [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) named on
> 2026-07-25, when it scoped itself to "rendering plus a minimal entity store" and
> put the rest behind a line: *"Not in scope: collision, camera, scene graph,
> physics, input mapping. Those are a game layer and belong in their own
> snapshot."* This is that snapshot.

## Where this stands (2026-08-30)

The **layer** is complete. The **game** does not exist. Everything through
`game::Camera` merged as `a9ca839` on PR #16; `game::Level` and `game::Action`
are **in the working tree and not yet committed** — `tests/test_action.cc` is
still untracked, so it has no revert path until someone stages it.

| Piece | State |
|---|---|
| `game::Entities` / `Entity` | done — promoted, with the box and tag |
| `game::collide` | done — `test_collide`, 30 cases / 245 assertions |
| `game::Camera` | done — `test_camera`, 21 / 330 |
| `tools/make_sfx.py` + `data/sfx.xml` + 5 WAVs | done — sounds heard and approved provisionally |
| `game::Level` | done 2026-08-30 — `test_level`, 19 cases / 70 assertions. Decision 10's spawn check is paid |
| `game::Action` | done 2026-08-30 — `test_action`, 15 cases / 78 assertions |
| `game::Sfx`, the cues | not started |
| the art ingest, the level, `orchard` | **blocked** on the frame-name question below |

Verified 2026-08-30, from forced cold recompiles: `desktop-software` and
`desktop-debug` each configure, build and test clean — 111 and 121 targets, zero
warnings with `WREEL_WERROR` on, **26/26 tests on both**. `test_entities` grew to
24 cases / 140 assertions in the same pass.

The previous entry here recorded **642 and 658** targets on 2026-08-22. That is
not a tree that shrank: those were *from-zero* configures into fresh build
directories, which build the eight pinned dependencies and count them, whereas
these directories already had all eight. Two measurements of different things,
and neither supersedes the other.

Those are the **only two presets that were run**. The other five have not been
configured in this pass, and the two `miyoomini` build directories are blocked by
the stale-cache item below — so "all seven presets configure" is a claim from
2026-08-22 that nothing since has re-established.

Not verified, and it is the same gap every snapshot before this one has had:
**no line of `game/` has run on ARM or under GCC 8.3.** Both green presets are
the same x86-64 host compiler and libstdc++, differing only in
`WREEL_TARGET_HAS_GPU` and optimisation. `game::collide` and `game::Camera` are
pure double/int arithmetic, and the two places a different toolchain would
plausibly disagree — the `.5`-boundary oscillation case in `test_camera`, the
flush-stop idempotence case in `test_collide` — are exactly the cases this
document predicted would be subtly wrong. One architecture is not two.

That statement is now narrower than it was, and the narrowing is worth stating
precisely because it is easy to over-read. On 2026-08-30 **one** test binary was
built with the union-miyoomini GCC 8.3 toolchain and run under `qemu-arm`:
`test_tmx`, 20 cases / 2410 assertions, zero warnings. It was done to settle
D30, a 32-bit `size_t` overflow that no 64-bit host can reproduce, and it is the
first time any of this tree has executed under the device compiler. It says
nothing about `game/` — `test_tmx` covers `loaders/`, and no `game/` test was
built or run there — and it is not a cross-preset pass. Two facts, and they must
not blur: one binary ran under GCC 8.3, and the cross presets remain unrun.

Also unpaid, from decision 4: **the fill-rate claim is still a prediction.**
Nothing has been measured, because `orchard --seconds` is the instrument and
`orchard` does not exist.

### Two housekeeping items a new session will trip over

- `build/miyoomini` and `build/miyoomini-e` are root-owned with stale caches
  generated inside a container against source `/src/CMakeLists.txt`, so
  `cmake --preset miyoomini` fails on a source-dir mismatch. The **preset is
  sound** — pointed at a fresh directory it configures cleanly. Only the
  default build directory is poisoned. Needs someone with root, not a fix here.

  Still true on 2026-08-30, and slightly more so. The root ownership dates from
  a container run on 2026-08-09, before any of this work; on 2026-08-30 the D30
  investigation deliberately re-ran `cmake` in the container against
  `build/miyoomini` to get a GCC 8.3 `test_tmx`, which added more root-owned
  artefacts to a directory that already had them. That was agreed at the time
  and it bought the only GCC 8.3 run this tree has. It did not create the
  problem and it does not change the remedy: `chown` it, or point the preset at
  a fresh directory.
- There is no `make format-file` target and no Makefile in this tree, though
  tooling instructions elsewhere reference one. Use `clang-format -i` directly.

### Known and deliberately uncorrected

Found during the work, judged not worth the churn at the time. Recorded so the
next reader meets them as decisions rather than as discoveries.

- **Decision 6 says mono matches the handheld mixer "exactly". It does not.**
  `WREEL_AUDIO_CHANNELS` is an unconditional `2` on every target
  (`cmake/ProjectOptions.cmake:153`), while the *rate* at lines 141-149 is
  conditional. So mono matches the handheld's sample rate exactly and its
  channel count on no target at all. The decision's conclusion is unaffected —
  `Mix_LoadWAV` converts once at load, and mono still halves the asset bytes for
  an identical audible result — only the justification overstates. The same
  phrasing has propagated to `tools/make_sfx.py` and `tools/README.md`, so the
  correction lands in three places when it is made.
- **`apply_fade` in `tools/make_sfx.py` both mutates in place and returns, and
  its caller discards the return.** Rewriting it as a pure function — the more
  natural spelling — would silently delete the fade from every effect with no
  test failing. That is a trap sitting directly on top of the defect decision 6
  calls the most common in generated blips.
- **Decision 6 reads as a permanent rejection of runtime synthesis.** The
  intended position is narrower: a runtime generator is a wanted future
  addition, out of scope now. "Not yet", not "never".

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

### 8. The behaviour tag is opaque to the module, and its sentinel is -1

Settled 2026-08-22, while implementing decision 5. Decision 5 says `Entity`
grows "a behaviour tag" and leaves the spelling open. The obvious spelling is
the vocabulary decision 3 already fixes — `enum class Behaviour { None, Player,
Frog, Opossum, Eagle, Cherry, Gem }` — and it is the wrong one, because it puts
one demo's animals inside `include/game/entities.hpp`, a module named for the
layer every other demo is meant to share and which `collide` and `Camera` now
build on. It also contradicts `game/CMakeLists.txt`'s own charter comment.

So the tag is a plain `int` the module never interprets. A demo declares its own
enum and casts once, at the spawn site. Decision 3's "typed table" of spawns
belongs to `game::Level` or to `orchard`, not here.

**The sentinel is `-1`, and the signedness is part of the same decision.** The
first attempt reserved `0` for "no rule attached". A C++ enumeration numbers from
0 unless told otherwise, so `Behaviour::Player == 0 ==` untagged, and the failure
is silent: the player spawns, draws and moves correctly and only the rules loop
skips it, which reads as an input bug. The fix is not a comment telling consumers
to start their enum at 1 — the next consumer will not read it — but a sentinel
outside the range anyone writes by accident.

### 9. `Aabb` lives in `collide.hpp`, and the box is stored relative

Settled 2026-08-22. The collision box's type belongs next to the free functions
that consume it, so `entities.hpp` includes `collide.hpp` rather than the
reverse. **This inverts the task order in this document**: `game::collide` has to
be written before `Entity` can grow a box.

Known cost, named rather than discovered: every consumer of the entity store now
compiles `gfx/tilemap.hpp` transitively, including `sprites`, which reads neither
new field.

The box is stored **relative to `(x, y)`**, with an `Entity::world_box()`
accessor. `(x, y)` is the entity's only position and `update()` integrates
velocity into it, so an absolute box would be a second copy that every mover has
to advance in step — and when one path forgets, both members hold perfectly valid
boxes that merely disagree. The accessor is what stops the addition being written
out at every call site, which is the unreadable conversion this document warned
about when it settled the box in the first place.

### 10. Out of bounds is solid, and collision does not rescue a bad spawn

Settled 2026-08-22. `TileLayer::at()` returns `empty_tile` outside the layer, so
the naive reading is that walking off the map finds open air. `game::collide`
treats outside as **solid** instead.

The consequence was found by probing rather than by reasoning, which is the only
reason it is stated correctly here: an entity spawned outside the map can be
**trapped**. The precise rule — and the header now says exactly this, with a test
asserting every number the paragraph quotes — is *stuck iff an unoccupied outside
cell lies between the box and the map on the axis of travel*. A box far outside
is stuck; a box abutting the boundary walks straight back in with no hit flag.

Collision deliberately does not rescue either case. An out-of-bounds spawn is
level data being wrong, and a silent nudge back inside means the bad spawn is
never noticed. **`game::Level` therefore owes a load-time check** that rejects a
spawn outside the map bounds, alongside the "fails loudly if the `collision` or
`spawns` layer is missing" behaviour it is already specced for.

Worth being honest about the strength of the argument: "a frozen entity is a
louder symptom than a silent rescue" holds for a far-outside spawn and **fails**
for a near-edge one, which walks in and then looks entirely correct. The loud
symptom is not guaranteed, which makes the Level check load-bearing rather than
belt-and-braces. Until it exists, the collide header is the only written record
of that failure mode.

That header was wrong twice before it was pinned — the first correction replaced
one overstatement with another. It is now backed by a test rather than by a third
carefully-worded sentence, which is the only thing that stops a fourth.

### 11. Decision 5's two-commit split was not made

Recorded 2026-08-22 because it is a cost that was paid, not an oversight.
Decision 5 asks for the promotion to land as two commits, the first
behaviour-neutral, so a regression in the extension has a working commit to
bisect to. Decision 9 made that impractical: with `Aabb` in `collide.hpp`, the
move can no longer precede the extension without also stripping and reinstating
an include, which means hand-carving a state that never existed as a tested tree.

The header placement is the better call for the code, and it cost the bisect
point. Everything landed as one commit, `a9ca839`.

## Proposed shape

```
game::Entity         the promoted struct + an AABB + a Behaviour tag
game::Entities       sprites::Entities, moved and extended (decision 5)
game::Level          the parsed layers + a copy of the collision layer + the spawn table
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

**`game::Level` shipped differently from the line above, in two ways, and the
code is right where this sketch was wrong.** Corrected 2026-08-30 against
`include/game/level.hpp`, which argues both at length:

- **It does not wrap a `TileMap`, and deliberately has no constructor from one.**
  A `gfx::TileMap` keeps the *tileset's* tile size, not the map grid's, and its
  `pixel_width()`/`pixel_height()` are built on that. Tiled lets a tileset hold
  tiles taller than the grid — how a 16×16 map draws a 16×32 tree — so on such a
  map a `TileMap` cannot recover the number `Level` needs, and every solid cell
  would land in the wrong place. A constructor that is silently wrong on a legal
  `.tmx` is worse than one that does not exist. `Level` takes the parsed layers
  instead: exactly what `loaders::load_tmx` returns, minus the tileset. That also
  keeps it headless, since a `TileMap` transitively needs a video driver, and
  headless is what lets it run under `qemu-user-static`.
- **It stores a copy of the collision layer, not an index into somebody else's
  vector.** This meets "resolve the layer once at load" more completely: there is
  no per-frame name lookup *and* no map to keep alive, so the drawable `TileMap`
  can be moved, rebuilt or dropped without invalidating the level. The copy is
  one `std::vector<int>` of width × height — 9 KB for the 64×36 fixture, against
  128 MB on the smallest target.

## Tasks

Ordered so something is on screen early, and so the parts that can be tested
headless are written before the parts that cannot.

**Two amendments from doing the work**, both 2026-08-22. The entity-store
promotion is listed after `collide` and `Camera` but was written to run before
them; decision 9 reversed that, because `Aabb` lives in `collide.hpp` and
`entities.hpp` includes it. And the promotion's "two commits" clause was not
honoured — see decision 11.

- [ ] Ingest the art. Copy frog, opossum, eagle, cherry, gem, puff and sparkle
      into `data/sunny-land/`, renaming `enemy-death` → `puff` and `attack` →
      `swoop` **in the copy**. The source frames are 1-based and unpadded
      (`frog-idle-1.png`), and `pack_atlas.py`'s verify pass opens
      `<animation>/{index:03d}.png`, so the copy renumbers as well as renames —
      as `data/sunny-land/foxy/` already did. Pack as `critters.{png,xml}` with
      `tools/pack_atlas.py` (verification is on by default; `--no-verify` turns it
      off — there is no `--verify` flag); hand-tune `critters.anim.xml`. Record the
      new rows in `data/PROVENANCE.md` under the existing CC0 grant.
      **Settle the frame-name question below first** — it is cheap now and
      expensive once `critters.anim.xml` is hand-tuned against it
- [x] `game::collide` — AABB against a tile layer, AABB against AABB, with the
      swept/resolve behaviour spelled out. Headless tests first; this is the part
      most likely to be subtly wrong and the easiest to pin. `util::floor_div`
      and `floor_mod` already exist for exactly the world-to-tile conversion this
      needs, and exist because C division truncates toward zero
- [x] `game::Camera` — follow with a deadzone, clamp to level bounds, integer
      output. Headless; it is arithmetic over supplied positions
- [x] `game::Level` — **done 2026-08-30 — `test_level`, 19 cases / 70
      assertions.** Takes the parsed layers rather than a `TileMap` and stores a
      *copy* of the `collision` layer rather than an index into one; both
      divergences from the original wording are deliberate and argued under
      Proposed shape above. Reads the `spawns` object layer into a typed table —
      `gfx::MapObject`, with `type` left a string so decision 8's actor
      vocabulary stays out of the module. Fails loudly if either layer is
      missing, rather than silently producing a level with no floor, and refuses
      four more shapes the header lists, including a collision layer whose tile
      data is shorter than its declared size. **Rejects a spawn outside the map
      bounds**, per decision 10, which is now paid
- [x] Promote the entity store to `wreel::game` per decision 5 — the move and the
      namespace first, with `test_entities` relinked and passing unchanged, and
      the AABB and behaviour tag added only after that is green. Two commits, so
      that a regression in the second one has a working first to bisect to
- [x] `game::Action` over `rig::Pad` — **done 2026-08-30 — `test_action`, 15
      cases / 78 assertions.** One indirection, justified by the raw pad mapping
      being a documented guess: when a handheld reports the wrong button, the fix
      should be one table, not a search for `Button::A`. The enum and its name
      table are now tied together by a `static_assert` on the deduced array size,
      so adding an action without extending the table fails the build rather than
      returning a null pointer from `action_name`
- [ ] A level, by extending `tools/make_tilemap.py` or a sibling — Tiled is still
      not available here, and `data/PROVENANCE.md` should keep saying so plainly
- [ ] `orchard` — the rules, the HUD, `--screenshot`, `--seconds`, `--mute`, and
      the fill-rate line. Builds on all five presets; no GPU requirement, same as
      `sprites`. Its `WREEL_BUNDLE_ASSETS` list names every file it opens,
      including the WAVs, since that list is what a firmware bundle carries
- [x] `tools/make_sfx.py` + `data/sfx.xml` — square with a duty parameter, an
      LFSR noise channel, an envelope, a 1–2 ms fade at both ends, 22050 Hz mono
      WAV out. Merge-not-overwrite on the description, as `pack_atlas.py` does
      for the animation sidecar, and a sha256 regeneration check
- [ ] Wire the cues: jump (glissando), land, pickup, bump, and the
      level-complete flourish. One chunk each. `--mute` exists so a timed run and
      a screenshot run are not also an audio test
- [x] Add the `data/emerald-droplets.xm` row to `data/PROVENANCE.md` — malmen,
      copyleft with artist attribution — done 2026-08-10, with the detail block
- [ ] Name `emerald-droplets.xm` in the demo's `WREEL_BUNDLE_ASSETS`, which waits
      on the demo existing
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

### Five questions opened 2026-08-30, none of them answered

These came out of the pass that finished `game::Level` and `game::Action`. Each
is a **decision nobody has made**, not a defect and not an oversight — they are
here so the next session meets them as open rather than rediscovering them. None
of them is what the art ingest is waiting on; that is still the frame-name
question below. Number 2 is the one with a date on it — it blocks the level task
specifically.

1. **What bound, if any, `game::Entity`'s coordinate conversion should carry, and
   whether exceeding it clamps or throws.** The `Entities::draw` rounding fix
   (D28) left the double→int conversion deliberately unguarded: a `double`
   outside `int`'s range — or a NaN, which is a distinct undefined behaviour
   reached by a single `update()` tick at infinite velocity — is UB on conversion
   whether it is floored first or not. Picking a coordinate bound is a design
   decision with a gameplay consequence, so the fix stopped at the rounding and
   said so.
2. **Which side of the spawn-layer name moves.** `data/sunnyland.tmx` names its
   object layer `objects`; `include/game/level.hpp` declares
   `spawn_layer_name = "spawns"` and `game/level.cc` throws when no object layer
   matches. So building a `Level` from the only `.tmx` in the tree throws *"no
   object layer named spawns"* before it ever reaches the spawn bounds check —
   **the two halves of that pipeline have never been connected end to end.**
   Neither file is wrong on its own; each is internally consistent, and the
   contract between them was simply never exercised. Either rename the layer in
   the fixture and in `tools/make_tilemap.py` which generates it, or widen what
   `Level` accepts. Deliberately unfixed in both directions, because picking is
   the decision. **This one will bite the moment the level task starts.**
3. **Which of D29's two candidate fixes to take for the Tiled tile-object
   origin.** An object with a `gid` is anchored bottom-left, and
   `loaders::load_tmx` reads `y` without inspecting `gid`, so a tile object's `y`
   is its bottom edge while `Level` reads it as the top. Both candidates are
   written out in the D29 row with their costs; neither is picked. Note the
   complication recorded there: bottom-left is Tiled's *default*, not an
   invariant — `objectalignment` on `<tileset>` can override it and
   `read_tileset_body` does not read that attribute.
4. **The largest map we pre-allocate for.** `loaders/tmx.cc` reserves before the
   length check is reached, which on a 64-bit host means a malformed declaration
   can ask for 16 GB up front. Closing it properly needs a policy number — the
   biggest map this project agrees to accept — and nobody has set one. Distinct
   from D30, which was the 32-bit overflow and is fixed.
5. **Where `gfx::TileLayer`'s `tiles.size() == width * height` invariant should
   be enforced.** Today it is a comment, enforced per-consumer:
   `loaders::load_tmx` checks it, and `game::Level` now checks it again because
   its constructor is public and takes bare vectors. That is two checks and no
   single owner, which is fine until a third consumer forgets. Recorded as a
   design question in D30's "does not cover" section.

Separately and **not decided**: whether ASAN or valgrind should become part of
the tooling here. Several findings in this pass were confirmed with throwaway
sanitizer builds in `/tmp` rather than anything in the tree, which is what raised
the question. No preset exists for either and none is planned pending that
discussion.

### One open question, found 2026-08-14 while checking the first task

**Still open as of 2026-08-22, and it is now the only thing blocking progress.**
The layer is done; the art ingest, the level, `orchard`, the cue wiring and the
bundle entries all sit behind this one answer.

One correction to the three options below, from re-reading them against decision
1: the objection to one-atlas-per-actor is *texture binds the Miyoo Mini may not
want to pay*, and decision 1 forty lines earlier records that binds were measured
**not** to be what is billed for, against 3.5 ns per pixel. That reason does not
survive this document's own finding. The real cost of per-actor atlases is five
files to keep in sync, not binds. Similarly, the risk attributed to teaching
`pack_atlas.py` a namespace argument is overstated if the argument is optional
and defaults to off — `data/foxy.*` would then regenerate through an unchanged
code path by construction.

Which leaves prefix-in-the-copy as the cheapest: the copy step is *already*
renaming (`enemy-death` → `puff`, `attack` → `swoop`) and renumbering to
`{index:03d}`, so a prefix is the same edit in the same place at no extra cost.

**How the `critters` atlas namespaces its frame names.** `pack_atlas.py` takes one
source directory and derives animation names from the directory names under it, so
packing frog, opossum, eagle and the pickups together produces bare `idle.000` and
`jump.000` — and the frog and the opossum both have an `idle`. Decision 1 above
spends its length on why a frame name is expensive to change after the fact, and
then this case falls outside it.

Three ways out, none of them decided:

- **Prefix in the copy** — `frog-idle/`, `opossum-idle/` as directory names. No tool
  change; the sidecar carries the prefix forever.
- **One atlas per actor** — `frog.png`, `opossum.png`, `eagle.png`, `pickups.png`.
  Matches `foxy.png`'s precedent and costs texture binds the Miyoo Mini may not
  want to pay.
- **Teach `pack_atlas.py` a namespace argument.** The cleanest names, and the only
  option that changes a tool `data/foxy.*` is already generated by — the
  regeneration-produces-identical-bytes property in `data/PROVENANCE.md` is what
  that would put at risk.

Everything else above is decided; what is unverified is listed under Risks and is a
measurement rather than a choice.

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
