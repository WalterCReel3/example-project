#include <gfx/renderer/texture.hpp>

#include <SDL.h>

#include <stdexcept>
#include <utility>

#include <gfx/renderer/context.hpp>

namespace gfx
{
namespace renderer
{

Texture::Texture(Context& context, SDL_Surface* surface)
    : _texture(nullptr)
    , _width(0)
    , _height(0)
{
    if (!surface) {
        throw std::runtime_error("cannot create a texture from a null surface");
    }

    _texture = SDL_CreateTextureFromSurface(context.renderer(), surface);
    if (!_texture) {
        throw std::runtime_error(std::string("could not upload texture: ") +
                                 SDL_GetError());
    }

    _width = surface->w;
    _height = surface->h;
    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);
}

Texture::Texture(Context& context, int width, int height)
    : _texture(nullptr)
    , _width(width)
    , _height(height)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("texture dimensions must be positive");
    }

    _texture = SDL_CreateTexture(context.renderer(), SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STATIC, width, height);
    if (!_texture) {
        throw std::runtime_error(std::string("could not create texture: ") +
                                 SDL_GetError());
    }

    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);
}

Texture::~Texture()
{
    if (_texture) {
        SDL_DestroyTexture(_texture);
    }
}

Texture::Texture(Texture&& other) noexcept
    : _texture(other._texture)
    , _width(other._width)
    , _height(other._height)
{
    other._texture = nullptr;
    other._width = 0;
    other._height = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) {
        if (_texture) {
            SDL_DestroyTexture(_texture);
        }
        _texture = other._texture;
        _width = other._width;
        _height = other._height;
        other._texture = nullptr;
        other._width = 0;
        other._height = 0;
    }
    return *this;
}

void Texture::set_color_mod(unsigned char r, unsigned char g, unsigned char b)
{
    if (_texture) {
        SDL_SetTextureColorMod(_texture, r, g, b);
    }
}

void Texture::set_alpha_mod(unsigned char a)
{
    if (_texture) {
        SDL_SetTextureAlphaMod(_texture, a);
    }
}

void Texture::set_smooth(bool smooth)
{
    if (_texture) {
        SDL_SetTextureScaleMode(_texture, smooth ? SDL_ScaleModeLinear
                                                 : SDL_ScaleModeNearest);
    }
}

void Texture::set_blend(bool blend)
{
    if (_texture) {
        SDL_SetTextureBlendMode(_texture, blend ? SDL_BLENDMODE_BLEND
                                                : SDL_BLENDMODE_NONE);
    }
}

} // namespace renderer
} // namespace gfx
