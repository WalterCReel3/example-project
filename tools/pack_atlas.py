#!/usr/bin/env python3
"""Pack per-animation frame PNGs into one sheet and a Sparrow atlas.

    tools/.venv/bin/python tools/pack_atlas.py \
        data/sunny-land/foxy --name foxy --out data

Input is a directory of animation directories, each holding frames named so
that a lexical sort is the playback order:

    foxy/idle/000.png  001.png  002.png  003.png
    foxy/run/000.png   ...

Output is `<out>/<name>.png` and `<out>/<name>.xml`, the latter in the Sparrow
/ Starling format `loaders::load_sparrow` reads. Frame ids are `<animation>.NNN`
-- `idle.000`, `run.005` -- which is the convention data/jetpackdude.xml already
uses and the one gfx::AnimatedSprite will sequence over.

Why a tool rather than a committed blob
---------------------------------------
The atlas is generated, so the thing under review is this script and the frames
it reads, not a binary nobody can diff. Re-running it must produce the same
bytes: the layout is a deterministic function of the sorted input, and nothing
here consults a clock, a hash seed or the filesystem order.

Trimming
--------
Transparent margins are cropped by default and recorded as Sparrow's
frameX/frameY/frameWidth/frameHeight, which is what a real packer does and what
makes the sheet smaller. Note the sign convention: Sparrow states the offset
from the *cropped* image back to the original's origin, so it is negative, and
the frame size is the size before cropping. include/gfx/atlas.hpp negates it on
load so that consumers add rather than subtract.

`--no-trim` keeps every frame at its authored size. Useful for a sheet destined
for code that indexes it as a uniform grid, where a variable cell size is worse
than the wasted pixels.

Layout
------
One row per animation, cells at the widest and tallest trimmed frame in that
row. Not the tightest possible packing, and deliberately: a row per animation
makes the sheet readable when something looks wrong, and the frames of one
animation stay contiguous both on the sheet and in the frame table. A tighter
packer is worth writing when a sheet stops fitting a target's texture cap --
the Miyoo Mini's is 640x480 (see planning/2026-07-31-miyoo-sdl2-fork/).
"""

import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET

try:
    from PIL import Image
except ImportError:  # pragma: no cover - a setup error, not a runtime one
    sys.exit(
        "Pillow is not installed. Create the tools virtualenv first:\n"
        "    python3 -m venv tools/.venv\n"
        "    tools/.venv/bin/pip install -r tools/requirements.txt"
    )


class Frame:
    """One source image, and where it ended up on the sheet."""

    def __init__(self, frame_id, image, trim):
        self.id = frame_id
        self.image = image  # cropped, if trimming
        self.trim_x, self.trim_y = trim  # offset of the crop in the original
        self.frame_width, self.frame_height = 0, 0  # 0x0 when untrimmed
        self.x, self.y = 0, 0  # assigned by layout()


def load_frames(animation_dir, trim):
    """Frames of one animation, in playback order."""
    frames = []
    for index, path in enumerate(sorted(animation_dir.glob("*.png"))):
        image = Image.open(path).convert("RGBA")
        frame = Frame(f"{animation_dir.name}.{index:03d}", image, (0, 0))

        if trim:
            box = image.getbbox()  # None for a fully transparent frame
            if box is None:
                # Keep one pixel rather than a zero-area region, which
                # load_sparrow rejects outright. A blank frame is legitimate in
                # a blink or a flash animation.
                box = (0, 0, 1, 1)
            if box != (0, 0, image.width, image.height):
                frame.frame_width, frame.frame_height = image.size
                frame.trim_x, frame.trim_y = box[0], box[1]
                frame.image = image.crop(box)

        frames.append(frame)
    return frames


def layout(animations):
    """Place frames, one animation per row. Returns the sheet size."""
    y = 0
    width = 0
    for frames in animations:
        row_height = max(f.image.height for f in frames)
        x = 0
        for frame in frames:
            frame.x, frame.y = x, y
            x += frame.image.width
        width = max(width, x)
        y += row_height
    return width, y


def build_sheet(animations, size):
    sheet = Image.new("RGBA", size, (0, 0, 0, 0))
    for frames in animations:
        for frame in frames:
            sheet.paste(frame.image, (frame.x, frame.y))
    return sheet


def build_xml(animations, image_name):
    root = ET.Element("TextureAtlas", {"imagePath": image_name})
    for frames in animations:
        for frame in frames:
            attributes = {
                "name": frame.id,
                "x": str(frame.x),
                "y": str(frame.y),
                "width": str(frame.image.width),
                "height": str(frame.image.height),
            }
            if frame.frame_width:
                # Negated back into Sparrow's own sense on the way out, the
                # inverse of what loaders/sparrow.cc does on the way in.
                attributes.update(
                    {
                        "frameX": str(-frame.trim_x),
                        "frameY": str(-frame.trim_y),
                        "frameWidth": str(frame.frame_width),
                        "frameHeight": str(frame.frame_height),
                    }
                )
            ET.SubElement(root, "SubTexture", attributes)

    ET.indent(root, space="    ")
    return ET.ElementTree(root)


DEFAULT_FPS = 12.0
DEFAULT_LOOP = "repeat"


def write_animations(animations, out, name, atlas_name, fps):
    """Seed or update the animation sidecar beside the atlas.

    The tool knows every animation and its frame count, so it writes the file
    rather than making someone type it. What it does *not* know is how fast each
    one should play or whether it loops -- those are authored, and this merges
    rather than overwrites so that regenerating an atlas never discards them.

    Concretely: frame counts are regenerated, `fps` and `loop` are preserved for
    any animation already in the file, and explicit <Frame> children are carried
    through untouched. A new animation arrives with defaults; one whose frames
    have disappeared is reported and dropped.
    """
    path = out / f"{name}.anim.xml"

    existing = {}
    if path.exists():
        for element in ET.parse(path).getroot().findall("Animation"):
            existing[element.get("name")] = element

    root = ET.Element("Animations", {"atlas": f"{name}.xml"})
    for frames in animations:
        animation = frames[0].id.rsplit(".", 1)[0]
        previous = existing.pop(animation, None)

        element = ET.SubElement(
            root,
            "Animation",
            {
                "name": animation,
                "fps": previous.get("fps") if previous is not None else f"{fps:g}",
                "loop": (
                    previous.get("loop") if previous is not None else DEFAULT_LOOP
                ),
                # Generated: a cross-check for the loader, which derives the
                # frame ids from the name and should find exactly this many.
                "frames": str(len(frames)),
            },
        )
        if previous is not None:
            for frame in previous.findall("Frame"):
                element.append(frame)

    for orphan in existing:
        print(f"  dropped {orphan}: no frames found for it", file=sys.stderr)

    ET.indent(root, space="    ")
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=True)
    print(f"{path}: {len(animations)} animations")


def verify(source, out, name):
    """Rebuild every frame from the atlas and compare it to its source.

    The check worth having, because the failure it catches is silent: a trim
    offset written with the wrong sign still produces a plausible sheet and a
    document that parses, and shows up only as sprites a few pixels out of
    place. Rebuilding the way a consumer must -- paste the region at the negated
    frameX/frameY inside a frameWidth x frameHeight canvas -- exercises the
    convention rather than restating it.
    """
    root = ET.parse(out / f"{name}.xml").getroot()
    sheet = Image.open(out / root.get("imagePath")).convert("RGBA")

    mismatches = 0
    subtextures = root.findall("SubTexture")
    for element in subtextures:
        frame_id = element.get("name")
        animation, index = frame_id.rsplit(".", 1)
        original = Image.open(
            source / animation / f"{int(index):03d}.png"
        ).convert("RGBA")

        def attribute(key):
            return int(element.get(key, 0))

        region = sheet.crop(
            (
                attribute("x"),
                attribute("y"),
                attribute("x") + attribute("width"),
                attribute("y") + attribute("height"),
            )
        )

        size = (
            attribute("frameWidth") or attribute("width"),
            attribute("frameHeight") or attribute("height"),
        )
        rebuilt = Image.new("RGBA", size, (0, 0, 0, 0))
        rebuilt.paste(region, (-attribute("frameX"), -attribute("frameY")))

        if rebuilt.size != original.size or rebuilt.tobytes() != original.tobytes():
            mismatches += 1
            print(
                f"  MISMATCH {frame_id}: rebuilt {rebuilt.size}, "
                f"source {original.size}",
                file=sys.stderr,
            )

    print(f"verified {len(subtextures)} frames, {mismatches} mismatches")
    return mismatches == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "source", type=pathlib.Path, help="directory of animation directories"
    )
    parser.add_argument(
        "--name", required=True, help="basename for the .png and .xml written"
    )
    parser.add_argument(
        "--out", type=pathlib.Path, default=pathlib.Path("data"),
        help="output directory (default: data)",
    )
    parser.add_argument(
        "--no-trim", dest="trim", action="store_false",
        help="keep frames at their authored size",
    )
    parser.add_argument(
        "--no-verify", dest="verify", action="store_false",
        help="skip the rebuild-and-compare pass",
    )
    parser.add_argument(
        "--no-animations", dest="animations_file", action="store_false",
        help="do not write the <name>.anim.xml sidecar",
    )
    parser.add_argument(
        "--fps", type=float, default=DEFAULT_FPS,
        help=f"frame rate for animations new to the sidecar "
             f"(default: {DEFAULT_FPS:g}); existing entries keep theirs",
    )
    args = parser.parse_args()

    directories = sorted(d for d in args.source.iterdir() if d.is_dir())
    if not directories:
        sys.exit(f"{args.source}: no animation directories")

    animations = [load_frames(d, args.trim) for d in directories]
    animations = [frames for frames in animations if frames]
    if not animations:
        sys.exit(f"{args.source}: no frames found")

    size = layout(animations)
    image_name = f"{args.name}.png"

    args.out.mkdir(parents=True, exist_ok=True)
    build_sheet(animations, size).save(args.out / image_name, optimize=True)
    build_xml(animations, image_name).write(
        args.out / f"{args.name}.xml", encoding="utf-8", xml_declaration=True
    )

    count = sum(len(frames) for frames in animations)
    trimmed = sum(1 for frames in animations for f in frames if f.frame_width)
    print(f"{args.out / image_name}: {size[0]}x{size[1]}")
    print(
        f"{args.out / (args.name + '.xml')}: {count} frames in "
        f"{len(animations)} animations, {trimmed} trimmed"
    )

    if args.animations_file:
        write_animations(animations, args.out, args.name, image_name, args.fps)

    if args.verify and not verify(args.source, args.out, args.name):
        sys.exit("atlas does not rebuild to its source frames")


if __name__ == "__main__":
    main()
