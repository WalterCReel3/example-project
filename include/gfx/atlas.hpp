#pragma once

#include <string>
#include <vector>

#include <gfx/renderer/context.hpp>
#include <gfx/renderer/texture.hpp>

//============================================================================
//
// Texture atlas: named regions of one texture
//
//     gfx::Atlas dude(std::move(sheet), std::move(frames));
//
//     const gfx::Atlas::Index run = dude.find("reddude.000");   // once
//     dude.draw(context, run + n, x, y);                        // per frame
//
// An *atlas* is arbitrarily packed, indexed by name, and its regions may be
// trimmed — which is what a Sparrow document describes. A *sprite sheet* is a
// uniform grid indexed by number, which is what a TMX tileset will be. They are
// different enough to want different types, so this one does not take the
// "sheet" name.
//
// Two things are deliberately not here. The atlas does not parse: a frame table
// is plain data and loaders::load_sparrow produces one without a Context, an
// SDL surface or a window, which is what keeps its tests headless. And the
// atlas does not resolve asset paths: the caller pairs a frame table with a
// Texture it uploaded itself.
//
// See planning/2026-07-25-software-2d-sprites-tiling/.
//
//============================================================================
namespace gfx
{

// A named region of the sheet. An aggregate: there is no invariant to maintain
// here, and validating a frame against a texture it does not know about is not
// something it could do anyway. The loader is what rejects a malformed one.
struct AtlasFrame {
    std::string id;

    // The region in the sheet, in sheet pixels.
    renderer::Rect source;

    // Where `source` sits inside the untrimmed frame, added to the destination
    // on draw. Zero for an untrimmed frame, so the common case costs nothing.
    //
    // Sparrow spells this as frameX/frameY and states it the other way round —
    // negative, measured from the trimmed region back to the original's origin.
    // The loader negates it once, here, so that every consumer adds rather than
    // some adding and some subtracting.
    int trim_x = 0;
    int trim_y = 0;

    // The untrimmed size, or 0x0 when the frame is not trimmed. Kept because it
    // is what a layout wants: two frames of an animation that trim differently
    // have different `source` sizes and the same frame size, and positioning
    // against the source size makes the sprite jitter.
    int frame_width = 0;
    int frame_height = 0;

    bool trimmed() const { return frame_width > 0 && frame_height > 0; }

    // The size to reserve for this frame — untrimmed where that is known.
    int width() const { return trimmed() ? frame_width : source.w; }
    int height() const { return trimmed() ? frame_height : source.h; }
};

class Atlas
{
public:
    typedef std::vector<AtlasFrame> Frames;
    typedef Frames::size_type Index;

    // What find() returns for a name the atlas does not have. Spelled like
    // std::string::npos because it means the same thing.
    static constexpr Index npos = static_cast<Index>(-1);

    // Takes the texture — it is move-only, so the move is visible at the call
    // site — and the frame table that goes with it. Nothing checks that the
    // frames lie inside the texture: see contains_frames() below, which the
    // caller runs if it wants that answered.
    Atlas(renderer::Texture sheet, Frames frames);

    // Rule of zero. Texture is move-only, so this is movable and not copyable
    // without declaring anything, and there is no resource here that a
    // destructor would have to help with.

    Index size() const { return _frames.size(); }
    bool empty() const { return _frames.empty(); }

    const AtlasFrame& operator[](Index index) const { return _frames[index]; }
    const Frames& frames() const { return _frames; }

    const renderer::Texture& texture() const { return _sheet; }

    // Resolve a name to an index, or npos. Linear: this is a load-time call —
    // drawing takes the index — so a map would cost memory and a build step to
    // save time nothing is spending.
    //
    // A document may name two frames the same. This returns the first, matching
    // the order the loader read them in.
    Index find(const std::string& id) const;

    // Draws frame `index` with its untrimmed top-left at (x, y), so a trimmed
    // and an untrimmed frame of the same animation line up.
    //
    // This is a member rather than a Context::draw() overload because Atlas
    // sits above gfx::renderer and the dependency should not point back.
    //
    // `scale` multiplies both axes, and the trim offset with them, which is the
    // reason it belongs here rather than at the call site: scaling a trimmed
    // frame means scaling where it sits inside its box too, and a caller doing
    // the arithmetic by hand gets that wrong in a way that only shows up on
    // frames that happen to be trimmed asymmetrically.
    //
    // Integer, not a float: pixel art at 2.5x has unevenly sized pixels, and a
    // handheld panel is an integer multiple of the art or it is not scaled.
    void draw(renderer::Context& context, Index index, int x, int y,
              int scale = 1) const;

    // Whether every frame's source rectangle lies inside the texture. Worth
    // asking once after construction: a frame that runs off the sheet is not an
    // error SDL reports, it is a blit that silently clamps or draws garbage,
    // and the usual cause is an atlas paired with the wrong image.
    bool contains_frames() const;

private:
    renderer::Texture _sheet;
    Frames _frames;
};

} // namespace gfx
