# Test registration.
#
# This is the modern form of the original tests/CMakeLists.txt `test_module`
# macro. Two properties of that macro are deliberately preserved:
#
#   1. Each test is a separate executable registered individually with CTest.
#   2. Tests run with the *repository root* as their working directory, so
#      fixture paths like "data/test.json" resolve exactly as they did in 2016.
#
# What's new: doctest instead of bare main(), and cross-built binaries are run
# through an emulator when one is configured.

include_guard(GLOBAL)

# Cross-built test binaries can execute on the dev box via qemu-user-static.
# Toolchain files set WREEL_TEST_EMULATOR; CMake also honours
# CMAKE_CROSSCOMPILING_EMULATOR, which ctest applies automatically.
if(CMAKE_CROSSCOMPILING AND WREEL_TEST_EMULATOR)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${WREEL_TEST_EMULATOR}"
        CACHE STRING "" FORCE)
    message(STATUS "Tests will run under emulator: ${WREEL_TEST_EMULATOR}")
endif()

# wreel_add_test(<name> [SOURCES ...] [LIBS ...])
#
# Defaults to a single source file <name>.cc, matching the original macro's
# one-file-per-test convention.
function(wreel_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})

    if(NOT ARG_SOURCES)
        set(ARG_SOURCES "${name}.cc")
    endif()

    add_executable(${name} ${ARG_SOURCES})
    wreel_set_target_defaults(${name})

    target_link_libraries(${name} PRIVATE
        doctest::doctest
        ${ARG_LIBS})

    # doctest supplies main() via DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN in each
    # test file; this keeps its assertion macros available in release builds.
    target_compile_definitions(${name} PRIVATE DOCTEST_CONFIG_SUPER_FAST_ASSERTS)

    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

    add_test(NAME ${name}
             COMMAND ${name}
             WORKING_DIRECTORY "${WREEL_TEST_WORKING_DIRECTORY}")

    # Surface the fixture root to tests that would rather not assume a cwd.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "WREEL_DATA_DIR=${WREEL_TEST_WORKING_DIRECTORY}/data")
endfunction()
