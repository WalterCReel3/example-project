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
| [2026-07-25-cxx17-modernization](2026-07-25-cxx17-modernization/) | `snapshot` | Remove removed-in-C++17 constructs, fix the defects they hide, then turn on `-Werror` |
| [2026-07-25-graphics-backends](2026-07-25-graphics-backends/) | `snapshot` | `gles2` and `gl33` backends; decouple model data from GL handles; retire `gl_legacy` |
| [2026-07-25-target-validation](2026-07-25-target-validation/) | `snapshot` | Prove the cross and Steam presets on real toolchains and hardware |
| [2026-07-25-packaging-distribution](2026-07-25-packaging-distribution/) | `snapshot` | Handheld bundles per firmware, Steam depot layout |
| [2026-07-25-midi-live-visuals](2026-07-25-midi-live-visuals/) | `snapshot` | The secondary goal: MIDI-driven demo-style graphics |

## Ordering

`cxx17-modernization` and `target-validation` are the two that unblock
everything else, and they are independent of each other:

- **`cxx17-modernization`** is a prerequisite for `graphics-backends`, because a
  new renderer written against the current `util/string.hpp` and `gfx::ObjModel`
  inherits their problems.
- **`target-validation`** is a prerequisite for `packaging-distribution`, because
  there is no point defining a bundle layout for a binary that has never run on
  the device.

`midi-live-visuals` is deliberately last. It is the fun one, it is desktop-only,
and it depends on nothing — which makes it the right thing to reach for when the
other work stalls.
