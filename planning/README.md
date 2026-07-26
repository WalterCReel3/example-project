# Planning

Scope snapshots and assessments for work that is understood but not yet started.
The point is that reasoning lives in the repository rather than in someone's head
or a chat log.

## Layout

```
planning/<iso-date>-<topic>/
    README.md       scope, motivation, tasks, risks, open questions
    *.md            supporting detail — inventories, measurements, designs
```

The date is when the snapshot was **written**, not when the work is due. A dated
directory is a point-in-time assessment: it is fine, and expected, for a later
snapshot to supersede an earlier one. Don't rewrite history in place — add a new
dated directory and mark the old one superseded.

## Status vocabulary

Each `README.md` opens with one of:

| Status | Meaning |
|---|---|
| `snapshot` | Assessed and scoped. Not started. |
| `in-progress` | Being worked on now. |
| `blocked` | Waiting on something named in the doc. |
| `done` | Landed. Kept for the reasoning, not the checklist. |
| `superseded` | Replaced by a later snapshot, which is linked. |

## Current snapshots

| Topic | Status | Summary |
|---|---|---|
| [2026-07-25-cxx17-modernization](2026-07-25-cxx17-modernization/) | `in-progress` | Remove removed-in-C++17 constructs, fix the defects they hide, then turn on `-Werror`. Warning load measured; character-classification approach decided |
| [2026-07-25-graphics-backends](2026-07-25-graphics-backends/) | `snapshot` | `gles2` and `gl33` backends; retire `gl_legacy`. Shrunk by the 2D-only decision |
| [2026-07-25-software-2d-sprites-tiling](2026-07-25-software-2d-sprites-tiling/) | `snapshot` | Where the handheld work goes: textures, atlases, TMX tilemaps, a minimal entity store |
| [2026-07-25-software-3d-rasteriser](2026-07-25-software-3d-rasteriser/) | `snapshot` | Deliberately not scheduled. Records what a CPU rasteriser would cost, and the cheaper alternatives |
| [2026-07-25-target-validation](2026-07-25-target-validation/) | `in-progress` | Prove the cross and Steam presets on real toolchains and hardware. Steps 1–2 done; the device toolchain, containers and hardware remain |
| [2026-07-25-packaging-distribution](2026-07-25-packaging-distribution/) | `snapshot` | Handheld bundles per firmware, Steam depot layout |
| [2026-07-25-midi-live-visuals](2026-07-25-midi-live-visuals/) | `snapshot` | The secondary goal: MIDI-driven demo-style graphics |

## Ordering

Revised 2026-07-25, after the software backend was settled as 2D only.

`cxx17-modernization` has done its job as an unblocker: `util/string.hpp` and the
logger are dealt with, and `desktop-software` builds warning-free. What remains in
it is confined to files `graphics-backends` deletes, so it no longer gates
anything.

**`software-2d-sprites-tiling` is the main line now.** It depends on nothing, it
is where a handheld game actually comes from, and the software backend cannot
currently draw a sprite at all.

The rest:

- **`target-validation`** remains a prerequisite for `packaging-distribution` —
  there is no point defining a bundle layout for a binary that has never run on
  the device. It is also the only way to answer the fill-rate question that
  `software-2d-sprites-tiling` lists as its main risk, which makes the two worth
  interleaving rather than sequencing.
- **`graphics-backends`** is now about `gles2` for the Mali handhelds and retiring
  `gl_legacy`. Neither blocks the 2D work.
- **`software-3d-rasteriser`** is deliberately unscheduled; it exists to record
  what was decided and what it would cost.

`midi-live-visuals` is still the one to reach for when other work stalls — it is
desktop-only and depends on nothing. It shares the frame-timing task with
`software-2d-sprites-tiling`, and its CPU effects want direct pixel access, which
is an open design question in that snapshot.
