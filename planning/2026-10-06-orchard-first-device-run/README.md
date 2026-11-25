# Orchard's first run on a Miyoo Mini

**Status:** `in-progress` — the bundle entry and the run logging are written
and built into the Onion bundle under GCC 8.3, and device run 1 has been done
and recorded ([results.md](../2026-07-25-target-validation/results.md)
§ 2026-10-06). Runs 2 and 3 have no log. The tree is committed on
`feature/orchard-demo` as eight commits, `3af0cb9`..`37bd1bb` (2026-10-16;
pushed per the local reflog — see [Where the tree is](#where-the-tree-is)), and
has moved on since the bundle was built (group 7, `1b811fd`).
Group 7 does not build under GCC 8.3 (2026-10-16, D34) and has to be fixed
before it ships.
**Written:** 2026-10-06; status, commit plan and tasks updated 2026-10-16

This is a handoff. The session that wrote it could edit and build on the dev box
but was refused `git commit` and every `docker` command by its permission mode,
and could not write under `build/`. Everything below that needs one of those is
for a session that has them. The last section is a prompt to start that session
with.

Parent initiative: [game-layer-and-demo](../2026-08-10-game-layer-and-demo/),
whose last open task is "add it to the Onion bundle as a fourth entry".

---

## Where the tree is

Branch `feature/orchard-demo`, cut from `master` at `62714da`. **Committed
2026-10-16** as eight commits: seven following the plan below, group 3 folded
into 6, then `37bd1bb`, which records them in the status lines.

| Group | Commit |
|---|---|
| 1, the game | `3af0cb9` feat(orchard): add the orchard platformer demo |
| 2, D32 | `479e26f` fix(tools): compare only visible pixels in pack_atlas.py's verify pass |
| 4, clouds | `c74e310` feat(data): bake cloud tiles into the tileset and regenerate orchard.tmx |
| 5, input evidence | `69d6aff` docs: record the Miyoo Mini input evidence and orchard's first device run |
| 6, with 3 | `dc534d4` feat(orchard): Onion bundle entry, run logging and per-phase frame timing |
| 7, mirroring | `1b811fd` feat(gfx): mirrored sprites, and per-character facing in orchard |
| 8, doc audit | `9921511` docs: audit the documentation against the tree, 2026-10-16 |
| — | `37bd1bb` docs: record the 2026-10-16 commits in place of "uncommitted" |

**Pushed, per the local record:** `git reflog` shows `update by push` for
`origin/feature/orchard-demo` at `9921511` (12:50 -0400) and `37bd1bb` (12:52)
on 2026-10-16, and that remote-tracking ref equals `37bd1bb`. The remote itself
was not checked, and whether a PR exists is unknown.

The plan is kept below as it was written, before the commits. Before them, the
index held the game as the September builders staged it and the working tree
was ahead of the index.

Every other branch, local and remote, was squash-merged through PRs #1–#18.
Nothing outstanding lives on one.

### What was uncommitted, in the order it should be committed

The staged/unstaged split is an accident of which builder ran `git add` and
carries no meaning. The grouping below is by subject.

1. **The game** — the index as it stands: `orchard/`, `data/critters.*`,
   `data/sunny-land/critters/`, the `game/` and `gfx/animation`
   changes, the tests, `tools/make_orchard_level.py` and the map as first
   generated. `git commit` with no paths.
2. **D32** — `tools/pack_atlas.py`,
   `planning/2026-07-25-cxx17-modernization/defects.md`. The verify pass
   compared colour under fully transparent pixels.
3. **The meter** — the September hunks of `orchard/demo.{cc,hpp}` and
   `sprites/demo.{cc,hpp}` (run-mean `ms/frame`, tilemap timed on its own), with
   `planning/2026-08-10-game-layer-and-demo/README.md`. `orchard/demo.{cc,hpp}`
   and `sprites/demo.cc` now also carry item 6's hunks, and `orchard/demo.cc`
   item 7's as well; split with `git add -p` or fold this into 6.
4. **Clouds** — `tools/make_orchard_level.py`, `data/orchard.tmx`,
   `data/tileset.png`, `data/PROVENANCE.md`. The same script change also
   picks subsoil per cell by hash (`DIRT_SEED`, `cell_hash`) instead of the
   `(column+row)%5` stripe, and drops the two stone tiles from the canopy; that
   is about 605 of the 626 changed ground cells in `orchard.tmx` and 7 canopy
   cells, and no `.md` describes it, so say it in the commit message.
5. **Input evidence** — `docs/MIYOO-MINI.md`, `docs/DEVELOPMENT.md`,
   `planning/2026-07-25-target-validation/results.md`,
   `planning/2026-09-01-rig-device-bindings/README.md`, `planning/README.md`.
   When written, one binding of seven had been seen on hardware and the
   documents said the group was verified; device run 1 has since made it two.
6. **This session** — `cmake/Packaging.cmake`,
   `cmake/templates/onion/launch.sh.in`, `util/logging.cc`,
   `tests/test_logging.cc`, `rig/input.cc`, `gfx/renderer/context.cc`,
   `include/gfx/renderer/context.hpp`, `orchard/main.cc`,
   `orchard/CMakeLists.txt`, the new hunks in `orchard/demo.{cc,hpp}` and
   `sprites/demo.cc`, and this directory. This session's paragraph and task
   note in the game-layer README and its index row in `planning/README.md` go
   in whole with groups 3 and 5, which own those files.
   It would read better as three: the bundle entry and launcher; the logger and
   input logging; the flush and the phase timing.
7. **Mirrored sprites and facing** — written 2026-10-09, after this handoff,
   and described in no other document. After group 1, which creates the files.
   `gfx::Mirror` and the `Atlas(Context&, SDL_Surface*, Frames)` constructor,
   which uploads a second, row-reversed copy of the sheet because the
   `Miyoo Mini` driver no-ops `SDL_RenderCopyEx`; a `Mirror` argument on
   `AnimatedSprite::draw`; `game::Entity::mirror`; and orchard's
   `native_facing()` / `drawn_reversed()`, applied to Foxy and the critters.
   Whole files: `include/gfx/atlas.hpp`, `gfx/atlas.cc`, `tests/test_atlas.cc`,
   `include/gfx/animation.hpp`, `gfx/animation.cc`, `tests/test_animation.cc`,
   `include/game/entities.hpp`, `game/entities.cc`, `tests/test_entities.cc`,
   `orchard/actors.{cc,hpp}`, `tests/test_orchard.cc` — in each, the unstaged
   hunks are this group's only. Plus the `load_sheet` / `mirrorable_atlas` /
   `mirror_for` hunks of `orchard/demo.cc`, by `git add -p`. 14 new
   `TEST_CASE`s (atlas 8, animation 1, entities 1, orchard 4, by grep).
   On 2026-10-16 `desktop-software` (full compile, ccache disabled) and
   `desktop-debug` built it with zero warnings and passed 26/26. **Under GCC 8.3
   it does not build**: an incremental build in `build/miyoomini-83` the same
   day stopped on two `-Wcast-align` errors, `tests/test_atlas.cc:489` and
   `tests/test_animation.cc:493`, both in this group's unstaged hunks
   ([D34](../2026-07-25-cxx17-modernization/defects.md), fixed 2026-11-25 and
   verified under GCC 8.3 for those two tests). A bundle still has to be rebuilt
   to carry it. It is not in the bundle that ran on the device.
8. **Documentation audit, 2026-10-16** — every file whose only uncommitted
   change is the audit's: `README.md`, `CLAUDE.md`, `THIRD-PARTY.md`,
   `docs/TARGETS.md`, `tools/README.md`,
   `platform/miyoomini/sdl2/PROVENANCE.md`, and these planning READMEs:
   `2026-07-25-cxx17-modernization`, `2026-07-25-graphics-backends`,
   `2026-07-25-midi-live-visuals`, `2026-07-25-packaging-distribution`,
   `2026-07-25-software-2d-sprites-tiling`, `2026-07-25-software-3d-rasteriser`,
   `2026-07-25-target-validation`, `2026-07-26-coppers-cracktro`,
   `2026-07-27-onion-bundle`, `2026-07-29-gles-free-runtime`,
   `2026-07-31-miyoo-sdl2-fork` and `2026-08-08-miyoo-sdl2-public-repo`.
   `tools/README.md` also has a staged change, which is group 1's.
   The same pass also edited files already owned by groups 2–6
   (`defects.md`, `data/PROVENANCE.md`, `results.md`, `docs/DEVELOPMENT.md`,
   `docs/MIYOO-MINI.md`, `planning/README.md`, the game-layer and
   rig-device-bindings READMEs, and this file). Those go in whole with that
   group — the audit's hunks are not worth an `add -p` split — and its commit
   message should say it includes the 2026-10-16 corrections.

   Coverage, checked 2026-10-16 against `git status --short
   --untracked-files=all`: every path's staged change is group 1's, and every
   unstaged or untracked path has exactly one owning group above. Three files
   are split by hunk on purpose, as groups 3, 6 and 7 say:
   `orchard/demo.cc` (3, 6, 7), `orchard/demo.hpp` and `sprites/demo.cc`
   (3, 6).

Do not `git add .` — `opportunity.txt` at the root is a personal file and not
the project's. Under a sandboxed session `git status` also lists root dotfiles
(`.bashrc`, `.gitconfig`, `.mcp.json`, `.claude/agents`, …) and `dist` as
untracked; those are the sandbox's placeholder mounts, not files in the
repository, and a session outside the sandbox will not see them. No Claude
attribution trailers on commits.

### What this session changed

- **Bundle entry.** `_wreel_add_onion_bundle` stages `App/Orchard`. Its log is
  folded into `stdout.log` through `REPORT`, and the Sprites entry got the same.
- **Default arguments for the entry are `--software --verbose`, and only the
  default.** The launcher runs `./orchard ${ORCHARD_ARGS---software --verbose}`:
  unset, that is what runs; set in `local.env`, even to nothing, it is exactly
  what was set — **so an `ORCHARD_ARGS` that omits `--software` is a run on the
  other driver.** Reasoning under *Why software first*.
- **`util::log_open_file` line-buffers.** A run's log is smaller than one stdio
  buffer, so a killed process left an empty file.
- **`orchard` logs** a build-identity line, why the loop ended, a press count per
  verb including zeros, per-phase `ms/frame` (step, clear, tilemap, entities,
  hud, present), the longest frame and the number of clamped frames.
- **`--verbose`** sets debug level, at which `rig::Pad` logs every key-down with
  its keysym, scancode and the button it resolved to, or `unmapped`. `rig::Pad`
  also logs `SDL_QUIT` at info level, in every program.
- **`gfx::renderer::Context::flush()`**, and its use at the timing boundaries in
  `orchard` and `sprites`. See *Where the frame goes*.
- **The launcher deletes the previous run's report before running**, so a
  program that dies before opening its log cannot have a stale one folded in.

### What was verified, and how

| Claim | Evidence |
|---|---|
| Desktop build is warning-free with these changes | scratch `desktop-software` tree outside `build/`, GCC 12, `WREEL_WERROR` on |
| A reviewer read the diff | one pass by a second agent; its two substantive findings — the batching artefact and the stale folded log — are fixed, and it found nothing that would fail to compile or warn |
| Tests | 24 of 26; `test_atlas` and `test_renderer` fail writing to a hard-coded `/tmp` that session's sandbox made read-only, before and after the changes |
| The new lines are written | a 2 s headless run (`dummy` video and audio) and reading `orchard.log` |
| The launcher is generated correctly | a `miyoomini` configure with `-DWREEL_TARGET_FIRMWARE=onion` produced `App/{Coppers,Orchard,Sprites,WreelDiag}`; `launch.sh` passes `sh -n` |
| orchard compiles under GCC 8.3 for ARM | `build/miyoomini-83`, 2026-09-21, **before** this session's changes |
| `test_orchard` under qemu | 26/26 in that same run |

**Not verified when written:** these changes under GCC 8.3; the `bundle-onion`
target itself — staging, `data/sfx/`, the tarball; the key-event line, which no
headless run can fire; anything on a device.

**Since closed, by evidence in the tree (checked 2026-10-16):**

- *Under GCC 8.3, and the bundle.* `pkg/wreel-0.2.0-onion.tar.gz` is dated
  2026-10-06 22:53 and holds `App/Orchard/{orchard,launch.sh,config.json}`,
  `lib/libSDL2-2.0.so.0`, five `data/sfx/*.wav` and sixteen assets. The device
  log's `ls -l` sizes for `orchard` (2287508) and `libSDL2-2.0.so.0` (1480704)
  match the tarball's, and its identity line reads `compiler 8.3.0, SDL 2.32.10`.
  The zero-warning claim rests on the build session's own account under
  *The build, as far as it is known*; no build log was kept.
- *The key-event line, and a device.* Device run 1 — root `stdout.log`,
  recorded in [results.md](../2026-07-25-target-validation/results.md)
  § 2026-10-06. Software driver, `--software --verbose`, 4383 frames at 59.7 fps
  (capped), played to the goal and `stopped by the rules`; 36
  `input: key Space … -> a` lines and nothing `unmapped`. Start was not pressed
  (`Quit pressed x0`), so the Start-versus-Menu question in run 1 is still open.

**Still not verified:** anything written after the bundle was built — group 7
above — under GCC 8.3 or on a device; device runs 2 and 3.

---

## What the September session left behind

Read out of its transcript, because none of it reached the tree. Its last builder
died on an API error after finishing the GCC 8.3 build and before reporting.

- **The qemu run was 22 of 26, and three of the four failures are the test
  suite's premise rather than the code's.** `test_audio` and `test_playlist`:
  the `miyoomini` SDL2 is built with `SDL_DUMMYAUDIO OFF`, so there is no audio
  device to open under emulation. `test_renderer`: it asserts the driver is
  `software` and that asking for an accelerated one throws, and the grafted
  `Miyoo Mini` driver is selected and calls itself accelerated.
- **`test_entities` segfaults under qemu at `tests/test_entities.cc:774`** and
  passes 30/30 with `SDL_RENDER_DRIVER=software`. The cause was not pinned. The
  working guess is the `Miyoo Mini` render driver running without its video
  driver's init under `SDL_VIDEODRIVER=dummy`. The core is
  `qemu_test_entities_20260921-182002_56.core` at the root. A second,
  `qemu_test_entities_20261006-224814_56.core`, is from the evening of the
  bundle build, five minutes before the tarball; nothing records which command
  produced it. Both are SIGSEGV with the same pc and lr. Recorded as
  [D33](../2026-07-25-cxx17-modernization/defects.md) on 2026-10-16, cause still
  not pinned.
- **`tools/make_tilemap.py` rewrites `data/tileset.png` from the pack**, which
  would silently erase the fourteen baked cloud tiles. No guard, no failing test.
- **`check_layout()` in `make_orchard_level.py`** checks that a branch is not
  inside the ground, not that it does not seal the corridor. One did, at columns
  46–48, and was fixed by hand.
- **Where the frame goes — and September's answer was an artefact.** It
  measured the tilemap at about 6% of the frame (0.11–0.20 us/tile) and asked
  what the rest was. SDL's software renderer batches: a draw call only queues,
  and the queue drains at the next present or texture change. The HUD destroys
  a temporary texture every frame, so the whole frame's rasterising was being
  charged to whatever ran when that happened, and `TileMap::draw` was timed
  queueing. `gfx::renderer::Context::flush()` now exists and both demos call it
  at each timing boundary. Desktop Debug, software driver, 640x480, `--fps 0`:

  ```
  before   tilemap 0.133   entities 0.021   hud 1.332          (capped run)
  after    tilemap 0.353   entities 0.021   hud 0.021   clear 0.073
  sprites  1295 tiles/frame, 1.354 ms tilemap, 1.05 us/tile    (was 0.11)
  ```

  So the tilemap is most of the frame, not 6% of it, and decision 4's
  2026-09-17 re-measurement in the game-layer README — "the backdrop is
  affordable, what separates the two demos is everything else" — rests on the
  artefact. That passage has a correction note pointing here; the figures in it
  have not been redone.

---

## Why software first

On the device SDL picks the `Miyoo Mini` render driver unless told otherwise.
Three things are known or predicted about orchard on it:

1. **`SDL_RenderClear` is ignored** — measured, `diag-stdout.log`. orchard's sky
   is `Context::clear()` with no backdrop layer (decision 4), so about three
   quarters of each frame is never painted. Predicted picture: black, with trails
   of every tile and sprite, and HUD digits overprinting.
   [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/) asks "is `Context::clear()`
   load-bearing anywhere on this target?" — it is now, and items 7 and 8 there
   are what fixes it.
2. **Partially off-screen blits are not clamped** before `MI_GFX_BitBlit` — read
   from the driver, never run. `TileMap::draw_layer` emits a negative destination
   on every frame the camera is not tile-aligned. What the vendor blitter does
   with one is unknown.
3. **About 350 synchronous blits a frame**, each a staging copy, a cache flush
   and a blocking wait. The only device figure is one full-screen blit.

SDL's software renderer has none of the three and passed every `wreel-diag`
check on the device. What it does not have is a frame-rate figure for this
workload, which is what the first run measures.

---

## Tasks

- [x] Commit, in the grouping above — done 2026-10-16: `3af0cb9`
      (1), `479e26f` (2), `c74e310` (4), `69d6aff` (5), `dc534d4` (6, with
      group 3 folded in), `1b811fd` (7), `9921511` (8), and `37bd1bb` (the
      status lines). Pushed per the local reflog; remote not checked
- [x] Build in the container under GCC 8.3 with `-DWREEL_TARGET_FIRMWARE=onion`,
      zero warnings, and build `bundle-onion`
- [x] List the tarball: `App/Orchard/{orchard,launch.sh,config.json}`,
      `lib/libSDL2-2.0.so.0`, `data/sfx/*.wav`, sixteen assets
- [ ] Record the 22/26 qemu result and the `test_entities` fault in
      `defects.md` and `docs/DEVELOPMENT.md § Status`; decide what the three
      premise failures should assert on this target. Both records are done,
      2026-10-16: D33 with both cores, and the `test_entities` and doctest-suite
      rows in § Status; the 22/26 figure has no log and is relayed in both as
      such. Re-run on the current tree 2026-10-16: 20 of the 24 suites that
      build pass, the same four failing; `test_entities` crashes in the case at
      `:774` and passes 31/31 with `SDL_RENDER_DRIVER=software` (results.md
      § 2026-10-16, D33). Still open: pinning the cause, and the premise-failure
      decision
- [x] Fix D34 (2026-11-25), the two `-Wcast-align` errors that stop group 7 building under
      GCC 8.3 (found 2026-10-16), then build `test_atlas` and `test_animation`
      there and run them under qemu
- [x] `docs/DEVELOPMENT.md § Status`: a row for orchard and the `game::` pieces,
      and the contradictions already in that section (2026-10-16)
- [x] Bring `planning/README.md`'s game-layer row and that README's "Where this
      stands" section in line with the tree (2026-10-16)
- [ ] `local.md`'s bundle recipe is the pre-2026-08-01 prebuilt-SDL2 route and
      no longer configures; it is gitignored and the user's to change
- [x] Device run 1 below — the user's. Root `stdout.log`; recorded in
      [results.md](../2026-07-25-target-validation/results.md) § 2026-10-06
- [ ] Device runs 2 and 3 below — the user's. No log in the tree for either
- [ ] Rebuild the bundle under GCC 8.3 once D34 is fixed (group 7 is
      committed, `1b811fd`): the bundle on the card predates it, so mirroring has not
      been seen on the `Miyoo Mini` driver

### The build, as far as it is known

`build/miyoomini` and `build/miyoomini-e` are owned by `nobody` with a cache
whose source directory is `/src`; leave them. `build/miyoomini-83` was configured
with the repository mounted at its host path, so it has to be again:

```sh
sg docker -c 'docker run --rm -v "$PWD":"$PWD" -w "$PWD" -u "$(id -u):$(id -g)" \
    wreel-miyoomini sh -c "\
      cmake --preset miyoomini -B build/miyoomini-83 -DWREEL_TARGET_FIRMWARE=onion && \
      cmake --build build/miyoomini-83 --target bundle-onion"'
```

Run as written on 2026-10-06: GCC 8.3.0, no warnings under `-Werror`. `sg docker`
is only needed where the session predates the group membership. The tarball
lands in `pkg/wreel-0.2.0-onion.tar.gz`. The August one, which has no orchard in
it, is owned by root and the container runs as the invoking user, so it was
moved aside to `pkg/wreel-0.2.0-onion.pre-orchard.tar.gz` first.

Re-run 2026-10-16 on the current tree with the default target in place of
`bundle-onion`, incrementally: it **fails under `-Werror`** on two
`-Wcast-align` errors in group 7's tests (D34), so the "no warnings" above
holds for the 2026-10-06 tree only. `pkg/` was not touched. Detail in
[results.md](../2026-07-25-target-validation/results.md) § 2026-10-16.

`bundle-onion` does not build the tests. The suite under qemu is the default
target and then `ctest`, in the same container with the toolchain's sysroot as
the loader prefix (the host's binfmt registration supplies qemu itself):

```sh
sg docker -c 'docker run --rm -v "$PWD":"$PWD" -w "$PWD" -u "$(id -u):$(id -g)" \
    -e QEMU_LD_PREFIX=/opt/miyoomini-toolchain/arm-linux-gnueabihf/sysroot \
    wreel-miyoomini sh -c "\
      cmake --build build/miyoomini-83 && \
      ctest --test-dir build/miyoomini-83 --output-on-failure"'
```

qemu writes a core for a crashing test into the test's working directory, and
ctest runs every test from the repository root (`WREEL_TEST_WORKING_DIRECTORY`
in `CMakeLists.txt`), whatever `-w` says. To keep cores out of the tree, add
`--ulimit core=0` to `docker run`, as the 2026-10-16 run did; qemu still prints
"core dumped" but writes nothing. To keep a core, run the binary directly with
the cwd in a scratch directory outside the repository.

**Do not build `bundle-onion` with the host's `arm-linux-gnueabihf-g++`.** It
configures, it would write the same tarball, and the binary inside would need
glibc 2.36.

### The device runs

Copy the tarball over the card's root. Each run leaves `App/<Entry>/stdout.log`;
bring it back to the repository root.

1. **Wreel Diagnostics**, then **Orchard**, both as shipped. Orchard is on the
   software driver and logging keys. Play it: press every control once, walk
   both ways, jump, reach a cherry, and **quit with Start** — the exit figures
   are written on the way out of the loop, and whether the Menu key ends the
   process some other way is not known.
2. **Orchard**, with `App/Orchard/local.env` holding
   `ORCHARD_ARGS="--software --fps 0 --seconds 30"`, and again with `--mute`
   added.
3. **Orchard** on the other driver: `ORCHARD_ARGS="--fps 0 --seconds 30"`. Look
   at the sky, the HUD and the screen edges while it scrolls.

Run 1's Orchard log is the root `stdout.log`, recorded in
[results.md](../2026-07-25-target-validation/results.md) § 2026-10-06. It was
played to the goal and ended by the rules rather than by Start, and the Wreel
Diagnostics half of run 1 left no new log at the root. Runs 2 and 3 have not
been returned. Each run's log is called `stdout.log`, so bring them back under
distinct names rather than over run 1's.

What each log answers:

| Question | Line |
|---|---|
| Which build, which SDL2 | `orchard 0.2.0 [target miyoomini], compiler …, SDL … linked` |
| Which driver | `renderer context 640x480 via …` |
| Does A reach the game | `orchard: Jump pressed xN`, `cue jump xN`; under `--verbose`, `input: key Space (sym 0x20, …) -> a` or `-> unmapped` |
| Was it Start or the Menu key | `orchard: stopped by the Quit verb` against `a quit event or Escape`; an `input: SDL_QUIT received` line above it means a signal or the window, not a key |
| What a frame costs | `orchard: ms/frame step …, clear …, tilemap …, entities …, hud …, present …` from an `--fps 0` run; a capped run reports the cap |
| Did it stall | `longest frame … ms, N clamped` |
| What the mixer costs | the same line with and without `--mute` |
| Did it crash | `--- exited N ---` with no `stopped by` line in the folded-in log below it |

If A does nothing there is no jump and the level cannot be crossed; quitting
still works. The `--verbose` log then names the keysym that did arrive.

---

## Open questions

- **Is the first-run default the right way round?** `--software` by default
  trades a known-wrong picture for an unmeasured frame rate. Run 3 is what would
  argue for reversing it.
- **Does the fix for the clear belong in the driver or in the level?** Items 7
  and 8 of the SDL2 fork make `RenderClear` work; a backdrop layer in the map
  sidesteps it at about 980 more tiles a frame. Decision 4 chose the clear for
  desktop overdraw reasons and did not have this driver in view.
- **What identifies a build?** The identity line carries the version, target,
  compiler and SDL2, and `0.2.0` is every build of this tree. The launcher's
  `ls -l` of the binary is the only thing that tells two bundles apart. A
  configure-time git revision goes stale without a reconfigure, which is why
  one was not added.

---

## Prompt for the next session

> Resume the orchard first-device-run work. Read `CLAUDE.md`, then
> `planning/2026-10-06-orchard-first-device-run/README.md`, which is the handoff
> and is authoritative on what is and is not verified — treat its claims as
> claims and check the ones you build on.
>
> You are on `feature/orchard-demo`, committed through `37bd1bb`, which
> the local reflog records as pushed (step 2 below is done). This session
> needs `git commit`, Docker (`sg docker -c '…'` if the socket is refused) and
> write access to `build/` and `pkg/`; if any of those is refused, stop and say
> which.
>
> 1. Build `desktop-software` and run its tests first. Expect 26/26 outside a
>    sandbox; anything else is a finding.
> 2. ~~Commit in the eight groups the handoff lists.~~ Done 2026-10-16
>    (`3af0cb9`..`9921511`, group 3 folded into 6; `37bd1bb` followed), and
>    the local reflog records both branch tips as pushed. Do not push or open a PR
>    without my say.
> 3. Build the Onion bundle in the `wreel-miyoomini` container under GCC 8.3
>    with `-DWREEL_TARGET_FIRMWARE=onion`. Zero warnings. The handoff's command
>    was run once, on 2026-10-06, before group 7 existed; fix it if it is wrong
>    and correct the document. As of 2026-10-16 the tree does **not** build
>    there: group 7's tests fail `-Wcast-align` at `tests/test_atlas.cc:489`
>    and `tests/test_animation.cc:493` (D34). Fix those before the bundle ships,
>    and record the fix in D34.
> 4. Verify the tarball's contents against the handoff's list, and that
>    `App/Orchard/launch.sh` runs `./orchard ${ORCHARD_ARGS---software --verbose} "$@"`.
>    Run `test_orchard` and the rest of the suite under qemu and compare with
>    the recorded results: 22/26 in September (relayed), and 24 run, 20
>    passing on 2026-10-16 with `test_atlas` and `test_animation` unbuilt.
> 5. The `test_entities` fault is D33 in `defects.md`, with both cores at the
>    root. Open them in gdb if the container has one; pin the cause or say it is
>    not pinned, and update D33 either way.
> 6. Tell me exactly what to copy to the card and how to do runs 2 and 3; run 1
>    is recorded. I run
>    the device and bring back `stdout.log` files to the repository root; when
>    they are there, analyse them against the handoff's table and record the
>    numbers in `planning/2026-07-25-target-validation/results.md`.
>
> Do not build `bundle-onion` with the host cross-compiler. Do not change
> orchard's defaults to make the device run easier — affordances are options.
