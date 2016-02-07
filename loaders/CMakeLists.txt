project(loaders)
file(GLOB LOADER_SOURCES "*.cc")
add_library(${PROJECT_NAME} STATIC ${LOADER_SOURCES})
target_link_libraries(${PROJECT_NAME} util posix)
