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
    // One line, because the glyph loop now lives on GlyphSheet: the HUD needs
    // the same plotting on the Miyoo Mini, where nothing drawn through
    // SDL_RenderCopy survives the driver's rotation, and two copies of it would
    // drift. dx and dy shift the whole string for the drop shadow.
    void pass(gfx::renderer::LayerLock& pixels, const ScrollState& s,
              std::uint32_t color, int dx, int dy)
    {
        _sheet.plot(pixels, s.text, s.x + dx, s.y + dy, s.scale, color);
    }

    const GlyphSheet& _sheet;
};

} // namespace

std::unique_ptr<Scroller> make_cpu_scroller(const GlyphSheet& sheet)
{
    return std::unique_ptr<Scroller>(new ScrollerCpu(sheet));
}

} // namespace coppers
