# Math: vectors, matrices, quaternions
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Math STATIC
    Quaternion.cxx
)
target_include_directories(Math PUBLIC ${SOURCE_DIR})
if(MSVC)
    target_compile_options(Math PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Math PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
