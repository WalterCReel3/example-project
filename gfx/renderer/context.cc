#include <gfx/renderer/context.hpp>

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdexcept>
#include <string>
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

void Context::draw_surface(SDL_Surface* surface, Rect* rect)
{
    if (!surface || !rect) {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(_renderer, surface);
    if (!texture) {
        util::log_error("could not create texture: %s", SDL_GetError());
        return;
    }

    SDL_Rect dst;
    dst.x = rect->x;
    dst.y = rect->y;
    dst.w = surface->w;
    dst.h = surface->h;

    SDL_RenderCopy(_renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);

    rect->w = dst.w;
    rect->h = dst.h;
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

} // namespace renderer
} // namespace gfx
