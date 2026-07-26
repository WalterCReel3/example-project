#pragma once

// The SDL_Renderer path — the renderer the game runs on, on every target.
//
// This is the baseline, not a fallback, and the driver underneath it is not
// always software. SDL picks a render driver at construction:
//
//     Miyoo Mini      software      the SSD202D has no 3D block at all
//     RK3326, H700    opengles2     Mali, so this is hardware accelerated
//     desktop         opengl        whatever Mesa offers
//
// The same code therefore CPU-blits on the weakest device and runs on the GPU
// on the others, which is why this is named for the abstraction rather than for
// the driver. It used to be called gfx::software, after the driver it happened
// to get on the Miyoo Mini; that name became actively misleading the moment the
// Mali targets started selecting opengles2.
//
// What it draws: textures, atlases, tilemaps, text — the 2D game path. What it
// cannot draw: anything needing a custom shader or a triangle pipeline. That is
// gfx::gles2, which is a separate renderer rather than an alternative
// implementation of this one, and a window is driven by one or the other.
//
// See planning/2026-07-26-gfx-renderer-and-gles2/.

#include <string>
#include <util/nocopy.hpp>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Surface;
struct SDL_Texture;
// Must match SDL_ttf.h exactly — it declares `typedef struct TTF_Font
// TTF_Font;` (the struct tag is TTF_Font, not _TTF_Font as older releases
// used). A mismatched tag here is a hard error in any TU that also includes
// SDL_ttf.h.
typedef struct TTF_Font TTF_Font;

namespace gfx
{
namespace renderer
{

struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct Rect {
    int x;
    int y;
    int w;
    int h;
};

// Which render driver to ask SDL for.
//
// PreferAccelerated is the default rather than Accelerated because a device
// whose GLES blobs are missing or broken should still boot into a playable game
// at a lower frame rate, instead of refusing to start. That is the same call
// audio::Device makes for a missing audio device — see
// docs/TARGETS.md § "No device is not an error".
//
// Accelerated is worth asking for explicitly in a tool that is *checking* the
// device, which is what wreel-probe does: there, silently falling back to
// software would hide the answer being looked for.
enum class Driver {
    PreferAccelerated, // hardware if available, software otherwise
    Accelerated,       // hardware or throw
    Software           // force the software driver, even where a GPU exists
};

// Owns a window and its SDL_Renderer.
class Context
{
public:
    // fullscreen defaults to true: handhelds have no window manager, and
    // SDL_WINDOW_FULLSCREEN_DESKTOP is the sane request there.
    Context(const std::string& title, int width, int height,
            bool fullscreen = true, Driver driver = Driver::PreferAccelerated);
    ~Context();

private:
    DISALLOW_COPY_AND_ASSIGN(Context);

public:
    SDL_Window* window() { return _window; }
    SDL_Renderer* renderer() { return _renderer; }

    int width() const { return _width; }
    int height() const { return _height; }

    // Name of the SDL render driver actually selected: "software",
    // "opengles2", "opengl". This is the only way to find out what a
    // PreferAccelerated request resolved to, so it is worth asserting in a test
    // rather than only logging it.
    std::string driver_name() const;

    // Whether the driver SDL gave us is hardware accelerated. False on the
    // Miyoo Mini, and false anywhere PreferAccelerated fell back.
    bool accelerated() const;

    void clear(const Color& color);
    void present();

    // Blits a surface at rect's origin. rect->w/h are filled in with the
    // surface's dimensions, matching gfx::render_surface's behaviour.
    void draw_surface(SDL_Surface* surface, Rect* rect);

    // Renders UTF-8 text and returns the pixel size consumed. Costly: this
    // rasterises and uploads every call, so cache the result for static
    // strings.
    void draw_text(const std::string& text, TTF_Font* font, const Color& color,
                   Rect* rect);

private:
    SDL_Window* _window;
    SDL_Renderer* _renderer;
    int _width;
    int _height;
};

} // namespace renderer
} // namespace gfx

