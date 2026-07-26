#include <gfx/gles2/api.hpp>

#include <util/format.hpp>
#include <util/logging.hpp>

#include <SDL.h>

#include <stdexcept>

namespace gfx
{
namespace gles2
{

namespace gl
{

#define WREEL_GLES2_DEFINE(name) decltype(&::gl##name) name = nullptr;
WREEL_GLES2_FUNCTIONS(WREEL_GLES2_DEFINE)
#undef WREEL_GLES2_DEFINE

} // namespace gl

namespace
{

bool loaded = false;

// SDL_GL_GetProcAddress returns void*; converting that to a function pointer is
// implementation-defined in ISO C++ and universal in practice. Confined to this
// one function so the cast appears once rather than forty times.
template<typename Fn>
void resolve(Fn& target, const char* name)
{
    void* address = SDL_GL_GetProcAddress(name);
    if (!address) {
        throw std::runtime_error(util::format(
            "GLES2 entry point '%s' not found: %s. The driver reported a GLES2 "
            "context but does not export the 2.0 core API",
            name, SDL_GetError()));
    }
    target = reinterpret_cast<Fn>(address);
}

} // namespace

void load_api()
{
    loaded = false;

#define WREEL_GLES2_RESOLVE(name) resolve(gl::name, "gl" #name);
    WREEL_GLES2_FUNCTIONS(WREEL_GLES2_RESOLVE)
#undef WREEL_GLES2_RESOLVE

    loaded = true;

    // Logged once at load rather than left for a bug report to ask for. The
    // renderer string is the only cheap way to tell a Mali blob from Mesa's
    // software rasteriser, and confusing the two wastes an afternoon.
    const GLubyte* version = gl::GetString(GL_VERSION);
    const GLubyte* renderer = gl::GetString(GL_RENDERER);
    util::log_info(
        "gles2: %s / %s",
        version ? reinterpret_cast<const char*>(version) : "(no version)",
        renderer ? reinterpret_cast<const char*>(renderer) : "(no renderer)");
}

bool api_loaded()
{
    return loaded;
}

} // namespace gles2
} // namespace gfx
