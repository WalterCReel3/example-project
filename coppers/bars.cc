#include "bars.hpp"

#include <cmath>

#include <gfx/renderer/layer.hpp>

namespace coppers
{

namespace
{

// Six bars on staggered centres, with speeds that share no small common
// multiple.
//
// Both parts matter and the first is easy to get wrong. Giving every bar the
// same centre and a large amplitude looks right written down — they sweep the
// whole screen — but sin() is symmetric, so bars with unrelated phases still
// arrive at the middle together and the field reads as three fat bars instead
// of six. Staggering the centres and using a smaller amplitude keeps each bar
// in its own band, crossing its neighbours rather than merging with them.
//
// The speeds are irregular for a different reason: with related speeds the
// whole field visibly repeats on a short cycle, and not appearing to repeat is
// the entire appeal of the effect.
const Bar bars[] = {
    {0.16, 0.13, 0.83, 0.0, 22}, {0.31, 0.16, 1.27, 1.9, 18},
    {0.45, 0.14, 0.61, 3.4, 26}, {0.58, 0.17, 1.53, 0.7, 16},
    {0.72, 0.15, 0.97, 4.8, 24}, {0.86, 0.12, 1.79, 2.6, 20},
};

const std::size_t bar_total = sizeof(bars) / sizeof(bars[0]);

const Palette palette_table[] = {
    {"copper", 255, 148, 40, 12, 8, 24}, {"steel", 150, 190, 255, 8, 10, 28},
    {"acid", 150, 255, 60, 10, 20, 10},  {"magenta", 255, 70, 190, 24, 6, 26},
    {"gold", 255, 220, 60, 26, 18, 4},
};

const std::size_t palette_total =
    sizeof(palette_table) / sizeof(palette_table[0]);

// Luminance profile across a bar, for d in -1..1 from its centre.
//
// Not a plain cosine. The highlight term is what makes a copper bar look like
// polished metal rather than a blurry stripe: a narrow bright band offset from
// the centre, as though a light source were above it. Removing it leaves
// something that reads as a gradient, which is the usual reason a first attempt
// at this effect looks wrong without it being obvious why.
double luminance(double d)
{
    const double edge = std::cos(d * 1.5707963267948966); // cos(d * pi/2)
    const double body = edge * edge; // dark edges, full centre
    const double highlight = std::exp(-((d + 0.35) * (d + 0.35)) * 18.0);
    const double value = body * 0.78 + highlight * 0.5;
    return value > 1.0 ? 1.0 : value;
}

unsigned char scale_channel(unsigned char channel, double factor)
{
    const double value = static_cast<double>(channel) * factor;
    if (value <= 0.0) {
        return 0;
    }
    if (value >= 255.0) {
        return 255;
    }
    return static_cast<unsigned char>(value + 0.5);
}

} // namespace

const Palette* palettes()
{
    return palette_table;
}

std::size_t palette_count()
{
    return palette_total;
}

BarField::BarField(int height, std::size_t palette_index)
    : _rows(static_cast<std::size_t>(height > 0 ? height : 1))
    , _palette(0)
    , _background(0)
{
    set_palette(palette_index);
    for (std::size_t i = 0; i < _rows.size(); ++i) {
        _rows[i] = _background;
    }
}

void BarField::set_palette(std::size_t index)
{
    _palette = palette_total ? index % palette_total : 0;
    const Palette& p = palette_table[_palette];
    _background = gfx::renderer::Layer::pack(p.bg_r, p.bg_g, p.bg_b);
}

void BarField::next_palette()
{
    set_palette(_palette + 1);
}

const char* BarField::palette_name() const
{
    return palette_table[_palette].name;
}

std::size_t BarField::bar_count() const
{
    return bar_total;
}

void BarField::resolve(double t)
{
    const int rows = height();
    const Palette& p = palette_table[_palette];

    for (int y = 0; y < rows; ++y) {
        _rows[static_cast<std::size_t>(y)] = _background;
    }

    // Bar heights are authored against reference_height() so that changing the
    // layer's resolution rescales the composition rather than reshaping it.
    const double scale =
        static_cast<double>(rows) / static_cast<double>(reference_height());

    // In order, so a later bar overwrites an earlier one where they overlap.
    // That ordering is the depth cue — without it the field looks flat.
    for (std::size_t b = 0; b < bar_total; ++b) {
        const Bar& bar = bars[b];

        const double half = bar.height * scale * 0.5;
        if (half < 0.5) {
            continue;
        }

        const double centre =
            (bar.centre + bar.amplitude * std::sin(t * bar.speed + bar.phase)) *
            static_cast<double>(rows);

        // Rounded outward so a bar never vanishes between two scanlines, and
        // clipped rather than wrapped: a bar leaving the top should leave.
        int first = static_cast<int>(std::floor(centre - half));
        int last = static_cast<int>(std::ceil(centre + half));
        if (last < 0 || first >= rows) {
            continue;
        }
        if (first < 0) {
            first = 0;
        }
        if (last > rows - 1) {
            last = rows - 1;
        }

        for (int y = first; y <= last; ++y) {
            // -1..1 across the bar, which is what luminance() expects.
            const double d = (static_cast<double>(y) - centre) / half;
            if (d < -1.0 || d > 1.0) {
                continue;
            }
            const double factor = luminance(d);
            _rows[static_cast<std::size_t>(y)] = gfx::renderer::Layer::pack(
                scale_channel(p.r, factor), scale_channel(p.g, factor),
                scale_channel(p.b, factor));
        }
    }
}

} // namespace coppers
