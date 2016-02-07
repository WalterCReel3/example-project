project(gfx)
file(GLOB GFX_SOURCES "*.cc")
add_library(${PROJECT_NAME} STATIC ${GFX_SOURCES})
