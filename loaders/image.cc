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
    util::log_debug("loading image: %s", path.c_str());
    SDL_Surface* original = IMG_Load(path.c_str());
    if (original == NULL) {
        util::log_error("could not load image %s: %s", path.c_str(),
                        IMG_GetError());
        throw runtime_error("Couldn't load image");
    }

    // http://forums.libsdl.org/viewtopic.php?t=9539&sid=cc1329c9a41455bc02c6da5ba6bca34c
    SDL_Surface* image = SDL_ConvertSurfaceFormat(
                             original, SDL_PIXELFORMAT_ABGR8888, 0);
    if (image == NULL) {
        util::log_error("could not convert image %s: %s", path.c_str(),
                        SDL_GetError());
        SDL_FreeSurface(original);
        throw runtime_error("Couldn't convert image");
    }
    SDL_FreeSurface(original);

    const SDL_PixelFormat* fmt = image->format;
    util::log_debug("image loaded: %s, bpp %d, Bpp %d, "
                    "masks R%08x G%08x B%08x A%08x",
                    path.c_str(), static_cast<int>(fmt->BitsPerPixel),
                    static_cast<int>(fmt->BytesPerPixel),
                    static_cast<unsigned int>(fmt->Rmask),
                    static_cast<unsigned int>(fmt->Gmask),
                    static_cast<unsigned int>(fmt->Bmask),
                    static_cast<unsigned int>(fmt->Amask));

    return image;
}

} // namespace loaders
