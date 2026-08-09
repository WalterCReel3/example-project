#pragma once

//============================================================================
//
// Renderer value types, and the predicates that go with them
//
// Colour and rectangles, split out of context.hpp because they are not the
// Context's: an Atlas frame, a TileSet cell and a Layer's destination are all
// rectangles, and none of them is a window. context.hpp includes this, so no
// consumer had to change.
//
// The counterpart of gfx/types.hpp, which holds the renderer-neutral geometry a
// *loader* produces. These are the ones a renderer consumes, in device pixels,
// which is why they are integers and live under gfx/renderer/.
//
//============================================================================
namespace gfx
{
namespace renderer
{

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct Rect {
    int x;
    int y;
    int w;
    int h;
};

// Whether `inner` lies entirely within `outer`.
//
// Named because the inline form was written twice — Atlas::contains_frames and
// TileSet::fits_texture — and both are asking the same question of a sheet: is
// this cell actually on it. The failure it catches is one SDL does not report,
// since a source rectangle past the edge of a texture clamps or reads a
// neighbouring cell and returns success either way.
inline bool contains(const Rect& outer, const Rect& inner)
{
    return inner.x >= outer.x && inner.y >= outer.y &&
           inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

// A rectangle at the origin, which is what a texture or a render target is when
// something is being tested against its bounds.
inline Rect bounds_of(int width, int height)
{
    return Rect{0, 0, width, height};
}

// Zero-area rectangles are not drawable, and a negative extent is a caller
// error rather than an empty one.
inline bool is_empty(const Rect& rect)
{
    return rect.w <= 0 || rect.h <= 0;
}

// Whether the two overlap in at least one pixel.
//
// The empty test is not redundant with the comparisons below it: a zero-width
// rectangle at x=5 spans the half-open range [5,5), which contains nothing, but
// the four-comparison form still reports an overlap with anything straddling
// x=5. "There is nothing to draw" is the reading that matches every caller.
inline bool intersects(const Rect& a, const Rect& b)
{
    if (is_empty(a) || is_empty(b)) {
        return false;
    }

    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}

} // namespace renderer
} // namespace gfx
