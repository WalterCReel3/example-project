#include <gfx/renderer/context.hpp>

#include <util/algorithm.hpp>

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstring>
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
        SDL_ShowCursor(SDL_DISABLE);

        // FULLSCREEN_DESKTOP rather than FULLSCREEN: it takes the panel's
        // native mode instead of asking for a mode switch the device may not
        // support. It takes that mode from the display's *desktop mode*, and
        // that is only safe where the driver sets one.
        //
        // The Miyoo Mini's driver does not. It builds its display with
        // `SDL_VideoDisplay display = {0}`, adds ten modes to it and calls
        // SDL_AddVideoDisplay without ever assigning desktop_mode, so the mode
        // stays zeroed. A FULLSCREEN_DESKTOP window then sizes itself to 0x0 —
        // and its presentation callback blits `{0, 0, window->w, window->h}` to
        // the panel every frame, which is a no-op that reports no error. The
        // symptom is a black screen with audio and input working normally.
        //
        // So there are three rungs, and the second is the Miyoo Mini's.
        //
        // FULLSCREEN_DESKTOP and FULLSCREEN fail differently on a driver like
        // that, which is the useful part: DESKTOP sizes the window from the
        // desktop mode and gets 0x0, while plain FULLSCREEN sizes it from the
        // closest *listed* mode — and that driver lists ten, 640x480 among
        // them, with a SetDisplayMode that returns success without doing
        // anything. Exclusive fullscreen therefore works where the desktop
        // variant cannot.
        //
        // The last rung is not a degraded mode on such hardware, whatever it
        // looks like from a desktop. That driver has no window manager and no
        // window concept: its presentation callback scales whatever the window
        // holds to the entire framebuffer, so a "windowed" surface is shown
        // exactly as a fullscreen one. It is last because on every other target
        // the distinction is real.
        SDL_DisplayMode mode;

        if (SDL_GetDesktopDisplayMode(0, &mode) == 0 && mode.w > 0 &&
            mode.h > 0) {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else if (SDL_GetNumVideoDisplays() > 0 &&
                   SDL_GetNumDisplayModes(0) > 0 &&
                   SDL_GetDisplayMode(0, 0, &mode) == 0 && mode.w > 0 &&
                   mode.h > 0) {
            flags |= SDL_WINDOW_FULLSCREEN;
            util::log_warning(
                "no desktop mode reported; using exclusive fullscreen against "
                "the driver's own mode list (%d modes, first %dx%d)",
                SDL_GetNumDisplayModes(0), mode.w, mode.h);
        } else {
            util::log_warning(
                "fullscreen requested, but the video driver reports neither a "
                "desktop mode nor a mode list; creating a %dx%d window, which "
                "such a driver still presents full screen",
                width, height);
        }
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

    resolve_output_size(width, height);

    // Blend so text and sprites composite rather than punching holes.
    SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

    // Which driver was selected decides whether this is a GPU or two Cortex-A7
    // cores doing the blitting, so it is worth a line at info rather than
    // debug.
    util::log_info("renderer context %dx%d via %s (%s)", _width, _height,
                   driver_name().c_str(),
                   accelerated() ? "accelerated" : "software");
}

// Four ways of asking how big the output is, in decreasing order of authority,
// because on a real device the first one answers 0x0 and reports success.
//
// The Miyoo Mini's SDL2 — a vendor fork with an MI_GFX video driver — returns a
// zero size from SDL_GetRendererOutputSize. Nothing downstream can work with
// that: coppers' layer clamped itself to 1x1, every frame drew nothing, and the
// only symptom was a black panel with the audio still playing. A renderer whose
// output size is unknown is not a renderer, so this asks the other questions
// SDL can answer and refuses to continue if none of them can.
//
// ALL FOUR are probed even once one has answered, and each is logged. One line
// per query costs nothing at startup and makes the device's answers directly
// comparable with the dev box's — which is how the 0x0 was identified, and is
// the only way to know whether the next firmware or the next device is broken
// in the same place or a different one.
void Context::resolve_output_size(int requested_width, int requested_height)
{
    int renderer_w = 0, renderer_h = 0;
    const bool renderer_ok =
        SDL_GetRendererOutputSize(_renderer, &renderer_w, &renderer_h) == 0;
    util::log_info("output size: renderer %dx%d (%s)", renderer_w, renderer_h,
                   renderer_ok ? "reported success" : SDL_GetError());

    int window_w = 0, window_h = 0;
    SDL_GetWindowSize(_window, &window_w, &window_h);
    util::log_info("output size: window %dx%d", window_w, window_h);

    int mode_w = 0, mode_h = 0;
    SDL_DisplayMode mode;
    const int display = SDL_GetWindowDisplayIndex(_window);
    if (display >= 0 && SDL_GetCurrentDisplayMode(display, &mode) == 0) {
        mode_w = mode.w;
        mode_h = mode.h;
        util::log_info("output size: display %d mode %dx%d @ %d Hz", display,
                       mode_w, mode_h, mode.refresh_rate);
    } else {
        util::log_info("output size: display mode unavailable (%s)",
                       SDL_GetError());
    }

    util::log_info("output size: requested %dx%d", requested_width,
                   requested_height);

    const char* source = nullptr;

    if (renderer_ok && renderer_w > 0 && renderer_h > 0) {
        _width = renderer_w;
        _height = renderer_h;
        source = "renderer output";
    } else if (window_w > 0 && window_h > 0) {
        _width = window_w;
        _height = window_h;
        source = "window";
    } else if (mode_w > 0 && mode_h > 0) {
        _width = mode_w;
        _height = mode_h;
        source = "display mode";
    } else if (requested_width > 0 && requested_height > 0) {
        // Right for a windowed run by construction; for a fullscreen one it is
        // a guess — but a guess that draws beats a certainty that does not, and
        // the lines above say which happened.
        _width = requested_width;
        _height = requested_height;
        source = "requested size";
    }

    if (!source) {
        SDL_DestroyRenderer(_renderer);
        _renderer = nullptr;
        SDL_DestroyWindow(_window);
        _window = nullptr;
        throw std::runtime_error(
            "no usable output size: renderer, window, display mode and "
            "requested size all reported zero");
    }

    if (std::strcmp(source, "renderer output") != 0) {
        util::log_warning("output size: taken from the %s, because the "
                          "renderer did not give one",
                          source);
    }
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
    if (!_renderer || !util::all_positive(_width, _height)) {
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
