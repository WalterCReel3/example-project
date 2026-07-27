#pragma once

#include <string>

//============================================================================
//
// A texture that outlives the frame
//
//     const gfx::renderer::Texture sheet(context, surface);
//     const Rect src = {16, 0, 16, 16};
//     const Rect dst = {x, y, 32, 32};
//     context.draw(sheet, &src, &dst);        // scaled 2x
//
// Context::draw_surface() creates an SDL_Texture, blits it and destroys it,
// once per call. That is acceptable for the occasional HUD string it was
// written for and ruinous for anything drawn repeatedly: an atlas would upload
// the whole sheet per sprite, and a tilemap would upload every tile every
// frame.
//
// So this exists to separate the upload from the draw. Together with the source
// rectangle on Context::draw() it is what makes an atlas expressible at all —
// draw_surface can only blit a whole surface at a point, and there is no way to
// say "this 16x16 cell of that sheet" through it.
//
// This is the piece planning/2026-07-25-software-2d-sprites-tiling/ lists as
// blocking everything else in that snapshot: Atlas, AnimatedSprite and TileMap
// are all expressed over it.
//
// Movable, not copyable: it owns a GPU-or-driver-side resource, so a copy would
// have to either duplicate an upload silently or double-free. Moving lets one
// live in a container or be returned from a loader, which is how gfx::Atlas
// will want to hold it.
//
//============================================================================
struct SDL_Renderer;
struct SDL_Surface;
struct SDL_Texture;

namespace gfx
{
namespace renderer
{

class Context;

class Texture
{
public:
    // Uploads `surface`. Does not take ownership of it — the caller still frees
    // it, which matches loaders::load_image returning a surface the caller
    // owns.
    //
    // Throws std::runtime_error if the upload fails. The Context must outlive
    // this: the texture belongs to its renderer.
    Texture(Context& context, SDL_Surface* surface);

    // An empty texture to render into, or to fill from pixels later.
    Texture(Context& context, int width, int height);

    ~Texture();

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    int width() const { return _width; }
    int height() const { return _height; }

    // Null only for a moved-from texture, which is not drawable.
    SDL_Texture* handle() const { return _texture; }

    // Multiplies every pixel by this colour on draw. Free on both drivers, and
    // the cheap way to tint a 1-bit glyph sheet — one upload, any colour,
    // rather than a surface per colour.
    void set_color_mod(unsigned char r, unsigned char g, unsigned char b);
    void set_alpha_mod(unsigned char a);

    // Nearest by default, like Layer, because that is what pixel art wants.
    void set_smooth(bool smooth);

    // Whether the texture blends when drawn. On by default, since the usual
    // case is a sprite or glyph with transparent margins. Turn it off for an
    // opaque full-screen blit, where blending costs real time on the software
    // driver.
    void set_blend(bool blend);

private:
    SDL_Texture* _texture;
    int _width;
    int _height;
};

} // namespace renderer
} // namespace gfx
