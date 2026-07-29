#include "glyphs.hpp"

#include <SDL.h>

#include <cmath>
#include <stdexcept>

#include <loaders/image.hpp>
#include <util/ascii.hpp>
#include <util/format.hpp>
#include <util/logging.hpp>

namespace coppers
{

namespace
{

// The sheet is two tones, but not exactly two: it also carries a stray #030303.
// So a glyph pixel is decided by a luminance threshold rather than by equality
// with a key colour, which tolerates that and any other near-black noise a
// re-encode might introduce.
//
// Equality against #000000 would have worked on this file today and broken on
// the next one, in a way that presents as a few speckled pixels rather than as
// an error.
const unsigned int luminance_threshold = 128;

bool is_glyph_pixel(std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
    // Rec. 601 weights in integer arithmetic, kept off the floating-point unit
    // because this runs once per pixel of the sheet at load time on a device
    // with no FPU to spare.
    const unsigned int luma = (299u * r + 587u * g + 114u * b) / 1000u;
    return luma >= luminance_threshold;
}

} // namespace

GlyphSheet::GlyphSheet(const std::string& path, int columns, int rows,
                       int first_char)
    : _mask()
    , _columns(columns)
    , _rows(rows)
    , _cell_w(0)
    , _cell_h(0)
    , _first(first_char)
    , _count(columns * rows)
{
    if (columns <= 0 || rows <= 0) {
        throw std::runtime_error("glyph sheet grid must be positive");
    }

    // load_image throws on failure rather than returning null, so there is no
    // null branch to write here.
    SDL_Surface* image = loaders::load_image(path);

    if (image->w % columns != 0 || image->h % rows != 0) {
        const std::string message = util::format(
            "glyph sheet %s is %dx%d, which does not divide into %dx%d cells",
            path.c_str(), image->w, image->h, columns, rows);
        SDL_FreeSurface(image);
        throw std::runtime_error(message);
    }

    _cell_w = image->w / columns;
    _cell_h = image->h / rows;

    // load_image converts to ABGR8888, so the byte order is known and the
    // channels can be read without consulting the surface's format.
    _mask.assign(static_cast<std::size_t>(_count) *
                     static_cast<std::size_t>(_cell_w * _cell_h),
                 0);

    const std::uint8_t* base = static_cast<const std::uint8_t*>(image->pixels);

    for (int cell = 0; cell < _count; ++cell) {
        const int col = cell % columns;
        const int row = cell / columns;
        const int origin_x = col * _cell_w;
        const int origin_y = row * _cell_h;

        for (int y = 0; y < _cell_h; ++y) {
            const std::uint8_t* src =
                base +
                static_cast<std::size_t>(origin_y + y) *
                    static_cast<std::size_t>(image->pitch) +
                static_cast<std::size_t>(origin_x) * 4;

            std::uint8_t* dst =
                _mask.data() +
                static_cast<std::size_t>(cell) *
                    static_cast<std::size_t>(_cell_w * _cell_h) +
                static_cast<std::size_t>(y * _cell_w);

            for (int x = 0; x < _cell_w; ++x) {
                // ABGR8888 in memory is R, G, B, A byte-wise on a little-endian
                // target. Every target here is little-endian; a big-endian one
                // would need the index order reversed.
                dst[x] = is_glyph_pixel(src[x * 4 + 0], src[x * 4 + 1],
                                        src[x * 4 + 2])
                             ? 1
                             : 0;
            }
        }
    }

    SDL_FreeSurface(image);

    util::log_info("glyphs: %s, %dx%d cells, %d glyphs from '%c' to '%c'",
                   path.c_str(), _cell_w, _cell_h, _count,
                   static_cast<char>(_first),
                   static_cast<char>(_first + _count - 1));
}

std::size_t GlyphSheet::cell_index(char c) const
{
    const int code = static_cast<int>(static_cast<unsigned char>(c));
    if (code < _first || code >= _first + _count) {
        return 0; // space, which is the first cell
    }
    return static_cast<std::size_t>(code - _first);
}

void GlyphSheet::cell_rect(char c, int& x, int& y, int& w, int& h) const
{
    const int index = static_cast<int>(cell_index(c));
    x = (index % _columns) * _cell_w;
    y = (index / _columns) * _cell_h;
    w = _cell_w;
    h = _cell_h;
}

SDL_Surface* GlyphSheet::build_surface() const
{
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, sheet_width(), sheet_height(), 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        throw std::runtime_error(
            std::string("could not build glyph surface: ") + SDL_GetError());
    }

    // White where the mask is set, fully transparent elsewhere. White because
    // the texture is tinted by colour modulation, which multiplies — so the
    // source has to be the identity for that to reproduce any colour.
    SDL_LockSurface(surface);
    std::uint32_t* pixels = static_cast<std::uint32_t*>(surface->pixels);
    const int stride = surface->pitch / static_cast<int>(sizeof(std::uint32_t));

    for (int cell = 0; cell < _count; ++cell) {
        const int origin_x = (cell % _columns) * _cell_w;
        const int origin_y = (cell / _columns) * _cell_h;

        for (int y = 0; y < _cell_h; ++y) {
            const std::uint8_t* src =
                _mask.data() +
                static_cast<std::size_t>(cell) *
                    static_cast<std::size_t>(_cell_w * _cell_h) +
                static_cast<std::size_t>(y * _cell_w);
            std::uint32_t* dst = pixels + (origin_y + y) * stride + origin_x;

            for (int x = 0; x < _cell_w; ++x) {
                dst[x] = src[x] ? 0xFFFFFFFFu : 0x00000000u;
            }
        }
    }

    SDL_UnlockSurface(surface);
    return surface;
}

std::string to_sheet_text(const std::string& text, const GlyphSheet& sheet)
{
    std::string out;
    out.reserve(text.size());

    for (std::size_t i = 0; i < text.size(); ++i) {
        // ascii_to_upper already leaves a non-lowercase character alone, so
        // there is no is_lower test to do here.
        const char upper = util::ascii_to_upper(text[i]);
        const int code = static_cast<int>(static_cast<unsigned char>(upper));
        out += (code >= sheet.first_char() && code <= sheet.last_char()) ? upper
                                                                         : ' ';
    }

    return out;
}

void GlyphSheet::plot(gfx::renderer::LayerLock& pixels, const std::string& text,
                      double x, int y, int scale, std::uint32_t color) const
{
    if (scale < 1) {
        scale = 1;
    }

    const int step = _cell_w * scale;
    const int layer_w = pixels.width();
    const int layer_h = pixels.height();

    for (std::size_t i = 0; i < text.size(); ++i) {
        const int origin_x = static_cast<int>(
            std::floor(x + static_cast<double>(i) * static_cast<double>(step)));

        if (origin_x + step <= 0) {
            continue;
        }
        if (origin_x >= layer_w) {
            break;
        }
        if (text[i] == ' ') {
            continue;
        }

        for (int gy = 0; gy < _cell_h; ++gy) {
            for (int sy = 0; sy < scale; ++sy) {
                const int py = y + gy * scale + sy;
                // Clipped per output row rather than per pixel: the row pointer
                // is only valid inside the layer, and hoisting the test out of
                // the inner loop is most of the difference between this being
                // competitive with a driver blit and not.
                if (py < 0 || py >= layer_h) {
                    continue;
                }

                std::uint32_t* row = pixels.row(py);

                for (int gx = 0; gx < _cell_w; ++gx) {
                    if (!pixel(text[i], gx, gy)) {
                        continue;
                    }
                    const int base = origin_x + gx * scale;
                    for (int sx = 0; sx < scale; ++sx) {
                        const int px = base + sx;
                        if (px < 0 || px >= layer_w) {
                            continue;
                        }
                        // Opaque store, not a blend. The sheet is 1-bit, so
                        // there is no partial coverage to composite and a
                        // read-modify-write per pixel would be paid for
                        // nothing.
                        row[px] = color;
                    }
                }
            }
        }
    }
}

} // namespace coppers
