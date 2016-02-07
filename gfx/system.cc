#include <string>
#include <stdexcept>
#include <algorithm>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <util/deleter.hpp>
#include <util/logging.hpp>
#include <gfx/system.hpp>

namespace gfx
{

using namespace std;
System* System::_instance;

System::System()
{
    util::logging.log() << "Inside System::System()" << endl;
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK) != 0) {
        string message = string(SDL_GetError());
        throw runtime_error(message);
    }

    if (TTF_Init() == -1) {
        throw runtime_error("Could not initialize TTF layer");
    }

    int init_flags = IMG_INIT_JPG | IMG_INIT_PNG;
    if (IMG_Init(init_flags) != init_flags) {
        throw runtime_error("Could not initialize IMG layer");
    }
}

System::~System()
{
    util::logging.log() << "Inside System::~System()" << endl;
    for_each(_contexts.begin(), _contexts.end(), util::deleter<Context>);
    _contexts.clear();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    System::_instance = NULL;
}

Context* System::create_context(const string& name)
{
    Context* context = new Context(name);
    _contexts.push_back(context);
    return context;
}

System* System::get_instance()
{
    if (System::_instance == NULL) {
        System::_instance = new System();
    }
    return _instance;
}

} // namespace gfx

// vim: set sts=2 sw=2 expandtab:
