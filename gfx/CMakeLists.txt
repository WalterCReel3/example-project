# libgfx — windowing and rendering.
#
# Sources are selected by WREEL_GFX_BACKEND. This is the gate that makes a Miyoo
# Mini build possible at all: the 2016 renderer is fixed-function desktop OpenGL
# plus GLU and GLEW, and the SSD202D has no GPU whatsoever.
#
#   software    SDL_Renderer, CPU blitting. Works everywhere. The baseline.
#   gl_legacy   the original 2016 code. Desktop only. Will be retired.
#
# See docs/TARGETS.md § "Graphics backends".

# Backend-agnostic: pure data, no rendering calls.
set(WREEL_GFX_SOURCES
    spritesheet.cc)

if(WREEL_GFX_BACKEND STREQUAL "software")
    list(APPEND WREEL_GFX_SOURCES
        software/system.cc
        software/context.cc)

elseif(WREEL_GFX_BACKEND STREQUAL "gl_legacy")
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

target_link_libraries(wreel_gfx
    PUBLIC  wreel::util
            ${WREEL_SDL2_TARGET}
            ${WREEL_SDL2_TTF_TARGET}
            ${WREEL_SDL2_IMAGE_TARGET})

# Let headers and sources branch on the active backend.
string(TOUPPER "${WREEL_GFX_BACKEND}" _backend_upper)
target_compile_definitions(wreel_gfx PUBLIC
    WREEL_GFX_BACKEND_${_backend_upper}=1
    WREEL_GFX_BACKEND_NAME="${WREEL_GFX_BACKEND}")

if(WREEL_GFX_BACKEND STREQUAL "gl_legacy")
    target_link_libraries(wreel_gfx PUBLIC
        GLEW::GLEW
        OpenGL::GL
        OpenGL::GLU)
endif()

set_target_properties(wreel_gfx PROPERTIES
    OUTPUT_NAME gfx
    EXPORT_NAME gfx)
