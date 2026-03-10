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
if(NOT IS_DIRECTORY ${BGFX_DIR})
	message(SEND_ERROR "Could not load bgfx, directory does not exist. ${BGFX_DIR}")
	return()
endif()

set(SPIRV_HEADERS ${BGFX_DIR}/3rdparty/spirv-headers)
set(SPIRV_TOOLS ${BGFX_DIR}/3rdparty/spirv-tools)

file(
	GLOB
	SPIRV_OPT_SOURCES
	# libspirv
	${SPIRV_TOOLS}/source/opt/*.cpp
	${SPIRV_TOOLS}/source/opt/*.h
	${SPIRV_TOOLS}/source/reduce/*.cpp
	${SPIRV_TOOLS}/source/reduce/*.h
	${SPIRV_TOOLS}/source/assembly_grammar.cpp
	${SPIRV_TOOLS}/source/assembly_grammar.h
	${SPIRV_TOOLS}/source/binary.cpp
	${SPIRV_TOOLS}/source/binary.h
	${SPIRV_TOOLS}/source/cfa.h
	${SPIRV_TOOLS}/source/diagnostic.cpp
	${SPIRV_TOOLS}/source/diagnostic.h
	${SPIRV_TOOLS}/source/disassemble.cpp
	${SPIRV_TOOLS}/source/disassemble.h
	${SPIRV_TOOLS}/source/enum_set.h
	${SPIRV_TOOLS}/source/enum_string_mapping.cpp
	${SPIRV_TOOLS}/source/enum_string_mapping.h
	${SPIRV_TOOLS}/source/ext_inst.cpp
	${SPIRV_TOOLS}/source/ext_inst.h
	${SPIRV_TOOLS}/source/extensions.cpp
	${SPIRV_TOOLS}/source/extensions.h
	${SPIRV_TOOLS}/source/instruction.h
	${SPIRV_TOOLS}/source/latest_version_glsl_std_450_header.h
	${SPIRV_TOOLS}/source/latest_version_opencl_std_header.h
	${SPIRV_TOOLS}/source/latest_version_spirv_header.h
	${SPIRV_TOOLS}/source/libspirv.cpp
	${SPIRV_TOOLS}/source/macro.h
	${SPIRV_TOOLS}/source/name_mapper.cpp
	${SPIRV_TOOLS}/source/name_mapper.h
	${SPIRV_TOOLS}/source/opcode.cpp
	${SPIRV_TOOLS}/source/opcode.h
	${SPIRV_TOOLS}/source/operand.cpp
	${SPIRV_TOOLS}/source/operand.h
	${SPIRV_TOOLS}/source/parsed_operand.cpp
	${SPIRV_TOOLS}/source/parsed_operand.h
	${SPIRV_TOOLS}/source/print.cpp
	${SPIRV_TOOLS}/source/print.h
	${SPIRV_TOOLS}/source/software_version.cpp
	${SPIRV_TOOLS}/source/spirv_constant.h
	${SPIRV_TOOLS}/source/spirv_definition.h
	${SPIRV_TOOLS}/source/spirv_endian.cpp
	${SPIRV_TOOLS}/source/spirv_endian.h
	${SPIRV_TOOLS}/source/spirv_optimizer_options.cpp
	${SPIRV_TOOLS}/source/spirv_reducer_options.cpp
	${SPIRV_TOOLS}/source/spirv_target_env.cpp
	${SPIRV_TOOLS}/source/spirv_target_env.h
	${SPIRV_TOOLS}/source/spirv_validator_options.cpp
	${SPIRV_TOOLS}/source/spirv_validator_options.h
	${SPIRV_TOOLS}/source/table.cpp
	${SPIRV_TOOLS}/source/table.h
	${SPIRV_TOOLS}/source/table2.cpp
	${SPIRV_TOOLS}/source/table2.h
	${SPIRV_TOOLS}/source/text.cpp
	${SPIRV_TOOLS}/source/text.h
	${SPIRV_TOOLS}/source/text_handler.cpp
	${SPIRV_TOOLS}/source/text_handler.h
	${SPIRV_TOOLS}/source/to_string.cpp
	${SPIRV_TOOLS}/source/to_string.h
	${SPIRV_TOOLS}/source/util/bit_vector.cpp
	${SPIRV_TOOLS}/source/util/bit_vector.h
	${SPIRV_TOOLS}/source/util/bitutils.h
	${SPIRV_TOOLS}/source/util/hex_float.h
	${SPIRV_TOOLS}/source/util/parse_number.cpp
	${SPIRV_TOOLS}/source/util/parse_number.h
	${SPIRV_TOOLS}/source/util/string_utils.cpp
	${SPIRV_TOOLS}/source/util/string_utils.h
	${SPIRV_TOOLS}/source/util/timer.h
	# val/ - use GLOB so newer SPIRV-Tools (DotProductPass, GroupPass, PipePass, etc.) are included
	${SPIRV_TOOLS}/source/val/*.cpp
	${SPIRV_TOOLS}/source/val/*.h
)

add_library(spirv-opt STATIC ${SPIRV_OPT_SOURCES})

# Put in a "bgfx" folder in Visual Studio
set_target_properties(spirv-opt PROPERTIES FOLDER "bgfx")

target_include_directories(
	spirv-opt
	PUBLIC ${SPIRV_TOOLS}/include #
	PRIVATE ${SPIRV_TOOLS} #
			${SPIRV_TOOLS}/include/generated #
			${SPIRV_TOOLS}/source #
			${SPIRV_HEADERS}/include #
)
