# Asset tooling

Python that turns purchased or authored art into the files `data/` ships. These
run **offline, by hand**, and their output is committed — the build has no Python
dependency and the cross-compile targets never see this directory.

That split is deliberate. Six of the seven presets cross-compile, two of them
inside device SDK containers, and adding a host-side asset step to the build
would have to work in all of them. Generating offline and committing the result
means the thing under review is the script plus its inputs, and the thing built
is a plain file.

## Setup

```sh
python3 -m venv tools/.venv
tools/.venv/bin/pip install -r tools/requirements.txt
```

`tools/.venv/` is gitignored; `requirements.txt` is pinned. Only the tools that
touch images need it — `make_sfx.py` is stdlib-only and runs under a bare
`python3`.

## `pack_atlas.py`

Packs per-animation frame PNGs into one sheet plus a Sparrow atlas, the format
`loaders::load_sparrow` reads.

```sh
tools/.venv/bin/python tools/pack_atlas.py data/sunny-land/foxy \
    --name foxy --out data
```

Two properties worth knowing, because things downstream rely on them:

- **It is deterministic.** Layout is a function of the sorted input, so
  regenerating an unchanged atlas produces identical bytes and shows up as no
  diff. If a rerun churns `data/foxy.png`, something is wrong.
- **It verifies itself.** Every frame is rebuilt from the generated atlas and
  compared to its source before the tool exits successfully. This is the check
  that matters for trimming: Sparrow states the trim offset in the opposite
  sense to the one a blit wants, and getting the sign wrong produces a sheet
  that looks right, parses cleanly, and draws sprites a few pixels out of place.
  `--no-verify` skips it.

Frame ids are `<animation>.NNN`, and the frames of one animation are contiguous
in the document, which is what lets `gfx::animation_from_prefix` build an
animation from a name alone.

### The animation sidecar

Sparrow describes where the frames are and nothing about how they play, so the
tool also seeds `<name>.anim.xml` with one entry per animation:

```xml
<Animation name="run" fps="12" loop="repeat" frames="6"/>
```

**Edit it.** `fps` and `loop` are the authored half and the defaults are a
guess; a regenerate **merges** rather than overwrites, so frame counts come back
fresh and your tuning stays. Explicit `<Frame id="..."/>` children override the
derived frame list and survive a regenerate too — that is how frame reuse and
holds get expressed, since a repeated id holds that frame for another tick.

`--fps` sets the default for animations *new* to the file; existing entries keep
theirs. `--no-animations` skips the file entirely.

## `make_sfx.py`

Renders the effect descriptions in `data/sfx.xml` to `data/sfx/*.wav`.

```sh
python3 tools/make_sfx.py
python3 tools/make_sfx.py --check     # regeneration check; writes nothing
```

Stdlib only, so no virtualenv. Output is **22050 Hz mono 16-bit**, the handheld
mixer profile exactly, so the target that can least afford resampling does none
and the desktop's upsample is paid once inside `Mix_LoadWAV`.

**`data/sfx.xml` is a tool input and is not read at runtime.** Nothing in the
engine parses it; the game loads the WAVs. It is called out here because the
file looks exactly like `foxy.anim.xml`, which *is* loaded, and the resemblance
invites wiring it up.

Like the animation sidecar it is **merged, not overwritten**: an `<Effect>`
already in the file comes back untouched — attributes, children and your
comments — and only cues missing from it get seeded. The tool owns one
attribute, `samples`, which it regenerates as a cross-check on the rendered
length. Everything else is authored.

`--check` re-renders from the description and compares against the committed
WAVs without writing, which is how "regenerating produces identical bytes" gets
verified rather than asserted. `sha256` is printed per file and is the digest
`sha256sum` reports, so it can be checked without this tool.

`--rate` renders at some other sample rate for a listening pass. It deliberately
does **not** rewrite `data/sfx.xml` when it is not 22050, because `samples`
would then describe a file that is not the one committed. Point `--out` at a
scratch directory for those; the tool refuses to write anywhere outside `data/`.

### What makes it sound 8-bit

Four properties, all load-bearing rather than stylistic, and worth not
"improving":

- **Duty cycle is the timbre knob**, not the waveform. 12.5 % is thin and reedy,
  50 % hollow and full; a square generator without it can only make one sound.
- **Noise is a real 15-bit LFSR** clocked at one of the 2A03's sixteen periods,
  not `random()`. Stepping the pitch means stepping the period, which is what
  makes the descending bump cue read as 8-bit rather than as filtered hiss.
- **Nothing is band-limited.** The naive square aliases at 22050 Hz and that
  aliasing is the requested sound. PolyBLEP would make it cleaner and wrong.
- **1–2 ms linear fade at both ends of every effect**, applied after the
  envelope and unconditionally. A square starting mid-cycle at full amplitude
  pops louder than the effect, and it is inaudible until it is played through
  something with bass response.

A **runtime** sound generator — waveforms, envelopes, modulation, effects,
parameterised per event — is wanted eventually and is out of scope for now. It
is separate work rather than a later version of this tool: it lives in the
engine, has to be real-time safe next to the mixer callback, and would make
pitch-per-event possible in a way pre-rendered chunks cannot. This tool neither
blocks it nor should grow toward it.

## `make_tilemap.py`

Generates the Tiled fixture — `tileset.png`, `sunnyland.tsx` and
`sunnyland.tmx` — from the Sunny Land pack.

```sh
tools/.venv/bin/python tools/make_tilemap.py --out data
```

Deterministic, like the atlas packer, so an unchanged map regenerates to
identical bytes. Unlike the atlas packer there is nothing here to hand-tune yet:
the map is a fixed layout function. Edit it in Tiled and the tool will overwrite
you — either stop running it, or move the change into `build_layers()`.

Two properties are load-bearing for the fill-rate measurement, so do not
"improve" them casually: the map is **larger than any target panel**, so a
renderer has to cull rather than draw everything, and its backdrop layer covers
**every cell**, so a screenful is a screenful of blits. A map with sky would
leave half the target unpainted and report a number that flatters the renderer.
