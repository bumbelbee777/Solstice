# Asset: model loading/parsing/streaming.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Asset STATIC
    Loading/AssetLoader.cxx
    Streaming/AssetStream.cxx
    IO/WavefrontParser.cxx
)
target_include_directories(Asset PUBLIC ${SOURCE_DIR})
target_link_libraries(Asset PUBLIC Core Material Math tinygltf)
if(MSVC)
    target_compile_options(Asset PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Asset PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
