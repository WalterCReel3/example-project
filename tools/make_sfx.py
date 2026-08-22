#!/usr/bin/env python3
"""Render the 8-bit effect descriptions in data/sfx.xml to WAV.

    python3 tools/make_sfx.py --description data/sfx.xml --out data/sfx

`data/sfx.xml` is a TOOL INPUT. It is not read at runtime and nothing in the
engine parses it -- the game loads the WAVs this writes and nothing else. If you
find yourself adding an XML loader for it, that is the wrong turn.

Stdlib only: `wave`, `xml.etree`, `math`. Unlike pack_atlas.py there is nothing
to install, so `python3` is enough and the tools virtualenv is optional.

Output format
-------------
22050 Hz, mono, signed 16-bit. That is the handheld mixer profile exactly (see
include/audio/device.hpp: 22050 / 2048 / 8 voices on a handheld, 44100 / 1024 /
16 on desktop), so the target that can least afford resampling does none; the
desktop's upsample happens once inside Mix_LoadWAV.

`--rate` can override it, and the committed assets are the default. The rate is
deliberately a command-line option rather than an attribute in the description:
the description is hand-tuned and a rate living in it would be one edit away
from shipping assets the handheld has to resample. Rendering a 44100 copy
somewhere scratch for a listening pass is a different act from regenerating
data/sfx/, and this keeps them different.

Why offline
-----------
Generating buffers next to an audio callback on a device whose SDL port has
needed fixing repeatedly buys a few kilobytes and costs a class of bug that only
reproduces on hardware. Rendering ahead of time also makes the effects
*listenable*, which is the only real review method for a sound, and byte-
identical regeneration is checkable by sha256 -- the property pack_atlas.py
already established for the atlas.

A **runtime** sound generator -- waveforms, amplitude envelopes, modulation,
effects, parameterised per event -- is wanted eventually and is out of scope
now. It is a different piece of work from this one, not a later version of it:
it lives in the engine, it has to be real-time safe next to the mixer callback,
and it makes pitch-per-event possible in a way pre-rendered chunks cannot. This
tool does not block it and should not grow toward it -- when that lands, the
generators here are the reference for what the sound is supposed to be.

The authored artifact is the description, not the WAV
-----------------------------------------------------
`data/sfx.xml` is seeded with defaults on first run and hand-tuned afterwards. A
regenerate **merges**: every effect already in the file is kept exactly as
authored, comments included, and only cues missing from it are added. The same
arrangement `foxy.anim.xml` uses. The one attribute this tool owns is `samples`,
regenerated as a cross-check on the rendered length.

What makes it sound 8-bit rather than merely synthesised
--------------------------------------------------------
Four things, all deliberate:

- **Duty cycle is the timbre knob.** The 2A03 offers 12.5 / 25 / 50 / 75 %;
  12.5 % is thin and reedy, 50 % hollow and full. A square generator without a
  duty parameter can only make one sound.
- **Noise is a real 15-bit LFSR**, clocked at one of the 2A03's sixteen periods.
  Stepping the pitch means stepping that period. Filtered white noise does not
  sound the same.
- **Nothing is band-limited.** A naive square aliases at 22050 Hz and the
  aliasing is part of the sound being asked for. PolyBLEP would make it cleaner
  and wrong.
- **A 1-2 ms linear fade at both ends of every effect.** A square that starts
  mid-cycle at full amplitude pops, and the pop is louder than the effect. It is
  inaudible on laptop speakers and obvious through anything with bass response,
  which is why it is applied unconditionally rather than left to the envelope.

One chunk per event
-------------------
Pitch movement lives inside the sample -- the jump cue is a glissando rendered
once. There is deliberately no pitch-variant set and no streak ladder:
SDL2_mixer cannot pitch-shift a chunk, so anything that varies pitch per event
has to be pre-rendered as separate chunks, and that machinery is being declined.
"""

import argparse
import hashlib
import math
import pathlib
import sys
import wave
import xml.etree.ElementTree as ET

# The handheld mixer profile. `--rate` overrides the first of these; mono
# 16-bit is not negotiable for a set of one-shot blips.
default_rate = 22050
sample_width = 2
channels = 1

# The 2A03's NTSC noise periods, in CPU cycles. Index 0 is the fastest (most
# hiss-like); index 15 is slow enough to read as a rattle. "Stepping the pitch
# down" is walking up this table.
noise_periods = (
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
)
cpu_hz = 1789773.0  # NTSC 2A03

# The full-scale fade the plan asks for, in milliseconds, if an effect does not
# state its own.
default_fade_ms = 1.5


def milliseconds_to_samples(ms, rate):
    return max(1, int(round(ms * rate / 1000.0)))


def envelope(count, attack_ms, decay_ms, sustain, release_ms, rate):
    """Per-sample amplitude for one segment.

    Attack ramps 0 -> 1, decay falls 1 -> sustain, the sustain level holds, and
    release falls sustain -> 0 at the tail. When the three timed stages do not
    fit in the segment they are scaled down proportionally rather than clipped,
    so shortening a note never silently drops its release.
    """
    attack = milliseconds_to_samples(attack_ms, rate) if attack_ms > 0 else 0
    decay = milliseconds_to_samples(decay_ms, rate) if decay_ms > 0 else 0
    release = milliseconds_to_samples(release_ms, rate) if release_ms > 0 else 0

    timed = attack + decay + release
    if timed > count:
        scale = count / timed
        attack = int(attack * scale)
        decay = int(decay * scale)
        release = count - attack - decay

    hold = count - attack - decay - release
    values = []
    for i in range(attack):
        values.append(i / attack)
    for i in range(decay):
        values.append(1.0 + (sustain - 1.0) * (i + 1) / decay)
    level = sustain if decay else 1.0
    values.extend([level] * hold)
    for i in range(release):
        values.append(level * (1.0 - (i + 1) / release))
    return values


def render_square(count, duty, start_hz, end_hz, rate):
    """A naive -- deliberately aliasing -- square, swept geometrically.

    Phase is accumulated rather than recomputed from the sample index, so a
    glissando stays phase-continuous; recomputing would step the waveform every
    time the frequency changes and buzz.
    """
    ratio = end_hz / start_hz
    phase = 0.0
    values = []
    for i in range(count):
        t = i / (count - 1) if count > 1 else 0.0
        values.append(1.0 if phase < duty else -1.0)
        phase += (start_hz * ratio ** t) / rate
        phase -= math.floor(phase)
    return values


def render_noise(count, periods, tap, rate):
    """The 2A03 noise channel: a 15-bit LFSR clocked at a table period.

    `tap` is 1 for the long sequence (32767 steps, hiss) and 6 for the short one
    (93 steps, short enough to be periodic, which reads as metallic). The
    register is seeded to 1, the hardware's power-on state; it is never allowed
    to reach zero, the one state this feedback cannot leave.

    The hardware silences the channel when bit 0 is set, so as a bipolar signal
    that bit selects the sign.
    """
    lfsr = 0x0001
    accumulator = 0.0
    values = []
    for i in range(count):
        period = noise_periods[periods[i * len(periods) // count]]
        accumulator += (cpu_hz / period) / rate
        while accumulator >= 1.0:
            accumulator -= 1.0
            feedback = (lfsr ^ (lfsr >> tap)) & 1
            lfsr = (lfsr >> 1) | (feedback << 14)
        values.append(-1.0 if (lfsr & 1) else 1.0)
    return values


def attribute(element, key, default):
    value = element.get(key)
    return default if value is None else type(default)(value)


def render_segment(element, rate):
    """One <Square> or <Noise> child, enveloped, as floats in [-1, 1]."""
    count = milliseconds_to_samples(attribute(element, "ms", 100.0), rate)

    if element.tag == "Square":
        duty = attribute(element, "duty", 0.5)
        if not 0.0 < duty < 1.0:
            sys.exit(f"duty must be in (0, 1), got {duty}")
        start_hz = attribute(element, "from", 440.0)
        end_hz = attribute(element, "to", start_hz)
        if start_hz <= 0.0 or end_hz <= 0.0:
            sys.exit(f"frequencies must be positive, got {start_hz}->{end_hz}")
        values = render_square(count, duty, start_hz, end_hz, rate)
    elif element.tag == "Noise":
        periods = [int(p) for p in element.get("periods", "8").split(",")]
        if not periods or any(not 0 <= p < len(noise_periods) for p in periods):
            sys.exit(f"noise periods must be 0..15, got {periods}")
        mode = element.get("mode", "long")
        if mode not in ("long", "short"):
            sys.exit(f"noise mode must be long or short, got {mode}")
        values = render_noise(count, periods, 1 if mode == "long" else 6, rate)
    else:
        sys.exit(f"unknown segment <{element.tag}>")

    gain = attribute(element, "gain", 1.0)
    shape = envelope(
        count,
        attribute(element, "attack", 1.0),
        attribute(element, "decay", 0.0),
        attribute(element, "sustain", 1.0),
        attribute(element, "release", 0.0),
        rate,
    )
    return [v * a * gain for v, a in zip(values, shape)]


def apply_fade(values, fade_ms, rate):
    """Linear fade in and out, in place, over the whole effect.

    Applied last and unconditionally -- after the envelopes, which may already
    end near zero. "Near zero" is what pops.
    """
    count = min(milliseconds_to_samples(fade_ms, rate), len(values) // 2)
    for i in range(count):
        gain = i / count
        values[i] *= gain
        values[-1 - i] *= gain
    return values


def render_effect(element, rate):
    """The whole effect: its segments concatenated, then faded, as int16."""
    values = []
    for segment in element:
        # Comments are real children here, because the parser keeps them so
        # that hand-written notes survive a merge. Everything else has to be a
        # segment: silently skipping an unrecognised tag would turn a typo into
        # a missing sound rather than an error.
        if segment.tag is ET.Comment or segment.tag is ET.ProcessingInstruction:
            continue
        values.extend(render_segment(segment, rate))
    if not values:
        sys.exit(f"{element.get('name')}: no <Square> or <Noise> segments")

    gain = attribute(element, "gain", 0.6)
    values = [v * gain for v in values]
    apply_fade(values, attribute(element, "fade", default_fade_ms), rate)

    frames = bytearray()
    for value in values:
        sample = max(-32768, min(32767, int(round(value * 32767.0))))
        frames += sample.to_bytes(2, "little", signed=True)
    return bytes(frames)


def write_wav(path, frames, rate):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(channels)
        out.setsampwidth(sample_width)
        out.setframerate(rate)
        out.writeframes(frames)


def read_wav(path):
    """(channels, sample width, rate), and the sample bytes."""
    with wave.open(str(path), "rb") as source:
        header = (
            source.getnchannels(),
            source.getsampwidth(),
            source.getframerate(),
        )
        return header, source.readframes(source.getnframes())


def verify(path, frames, rate):
    """Re-read the file and confirm it is the format that was asked for.

    Worth the eight lines: a wrong framerate or a stereo header produces a WAV
    that plays fine on the desk and makes Mix_LoadWAV resample on the one target
    that cannot afford it, and nothing about the sound gives it away.
    """
    header, data = read_wav(path)
    expected = (channels, sample_width, rate)
    if header != expected:
        print(
            f"  {path}: header {header}, expected {expected}",
            file=sys.stderr,
        )
        return False
    if data != frames:
        print(
            f"  {path}: content differs from what was rendered",
            file=sys.stderr,
        )
        return False
    return True


# Seeded on first run and hand-tuned afterwards; a regenerate never overwrites
# an effect already in the description. Frequencies are equal-tempered A440.
default_effects = """\
<Effects>
    <!-- Tool input. Not read at runtime: the game loads the WAVs.
         Rendered to 22050 Hz mono by tools/make_sfx.py; the rate lives in the
         tool, not here. Times are milliseconds, frequencies hertz, duty a
         fraction in (0, 1), noise periods indices into the 2A03's table where
         0 is the fastest. `samples` is regenerated by the tool. -->

    <!-- Glissando, root to a fifth above, over its own duration. Thin 12.5%
         duty so it reads as a jump rather than a bass note. -->
    <Effect name="jump" gain="0.55" fade="1.5">
        <Square duty="0.125" from="392" to="587.33" ms="70"
                attack="1" decay="0" sustain="1" release="16" />
    </Effect>

    <!-- Impact then body: a short noise transient followed by a falling
         hollow square. Nothing overlaps: one chunk means one voice. -->
    <Effect name="land" gain="0.5" fade="1.5">
        <Noise mode="long" periods="10,12" ms="18"
               attack="1" decay="14" sustain="0.15" release="3" />
        <Square duty="0.5" from="165" to="98" ms="46"
                attack="1" decay="30" sustain="0.3" release="14" />
    </Effect>

    <!-- Two-step coin blip, quarter duty for brightness. -->
    <Effect name="pickup" gain="0.5" fade="1.5">
        <Square duty="0.25" from="1318.51" to="1318.51" ms="26"
                attack="1" decay="0" sustain="1" release="2" />
        <Square duty="0.25" from="1975.53" to="1975.53" ms="72"
                attack="1" decay="0" sustain="1" release="34" />
    </Effect>

    <!-- The bump that sits foxy down dizzy: descending noise, i.e. walking up
         the period table, over a quarter second. -->
    <Effect name="bump" gain="0.5" fade="1.5">
        <Noise mode="long" periods="4,6,8,9,10,11,12,13" ms="260"
               attack="1" decay="190" sustain="0.28" release="60" />
    </Effect>

    <!-- Level complete: a rising arpeggio into a held octave. -->
    <Effect name="level-complete" gain="0.5" fade="2.0">
        <Square duty="0.5" from="523.25" to="523.25" ms="66"
                attack="1" decay="0" sustain="1" release="3" />
        <Square duty="0.5" from="659.25" to="659.25" ms="66"
                attack="1" decay="0" sustain="1" release="3" />
        <Square duty="0.5" from="783.99" to="783.99" ms="66"
                attack="1" decay="0" sustain="1" release="3" />
        <Square duty="0.25" from="1046.5" to="1046.5" ms="240"
                attack="1" decay="60" sustain="0.55" release="120" />
    </Effect>
</Effects>
"""


def comment_preserving_parser():
    """A parser that keeps comments, so hand-written notes survive a merge."""
    return ET.XMLParser(target=ET.TreeBuilder(insert_comments=True))


def load_description(path):
    """The description, seeded if absent and topped up if incomplete.

    Merge, not overwrite: an <Effect> already present is returned untouched,
    whatever has been done to its attributes and children. Only cues missing
    from the file are appended. This is the same contract pack_atlas.py has with
    foxy.anim.xml and it is what makes this file the authored artifact.
    """
    seeded = ET.fromstring(default_effects, parser=comment_preserving_parser())
    if not path.exists():
        print(f"{path}: seeded with {len(seeded.findall('Effect'))} effects")
        return seeded

    root = ET.parse(path, parser=comment_preserving_parser()).getroot()
    present = {e.get("name") for e in root.findall("Effect")}
    added = [
        e for e in seeded.findall("Effect") if e.get("name") not in present
    ]
    for element in added:
        root.append(element)
    if added:
        names = ", ".join(e.get("name") for e in added)
        print(f"{path}: added missing default effects: {names}")
    return root


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--description", type=pathlib.Path,
        default=pathlib.Path("data/sfx.xml"),
        help="effect description to merge into (default: data/sfx.xml)",
    )
    parser.add_argument(
        "--out", type=pathlib.Path, default=pathlib.Path("data/sfx"),
        help="output directory for the WAVs (default: data/sfx)",
    )
    parser.add_argument(
        "--rate", type=int, default=default_rate,
        help=f"sample rate (default: {default_rate}, the handheld mixer's; the "
             f"committed assets are that and changing it is not a regenerate)",
    )
    parser.add_argument(
        "--check", action="store_true",
        help="render and compare against the committed WAVs; write nothing",
    )
    parser.add_argument(
        "--no-verify", dest="verify", action="store_false",
        help="skip the read-back format check",
    )
    args = parser.parse_args()

    if args.rate <= 0:
        sys.exit(f"--rate must be positive, got {args.rate}")

    # The tool writes assets, so it writes inside data/ or not at all.
    data_root = pathlib.Path("data").resolve()
    if not data_root.is_dir():
        sys.exit("data/ not found: run this from the repository root")
    for path in (args.out, args.description):
        resolved = path.resolve()
        if data_root != resolved and data_root not in resolved.parents:
            sys.exit(f"{path}: outside data/, refusing")

    root = load_description(args.description)
    effects = root.findall("Effect")
    if not effects:
        sys.exit(f"{args.description}: no <Effect> elements")

    names = [e.get("name") for e in effects]
    if len(set(names)) != len(names) or not all(names):
        sys.exit(f"{args.description}: effects need unique non-empty names")

    if not args.check:
        args.out.mkdir(parents=True, exist_ok=True)

    failures = 0
    for element in effects:
        frames = render_effect(element, args.rate)
        count = len(frames) // sample_width
        # Generated: the cross-check on what the description asked for. It
        # counts samples, so it is only meaningful at the rate the committed
        # assets use; see the guard on rewriting the description below.
        if args.rate == default_rate:
            element.set("samples", str(count))

        path = args.out / f"{element.get('name')}.wav"
        if args.check:
            if not path.exists():
                print(f"  MISSING {path}", file=sys.stderr)
                failures += 1
                continue
            header, committed = read_wav(path)
            if header != (channels, sample_width, args.rate):
                print(f"  MISMATCH {path}: header {header}", file=sys.stderr)
                failures += 1
                continue
            if committed != frames:
                on_disk = len(committed) // sample_width
                detail = (
                    f"{on_disk} frames on disk, {count} rendered"
                    if on_disk != count
                    else f"same {count} frames, different samples"
                )
                print(f"  MISMATCH {path}: {detail}", file=sys.stderr)
                failures += 1
                continue
        else:
            write_wav(path, frames, args.rate)
            if args.verify and not verify(path, frames, args.rate):
                failures += 1
                continue

        # Over the whole file, not just the sample data, so it is the digest
        # `sha256sum data/sfx/*.wav` reports and anyone can check it without
        # this tool. The wave module's header is fixed-size and deterministic,
        # so it adds nothing that varies between runs.
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        print(
            f"{path}: {count} frames, {count * 1000.0 / args.rate:.1f} ms, "
            f"sha256 {digest}"
        )

    if not args.check:
        # A render at any other rate is a scratch listening copy, not a
        # regenerate. Writing the description back would put that run's sample
        # counts -- and anything it seeded -- into the file that describes the
        # committed 22050 Hz assets.
        if args.rate != default_rate:
            print(
                f"{args.description}: left alone, --rate {args.rate} is not "
                f"{default_rate}"
            )
        else:
            ET.indent(root, space="    ")
            ET.ElementTree(root).write(
                args.description, encoding="utf-8", xml_declaration=True
            )
            print(f"{args.description}: {len(effects)} effects")

            stale = sorted(
                p.name for p in args.out.glob("*.wav")
                if p.stem not in set(names)
            )
            for orphan in stale:
                print(f"  stale {args.out / orphan}: no effect of that name",
                      file=sys.stderr)

    if failures:
        sys.exit(f"{failures} effect(s) did not match")


if __name__ == "__main__":
    main()
