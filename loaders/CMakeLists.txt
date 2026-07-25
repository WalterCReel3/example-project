# libloaders — asset parsing.
#
# obj.cc is gated on the GL backend because it populates gfx::ObjModel, which
# holds GLuint vertex/colour/index buffer handles and so is inherently OpenGL.
# Decoupling model data from GPU buffers is part of the software-backend work;
# until then a software build gets image loading only.

set(WREEL_LOADERS_SOURCES
    image.cc
    sparrow.cc)

if(WREEL_GFX_BACKEND STREQUAL "gl_legacy")
    list(APPEND WREEL_LOADERS_SOURCES obj.cc)
endif()

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
