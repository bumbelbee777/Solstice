# Core: engine fundamentals, asset loading, materials, etc.
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Core STATIC
    Debug.cxx
    Audio.cxx
    WavefrontParser.cxx
    Cache.cxx
    MLP.cxx
    CNN.cxx
    NavMesh.cxx
    FSM.cxx
    AssetStream.cxx
    Save.cxx
    JSON.cxx
    Base64.cxx
    Material.cxx
    AssetLoader.cxx
    Profiler.cxx
    ScopeTimer.cxx
    Mmap.cxx
    LZ4.cxx
    Relic/Bootstrap.cxx
    Relic/Reader.cxx
    Relic/VirtualTable.cxx
    Relic/Decompress.cxx
    Relic/Delta.cxx
    Relic/AssetService.cxx
    Relic/Relic.cxx
)
target_include_directories(Core PUBLIC ${SOURCE_DIR})
if(DEFINED zstd_SOURCE_DIR)
    target_include_directories(Core PRIVATE ${zstd_SOURCE_DIR}/lib)
endif()
target_link_libraries(Core PUBLIC Math SDL3-shared SDL3_mixer-static tinygltf imgui bgfx libzstd_static)
if(MSVC)
    target_compile_options(Core PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Core PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
