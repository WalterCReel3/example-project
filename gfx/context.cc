#include <stdexcept>
#include <GL/glew.h>
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <gfx/utils.hpp>
#include <gfx/context.hpp>

namespace gfx
{

using namespace std;

Context::Context(const std::string& title, bool fullscreen)
    : _context()
    , _sdl_window()
    , _window_width()
    , _window_height()
{
    Uint32 flags = SDL_WINDOW_OPENGL;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
        // NOTE: while full screen we will also set
        // relative mouse motion
        SDL_ShowCursor(SDL_DISABLE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
    }

    SDL_DisplayMode dm;
    SDL_GetDesktopDisplayMode(0, &dm);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    _sdl_window = SDL_CreateWindow(
                      title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                      dm.w, dm.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (_sdl_window == NULL) {
        throw std::runtime_error("Couldn't get window");
    }

    _context = SDL_GL_CreateContext(_sdl_window);
    if (_context == NULL) {
        throw std::runtime_error("Couldn't create context");
    }

    GLenum err = glewInit();
    if (GLEW_OK != err) {
        std::string error_string = (const char*)glewGetErrorString(err);
        throw std::runtime_error(error_string);
    }
    SDL_GetWindowSize(_sdl_window, &_window_width, &_window_height);
    set_3d();
}

void Context::set_3d()
{
    float aspect = (float)_window_width / (float)_window_height;
    glViewport(0, 0, _window_width, _window_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    gluPerspective(60.0, aspect, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);

    // Scene Setup
    glClearColor(0.0, 0.0, 0.5, 0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}

void Context::set_2d()
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, (double)_window_width, 0, (double)_window_height, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
}

void Context::set_ortho()
{
    int view_port[4];
    glGetIntegerv(GL_VIEWPORT, view_port);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, view_port[2], 0, view_port[3], -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
}

void Context::unset_ortho()
{
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

Context::~Context()
{
    if (_context) {
        SDL_GL_DeleteContext(_context);
    }
    if (_sdl_window) {
        SDL_DestroyWindow(_sdl_window);
    }
}


// const SDL_VideoInfo *video = SDL_GetVideoInfo();
// if (video == NULL) {
//   throw runtime_error("Could not get video info");
// }
// int width;
// int height;
// SDL_GetWindowSize(_window, &width, &height);
// cout << "SDL: Video mode set ("
//      << width
//      << ","
//      << height << ")"
//      << video->vfmt->BitsPerPixel
//      << ")" << endl;
// int ndisplays = SDL_GetNumVideoDisplays();
// cout << "Number of displays: " << ndisplays << endl;

} // namespace gfx

// vim: set sts=2 sw=2 expandtab:
