#!/usr/bin/env python3
"""Generate orchard's level: data/orchard.tmx, against data/sunnyland.tsx.

    tools/.venv/bin/python tools/make_orchard_level.py --out data

Writes `orchard.tmx` and nothing else. The tileset it references —
`sunnyland.tsx` and `tileset.png` — is already in the tree, so this needs
neither Pillow nor the gitignored Sunny Land bundle and runs in a clean
checkout, which `make_tilemap.py` does not.

    tools/.venv/bin/python tools/make_orchard_level.py --out data --bake-tiles

`--bake-tiles` additionally redraws the cloud tiles into `data/tileset.png`,
and is the one path that needs Pillow. It is not part of generating the map:
the baked sheet is committed, so this only has to run when `CLOUD_SHAPES`
changes. Everything above stays true of the default invocation — the import is
inside `bake_cloud_tiles` so that a stdlib-only interpreter still writes the
map.

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
    canopy      tufts and sprigs standing on the surface, solid to nothing
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
import math
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

# Grass tufts and leafy sprigs, and the whole of the canopy: this sheet has no
# rock that can stand on grass. 180 and 182 look like loose rocks but are sparse
# *subsoil* — the same dark-maroon matrix and three-colour palette as DIRT's
# 76/80/126/128/130 and the soil block 36, at 100% alpha coverage against 50-57%
# for the four named below — so above the surface they read as brown squares.
# The sheet's actual rocks (251, 255, 260, 261, 277, 279, 376, 380, 426, 430)
# are edge pieces of a larger mass, sheared flush at a tile boundary, so one
# placed alone shows a sawn-off face. Those are the only partly-transparent
# non-foliage tiles that are not the branch logs, so there is no third kind.
UNDERGROWTH = (176, 178, 184, 186)
COLLISION_MARK = 103            # a plain opaque block: the layer reads as a
                                # mask when it is switched on in Tiled

# CLOUDS ARE DRAWN HERE, NOT TAKEN FROM THE PACK.
#
# The sheet has no cloud in it. An earlier attempt recoloured rock rubble
# (277, 279, 376, 380) into pale tones and it read as popcorn, because a rock's
# internal detail is random and high-contrast while a cloud's shading follows
# its silhouette. Recolouring cannot fix that — it is the shape that is wrong.
#
# A cloud is a much simpler thing to draw than a rock, so these tiles are
# generated by `--bake-tiles` below and written into the sheet. Three properties
# do the work, and they are why this reads as a cloud rather than as a lump:
#
#   * a FLAT BOTTOM. This is most of the effect. Every column of the silhouette
#     is filled from its highest covered row straight down to CLOUD_BASE, so the
#     underside is one straight edge and the notches between lobes close up.
#     A blob with a scalloped bottom reads as foam or as rubble.
#   * BUMPS ON TOP ONLY, as the union of circles of differing radius along a
#     horizontal axis.
#   * TWO TONES, not four, with the darker one banded along the bottom of the
#     shape rather than scattered through it.
#
# Written into tile ids 541-574, which are already in the sheet and fully
# transparent — verified with one getbbox() per tile, a contiguous run of 34.
# Appending a row instead would change `tileset.png`'s dimensions and turn
# tests/test_tmx.cc red, which asserts tilecount 575 and a 400x368 image.
CLOUD_FIRST = 541
CLOUD_BODY = (250, 252, 252, 255)
CLOUD_SHADE = (196, 214, 232, 255)
CLOUD_BASE = 12                 # the flat underside, as a row within the tile
CLOUD_SHADE_ROWS = 1            # rows of the darker tone, up from CLOUD_BASE
CLOUD_BODY_MIN = 2              # body rows every column keeps above the shade

# (width in tiles, [(cx, cy, radius), ...]) in pixels from the shape's own left
# edge. Five distinct silhouettes at three widths, and none of them mirrored:
# a shape symmetric about its own centre reads as a motif and the eye finds the
# axis immediately. Different left and right ends are the point, not sloppiness.
CLOUD_SHAPES = (
    (3, ((11, 9, 5.0), (21, 6, 7.0), (31, 8, 5.5), (39, 10, 4.0))),
    (2, ((9, 9, 5.0), (18, 7, 6.0), (26, 10, 4.0))),
    (4, ((10, 10, 4.5), (19, 7, 6.5), (29, 9, 5.5), (40, 6, 7.0),
         (52, 9, 5.0), (59, 11, 3.5))),
    (3, ((8, 10, 4.5), (17, 9, 5.5), (28, 6, 7.0), (39, 9, 5.0))),
    (2, ((8, 10, 4.5), (16, 6, 6.5), (25, 9, 4.5))),
)

# The seed for subsoil variant selection. A constant in the source rather than
# an argument or anything drawn from the environment: this file promises a
# fixed function with no clock and no RNG seed, and data/PROVENANCE.md rests on
# regenerating an unchanged map being a no-op in the diff. Change this and the
# whole dirt mass reshuffles, which is the only thing it is for.
DIRT_SEED = 0x5EEDD127

# The same discipline, a different seed, and deliberately a different *kind* of
# noise. The subsoil wants independent per-cell choice; scattering clouds is the
# opposite problem. Coherent noise clusters — that is what coherence means — so
# thresholding a field for placement gives clumps and bald patches. These are
# placed by STRATIFIED JITTER instead: one cloud per band of columns, jittered
# within its band. That bounds the gap at both ends, which is what "well
# separated but irregularly spaced" actually asks for, and no two runs of the
# eye find a period.
CLOUD_SEED = 0xC10DD00D

# One cloud per this many columns, before jitter.
CLOUD_STRATUM = 12

# The altitude band, inclusive, in rows. The floor of this band is set by the
# branches: the tall climb tops out at row 16, and a cloud lower than that would
# sit on a platform. The ceiling is set by the camera — it clamps at y=160 while
# the player is on the ground, so rows above 10 are off the top of a 640x480
# screen for all of normal play and a cloud there would only ever be seen from
# the top of the middle climb.
CLOUD_TOP = 11
CLOUD_BOTTOM = 15

# Columns and rows the eagle's spawn box occupies, which a cloud must not be
# drawn behind. It flies at (70, 12) with a 32x24 box, so it covers columns
# 70-71 across rows 11-12; the margin widens that to the span its swoop uses.
CLOUD_KEEPOUT_COLUMNS = (67, 75)
CLOUD_KEEPOUT_ROWS = (11, 13)

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


def cell_hash(column, row, seed):
    """A 32-bit avalanche hash of one lattice point. Pure, and stdlib only.

    The multipliers and shifts are an ordinary xorshift-multiply finaliser. The
    only property being relied on is that it decorrelates neighbouring inputs,
    so that reducing the result modulo a small variant count leaves no period a
    reader can see.

    Python's built-in `hash()` would not do: it is randomised per process for
    `str` and `bytes` under PYTHONHASHSEED, which is exactly the kind of
    environment dependence this file promises not to have.
    """
    value = ((column * 0x27D4EB2D) ^ (row * 0x165667B1) ^ seed) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x2C1B3C6D) & 0xFFFFFFFF
    value ^= value >> 12
    value = (value * 0x297A2D39) & 0xFFFFFFFF
    value ^= value >> 15
    return value


def surface_row(column):
    """Row of the topmost solid ground cell in `column`."""
    rise = 0
    for start, amount in TERRACES:
        if column >= start:
            rise = amount
    return ROWS - SOIL_DEPTH - rise


def cloud_tiles(shape):
    """The tile ids shape `shape` occupies, left to right.

    The shapes are laid out consecutively from CLOUD_FIRST, so this is the one
    place that knows the packing and `bake_cloud_tiles` uses it too — the sheet
    and the map cannot disagree about which slot holds which piece.
    """
    first = CLOUD_FIRST
    for index, (width, _) in enumerate(CLOUD_SHAPES):
        if index == shape:
            return tuple(range(first, first + width))
        first += width
    raise IndexError(shape)


def cloud_placements():
    """Clouds as (column, row, shape), left to right.

    Stratified jitter: the sky is cut into bands of CLOUD_STRATUM columns and
    each band contributes at most one cloud, placed at a hashed offset inside
    it. Width, shape and altitude all come from the same hash, so the whole
    sky is a pure function of CLOUD_SEED.
    """
    placements = []
    previous_shape = None

    for index, start in enumerate(range(0, COLS, CLOUD_STRATUM)):
        # Three separate hashes rather than three bit-slices of one. The band
        # index and its first column are perfectly correlated inputs, and
        # slicing a single avalanche of them put four of seven clouds on the
        # same shape and five of seven at the same altitude.
        shape = cell_hash(index, 0, CLOUD_SEED) % len(CLOUD_SHAPES)

        # A repeated shape next to itself is the one coincidence the eye does
        # catch, so step off it. This biases the distribution slightly and that
        # is the right trade: the previous attempt's visible defect was a
        # period of two.
        if shape == previous_shape:
            shape = (shape + 1) % len(CLOUD_SHAPES)

        width = CLOUD_SHAPES[shape][0]

        # The jitter range leaves the cloud inside its own band, so two
        # neighbours can never touch however the hash falls.
        span = CLOUD_STRATUM - width
        if span <= 0 or start + width > COLS:
            continue

        column = start + (cell_hash(index, 1, CLOUD_SEED) % span)
        row = CLOUD_TOP + (cell_hash(index, 2, CLOUD_SEED)
                           % (CLOUD_BOTTOM - CLOUD_TOP + 1))

        # Lifted clear of the eagle rather than dropped: dropping the band left
        # a 27-column hole in the sky, which is a more visible artefact than
        # anything the jitter was protecting against.
        if overlaps_keepout(column, width, row):
            row = CLOUD_KEEPOUT_ROWS[1] + 1
            if row > CLOUD_BOTTOM:
                continue

        placements.append((column, row, shape))
        previous_shape = shape

    return placements


def overlaps_keepout(column, width, row):
    """Whether a cloud would be drawn behind the eagle's patrol."""
    first_column, last_column = CLOUD_KEEPOUT_COLUMNS
    first_row, last_row = CLOUD_KEEPOUT_ROWS
    return (column <= last_column and column + width - 1 >= first_column
            and first_row <= row <= last_row)


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
        # `(column + row) % len(DIRT)` was a diagonal stripe: constant along
        # every anti-diagonal and repeating every five cells on both axes. With
        # 76 and 80 each carrying a heavy dark region, that period showed up as
        # dark seams ruled across the whole mass.
        #
        # The hash is deliberately not smoothed into value noise. Smoothing
        # correlates neighbours, and for a five-way choice between variants of
        # the same material that yields patches of one tile — a blotch instead
        # of a stripe, which is no better. Independent per-cell selection is
        # what reads as texture.
        for row in range(top + 1, ROWS):
            put_solid(column, row,
                      DIRT[cell_hash(column, row, DIRT_SEED) % len(DIRT)])

        # Something standing on the surface, often enough to read as growth and
        # sparsely enough that the layer stays nearly empty — which is the half
        # of decision 4 this layer is answerable for.
        if column % 5 == 2:
            canopy[(top - 1) * COLS + column] = \
                UNDERGROWTH[(column // 5) % len(UNDERGROWTH)]

    for column, row in BRANCHES:
        for i, tile in enumerate(BRANCH):
            put_solid(column + i, row, tile)

    # Sky decor, so `ground` and not a third visible layer — decision 4 counts
    # layers, and fourteen cells do not earn one. Written straight into `ground`
    # rather than through put_solid(): a cloud must leave `collision` alone, and
    # the two are otherwise filled from the same decision on purpose.
    for column, row, shape in cloud_placements():
        for i, tile in enumerate(cloud_tiles(shape)):
            ground[row * COLS + column + i] = tile

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


def draw_cloud(width, circles):
    """One cloud as a list of rows of RGBA tuples, `width` tiles wide.

    The silhouette is the union of `circles`, but only its *top* contour is
    used: each column is then filled from that contour straight down to
    CLOUD_BASE. That is what gives the flat underside, and it is also what
    closes the gaps between lobes that would otherwise read as separate blobs
    rather than as one cloud.
    """
    pixels = width * TILE
    blank = (0, 0, 0, 0)
    rows = [[blank] * pixels for _ in range(TILE)]

    for x in range(pixels):
        top = None
        for cx, cy, radius in circles:
            dx = x + 0.5 - cx
            if abs(dx) > radius:
                continue
            height = math.sqrt(radius * radius - dx * dx)
            contour = int(math.ceil(cy - height - 0.5))
            if top is None or contour < top:
                top = contour

        if top is None:
            continue

        top = max(0, top)

        # The shade is the bottom band of the shape, but every column keeps at
        # least CLOUD_BODY_MIN rows of body above it. Without that floor the
        # short columns at each end — where a low, small circle contributes only
        # two or three rows — come out as pure shade, and the cloud reads as
        # sitting on a grey plinth wider than itself rather than as having a
        # shaded underside.
        shade_from = max(CLOUD_BASE - CLOUD_SHADE_ROWS + 1,
                         top + CLOUD_BODY_MIN)

        for y in range(top, CLOUD_BASE + 1):
            rows[y][x] = CLOUD_SHADE if y >= shade_from else CLOUD_BODY

    return rows


def bake_cloud_tiles(sheet):
    """Draw the cloud tiles into `sheet` in place, and report what was written.

    Pillow is imported here and nowhere else, so generating the map stays a
    stdlib-only operation that runs in a clean checkout. The baked sheet is in
    the tree, so this only has to run when a shape above changes.
    """
    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow is not installed; see tools/README.md")

    image = Image.open(sheet).convert("RGBA")
    columns = image.width // TILE

    written = []
    for shape, (width, circles) in enumerate(CLOUD_SHAPES):
        rows = draw_cloud(width, circles)
        for i, tile in enumerate(cloud_tiles(shape)):
            left = (tile % columns) * TILE
            top = (tile // columns) * TILE
            for y in range(TILE):
                for x in range(TILE):
                    image.putpixel((left + x, top + y),
                                   rows[y][i * TILE + x])
            written.append(tile)

    image.save(sheet)
    return written


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
    parser.add_argument(
        "--bake-tiles", action="store_true",
        help="redraw the cloud tiles into tileset.png (needs Pillow); only "
             "necessary when CLOUD_SHAPES changes")
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

    if args.bake_tiles:
        sheet = args.out / "tileset.png"
        if not sheet.exists():
            sys.exit(f"{sheet}: not found; run tools/make_tilemap.py first.")
        written = bake_cloud_tiles(sheet)
        print(f"{sheet}: drew {len(written)} cloud tiles into ids "
              f"{written[0]}-{written[-1]}")

    tmx = write_map(args.out, layers, rects)

    solid = sum(1 for tile in layers[2][1] if tile != EMPTY)
    print(f"{tmx}: {COLS}x{ROWS} tiles of {TILE}x{TILE} "
          f"({COLS * TILE}x{ROWS * TILE} pixels), {len(layers)} tile layers, "
          f"{len(rects)} spawns")
    print(f"  solid cells: {solid} of {COLS * ROWS} "
          f"({100 * solid // (COLS * ROWS)}%)")


if __name__ == "__main__":
    main()
