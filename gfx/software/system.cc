#include <gfx/software/system.hpp>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <stdexcept>
#include <string>
#include <util/logging.hpp>

namespace gfx
{
namespace software
{

System::System()
    : _contexts()
{
    // Joystick is required, not optional: handhelds have no keyboard, so
    // failing to init it leaves the device with no input at all.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        throw std::runtime_error(std::string("SDL_Init failed: ")
                                 + SDL_GetError());
    }

    if (TTF_Init() != 0) {
        SDL_Quit();
        throw std::runtime_error(std::string("TTF_Init failed: ")
                                 + TTF_GetError());
    }

    const int want = IMG_INIT_PNG | IMG_INIT_JPG;
    const int got = IMG_Init(want);
    if ((got & want) != want) {
        // Not fatal on its own — report which codec is missing and continue,
        // since a build may legitimately ship only one asset format.
        util::logging.warning()
            << "IMG_Init: requested PNG|JPG, got "
            << ((got & IMG_INIT_PNG) ? "PNG " : "")
            << ((got & IMG_INIT_JPG) ? "JPG " : "")
            << "(" << IMG_GetError() << ")" << std::endl;
    }

    util::logging.info() << "software system initialised, video driver "
                         << (SDL_GetCurrentVideoDriver()
                                 ? SDL_GetCurrentVideoDriver()
                                 : "(none)")
                         << std::endl;
}

System::~System()
{
    for (Context* context : _contexts) {
        delete context;
    }
    _contexts.clear();

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

Context* System::create_context(const std::string& title, int width, int height,
                                bool fullscreen)
{
    Context* context = new Context(title, width, height, fullscreen);
    _contexts.push_back(context);
    return context;
}

} // namespace software
} // namespace gfx
