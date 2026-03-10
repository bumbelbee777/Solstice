# Skeleton: bone tree, pose, and posing helpers
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Skeleton STATIC
    Skeleton.cxx
    PoseOps.cxx
)
target_include_directories(Skeleton PUBLIC ${SOURCE_DIR})
target_link_libraries(Skeleton PUBLIC Math)
if(MSVC)
    target_compile_options(Skeleton PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Skeleton PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
