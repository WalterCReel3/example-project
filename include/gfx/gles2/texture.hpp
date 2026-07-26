#pragma once

#include <string>

struct SDL_Surface;

namespace gfx
{
namespace gles2
{

//============================================================================
//
// A GL texture, owned
//
// Created once from an SDL_Surface — a loaded image, or the output of
// TTF_RenderUTF8_Blended — and kept. That is the difference from
// gfx::renderer::Context::draw_surface, which creates and destroys an
// SDL_Texture per call: fine for the occasional HUD string it was written for,
// ruinous for anything drawn every frame.
//
// No power-of-two rounding, unlike the 2016 gfx::render_text, which built an
// intermediate power-of-two surface and blitted into it. GLES 2.0 core allows
// non-power-of-two textures provided wrapping is CLAMP_TO_EDGE and no mipmaps
// are requested, which is exactly how these are used. The rounding cost memory
// and made the texture coordinates a fraction of the surface rather than 0..1.
//
//============================================================================
class Texture
{
public:
    // Converts to ABGR8888 if needed and uploads. Throws std::runtime_error if
    // the surface is null or the upload cannot be made. Does not take ownership
    // of the surface: the caller frees it, and may do so immediately
    // afterwards.
    explicit Texture(SDL_Surface* surface);
    ~Texture();

    Texture(Texture&& rh) noexcept;
    Texture& operator=(Texture&& rh) noexcept;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int unit = 0) const;

    int width() const { return _width; }
    int height() const { return _height; }
    unsigned int id() const { return _texture; }

private:
    void release();

    unsigned int _texture;
    int _width;
    int _height;
};

} // namespace gles2
} // namespace gfx
