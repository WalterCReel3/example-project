cmake_minimum_required(VERSION 2.8)
project(WinBuildTest)

if(GCC)
    set(CMAKE_CXX_FLAGS "-Wall -Werror")
endif()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, default to Debug")
    set(CMAKE_BUILD_TYPE "Debug")
endif()

#
# Basic Directory Layout
#
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")
set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist")

set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

set(TEST_DIRECTORY ${CMAKE_SOURCE_DIR})
set(PROJECT_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(EXECUTABLE_OUTPUT_PATH ${RUNTIME_OUTPUT_DIRECTORY})

include_directories("${PROJECT_INCLUDE_DIR}")

#
# Installed Third Party Packages
#
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_image REQUIRED)
 
include_directories("${SDL2_INCLUDE_DIR}")
include_directories("${SDL2_TTF_INCLUDE_DIRS}")
include_directories("${SDL2_IMAGE_INCLUDE_DIRS}")
include_directories("${GLEW_INCLUDE_DIR}")

#
# Embedded Third Party Packages
#
include(ExternalProject)
include(ExternalRapidJson)

# Pull in installed headers
include_directories("${CMAKE_BINARY_DIR}/include")

# Allow for tests
enable_testing()

#
# COMPONENTS
#
add_subdirectory(project1)
add_subdirectory(posix)
add_subdirectory(util)
add_subdirectory(gfx)
add_subdirectory(loaders)
add_subdirectory(skratch)

#
# Tests
#
add_subdirectory(tests)
