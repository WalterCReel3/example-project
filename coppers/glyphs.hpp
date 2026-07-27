#pragma once

#include <cstdint>
#include <string>
#include <vector>

//============================================================================
//
// The glyph sheet
//
// data/glyphs-16x16.png is 320x48: sixty 16x16 cells, twenty columns, three
// rows, covering ASCII 32..91 — space, punctuation, digits and uppercase. No
// lowercase, which is not a limitation to work around but what a cracktro
// scroller wants anyway; text is upper-cased on the way in.
//
// TWO PROPERTIES OF THE FILE THAT SHAPE THIS CLASS
//
// *It has no alpha channel.* Transparency is carried by the colour: the sheet
// is two tones, near-black background and near-white glyph. So it is read as a
// 1-bit MASK rather than colour-keyed into a surface. That is the more useful
// reading anyway — a mask can be drawn in any colour, so the palette switch
// recolours the scroller as well as the bars, from one upload.
//
// *The grid is uniform*, so glyph index to source rectangle is arithmetic and
// there is no atlas metadata to parse. That makes this the simplest possible
// first consumer of Context::draw()'s source rectangle, which is the point:
// the sprite path gets exercised by something with no other moving parts.
//
// One sheet feeds both scrollers. ScrollerTexture uploads the tinted surface
// once and blits cells from it; ScrollerCpu reads the mask directly and plots
// into the layer. They are drawing provably identical glyphs, which is what
// makes comparing their cost mean anything.
//
//============================================================================
struct SDL_Surface;

namespace coppers
{

class GlyphSheet
{
public:
    // Loads and decodes the sheet. Throws std::runtime_error if the file is
    // missing or is not the expected geometry — a silently wrong grid would
    // present as scrambled text rather than as an error.
    GlyphSheet(const std::string& path, int columns, int rows, int first_char);

    int cell_width() const { return _cell_w; }
    int cell_height() const { return _cell_h; }

    // Highest character this sheet can draw. Anything outside
    // [first_char, last_char] becomes a space.
    int first_char() const { return _first; }
    int last_char() const { return _first + _count - 1; }

    // Whether the glyph pixel at (x, y) within the cell for `c` is set.
    // Unchecked on x and y, which must be inside the cell.
    bool pixel(char c, int x, int y) const
    {
        const std::size_t base =
            cell_index(c) * static_cast<std::size_t>(_cell_w * _cell_h);
        return _mask[base + static_cast<std::size_t>(y * _cell_w + x)] != 0;
    }

    // Where the cell for `c` sits in the sheet image, for a source-rect blit.
    void cell_rect(char c, int& x, int& y, int& w, int& h) const;

    // The sheet as an ARGB surface, white glyphs on transparent, ready to
    // upload. The caller owns it and must SDL_FreeSurface it. Tinting is left
    // to the texture's colour modulation rather than baked in here, so a
    // palette change costs nothing.
    //
    // Note the forward declaration is at global scope above: writing
    // `struct SDL_Surface*` here would declare coppers::SDL_Surface and every
    // use of it would then fail to convert to the real one.
    SDL_Surface* build_surface() const;

    int sheet_width() const { return _columns * _cell_w; }
    int sheet_height() const { return _rows * _cell_h; }

private:
    std::size_t cell_index(char c) const;

    std::vector<std::uint8_t> _mask; // one byte per pixel, cell-major
    int _columns;
    int _rows;
    int _cell_w;
    int _cell_h;
    int _first;
    int _count;
};

// Upper-cases and replaces anything the sheet cannot draw, so a message written
// in mixed case renders rather than turning into blanks.
std::string to_sheet_text(const std::string& text, const GlyphSheet& sheet);

} // namespace coppers
