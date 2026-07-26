# libgfx — windowing and rendering.
#
# The renderers are additive, not alternatives:
#
#   renderer/   SDL_Renderer. Built ALWAYS, on every target. Draws the game.
#               Its driver is software on the Miyoo Mini and opengles2 on Mali,
#               which is why it is not named after either.
#   gles2/      our own GL context, for shaders and 3D. WREEL_ENABLE_GLES2,
#               defaulted from device capability. Never on the Miyoo Mini.
#   gl_legacy   the original 2016 fixed-function code: context.cc, system.cc,
#               obj.cc, utils.cc. Desktop only, WREEL_ENABLE_GL_LEGACY, and
#               deleted once skratch is ported off it.
#
# A window is driven by one renderer or the other; both being compiled is what
# lets one build contain a 2D game executable and a shader demo.
#
# See planning/2026-07-26-gfx-renderer-and-gles2/ and
# docs/TARGETS.md § "Graphics backends".

# Renderer-agnostic: pure data, no rendering calls.
set(WREEL_GFX_SOURCES
    spritesheet.cc
    renderer/system.cc
    renderer/context.cc)

if(WREEL_ENABLE_GLES2)
    list(APPEND WREEL_GFX_SOURCES
        gles2/api.cc
        gles2/context.cc
        gles2/program.cc
        gles2/mesh_buffer.cc
        gles2/sprite_renderer.cc
        gles2/texture.cc)
endif()

if(WREEL_ENABLE_GL_LEGACY)
    list(APPEND WREEL_GFX_SOURCES
        system.cc
        context.cc
        obj.cc
        utils.cc)
endif()

add_library(wreel_gfx STATIC ${WREEL_GFX_SOURCES})
add_library(wreel::gfx ALIAS wreel_gfx)

wreel_set_target_defaults(wreel_gfx)

target_include_directories(wreel_gfx PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# glm is PUBLIC, unlike pugixml under wreel_util: include/gfx/types.hpp names
# glm::vec3 in gfx::Mesh, so every consumer of a Mesh needs the headers. That is
# the deliberate exception to "wrap it, don't spread it" recorded in
# docs/TARGETS.md — a maths vector is a value type, not a subsystem.
target_link_libraries(wreel_gfx
    PUBLIC  wreel::util
            glm::glm
            ${WREEL_SDL2_TARGET}
            ${WREEL_SDL2_TTF_TARGET}
            ${WREEL_SDL2_IMAGE_TARGET})

# Let headers, sources and wreel-probe report which renderers are present. These
# say "compiled in", not "in use" — the choice is per-window now, not per-build.
target_compile_definitions(wreel_gfx PUBLIC
    WREEL_GFX_RENDERER=1)

# No GL library is linked, deliberately. gfx/gles2/api.cc resolves every entry
# point with SDL_GL_GetProcAddress, so nothing here gains a libGLESv2 soname
# dependency. Three reasons, in include/gfx/gles2/api.hpp: a DT_NEEDED on GLESv2
# would stop a 2D-only binary starting on a device with no GLES blob, since both
# renderers live in this one library; handheld firmwares put their vendor blobs
# where only SDL_GL_LoadLibrary knows to look; and Debian's aarch64 sysroot has no
# libGLESv2.so at all, so linking would break the cross compile-check that is the
# only verification available before hardware.
if(WREEL_ENABLE_GLES2)
    target_compile_definitions(wreel_gfx PUBLIC WREEL_GFX_GLES2=1)
endif()

if(WREEL_ENABLE_GL_LEGACY)
    target_compile_definitions(wreel_gfx PUBLIC WREEL_GFX_GL_LEGACY=1)
    target_link_libraries(wreel_gfx PUBLIC
        GLEW::GLEW
        OpenGL::GL
        OpenGL::GLU)
endif()

set_target_properties(wreel_gfx PROPERTIES
    OUTPUT_NAME gfx
    EXPORT_NAME gfx)
