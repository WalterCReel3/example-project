# - Find the OpenGL Extension Wrangler Library (GLEW)
# This module defines the following variables:
#  GLEW_INCLUDE_DIRS - include directories for GLEW
#  GLEW_LIBRARIES - libraries to link against GLEW
#  GLEW_FOUND - true if GLEW has been found and can be used

#=============================================================================
# Copyright 2012 Benjamin Eikel
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

find_path(GLEW_INCLUDE_DIR GL/glew.h
	HINTS ENV GLEWDIR
	PATH_SUFFIXES
		include
		i686-w64-mingw32/include
		x86_64-w64-mingw32/include
	PATHS
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/
		/usr/
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	find_library(GLEW_LIBRARY
		NAMES
			glew
			glew32
			glew32s
		HINTS ENV GLEWDIR
		PATH_SUFFIXES
			lib64
			lib
			lib/Release/x64
			lib/x64
			x86_64-w64-mingw32/lib
	)
# On 32bit build find the 32bit libs
else(CMAKE_SIZEOF_VOID_P EQUAL 4)
	find_library(GLEW_LIBRARY
		NAMES
			glew
			glew32
			glew32s
		HINTS ENV GLEWDIR
		PATH_SUFFIXES
			lib
			lib/x86
			lib/Release/Win32
			x86-w32-mingw32/lib
	)
endif(CMAKE_SIZEOF_VOID_P EQUAL 8)

set(GLEW_INCLUDE_DIRS ${GLEW_INCLUDE_DIR})
set(GLEW_LIBRARIES ${GLEW_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLEW
	REQUIRED_VARS GLEW_INCLUDE_DIR GLEW_LIBRARY)

mark_as_advanced(GLEW_INCLUDE_DIR GLEW_LIBRARY)
