project(posix)
file(GLOB POSIX_SOURCES "*.cc")
add_library(${PROJECT_NAME} STATIC ${POSIX_SOURCES})
