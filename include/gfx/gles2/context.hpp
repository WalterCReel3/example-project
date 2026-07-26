#pragma once

#include <string>

#include <util/nocopy.hpp>

struct SDL_Window;
typedef void* SDL_GLContext;

//============================================================================
//
// A GLES 2.0 window and context
//
// The other renderer. gfx::renderer draws the game through SDL_Renderer; this
// owns a GL context outright, which is what makes custom shaders and a triangle
// pipeline possible. A window is driven by one or the other — SDL_Renderer
// manages its window's context internally and mixing the two is not worth the
// state-restoration discipline.
//
// GLES 2.0 rather than desktop GL, for both targets at once: GLES 2.0 is a near
// subset of desktop GL 2.1+, desktop Mesa exposes an ES profile directly, and
// the Mali handhelds expose nothing else reliably. One shader dialect — GLSL
// ES 1.00 — therefore serves the dev box and the devices, which is why this was
// chosen over a gl33 core renderer that would run on nothing being targeted
// except Steam.
//
// Never built for the Miyoo Mini: the SSD202D has no 3D block, so
// WREEL_ENABLE_GLES2 is rejected there at configure time.
//
//============================================================================
namespace gfx
{
namespace gles2
{

class Context
{
public:
    // fullscreen defaults to true for the same reason gfx::renderer's does:
    // handhelds have no window manager, and display takeover is the intended
    // presentation rather than a misfeature.
    Context(const std::string& title, int width, int height,
            bool fullscreen = true);
    ~Context();

private:
    DISALLOW_COPY_AND_ASSIGN(Context);

public:
    SDL_Window* window() { return _window; }

    // Drawable size in pixels, which is not the window size under a scaling
    // compositor or a HiDPI display. This is what the viewport and any
    // projection aspect ratio must be built from.
    int width() const { return _width; }
    int height() const { return _height; }

    // GL_VERSION and GL_RENDERER as the driver reports them. On a handheld this
    // is how you tell a real Mali blob from Mesa's software rasteriser, which
    // is a distinction that otherwise only shows up as a frame rate.
    std::string version() const;
    std::string renderer_name() const;

    // Whether the context actually came back as an ES profile. SDL will happily
    // give a compatibility-profile desktop context when an ES one was requested
    // and the driver prefers it; shaders written to GLSL ES 1.00 mostly still
    // work there, so this is reported rather than treated as fatal.
    bool is_es_profile() const { return _es_profile; }

    void set_viewport();
    void clear(float r, float g, float b);
    void present();

    // Writes the current back buffer to a BMP. Returns false and logs on
    // failure.
    //
    // This is the only way to find out whether this renderer actually draws
    // anything: there is no headless GL, so gfx::gles2 has no unit tests, and
    // "the process did not crash" is not evidence that geometry appeared. It is
    // also how a handheld can be checked over SSH, where nobody is looking at
    // the panel — see planning/2026-07-25-target-validation/ step 4.
    //
    // Call it after drawing and before present(): the back buffer is undefined
    // after a swap.
    bool save_screenshot(const std::string& path) const;

private:
    SDL_Window* _window;
    SDL_GLContext _gl;
    int _width;
    int _height;
    bool _es_profile;
};

} // namespace gles2
} // namespace gfx
