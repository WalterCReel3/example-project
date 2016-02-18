#include <util/logging.hpp>
#include <loaders/image.hpp>
#include <cstdio>
#include <string>
#include <stdexcept>
#include <SDL.h>
#include <SDL_image.h>

namespace loaders
{

SDL_Surface* load_image(const std::string& path)
{
    using namespace std;
    SDL_Surface* original = IMG_Load(path.c_str());
    util::logging.debug() << "Loading image: " << path << endl;
    if (original == NULL) {
        util::logging.error() << "ERROR: Couldn't load image ";
        throw runtime_error("Couldn't load image");
    }

    // http://forums.libsdl.org/viewtopic.php?t=9539&sid=cc1329c9a41455bc02c6da5ba6bca34c
    SDL_Surface* image = SDL_ConvertSurfaceFormat(
                             original, SDL_PIXELFORMAT_ABGR8888, 0);
    if (image == NULL) {
        util::logging.error() << "ERROR: Couldn't convert image ";
        throw runtime_error("Couldn't convert image");
    }
    SDL_FreeSurface(original);

    SDL_PixelFormat *fmt = image->format;
    util::logging.debug()
            << "Image Loaded and converted:"
            << "\n  bpp:   " << (int)fmt->BitsPerPixel
            << "\n  Bpp:   " << (int)fmt->BytesPerPixel
            << "\n  Rmask: " << std::hex << (unsigned int)fmt->Rmask
            << "\n  Gmask: " << std::hex << (unsigned int)fmt->Gmask
            << "\n  Bmask: " << std::hex << (unsigned int)fmt->Bmask
            << "\n  Amask: " << std::hex << (unsigned int)fmt->Amask
            << endl;

    return image;
}

} // namespace loaders
