project(tests)

set(TEST_LIBS
	util
	posix
	${OPENGL_LIBRARIES}
	${GLEW_LIBRARIES})

macro (test_module tname)
  add_executable(${tname} ${tname}.cc)
  target_link_libraries(${tname} ${TEST_LIBS})
  add_test(NAME ${tname} COMMAND ${tname} WORKING_DIRECTORY ${TEST_DIRECTORY})
endmacro (test_module)

test_module(test_basic)
test_module(test_file)
test_module(test_json)
