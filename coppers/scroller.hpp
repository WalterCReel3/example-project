#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <gfx/renderer/layer.hpp>

#include "glyphs.hpp"

//============================================================================
//
// Two ways to draw a scroller, switchable while it runs
//
// The demo ships both and a button swaps between them with their costs on
// screen, because the contrast is the useful part — the same reason skratch is
// kept as the 2016-versus-now GL comparison. An assertion about which is faster
// is worth much less than the two numbers side by side on the actual device.
//
//   ScrollerTexture   glyph sheet uploaded once, one source-rect blit per
//                     character through SDL. Draws into the render target,
//                     after the layer has been composited, so it is always at
//                     the window's resolution and unaffected by --layer-height.
//
//   ScrollerCpu       the same glyph mask, plotted by hand into the locked
//                     layer. Draws BEFORE the layer is composited, so it lives
//                     in the framebuffer at the layer's resolution and scales
//                     up with it.
//
// That difference is not an accident of the implementations, it is what each
// mechanism *is*, and it has a visible consequence: at --layer-height 240 on a
// 480-line panel the CPU text is chunky and the texture text is crisp. Worth
// seeing rather than hiding by forcing them to match.
//
// VIRTUAL DISPATCH IS CORRECT HERE. CLAUDE.md prefers compile-time
// polymorphism, and that preference is scoped to choices fixed at build time —
// which is why util::File selects its FileImpl with an #ifdef. This choice is
// made by a button press at runtime. A vtable is the right tool; do not
// "modernise" this into a traits dispatch or a template.
//
//============================================================================
namespace gfx
{
namespace renderer
{
class Context;
}
} // namespace gfx

namespace coppers
{

// When in the frame a scroller wants to be called. The two mechanisms genuinely
// cannot run at the same point — one needs the layer locked, the other needs it
// already composited — so the demo asks rather than guessing.
enum class ScrollPhase {
    Plot,     // during the layer lock, before compositing
    Composite // after the layer has been drawn to the render target
};

struct ScrollState {
    std::string text; // already passed through to_sheet_text()
    double x;         // left edge of the first glyph, in target pixels
    int y;            // top edge, in target pixels
    int scale;        // integer glyph magnification
    unsigned char r;  // colour, following the palette
    unsigned char g;
    unsigned char b;

    // A drop shadow one glyph-pixel down and right. Without it the text is
    // invisible wherever it crosses a bar, because both follow the same
    // palette. Period correct, and applied identically by both scrollers so the
    // cost comparison stays like for like — it roughly doubles the work for
    // each of them.
    bool shadow;
    unsigned char shadow_r;
    unsigned char shadow_g;
    unsigned char shadow_b;
};

class Scroller
{
public:
    virtual ~Scroller() {}

    virtual const char* name() const = 0;
    virtual ScrollPhase phase() const = 0;

    // Only one of these does anything for a given implementation; the other is
    // a no-op. Two hooks rather than one because the phases differ — folding
    // them into a single call would mean passing a locked layer to something
    // that must not touch it.
    virtual void plot(gfx::renderer::LayerLock& pixels, const ScrollState& s)
    {
        (void)pixels;
        (void)s;
    }

    virtual void composite(gfx::renderer::Context& context,
                           const ScrollState& s)
    {
        (void)context;
        (void)s;
    }

    // Microseconds spent in the most recent draw, smoothed. The measurement the
    // comparison exists for.
    double cost_us() const { return _cost_us; }

    // Width of `text` in target pixels, so the demo knows when it has scrolled
    // clear of the screen.
    virtual double text_width(const std::string& text, int scale) const = 0;

protected:
    void record(double microseconds)
    {
        if (_cost_us <= 0.0) {
            _cost_us = microseconds;
        } else {
            _cost_us += 0.05 * (microseconds - _cost_us);
        }
    }

private:
    double _cost_us = 0.0;
};

// One source-rect blit per glyph, from a texture uploaded once.
std::unique_ptr<Scroller> make_texture_scroller(gfx::renderer::Context& context,
                                                const GlyphSheet& sheet);

// The same glyphs plotted by hand into the layer.
std::unique_ptr<Scroller> make_cpu_scroller(const GlyphSheet& sheet);

} // namespace coppers
