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
copper bars, starfield, feedback. On desktop these are shader-friendly, which
argues for building this *after* `gles2` exists — but plasma and rotozoom are
famously CPU-plotted effects, and the `software` backend can already do direct
pixel work. Starting there costs nothing and needs no new renderer.

## Why this might come before `gles2`

The software backend can carry a real plasma or rotozoom today. Building one or
two CPU effects first would:

- exercise `gfx::software::Context` under sustained per-frame load, which nothing
  currently does
- surface whatever the software backend is missing (direct framebuffer access,
  double buffering, timing) while the API is still cheap to change
- produce something demoable without waiting on hardware or a shader pipeline

That is a stronger argument than it first looks. The software backend is currently
unexercised beyond a headless smoke test.

## Tasks

- [ ] `WREEL_ENABLE_MIDI` option, desktop presets only
- [ ] `util::midi` facade over RtMidi; enumerate and open ports
- [ ] `test_midi.cc` — parsing and dispatch against synthetic event bytes, no
      hardware required
- [ ] Parameter table with CC binding and smoothing
- [ ] JSON mapping file per controller, loaded through the `util::json` facade
- [ ] One CPU effect on the software backend (plasma or rotozoom)
- [ ] Frame timing: the demo loop currently `SDL_Delay(10)`s unconditionally,
      which is neither a frame cap nor vsync
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

- `include/gfx/software/context.hpp` — what is available to draw with today
- [docs/TARGETS.md](../../docs/TARGETS.md)
- `scripts/bootstrap-debian.sh --midi`
