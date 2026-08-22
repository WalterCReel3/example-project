#pragma once

#include <gfx/tilemap.hpp>

//============================================================================
//
// Collision: a box against a tile layer, and a box against a box
//
//     const game::TileGrid solid{layer, 16, 16};
//     if (game::overlaps_solid(solid, box)) { ... }
//
//     const game::Resolution moved = game::slide(solid, box, dx, dy);
//     entity.x = moved.x;
//     entity.y = moved.y;
//
// Free functions over plain data, not a class. There is no state to keep — a
// tile layer and a box are both arguments — and statelessness is what lets the
// whole of this be tested without a renderer, a window or a .tmx, the way
// loaders::load_sparrow and loaders::load_obj already are.
//
//============================================================================
namespace game
{

// An axis-aligned box in world pixels, HALF-OPEN on both axes: it spans
// [x, x + width) and [y, y + height). Two boxes sharing an edge therefore do
// not overlap, and a mover resting exactly on a floor is touching it rather
// than inside it. Every boundary case below follows from that one choice.
//
// A box with zero or negative extent on either axis overlaps nothing at all,
// matching gfx::renderer::intersects — "there is nothing here" is the reading
// every caller wants, and it means a default-constructed box is inert rather
// than a point collider at the origin.
//
// Doubles, because the entity position these are built from is a double: it
// integrates a velocity, and rounding the box to integers every frame would
// make a slow mover's collisions stutter independently of its sprite.
struct Aabb {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

// A tile layer plus the pixel size of one cell, which is what the layer itself
// does not carry — gfx::TileLayer is a grid of ids and the dimensions live on
// the gfx::TileSet beside it.
//
// Holds a reference, so it is a short-lived argument rather than something to
// store.
struct TileGrid {
    const gfx::TileLayer& layer;
    int tile_width;
    int tile_height;
};

//============================================================================
// What counts as solid
//============================================================================

// Any cell holding a tile is solid; a cell holding gfx::empty_tile is not.
// That is decision 2 of planning/2026-08-10-game-layer-and-demo/: solidity is
// authored as a dedicated invisible tile layer rather than as a per-tile
// property, so the presence of a tile IS the data.
//
// OUTSIDE THE LAYER IS SOLID, and this is the one decision here that is not
// forced by the half-open rule, so the reasoning matters.
//
// gfx::TileLayer::at() answers empty_tile outside the layer, deliberately, so
// that a renderer walking a viewport does not have to clamp first. Inheriting
// that here would be the naive reading and it is the wrong one: the two callers
// want opposite things. "Do not draw what is not there" is right for a
// renderer; "the player may walk into what is not there" is a level with no
// edges, and a mover that leaves the map falls forever with no way back. That
// failure is silent, unrecoverable in play, and costs a level restart.
//
// Solid outside costs a mover bumping an invisible wall at the map edge, which
// is immediately visible, self-correcting, and exactly what a level author
// wants anyway — the alternative is hand-drawing a border of collision tiles
// around every map and remembering to. The camera already clamps to level
// bounds (decision 4), so the wall sits where the view stops.
//
// The practical consequence: this does NOT call TileLayer::at(). It bounds-
// checks itself and answers the opposite way.
bool solid_at(const TileGrid& grid, int column, int row);

//============================================================================
// Overlap
//============================================================================

// Whether the two boxes share at least one point, half-open on both axes.
// Touching edges are not an overlap; either box being empty is not an overlap.
bool overlaps(const Aabb& a, const Aabb& b);

// Whether the box covers any part of any solid cell.
//
// A box flush against a tile's edge does not overlap it: a box spanning
// [16, 32) on a 16px grid covers column 1 only, never column 0 or column 2.
// This is the case that decides whether a mover catches on flat floors, and it
// falls out of the half-open rule rather than being special-cased.
bool overlaps_solid(const TileGrid& grid, const Aabb& box);

//============================================================================
// Moving
//============================================================================

// Where a box ended up, and whether each axis was stopped short.
//
// `hit_x` and `hit_y` report that motion on that axis was blocked, not that the
// box is touching something: a box that was already flush against a wall and is
// pushed further into it reports a hit and does not move, while a box moving
// parallel to that same wall reports no hit.
struct Resolution {
    double x = 0.0;
    double y = 0.0;
    bool hit_x = false;
    bool hit_y = false;
};

// Move `box` by (dx, dy) against the layer, stopping flush against whatever it
// runs into. Returns the box's resolved origin.
//
// THE RULES, so a caller can predict this without reading collide.cc:
//
// - AXIS SEPARATED, X FIRST. X is swept and resolved with the box's ORIGINAL y
//   span, then Y is swept with the already-resolved x. This is not a
//   time-of-impact sweep and deliberately so: TOI has to pick a winning axis at
//   a corner, and every cheap way of picking is wrong in some case that shows
//   up as a mover snagging on a seam between two floor tiles. Two independent
//   1D sweeps have no such case, at the cost of allowing a fast enough mover to
//   round a corner it arguably clipped.
//
// - A CORNER HIT stops both axes. Moving diagonally into the inside corner of
//   two walls ends flush on X and flush on Y, with both hit flags set. Moving
//   diagonally at the outside corner of a single block stops on X only, because
//   once X is flush the Y sweep has a clear column.
//
// - A ZERO-LENGTH SWEEP moves nothing and reports no hit, on each axis
//   independently. slide(grid, box, 0, 0) returns box's own origin with both
//   flags false even if the box is buried in solid tiles: nothing was asked
//   for, so nothing is refused. dx == 0 with dy != 0 resolves only Y.
//
// - AN ALREADY-OVERLAPPING START IS NOT TRAPPED BY THE CELL IT IS IN. Only
//   cells the box does not already occupy can block it. A box that begins
//   inside a wall — spawned badly, or left there by a level edit — can move
//   out of it, and can also move further along inside it, but cannot cross
//   into a NEW solid cell. The alternative, treating the current overlap as a
//   blocker, freezes such an entity at the position that was already wrong.
//
//   WHAT THAT MEANS OUTSIDE THE LAYER, which is worth knowing before it is met
//   in a debugger, because the two rules interact and neither one alone
//   predicts it. Every cell outside is solid, but only cells the box does not
//   already occupy can block it. So a box that starts OUTSIDE THE LAYER is
//   stuck exactly when an outside cell it does NOT already occupy lies between
//   it and the map — and free to walk home when there is no such cell, which is
//   the case whenever the cells it stands in already reach the map's edge.
//
//   Concretely, against a 4x4 layer of 16px cells: a 16-wide box at x = -16
//   fills the cell adjacent to column 0, so sliding it right by 16 lands it at
//   x = 0 reporting no hit at all. An 8-wide box at x = -24 has the unoccupied
//   cell [-16, 0) in the way, so the same push stops it flush at x = -24; one
//   at x = -40 stays at -40 however hard it is pushed, reporting a hit every
//   frame. All three are run rather than reasoned, and stay that way: the
//   "walks home iff" case in tests/test_collide.cc asserts each of them, so
//   this paragraph fails a build if it stops being true. Amend it from a test
//   run and not from reading sweep() — the interaction of the two rules above
//   is easy to follow to the wrong answer.
//
//   Either way this is a spawn-validation problem rather than a collision one,
//   and it is left here deliberately: the fix belongs in whatever reads the
//   spawn table, which should reject an out-of-bounds spawn loudly at load
//   rather than having collision quietly rescue it at runtime. Note that the
//   walk-home case makes that MORE necessary and not less — an entity frozen
//   at the edge with hit flags set is at least a loud symptom, whereas one
//   that strolls back inside from just past the boundary leaves nothing to
//   notice and the bad spawn survives into the next level edit.
//
// - STOPPING IS FLUSH, not short. A box blocked moving right ends with its
//   right edge exactly on the blocking column's left edge, so a repeated call
//   is idempotent: sliding into a wall twice leaves the box where the first
//   call put it and reports a hit both times.
Resolution slide(const TileGrid& grid, const Aabb& box, double dx, double dy);

// The half-open interval [lo, hi) expressed as the inclusive range of tile
// indexes it touches. Returns false, leaving the outputs untouched, when the
// interval is empty — which is the caller's signal that there is nothing to
// test rather than a range to iterate.
//
// Exposed because it is the world-to-tile conversion every function above
// shares, and it is the part most worth pinning directly: it is where
// util::floor_div earns its place, since a coordinate left of the origin
// divided with C's truncation reports cell 0 for the whole cell before it.
bool tile_span(double lo, double hi, int tile, int& first, int& last);

} // namespace game
