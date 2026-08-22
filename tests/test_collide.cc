// game::collide — a box against a tile layer, and a box against a box.
//
// Entirely headless. The whole module is free functions over a gfx::TileLayer
// and two rectangles, so there is no Context, no texture and no .tmx fixture
// here: the layers below are built in the test, which keeps the arithmetic
// under test rather than the parser.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/collide.hpp>
#include <gfx/tilemap.hpp>

#include <string>
#include <vector>

namespace
{

const int tile = 16;

// A layer of `width` x `height` cells, every one empty.
gfx::TileLayer empty_layer(int width, int height)
{
    gfx::TileLayer layer;
    layer.name = "collision";
    layer.width = width;
    layer.height = height;
    layer.tiles.assign(static_cast<std::size_t>(width) * height,
                       gfx::empty_tile);
    return layer;
}

void fill(gfx::TileLayer& layer, int column, int row)
{
    layer.tiles[static_cast<std::size_t>(row) * layer.width + column] = 0;
}

game::TileGrid grid_over(const gfx::TileLayer& layer)
{
    return game::TileGrid{layer, tile, tile};
}

game::Aabb box(double x, double y, double w, double h)
{
    return game::Aabb{x, y, w, h};
}

} // namespace

//============================================================================
// The world-to-tile conversion
//============================================================================

TEST_CASE("tile_span maps a half-open interval to the cells it touches")
{
    int first = -99;
    int last = -99;

    // Exactly one cell, flush at both ends.
    REQUIRE(game::tile_span(0.0, 16.0, tile, first, last));
    CHECK(first == 0);
    CHECK(last == 0);

    // One pixel past the boundary reaches the next cell.
    REQUIRE(game::tile_span(0.0, 17.0, tile, first, last));
    CHECK(first == 0);
    CHECK(last == 1);

    // Starting exactly on a boundary does not touch the cell before it. This
    // is the case that decides whether a mover catches on a flat floor.
    REQUIRE(game::tile_span(16.0, 32.0, tile, first, last));
    CHECK(first == 1);
    CHECK(last == 1);

    // Spanning several cells.
    REQUIRE(game::tile_span(8.0, 40.0, tile, first, last));
    CHECK(first == 0);
    CHECK(last == 2);
}

TEST_CASE("tile_span floors toward negative infinity, not toward zero")
{
    int first = -99;
    int last = -99;

    // The whole reason util::floor_div exists. C division truncates, so
    // -1 / 16 == 0 and every position in the cell left of the origin would
    // report cell 0 — the map's first column would absorb everything outside
    // it and a mover would collide with the wrong cells.
    REQUIRE(game::tile_span(-1.0, 0.0, tile, first, last));
    CHECK(first == -1);
    CHECK(last == -1);

    REQUIRE(game::tile_span(-16.0, 0.0, tile, first, last));
    CHECK(first == -1);
    CHECK(last == -1);

    REQUIRE(game::tile_span(-17.0, -16.0, tile, first, last));
    CHECK(first == -2);
    CHECK(last == -2);

    // Straddling the origin.
    REQUIRE(game::tile_span(-1.0, 1.0, tile, first, last));
    CHECK(first == -1);
    CHECK(last == 0);
}

TEST_CASE("tile_span reports an empty interval rather than a range")
{
    int first = 5;
    int last = 5;

    CHECK_FALSE(game::tile_span(10.0, 10.0, tile, first, last));
    CHECK_FALSE(game::tile_span(10.0, 9.0, tile, first, last));

    // The outputs are left alone, so a caller cannot half-use them.
    CHECK(first == 5);
    CHECK(last == 5);
}

TEST_CASE("tile_span covers fractional edges")
{
    int first = -99;
    int last = -99;

    // [15.5, 16.0) is still entirely inside cell 0.
    REQUIRE(game::tile_span(15.5, 16.0, tile, first, last));
    CHECK(first == 0);
    CHECK(last == 0);

    // [15.5, 16.5) crosses into cell 1.
    REQUIRE(game::tile_span(15.5, 16.5, tile, first, last));
    CHECK(first == 0);
    CHECK(last == 1);
}

TEST_CASE("tile_span floors a NEGATIVE FRACTIONAL edge, not truncates it")
{
    // The pixel conversion, one level below util::floor_div. floor and
    // truncation agree everywhere except at a negative non-integer, so these
    // are the only inputs that tell the two apart — the integer cases above
    // pass either way. Every one of these is wrong if the low edge is rounded
    // toward zero instead of down.
    int first = -99;
    int last = -99;

    // Half a pixel left of the origin is in the cell BEFORE it, not in cell 0.
    REQUIRE(game::tile_span(-0.5, 0.5, tile, first, last));
    CHECK(first == -1);
    CHECK(last == 0);

    // Entirely within the cell left of the origin, and narrower than a pixel:
    // truncation puts the low edge at pixel 0, above the high edge, and the
    // interval reads as empty rather than as cell -1.
    REQUIRE(game::tile_span(-0.5, 0.0, tile, first, last));
    CHECK(first == -1);
    CHECK(last == -1);

    // Away from the origin, so this is the pixel conversion and not floor_div
    // rescuing it: [-16.5, -16.0) lies in cell -2, whose pixels are [-32,-16).
    REQUIRE(game::tile_span(-16.5, -16.0, tile, first, last));
    CHECK(first == -2);
    CHECK(last == -2);

    // Straddling the origin on a fraction at both ends.
    REQUIRE(game::tile_span(-0.5, 16.5, tile, first, last));
    CHECK(first == -1);
    CHECK(last == 1);
}

TEST_CASE("a box half a pixel outside the layer is outside it")
{
    // The behavioural face of the case above: rounding the low edge toward
    // zero would report this box as entirely inside the map, and a mover would
    // walk a fraction of a pixel off the edge with nothing stopping it.
    const gfx::TileLayer layer = empty_layer(4, 4);
    const game::TileGrid grid = grid_over(layer);

    CHECK(game::overlaps_solid(grid, box(-0.5, 0.0, 16.0, 16.0)));
    CHECK(game::overlaps_solid(grid, box(0.0, -0.5, 16.0, 16.0)));

    // Flush on the edge is still inside, so the case above is testing the
    // fraction and not merely the boundary.
    CHECK_FALSE(game::overlaps_solid(grid, box(0.0, 0.0, 16.0, 16.0)));
}

//============================================================================
// Solidity, and what lies outside the layer
//============================================================================

TEST_CASE("a cell holding a tile is solid and an empty cell is not")
{
    gfx::TileLayer layer = empty_layer(4, 4);
    fill(layer, 1, 1);

    const game::TileGrid grid = grid_over(layer);
    CHECK(game::solid_at(grid, 1, 1));
    CHECK_FALSE(game::solid_at(grid, 0, 0));
    CHECK_FALSE(game::solid_at(grid, 2, 1));
}

TEST_CASE("outside the layer is solid in every direction")
{
    // The decision recorded in the header: TileLayer::at() answers empty
    // outside the layer for the renderer's benefit, and collision answers the
    // opposite way so that a mover cannot leave the map and fall forever.
    const gfx::TileLayer layer = empty_layer(4, 4);
    const game::TileGrid grid = grid_over(layer);

    CHECK(game::solid_at(grid, -1, 0));  // left
    CHECK(game::solid_at(grid, 4, 0));   // right
    CHECK(game::solid_at(grid, 0, -1));  // above
    CHECK(game::solid_at(grid, 0, 4));   // below
    CHECK(game::solid_at(grid, -1, -1)); // diagonally out

    // And the cells just inside those edges are not.
    CHECK_FALSE(game::solid_at(grid, 0, 0));
    CHECK_FALSE(game::solid_at(grid, 3, 3));
}

TEST_CASE("a box leaving the layer collides with the outside")
{
    const gfx::TileLayer layer = empty_layer(4, 4);
    const game::TileGrid grid = grid_over(layer);

    // Fully inside: 4x4 cells of 16px is a 64x64 pixel map.
    CHECK_FALSE(game::overlaps_solid(grid, box(0.0, 0.0, 64.0, 64.0)));

    // One pixel past each edge.
    CHECK(game::overlaps_solid(grid, box(-1.0, 0.0, 64.0, 64.0)));
    CHECK(game::overlaps_solid(grid, box(0.0, -1.0, 64.0, 64.0)));
    CHECK(game::overlaps_solid(grid, box(1.0, 0.0, 64.0, 64.0)));
    CHECK(game::overlaps_solid(grid, box(0.0, 1.0, 64.0, 64.0)));
}

//============================================================================
// Box against tiles
//============================================================================

TEST_CASE("overlaps_solid finds a box sitting on a solid cell")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 2, 2); // pixels [32,48) x [32,48)

    const game::TileGrid grid = grid_over(layer);

    CHECK(game::overlaps_solid(grid, box(32.0, 32.0, 16.0, 16.0)));
    CHECK(game::overlaps_solid(grid, box(40.0, 40.0, 1.0, 1.0)));

    // Flush against each side of it, and therefore not overlapping it.
    CHECK_FALSE(game::overlaps_solid(grid, box(16.0, 32.0, 16.0, 16.0)));
    CHECK_FALSE(game::overlaps_solid(grid, box(48.0, 32.0, 16.0, 16.0)));
    CHECK_FALSE(game::overlaps_solid(grid, box(32.0, 16.0, 16.0, 16.0)));
    CHECK_FALSE(game::overlaps_solid(grid, box(32.0, 48.0, 16.0, 16.0)));

    // Half a pixel into it from each side, and therefore overlapping.
    CHECK(game::overlaps_solid(grid, box(16.5, 32.0, 16.0, 16.0)));
    CHECK(game::overlaps_solid(grid, box(47.5, 32.0, 16.0, 16.0)));
}

TEST_CASE("a box spanning several cells sees all of them")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 5, 1); // far from the box's first cell

    const game::TileGrid grid = grid_over(layer);

    // Covers columns 0..5 on row 1, so it must reach the solid one at the end
    // rather than stopping at the first cell it looks at.
    CHECK(game::overlaps_solid(grid, box(0.0, 16.0, 90.0, 16.0)));

    // One pixel shorter stops at column 4 and misses it.
    CHECK_FALSE(game::overlaps_solid(grid, box(0.0, 16.0, 80.0, 16.0)));
}

TEST_CASE("an empty box overlaps nothing, even inside a solid cell")
{
    gfx::TileLayer layer = empty_layer(4, 4);
    fill(layer, 1, 1);

    const game::TileGrid grid = grid_over(layer);

    CHECK_FALSE(game::overlaps_solid(grid, box(20.0, 20.0, 0.0, 0.0)));
    CHECK_FALSE(game::overlaps_solid(grid, box(20.0, 20.0, 0.0, 8.0)));
    CHECK_FALSE(game::overlaps_solid(grid, box(20.0, 20.0, 8.0, -4.0)));
}

//============================================================================
// Box against box
//============================================================================

TEST_CASE("overlaps is half-open, so touching edges do not count")
{
    const game::Aabb a = box(0.0, 0.0, 10.0, 10.0);

    CHECK(game::overlaps(a, box(5.0, 5.0, 10.0, 10.0)));
    CHECK(game::overlaps(a, box(-5.0, -5.0, 10.0, 10.0)));

    // Sharing an edge exactly.
    CHECK_FALSE(game::overlaps(a, box(10.0, 0.0, 10.0, 10.0)));
    CHECK_FALSE(game::overlaps(a, box(0.0, 10.0, 10.0, 10.0)));
    CHECK_FALSE(game::overlaps(a, box(-10.0, 0.0, 10.0, 10.0)));

    // Half a pixel of genuine overlap.
    CHECK(game::overlaps(a, box(9.5, 0.0, 10.0, 10.0)));

    // Sharing only a corner point.
    CHECK_FALSE(game::overlaps(a, box(10.0, 10.0, 10.0, 10.0)));
}

TEST_CASE("overlaps is symmetric and containment counts")
{
    const game::Aabb outer = box(0.0, 0.0, 100.0, 100.0);
    const game::Aabb inner = box(40.0, 40.0, 5.0, 5.0);

    CHECK(game::overlaps(outer, inner));
    CHECK(game::overlaps(inner, outer));

    const game::Aabb apart = box(200.0, 200.0, 5.0, 5.0);
    CHECK_FALSE(game::overlaps(outer, apart));
    CHECK_FALSE(game::overlaps(apart, outer));
}

TEST_CASE("an empty box overlaps no other box")
{
    const game::Aabb solid = box(0.0, 0.0, 10.0, 10.0);

    CHECK_FALSE(game::overlaps(solid, box(5.0, 5.0, 0.0, 0.0)));
    CHECK_FALSE(game::overlaps(box(5.0, 5.0, 0.0, 0.0), solid));
    CHECK_FALSE(game::overlaps(solid, box(5.0, 5.0, 0.0, 5.0)));
    CHECK_FALSE(game::overlaps(solid, box(5.0, 5.0, -5.0, 5.0)));
}

//============================================================================
// Sliding
//============================================================================

TEST_CASE("an unobstructed slide moves the whole way and reports no hit")
{
    const gfx::TileLayer layer = empty_layer(16, 16);
    const game::TileGrid grid = grid_over(layer);

    const game::Resolution moved =
        game::slide(grid, box(32.0, 32.0, 8.0, 8.0), 5.0, -3.0);

    CHECK(moved.x == doctest::Approx(37.0));
    CHECK(moved.y == doctest::Approx(29.0));
    CHECK_FALSE(moved.hit_x);
    CHECK_FALSE(moved.hit_y);
}

TEST_CASE("a zero-length slide moves nothing and reports no hit")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 2, 2);

    const game::TileGrid grid = grid_over(layer);
    const game::Aabb start = box(20.0, 20.0, 8.0, 8.0);

    const game::Resolution still = game::slide(grid, start, 0.0, 0.0);
    CHECK(still.x == doctest::Approx(20.0));
    CHECK(still.y == doctest::Approx(20.0));
    CHECK_FALSE(still.hit_x);
    CHECK_FALSE(still.hit_y);

    // Nothing was asked for, so nothing is refused — even for a box that is
    // already buried in solid tiles.
    const game::Resolution buried =
        game::slide(grid, box(34.0, 34.0, 8.0, 8.0), 0.0, 0.0);
    CHECK(buried.x == doctest::Approx(34.0));
    CHECK(buried.y == doctest::Approx(34.0));
    CHECK_FALSE(buried.hit_x);
    CHECK_FALSE(buried.hit_y);

    // One axis at a time: dx == 0 must not resolve or report on X.
    const game::Resolution only_y = game::slide(grid, start, 0.0, 2.0);
    CHECK(only_y.x == doctest::Approx(20.0));
    CHECK_FALSE(only_y.hit_x);
}

TEST_CASE("a blocked slide stops flush against the tile it ran into")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 3, 1); // pixels [48,64) x [16,32)

    const game::TileGrid grid = grid_over(layer);

    // Moving right from x=16 with an 8-wide box: the right edge would reach
    // 60, well inside column 3, so it stops with its right edge on 48.
    const game::Resolution moved =
        game::slide(grid, box(16.0, 16.0, 8.0, 8.0), 36.0, 0.0);

    CHECK(moved.x == doctest::Approx(40.0)); // 48 - 8
    CHECK(moved.hit_x);
    CHECK_FALSE(moved.hit_y);

    // Flush, not short: the box's right edge is exactly the blocker's left.
    CHECK(moved.x + 8.0 == doctest::Approx(48.0));
}

TEST_CASE("sliding into a wall is idempotent")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 3, 1);

    const game::TileGrid grid = grid_over(layer);

    const game::Resolution first =
        game::slide(grid, box(16.0, 16.0, 8.0, 8.0), 36.0, 0.0);
    REQUIRE(first.hit_x);

    // A second call from where the first one landed must not creep, and must
    // still report the hit rather than reporting a clear path.
    const game::Resolution second =
        game::slide(grid, box(first.x, first.y, 8.0, 8.0), 36.0, 0.0);

    CHECK(second.x == doctest::Approx(first.x));
    CHECK(second.hit_x);
}

TEST_CASE("a slide is blocked in each of the four directions")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 2, 2); // pixels [32,48) x [32,48)

    const game::TileGrid grid = grid_over(layer);

    // Right into it: right edge stops on 32.
    const game::Resolution right =
        game::slide(grid, box(16.0, 34.0, 8.0, 8.0), 20.0, 0.0);
    CHECK(right.hit_x);
    CHECK(right.x == doctest::Approx(24.0));

    // Left into it: left edge stops on 48.
    const game::Resolution left =
        game::slide(grid, box(56.0, 34.0, 8.0, 8.0), -20.0, 0.0);
    CHECK(left.hit_x);
    CHECK(left.x == doctest::Approx(48.0));

    // Down into it: bottom edge stops on 32.
    const game::Resolution down =
        game::slide(grid, box(34.0, 16.0, 8.0, 8.0), 0.0, 20.0);
    CHECK(down.hit_y);
    CHECK(down.y == doctest::Approx(24.0));

    // Up into it: top edge stops on 48.
    const game::Resolution up =
        game::slide(grid, box(34.0, 56.0, 8.0, 8.0), 0.0, -20.0);
    CHECK(up.hit_y);
    CHECK(up.y == doctest::Approx(48.0));
}

TEST_CASE("a slide parallel to a wall is not blocked by it")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    for (int row = 0; row < 8; ++row) {
        fill(layer, 3, row); // a full-height wall at x in [48,64)
    }

    const game::TileGrid grid = grid_over(layer);

    // Flush against the wall's left face, moving down along it.
    const game::Resolution moved =
        game::slide(grid, box(40.0, 16.0, 8.0, 8.0), 0.0, 10.0);

    CHECK(moved.y == doctest::Approx(26.0));
    CHECK_FALSE(moved.hit_x);
    CHECK_FALSE(moved.hit_y);
}

TEST_CASE("an inside corner stops both axes")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    // An L: a wall to the right and a floor below, meeting at (3,3).
    fill(layer, 3, 2);
    fill(layer, 3, 3);
    fill(layer, 2, 3);

    const game::TileGrid grid = grid_over(layer);

    // An 8px box in the empty cell (2,2) driven hard into the corner.
    const game::Resolution moved =
        game::slide(grid, box(34.0, 34.0, 8.0, 8.0), 20.0, 20.0);

    CHECK(moved.hit_x);
    CHECK(moved.hit_y);
    CHECK(moved.x == doctest::Approx(40.0)); // right edge flush on 48
    CHECK(moved.y == doctest::Approx(40.0)); // bottom edge flush on 48
}

TEST_CASE("an outside corner stops on X only, because X resolves first")
{
    // The documented consequence of resolving axis by axis with X first. A
    // single block, approached diagonally from above-left: X is swept with the
    // original y span and is blocked, and the Y sweep then runs in the column
    // the box was pushed back into, which is clear.
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 3, 3); // pixels [48,64) x [48,64)

    const game::TileGrid grid = grid_over(layer);

    // Box overlapping rows of the block already, moving down and right.
    const game::Resolution moved =
        game::slide(grid, box(36.0, 50.0, 8.0, 8.0), 20.0, 4.0);

    CHECK(moved.hit_x);
    CHECK(moved.x == doctest::Approx(40.0));
    CHECK_FALSE(moved.hit_y);
    CHECK(moved.y == doctest::Approx(54.0));
}

TEST_CASE("a box that starts overlapping is not trapped")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 2, 2); // pixels [32,48) x [32,48)

    const game::TileGrid grid = grid_over(layer);

    // Buried in the middle of the solid cell. It must be able to leave.
    const game::Resolution out =
        game::slide(grid, box(36.0, 36.0, 8.0, 8.0), -20.0, 0.0);

    CHECK(out.x == doctest::Approx(16.0));
    CHECK_FALSE(out.hit_x);

    // It may also move further along inside the cell it already occupies.
    const game::Resolution along =
        game::slide(grid, box(34.0, 36.0, 8.0, 8.0), 2.0, 0.0);
    CHECK(along.x == doctest::Approx(36.0));
    CHECK_FALSE(along.hit_x);
}

TEST_CASE("an overlapping box is still blocked from entering a new cell")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    fill(layer, 2, 2); // the cell it starts in
    fill(layer, 4, 2); // pixels [64,80), a separate blocker further right

    const game::TileGrid grid = grid_over(layer);

    // Starts inside (2,2). Column 3 is clear, column 4 is not, so it crosses
    // the gap and stops flush on 64 rather than being freed by its own
    // starting overlap.
    const game::Resolution moved =
        game::slide(grid, box(34.0, 36.0, 8.0, 8.0), 40.0, 0.0);

    CHECK(moved.hit_x);
    CHECK(moved.x == doctest::Approx(56.0)); // 64 - 8
}

TEST_CASE("a slide is blocked by the edge of the layer")
{
    const gfx::TileLayer layer = empty_layer(4, 4); // 64x64 pixels
    const game::TileGrid grid = grid_over(layer);

    // Off the right-hand edge.
    const game::Resolution right =
        game::slide(grid, box(48.0, 16.0, 8.0, 8.0), 20.0, 0.0);
    CHECK(right.hit_x);
    CHECK(right.x == doctest::Approx(56.0)); // 64 - 8

    // Off the left-hand edge, into negative coordinates — the case truncating
    // division gets wrong.
    const game::Resolution left =
        game::slide(grid, box(8.0, 16.0, 8.0, 8.0), -20.0, 0.0);
    CHECK(left.hit_x);
    CHECK(left.x == doctest::Approx(0.0));

    // Off the bottom.
    const game::Resolution down =
        game::slide(grid, box(16.0, 48.0, 8.0, 8.0), 0.0, 20.0);
    CHECK(down.hit_y);
    CHECK(down.y == doctest::Approx(56.0));

    // Off the top.
    const game::Resolution up =
        game::slide(grid, box(16.0, 8.0, 8.0, 8.0), 0.0, -20.0);
    CHECK(up.hit_y);
    CHECK(up.y == doctest::Approx(0.0));
}

TEST_CASE("a slide across a flat floor does not catch on the seams")
{
    // The failure the half-open rule exists to prevent: a mover resting
    // exactly on a floor, walked sideways across the boundary between two
    // floor tiles, must not report a hit at each seam.
    gfx::TileLayer layer = empty_layer(8, 8);
    for (int column = 0; column < 8; ++column) {
        fill(layer, column, 4); // floor at y in [64,80)
    }

    const game::TileGrid grid = grid_over(layer);

    // A 12-wide box resting exactly on the floor: bottom edge at 64.
    double x = 4.0;
    for (int step = 0; step < 40; ++step) {
        const game::Resolution moved =
            game::slide(grid, box(x, 52.0, 12.0, 12.0), 2.0, 0.0);
        CHECK_FALSE(moved.hit_x);
        CHECK(moved.y == doctest::Approx(52.0));
        x = moved.x;
    }

    CHECK(x == doctest::Approx(84.0));
}

TEST_CASE("a mover falling onto a floor lands flush on it")
{
    gfx::TileLayer layer = empty_layer(8, 8);
    for (int column = 0; column < 8; ++column) {
        fill(layer, column, 4); // floor at y in [64,80)
    }

    const game::TileGrid grid = grid_over(layer);

    const game::Resolution landed =
        game::slide(grid, box(20.0, 20.0, 12.0, 12.0), 0.0, 100.0);

    CHECK(landed.hit_y);
    CHECK(landed.y == doctest::Approx(52.0)); // 64 - 12

    // And resting there, another downward push does not sink into it.
    const game::Resolution resting =
        game::slide(grid, box(landed.x, landed.y, 12.0, 12.0), 0.0, 4.0);
    CHECK(resting.hit_y);
    CHECK(resting.y == doctest::Approx(52.0));
}

TEST_CASE("a slide works in negative coordinates")
{
    // Nothing here is inside the layer, so every cell is solid by the
    // out-of-bounds rule; what is being checked is that the tile arithmetic
    // does not break down left of and above the origin.
    const gfx::TileLayer layer = empty_layer(4, 4);
    const game::TileGrid grid = grid_over(layer);

    // Starting outside, moving further out: it is already overlapping, so it
    // is not trapped and may keep going.
    const game::Resolution out =
        game::slide(grid, box(-40.0, -40.0, 8.0, 8.0), -8.0, -8.0);
    CHECK(out.x == doctest::Approx(-48.0));
    CHECK(out.y == doctest::Approx(-48.0));
}

TEST_CASE(
    "a box outside the layer walks home iff no outside cell is in its way")
{
    // The exact out-of-bounds rule the header states, pinned in both
    // directions so that the statement there cannot drift from the behaviour.
    // Outside is solid, but only cells the box does NOT already occupy can
    // block it — so the escape route is precisely the cells the box already
    // stands in, and every number below is one the header quotes.
    const gfx::TileLayer layer = empty_layer(4, 4); // 64x64 pixels
    const game::TileGrid grid = grid_over(layer);

    // Filling the cell adjacent to the map, [-16, 0): the only cell between
    // this box and column 0 is the one it is standing in, so nothing blocks it
    // and it walks straight back in with no hit at all.
    const game::Resolution home =
        game::slide(grid, box(-16.0, 16.0, 16.0, 16.0), 16.0, 0.0);
    CHECK(home.x == doctest::Approx(0.0));
    CHECK_FALSE(home.hit_x);

    // One cell further out. Cell -1 now lies between the box and the map, is
    // outside, and is not occupied — so it blocks, and the box stops flush
    // against it instead of reaching the map.
    const game::Resolution stopped =
        game::slide(grid, box(-24.0, 16.0, 8.0, 8.0), 24.0, 0.0);
    CHECK(stopped.hit_x);
    CHECK(stopped.x == doctest::Approx(-24.0)); // -16 - 8, where it started

    // Further out still, and the same rule keeps it there however hard it is
    // pushed: this is the x = -40 probe the header cites.
    const game::Resolution stuck =
        game::slide(grid, box(-40.0, 16.0, 8.0, 8.0), 64.0, 0.0);
    CHECK(stuck.hit_x);
    CHECK(stuck.x == doctest::Approx(-40.0));

    // A box wide enough to span from outside to the map edge escapes for the
    // same reason the first one does — every cell in the way is one it already
    // occupies.
    const game::Resolution wide =
        game::slide(grid, box(-32.0, 16.0, 32.0, 16.0), 16.0, 0.0);
    CHECK_FALSE(wide.hit_x);
    CHECK(wide.x == doctest::Approx(-16.0));

    // And the rule is not an X-axis accident.
    const game::Resolution down =
        game::slide(grid, box(16.0, -16.0, 16.0, 16.0), 0.0, 16.0);
    CHECK(down.y == doctest::Approx(0.0));
    CHECK_FALSE(down.hit_y);

    const game::Resolution held =
        game::slide(grid, box(16.0, -24.0, 8.0, 8.0), 0.0, 24.0);
    CHECK(held.hit_y);
    CHECK(held.y == doctest::Approx(-24.0));
}
