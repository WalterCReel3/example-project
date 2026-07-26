#pragma once

// SDL ships its own copies of the Khronos GLES2 headers and will use them when
// this is defined; without it, SDL_opengles2.h includes the *system*
// <GLES2/gl2.h> on Linux. Defined here so the renderer compiles against a
// device sysroot that carries no GLES2 headers — which Debian's aarch64 cross
// sysroot does not, and a stripped firmware rootfs generally will not either.
// The declarations are what this file needs; the addresses come from SDL at
// runtime.
#define SDL_USE_BUILTIN_OPENGL_DEFINITIONS 1
#include <SDL_opengles2.h>

//============================================================================
//
// GLES 2.0 entry points, loaded through SDL
//
// Every GL function this renderer uses is a function pointer resolved with
// SDL_GL_GetProcAddress rather than a symbol linked from libGLESv2. Calls are
// spelled gl::CreateShader rather than glCreateShader, so the two cannot be
// confused.
//
// Why not just link -lGLESv2, which is one line of CMake:
//
//   - It would give every binary a libGLESv2 soname dependency. Everything here
//     links static by design, and the packaging snapshot's "no dynamic
//     libraries to install" claim depends on that staying true — see
//     docs/TARGETS.md § "Pinned dependencies".
//   - Handheld firmwares ship vendor GLES blobs in vendor-specific places, and
//     some do not provide the plain `libGLESv2.so` development soname at all.
//     SDL_GL_LoadLibrary already knows how to find whatever is there, because
//     that is how SDL's own opengles2 render driver reaches the GPU.
//   - Debian's aarch64 cross sysroot has no GLES library to link against, so a
//     link-time dependency would mean the cross presets could not compile-check
//     this code — which is the one form of verification available before
//     hardware.
//
// The signatures are not written out. Each is `decltype(&::glFoo)`, taken from
// the prototypes SDL_opengles2.h already declares, so a wrong argument list
// here is impossible rather than merely unlikely. The names come from one macro
// list, which is the same generate-from-a-list approach
// include/posix/errors.hpp uses for its exception types.
//
// ORDERING: load() needs a current GL context, since that is what
// SDL_GL_GetProcAddress resolves against on most drivers. gles2::Context calls
// it after SDL_GL_MakeCurrent; nothing else should have to.
//
//============================================================================

// The subset actually used. Adding a function means adding one line here.
#define WREEL_GLES2_FUNCTIONS(X) \
    /* errors and strings */     \
    X(GetError)                  \
    X(GetString)                 \
    /* frame state */            \
    X(Viewport)                  \
    X(ClearColor)                \
    X(Clear)                     \
    X(Enable)                    \
    X(Disable)                   \
    X(BlendFunc)                 \
    X(DepthFunc)                 \
    /* shaders and programs */   \
    X(CreateShader)              \
    X(ShaderSource)              \
    X(CompileShader)             \
    X(GetShaderiv)               \
    X(GetShaderInfoLog)          \
    X(DeleteShader)              \
    X(CreateProgram)             \
    X(AttachShader)              \
    X(DetachShader)              \
    X(LinkProgram)               \
    X(GetProgramiv)              \
    X(GetProgramInfoLog)         \
    X(DeleteProgram)             \
    X(UseProgram)                \
    X(GetUniformLocation)        \
    X(GetAttribLocation)         \
    X(UniformMatrix4fv)          \
    X(Uniform1i)                 \
    X(Uniform4fv)                \
    /* buffers */                \
    X(GenBuffers)                \
    X(BindBuffer)                \
    X(BufferData)                \
    X(DeleteBuffers)             \
    /* vertex attributes */      \
    X(EnableVertexAttribArray)   \
    X(DisableVertexAttribArray)  \
    X(VertexAttribPointer)       \
    /* drawing */                \
    X(DrawArrays)                \
    X(DrawElements)              \
    /* textures */               \
    X(GenTextures)               \
    X(BindTexture)               \
    X(TexImage2D)                \
    X(TexParameteri)             \
    X(DeleteTextures)            \
    X(ActiveTexture)             \
    X(PixelStorei)

namespace gfx
{
namespace gles2
{

// Deliberately a nested namespace rather than a struct of members: call sites
// read gl::BindBuffer(...), which is close enough to the C spelling to be
// recognisable and different enough not to be mistaken for it.
namespace gl
{

#define WREEL_GLES2_DECLARE(name) extern decltype(&::gl##name) name;
WREEL_GLES2_FUNCTIONS(WREEL_GLES2_DECLARE)
#undef WREEL_GLES2_DECLARE

} // namespace gl

// Resolves every entry point above. Throws std::runtime_error naming the first
// function that could not be found, because a null pointer here would otherwise
// surface as a crash inside an unrelated draw call.
//
// Idempotent: calling it again re-resolves, which is what a second context on a
// different driver would need.
void load_api();

// True once load_api() has completed successfully. Anything in this namespace
// that dereferences an entry point without a context is a programming error,
// and this is what an assertion or a guard clause tests.
bool api_loaded();

} // namespace gles2
} // namespace gfx
