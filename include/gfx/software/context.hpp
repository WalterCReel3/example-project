#ifndef WREEL_GFX_SOFTWARE_CONTEXT_HPP
#define WREEL_GFX_SOFTWARE_CONTEXT_HPP

// Software rendering backend — SDL_Renderer, CPU blitting, no GPU.
//
// This is the baseline backend, not a fallback. The Miyoo Mini's SigmaStar
// SSD202D has no 3D block at all, so this is the only thing that can run there,
// which means it has to be good enough to be the common path everywhere.
//
// Kept in namespace gfx::software rather than replacing gfx::Context so the
// legacy fixed-function backend can coexist during the port. The two converge
// behind one interface once gl_legacy is retired.

#include <string>
#include <util/nocopy.hpp>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Surface;
struct SDL_Texture;
// Must match SDL_ttf.h exactly — it declares `typedef struct TTF_Font TTF_Font;`
// (the struct tag is TTF_Font, not _TTF_Font as older releases used). A
// mismatched tag here is a hard error in any TU that also includes SDL_ttf.h.
typedef struct TTF_Font TTF_Font;

namespace gfx
{
namespace software
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

// Owns a window and its software renderer.
class Context
{
public:
    // fullscreen defaults to true: handhelds have no window manager, and
    // SDL_WINDOW_FULLSCREEN_DESKTOP is the sane request there.
    Context(const std::string& title, int width, int height,
            bool fullscreen = true);
    ~Context();

private:
    DISALLOW_COPY_AND_ASSIGN(Context);

public:
    SDL_Window* window() { return _window; }
    SDL_Renderer* renderer() { return _renderer; }

    int width() const { return _width; }
    int height() const { return _height; }

    // Name of the SDL render driver actually selected, e.g. "software".
    std::string driver_name() const;

    void clear(const Color& color);
    void present();

    // Blits a surface at rect's origin. rect->w/h are filled in with the
    // surface's dimensions, matching gfx::render_surface's behaviour.
    void draw_surface(SDL_Surface* surface, Rect* rect);

    // Renders UTF-8 text and returns the pixel size consumed. Costly: this
    // rasterises and uploads every call, so cache the result for static strings.
    void draw_text(const std::string& text, TTF_Font* font, const Color& color,
                   Rect* rect);

private:
    SDL_Window* _window;
    SDL_Renderer* _renderer;
    int _width;
    int _height;
};

} // namespace software
} // namespace gfx

#endif
