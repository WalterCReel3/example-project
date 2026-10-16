# A game layer, and a small game to define it

**Status:** `in-progress` — the game layer landed 2026-08-22 and was finished
2026-08-30 with `game::Level` and `game::Action`; one landed piece has since been
removed from the layer entirely. The game itself, `orchard`, is playable end to
end as of 2026-09-14 — see
[decision 18](#18-contact-costs-the-last-pickup-the-goal-ends-the-run) — and has
had its cues and music since 2026-09-15. It has run once on a Miyoo Mini, from
the GCC 8.3 Onion bundle, and was played to the goal
([results.md](../2026-07-25-target-validation/results.md), 2026-10-06).
`orchard/` and everything after #18 is committed on `feature/orchard-demo` as
`3af0cb9`..`9921511` (2026-10-16), not pushed. See
[Where this stands](#where-this-stands-2026-10-16).
**Written:** 2026-08-10
**Blocked by:** nothing. The frame-name question that held the art ingest and
everything downstream of it is answered — see
[decision 15](#15-frame-names-are-namespaced-by-prefixing-in-the-copy).
Device verification runs in parallel and is
[target-validation](../2026-07-25-target-validation/)'s job, not this
document's.

> This is the successor
> [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) named on
> 2026-07-25, when it scoped itself to "rendering plus a minimal entity store" and
> put the rest behind a line: *"Not in scope: collision, camera, scene graph,
> physics, input mapping. Those are a game layer and belong in their own
> snapshot."* This is that snapshot.

## Where this stands (2026-10-16)

The **layer** is complete, one piece smaller than it was, and all of it is
committed: everything through `game::Camera` merged as `a9ca839` on PR #16;
`game::Level` and `game::Action` merged as `c1a1c00` on PR #17, which also
carried D28–D30; the deletion of `game::Action` merged as `62714da` on PR #18,
2026-09-04. Nothing after that is on `master`.

The **game** exists and is committed on `feature/orchard-demo` (cut from
`62714da` on 2026-10-06), not pushed. `orchard` has the level, the player,
the cast, the rules, the HUD, the cues and the music; the table below has each
slice. It was committed 2026-10-16 in the groups
[orchard-first-device-run](../2026-10-06-orchard-first-device-run/) laid out:
`3af0cb9` "feat(orchard): add the orchard platformer demo" is the game itself,
and `479e26f`, `c74e310`, `69d6aff`, `dc534d4`, `1b811fd` and `9921511` follow
it (that handoff's § Where the tree is has the subjects).

**It has run on a device once.** The Onion bundle built under GCC 8.3 in the
`wreel-miyoomini` container was launched as shipped (`--software --verbose`) on
a Miyoo Mini and the level was played to the goal: 4383 frames at 59.7 fps on a
capped run, the game's own work 4.1 ms of a 16.762 ms frame, `present` 12.582 ms,
cues playing, exit `stopped by the rules`. Recorded in
[results.md § 2026-10-06](../2026-07-25-target-validation/results.md); the log
is the root `stdout.log`. One run, software renderer only; the handoff's runs 2
and 3 (uncapped, and the `Miyoo Mini` render driver) have no log in the tree.
The log shows the cues and `emerald-droplets.xm` executing; 2026-10-16:
confirmed audible by the user on the device (sound effects and module playback
in orchard and coppers); not logged. The mixer's cost is still unisolated: no
`--mute` run.

**On the dev box, 2026-10-16:** `desktop-software`, a full compile with ccache
disabled, and `desktop-debug`, 35 TUs recompiled (ccache misses), both build the
current working tree with zero warnings under `WREEL_WERROR` and pass **26/26**
([results.md § 2026-10-16](../2026-07-25-target-validation/results.md)).

**Under GCC 8.3, 2026-10-16, the current tree does not build.** An incremental
build in `build/miyoomini-83` stops on two `-Wcast-align` errors in the
mirroring tests below,
[D34](../2026-07-25-cxx17-modernization/defects.md). Under `qemu-arm`, 20 of the
24 suites that do build pass; the failing four are September's.

**After the handoff, unrecorded until now: mirrored sprites and facing
(2026-10-09; committed 2026-10-16 as `1b811fd`).** The `mini` render driver no-ops
`SDL_RenderCopyEx` ([MIYOO-MINI.md](../../docs/MIYOO-MINI.md)), so a
left-facing sprite cannot be had from the renderer. `gfx::Atlas` gained a
constructor from an `SDL_Surface` that uploads a second, row-reversed copy of
the sheet, and `gfx::Mirror { None, Horizontal }` is threaded through
`AnimatedSprite::draw` and `game::Entity::mirror`. Mirroring reflects about the
untrimmed frame, and the cost is the sheet held twice, only for atlases built
that way. orchard records each actor's native facing (`native_facing()`,
`drawn_reversed()` in `orchard/actors.*`: foxy's art faces right, the opossum,
frog and eagle face left, pickups and the goal have none) and loads foxy and
the critters mirrorable. +14 test cases: `test_atlas` +8, `test_animation` +1,
`test_entities` +1, `test_orchard` +4. It was written after the handoff,
whose commit plan has since been extended to eight groups; this is group 7
([orchard-first-device-run § What was uncommitted](../2026-10-06-orchard-first-device-run/README.md#what-was-uncommitted-in-the-order-it-should-be-committed)).
Not run on a device. The 2026-10-16 desktop runs above compiled and passed its
tests. Under GCC 8.3 its library code compiles but `test_atlas` and
`test_animation` do not (D34), so their new cases have not run there;
`test_orchard`, with its facing cases, passed under `qemu-arm`. It needs D34
fixed before it goes into a bundle.

The rest of this section is the 2026-09-01 account of the layer, with its
statements of state brought up to date.

**`game::Action` shipped and has been deleted** — it is the one row below that
went backwards. It defines orchard's verb vocabulary inside a module whose
charter excludes exactly that, and underneath that it is a rename rather than an
abstraction. Decision 12 recorded that diagnosis and proposed reworking it into
`game::InputBindings<Verb>`; **decision 14 supersedes that remedy** — the row it
occupies belongs to `rig`, the verbs belong to whichever executable declares
them, and `game/` has no input line at all. Decision 12 is left in place with its
reasoning rather than rewritten, because most of it was right.

The deletion is committed, as `62714da` on PR #18. `include/game/action.hpp`,
`game/action.cc` and `tests/test_action.cc` are gone, along with their entries
in `game/CMakeLists.txt` and `tests/CMakeLists.txt`. Two charter changes went
with them, both argued under decision 14: **"input actions" is out of the
belongs-here list**, and the paragraph headed "THE rig CLAUSE EXCLUDES A JOB,
NOT A NAME" went with it, since it existed only to reconcile that entry with
`wreel::rig` sitting on the does-NOT list. `wreel::rig` also came off
`wreel_game`'s PUBLIC link line — the only reason recorded for it was
`action.hpp` including `<rig/input.hpp>`, and the one remaining `rig::` mention
anywhere under `game/` or `include/game/` is `rig::asset_path` inside a usage
example in a comment. The module's dependency line is now
`game -> gfx -> util -> posix`.

Both desktop presets were green after it at **25/25** on 2026-09-01, one fewer
than the 26/26 recorded below, and `test_action` is the only test that left.
`test_orchard` has since brought the count back to 26.

| Piece | State |
|---|---|
| `game::Entities` / `Entity` | done — promoted, with the box and tag |
| `game::collide` | done — `test_collide`, 30 cases / 245 assertions |
| `game::Camera` | done — `test_camera`, 21 / 330 |
| `tools/make_sfx.py` + `data/sfx.xml` + 5 WAVs | done — sounds heard and approved provisionally |
| `game::Level` | done 2026-08-30 — `test_level`, 19 cases / 70 assertions. Decision 10's spawn check is paid |
| `game::Action` | **superseded 2026-09-01 — deleted from this layer, see decision 14.** Shipped as `test_action`, 15 cases / 78 assertions, all of which pin orchard's verbs rather than the mechanism. Decision 12 first proposed reworking it into `game::InputBindings`; decision 14 supersedes that and removes it instead. The verb table is orchard's, in `orchard/verbs.{hpp,cc}` |
| `game::Sfx`, the cues | **`game::Sfx` superseded 2026-09-15 — never built, see [decision 17](#17-gamesfx-is-not-built-the-cue-table-is-orchards).** The cue table is `orchard::Cues` in `orchard/cues.{hpp,cc}`: five WAVs, the module, and `--mute` as constructing no device at all. Wired to the rules and to the player's jump and landing edges |
| `tools/make_orchard_level.py` + `data/orchard.tmx` | done 2026-09-08 — 96×40, two visible layers and an invisible `collision` layer, 23 spawns. **The first `.tmx` in this tree that builds a `game::Level`**, proven in `test_tmx` rather than asserted |
| the art ingest, `orchard` | the ingest landed as `data/critters.{png,xml}` and `critters.anim.xml`. `orchard` is four slices in: the level and the player, the cast, the rules with the HUD and the goal, and — 2026-09-15 — the cues and the music. `tests/test_orchard.cc` was 26 cases / 180 assertions at 2026-09-15 and crosses the shipped map twice, once on geometry alone in 758 frames and once under the rules in 1204. **Both numbers are unchanged by the sound**, which is the check that says no cue was wired into the simulation. It is 30 cases by grep at `1b811fd` (26 at `3af0cb9`, the game's commit): the four "Facing" cases from 2026-10-09; assertion count not re-taken |
| mirrored sprites and facing | written 2026-10-09, committed 2026-10-16 as `1b811fd` — `gfx::Mirror`, the mirrorable `gfx::Atlas`, `Entity::mirror`, orchard's per-actor facing; see above. Group 7 of the handoff's commit plan. 26/26 on both desktop presets 2026-10-16; **does not build under GCC 8.3** (D34, open) |
| the Onion bundle entry | `App/Orchard` built under GCC 8.3 into `pkg/wreel-0.2.0-onion.tar.gz` 2026-10-06; **run once on a Miyoo Mini and played to the goal** — [results.md § 2026-10-06](../2026-07-25-target-validation/results.md). Software renderer only; runs 2 and 3 not returned |
| `orchard`'s rules | done 2026-09-14 — collection, contact, the goal and a score line. See [decision 18](#18-contact-costs-the-last-pickup-the-goal-ends-the-run) |
| `orchard`'s cues and music | done 2026-09-15 — five WAVs on five events, `emerald-droplets.xm` playing, `--mute` opening no device, and voices on the fill-rate line. See [decision 17](#17-gamesfx-is-not-built-the-cue-table-is-orchards). Not listened to when written. 2026-10-16: confirmed audible by the user on the device (sound effects and module playback in orchard and coppers); not logged. Firmware not stated; the matching orchard log is the Onion run. The mixer's cost is still unisolated |
| the sprite baseline anchor | done 2026-09-15 — `gfx::AnimatedSprite::frame_art()`, and `Entity::align_box_to_sprite()` sits the box on the art rather than on the logical frame. The frog's ±5 px hop pop is 0.00 px at all eight transitions over 400 frames, and the player's 1 px run bob and 3 px jump pop went with it. `sprites --screenshot` byte-identical; 758 and 1204 unchanged. See [decision 19](#19-the-baseline-anchor-reads-the-art-not-the-logical-frame) |

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

**This initiative is scoped to desktop and the Miyoo Mini until the ordered
devices arrive.** That narrows what this work is answerable for, not what the
project supports: `steam`, `rk3326` and `h700` remain configured presets and
remain in [docs/TARGETS.md](../../docs/TARGETS.md), and nothing here may be
written on the assumption they are gone. Two things follow while the narrowing
holds. The unrun-cross-preset gap above is only worth paying down for
`miyoomini`, which is also the one target `desktop-software` already stands in
for. And every device measurement this document asks for — the fill-rate run in
decision 4, the 24-channel mixing load under Risks — is a Miyoo Mini measurement,
because it is the only device there is. The condition is the hardware arriving,
not a date; when it does, the matrix widens back with no decision needed.

As of 2026-09-01, and it was the same gap every snapshot before this one had:
**no line of `game/` had run on ARM or under GCC 8.3.** That is no longer true;
see the end of this passage. Both green presets are
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
first time any of this tree has executed under the device compiler. It said
nothing about `game/` — at the time `test_tmx` covered `loaders/` only, and no
`game/` test was built or run there — and it is not a cross-preset pass. Two
facts, and they must not blur: one binary ran under GCC 8.3, and the cross
presets remain unrun.

**That binary now reaches further than it did, and nobody has re-run it.** Since
2026-09-08 `test_tmx` links `wreel::game` for the `data/orchard.tmx` cases, so
the same GCC 8.3 / `qemu-arm` run would exercise `game::Level` and part of
`game::collide` as well. That is a route `game/` did not have; it is also
entirely unexercised, because the run has not been repeated. Nothing about
`game/` on ARM or under GCC 8.3 has changed as a matter of fact — only what a
future run would cover.

**Since then `game/` has run on ARM under GCC 8.3 three times: once on the
device and twice under qemu. The device run has a log in this tree; neither
qemu run does.**

- **On the device, 2026-10-06** (build date): `orchard` links `wreel::game`,
  was built under GCC 8.3 in the container, and played to the goal on a Miyoo
  Mini — [results.md](../2026-07-25-target-validation/results.md). That
  exercises `game::Level`, `collide`, `Camera` and `Entities` on hardware, as
  one program; it runs no test.
- **Under `qemu-arm`, 2026-09-21**: the whole suite, built under GCC 8.3 in
  `build/miyoomini-83`, passed **22 of 26**, `test_orchard` 26/26 among them.
  `test_audio`, `test_playlist` and `test_renderer` failed on their premises
  (no dummy audio driver; the grafted `Miyoo Mini` render driver is selected and
  calls itself accelerated), and `test_entities` segfaulted, cause not pinned.
  **Relayed, not verified:** this is known only from the
  [handoff](../2026-10-06-orchard-first-device-run/), which read it out of the
  September session's transcript; no log of it is in the tree. The segfault is
  [D33](../2026-07-25-cxx17-modernization/defects.md#d33--test_entities-segfaults-under-qemu-arm-on-the-miyoomini-build),
  which also records a second core from 2026-10-06 stopped at the same `pc`.
- **Under `qemu-arm`, 2026-10-16**, on the current working tree, with its logs
  in a scratch directory outside the repository: the build fails on D34, so `test_atlas` and `test_animation` were excluded. 20 of
  the other 24 pass, `test_orchard` among them; the same four fail.
  `test_entities` crashes in the case at `test_entities.cc:774` and passes
  31/31 with `SDL_RENDER_DRIVER=software`. Detail in
  [results.md § 2026-10-16](../2026-07-25-target-validation/results.md) and D33.

Of the cross presets, only `miyoomini` has been run since, and into
`build/miyoomini-83` rather than its poisoned default directory; the others
have not been re-run.

Also from decision 4: **the fill-rate claim is measured on the desktop and once
on the device, and the desktop figures do not stand.** The 2026-09-13 and
2026-09-17 desktop numbers under decision 4 were taken before
`Context::flush()` existed and time the queueing of the tilemap rather than its
drawing; the handoff's flushed re-measure puts the tilemap at most of the
frame. On the device, flushed and capped: 316 tiles/frame, tilemap 2.274 ms of
16.762, 7.21 µs/tile. The orchard-against-`sunnyland` comparison the decision
turns on has not been redone with the flushed meter on either.

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
- **Loading a tracker module prints libxmp's debug trace to `stdout`, and it is
  the same stream the fill-rate line goes to.** Found 2026-09-15 while wiring
  orchard's music; **not caused by that work and not fixed by it.** The pinned
  SDL2_mixer vendors libxmp, and on a `Debug` build type — which both desktop
  presets are — its `D_()` tracing is compiled in and goes to standard output
  rather than standard error. `coppers` has had it all along: a `.mod` load
  prints 65 lines, and `emerald-droplets.xm` prints 130, because the XM has 46
  patterns and 17 instruments to enumerate. The `tiles/frame … ms/frame …` line
  results.md parses is intact and on its own line; it is simply buried. The fix
  is in `cmake/Dependencies.cmake`, not in any demo, and it is worth checking
  whether a Release-type build is already quiet before spending anything on it.
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
| `rig::Pad` | done, named `Button`s, three input paths, `down()` and `pressed()` | **nothing, per decision 14** — the binding row is rig's own, not a layer in `game/`. A demo declares its verbs and maps them itself. Per-device work and `released()` are deferred to [rig-device-bindings](../2026-09-01-rig-device-bindings/) |
| `rig::FrameClock` / `FrameTiming` | done, clamped delta | a fixed-step mode for reproducible screenshots |
| `tools/pack_atlas.py` | done, trims, verifies by rebuilding each frame, seeds an animation sidecar | nothing |
| `tools/make_tilemap.py` | done, writes `.tsx` + `.tmx`, three tile layers and one object layer | nothing — the level it did not write is `tools/make_orchard_level.py`'s, a sibling that reuses its `.tsx` |

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

Three of the pack's animation names would be wrong or useless in this game's
vocabulary. Two name something this game does not have:
`Misc/Sunnyland FX/Sprites/enemy-death/` (6 frames, 40×41) is a generic dust puff,
and `Characters/eagle/Sprites/attack/` (4 frames, 40×41) is a wing-down glide —
nothing here dies and nothing attacks. The third,
`Misc/Sunnyland FX/Sprites/item-feedback/` (4 frames, 32×32), is not wrong so much
as it describes a pack's catalogue rather than a thing on screen; it is the
twinkle a pickup leaves behind. `tools/pack_atlas.py` takes frame names from directory
names, so whatever those directories are called at pack time is what lands in
`data/*.xml` and gets quoted in every call site and test that follows.

So they get renamed on ingest — `puff`, `swoop` and `sparkle` — and the rename
happens in the copy into `data/sunny-land/`, not by patching the atlas afterward.
Renaming a frame later means touching the atlas, the sidecar, the code and the
tests at once.

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
| `Misc/Sunnyland FX/Sprites/item-feedback` → `sparkle` | 4 | 32×32 | pickup sparkle |
| `Misc/Sunnyland FX/Sprites/enemy-death` → `puff` | 6 | 40×41 | the bump reaction |

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

#### The source layout, and three things that do not follow from the table

Re-measured 2026-09-01 against `data/Sunny-land-files/`, the upstream bundle as
purchased. It is gitignored — `data/PROVENANCE.md` and `.gitignore` both say so —
so it has to be unpacked before an ingest can run, and none of the paths below
resolve in a clean checkout. Where each target comes from, under `Assets/`:

| Target | Source path | Note |
|---|---|---|
| frog | `Characters/frog/Sprites/` | `idle` and `jump`, those two only |
| opossum | `Characters/Opossum/opossum/` | flat — no animation subdirectory |
| eagle | `Characters/eagle/Sprites/` | `attack` only |
| cherry | `Misc/Sunnyland items/Sprites/cherry` | |
| gem | `Misc/Sunnyland items/Sprites/gem` | |
| puff | `Misc/Sunnyland FX/Sprites/enemy-death` | |
| sparkle | `Misc/Sunnyland FX/Sprites/item-feedback` | |

**The opossum has no per-animation directory.** The frog and the eagle are
`Sprites/<animation>/`; the opossum is six frames sitting flat in
`Opossum/opossum/` as `opossum-1.png` … `opossum-6.png`. Every other animation
name in this ingest is transcribed from a source directory, and this one has to be
**invented** — there is no directory to take it from. It is `opossum-walk`: the
six frames are a ground patroller's single locomotion cycle, and the name should
say what the animation is rather than repeat the actor, which is what a bare
`opossum` would do once it sits in an atlas beside `frog-idle`.

**There are three renames on ingest, not two.** `enemy-death` → `puff` and
`attack` → `swoop` are the two the naming paragraph above argues. `sparkle` is the
third and has the same justification — a pack-generic name for something this game
uses specifically — but nothing in the source is called `sparkle` at all, so it is
a rename rather than a transcription and the ingest step has to be told. It was
previously carried only in the *Role* column above, which is a description of what
the art is for and not an instruction.

**Eight animations is the entire vocabulary, and it is thinner than "The game"
reads.** `frog-idle`, `frog-jump`, `opossum-walk`, `eagle-swoop`, `cherry`, `gem`,
`puff`, `sparkle`. The frog has two; the opossum and the eagle have one apiece, so
two of the three animals have a single pose with no idle, no turn and no second
gait. That is workable and it is not an obstacle — "The game" above already has the
animals following fixed paths and ignoring the player, and an animal that never
reacts never needs a pose for reacting — but it constrains how the animals can be
written, and it is much less than a reader of "Three animals wander it" would
otherwise assume. It belongs
here rather than being discovered while hand-tuning `critters.anim.xml`. Foxy is
the exception: it already has more than the game needs.

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

**Checked 2026-09-13.** Both uncapped (`--fps 0 --seconds 3`) on
`desktop-software`, 640x480, software driver, on an x86-64 dev box:

```
orchard      320 tiles/frame   1.598 ms/frame   626 fps
sunnyland   1296 tiles/frame   3.244 ms/frame   308 fps   (sprites --tilemap)
```

A quarter of the tiles and half the frame cost, against a fixture built to be
the worst case. Note the per-tile figure moves the *other* way — 4.99 us/tile
against 2.50 — because the fixed per-frame work (the clear, the sprite, the
present) is now spread over a quarter as many tiles; the per-frame number is the
one the budget is written in. **This is a desktop measurement and not the
answer**: the claim is about two Cortex-A7 cores, and nothing here has run on a
Miyoo Mini. A capped run reports the cap and not the cost, so `--fps 0` is
required to measure anything.

**Re-measured 2026-09-17 with a corrected instrument, and the conclusion above
is narrower than it looked.** Two defects were found in the meter, neither in
the tile count:

- `ms/frame` was `1000 / FrameClock::fps()`, and `fps()` is an exponential
  moving average weighted 0.05 on the newest frame. It described the last
  handful of frames, not the run — five runs of identical work spread 0.898 to
  1.709 ms. It is now the run mean, `elapsed() / (frames() - 1)`.
- `us/tile` divided the *whole frame* by the tile count, so it charged the
  clear, the entities and the per-frame HUD rasterisation to the tilemap. The
  demos now time `TileMap::draw` directly and divide that.

`tiles/frame` was never wrong. It counts blits and clips to the target, which
`tests/test_tmx.cc` already pinned. The three disputed figures were three
window sizes of the same sound meter: 320/318 at 640x480, 567 at 1024x768
(reproduced exactly), 903 fullscreen — where a 1920x1080 panel holds all 96x40
cells of the map, so "tiles drawn" legitimately equals "filled cells". The
report line now carries `view WxH` and a min/max so this cannot recur.

Same preset, same 640x480, `--fps 0 --seconds 3`:

```
orchard      318 tiles/frame   1.06-1.35 ms/frame   0.063-0.082 ms tilemap   0.20 us/tile
sunnyland   1296 tiles/frame   2.16-2.20 ms/frame   0.144-0.146 ms tilemap   0.11 us/tile
```

**Corrected 2026-10-06: the figures below were taken on a batching renderer and
time the queueing of the tilemap, not the drawing of it.** With the queue flushed
at each boundary the tilemap is most of the frame and `us/tile` is roughly ten
times what is printed here. The conclusion drawn from them does not stand as
written; see
[orchard-first-device-run](../2026-10-06-orchard-first-device-run/), *Where the
frame goes*. Not yet re-measured in this document.

**The tilemap is about 6% of the frame in both.** The backdrop orchard omits is
978 extra tiles and costs roughly 0.08 ms — not the ~1.6 ms gap between the two
demos, which is the sprite grid and the HUD. So decision 4's architectural
reasoning stands (a covering layer *is* a full-screen blit, and it does cost
linearly in coverage) but the claim "this level is cheaper **because** it has no
backdrop" is not what the numbers show at this resolution on this hardware: at
0.08 ms the backdrop is affordable here, and what separates the two demos is
everything else in the frame.

What remains genuinely unestablished is the part that always was: two Cortex-A7
cores with no GPU, where a full-screen blit competes with the mixer for the same
memory bandwidth and the ratios need not resemble these at all. That is the
user's to run.

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

### 12. The input layer binds inputs; it does not define actions

> **Superseded the same day by decision 14, in its remedy only.** Read this
> section for its diagnosis, which stands and is cited by decision 14, and not
> for what it proposes to build. `game::InputBindings<Verb>` was never written
> and will not be: the row it occupies turned out to be `rig`'s, so
> `game::Action` is deleted rather than reworked. The reservation of the name
> `Action` survives, and is the part of this decision with the most in it.
>
> Kept in place unedited below rather than rewritten, per this project's habit
> of superseding a decision rather than erasing it. A decision reworded to look
> as though it always said the current thing takes its reasoning with it, and
> the reasoning is most of what this one is worth.

Recorded 2026-09-01, after `game::Action` shipped in PR #17 and was read again.
Two things are wrong with what landed, and the second is the one worth a section.

**The enum is orchard's vocabulary, inside a module whose charter excludes
exactly that.** `game/CMakeLists.txt` puts "the rules of any ONE game, which live
in that game's executable directory" on the does-NOT list, and adds: "A demo's
own rules — what its pickups are worth, when it is over — belong to the demo.
What lands here is what a second demo would otherwise copy." `Action::Pickup` is
orchard's verb. The tell was already in the header, which justified the *absence*
of Up/Down with "nothing in the design climbs or crouches under player control" —
one game's design settling a shared module's contents, which is the dumping
ground the charter's own preamble warns about.

**The deeper error: the class is a rename, not an abstraction.** There are three
levels here, not two:

```
transport       SDL scancode, joystick button index    rig's job, and done
named switch    rig::Button::A, Button::Up             device-independent, but
                                                       still "which switch"
intent          Jump, Dash, Hadouken                   what the player meant
```

`ActionMap` maps intent-names onto switch-names through a static bijection with
no state and no time, which never leaves the middle row. There is a clean test
for whether something is really an action layer: **can it produce a signal that
no single switch state can produce?** An instantaneous 1:1 lookup fails by
construction — and so would multi-bind, because "A or Up" is still a set of
switches OR'd together.

**What actually produces actions is a recognizer over input history** — a
quarter-circle, a double-tap dash, a charge. Those are genuinely intent-level and
they genuinely recur across games, which is this project's own test for what
belongs in a shared module.

**Decided anyway: not now, and not here.** `game/CMakeLists.txt` says the module
was created on an actual second consumer "rather than a guess that a second
consumer would eventually appear". Orchard is walk/jump/pickup avoidance — no
combo, no charge, no dash — and it does not exist yet. Recognizers are per-game
until something proves otherwise. The model is right and the build is premature;
those are not in tension.

So:

- The surviving class is `game::InputBindings<Verb>` in
  `include/game/input_bindings.hpp`, a template over the game's own verb enum.
  It is header-only, and `game/action.cc` disappears. The name is deliberately
  *input*-binding rather than *button*-binding, because an axis or a pointer may
  be bindable later and should not force a rename.
- **The name `Action` is held in reserve** for the intent-level thing rather than
  spent on an alias. Reserving it is most of the value of this decision.
- The scope stays at one level: no action or event history, no per-frame tick, no
  lifecycle. It stays a pure stateless lookup, and that is deliberate — a
  recognizer will want a binding table as its *input config*, and a pure function
  is far easier to absorb into one than a ticked object is.

**One correction worth recording, because the obvious example misleads.**
Tap-versus-hold — "a quick release is a strong jump, a slow one weak" — is
usually the wrong thing to model as two actions. You cannot know at press time
that a tap was a tap, so firing on release starts the jump after the player let
go, which feels broken. The standard solution is not a recognizer at all: one
`Jump` fires on the edge, and height is modulated by polling `down(Jump)` through
the ascent. Motion inputs, not tap/hold, are the case that actually needs
history.

### 13. Multi-bind is deferred, and it is not free

> **Relocated by decision 14, not superseded.** Every conclusion below is
> unchanged; only its address is. The `released()` analysis is a finding about
> `rig::Pad`'s interface, and decision 14 moves the whole binding row to `rig`,
> so this belongs with that work — carry it into
> [rig-device-bindings](../2026-09-01-rig-device-bindings/). The one clause that
> does not travel is the last: `bind()`'s replace-semantics is now advice to
> whoever writes orchard's own table, since there is no shared class to give a
> signature to.

One verb reads one button. Binding `Jump` to both A and Up is a real want —
comfort and accessibility, dpad *or* face button — and it is not here.

**The first argument for doing it now was wrong.** It looked nearly free, since
decision 12 changes the table's element type and `bind()`'s signature anyway. It
is not, and the reason is worth keeping because it is not obvious.

`down()` for a multi-button verb is a trivial OR. `pressed()` is not. The
`Source` concept offers `down(b)` and `pressed(b)`, and **`pressed` carries
information only about buttons that are currently down.** A button released this
frame reads `down=false, pressed=false`, indistinguishable from one that has been
up for a minute. The verb's previous-frame state is therefore not reconstructible
from the current frame, and stateless multi-bind edge detection is **impossible**
under the concept as documented. That is not a gap in some predicate; it is a
hole in the interface's information content.

The counterexample is worth writing down, because the plausible-looking predicate
`(any bound pressed) && !(any bound down-but-not-pressed)` survives casual
inspection and is wrong. Bind `Jump` to A and Up. Hold A on frame *f-1*. On frame
*f*, release A and press Up. The verb was down in both frames so no edge is owed
— but that predicate reports one, and a spurious edge is precisely the one-shot
cue retrigger that the Risks section's "pressed(), not down()" note exists to
prevent.

Two ways to buy it, neither free:

- **State in the binding layer** — one previous-frame bit per verb plus a
  mandatory per-frame tick. This reintroduces the lifecycle decision 12 scoped
  out, for the smallest payoff available, and makes the class harder to absorb
  into a recognizer later.
- **Widen `Source` with `released()`**, after which
  `was_down(b) = (down(b) && !pressed(b)) || released(b)` and the binding layer
  stays stateless. This is the better shape and the recommended route when the
  want arrives.

**What `released()` would cost, checked rather than assumed.** `rig::Pad` keeps
`_down[]` and `_pressed[]` and no previous-frame array; edges are recorded at the
transition in `Pad::set()`, which is the single funnel for all three input paths.
`_released` is the exact mirror of `_pressed` — `if (!is_down && _down[index])` —
plus a `memset` in `begin_frame()` and an accessor. Purely additive, so nothing
using `rig::Pad` today breaks. `down`/`pressed`/`released` is also the standard
triple, so this completes an existing idiom rather than inventing generality.

**Why it is still deferred:** nothing consumes the binding layer at all yet, and
pulling a `rig/` API change into a `game/` scope cleanup is creep by this
project's own standard. The mis-enumerated-pad justification does not survive
either — a wrongly-enumerated pad is fixed by *rebinding*, not by binding two
buttons.

`bind()` should therefore take replace-semantics now, leaving room for an
`add_binding()` beside it later, so the arrival of multi-bind does not rename
something games already call.

### 14. The binding row belongs to `rig`, and `game::Action` is deleted

Recorded 2026-09-01 by the user's direct call, hours after decisions 12 and 13
and before any of decision 12's code was written. It supersedes decision 12's
**remedy** and leaves its diagnosis standing.

**The substance of the call, as recorded.** Nothing that is specific to a *game*
belongs in the shared library layer. The `rig` layer knows the inputs. A binding
is a shorthand for different **devices** aliasing — maybe even 1:1 — onto the
specific inputs a device actually has: an Anbernic H700 may map a LEFT
differently from a Miyoo, and a Miyoo differently from a TrimUI. **The game
offers the context of what the input actually is.**

Decision 12's own three rows, with an owner written against each:

```
transport      SDL scancode, joystick button index, hat 0   rig — done
device alias   this device's LEFT -> rig::Button::Left      rig — three paths,
                                                            see below
meaning        Jump, Pickup                                 the demo, in
                                                            orchard/ — and the
                                                            map from it, until
                                                            a second consumer
```

`game/` gets nothing on any row. Two different reasons are doing that work and
they are not equally strong: the verb **vocabulary** is excluded by layer, and
the **mechanism** that would map it — which, parameterised over a game's own
enum, would carry no vocabulary at all — is excluded by *timing*, because there
is no second consumer to shape it against. "Why the mechanism is not kept behind
a template" below is where that second reason is argued, and it is the one that
would be reopened first.

So `game::Action` is **deleted** from the shared
layer rather than reworked into `game::InputBindings<Verb>`, and the
verb-to-button table becomes orchard's when orchard exists. Nothing migrates with
it: grepping `ActionMap`, `action_name` and `game::Action` across the tree finds
`include/game/action.hpp`, `game/action.cc` and `tests/test_action.cc` and no
other consumer.

That is the whole route to "delete", and it is short: a table of
`Left/Right/Jump/Pickup` is one game's vocabulary whatever row it sits on, and
one game's vocabulary does not go in the shared layer.

#### What rig actually does on the alias row

- **`SDL_GameController`, when SDL recognises the device.** The alias is SDL's
  own mapping database. Done, and not this project's code.
- **The keyboard — which is where the Miyoo Mini's alias already lives, and it
  is not a guess.** `docs/MIYOO-MINI.md` §6.3 and the comment on `from_key` in
  `rig/input.cc` both record that the Miyoo's SDL2 video driver reads
  `/dev/input` and translates Linux `KEY_*` codes straight to keysyms, so **no
  joystick is ever enumerated at all**: the handheld logs "no pad attached,
  keyboard only" and every face button arrives as an ordinary key press.
  `from_key` therefore carries a second, device-specific group of cases taken
  from Onion's `src/common/system/keymap_sw.h` — A = `SPACE`, B = `LCTRL`,
  X = `LSHIFT`, Y = `LALT`. How far that is confirmed is worth taking from
  [docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md) § 6.3 rather than restating:
  the same binary, with these bindings compiled in, took input correctly on
  **stock** firmware on 2026-07-30, and that is "one run — enough to stop
  treating the bindings as Onion-specific, not enough to have verified every key
  on stock." **rig is already doing per-device aliasing, for the one handheld
  that has actually run this code.** The alias row is not unstarted; it is
  started, in `rig`, for one device, inline in `from_key`.
- **A raw `SDL_Joystick` otherwise**, buttons by index and the dpad from hat 0.
  This one *is* a guess and is logged as one. Note what follows from the point
  above: no device tested so far has taken this path, because the Miyoo takes the
  keyboard path instead. The documented guess in the Motivation section above is
  still a guess and is still unexercised.

**A recorded decision already owns the remedy for that guess, and this one does
not overturn it.** [results.md](../2026-07-25-target-validation/results.md),
2026-07-27: "Once a device reports its GUID, the right fix is an
`SDL_GameControllerAddMapping` string rather than more guessing." That stands.
Per-device work in `rig` starts from adding mappings to SDL's own database, not
from inventing a second alias table beside it — and the tension is live, because
`from_key`'s Onion group already *is* a second alias table, for the path SDL's
database cannot reach. Reconciling those two shapes is
[rig-device-bindings](../2026-09-01-rig-device-bindings/)'s question, not this
document's, and that work is **deferred rather than scheduled**.

**"rig knows the inputs — keyboard, gamepad, stick" overstates rig as it stands.**
There is no axis handling in `rig/input.cc` at all: it reports
`SDL_JoystickNumAxes` in a log line and handles no `SDL_JOYAXISMOTION` or
`SDL_CONTROLLERAXISMOTION` event. `rig::Pad` is twelve buttons and a quit flag.
Decision 12 chose *input*-binding over *button*-binding so an axis could be bound
later without a rename; that foresight moves to `rig` along with the row, and it
is foresight about a capability rig does not have yet.

#### What survives from decision 12, and what does not

Survives, and is why that section is kept rather than deleted: that
`Action::Pickup` is orchard's verb inside a module whose charter excludes exactly
that; that the tell was already in the header, which justified the *absence* of
Up/Down by appeal to one game's design; that a static bijection with no state and
no time cannot produce a signal no single switch state can produce, and so is not
an action layer; that what would be one is a recognizer over input history; and
that **the name `Action` is held in reserve** for that rather than spent on an
alias. Decision 12 diagnosed correctly and then kept the patient.

Does not survive: the remedy. `game::InputBindings<Verb>` is not written,
`include/game/input_bindings.hpp` does not exist and will not, and decision 12's
closing bullet list of that class's properties is void. Decision 12's correction
about tap-versus-hold — that you cannot know at press time that a tap was a tap,
so one `Jump` fires on the edge and height is modulated by polling `down(Jump)`
through the ascent — is about how a *game* handles input and is unaffected by
where the table lives.

#### The charter in `game/CMakeLists.txt` is wrong and comes out

Its belongs-here list includes **"input actions"**. Under this decision that is
not this module's business, and the line goes. So, probably, does the paragraph
headed "THE rig CLAUSE EXCLUDES A JOB, NOT A NAME": it exists only to reconcile
"input actions" being in the belongs-here list with `wreel::rig` being on the
does-NOT list, and once the first is gone it argues against nothing. Named here
because it is a charter change and a substantive one, not the mechanical
consequence of deleting a file. The edit itself belongs to the deletion task.

Worth checking at the same time and not assumed here: `target_link_libraries`
gives `wreel::rig` as PUBLIC and says it is so "for that reason only" — because
`include/game/action.hpp` includes `<rig/input.hpp>`. If that was genuinely the
only reason, the dependency comes out of the link line with the header.

#### This is the answer `entities.hpp` already gave, not a second one

Decision 8 settled the same question for the behaviour tag, and settled it
without a template: the module never interprets the tag, it is a plain `int` with
`no_behaviour = -1`, and the demo declares its own `enum class Behaviour` and
casts once at the spawn site. The vocabulary belongs to the demo. Deleting the
verb enum makes `game/` consistent with that precedent rather than offering a
second, template-shaped answer to the same question — a reader meeting both would
otherwise have to work out which one to copy.

`include/game/action.hpp` had already reached the same place for the device half
of the problem, and the paragraph is worth carrying out of a file that is about
to be deleted:

> THE DEVICE IS MIS-ENUMERATED… the right fix is rig::Pad's table — fixing it
> there fixes every demo at once and keeps the vocabulary honest. Fixing it here
> would leave `Button::A` meaning the wrong thing for the next program.

This decision completes that reasoning rather than overturning it: the header
assigned the device row to `rig` and kept the verb row, and what changes here is
only where the verb row lives.

#### Why the mechanism is not kept behind a template

The vocabulary has to go on the argument above. That leaves a separate question,
because a `game::InputBindings<Verb>` parameterised over the game's own enum
would carry no vocabulary at all — it is `Entity::behaviour`'s pattern with the
opacity moved from a runtime `int` to a template parameter, and `Entity::behaviour`
is right where it is.

The rule that separates them is this module's own, quoted in decision 12 above.
`game/CMakeLists.txt` records that `wreel_game` was created on "an actual second
consumer… rather than a guess that a second consumer would eventually appear."

- The entity store **has** consumers: decision 5's trigger fired as written, and
  `sprites` and four `game/` tests use it today.
- The verb map has **none**. Its only prospective consumer is `orchard`, which
  does not exist. Nothing in the tree includes it but its own test.

So the mechanism is not built here yet for the same reason the module itself was
not built early: one consumer does not say where the seam goes. Decision 5 put it
directly — "two known users do not tell you where the seam goes, and the version
that guesses is the one that has to be undone."

#### The cost this decision pays

The charter sentence that supplies that rule continues: **"What lands here is
what a second demo would otherwise copy."** A verb-to-button map in `orchard/` is
exactly what a second demo would copy. This decision takes that duplication in
exchange for not guessing at a shared shape before there is a second consumer to
shape it against.

The cost falls due when a second demo wants one. The move then is to promote it
out of `orchard/` the way `sprites::Entities` was promoted — on the actual second
consumer, which will by then be able to say what the interface needs to be. If
what comes back looks like decision 12's `InputBindings<Verb>`, decision 12 was
right about the shape and early about the timing.

**Nothing moves today.** "The verb table becomes orchard's" is a promise, not a
relocation: `orchard/` does not exist. What this decision does now is delete three
files and a charter line; the replacement is written when the demo is.

#### Verification

Nothing was built, tested or run to *reach* this decision, nor decisions 12 and
13 before it: it is a reading of the tree plus a call by the user. The deletion
it mandates has since been built and tested — 25/25 on `desktop-software` and
`desktop-debug` with zero warnings, and that is now the last measurement of any
kind. **No line of `game/` has run on ARM or under GCC 8.3, and nothing has run
on a device.**

### 15. Frame names are namespaced by prefixing in the copy

Settled 2026-09-01, closing the question opened 2026-08-14 that the art ingest and
everything behind it were waiting on.

**The source directories are copied into `data/sunny-land/` under their namespaced
names** — `frog-idle/`, `frog-jump/`, `opossum-walk/`, `eagle-swoop/` — and
`tools/pack_atlas.py` derives the animation names from them unchanged. Separator
is a hyphen, matching `wall-grab` in `foxy.anim.xml`.

The name *is* the directory name: `pack_atlas.py` builds every frame id as
`f"{animation_dir.name}.{index:03d}"`. Decision 1 settled that the frog, opossum,
eagle and the four items share one sheet, so those directory names all land in one
namespace, and the frog and the opossum both arrive with an `idle`. Namespacing is
what the one-sheet decision costs, and this is where it gets paid.

**The prefix is free at the point it lands.** The copy is already renaming and
renumbering — the source frames are 1-based and unpadded (`frog-idle-1.png`),
while `pack_atlas.py`'s verify pass opens `<animation>/{index:03d}.png`, which is
why `data/sunny-land/foxy/` holds `000.png` and not the names it was copied from.
Prefixing is the same edit to the same step, and no tool changes: `pack_atlas.py`
is untouched, so the property `data/PROVENANCE.md` records for `foxy.{png,xml}` —
that regenerating produces identical bytes — is preserved by construction rather
than by retesting.

**This does not reopen decision 1.** One atlas per actor would make the collision
impossible instead of naming around it, and it would not contradict decision 1's
cost model — that model records binds as *not* what is billed for, so splitting
the sheet costs nothing it measures. It is declined anyway, and the reason is not
a cost: decision 1 is settled, its subject is how many sheets there are, and
wanting shorter names is not new information about that. Splitting would also buy
five files to keep in sync where there is one. A settled call gets reopened by
evidence, not by a naming convenience.

**The permanence decision 1 warns about is real and bounded here.** Decision 1
declines to repack `foxy` partly because `data/foxy.xml` is a live test fixture,
and a frame name is generally expensive to change once anything quotes it. For
`critters` that exposure is smaller than it looks, because `AnimationSet` is built
to make it so: it resolves a name to an `Index` once and then uses the `Index`, and
`AnimatedSprite::play()` takes a `const Animation&` rather than a string. So the
names appear in `AnimationSet::find()` calls at setup — `set[set.find("frog-idle")]`,
the spelling `include/loaders/animations.hpp` documents — and not in the per-frame
path. A rename touches `critters.anim.xml` and orchard's setup, which is one place
each, rather than every site that plays an animation.

That is a bound, not an exemption. Eight names chosen now are eight names the
sidecar carries for good, and the reason to get them right at ingest stands.

**The rule, which is the part that generalises:** *every animation name is unique
within its atlas and reads as what it is.* That yields `frog-idle`, `frog-jump`,
`opossum-walk`, `eagle-swoop`, `cherry`, `gem`, `puff`, `sparkle` — the four
single-animation items carry no prefix, because they have nothing to be
distinguished from and `cherry-cherry` names nothing that `cherry` does not.

The rule is stated rather than a blanket "prefix everything" because the next
person adding to `critters` needs to know which way to go, and the two cases go
different ways. An actor with more than one animation is prefixed; a
single-animation item whose own name is already unique and descriptive stays bare.
Where they meet — a second item that happens to collide, or an actor whose one
animation needs saying — uniqueness decides first and legibility second.

### 16. The sprite-to-box offset is a pair of operations, not a field

Settled 2026-09-13. `entities.hpp` records that the entity store's interface gets
defined "when a second consumer turns up and says what the interface actually
needs to be, not before, on a guess about what that will be." `orchard` is that
consumer, and what it said is this: `Entity::draw()` draws the sprite at
`(x, y)` and `box` is relative to `(x, y)`, so **`(x, y)` means two things at
once** — the sprite's origin and the box's anchor. Every actor whose art is
bigger than its collider carries the difference at the call site, which in
`orchard/demo.cc` was nine lines reaching into the atlas by hand, and six more
actor types were queued behind it.

**No field, and no mode.** The Risks section below names "`game::Entity` becomes
the place every later need gets appended to, one field at a time" as decision 5's
residual risk, and a stored `offset_x/offset_y` or an `anchor` enum is exactly
that. What landed instead is two operations over members that already exist:

- `gfx::AnimatedSprite::frame_width()` / `frame_height()` — the *logical*
  (untrimmed) size of the frame showing now. The atlas and the current index are
  both private, so before this a caller wanting to place a sprite had to be
  handed the atlas separately and index it itself; that is the arithmetic
  `orchard` was doing. Zero when there is nothing to show, so "no answer yet" is
  distinguishable from a 0×0 frame.
- `Entity::align_box_to_sprite()` and `Entity::place_box(world_x, world_y)`.
  The first writes `box.x`/`box.y` from the frame size by the one placement
  convention — **centred horizontally, bottom edges flush** — and touches only
  the offset, never the extent. The second is the exact inverse of
  `world_box()`, and exists for the same reason it does: so a caller that has
  resolved where the *box* must be does not subtract the offset by hand and get
  the direction wrong.

Methods rather than free functions, unlike `game::collide`: `collide` is free
functions because it has no state and its arguments are a tile layer and a box,
whereas both of these read and write an entity's own members and `place_box()`
is literally `world_box()` run backwards. Splitting the pair across the two
spellings would have been the arbitrary choice.

**Both are opt-in, and that is load-bearing.** `sprites`, the first consumer,
never sets a box at all. Anything that made `draw()` offset by the box
unconditionally would have moved every entity in that demo by half a frame.
`draw()` is unchanged; `align_box_to_sprite()` is a no-op when the sprite has
nothing to show, so an unresolved frame leaves the spawn's box where the spawn
put it rather than shifting the collider half its own width.

**A premise in the report that prompted this was wrong, and checking it changed
what was viable.** The report said the offset "has to be recomputed per frame
... because trimmed frames differ in size." `AtlasFrame::width()`/`height()`
return the *untrimmed* size, and no animation in either shipped atlas has a
non-uniform logical frame size — `foxy` is 33×32 across all 36 frames, and
`critters` has six sizes, one per animation and uniform within each. So the
offset is constant per animation and there was never a per-frame constraint to
design around. `orchard` still calls `align_box_to_sprite()` every step, because
deriving from the frame showing now is always right and never stale — across
animations the size *does* change, which is what would bite a value computed
once at spawn.

**Proof, because this moves arithmetic and must not move a pixel:**
`orchard --screenshot` at 30, 90, 150, 240 and 400 frames is byte-identical
before and after on `desktop-software`, and at 240 frames on `desktop-debug`
too; `sprites --screenshot` is byte-identical as well, which is what covers the
first consumer through the `AnimatedSprite::draw()` refactor. A deliberate
one-pixel error in the bottom-align was injected and *did* change the hash, so
the check is sensitive rather than vacuously passing. 26/26 on both presets,
plus 10 new cases. Nothing has run on a device or under GCC 8.3.

**One thing found on the way, documented rather than changed.** `orchard` writes
`Entity::x`/`y` from the resolved collision position and then calls
`Entities::update(delta)`, which integrates velocity into position
unconditionally. It is correct today only because the avatar's velocity stays
zero. An actor that sets both gets both — the resolved position is *added to*,
not overwritten, so the symptom is a drift rather than an obviously ignored
write. `entities.hpp` now says which of the two owns position: an entity the
store moves sets a velocity and does not write `(x, y)`; an entity collision
moves writes `(x, y)` through `place_box()` and leaves velocity at zero.

### 17. `game::Sfx` is not built; the cue table is orchard's

Settled by the operator, implemented 2026-09-15. It sits above decision 18 by
number and lands after it in time, which decision 18's own opening paragraph
already explains.

**The reasoning is the charter's, not an argument against a proposal.**
`game/CMakeLists.txt` puts **"anything that makes sound (`wreel::audio`)"** on
its does-NOT list, and separately puts **"the rules of any ONE game, which live
in that game's executable directory"** there too. A `game::Sfx` would be on the
wrong side of both clauses at once: the cue names are `jump`, `land`, `pickup`,
`bump` and `level-complete`, which is orchard's vocabulary and nobody else's,
and holding `audio::Sound` handles would put `wreel::audio` on a dependency line
that reads `game -> gfx -> util -> posix`. The module has just finished getting
`wreel::rig` off that line under decision 14; adding `wreel::audio` to it in the
next slice would be undoing that work while the reasoning was still warm.

**This is decision 14's reasoning arriving a second time, and the second arrival
is the part worth recording.** `game::Action` was "named `rig::Button` handles"
carrying orchard's verbs. `game::Sfx` was "named `audio::Sound` handles" carrying
orchard's cues. They are the same error with one noun changed, and — this is the
bit — **they entered the plan by the same route**: the **Proposed shape** sketch
below, written before the charter existed, quoted afterwards as though it were a
specification. That section already records how `game::Action
Pickup/Jump/Left/Right` got into the tree: "not by anyone arguing for it, but by
a sketch being quoted as a specification." It has now happened twice.

So the guard is stated once here rather than being rediscovered a third time:
**a line in Proposed shape is a sketch, and the charter in
`game/CMakeLists.txt` is the authority.** Where they disagree the charter wins,
and the sketch gets corrected — which is what the `game::Sfx` line below now
records rather than silently losing.

#### The cues are named for the WAVs, not for the rules that fire them

The rules slice left a seam of three — `Cue::Collect`, `Cue::Bump`,
`Cue::Victory` — named after the rule that fired. The five files are named
`jump`, `land`, `pickup`, `bump`, `level-complete`. Those are two vocabularies
for one set of things, and this slice renamed the enum rather than adding two to
it.

The WAV names won because with five cues and five files, matching names make
`cue_name()` the filename stem: `orchard::Cues` loads by looping over the enum
and there is exactly one place that says which sounds exist. Naming for rules
needs a second table from rule to file, and two tables that have to agree is the
shape that drifts without anything failing. The cost is that renaming a WAV
renames an enumerator, which is a compile error — the cheap kind of wrong.

#### `jump` and `land` come off edges the player does not report

Three of the five are free. `pickup`, `bump` and `level-complete` come off
`orchard::Rules`, which is already once-per-event by construction — decision 18
argues each of the three latches, and the cue is simply the `true` return.

`jump` and `land` have no rule behind them at all, and this is the constraint the
Risks section names: **effects are one-shot and fire on edges, once per event,
never once per frame of the state that caused them.** `Player::on_ground()` is a
*state*. A player standing still is on the ground on all sixty frames of every
second it stands there, and a `land` cue taken from it directly restarts the same
chunk sixty times a second. On this dev box — 44100 Hz, 16 voices, a forgiving
buffer — that is inaudible as a defect. On the handheld, 22050 Hz with 8 voices
and a 2048-sample buffer, it is a starved mixer.

`orchard::CueEdges` turns the two states into the two events, and **the jump edge
is taken from the vertical velocity turning upward, not from the ground going
away.** That is not a stylistic preference. `coyote_time` exists precisely so a
jump can fire several frames *after* the ground has gone, by which point the
ground transition has already been spent on the walk-off; a ground-based detector
misses every coyote jump. `Player::step()` applies gravity unconditionally and
zeroes the velocity at both a floor and a ceiling, so a launch is the only thing
in it that can turn the velocity upward — which makes the velocity sign crossing
exact rather than approximate.

Pinned in `tests/test_orchard.cc` rather than asserted: 300 frames of standing
still produce **zero** land cues, one jump produces exactly one of each, a
coyote-time press produces its jump cue, and the shipped 758-frame crossing
produces 6 jump and 10 land cues — two orders of magnitude below the frame count,
which is what "one-shot" means as a number.

#### `--mute` silences by opening nothing

`include/audio/device.hpp` states that a program constructing no `Device` does
not initialise the audio subsystem at all. So `--mute` constructs no `Device`,
loads no chunk and loads no module, rather than setting a volume to zero. A timed
run and a screenshot run are then not audio tests in any residual sense, and the
`--seconds` pair with and without `--mute` is the *with music / without music*
measurement the Risks section asks for.

Checked rather than assumed: `--screenshot` at 1400 frames is byte-identical
across two sounding runs and a muted one.

### 18. Contact costs the last pickup; the goal ends the run

Settled 2026-09-14, implementing the rules slice. **Numbered 18 and not
17**: 17 is reserved for the cue table, which the operator settled separately and
which lands in the slice after this one. The numbers are identifiers and not a
chronology, so this one landing first is not a reason to renumber that one. "The game" above already
settles most of this and it is quoted rather than re-argued: *Touching one costs
you your last pickup and sits you down dizzy for a moment.* Avoidance, not
combat — nothing kills, there is no health, and the only fail state is losing
progress you can re-earn. What follows is the four things that paragraph does
not say, each of which had to be decided to write `orchard/rules.cc`.

**A bump is edge-triggered, per animal, and that is the whole of the
anti-loop.** A bump lands on the frame an overlap *begins* and on no other, so
sixty frames spent standing inside an opossum is one bump — the same
press-versus-held distinction `Intent` already makes for the Jump button, and
the thing most likely to be silently wrong because every frame of it draws
correctly. The latch is kept whatever the phase, which is what settles the case
that would otherwise trap a run: a dizzy spell ending while the player is still
inside the animal that caused it has no edge left to give, so the player has to
separate before it can be bumped again. Without that it is re-bumped on the
recovery frame, loses the next pickup, and never walks out. A grace timer was
the alternative and is worse: it has to be long enough for the worst case and is
therefore too long for every other one.

**The goal ends the run, and this narrows "The game".** That paragraph ends
*Collect everything and the level is done*, and it was written 2026-08-10. The
map gained a `goal` spawn on 2026-09-08, three weeks later, and the level ships
with one. So the exit condition is reaching the goal and not clearing the map:
the pickups are what the run is *scored* on and the goal is what *ends* it,
which also means a run can end with pickups left behind and that is a choice the
player made rather than a failure. Requiring a full sweep would make the last
cherry a search, which is a different game and a worse one on a level 2.4
screens wide. **The older sentence is superseded, not reinterpreted.**

**`victory` is one frame, so `Stakes::victory_time` holds the run open.**
Decision 7's note, paid. The same reasoning that picked `dizzy` over `hurt` at
ingest applies harder here: six frames on a repeat loop can at least be watched,
whereas one frame has no duration at all and the art cannot end anything. The
flourish is therefore the HUD line and the cue, and the clock is the rules'.

**`sparkle` marks a collection and `puff` marks a bump**, which is read off the
art table above rather than chosen: `sparkle` is the pack's `item-feedback` —
"the twinkle a pickup leaves behind" — and `puff` is its `enemy-death`, a
generic dust cloud renamed on ingest because nothing in this game dies. Both are
`Loop::Once`, and a burst is an entity with a sprite, no box and no rules state,
reaped when `finished()` goes true.

#### The score is per `Behaviour`, not a field per pickup

`Stakes::points[]` and the run's held counts are both indexed by `Behaviour`.
A `cherries` field beside a `gems` field reads better at one call site and makes
the pickup vocabulary exist in three places — the map, the enum, and the score —
so adding a pear would be an edit to all three and forgetting the third is a
pickup that scores nothing while drawing perfectly. The subscript idiom is
already in the tree: `tests/test_orchard.cc` counts the shipped spawn table that
way.

#### The eagle's collision box is centred on its frame, and the fix is orchard's

`Entity::align_box_to_sprite()` centres horizontally with the bottom edges
flush, which is right for anything that stands. A flier has no feet, so there is
nothing for that rule to be right about.

**Measured, not argued.** Over all four `eagle-swoop` frames in the shipped
atlas the art's centre of mass is row 21.8 of the 41-row logical frame. A 24-row
box placed bottom-flush spans rows 17–41, centred on row 29: **7.2 rows low,
most of a third of its own height**, which puts the bird's head outside its own
hitbox and the empty sky under its talons inside it. Centred spans 8.5–32.5 and
is 1.3 rows out. Before this slice that was invisible; now that contact costs a
pickup it is the difference between being hit by a bird and being hit by
nothing.

**It landed in `orchard` and not in `game::Entity`, deliberately.** Decision 16
is one slice old and rejected a stored `offset_x/offset_y` and an `anchor` enum
*by name*, as the shape that would make `Entity` the place every later need gets
appended to. One flier in one demo is not the second consumer that would justify
reopening that. `orchard/demo.cc` has a nine-line `align_box_to_flight()` in its
anonymous namespace instead. If a second demo grows a flier the question is
open again, and the honest form of it then is not "add a mode" but "should the
box be aligned to the *art* rather than to the logical frame" — see the next
item, which is the same question arriving from a different direction.

**Proved by screenshot diff rather than by reading it.** With and without the
eagle branch, at 1150 frames on `desktop-software`, the only pixels that differ
are a 31×32 region containing the eagle, and its art moved down exactly 8 px
(17 − 8.5, floored at draw). Nothing else in the frame moved, which is the claim
that mattered: the box stays where the spawn put it and only the art moves.

#### Found on the way: the frog is drawn 5 px above its own collision box

Not fixed in this slice, and not fixable from `orchard` — **fixed 2026-09-15 by
[decision 19](#19-the-baseline-anchor-reads-the-art-not-the-logical-frame)**,
which is where the answer and its proof are. `frog-idle`'s logical frame is 35×32
but its art occupies rows 6–27, leaving five empty rows at the *bottom*. So
"bottom edges flush" aligns the box to the frame's bottom and the frog is drawn
floating five pixels over the floor it is standing on — visible in
`orchard --screenshot --frames 480`. `frog-jump`'s art reaches row 32, so the
frog also drops five pixels the instant it leaves the ground and rises five when
it lands.

The opossum is the only one of the three the convention is actually right for:
its art runs to row 28 of a 28-row frame. So `align_box_to_sprite()`'s premise —
that a logical frame's bottom edge is the art's baseline — holds for one actor
out of three in this atlas and is a property of the *art*, not of the rule.

The general fix is to align the box to where the art actually is, which needs
the trim rectangle. `AtlasFrame` has it and `AnimatedSprite` does not expose it,
so no demo can do this correctly today and a per-actor fudge constant in
`orchard` would be six magic numbers hiding a missing accessor. **That makes it a
`gfx`/`game` question and it is raised as one rather than worked around here.**

**Answered 2026-09-15 by [decision 19](#19-the-baseline-anchor-reads-the-art-not-the-logical-frame).**

### 19. The baseline anchor reads the art, not the logical frame

Settled 2026-09-15, as the `gfx`/`game` answer to the question decision 18 left
open above. `gfx::AnimatedSprite` gains `frame_art()` — the current cel's
trimmed region as a rectangle in frame-local pixels — and
`Entity::align_box_to_sprite()` sits the box's bottom edge on the art's lowest
drawn row instead of on the logical frame's bottom edge.

**No `anchor` enum and no stored offset.** Decision 16 rejected both by name and
that still holds. This corrects what the one convention *measures*; it does not
add a second one.

**The two axes deliberately read different things**, and the asymmetry is the
whole design. Vertically the box reads the art, because the bottom edge is a
*contact point*. Horizontally it keeps reading the logical frame, because there
is no contact point on the sides and the logical frame is the only reference
that is constant across an animation. Centring on the art's own bounding box
instead would pin its horizontal centre, and `opossum-walk` shows what that
costs: its art spans cols 1–29 on one cel and 1–36 on another, so the body would
slide 3.5 px backwards to hold the centre of an extending limb still.

**What it fixed, measured on `desktop-software` with the frog probed per
frame over 400 frames.** Before, the frog's art sat 5.00 px above its collision
box while grounded and flush with it while airborne, so the art stepped ±5 px at
all eight take-off and landing transitions. After, the gap is 0.00 px on every
frame of the run and the step at all eight transitions is 0.00 px. The grounded
frog's lowest drawn row moved from 475 to 480 — the floor its box was already
resting on.

**The player had the same defect and nobody had noticed.** `foxy`'s `run` cels
end at rows 32, 31, 32, 32, 31, 32 of a 32-row frame and its `jump` cels at 30
and 29, so the fox bobbed 1 px twice per stride and popped 2–3 px on every jump.
Both are gone. Measured by screenshot diff at frames 100 and 108: the player's
art moved +3 px airborne and +1 px on the two run cels that were short.

**The opossum was not the clean case decision 18 recorded.** Its art reaches row
28 of 28 on five cels but row 27 on `opossum-walk.000`, so it carried a 1 px bob
of its own. The premise held for *no* actor in `critters` without qualification.

#### The flight anchor stays, because it answers a different question

The report that produced this row claimed that aligning to the art would be
"correct for all four cases with no mode at all", so `orchard`'s
`align_box_to_flight()` could be deleted. **Checked, and it is wrong.**

`eagle-swoop`'s art ends at rows 39, 38, 39, 39 of its 41-row frame, so
bottom-flush on the *art* puts its 24-row box at a centre of 26.75 against an
art centre of mass at 21.8 — **4.5 rows low**, where bottom-flush on the frame
was 7.2 rows low and centring is 1.3. Reading the art recovers about a third of
the error and leaves the rest. It does not subsume the flier.

**The reason it cannot, stated so nobody has to rederive it:** the two calls
answer different questions, not one question with a mode.
`align_box_to_sprite()` answers *"where are this thing's feet"*, and its whole
content is that the answer is a property of the art rather than of the frame.
A flier has no feet. There is no baseline to find, correctly or incorrectly, so
a better way of finding one changes nothing. "A baseline in the wrong place" and
"no baseline at all" are different defects and a single rule cannot be right
about both — which is the concrete form of why decision 16's rejected `anchor`
enum is still the wrong shape. The enum would make them look like two settings
of one thing.

**The helper is renamed `centre_box_on_sprite()` and now has two callers**, in
`orchard/demo.cc`'s anonymous namespace. `align_box_to_flight` was named for the
eagle, and the goal marker is the second piece of art in this demo with no
baseline: it draws `sparkle`, a radially symmetric burst whose centre of mass is
row 16.0–16.5 of a 32-row frame on every cel while its lowest drawn row travels
from 25 to 32 as it expands. Anchoring that to its own growing edge pushed it
7 px into the ground it marks — a regression this change introduced and this
routing removes. Two pieces of centred art in one demo is a routing table in
that demo; it is still not a mode on the shared call.

#### What the screenshots show

`sprites --screenshot` is **byte-identical**, captured from this build either
side of the change. It is the first consumer, it never sets a box, and it is the
control on `frame_art()` having touched the draw path.

`orchard --screenshot` changes, and every region that moved is accounted for.
At frames 1, 30, 60, 200, 600 and 1150 the differing pixels resolve to: the frog
(+5 px, the defect), the opossum (+1 px, and only on the cel that is short), the
cherries (+3 px), the gems (+1 px) and the player (+1 px running, +3 px
airborne). **The eagle and the goal marker do not appear in the diff at all** —
both are centred and neither reads the changed axis.

The cherries and gems move because they are also centred art with no baseline:
`cherry`'s 15-row art sits in a 21-row frame with three empty rows above and
three below. The shift is an improvement rather than a correction — the cherry's
art centre goes from 2.5 px above its pickup box's centre to 0.5 px below it —
but it is **not** what this row set out to change, and routing the pickups to
`centre_box_on_sprite()` as well would make them exact. Left alone deliberately:
that is a placement judgement about two more actors, not part of fixing the
anchor, and it is recorded here rather than made quietly.

**A caution for whoever runs the next screenshot diff.** The first run of this
one was contaminated: another row regenerated `data/orchard.tmx` and
`data/tileset.png` between the baseline capture and the comparison, and the
clouds it added read as five sky-coloured regions that looked like this change
moving tiles. `orchard --screenshot` is deterministic — three runs produced
identical bytes — so the whole discrepancy was the data moving underneath.
Capture both sides against a frozen copy of `data/` through `$WREEL_DATA_DIR`
when other rows are live.

## Proposed shape

```
game::Entity         the promoted struct + an AABB + a Behaviour tag
game::Entities       sprites::Entities, moved and extended (decision 5)
game::Level          the parsed layers + a copy of the collision layer + the spawn table
game::collide        AABB vs. the collision layer; AABB vs. AABB
game::Camera         follow with a deadzone, clamped to level bounds, integer out
game::Sfx            named audio::Sound handles, loaded once through asset_path()
                     ^ WRONG AND NOT BUILT — superseded by decision 17.
                       The cue table is orchard::Cues, in orchard/cues.{hpp,cc}
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

**The `game::Sfx` line is the second one this sketch got wrong in the same way,
and decision 17 argues it at length.** It names a shared module for one game's
five cue names, and it puts `wreel::audio` on a dependency line the charter
excludes it from — the identical pair of faults the action line below had, with
one noun changed. Nothing was built from it. It is struck above rather than
deleted, so the route by which it arrived stays legible.

**The action line above was wrong, and this document is where the error started.
It has now been corrected twice, and the honest account is that the first
correction did not go far enough.**

The original line read `game::Action  Pickup/Jump/Left/Right over rig::Button`,
which named one game's verbs as the shared module's surface.
`include/game/action.hpp` then cited this section as its authority for exactly
those four, and a spec sentence reads as settled once it is quoted back. That is
how one game's vocabulary got into a shared module: not by anyone arguing for it,
but by a sketch being quoted as a specification.

The first correction, 2026-09-01 under decision 12, replaced it with
`game::InputBindings  a game's own verb enum -> rig::Button`. That removed the
verbs and kept the line — the shape stayed, and only its vocabulary was made
generic. The second correction, the same day under decision 14, **removes the
line entirely.** `game/` carries no input entry in this sketch, because it
carries no input row at all: the binding belongs to `rig`, the verbs belong to
whichever executable declares them, and there is nothing left in between for this
module to hold.

Naming the two rounds rather than presenting the second as the correction is the
point. The first one fixed what the line *said* and left what it *was*, which is
the failure mode a sketch invites — the shape survives being corrected because
nobody is looking at it.

## Tasks

Ordered so something is on screen early, and so the parts that can be tested
headless are written before the parts that cannot.

**Two amendments from doing the work**, both 2026-08-22. The entity-store
promotion is listed after `collide` and `Camera` but was written to run before
them; decision 9 reversed that, because `Aabb` lives in `collide.hpp` and
`entities.hpp` includes it. And the promotion's "two commits" clause was not
honoured — see decision 11.

- [x] Ingest the art — **done, committed 2026-10-16 in `3af0cb9`**: the eight directories under
      `data/sunny-land/critters/`, `data/critters.{png,xml}` and
      `critters.anim.xml` (file dates 2026-09-05), with their
      `data/PROVENANCE.md` rows. Unblocked by decision 15, which settles the
      names. Unpack
      `data/Sunny-land-files/` first; it is gitignored and not in a clean
      checkout. Copy the seven targets into `data/sunny-land/` as the eight
      directories decision 15 names: `frog-idle`, `frog-jump`, `opossum-walk`,
      `eagle-swoop`, `cherry`, `gem`, `puff`, `sparkle`. Three of those are
      renames rather than transcriptions — `enemy-death` → `puff`,
      `attack` → `swoop`, `item-feedback` → `sparkle` — and `opossum-walk` is
      invented, because the opossum's six frames sit flat in
      `Characters/Opossum/opossum/` with no animation directory to take a name
      from. All of it happens **in the copy**, not by patching the atlas after.
      The source frames are 1-based and unpadded (`frog-idle-1.png`), and
      `pack_atlas.py`'s verify pass opens `<animation>/{index:03d}.png`, so the
      copy renumbers as well as renames — as `data/sunny-land/foxy/` already did.
      Pack as `critters.{png,xml}` with `tools/pack_atlas.py` (verification is on
      by default; `--no-verify` turns it off — there is no `--verify` flag);
      hand-tune `critters.anim.xml`, which is eight animations and no more.
      Record the new rows in `data/PROVENANCE.md` under the existing CC0 grant
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
- [x] `game::Action` over `rig::Pad` — **done 2026-08-30, `test_action`, 15 cases
      / 78 assertions; superseded 2026-09-01, see decisions 12 and 14.** One
      indirection, justified by the raw pad mapping being a documented guess:
      when a handheld reports the wrong button, the fix should be one table, not
      a search for `Button::A`. **That justification does not survive either**,
      and decision 14 says why: fixing a mis-enumerated pad in a `game/` table
      leaves `Button::A` meaning the wrong thing for the next program, so the fix
      was always rig's. The header said as much itself and the task line here
      did not
- [ ] ~~Rework it into `game::InputBindings<Verb>` per decisions 12 and 13~~ —
      **void, superseded by decision 14 before any of it was written.** No
      `game::InputBindings` exists and none will
- [x] **Delete `game::Action` from the shared layer**, per decision 14 — **done
      2026-09-01.** `include/game/action.hpp`, `game/action.cc` and
      `tests/test_action.cc` removed, with their entries in
      `game/CMakeLists.txt` and `tests/CMakeLists.txt` and the four-line comment
      above the test registration. Both substantive edits in
      `game/CMakeLists.txt` were made: **"input actions" is out of the charter's
      belongs-here list**, and the "THE rig CLAUSE EXCLUDES A JOB, NOT A NAME"
      paragraph went with it, having existed only to reconcile that entry
      against `wreel::rig` on the does-NOT list. `wreel::rig` came off the
      PUBLIC link line, checked rather than assumed: the only `rig::` left under
      `game/` or `include/game/` is `rig::asset_path` in a usage example in a
      comment, so nothing compiled references it. The dependency line reads
      `game -> gfx -> util -> posix`. **25/25 on `desktop-software` and
      `desktop-debug`**, zero warnings with `WREEL_WERROR` on; `test_action` is
      the only test that left and no other registration changed. Nothing ran on
      a device or under GCC 8.3. Nothing migrated — there were no consumers
      outside the test. The verb table is written in `orchard/` when `orchard`
      is
- [ ] *Out of scope for this initiative, pointer only:* per-device input bindings
      in `rig` — [rig-device-bindings](../2026-09-01-rig-device-bindings/).
      Deferred, not scheduled. It inherits decision 13's `released()` analysis
      and the open question of how a per-device table relates to
      `SDL_GameControllerAddMapping`, which results.md already prefers
- [x] A level, by extending `tools/make_tilemap.py` or a sibling — **done
      2026-09-08, as a sibling: `tools/make_orchard_level.py` writes
      `data/orchard.tmx`.** Tiled is still not available and `data/PROVENANCE.md`
      says so for this map as it does for the fixture. A sibling rather than a
      flag because the two maps have opposite jobs — decision 4 authors this one
      *against* the overdraw `sunnyland.tmx` exists to maximise — and because
      regenerating the fixture would churn tests that assert against it. It
      borrows the fixture's `sunnyland.tsx` and `tileset.png`, so it adds no art
      and needs neither Pillow nor the gitignored pack. 96×40 tiles of 16×16 =
      1536×640, larger than the 640×480 panel on both axes so the camera scrolls
      and clamps on each; `ground` and `canopy` visible, `collision`
      `visible="0"`; 23 spawns typed `player`, `frog`, `opossum`, `eagle`,
      `cherry`, `gem`, `goal`. **The end-to-end gap open question 2 named is
      closed**: `test_tmx` now loads the file and builds a `game::Level` from it,
      five cases / 123 assertions, and `tests/CMakeLists.txt` gained
      `wreel::game` on that line to allow it
- [x] `orchard` — **done: the rules and HUD 2026-09-14, the cues and music
      2026-09-15; committed 2026-10-16 in `3af0cb9`** (rows below and in the table at the top). Built
      under GCC 8.3 and played to the goal on a Miyoo Mini from the Onion
      bundle; see the Onion bundle task below. The rules, the HUD,
      `--screenshot`, `--seconds`, `--mute`, and
      the fill-rate line. No GPU requirement, same as `sprites`. Builds on the
      desktop presets and `miyoomini`, which is the whole matrix this initiative
      is answerable for until the ordered devices arrive; nothing about it should
      be written so that `steam`, `rk3326` or `h700` would need changing when
      they are added back. Its `WREEL_BUNDLE_ASSETS` list names every file it opens,
      including the WAVs, since that list is what a firmware bundle carries.
      **First slice done 2026-09-13**: the executable, the draw, and walking and
      jumping. `orchard/{main,demo,player,verbs}.cc` with the verb table in
      `verbs.cc` per decision 14, `--screenshot`/`--seconds`/`--mute`, and the
      fill-rate line. Animals, pickups, the goal as a goal, the HUD and every cue
      are still out; `--mute` is a flag with nothing behind it. The `critters`
      atlas is loaded and checked but nothing is placed from it. 26/26 on
      `desktop-software` and `desktop-debug`, zero warnings; nothing has run on a
      device or under GCC 8.3
- [x] **Is the level playable, not merely loadable** — **answered 2026-09-13:
      yes.** `tests/test_orchard.cc` compiles `orchard/player.cc` in and walks
      the real `data/orchard.tmx` from the `player` spawn to the `goal` spawn,
      arriving in 758 frames on a 1/60 step. The jump is `jump_speed` 400 px/s
      against `gravity` 1200 px/s^2 — a 66.6 px rise in closed form, 63.3 px as
      stepped at 60 Hz — which clears the 48 px branch step with margin. **Two
      corrections to what the map was believed to be**, both read off the shipped
      file rather than the generator: the ground profile steps up 32 px in
      **three** places and not two (columns 20, 40 and 62), and the branch at
      columns 46-48 sits two rows over ground at row 30, leaving a 16 px gap that
      a 28 px player **cannot walk through**. The middle of the level therefore
      has no ground-level route at all: crossing it means mounting that platform.
      That is playable and arguably intentional, but it was not a property anyone
      had stated, and neither `check_layout` nor `check_level_contract` in
      `tools/make_orchard_level.py` would catch a branch placed low enough to seal
      a route rather than low enough to bury itself
- [x] `tools/make_sfx.py` + `data/sfx.xml` — square with a duty parameter, an
      LFSR noise channel, an envelope, a 1–2 ms fade at both ends, 22050 Hz mono
      WAV out. Merge-not-overwrite on the description, as `pack_atlas.py` does
      for the animation sidecar, and a sha256 regeneration check
- [x] Wire the cues: jump (glissando), land, pickup, bump, and the
      level-complete flourish. One chunk each. `--mute` exists so a timed run and
      a screenshot run are not also an audio test — **done 2026-09-15 as
      `orchard::Cues`, per decision 17, and not as `game::Sfx`.** The three cues
      with rules behind them come off `Rules`' own edges; `jump` and `land` come
      off `orchard::CueEdges`, which reads the launch from the vertical velocity
      turning upward rather than from `on_ground()`, because `coyote_time` means
      a jump can fire after the ground has gone. `--mute` opens no device at all.
      26/26 on `desktop-software` and `desktop-debug`, zero warnings; the two
      crossings still take 758 and 1204 frames. The real `pulseaudio` device was
      opened and all five chunks played through it with nothing dropped, so the
      mixer path is exercised rather than stubbed. **Nobody has listened to it,
      though** — every check here is a count, a log line or a checksum, and
      whether the cues sit well against the module is still unheard.
      2026-10-16: confirmed audible by the user on the device (sound effects
      and module playback in orchard and coppers); not logged. That says the
      output is heard and correct, not how the cues balance against the
      module, which nobody has reported on. Decision 6
      already records that listening is the only real review method for a sound
- [x] Add the `data/emerald-droplets.xm` row to `data/PROVENANCE.md` — malmen,
      copyleft with artist attribution — done 2026-08-10, with the detail block
- [x] Name `emerald-droplets.xm` in the demo's `WREEL_BUNDLE_ASSETS`, which waits
      on the demo existing — **done 2026-09-15**, with the five WAVs beside it as
      `sfx/<name>.wav`. `data/sfx.xml` is deliberately NOT listed: it is
      `make_sfx.py`'s input and no line of the game opens it. Checked rather
      than assumed that `cmake -E copy_if_different` creates the `data/sfx/`
      subdirectory in a bundle, since every other entry in that list is flat
- [ ] Add it to the Onion bundle as a fourth entry, and to `docs/DEVELOPMENT.md
      § Status` with an honest verification state — **entry written 2026-10-06,
      built under GCC 8.3 into `pkg/wreel-0.2.0-onion.tar.gz` the same day, and
      run once on a Miyoo Mini, played to the goal**
      ([results.md](../2026-07-25-target-validation/results.md)). Open: the
      `docs/DEVELOPMENT.md § Status` row, and committing. The rest is in
      [orchard-first-device-run](../2026-10-06-orchard-first-device-run/)

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

*2026-10-16, citing [results.md § 2026-10-06](../2026-07-25-target-validation/results.md#2026-10-06--orchard-first-run-on-the-device):*
with the XM playing, the one device run held a capped 59.7 fps and the game's
own work was 4.1 ms a frame. That shows the 24-channel mix fits under the cap;
it does not say what the mix costs, because the `--mute` half of the pair has
not been run on the device.

**That experiment is now runnable rather than arguable, and it is the user's to
run.** Since 2026-09-15 `orchard` plays the module and `--mute` opens no audio
device at all, so `orchard --seconds N` and `orchard --seconds N --mute` are the
two halves of the measurement with nothing else differing between them. The
fill-rate line reports voices alongside the tile figures for the same reason.
On this dev box the two read 16.659 and 16.673 ms/frame at 318 tiles/frame,
which is a difference of 0.08% and says nothing at all — a 44100 Hz mixer with
16 voices on an idle desktop was never going to be the bottleneck. **Nor could
that pair have shown anything either way: 16.66 ms is 60.0 fps to three digits,
so both runs were sitting on a frame cap and reporting it rather than reporting
a cost.** Uncapped and with the corrected meter the frame is around 1.1 ms, so
the cap was absorbing more than 90% of the interval. Re-run it with `--fps 0`.
The number
that matters is the same pair on the Miyoo Mini, where 24 channels are mixed at
22050 Hz on two Cortex-A7 cores that are also rasterising every pixel.

**Generated audio is easy to get subtly wrong in ways that only show up on the
device.** The dev box is 44100 Hz with 16 voices and a forgiving buffer; the
Miyoo Mini is 22050 with 8 and 2048 samples. A cue that fires every frame while a
button is held, or eight overlapping pickups, will sound fine here and starve
there. Effects are one-shot and triggered on edges (`pressed()`, not `down()`),
and the fill-rate run reports voice count alongside frame cost.

**Paid on 2026-09-15, and the edge that needed the care was not the button
one.** Three of the five cues come off `Rules`, which is already once-per-event.
The two that do not — `jump` and `land` — had to be derived from
`Player::on_ground()` and `velocity_y()`, which are *states* and not events;
decision 17 argues the derivation and `tests/test_orchard.cc` pins it, including
the case that 300 frames of standing still produce zero `land` cues. The
fill-rate line now carries `voices N peak M dropped K`.

**One caveat on `peak`, because it is easy to misread.** It is a lower bound
derived from the voice index `Sound::play()` returns, and it is only meaningful
from a run that keeps real time. A `--screenshot` run steps a fixed delta as
fast as it can while the mixer drains at wall-clock rate, so its cues pile up:
the same 1400-frame play reports peak 6 as a screenshot and peak 1 over
`--seconds`. Quote the `--seconds` figure.

**"Simple game" is the least reliable estimate in software.** The scope above is
deliberately small and will still try to grow a title screen, a pause menu, a
second level and sound. The guard is that none of those is in Tasks, and adding
one means saying so here first.

**The fill-rate claim in decision 4 is a prediction.** A level built against
overdraw *should* cost less than the fixture. It has not been measured and might
not, particularly if a parallax layer sneaks in. `orchard --seconds` is the
instrument; the number goes in results.md next to the others.

*2026-10-16, citing [results.md § 2026-10-06](../2026-07-25-target-validation/results.md#2026-10-06--orchard-first-run-on-the-device):*
measured once on the device, flushed and capped: 316 tiles/frame, tilemap
2.274 ms of 16.762, no parallax layer. The comparison against the fixture that
the claim is about has not been redone with the flushed meter, so the claim
itself is still unsettled.

**Still nothing has run on a device.** Unchanged from every snapshot before this
one, and this one adds code on top of a fill-rate figure taken only on a dev box —
1.162 ms at 640×480, times a guessed 10–20× for two Cortex-A7 cores, which
straddles the 60 fps budget. The mitigation is that this work does not *depend* on
that answer: a wrong guess changes how much fits on screen, not whether the
collision code is correct. But it is the reason not to build a second level before
the first one runs on hardware.

**Since 2026-10-06 the first one has run on hardware**, once: software
renderer, capped, music on, 59.7 fps, the game's own work 4.1 ms a frame — see
[Where this stands](#where-this-stands-2026-10-16). Uncapped, muted and the
`Miyoo Mini` render driver are still unmeasured.

## Open questions

- ~~Does the level-complete flourish use `victory` (1 frame) or a still?~~
  **Answered 2026-08-10: `victory`.** `foxy.anim.xml` declares it `fps="6"
  loop="repeat" frames="1"`, so it holds the pose until the game moves on —
  the same property that decided the bump cue, and for the same reason: **the
  game state sets the duration, not the art.** Nothing in the code has to
  special-case a still, and `AnimatedSprite` plays it like any other animation.

  The consequence stands unchanged: with one frame of art doing the work, the
  flourish has to come from the sound cue and the HUD.

### Five questions opened 2026-08-30, one of them since answered

These came out of the pass that finished `game::Level` and `game::Action`. Each
is a **decision nobody has made**, not a defect and not an oversight — they are
here so the next session meets them as open rather than rediscovering them. None
of them was what the art ingest was waiting on; that was the frame-name question
below, now answered as decision 15. Number 2 was the one with a date on it — it
blocked the level task specifically, and the level task answered it.

1. **What bound, if any, `game::Entity`'s coordinate conversion should carry, and
   whether exceeding it clamps or throws.** The `Entities::draw` rounding fix
   (D28) left the double→int conversion deliberately unguarded: a `double`
   outside `int`'s range — or a NaN, which is a distinct undefined behaviour
   reached by a single `update()` tick at infinite velocity — is UB on conversion
   whether it is floored first or not. Picking a coordinate bound is a design
   decision with a gameplay consequence, so the fix stopped at the rounding and
   said so.
2. ~~**Which side of the spawn-layer name moves.**~~ **Answered 2026-09-08:
   neither.** The question assumed one map and two consumers. There are now two
   maps: `data/sunnyland.tmx` is a `gfx::TileMap` fill-rate fixture that no
   `Level` consumes, and `data/orchard.tmx` is a level and meets `Level`'s
   contract as written — a `collision` tile layer and a `spawns` object layer,
   both spelled as `include/game/level.hpp` declares them. Nothing was renamed
   and `Level` was not widened.

   The end-to-end gap this question was really about **is closed, by running it
   rather than by arguing it**: `test_tmx` loads `data/orchard.tmx` through
   `loaders::load_tmx`, builds a `game::Level` from the result, and checks the
   spawn table against the collision layer the same file carries. That contract
   had never been exercised on a file before, in either direction.

   One case pins the negative as well — the fixture still throws `LevelError`,
   deliberately. Asking for a `Level` from a stress fixture is a category error,
   not a missing feature, and the case is there so renaming either layer has to
   be a decision rather than a diff.
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

### One question opened 2026-09-01

6. **Whether the binding table survives underneath a recognizer, or is absorbed
   into it.** Decision 12 kept `InputBindings` small partly on the argument that
   a recognizer would consume it as configuration — fighting games express motion
   inputs in terms of directions and attack buttons, which are bound names, so
   the binding layer would sit below. That is an architectural hunch and not a
   checked fact, which is why it is here rather than asserted in a header: the
   alternative, a recognizer that reads raw `rig::Button` and leaves the binding
   table redundant, is not obviously worse. Nothing needs this answered until a
   recognizer is actually wanted, and that is not in this initiative.

   **Still open under decision 14, and it has moved rather than closed.** There
   is no `game::InputBindings` for a recognizer to sit on top of, so the question
   is now whether a recognizer would consume *rig's* per-device binding or a
   demo's own verb table — or both, one on each side of it. Decision 14 makes
   that a question spanning `rig` and a demo with nothing in between, which is a
   change of shape and not an answer. The reservation of the name `Action` is
   what keeps it answerable later.

### The question found 2026-08-14, answered 2026-09-01

- ~~**How the `critters` atlas namespaces its frame names.**~~ **Answered
  2026-09-01: prefix in the copy, hyphen-separated — [decision
  15](#15-frame-names-are-namespaced-by-prefixing-in-the-copy).** This was the
  only thing blocking progress; the art ingest, the level, `orchard`, the cue
  wiring and the bundle entries all sat behind it, and none of them does now.

  The problem, for the record it was raised on: `pack_atlas.py` takes one source
  directory and derives animation names from the directory names under it, so
  packing frog, opossum, eagle and the pickups together produces bare `idle.000`
  and `jump.000` — and the frog and the opossum both have an `idle`. Decision 1
  spends its length on why a frame name is expensive to change after the fact,
  and then this case fell outside it.

  Two of the three candidates weighed here were weighed against reasons that did
  not hold, which is worth keeping because both errors were the same shape — a
  cost asserted rather than looked up. One atlas per actor was charged with
  *texture binds the Miyoo Mini may not want to pay*, when decision 1 forty lines
  earlier records that binds were measured **not** to be what is billed for,
  against 3.5 ns per pixel; its real cost is five files to keep in sync. And
  teaching `pack_atlas.py` a namespace argument was charged with risking the
  regeneration-produces-identical-bytes property of `data/foxy.*`, which an
  optional argument defaulting to off would not touch. Decision 15 rejects both
  on their actual costs instead.

Everything else above is decided; what is unverified is listed under Risks and is a
measurement rather than a choice.

## References

- [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/) — the
  predecessor, and the source of the scope line this snapshot picks up
- [target-validation/results.md](../2026-07-25-target-validation/results.md) — the
  fill-rate measurements decision 4 is built on, and the 2026-07-27 entry that
  already assigns the mis-enumerated-pad fix to `SDL_GameControllerAddMapping`
  rather than to more guessing (decision 14)
- [rig-device-bindings](../2026-09-01-rig-device-bindings/) — where the binding
  row goes under decision 14, and where decision 13's `released()` analysis is
  carried. Deferred, not scheduled
- `docs/MIYOO-MINI.md` §6.3 — the Miyoo's pad *is* a keyboard and enumerates no
  joystick, which is why `rig::Pad`'s per-device alias for it lives in the
  keyboard path and is verified rather than guessed (decision 14)
- [packaging-distribution](../2026-07-25-packaging-distribution/) — where a fourth
  bundle entry lands
- `data/PROVENANCE.md` — the CC0 grant this art arrives under, and the unknown
  section the three `.mod` files are still in
- `include/audio/device.hpp` — the codec cost model decision 6 rests on
- `cmake/ProjectOptions.cmake:130` — the codec tiers; `minimal` is WAV + tracker
- `include/gfx/tilemap.hpp`, `include/rig/input.hpp`, `sprites/entities.hpp`
