# libutil — file I/O, logging, string tokenizing, JSON.
#
# The platform backend is selected at *compile time* rather than through virtual
# dispatch: util::File holds a FileImpl by value, and which FileImpl that is
# depends on which source file gets compiled. Zero overhead, no vtable.
# include/util/file.hpp selects the matching header with the same condition.
#
# The original build globbed posix/*.cc and mswin/*.cc; listing sources
# explicitly means adding a file is a visible change rather than a silent one.

set(WREEL_UTIL_SOURCES
    file.cc
    io.cc
    logging.cc
    xml.cc)

if(WIN32)
    list(APPEND WREEL_UTIL_SOURCES mswin/fileimpl.cc)
else()
    list(APPEND WREEL_UTIL_SOURCES posix/fileimpl.cc)
endif()

add_library(wreel_util STATIC ${WREEL_UTIL_SOURCES})
add_library(wreel::util ALIAS wreel_util)

wreel_set_target_defaults(wreel_util)

target_include_directories(wreel_util PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# nlohmann/json is PUBLIC only until the util::json facade exists; once JSON
# access lives behind that facade this becomes PRIVATE and the vendor type stops
# appearing in module signatures. See docs/TARGETS.md § "Wrap it, don't spread it".
#
# pugixml is PRIVATE from the outset, which is what that comment is describing:
# include/util/xml.hpp names no pugi:: type, so no consumer of wreel::util needs
# pugixml's headers or its include path.
target_link_libraries(wreel_util
    PUBLIC  wreel::posix
            nlohmann_json::nlohmann_json
    PRIVATE pugixml::pugixml)

set_target_properties(wreel_util PROPERTIES
    OUTPUT_NAME util
    EXPORT_NAME util)
