# libposix — errno translated into typed C++ exceptions.
#
# No dependencies beyond libc. This is the bottom of the stack.

add_library(wreel_posix STATIC
    errors.cc)
add_library(wreel::posix ALIAS wreel_posix)

wreel_set_target_defaults(wreel_posix)

target_include_directories(wreel_posix PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

set_target_properties(wreel_posix PROPERTIES
    OUTPUT_NAME posix
    EXPORT_NAME posix)
