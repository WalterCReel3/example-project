project(project1)
file(GLOB SKRATCH_SOURCES "*.cc")
add_executable(project1 ${SKRATCH_SOURCES})
target_link_libraries(project1
			${SDL2_LIBRARY}
			${GLEW_LIBRARIES}
			${OPENGL_LIBRARIES})
install(TARGETS project1 DESTINATION bin)
