#!/usr/bin/env python3
"""Generate the Tiled fixture: a .tsx tileset and a .tmx map.

    tools/.venv/bin/python tools/make_tilemap.py --out data

Writes `tileset.png` (copied from the Sunny Land pack), `sunnyland.tsx` and
`sunnyland.tmx`. The output is what Tiled itself writes for the same map, so it
opens and edits in Tiled — this generates a starting point, it does not replace
the editor.

Why generated rather than authored
----------------------------------
The snapshot asks for a fixture authored in Tiled. Tiled is a GUI and there is
no way to drive it from here, so this produces an equivalent file instead: same
format, same attribute set, CSV layer data, an external tileset reference. The
difference that matters is honesty about where it came from, which
data/PROVENANCE.md records.

It is deterministic — the layout is a fixed function with no clock and no RNG
seed — so regenerating an unchanged map is a no-op in the diff.

Format decisions, both recorded in the snapshot
-----------------------------------------------
CSV layer data, not base64+zlib: the compressed form would pull in a zlib
dependency the project does not currently expose, for a map that is 9 KB as
text.

An *external* .tsx rather than an embedded <tileset>, because that is the path
loaders::load_tmx has to support anyway — Tiled writes external tilesets by
default once a tileset is shared between maps, so a loader that only handles the
embedded form fails on the first real map it meets.
"""

import argparse
import pathlib
import shutil
import sys
import xml.etree.ElementTree as ET

# Tiles picked out of the pack's tileset by eye; see the labelled contact sheet
# in the commit that added this. Ids are 0-based into a 25-column grid, which is
# what a .tsx numbers from — the +1 to a TMX gid happens at write time.
GRASS = (26, 28, 30)          # grass-topped dirt, three variants
DIRT = (76, 80, 126, 128, 130)  # fill below the surface
PLATFORM = (34, 35, 36)       # floating platform: left, middle, right
DECOR = (176, 178, 180, 182, 184, 186)  # grass tufts, rocks, bushes
# Fully opaque cave brick. The backdrop uses these rather than leaving sky,
# which makes every cell on screen cost a blit — see build_layers().
BACKDROP = (417, 418, 419, 414, 415)

TILE = 16
COLUMNS = 25          # of the tileset image
EMPTY = 0             # a TMX gid of 0 means "no tile"


def surface_height(column, rows):
    """Ground height at a column, as a row index. Deterministic and gentle.

    Not noise: a fixture wants to look like a level rather than like terrain,
    and a shape that can be read off the source is easier to check a renderer
    against than one that cannot.
    """
    base = rows - 6
    if 18 <= column < 24:
        base -= 2
    if 30 <= column < 40:
        base -= 4
    if 46 <= column < 52:
        base -= 1
    return base


def build_layers(cols, rows):
    """The three tile layers, each a flat list of 0-based tile ids or EMPTY."""
    ground = [EMPTY] * (cols * rows)
    decoration = [EMPTY] * (cols * rows)
    # A cave wall under everything, so the map is a subterranean level rather
    # than a surface one with an empty sky.
    #
    # That is a fixture decision with a measuring purpose behind it: with the
    # backdrop filled, every cell on screen costs a blit and the map is the
    # worst case the fill-rate risk in the snapshot is actually about. A sky
    # would leave half the screen free and quietly flatter the renderer.
    backdrop = [
        BACKDROP[(x * 7 + y * 13) % len(BACKDROP)]
        for y in range(rows)
        for x in range(cols)
    ]

    for x in range(cols):
        top = surface_height(x, rows)
        ground[top * cols + x] = GRASS[x % len(GRASS)]
        for y in range(top + 1, rows):
            ground[y * cols + x] = DIRT[(x + y) % len(DIRT)]

        # Something standing on the surface every few columns.
        if x % 7 == 3:
            decoration[(top - 1) * cols + x] = DECOR[(x // 7) % len(DECOR)]

    # Floating platforms, three tiles wide, well clear of the surface.
    for start_x, y in ((8, rows - 11), (26, rows - 14), (43, rows - 12)):
        for i, tile in enumerate(PLATFORM):
            decoration[y * cols + start_x + i] = tile

    return [("backdrop", backdrop), ("ground", ground),
            ("decoration", decoration)]


def csv_data(tiles, cols):
    """Layer data as Tiled writes it: gids, comma separated, one row per line."""
    lines = []
    for y in range(0, len(tiles), cols):
        row = tiles[y:y + cols]
        lines.append(",".join(str(t + 1 if t != EMPTY else 0) for t in row))
    return "\n" + ",\n".join(lines) + "\n"


def write_tileset(out, image_name, image_size):
    tile_count = COLUMNS * (image_size[1] // TILE)

    root = ET.Element("tileset", {
        "version": "1.10",
        "tiledversion": "1.10.2",
        "name": "sunnyland",
        "tilewidth": str(TILE),
        "tileheight": str(TILE),
        "tilecount": str(tile_count),
        "columns": str(COLUMNS),
    })
    ET.SubElement(root, "image", {
        "source": image_name,
        "width": str(image_size[0]),
        "height": str(image_size[1]),
    })

    path = out / "sunnyland.tsx"
    ET.indent(root, space=" ")
    ET.ElementTree(root).write(path, encoding="UTF-8", xml_declaration=True)
    return path, tile_count


def write_map(out, cols, rows, layers):
    root = ET.Element("map", {
        "version": "1.10",
        "tiledversion": "1.10.2",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "width": str(cols),
        "height": str(rows),
        "tilewidth": str(TILE),
        "tileheight": str(TILE),
        "infinite": "0",
        "nextlayerid": str(len(layers) + 2),
        "nextobjectid": "3",
    })

    # External, so the loader has to resolve a .tsx relative to the map.
    ET.SubElement(root, "tileset", {"firstgid": "1", "source": "sunnyland.tsx"})

    for index, (name, tiles) in enumerate(layers, start=1):
        layer = ET.SubElement(root, "layer", {
            "id": str(index),
            "name": name,
            "width": str(cols),
            "height": str(rows),
        })
        data = ET.SubElement(layer, "data", {"encoding": "csv"})
        data.text = csv_data(tiles, cols)

    # One object layer, because a map format that only carries tiles is not the
    # one Tiled writes and a loader that ignores objects will meet them anyway.
    group = ET.SubElement(root, "objectgroup",
                          {"id": str(len(layers) + 1), "name": "objects"})
    spawn_y = surface_height(4, rows) * TILE
    ET.SubElement(group, "object", {
        "id": "1", "name": "spawn", "type": "point",
        "x": str(4 * TILE), "y": str(spawn_y),
    })
    ET.SubElement(group, "object", {
        "id": "2", "name": "goal", "type": "point",
        "x": str((cols - 6) * TILE), "y": str(surface_height(cols - 6, rows) * TILE),
    })

    path = out / "sunnyland.tmx"
    ET.indent(root, space=" ")
    ET.ElementTree(root).write(path, encoding="UTF-8", xml_declaration=True)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("data"))
    parser.add_argument(
        "--source", type=pathlib.Path,
        default=pathlib.Path("data/Sunny-land-files/Assets/environment/tileset.png"),
        help="the pack's tileset image, copied into --out",
    )
    # 64x36 at 16px is 1024x576 — larger than any target panel, so a renderer
    # has to cull rather than draw the whole map.
    parser.add_argument("--cols", type=int, default=64)
    parser.add_argument("--rows", type=int, default=36)
    args = parser.parse_args()

    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow is not installed; see tools/README.md")

    if not args.source.exists():
        sys.exit(
            f"{args.source}: not found. The Sunny Land pack is gitignored — see "
            "data/PROVENANCE.md for what it is and where it goes."
        )

    args.out.mkdir(parents=True, exist_ok=True)
    image_name = "tileset.png"
    shutil.copyfile(args.source, args.out / image_name)
    image_size = Image.open(args.out / image_name).size

    tsx, tile_count = write_tileset(args.out, image_name, image_size)
    layers = build_layers(args.cols, args.rows)
    tmx = write_map(args.out, args.cols, args.rows, layers)

    print(f"{args.out / image_name}: {image_size[0]}x{image_size[1]}")
    print(f"{tsx}: {tile_count} tiles of {TILE}x{TILE}, {COLUMNS} columns")
    print(f"{tmx}: {args.cols}x{args.rows}, {len(layers)} tile layers")


if __name__ == "__main__":
    main()
