# bgfx.cmake - bgfx building in cmake
# Written in 2017 by Joshua Brookover <joshua.al.brookover@gmail.com>
#
# To the extent possible under law, the author(s) have dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication along with
# this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.

# Grab the shaderc source files
file(
	GLOB
	SHADERC_SOURCES #
	${BGFX_DIR}/tools/shaderc/*.cpp #
	${BGFX_DIR}/tools/shaderc/*.h #
	${BGFX_DIR}/src/shader* #
)

# DXIL backend requires Windows SDK headers (for example unknwnbase.h).
# Exclude it on non-Windows hosts so shaderc builds on Linux/macOS.
if(NOT WIN32)
	list(REMOVE_ITEM SHADERC_SOURCES "${BGFX_DIR}/tools/shaderc/shaderc_dxil.cpp")
	set(BGFX_SHADERC_DXIL_STUB "${BGFX_DIR}/tools/shaderc/shaderc_dxil_stub.cpp")
	file(WRITE "${BGFX_SHADERC_DXIL_STUB}" [=[
#include "shaderc.h"

namespace bgfx
{
bool compileDxilShader(const Options&, uint32_t, const std::string&, bx::WriterI*, bx::WriterI* _messages)
{
	if (nullptr != _messages)
	{
		writef(_messages, "DXIL shader compilation is not supported in this build.\n");
	}
	return false;
}
} // namespace bgfx
]=])
	list(APPEND SHADERC_SOURCES "${BGFX_SHADERC_DXIL_STUB}")
endif()

add_executable(shaderc ${SHADERC_SOURCES})

target_link_libraries(
	shaderc
	PRIVATE bx
			bgfx-vertexlayout
			fcpp
			glslang
			glsl-optimizer
			spirv-opt
			spirv-cross
)
target_link_libraries(
	shaderc
	PRIVATE bx
			bimg
			bgfx-vertexlayout
			fcpp
			glslang
			glsl-optimizer
			spirv-opt
			spirv-cross
			webgpu
)
if(BGFX_AMALGAMATED)
	target_link_libraries(shaderc PRIVATE bgfx-shader)
endif()

set_target_properties(
	shaderc PROPERTIES FOLDER "bgfx/tools" #
					   OUTPUT_NAME ${BGFX_TOOLS_PREFIX}shaderc #
)

if(BGFX_BUILD_TOOLS_SHADER)
	add_executable(bgfx::shaderc ALIAS shaderc)
	if(BGFX_CUSTOM_TARGETS)
		add_dependencies(tools shaderc)
	endif()
endif()

if(ANDROID)
	target_link_libraries(shaderc PRIVATE log)
elseif(IOS)
	set_target_properties(shaderc PROPERTIES MACOSX_BUNDLE ON MACOSX_BUNDLE_GUI_IDENTIFIER shaderc)
endif()

if(BGFX_INSTALL)
	install(TARGETS shaderc EXPORT "${TARGETS_EXPORT_NAME}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
