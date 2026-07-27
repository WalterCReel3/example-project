#include <cmath>

#include <gfx/renderer/layer.hpp>
#include <rig/timing.hpp>

#include "scroller.hpp"

namespace coppers
{

namespace
{

// The same glyphs, plotted a pixel at a time into the locked layer.
//
// This is the hand-written half of the comparison. It does what SDL's blitter
// does — clip, scale, composite — for this one case, which is exactly the work
// the texture path gets for free, and the point of having both is to find out
// what that convenience costs or saves on the device.
//
// Note what it can do that the texture path cannot: the glyphs are IN the
// framebuffer, so any subsequent per-pixel effect applies to text and bars
// together. That is the reason this mechanism is worth keeping even if it loses
// on speed.
class ScrollerCpu : public Scroller
{
public:
    explicit ScrollerCpu(const GlyphSheet& sheet)
        : _sheet(sheet)
    {
    }

    const char* name() const override { return "cpu"; }
    ScrollPhase phase() const override { return ScrollPhase::Plot; }

    void plot(gfx::renderer::LayerLock& pixels, const ScrollState& s) override
    {
        const double start = rig::FrameClock::now();

        const int offset = s.scale < 1 ? 1 : s.scale;
        if (s.shadow) {
            pass(pixels, s,
                 gfx::renderer::Layer::pack(s.shadow_r, s.shadow_g, s.shadow_b),
                 offset, offset);
        }
        pass(pixels, s, gfx::renderer::Layer::pack(s.r, s.g, s.b), 0, 0);

        record((rig::FrameClock::now() - start) * 1000000.0);
    }

    double text_width(const std::string& text, int scale) const override
    {
        return static_cast<double>(text.size()) *
               static_cast<double>(_sheet.cell_width() * scale);
    }

private:
    void pass(gfx::renderer::LayerLock& pixels, const ScrollState& s,
              std::uint32_t color, int dx, int dy)
    {
        const int cell_w = _sheet.cell_width();
        const int cell_h = _sheet.cell_height();
        const int scale = s.scale < 1 ? 1 : s.scale;
        const int step = cell_w * scale;
        const int layer_w = pixels.width();
        const int layer_h = pixels.height();

        for (std::size_t i = 0; i < s.text.size(); ++i) {
            const int origin_x = static_cast<int>(std::floor(
                s.x + static_cast<double>(i) * static_cast<double>(step)));

            if (origin_x + step <= 0) {
                continue;
            }
            if (origin_x >= layer_w) {
                break;
            }
            if (s.text[i] == ' ') {
                continue;
            }

            for (int gy = 0; gy < cell_h; ++gy) {
                for (int sy = 0; sy < scale; ++sy) {
                    const int y = s.y + dy + gy * scale + sy;
                    // Clipped per output row rather than per pixel: the row
                    // pointer is only valid inside the layer, and hoisting the
                    // test out of the inner loop is most of the difference
                    // between this being competitive and not.
                    if (y < 0 || y >= layer_h) {
                        continue;
                    }

                    std::uint32_t* row = pixels.row(y);

                    for (int gx = 0; gx < cell_w; ++gx) {
                        if (!_sheet.pixel(s.text[i], gx, gy)) {
                            continue;
                        }
                        const int base = origin_x + dx + gx * scale;
                        for (int sx = 0; sx < scale; ++sx) {
                            const int x = base + sx;
                            if (x < 0 || x >= layer_w) {
                                continue;
                            }
                            // Opaque store, not a blend. The sheet is 1-bit, so
                            // there is no partial coverage to composite and a
                            // read-modify-write per pixel would be paid for
                            // nothing.
                            row[x] = color;
                        }
                    }
                }
            }
        }
    }

    const GlyphSheet& _sheet;
};

} // namespace

std::unique_ptr<Scroller> make_cpu_scroller(const GlyphSheet& sheet)
{
    return std::unique_ptr<Scroller>(new ScrollerCpu(sheet));
}

} // namespace coppers
