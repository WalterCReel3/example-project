#include <gfx/renderer/context.hpp>

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdexcept>
#include <string>

#include <gfx/renderer/texture.hpp>
#include <util/logging.hpp>

namespace gfx
{
namespace renderer
{

namespace
{

SDL_Color to_sdl(const Color& c)
{
    SDL_Color out;
    out.r = c.r;
    out.g = c.g;
    out.b = c.b;
    out.a = c.a;
    return out;
}

Uint32 to_sdl(Driver driver)
{
    switch (driver) {
    case Driver::Accelerated:
        return SDL_RENDERER_ACCELERATED;
    case Driver::Software:
        return SDL_RENDERER_SOFTWARE;
    case Driver::PreferAccelerated:
        // Deliberately 0 rather than SDL_RENDERER_ACCELERATED. SDL walks its
        // driver list in preference order and takes the first that satisfies
        // the flags, with the accelerated drivers ahead of software -- so 0
        // already means "accelerated if you can, software if you cannot".
        // Passing SDL_RENDERER_ACCELERATED here would turn the fallback into a
        // failure.
        break;
    }
    return 0;
}

} // namespace

Context::Context(const std::string& title, int width, int height,
                 bool fullscreen, Driver driver)
    : _window(nullptr)
    , _renderer(nullptr)
    , _width(0)
    , _height(0)
{
    Uint32 flags = 0;
    if (fullscreen) {
        // FULLSCREEN_DESKTOP rather than FULLSCREEN: it takes the panel's
        // native mode instead of asking for a mode switch the device may not
        // support.
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(SDL_DISABLE);
    }

    _window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!_window) {
        throw std::runtime_error(std::string("could not create window: ") +
                                 SDL_GetError());
    }

    // -1 lets SDL pick the first driver in its own preference order that
    // satisfies the flags; see to_sdl(Driver) for why PreferAccelerated passes
    // 0.
    _renderer = SDL_CreateRenderer(_window, -1, to_sdl(driver));
    if (!_renderer) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(_window);
        _window = nullptr;
        throw std::runtime_error("could not create renderer: " + error);
    }

    SDL_GetRendererOutputSize(_renderer, &_width, &_height);

    // Blend so text and sprites composite rather than punching holes.
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

    // Which driver was selected decides whether this is a GPU or two Cortex-A7
    // cores doing the blitting, so it is worth a line at info rather than
    // debug.
    util::log_info("renderer context %dx%d via %s (%s)", _width, _height,
                   driver_name().c_str(),
                   accelerated() ? "accelerated" : "software");
}

Context::~Context()
{
    if (_renderer) {
        SDL_DestroyRenderer(_renderer);
    }
    if (_window) {
        SDL_DestroyWindow(_window);
    }
}

std::string Context::driver_name() const
{
    SDL_RendererInfo info;
    if (!_renderer || SDL_GetRendererInfo(_renderer, &info) != 0) {
        return "(unknown)";
    }
    return info.name ? info.name : "(unnamed)";
}

bool Context::accelerated() const
{
    SDL_RendererInfo info;
    if (!_renderer || SDL_GetRendererInfo(_renderer, &info) != 0) {
        return false;
    }
    // Read back from SDL rather than inferred from the requested Driver: a
    // PreferAccelerated request can resolve either way, and that is the whole
    // question this answers.
    return (info.flags & SDL_RENDERER_ACCELERATED) != 0;
}

void Context::clear(const Color& color)
{
    SDL_SetRenderDrawColor(_renderer, color.r, color.g, color.b, color.a);
    SDL_RenderClear(_renderer);
}

void Context::present()
{
    SDL_RenderPresent(_renderer);
}

void Context::draw(const Texture& texture, const Rect* src, const Rect* dst)
{
    if (!texture.handle()) {
        return;
    }

    SDL_Rect source;
    SDL_Rect target;

    if (src) {
        source.x = src->x;
        source.y = src->y;
        source.w = src->w;
        source.h = src->h;
    }
    if (dst) {
        target.x = dst->x;
        target.y = dst->y;
        target.w = dst->w;
        target.h = dst->h;
    }

    SDL_RenderCopy(_renderer, texture.handle(), src ? &source : nullptr,
                   dst ? &target : nullptr);
}

void Context::draw_surface(SDL_Surface* surface, Rect* rect)
{
    if (!surface || !rect) {
        return;
    }

    // Built on Texture rather than calling SDL_CreateTextureFromSurface
    // directly, so the upload happens in exactly one place. Two copies of this
    // would drift the moment one of them gained a scale mode or a blend
    // setting, and the difference would show as a sprite path and a text path
    // that filter differently for no stated reason.
    try {
        const Texture texture(*this, surface);

        Rect target;
        target.x = rect->x;
        target.y = rect->y;
        target.w = texture.width();
        target.h = texture.height();

        draw(texture, nullptr, &target);

        rect->w = target.w;
        rect->h = target.h;
    } catch (const std::exception& e) {
        // Preserves the old behaviour of logging and carrying on: a failed HUD
        // upload should cost the text, not the frame.
        util::log_error("draw_surface: %s", e.what());
    }
}

void Context::draw_text(const std::string& text, TTF_Font* font,
                        const Color& color, Rect* rect)
{
    if (!font || !rect || text.empty()) {
        return;
    }

    // Blended gives antialiased output; on the weakest devices Solid is cheaper
    // if this ever shows up in a profile.
    SDL_Surface* rendered =
        TTF_RenderUTF8_Blended(font, text.c_str(), to_sdl(color));
    if (!rendered) {
        util::log_error("could not render text: %s", TTF_GetError());
        return;
    }

    // No power-of-two rounding here, unlike the GL path — SDL_Renderer has no
    // such constraint, so gfx::render_text's make_texture_size() is
    // unnecessary.
    draw_surface(rendered, rect);
    SDL_FreeSurface(rendered);
}

bool Context::save_screenshot(const std::string& path) const
{
    if (!_renderer || _width <= 0 || _height <= 0) {
        util::log_error("screenshot: no renderer");
        return false;
    }

    // ARGB8888 rather than the renderer's native format: SDL_RenderReadPixels
    // converts on the way out, and asking for one known format means the BMP is
    // identical whichever driver produced it. A software-driver screenshot and
    // an opengles2 one are then directly comparable, which is the point of
    // taking them.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, _width, _height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        util::log_error("screenshot: could not allocate surface: %s",
                        SDL_GetError());
        return false;
    }

    // No row flip here, unlike the GL path: SDL_Renderer's origin is already
    // top-left, which is also the BMP writer's.
    if (SDL_RenderReadPixels(_renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        util::log_error("screenshot: SDL_RenderReadPixels failed: %s",
                        SDL_GetError());
        SDL_FreeSurface(surface);
        return false;
    }

    const bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
    if (!ok) {
        util::log_error("screenshot: could not write %s: %s", path.c_str(),
                        SDL_GetError());
    } else {
        util::log_info("screenshot: wrote %s (%dx%d)", path.c_str(), _width,
                       _height);
    }

    SDL_FreeSurface(surface);
    return ok;
}

} // namespace renderer
} // namespace gfx
