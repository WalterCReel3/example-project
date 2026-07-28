#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

//============================================================================
//
// The copper bar field
//
// Amiga copper bars were a list of colour registers rewritten per scanline by
// the copper coprocessor, so a "bar" is not an object drawn on a screen — it is
// a range of scanlines whose background colour differs. That is why this
// produces one colour per scanline and nothing else: it is the shape the effect
// actually has, and it makes the whole thing a function of time.
//
//     BarField field;
//     field.resolve(elapsed_seconds);
//     const std::uint32_t* rows = field.rows();   // one per scanline
//
// Kept free of any rendering call so it can be tested without a window, the
// same split rig::FrameTiming uses. Everything here is arithmetic over a time
// value; the plotting loop is four lines in the demo.
//
// Bars are opaque and later ones win, rather than blending. Both are period
// correct — there was nothing to blend with, since the copper was replacing the
// background colour outright — and it is also what keeps the resolve O(bars +
// scanlines) instead of needing a per-pixel composite.
//
//============================================================================
namespace coppers
{

// A colour ramp. Copper bars are a luminance gradient over one hue rather than
// a blend between two colours: dark at the bar's edges, saturated in the
// middle, and a highlight band just off centre that is what makes them read as
// metallic rather than as a soft gradient.
struct Palette {
    const char* name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    // Background behind the bars. Near-black rather than black: a true black
    // background makes the darkest bar edges disappear into it.
    unsigned char bg_r;
    unsigned char bg_g;
    unsigned char bg_b;
};

// Cycled by a button. Chosen to be distinguishable on a small panel in
// daylight, which rules out the subtle ones.
const Palette* palettes();
std::size_t palette_count();

// One bar: a band of scanlines oscillating vertically.
struct Bar {
    double centre;    // 0..1 of the screen, the midpoint of its travel
    double amplitude; // 0..1 of the screen
    double speed;     // radians per second
    double phase;     // radians, so bars do not move in lockstep
    int height;       // scanlines, at the reference height below
};

class BarField
{
public:
    // height is the number of scanlines to resolve, i.e. the layer's height.
    BarField(int height, std::size_t palette_index = 0);

    // Recomputes every scanline for time t in seconds.
    void resolve(double t);

    // One packed ARGB8888 colour per scanline, height() entries. Valid after
    // the first resolve(); before that, every entry is the background.
    const std::uint32_t* rows() const { return _rows.data(); }
    int height() const { return static_cast<int>(_rows.size()); }

    // Bars are sized against a 240-scanline reference and scaled to the actual
    // height, so switching internal resolution changes the sharpness and not
    // the composition. Without this a 240-line layer and a 560-line one look
    // like different demos.
    static int reference_height() { return 240; }

    std::size_t palette_index() const { return _palette; }
    void set_palette(std::size_t index);
    void next_palette();
    const char* palette_name() const;

    // Number of bars, exposed for the HUD rather than for control.
    std::size_t bar_count() const;

private:
    std::vector<std::uint32_t> _rows;
    std::size_t _palette;
    std::uint32_t _background;
};

} // namespace coppers
