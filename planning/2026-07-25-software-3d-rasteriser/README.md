# Software-rendered 3D

**Status:** `snapshot`
**Written:** 2026-07-25
**Blocked by:** nothing, and deliberately not scheduled

## Why this exists as a document

A placeholder with reasoning, not a plan. When
[graphics-backends](../2026-07-25-graphics-backends/) asked whether the software
backend needs 3D, the answer was **no, not at this point** — and that answer is
worth keeping next to what it would actually cost, so the question does not get
reopened casually or answered differently by accident later.

## The decision it records

The software backend is 2D only: sprites, tiling, text. A handheld game here will
be some form of 2D, or mocked/basic 3D done with bespoke rendering. Neither needs a
general triangle pipeline.

Scoped separately, and where the actual work is going:
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

## What it would take, if it is ever wanted

Recorded so the size of the lift is visible rather than guessed at. The target is
the Miyoo Mini: two Cortex-A7 cores at 1.2 GHz, NEON, 128 MB of RAM shared with the
OS, no GPU and therefore no depth buffer or texture unit to borrow.

- **Rasterisation.** Triangle setup, edge functions, fill rules, sub-pixel
  accuracy. Getting the fill rule wrong produces seams and double-shaded pixels
  along shared edges.
- **Depth.** A z-buffer at 320×240 is 150 KB at 16 bits — affordable, but it is
  another full-screen buffer to clear every frame, and clears are already a
  meaningful fraction of the budget at this scale.
- **Clipping.** Near-plane clipping in homogeneous space, plus guard-band or
  scissor handling. Skipping this is the usual source of spectacular geometry
  glitches.
- **Perspective-correct interpolation.** A divide per pixel, or per-span with
  subdivision. This is where a naive implementation loses most of its performance.
- **Texture sampling.** Nearest is affordable; bilinear probably is not, and
  neither is mipmapping without the memory to store the chain.
- **Fixed-point or NEON maths.** The A7 has VFP, but scalar float in an inner loop
  will not hold a frame rate here.

Two honest alternatives that come first if 3D is ever wanted on a handheld:

1. **Target the Mali devices instead.** The RK3326 and H700 both have GPUs, so
   `gles2` gives them real 3D. Only the Miyoo Mini genuinely lacks one, and it is
   the weakest device in the matrix by a wide margin.
2. **Fake it.** Mode-7 style affine floors, scaled-sprite pseudo-3D, pre-rendered
   sprite rotations, raycasting. All are 2D operations, all were what this class of
   hardware actually shipped, and all fit the existing blitting path. This is what
   "mocked/basic 3D with bespoke rendering" means, and it is where to start.

## If this is picked up

Do not start with a rasteriser. Start by measuring what the existing 2D path
achieves on real hardware under sustained load — see
[target-validation](../2026-07-25-target-validation/), which has still never run
this code on a device. A frame budget measured on the Miyoo Mini is the only thing
that makes this scopeable, and without it any estimate here is speculation.

## References

- [docs/TARGETS.md § 3 — Miyoo Mini has no GPU](../../docs/TARGETS.md)
- [graphics-backends § Decided: the software backend is 2D only](../2026-07-25-graphics-backends/README.md)
