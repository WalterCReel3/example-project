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

`tools/.venv/` is gitignored; `requirements.txt` is pinned.

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
