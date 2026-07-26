#include <gfx/system.hpp>

#include <util/logging.hpp>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <stdexcept>
#include <string>

namespace gfx
{

System::System()
{
    // Joystick is required, not optional: handhelds have no keyboard, so
    // failing to init it leaves the device with no input at all.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ") +
                                 SDL_GetError());
    }

    if (TTF_Init() != 0) {
        SDL_Quit();
        throw std::runtime_error(std::string("TTF_Init failed: ") +
                                 TTF_GetError());
    }

    const int want = IMG_INIT_PNG | IMG_INIT_JPG;
    const int got = IMG_Init(want);
    if ((got & want) != want) {
        // Not fatal on its own — report which codec is missing and continue.
        util::log_warning("IMG_Init: requested PNG|JPG, got %s%s(%s)",
                          (got & IMG_INIT_PNG) ? "PNG " : "",
                          (got & IMG_INIT_JPG) ? "JPG " : "", IMG_GetError());
    }

    const char* driver = SDL_GetCurrentVideoDriver();
    util::log_info("gfx system initialised, video driver %s",
                   driver ? driver : "(none)");
}

System::~System()
{
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

} // namespace gfx
