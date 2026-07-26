#include <gfx/gles2/context.hpp>

#include <gfx/gles2/api.hpp>

#include <util/logging.hpp>

#include <SDL.h>

#include <stdexcept>

namespace gfx
{
namespace gles2
{

Context::Context(const std::string& title, int width, int height,
                 bool fullscreen)
    : _window(nullptr)
    , _gl(nullptr)
    , _width(0)
    , _height(0)
    , _es_profile(false)
{
    // Requested BEFORE SDL_CreateWindow: these are attributes of the context
    // the window is created with, and setting them afterwards has no effect on
    // it.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // 16 bits, not 24. A depth buffer at 640x480 costs 600 KB at 16 bits and
    // 900 KB at 24 on a device with 128 MB total, and Mali's fast paths are
    // 16-bit. Deep scenes are not what this renders.
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

    Uint32 flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        // FULLSCREEN_DESKTOP takes the panel's native mode rather than
        // requesting a mode switch a handheld may not offer.
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        SDL_ShowCursor(SDL_DISABLE);
    }

    _window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!_window) {
        throw std::runtime_error(std::string("could not create GL window: ") +
                                 SDL_GetError());
    }

    _gl = SDL_GL_CreateContext(_window);
    if (!_gl) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(_window);
        _window = nullptr;
        throw std::runtime_error("could not create GLES2 context: " + error +
                                 ". The driver may not offer an ES profile");
    }

    if (SDL_GL_MakeCurrent(_window, _gl) != 0) {
        const std::string error = SDL_GetError();
        SDL_GL_DeleteContext(_gl);
        SDL_DestroyWindow(_window);
        _gl = nullptr;
        _window = nullptr;
        throw std::runtime_error("could not make the GLES2 context current: " +
                                 error);
    }

    // Only now: SDL_GL_GetProcAddress resolves against the current context on
    // most drivers, so this cannot move earlier.
    load_api();

    // What was asked for is not always what arrives. SDL will return a
    // compatibility-profile desktop context where the driver prefers one, and
    // GLSL ES 1.00 shaders mostly still compile there — so this is reported
    // rather than rejected.
    int profile = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile);
    _es_profile = (profile == SDL_GL_CONTEXT_PROFILE_ES);
    if (!_es_profile) {
        util::log_warning(
            "gles2: asked for an ES profile and got mask 0x%x; "
            "GLSL ES 1.00 shaders usually still work, but this is "
            "not the configuration the handhelds run",
            static_cast<unsigned>(profile));
    }

    // Drawable size, not window size: they differ under a scaling compositor,
    // and the viewport and projection aspect must both come from the former.
    SDL_GL_GetDrawableSize(_window, &_width, &_height);

    set_viewport();
    gl::Enable(GL_DEPTH_TEST);
    gl::DepthFunc(GL_LEQUAL);
    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    util::log_info("gles2 context %dx%d, ES profile %s", _width, _height,
                   _es_profile ? "yes" : "no");
}

Context::~Context()
{
    if (_gl) {
        SDL_GL_DeleteContext(_gl);
    }
    if (_window) {
        SDL_DestroyWindow(_window);
    }
}

std::string Context::version() const
{
    if (!api_loaded()) {
        return "(no context)";
    }
    const GLubyte* text = gl::GetString(GL_VERSION);
    return text ? reinterpret_cast<const char*>(text) : "(unknown)";
}

std::string Context::renderer_name() const
{
    if (!api_loaded()) {
        return "(no context)";
    }
    const GLubyte* text = gl::GetString(GL_RENDERER);
    return text ? reinterpret_cast<const char*>(text) : "(unknown)";
}

void Context::set_viewport()
{
    gl::Viewport(0, 0, _width, _height);
}

void Context::clear(float r, float g, float b)
{
    gl::ClearColor(r, g, b, 1.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Context::present()
{
    SDL_GL_SwapWindow(_window);
}

} // namespace gles2
} // namespace gfx
