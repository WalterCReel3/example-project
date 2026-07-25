#ifndef WREEL_GFX_SOFTWARE_SYSTEM_HPP
#define WREEL_GFX_SOFTWARE_SYSTEM_HPP

// Subsystem lifetime for the software backend.
//
// Same shape as gfx::System, minus GLEW — there is no GL to load entry points
// for. Unlike the original, ownership is explicit rather than a leaked
// singleton: the 2016 gfx::System::get_instance() heap-allocated an instance
// that Application's destructor then deleted, leaving the static _instance
// pointer dangling if anything asked for it again.

#include <string>
#include <vector>
#include <util/nocopy.hpp>
#include <gfx/software/context.hpp>

namespace gfx
{
namespace software
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

} // namespace software
} // namespace gfx

#endif
