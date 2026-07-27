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
| [2026-07-25-cxx17-modernization](2026-07-25-cxx17-modernization/) | `done` | Removed the removed-in-C++17 constructs and the defects they hid. **167 warnings to zero**, `-Werror` on since 2026-07-26. Kept for the defect inventory |
| [2026-07-25-graphics-backends](2026-07-25-graphics-backends/) | `superseded` | `gles2` and `gl33` backends; retire `gl_legacy`. Replaced by the two-renderer snapshot below |
| [2026-07-26-gfx-renderer-and-gles2](2026-07-26-gfx-renderer-and-gles2/) | `done` | `gfx::renderer` everywhere, accelerated on Mali; `gfx::gles2` for shaders and 3D; `skratch` ported onto it as the modern-GL reference; 2016 backend deleted and `WREEL_WERROR` on |
| [2026-07-25-software-2d-sprites-tiling](2026-07-25-software-2d-sprites-tiling/) | `in-progress` | Where the handheld work goes: textures, atlases, TMX tilemaps, a minimal entity store. XML dependency landed; `Texture` is next |
| [2026-07-25-software-3d-rasteriser](2026-07-25-software-3d-rasteriser/) | `snapshot` | Deliberately not scheduled. Records what a CPU rasteriser would cost, and the cheaper alternatives |
| [2026-07-25-target-validation](2026-07-25-target-validation/) | `in-progress` | Prove the cross and Steam presets on real toolchains and hardware. Steps 1–2 done; the device toolchain, containers and hardware remain |
| [2026-07-26-coppers-cracktro](2026-07-26-coppers-cracktro/) | `snapshot` | A copper-bar cracktro on `gfx::renderer`: the first thing to run on a Miyoo Mini Plus / Flip, and the instrument that takes the fill-rate measurement. Shares `Texture` with the 2D snapshot |
| [2026-07-25-packaging-distribution](2026-07-25-packaging-distribution/) | `snapshot` | Handheld bundles per firmware, Steam depot layout |
| [2026-07-25-midi-live-visuals](2026-07-25-midi-live-visuals/) | `snapshot` | The secondary goal: MIDI-driven demo-style graphics |

## Ordering

Revised 2026-07-26, after the renderer rework landed.

Two snapshots closed. `cxx17-modernization` finished the job it existed for: the
removed-in-C++17 constructs are gone, the defects they hid are fixed or withdrawn,
and the tree is at **zero warnings on all five configured presets** with
`WREEL_WERROR=ON`. `gfx-renderer-and-gles2` replaced `graphics-backends` and
delivered both renderers, deleting the 2016 backend — which is what cleared the last
30 warnings, since they were all in code it removed.

**`software-2d-sprites-tiling` is the main line, and it now has two prerequisites
met that it did not before:** `util::xml` reads Sparrow and TMX, and its rendering
target is hardware accelerated on the Mali handhelds rather than CPU-blitting. What
it still needs is the thing it always needed — `gfx::renderer::Texture` and a
source-rect blit, because the renderer cannot draw a sprite from an atlas yet.

The rest:

- **`target-validation`** is now the binding constraint on almost everything. Six
  snapshots' worth of work has been verified by compiling and by tests, and **no
  part of this project has run on a device**. It remains a prerequisite for
  `packaging-distribution`, it owns the fill-rate question that
  `software-2d-sprites-tiling` calls its main risk, and it is the only way to find
  out whether the Mali blobs expose the GLES2 context `gfx::gles2` now assumes.
  `skratch --screenshot` and `wreel-probe` exist to make that check a command
  rather than a judgement.
- **`software-3d-rasteriser`** is deliberately unscheduled; it exists to record
  what was decided and what it would cost. Note that `gfx::gles2` gives the two
  Mali devices real 3D, so the only target it would ever serve is the Miyoo Mini.

`midi-live-visuals` is the one to reach for when other work stalls — desktop-only,
depends on nothing, and its shader-based effects are now possible rather than
hypothetical. It shares the frame-timing task with `software-2d-sprites-tiling`;
`skratch`'s loop still `SDL_Delay(10)`s unconditionally.

## Revised again, 2026-07-26: `coppers-cracktro` goes first

Added after the ordering above was written, and it changes what to do next rather
than adding to the queue.

The deadlock in that ordering is that `target-validation` is the binding constraint
on everything, and its two most valuable remaining steps — the fill-rate
measurement and a device run — need *a program worth running on a device*.
`wreel-probe` reports capabilities but draws nothing, and `skratch` needs
`gfx::gles2`, which the Miyoo Mini will never have. So there has been no way to
take the measurement that `software-2d-sprites-tiling` calls its main risk.

[coppers-cracktro](2026-07-26-coppers-cracktro/) is that program, and it is not a
detour:

- Its stage 2 **is** `software-2d-sprites-tiling`'s blocking task —
  `gfx::renderer::Texture` and a source-rect blit. The work is shared, not
  duplicated.
- A full-screen raster field is the fill-rate stress case, so
  `target-validation`'s open question is answered as a by-product.
- It adds the `gfx::renderer` screenshot path that `target-validation` records as
  missing, which is how any device gets checked over SSH.
- It settles the locked-pixels design question that `software-2d-sprites-tiling`
  and `midi-live-visuals` are both holding open, and which is cheap now and
  expensive after the API sets.
- It forces the frame clock those two share.

It also found three things worth knowing on its own: `WREEL_BUILD_DEMOS` disables
itself on every GPU-less target, so `miyoomini` builds no demo at all; SDL's ARM
NEON blitters are unbuilt on the one target that does all its pixel work on the
CPU; and the `320×240` these documents pose their cost questions at does not match
the 640×480 panel those devices have.
