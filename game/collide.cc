#include <game/collide.hpp>

#include <util/integer.hpp>

#include <cmath>

namespace game
{

namespace
{

// The inclusive range of whole pixels a half-open interval [lo, hi) touches.
//
// A pixel p covers [p, p + 1), so it is touched when p < hi and p + 1 > lo —
// which is floor(lo) through ceil(hi) - 1. Going via pixels rather than
// dividing the doubles directly is what lets util::floor_div do the cell
// arithmetic in integers, where it is exact; tiles are pixel-aligned, so no
// cell is gained or lost on the way.
void pixel_span(double lo, double hi, int& first, int& last)
{
    first = static_cast<int>(std::floor(lo));
    last = static_cast<int>(std::ceil(hi)) - 1;
}

bool is_empty(const Aabb& box)
{
    return box.width <= 0.0 || box.height <= 0.0;
}

double right_of(const Aabb& box)
{
    return box.x + box.width;
}

double bottom_of(const Aabb& box)
{
    return box.y + box.height;
}

// Whether any cell in columns [first_column, last_column] of the given row
// range is solid.
bool any_solid(const TileGrid& grid, int first_column, int last_column,
               int first_row, int last_row)
{
    for (int row = first_row; row <= last_row; ++row) {
        for (int column = first_column; column <= last_column; ++column) {
            if (solid_at(grid, column, row)) {
                return true;
            }
        }
    }
    return false;
}

// One axis of the slide, written once and used for both.
//
// `lo`/`extent` are the moving box on the axis being swept; `other_lo`/
// `other_extent` are its fixed span on the other axis. `tile` and `other_tile`
// are the matching cell sizes. `column_major` says which way round to hand the
// two indexes to solid_at, which is the only thing that differs between
// sweeping X and sweeping Y.
//
// Returns the resolved `lo` and sets `hit`.
double sweep(const TileGrid& grid, double lo, double extent, double delta,
             double other_lo, double other_extent, int tile, int other_tile,
             bool column_major, bool& hit)
{
    hit = false;

    if (delta == 0.0 || extent <= 0.0 || other_extent <= 0.0) {
        return lo;
    }

    // The fixed span, as cells. It cannot be empty: other_extent > 0 above.
    int other_first = 0;
    int other_last = 0;
    if (!tile_span(other_lo, other_lo + other_extent, other_tile, other_first,
                   other_last)) {
        return lo;
    }

    // The cells the box already occupies on the swept axis. These are excluded
    // from the scan, which is what keeps an already-overlapping box from being
    // trapped by the very cell it is standing in.
    int start_first = 0;
    int start_last = 0;
    if (!tile_span(lo, lo + extent, tile, start_first, start_last)) {
        return lo;
    }

    const double moved_lo = lo + delta;
    int dest_first = 0;
    int dest_last = 0;
    if (!tile_span(moved_lo, moved_lo + extent, tile, dest_first, dest_last)) {
        return lo;
    }

    if (delta > 0.0) {
        // Scan rightward/downward from the first cell not already occupied.
        for (int cell = start_last + 1; cell <= dest_last; ++cell) {
            const bool blocked =
                column_major
                    ? any_solid(grid, cell, cell, other_first, other_last)
                    : any_solid(grid, other_first, other_last, cell, cell);
            if (blocked) {
                hit = true;
                // Flush: the trailing edge lands on this cell's near edge.
                return static_cast<double>(cell) * tile - extent;
            }
        }
        return moved_lo;
    }

    for (int cell = start_first - 1; cell >= dest_first; --cell) {
        const bool blocked =
            column_major ? any_solid(grid, cell, cell, other_first, other_last)
                         : any_solid(grid, other_first, other_last, cell, cell);
        if (blocked) {
            hit = true;
            // Flush: the leading edge lands on this cell's far edge.
            return static_cast<double>(cell + 1) * tile;
        }
    }
    return moved_lo;
}

} // namespace

bool tile_span(double lo, double hi, int tile, int& first, int& last)
{
    if (!(hi > lo) || tile <= 0) {
        return false;
    }

    int first_pixel = 0;
    int last_pixel = 0;
    pixel_span(lo, hi, first_pixel, last_pixel);

    if (last_pixel < first_pixel) {
        return false;
    }

    first = util::floor_div(first_pixel, tile);
    last = util::floor_div(last_pixel, tile);
    return true;
}

bool solid_at(const TileGrid& grid, int column, int row)
{
    // Not TileLayer::at(), which answers empty_tile outside the layer for the
    // renderer's benefit. Outside the layer is solid here; see the header.
    if (column < 0 || row < 0 || column >= grid.layer.width ||
        row >= grid.layer.height) {
        return true;
    }

    return grid.layer(column, row) != gfx::empty_tile;
}

bool overlaps(const Aabb& a, const Aabb& b)
{
    if (is_empty(a) || is_empty(b)) {
        return false;
    }

    return a.x < right_of(b) && b.x < right_of(a) && a.y < bottom_of(b) &&
           b.y < bottom_of(a);
}

bool overlaps_solid(const TileGrid& grid, const Aabb& box)
{
    if (is_empty(box)) {
        return false;
    }

    int first_column = 0;
    int last_column = 0;
    if (!tile_span(box.x, right_of(box), grid.tile_width, first_column,
                   last_column)) {
        return false;
    }

    int first_row = 0;
    int last_row = 0;
    if (!tile_span(box.y, bottom_of(box), grid.tile_height, first_row,
                   last_row)) {
        return false;
    }

    return any_solid(grid, first_column, last_column, first_row, last_row);
}

Resolution slide(const TileGrid& grid, const Aabb& box, double dx, double dy)
{
    Resolution result;
    result.x = box.x;
    result.y = box.y;

    if (is_empty(box) || grid.tile_width <= 0 || grid.tile_height <= 0) {
        // An empty box collides with nothing, so it moves freely; a grid with
        // no cell size has no cells to run into.
        result.x = box.x + dx;
        result.y = box.y + dy;
        return result;
    }

    // X first, against the box's original y span, then Y against the resolved
    // x. See the header for why this is two 1D sweeps and not a swept AABB.
    result.x = sweep(grid, box.x, box.width, dx, box.y, box.height,
                     grid.tile_width, grid.tile_height,
                     /*column_major=*/true, result.hit_x);

    result.y = sweep(grid, box.y, box.height, dy, result.x, box.width,
                     grid.tile_height, grid.tile_width,
                     /*column_major=*/false, result.hit_y);

    return result;
}

} // namespace game
