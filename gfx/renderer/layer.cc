#include <gfx/renderer/layer.hpp>

#include <SDL.h>

#include <stdexcept>
#include <string>
#include <utility>

#include <gfx/renderer/context.hpp>
#include <util/logging.hpp>

namespace gfx
{
namespace renderer
{

LayerLock::LayerLock(SDL_Texture* texture, std::uint32_t* pixels, int stride,
                     int width, int height, bool upload)
    : _texture(texture)
    , _pixels(pixels)
    , _stride(stride)
    , _width(width)
    , _height(height)
    , _upload(upload)
{
}

LayerLock::LayerLock(LayerLock&& other) noexcept
    : _texture(other._texture)
    , _pixels(other._pixels)
    , _stride(other._stride)
    , _width(other._width)
    , _height(other._height)
    , _upload(other._upload)
{
    // The moved-from guard must not unlock in its destructor, or the texture is
    // unlocked while this one still holds a pointer into it.
    other._texture = nullptr;
    other._pixels = nullptr;
}

LayerLock& LayerLock::operator=(LayerLock&& other) noexcept
{
    if (this != &other) {
        if (_texture) {
            SDL_UnlockTexture(_texture);
        }
        _texture = other._texture;
        _pixels = other._pixels;
        _stride = other._stride;
        _width = other._width;
        _height = other._height;
        _upload = other._upload;
        other._texture = nullptr;
        other._pixels = nullptr;
    }
    return *this;
}

LayerLock::~LayerLock()
{
    if (!_texture) {
        return;
    }

    if (_upload) {
        // The pixels are the Layer's own buffer rather than SDL's staging
        // memory, so this is the point at which the frame reaches the texture.
        SDL_UpdateTexture(_texture, nullptr, _pixels,
                          _stride * static_cast<int>(sizeof(std::uint32_t)));
    } else {
        SDL_UnlockTexture(_texture);
    }
}

Layer::Layer(Context& context, int width, int height)
    : _renderer(context.renderer())
    , _texture(nullptr)
    , _width(width)
    , _height(height)
    , _readback(false)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("layer dimensions must be positive");
    }

    // STREAMING, not STATIC: this is written every frame, and SDL's static path
    // is optimised for the opposite case. ARGB8888 because it is the format the
    // widest set of drivers takes without an internal conversion, and because a
    // packed 32-bit pixel is one store per pixel in the plotting loop.
    _texture = SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!_texture) {
        throw std::runtime_error(
            std::string("could not create layer texture: ") + SDL_GetError());
    }

    // Nearest by default. A 2x integer upscale with linear filtering looks
    // soft-focused rather than pixelated, which is the wrong answer for
    // everything this will be used for.
    SDL_SetTextureScaleMode(_texture, SDL_ScaleModeNearest);

    // Opaque background layer: blending every pixel of a full-screen blit costs
    // real time on the software driver and buys nothing when the layer covers
    // the target. Effects wanting to composite over something can change it.
    SDL_SetTextureBlendMode(_texture, SDL_BLENDMODE_NONE);
}

Layer::~Layer()
{
    if (_texture) {
        SDL_DestroyTexture(_texture);
    }
}

LayerLock Layer::lock()
{
    // Readback mode hands out the Layer's own buffer and uploads it on unlock.
    // No SDL_LockTexture at all: what the caller plots stays readable, which is
    // the whole point, and a locked texture's contents are not.
    if (_readback) {
        return LayerLock(_texture, _pixels.data(), _width, _width, _height,
                         true);
    }

    void* pixels = nullptr;
    int pitch = 0;

    if (SDL_LockTexture(_texture, nullptr, &pixels, &pitch) != 0) {
        util::log_error("could not lock layer: %s", SDL_GetError());
        return LayerLock(nullptr, nullptr, 0, _width, _height, false);
    }

    // SDL reports the pitch in bytes; the guard hands out std::uint32_t*, so it
    // is converted once here rather than at every row() call. Exact for a
    // 32-bpp format, whose pitch is always a whole number of pixels.
    const int stride = pitch / static_cast<int>(sizeof(std::uint32_t));

    return LayerLock(_texture, static_cast<std::uint32_t*>(pixels), stride,
                     _width, _height, false);
}

void Layer::set_readback(bool enabled)
{
    if (enabled == _readback) {
        return;
    }

    _readback = enabled;

    if (enabled) {
        _pixels.assign(static_cast<std::size_t>(_width) *
                           static_cast<std::size_t>(_height),
                       0);
    } else {
        // swap with an empty vector rather than clear(): clear() keeps the
        // capacity, and this is 1.2 MB on a device with 128 MB total.
        std::vector<std::uint32_t>().swap(_pixels);
    }
}

bool Layer::save_bmp(const std::string& path) const
{
    if (!_readback || _pixels.empty()) {
        util::log_error(
            "layer: save_bmp needs set_readback(true); a locked texture cannot "
            "be read back");
        return false;
    }

    // Const-correct at our end: SDL's surface-from-pixels takes a non-const
    // pointer because a surface is writable in general, but SDL_SaveBMP only
    // reads.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<std::uint32_t*>(_pixels.data()), _width, _height, 32,
        _width * static_cast<int>(sizeof(std::uint32_t)),
        SDL_PIXELFORMAT_ARGB8888);

    if (!surface) {
        util::log_error("layer: could not wrap pixels for %s: %s", path.c_str(),
                        SDL_GetError());
        return false;
    }

    const bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
    if (!ok) {
        util::log_error("layer: could not write %s: %s", path.c_str(),
                        SDL_GetError());
    } else {
        util::log_info("layer: wrote %s (%dx%d)", path.c_str(), _width,
                       _height);
    }

    SDL_FreeSurface(surface);
    return ok;
}

void Layer::draw(const Rect* dst)
{
    if (!dst) {
        SDL_RenderCopy(_renderer, _texture, nullptr, nullptr);
        return;
    }

    SDL_Rect target;
    target.x = dst->x;
    target.y = dst->y;
    target.w = dst->w;
    target.h = dst->h;
    SDL_RenderCopy(_renderer, _texture, nullptr, &target);
}

void Layer::set_smooth(bool smooth)
{
    SDL_SetTextureScaleMode(_texture, smooth ? SDL_ScaleModeLinear
                                             : SDL_ScaleModeNearest);
}

} // namespace renderer
} // namespace gfx
