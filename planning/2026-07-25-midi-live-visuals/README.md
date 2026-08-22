# MIDI-driven live visuals

**Status:** `snapshot`
**Written:** 2026-07-25
**Blocked by:** nothing

## Motivation

The project's stated secondary goal: a toolkit for MIDI-controlled, old-school
demo-style graphics driven from a hardware controller.

This pulls in a usefully *opposite* direction from the handheld work. Handhelds
force a small, portable, CPU-cheap core with no GPU assumptions. Live visuals want
fast iteration, expressive rendering, and low-latency input. Keeping both honest
is a feature — a codebase that serves only one tends to grow assumptions the other
cannot pay for.

It is also the right thing to reach for when the handheld work stalls on hardware
availability, since it depends on nothing and runs on the dev box.

## Dependency

`librtmidi-dev` 5.0 is available on Debian 12 and already in the bootstrap
script's `midi` group, alongside `alsa-utils` for `aconnect -l` to enumerate
ports.

RtMidi is the right pick over raw ALSA sequencer: it is small, has no other
dependencies, and its cross-platform abstraction costs nothing here even though
Linux is the only target. Keep it **desktop-only and optional** —
`WREEL_ENABLE_MIDI`, default `OFF`, never enabled for a handheld preset. A Miyoo
Mini has no MIDI port and no cycles to spare.

Wrap it behind a `util::midi` facade for the same reason JSON is wrapped: the
vendor type should not appear in module signatures. See
[docs/TARGETS.md § Wrap it, don't spread it](../../docs/TARGETS.md).

## Shape

Three pieces, roughly independent:

**Input.** A `midi::Source` that opens a port and drains events. Note on/off,
CC, pitch bend, clock. Decide early whether events are polled once per frame
(simple, adds up to one frame of latency) or delivered on RtMidi's callback
thread into a lock-free queue (correct, more machinery). For visuals driven by a
human turning knobs, polling per frame is almost certainly sufficient — 16 ms is
below the threshold where it matters.

**Parameter binding.** A named parameter table that CCs map onto, with smoothing.
`bind("warp.amount", cc=21, range=[0,4], smooth=0.15)`. This is the piece that
makes it a *toolkit* rather than one hardcoded demo, and it is the natural first
consumer of the JSON config work — a mapping file per controller, with comments,
which is exactly what `ignore_comments` was kept for.

**Effects.** The actual old-school demo material: plasma, tunnel, rotozoom,
copper bars, starfield, feedback. Both routes are open now, which they were not
when this was written.

## Both renderers are available — pick per effect

> **Rewritten 2026-07-26.** This section used to ask whether the MIDI work should
> come *before* a `gles2` backend, and argued for CPU effects on the software
> backend because no shader pipeline existed. That question is closed:
> [gfx::gles2](../2026-07-26-gfx-renderer-and-gles2/) is implemented, `skratch`
> renders through it, and `gfx::software` is now `gfx::renderer`.

So the choice is per effect rather than per project:

- **`gfx::gles2`** — a fragment shader is the natural home for plasma, tunnel and
  feedback, and `Program` reports compile errors with the driver's info log, so
  iterating on shader source is not a guessing game. Desktop and both Mali
  handhelds; never the Miyoo Mini.
- **`gfx::renderer`** — still the right answer for anything wanting direct pixel
  access, via `SDL_LockTexture`, and the only option on the Miyoo Mini. Note that
  `renderer::Context` has no locked-pixels view yet; that is the open design
  question in
  [software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

What has *not* changed: neither renderer has been exercised under sustained
per-frame load by anything except `skratch`, and nothing has run on a device. An
effect running at a real frame rate would be the first evidence either way.

## Tasks

- [ ] `WREEL_ENABLE_MIDI` option, desktop presets only
- [ ] `util::midi` facade over RtMidi; enumerate and open ports
- [ ] `test_midi.cc` — parsing and dispatch against synthetic event bytes, no
      hardware required
- [ ] Parameter table with CC binding and smoothing
- [ ] JSON mapping file per controller, loaded through the `util::json` facade
- [ ] One effect end to end — a fragment shader through `gfx::gles2`, or a
      CPU-plotted one through `gfx::renderer` if the Miyoo Mini is a target for it
- [x] Frame timing — **done elsewhere.** `rig::FrameClock` landed with the coppers
      work and `skratch` holds one; the unconditional `SDL_Delay(10)` this task was
      written against survives only as a comment. A `visuals/` executable uses the
      same clock rather than growing its own
- [ ] A `visuals/` executable, separate from `skratch`

## Open questions

- Which controller? Axis and CC layouts are device-specific, and the mapping file
  format should be designed against a real one rather than in the abstract.
- Audio-reactive as well as MIDI-reactive? An FFT over captured audio is a
  different input path and a much larger scope. Probably a separate snapshot.
- Does this want its own render loop, or should `skratch`'s be generalised? Given
  `skratch` is welded to fixed-function GL and slated for rework, a fresh loop is
  likely cleaner than adapting it.

## References

- `include/gfx/renderer/context.hpp` and `include/gfx/gles2/` — what is available
  to draw with today
- [docs/TARGETS.md](../../docs/TARGETS.md)
- `scripts/bootstrap-debian.sh --midi`
