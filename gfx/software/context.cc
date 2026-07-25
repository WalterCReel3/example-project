#include <gfx/software/context.hpp>

#include <SDL.h>
#include <SDL_ttf.h>
#include <stdexcept>
#include <string>
#include <util/logging.hpp>

namespace gfx
{
namespace software
{

namespace {

SDL_Color to_sdl(const Color& c)
{
    SDL_Color out;
    out.r = c.r;
    out.g = c.g;
    out.b = c.b;
    out.a = c.a;
    return out;
}

} // namespace

Context::Context(const std::string& title, int width, int height,
                 bool fullscreen)
    : _window(nullptr)
    , _renderer(nullptr)
    , _width(0)
    , _height(0)
{
    Uint32 flags = 0;
    if (fullscreen) {
        // FULLSCREEN_DESKTOP rather than FULLSCREEN: it takes the panel's
        // native mode instead of asking for a mode switch the device may not
        // support. The original Context requested a real mode change and also
        // OR'd in SDL_WINDOW_FULLSCREEN unconditionally, ignoring its own
        // parameter.
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(SDL_DISABLE);
    }

    _window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!_window) {
        throw std::runtime_error(std::string("could not create window: ")
                                 + SDL_GetError());
    }

    // -1 lets SDL pick the first driver satisfying the flags. On a GPU-less
    // device that resolves to the software renderer; asking for
    // SDL_RENDERER_SOFTWARE explicitly would refuse acceleration where it does
    // exist, which is not what we want on desktop.
    _renderer = SDL_CreateRenderer(_window, -1, 0);
    if (!_renderer) {
        SDL_DestroyWindow(_window);
        _window = nullptr;
        throw std::runtime_error(std::string("could not create renderer: ")
                                 + SDL_GetError());
    }

    SDL_GetRendererOutputSize(_renderer, &_width, &_height);

    // Blend so text and sprites composite rather than punching holes.
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

    util::logging.info() << "software context " << _width << "x" << _height
                         << " via " << driver_name() << std::endl;
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
        util::logging.error() << "could not create texture: " << SDL_GetError()
                              << std::endl;
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
    SDL_Surface* rendered = TTF_RenderUTF8_Blended(font, text.c_str(),
                                                   to_sdl(color));
    if (!rendered) {
        util::logging.error() << "could not render text: " << TTF_GetError()
                              << std::endl;
        return;
    }

    // No power-of-two rounding here, unlike the GL path — SDL_Renderer has no
    // such constraint, so gfx::render_text's make_texture_size() is unnecessary.
    draw_surface(rendered, rect);
    SDL_FreeSurface(rendered);
}

} // namespace software
} // namespace gfx
