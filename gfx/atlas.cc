#include <gfx/atlas.hpp>

#include <SDL.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>

#include <util/algorithm.hpp>

namespace gfx
{

namespace
{

// Uploads `surface` with each row reversed. The copy keeps the surface's own
// format, palette, colour key and blend mode, so the texture this produces
// differs from a plain upload in the order of its columns and in nothing else.
renderer::Texture upload_mirrored(renderer::Context& context,
                                  SDL_Surface* surface)
{
    const std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> copy(
        SDL_DuplicateSurface(surface), SDL_FreeSurface);
    if (!copy) {
        throw std::runtime_error(std::string("could not copy sheet: ") +
                                 SDL_GetError());
    }

    // Below eight bits a byte holds several pixels, and reversing bytes would
    // reverse the pixels in groups rather than one by one.
    if (copy->format->BitsPerPixel < 8) {
        throw std::runtime_error(
            "cannot mirror a sheet of less than 8 bits per pixel");
    }

    if (SDL_LockSurface(copy.get()) != 0) {
        throw std::runtime_error(std::string("could not lock sheet: ") +
                                 SDL_GetError());
    }

    const int pixel = copy->format->BytesPerPixel;
    Uint8* row = static_cast<Uint8*>(copy->pixels);
    for (int y = 0; y < copy->h; ++y, row += copy->pitch) {
        Uint8* left = row;
        Uint8* right = row + (copy->w - 1) * pixel;
        for (; left < right; left += pixel, right -= pixel) {
            std::swap_ranges(left, left + pixel, right);
        }
    }

    SDL_UnlockSurface(copy.get());

    return renderer::Texture(context, copy.get());
}

} // namespace

Atlas::Atlas(renderer::Texture sheet, Frames frames)
    : _sheet(std::move(sheet))
    , _frames(std::move(frames))
{
}

Atlas::Atlas(renderer::Context& context, SDL_Surface* sheet, Frames frames)
    : _sheet(context, sheet)
    , _mirrored(upload_mirrored(context, sheet))
    , _frames(std::move(frames))
{
}

Atlas::Index Atlas::find(const std::string& id) const
{
    return util::index_of(
        _frames, [&id](const AtlasFrame& frame) { return frame.id == id; });
}

void Atlas::draw(renderer::Context& context, Index index, int x, int y,
                 int scale, Mirror mirror) const
{
    if (scale < 1) {
        return;
    }

    const AtlasFrame& frame = _frames[index];
    renderer::Rect dst = {x + frame.trim_x * scale, y + frame.trim_y * scale,
                          frame.source.w * scale, frame.source.h * scale};

    if (mirror == Mirror::None) {
        context.draw(_sheet, &frame.source, &dst);
        return;
    }

    if (!_mirrored) {
        throw std::logic_error(
            "mirrored draw from an atlas built without a mirrored sheet");
    }

    // The same reflection twice: the region's columns within the sheet, and
    // the region's place within its untrimmed box. width() is the region's own
    // width for an untrimmed frame, which leaves dst.x at x.
    const renderer::Rect source = {
        _sheet.width() - (frame.source.x + frame.source.w), frame.source.y,
        frame.source.w, frame.source.h};
    dst.x = x + (frame.width() - frame.trim_x - frame.source.w) * scale;

    context.draw(*_mirrored, &source, &dst);
}

bool Atlas::contains_frames() const
{
    const renderer::Rect sheet =
        renderer::bounds_of(_sheet.width(), _sheet.height());

    return std::all_of(_frames.begin(), _frames.end(),
                       [&sheet](const AtlasFrame& frame) {
                           return renderer::contains(sheet, frame.source);
                       });
}

} // namespace gfx
