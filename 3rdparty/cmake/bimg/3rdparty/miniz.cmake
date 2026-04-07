# bgfx.cmake - bgfx building in cmake
# Written in 2017 by Joshua Brookover <joshua.al.brookover@gmail.com>
#
# To the extent possible under law, the author(s) have dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication along with
# this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

# Ensure the directory exists
if(NOT IS_DIRECTORY ${BIMG_DIR})
	message(SEND_ERROR "Could not load bimg, directory does not exist. ${BIMG_DIR}")
	return()
endif()

if(NOT MINIZ_LIBRARIES)
	file(GLOB_RECURSE #
		 MINIZ_SOURCES #
		 ${BIMG_DIR}/3rdparty/tinyexr/deps/miniz/miniz.* #
	)
	# image_decode.cpp may include either <miniz.c> or <miniz/miniz.c>
	# depending on bimg/tinyexr snapshot. Point include dir to deps root so
	# <miniz/miniz.c> resolves, and also collect legacy flat-layout sources.
	file(GLOB_RECURSE #
		 MINIZ_SOURCES_LEGACY #
		 ${BIMG_DIR}/3rdparty/tinyexr/deps/miniz.* #
	)
	list(APPEND MINIZ_SOURCES ${MINIZ_SOURCES_LEGACY})
	list(REMOVE_DUPLICATES MINIZ_SOURCES)
	# tinyexr.h uses #include <miniz.h>; headers live under deps/miniz/, not deps/.
	set(MINIZ_INCLUDE_DIR ${BIMG_DIR}/3rdparty/tinyexr/deps/miniz)

	# Vendored C only: silence #pragma message and other third-party noise (one TU, low risk).
	if(MINIZ_SOURCES AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		set_source_files_properties(${MINIZ_SOURCES} PROPERTIES COMPILE_OPTIONS "-w")
	endif()
endif()
