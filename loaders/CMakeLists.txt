# libloaders — asset parsing.
#
# Every source builds on every target. obj.cc used to be gated on the GL backend
# because it filled a gfx::ObjModel holding GLuint buffer handles, so a text parser
# transitively included SDL_opengl.h. It produces gfx::Mesh now — plain data — and
# GPU residency is the renderer's job, which is what removed the gate and made
# tests/test_obj.cc possible on the GPU-less targets.

set(WREEL_LOADERS_SOURCES
    image.cc
    obj.cc
    sparrow.cc)

add_library(wreel_loaders STATIC ${WREEL_LOADERS_SOURCES})
add_library(wreel::loaders ALIAS wreel_loaders)

wreel_set_target_defaults(wreel_loaders)

target_include_directories(wreel_loaders PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

target_link_libraries(wreel_loaders
    PUBLIC  wreel::util
            wreel::gfx
    PRIVATE wreel::posix)

set_target_properties(wreel_loaders PROPERTIES
    OUTPUT_NAME loaders
    EXPORT_NAME loaders)
