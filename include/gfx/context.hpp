#ifndef __GFX_VIEW_HPP__
#define __GFX_VIEW_HPP__

#include <utility>
#include <string>
#include <gfx/primitives.hpp>

struct SDL_Window;
namespace gfx
{

class Context
{
public:
    Context(const std::string& title, bool fullscreen=true);
    ~Context();
    // DISALLOW_COPY_AND_ASSIGN(Context);

    SDL_Window* window()
    {
        return _sdl_window;
    }
    void set_3d();
    void set_2d();
    void set_ortho();
    void unset_ortho();

private:
    SDL_GLContext _context;
    SDL_Window* _sdl_window;
    int _window_width;
    int _window_height;
};

} // namespace gfx

#endif
