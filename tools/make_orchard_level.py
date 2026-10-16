#!/usr/bin/env python3
"""Generate orchard's level: data/orchard.tmx, against data/sunnyland.tsx.

    tools/.venv/bin/python tools/make_orchard_level.py --out data

Writes `orchard.tmx` and nothing else. The tileset it references —
`sunnyland.tsx` and `tileset.png` — is already in the tree, so this needs
neither Pillow nor the gitignored Sunny Land bundle and runs in a clean
checkout, which `make_tilemap.py` does not.

A sibling of make_tilemap.py rather than a flag on it
-----------------------------------------------------
`data/sunnyland.tmx` is deliberately the *worst* case: decision 4 of
planning/2026-08-10-game-layer-and-demo/ has its backdrop covering every cell so
that every screen cell costs a blit and the fill-rate meter moves. This level is
built the other way round — no backdrop at all, sky is `Context::clear()` — so
one generator could serve both only through a flag that means "be good" or "be
bad". `sunnyland.tmx` is also a fixture several tests assert against, and
regenerating it to share code here would churn them for nothing.

What is shared is the art: the same pack, the same `tileset.png`, the same
external `sunnyland.tsx`. A second tileset would buy nothing.

Format decisions, inherited unchanged from the sibling and recorded in the
snapshot: CSV layer data rather than base64+zlib, because the compressed form
needs a zlib dependency this project has not taken; an *external* `.tsx` rather
than an embedded `<tileset>`, because that is the path `loaders::load_tmx` has to
support anyway.

It is deterministic — the layout is a fixed function of the tables below, with no
clock and no RNG seed — so regenerating an unchanged map is a no-op in the diff.

Why generated rather than authored
----------------------------------
Tiled is a GUI and there is no way to drive it from here, so this writes what
Tiled writes for the same map: same format, same attribute set, CSV data, an
external tileset reference. It opens and edits in Tiled. `data/PROVENANCE.md`
records that plainly rather than letting the `tiledversion` attribute imply an
editor has ever touched the file.

The shape, and the numbers behind it
------------------------------------
96x40 tiles of 16 px is 1536x640 pixels. The Miyoo Mini's panel is 640x480, so
the level is 2.4 screens wide and 1.33 screens tall: the camera has 896 px of
horizontal travel and 160 px of vertical, which exercises `game::Camera`'s clamp
on both axes instead of pinning one of them flat. A map exactly 30 rows tall
never scrolls vertically at all. The cost is bounded: `game::Level` copies the
collision layer, and 96x40 ints is 15 KB against the smallest target's 128 MB.

Three layers, which is decision 4's two visible ones plus the invisible
collision layer:

    ground      grass, soil and the branch platforms — everything solid
    canopy      tufts, sprigs and stones standing on the surface, solid to
                nothing
    collision   one marker tile per solid cell, visible="0"

The collision layer is built from the same `solid` decision as `ground` in the
same pass, so the two cannot drift. Decision 2 makes solidity a layer rather
than a per-tile property; that layer is what `game::Level` reads.

There is no tree in this tileset. The pack's environment sheet is a cave and
hillside set — the closest things to foliage are grass tufts and two leafy
sprigs — so the orchard is in the fruit rather than in the trees, and the
canopy layer carries undergrowth.

THE ONE NUMBER A CHANGE TO ORCHARD WOULD INVALIDATE: the branch platforms step
up in threes, 3 rows or 48 px at a time, and the first of each climb is 2 or 3
rows above the ground beneath it. That is the jump height orchard has to
provide. If the demo's jump turns out lower, `BRANCHES` is what changes, not the
physics.
"""

import argparse
import pathlib
import sys
import xml.etree.ElementTree as ET

TILE = 16
EMPTY = 0  # a TMX gid of 0 means "no tile"; the +1 happens at write time

# Tile ids, 0-based into sunnyland.tsx's 25-column grid. GRASS and DIRT are the
# sibling's selection unchanged; the rest were read off the same sheet.
GRASS = (26, 28, 30)            # grass-topped soil, three variants
DIRT = (76, 80, 126, 128, 130)  # fill below the surface

# A log platform: rounded left cap, flush middle, rounded right cap. The sheet
# carries each piece twice — 34 == 85, 35 == 133, 83 == 134 — and the copies are
# pixel-identical, so which of each pair is named here is arbitrary. What is not
# arbitrary is that a platform's right end is 83 and not 36: 36 is a plain soil
# block (== 82 == 86 == 132), and the tile that terminates the log is one row
# further down the sheet than the two that start it.
BRANCH = (34, 35, 83)
UNDERGROWTH = (176, 178, 184, 186)  # grass tufts and leafy sprigs
STONE = (180, 182)              # loose rocks
COLLISION_MARK = 103            # a plain opaque block: the layer reads as a
                                # mask when it is switched on in Tiled

# Fixed rather than an argument, unlike the sibling's --cols/--rows: every table
# below is written in absolute columns, so a different width would need a
# different level rather than the same one stretched.
COLS = 96
ROWS = 40

# Rows of soil under the lowest ground surface. The surface never rises above
# `ROWS - SOIL_DEPTH - max(rise)`, so this is also what keeps the profile inside
# the map.
SOIL_DEPTH = 6

# The ground profile, as (first column, rows above the base). Deterministic and
# readable off the source for the same reason the fixture's surface_height() is:
# a shape you can read is one you can check a renderer against. No step is more
# than 2 rows, so nothing here is a wall.
TERRACES = ((0, 0), (12, 1), (20, 3), (30, 2), (40, 4), (52, 3), (62, 5),
            (74, 3), (86, 4))

# Branch platforms, as (leftmost column, row). Three tiles wide, and grouped
# into three climbs: two short ones at either end and the tall one in the middle
# that reaches row 16, which is what takes the camera off its top clamp.
BRANCHES = ((14, 30), (19, 27), (24, 24),
            (34, 29),
            (46, 28), (51, 25), (56, 22), (61, 19), (66, 16),
            (79, 28), (84, 25), (89, 22))

# Spawns, as (type, column, row or None for the ground surface). `type` is the
# demo's vocabulary and decision 3's carrier; decision 8 keeps it out of
# game/, so it stays a string here and is mapped to orchard's own enum at the
# spawn site. There is no `name` attribute on any of these — a second copy of
# the role would be a second thing to keep in step.
#
# One player, three animals to route around, and pickups spread far enough
# apart that collecting them is what makes the level worth crossing.
SPAWNS = (
    ("player", 3, None),
    ("opossum", 28, None),
    ("frog", 44, None),
    ("eagle", 70, 12),
    ("cherry", 6, None),
    ("cherry", 10, None),
    ("cherry", 15, 30),
    ("cherry", 20, 27),
    ("cherry", 31, None),
    ("cherry", 35, 29),
    ("cherry", 42, None),
    ("cherry", 47, 28),
    ("cherry", 52, 25),
    ("cherry", 57, 22),
    ("cherry", 62, 19),
    ("cherry", 72, None),
    ("cherry", 80, 28),
    ("cherry", 85, 25),
    ("cherry", 94, None),
    ("gem", 25, 24),
    ("gem", 67, 16),
    ("gem", 90, 22),
    ("goal", 92, None),
)

# The one spawn that stands on nothing. An explicit row means "resting on the
# branch there" for everything else, and the eagle's row is open air over the
# tall climb — it is the animal that ignores terrain.
FLIERS = ("eagle",)

# The box each spawn is written with, in pixels. Under the spawn idiom
# include/game/entities.hpp documents — `entity.box = {0, 0, object.width,
# object.height}` — this rectangle *is* the entity's collision box, which is
# also the rectangle game::Level bounds-checks. So these are collision boxes and
# not the art's frame size: the art is bigger in every case, and cherry's 21x21
# sprite around a 16x16 box is the usual pickup slack.
BOXES = {
    "player": (16, 28),
    "opossum": (28, 20),
    "frog": (24, 20),
    "eagle": (32, 24),
    "cherry": (16, 16),
    "gem": (12, 12),
    "goal": (24, 32),
}


def surface_row(column):
    """Row of the topmost solid ground cell in `column`."""
    rise = 0
    for start, amount in TERRACES:
        if column >= start:
            rise = amount
    return ROWS - SOIL_DEPTH - rise


def branch_cells():
    """Every (column, row) a branch platform occupies."""
    return {(column + i, row)
            for column, row in BRANCHES
            for i in range(len(BRANCH))}


def build_layers():
    """The three layers, each a flat list of 0-based tile ids or EMPTY.

    `collision` is filled from the same decision that fills `ground`, so a solid
    cell cannot exist in one and not the other.
    """
    ground = [EMPTY] * (COLS * ROWS)
    canopy = [EMPTY] * (COLS * ROWS)
    collision = [EMPTY] * (COLS * ROWS)

    def put_solid(column, row, tile):
        ground[row * COLS + column] = tile
        collision[row * COLS + column] = COLLISION_MARK

    for column in range(COLS):
        top = surface_row(column)
        put_solid(column, top, GRASS[column % len(GRASS)])
        for row in range(top + 1, ROWS):
            put_solid(column, row, DIRT[(column + row) % len(DIRT)])

        # Something standing on the surface, often enough to read as growth and
        # sparsely enough that the layer stays nearly empty — which is the half
        # of decision 4 this layer is answerable for.
        if column % 5 == 2:
            canopy[(top - 1) * COLS + column] = \
                UNDERGROWTH[(column // 5) % len(UNDERGROWTH)]
        elif column % 11 == 7:
            canopy[(top - 1) * COLS + column] = STONE[(column // 11) % 2]

    for column, row in BRANCHES:
        for i, tile in enumerate(BRANCH):
            put_solid(column + i, row, tile)

    return [("ground", ground, True), ("canopy", canopy, True),
            ("collision", collision, False)]


def spawn_rects():
    """The spawn table as (type, x, y, width, height) in map-origin pixels.

    A pickup or an animal rests its box on whatever it stands on: the ground
    surface, or the top of the branch platform named in its row.
    """
    rects = []
    for kind, column, row in SPAWNS:
        width, height = BOXES[kind]
        floor = surface_row(column) if row is None else row
        rects.append((kind, column * TILE, floor * TILE - height, width,
                      height))
    return rects


def check_layout(rects):
    """Refuse to write a level whose own layout is wrong.

    Three ways the tables above can disagree with each other, none of which
    `game::Level` would catch — a platform buried in the hillside and a pickup
    hanging in the air are both perfectly loadable levels.
    """
    problems = []
    cells = branch_cells()

    for column, row in BRANCHES:
        for i in range(len(BRANCH)):
            if row >= surface_row(column + i):
                problems.append(
                    f"branch at column {column} row {row} is inside the ground, "
                    f"which is at row {surface_row(column + i)} in column "
                    f"{column + i}")

    for kind, column, row in SPAWNS:
        if row is None:
            continue
        if kind in FLIERS:
            if row >= surface_row(column):
                problems.append(
                    f"{kind} at column {column} flies at row {row}, which is "
                    "under the ground")
        elif (column, row) not in cells:
            problems.append(
                f"{kind} at column {column} is perched on row {row}, where "
                "there is no branch")

    for kind, x, y, width, height in rects:
        if y < 0 or x < 0:
            problems.append(f"{kind} spawns at ({x}, {y})")

    return problems


def check_level_contract(layers, rects):
    """The half of game::Level's refusal list this file can get wrong.

    include/game/level.hpp is authoritative and this is a copy of its rule, kept
    here so a bad map is caught where it is written rather than in a test that
    may not exist yet. The two that cannot be got wrong from here are left out:
    a draw offset, which is never written, and a non-positive dimension.
    """
    problems = []
    pixel_width = COLS * TILE
    pixel_height = ROWS * TILE

    names = [name for name, _, _ in layers]
    if "collision" not in names:
        problems.append('no tile layer named "collision"')
    for name, tiles, _ in layers:
        if len(tiles) != COLS * ROWS:
            problems.append(
                f'layer "{name}" has {len(tiles)} cells, not {COLS * ROWS}')

    if not rects:
        problems.append('the "spawns" layer is empty')

    def inside(low, extent, limit):
        if low < 0:
            return False
        if extent > 0:
            return low + extent <= limit
        return low < limit

    for kind, x, y, width, height in rects:
        if not (inside(x, width, pixel_width) and
                inside(y, height, pixel_height)):
            problems.append(
                f"{kind} at ({x}, {y}) {width}x{height} is outside the "
                f"{pixel_width}x{pixel_height} level")

    return problems


def csv_data(tiles):
    """Layer data as Tiled writes it: gids, comma separated, one row per line."""
    lines = []
    for start in range(0, len(tiles), COLS):
        row = tiles[start:start + COLS]
        lines.append(",".join(str(t + 1 if t != EMPTY else 0) for t in row))
    return "\n" + ",\n".join(lines) + "\n"


def write_map(out, layers, rects):
    root = ET.Element("map", {
        "version": "1.10",
        "tiledversion": "1.10.2",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "width": str(COLS),
        "height": str(ROWS),
        "tilewidth": str(TILE),
        "tileheight": str(TILE),
        "infinite": "0",
        "nextlayerid": str(len(layers) + 2),
        "nextobjectid": str(len(rects) + 1),
    })

    ET.SubElement(root, "tileset", {"firstgid": "1", "source": "sunnyland.tsx"})

    for index, (name, tiles, visible) in enumerate(layers, start=1):
        attributes = {
            "id": str(index),
            "name": name,
            "width": str(COLS),
            "height": str(ROWS),
        }
        if not visible:
            attributes["visible"] = "0"

        layer = ET.SubElement(root, "layer", attributes)
        data = ET.SubElement(layer, "data", {"encoding": "csv"})
        data.text = csv_data(tiles)

    group = ET.SubElement(root, "objectgroup",
                          {"id": str(len(layers) + 1), "name": "spawns"})
    for index, (kind, x, y, width, height) in enumerate(rects, start=1):
        ET.SubElement(group, "object", {
            "id": str(index),
            "type": kind,
            "x": str(x),
            "y": str(y),
            "width": str(width),
            "height": str(height),
        })

    path = out / "orchard.tmx"
    ET.indent(root, space=" ")
    ET.ElementTree(root).write(path, encoding="UTF-8", xml_declaration=True)
    return path


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("data"))
    args = parser.parse_args()

    layers = build_layers()
    rects = spawn_rects()

    problems = check_layout(rects) + check_level_contract(layers, rects)
    if problems:
        sys.exit("\n".join(f"{parser.prog}: {problem}" for problem in problems))

    if not (args.out / "sunnyland.tsx").exists():
        sys.exit(f"{args.out / 'sunnyland.tsx'}: not found. The map references "
                 "it by name; run tools/make_tilemap.py first.")

    args.out.mkdir(parents=True, exist_ok=True)
    tmx = write_map(args.out, layers, rects)

    solid = sum(1 for tile in layers[2][1] if tile != EMPTY)
    print(f"{tmx}: {COLS}x{ROWS} tiles of {TILE}x{TILE} "
          f"({COLS * TILE}x{ROWS * TILE} pixels), {len(layers)} tile layers, "
          f"{len(rects)} spawns")
    print(f"  solid cells: {solid} of {COLS * ROWS} "
          f"({100 * solid // (COLS * ROWS)}%)")


if __name__ == "__main__":
    main()
