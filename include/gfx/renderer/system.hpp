#pragma once

// Subsystem lifetime for the SDL_Renderer path.
//
// Same shape as gfx::System, minus GLEW — SDL loads the GL entry points its own
// render drivers need, so there is nothing for us to load here even when the
// driver underneath is opengles2. Unlike the original, ownership is explicit
// rather than a leaked singleton: the 2016 gfx::System::get_instance()
// heap-allocated an instance that Application's destructor then deleted,
// leaving the static _instance pointer dangling if anything asked for it again.

#include <string>
#include <vector>
#include <util/nocopy.hpp>
#include <gfx/renderer/context.hpp>

namespace gfx
{
namespace renderer
{

class System
{
public:
    System();
    ~System();

private:
    DISALLOW_COPY_AND_ASSIGN(System);

public:
    // Contexts are owned by the System and destroyed with it.
    Context* create_context(const std::string& title, int width, int height,
                            bool fullscreen = true);

private:
    std::vector<Context*> _contexts;
};

} // namespace renderer
} // namespace gfx

