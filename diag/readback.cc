// Getting the frame back off the device.
//
// Two paths, tried in order, because the interesting target implements neither
// reliably:
//
//   SDL_RenderReadPixels  correct wherever it exists. The Miyoo Mini's backend
//                         returns SDL_Unsupported from it, which is honest.
//   /dev/fb0              what the panel is actually showing. Slower, uglier,
//                         and the only ground truth on that device — it sees
//                         through the driver rather than asking it.
//
// The framebuffer path reads the *visible* buffer, which is not necessarily the
// one just drawn: the mini driver double-buffers by panning yoffset, so the
// current offset has to be read back from the device rather than assumed.

#include "diag.hpp"

#include <SDL.h>

#include <cstring>
#include <string>

#include <util/format.hpp>

#if defined(__linux__)
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace diag
{

std::uint32_t Image::at(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return 0;
    }
    return pixels[static_cast<std::size_t>(y) * width + x];
}

bool Image::rgb_near(int x, int y, std::uint32_t rgb, int tolerance) const
{
    const std::uint32_t got = at(x, y);
    const int dr = static_cast<int>((got >> 16) & 0xff) -
                   static_cast<int>((rgb >> 16) & 0xff);
    const int dg = static_cast<int>((got >> 8) & 0xff) -
                   static_cast<int>((rgb >> 8) & 0xff);
    const int db =
        static_cast<int>(got & 0xff) - static_cast<int>(rgb & 0xff);

    const int adr = dr < 0 ? -dr : dr;
    const int adg = dg < 0 ? -dg : dg;
    const int adb = db < 0 ? -db : db;

    return adr <= tolerance && adg <= tolerance && adb <= tolerance;
}

const char* readback_name(Readback source)
{
    switch (source) {
    case Readback::None:
        return "none";
    case Readback::RenderReadPixels:
        return "SDL_RenderReadPixels";
    case Readback::Framebuffer:
        return "/dev/fb0";
    }
    return "?";
}

namespace
{

Image read_via_sdl(SDL_Renderer* renderer)
{
    Image image;

    int width = 0;
    int height = 0;
    if (SDL_GetRendererOutputSize(renderer, &width, &height) != 0) {
        return image;
    }
    // The Miyoo Mini's backend answers 0x0 and reports success. Treat a
    // degenerate size as "no readback here" rather than allocating nothing and
    // calling it a frame.
    if (width <= 0 || height <= 0) {
        return image;
    }

    std::vector<std::uint32_t> buffer(
        static_cast<std::size_t>(width) * height, 0);
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                             buffer.data(),
                             width * static_cast<int>(sizeof(std::uint32_t))) !=
        0) {
        return image;
    }

    image.width = width;
    image.height = height;
    image.pixels = std::move(buffer);
    return image;
}

#if defined(__linux__)

// One mapping kept open for the life of the run. Repeatedly mmapping a
// framebuffer while the driver is panning it is a good way to get a torn or
// stale frame, and the checks call this many times.
struct FbMap
{
    int fd = -1;
    unsigned char* base = nullptr;
    std::size_t length = 0;
    fb_fix_screeninfo finfo {};

    ~FbMap()
    {
        if (base && base != MAP_FAILED) {
            munmap(base, length);
        }
        if (fd >= 0) {
            close(fd);
        }
    }

    bool open_once()
    {
        if (fd >= 0) {
            return base != nullptr && base != MAP_FAILED;
        }

        fd = ::open("/dev/fb0", O_RDONLY);
        if (fd < 0) {
            return false;
        }
        if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) != 0) {
            return false;
        }

        length = finfo.smem_len;
        base = static_cast<unsigned char*>(
            mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0));
        return base != MAP_FAILED;
    }
};

FbMap& fb_map()
{
    static FbMap map;
    return map;
}

std::uint32_t unpack(const unsigned char* pixel, const fb_var_screeninfo& v)
{
    if (v.bits_per_pixel == 32) {
        std::uint32_t raw = 0;
        std::memcpy(&raw, pixel, sizeof(raw));
        const auto channel = [&](const fb_bitfield& field) -> std::uint32_t {
            std::uint32_t value = (raw >> field.offset) &
                                  ((1u << field.length) - 1u);
            // Normalise to 8 bits so a 10:10:10 framebuffer would still
            // compare against an 8-bit expectation.
            if (field.length < 8) {
                value <<= (8 - field.length);
            } else if (field.length > 8) {
                value >>= (field.length - 8);
            }
            return value;
        };
        return 0xff000000u | (channel(v.red) << 16) | (channel(v.green) << 8) |
               channel(v.blue);
    }

    if (v.bits_per_pixel == 16) {
        std::uint16_t raw = 0;
        std::memcpy(&raw, pixel, sizeof(raw));
        const std::uint32_t r = (raw >> 11) & 0x1f;
        const std::uint32_t g = (raw >> 5) & 0x3f;
        const std::uint32_t b = raw & 0x1f;
        // 5/6-bit to 8-bit with the high bits replicated into the low ones,
        // so 0x1f becomes 0xff rather than 0xf8 and an exact-white comparison
        // still holds.
        return 0xff000000u | (((r << 3) | (r >> 2)) << 16) |
               (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
    }

    return 0;
}

Image read_via_framebuffer()
{
    Image image;

    FbMap& map = fb_map();
    if (!map.open_once()) {
        return image;
    }

    // Re-read every time: yoffset is what the driver panned to on the last
    // present, and it is the whole reason this is not just "read offset 0".
    fb_var_screeninfo vinfo {};
    if (ioctl(map.fd, FBIOGET_VSCREENINFO, &vinfo) != 0) {
        return image;
    }
    if (vinfo.xres == 0 || vinfo.yres == 0) {
        return image;
    }

    const std::size_t bytes_per_pixel = vinfo.bits_per_pixel / 8u;
    if (bytes_per_pixel != 2 && bytes_per_pixel != 4) {
        return image;
    }

    const std::size_t stride = map.finfo.line_length;
    const std::size_t origin = static_cast<std::size_t>(vinfo.yoffset) * stride +
                               static_cast<std::size_t>(vinfo.xoffset) *
                                   bytes_per_pixel;

    const std::size_t needed =
        origin + static_cast<std::size_t>(vinfo.yres) * stride;
    if (needed > map.length) {
        return image;
    }

    image.width = static_cast<int>(vinfo.xres);
    image.height = static_cast<int>(vinfo.yres);
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);

    for (int y = 0; y < image.height; ++y) {
        const unsigned char* row = map.base + origin + y * stride;
        for (int x = 0; x < image.width; ++x) {
            image.pixels[static_cast<std::size_t>(y) * image.width + x] =
                unpack(row + x * bytes_per_pixel, vinfo);
        }
    }
    return image;
}

#else

Image read_via_framebuffer()
{
    return Image();
}

#endif // __linux__

} // namespace

Image read_frame(SDL_Renderer* renderer, Readback* source)
{
    Image image = read_via_sdl(renderer);
    if (image.valid()) {
        if (source) {
            *source = Readback::RenderReadPixels;
        }
        return image;
    }

    image = read_via_framebuffer();
    if (image.valid()) {
        if (source) {
            *source = Readback::Framebuffer;
        }
        return image;
    }

    if (source) {
        *source = Readback::None;
    }
    return Image();
}

bool framebuffer_size(int* width, int* height)
{
#if defined(__linux__)
    const int fd = ::open("/dev/fb0", O_RDONLY);
    if (fd < 0) {
        return false;
    }

    fb_var_screeninfo vinfo {};
    const bool ok = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0;
    close(fd);

    if (!ok || vinfo.xres == 0 || vinfo.yres == 0) {
        return false;
    }

    // xres/yres, not the *_virtual pair: the virtual height is twice the panel
    // on this device because the driver double-buffers by panning, and a window
    // sized from that would be twice the screen.
    *width = static_cast<int>(vinfo.xres);
    *height = static_cast<int>(vinfo.yres);
    return true;
#else
    (void)width;
    (void)height;
    return false;
#endif
}

bool framebuffer_probe(std::string* description)
{
#if defined(__linux__)
    const int fd = ::open("/dev/fb0", O_RDONLY);
    if (fd < 0) {
        return false;
    }

    fb_var_screeninfo vinfo {};
    fb_fix_screeninfo finfo {};
    const bool ok = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0 &&
                    ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == 0;
    close(fd);

    if (!ok) {
        return false;
    }

    if (description) {
        *description = util::format(
            "%ux%u virtual %ux%u, %u bpp, stride %u, pan (%u,%u), %s",
            vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual,
            vinfo.bits_per_pixel, finfo.line_length, vinfo.xoffset,
            vinfo.yoffset, finfo.id);
    }
    return true;
#else
    (void)description;
    return false;
#endif
}

} // namespace diag
