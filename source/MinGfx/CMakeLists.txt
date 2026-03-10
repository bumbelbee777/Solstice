# MinGfx: shared graphics utilities (easing, keyframes)
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(MinGfx STATIC
    EasingFunction.cxx
)
target_include_directories(MinGfx PUBLIC ${SOURCE_DIR})
target_link_libraries(MinGfx PUBLIC Math)
if(MSVC)
    target_compile_options(MinGfx PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(MinGfx PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
