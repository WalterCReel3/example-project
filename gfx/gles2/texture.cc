#include <gfx/gles2/texture.hpp>

#include <gfx/gles2/api.hpp>

#include <util/format.hpp>

#include <SDL.h>

#include <stdexcept>

namespace gfx
{
namespace gles2
{

Texture::Texture(SDL_Surface* surface)
    : _texture(0)
    , _width(0)
    , _height(0)
{
    if (!surface) {
        throw std::runtime_error("Texture: null surface");
    }

    // GL_RGBA with GL_UNSIGNED_BYTE wants bytes in R,G,B,A order, which is
    // SDL_PIXELFORMAT_ABGR8888 on a little-endian machine — the same format
    // loaders::load_image already converts to. Converting unconditionally would
    // copy a whole image for nothing, so this only converts when it has to.
    SDL_Surface* source = surface;
    SDL_Surface* converted = nullptr;
    if (surface->format->format != SDL_PIXELFORMAT_ABGR8888) {
        converted =
            SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
        if (!converted) {
            throw std::runtime_error(
                std::string("Texture: could not convert surface: ") +
                SDL_GetError());
        }
        source = converted;
    }

    gl::GenTextures(1, &_texture);
    if (_texture == 0) {
        if (converted) {
            SDL_FreeSurface(converted);
        }
        throw std::runtime_error("Texture: glGenTextures produced no name");
    }

    gl::BindTexture(GL_TEXTURE_2D, _texture);

    // CLAMP_TO_EDGE and no mipmaps are what make a non-power-of-two texture
    // legal in GLES 2.0 core. Changing either without also rounding the size up
    // would produce a black texture on a conformant driver and work on Mesa,
    // which is the worst combination to debug.
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // SDL surfaces can be padded to a row alignment that is not 4 bytes, and GL
    // defaults to assuming 4. Setting it from the surface avoids the diagonal
    // skew that a mismatch produces.
    SDL_LockSurface(source);
    gl::PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, source->w, source->h, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, source->pixels);
    SDL_UnlockSurface(source);

    gl::BindTexture(GL_TEXTURE_2D, 0);

    _width = source->w;
    _height = source->h;

    if (converted) {
        SDL_FreeSurface(converted);
    }

    const GLenum error = gl::GetError();
    if (error != GL_NO_ERROR) {
        release();
        throw std::runtime_error(
            util::format("Texture: upload of %dx%d failed with GL error 0x%x",
                         _width, _height, static_cast<unsigned>(error)));
    }
}

Texture::~Texture()
{
    release();
}

void Texture::release()
{
    if (_texture != 0) {
        gl::DeleteTextures(1, &_texture);
        _texture = 0;
    }
}

Texture::Texture(Texture&& rh) noexcept
    : _texture(rh._texture)
    , _width(rh._width)
    , _height(rh._height)
{
    rh._texture = 0;
    rh._width = 0;
    rh._height = 0;
}

Texture& Texture::operator=(Texture&& rh) noexcept
{
    if (this != &rh) {
        release();
        _texture = rh._texture;
        _width = rh._width;
        _height = rh._height;
        rh._texture = 0;
        rh._width = 0;
        rh._height = 0;
    }
    return *this;
}

void Texture::bind(int unit) const
{
    gl::ActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
    gl::BindTexture(GL_TEXTURE_2D, _texture);
}

} // namespace gles2
} // namespace gfx
