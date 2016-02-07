project(skratch)
file(GLOB SKRATCH_SOURCES "*.cc")
add_executable(skratch ${SKRATCH_SOURCES})
target_link_libraries(skratch
			util
			posix
			loaders
			gfx
			${SDL2_LIBRARY}
			${SDL2_TTF_LIBRARIES}
			${SDL2_IMAGE_LIBRARIES}
			${GLEW_LIBRARIES}
			${OPENGL_LIBRARIES})

install(TARGETS skratch DESTINATION bin)
