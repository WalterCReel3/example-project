#include <SDL.h>

#include <cmath>

#include <gfx/renderer/context.hpp>
#include <gfx/renderer/texture.hpp>
#include <rig/timing.hpp>

#include "scroller.hpp"

namespace coppers
{

namespace
{

// One blit per visible glyph, from a sheet uploaded once at construction.
//
// This is the path that exercises Context::draw()'s source rectangle, which is
// the API that planning/2026-07-25-software-2d-sprites-tiling/ is blocked on —
// so the scroller doubles as the first real consumer of the sprite path rather
// than being throwaway demo code.
class ScrollerTexture : public Scroller
{
public:
    ScrollerTexture(gfx::renderer::Context& context, const GlyphSheet& sheet)
        : _sheet(sheet)
        , _texture(build(context, sheet))
    {
        // Blending on: the sheet is white-on-transparent, so the glyph margins
        // must let the copper bars through rather than punching black holes.
        _texture.set_blend(true);
    }

    const char* name() const override { return "texture"; }
    ScrollPhase phase() const override { return ScrollPhase::Composite; }

    void composite(gfx::renderer::Context& context,
                   const ScrollState& s) override
    {
        const double start = rig::FrameClock::now();

        // Shadow first so the bright pass lands on top of it.
        if (s.shadow) {
            _texture.set_color_mod(s.shadow_r, s.shadow_g, s.shadow_b);
            pass(context, s, s.scale, s.scale);
        }
        _texture.set_color_mod(s.r, s.g, s.b);
        pass(context, s, 0, 0);

        // Without this the number below is the time spent *queueing* draw
        // commands, not executing them — SDL batches, so the glyphs are still
        // unblitted when the timer stops. The first version of this reported
        // 4 us against the CPU blitter's 167 and the comparison looked
        // one-sided; flushing showed the texture path was in fact the slower of
        // the two on the software driver. Measuring an API that batches without
        // forcing execution measures nothing.
        SDL_RenderFlush(context.renderer());

        record((rig::FrameClock::now() - start) * 1000000.0);
    }

    double text_width(const std::string& text, int scale) const override
    {
        return static_cast<double>(text.size()) *
               static_cast<double>(_sheet.cell_width() * scale);
    }

private:
    // One offset pass over the visible glyphs. The colour is already set: it is
    // driver state rather than per-draw data, so modulating inside the loop
    // would be sixty redundant state changes a frame.
    void pass(gfx::renderer::Context& context, const ScrollState& s, int dx,
              int dy)
    {
        const int cell_w = _sheet.cell_width();
        const int cell_h = _sheet.cell_height();
        const int step = cell_w * s.scale;
        const int target_w = context.width();

        for (std::size_t i = 0; i < s.text.size(); ++i) {
            const int x = static_cast<int>(std::floor(
                s.x + static_cast<double>(i) * static_cast<double>(step)));

            // Cull off-screen glyphs. Without this the cost scales with the
            // whole message rather than with what is visible, and the message
            // is long enough that it would dominate the measurement.
            if (x + step <= 0) {
                continue;
            }
            if (x >= target_w) {
                break;
            }

            if (s.text[i] == ' ') {
                continue;
            }

            gfx::renderer::Rect src;
            _sheet.cell_rect(s.text[i], src.x, src.y, src.w, src.h);

            gfx::renderer::Rect dst;
            dst.x = x + dx;
            dst.y = s.y + dy;
            dst.w = step;
            dst.h = cell_h * s.scale;

            context.draw(_texture, &src, &dst);
        }
    }

    static gfx::renderer::Texture build(gfx::renderer::Context& context,
                                        const GlyphSheet& sheet)
    {
        SDL_Surface* surface = sheet.build_surface();
        try {
            gfx::renderer::Texture texture(context, surface);
            SDL_FreeSurface(surface);
            return texture;
        } catch (...) {
            SDL_FreeSurface(surface);
            throw;
        }
    }

    const GlyphSheet& _sheet;
    gfx::renderer::Texture _texture;
};

} // namespace

std::unique_ptr<Scroller> make_texture_scroller(gfx::renderer::Context& context,
                                                const GlyphSheet& sheet)
{
    return std::unique_ptr<Scroller>(new ScrollerTexture(context, sheet));
}

} // namespace coppers
