# Material: compact material representation and serialization.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Material STATIC
    Material.cxx
)
target_include_directories(Material PUBLIC ${SOURCE_DIR})
target_link_libraries(Material PUBLIC Core Math)
if(MSVC)
    target_compile_options(Material PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Material PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
