cmake_minimum_required(VERSION 3.20)
project(Solstice VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Solstice as a shared library (DLL on Windows)
add_library(SolsticeEngine SHARED
    "source/Core/Debug.cxx"
    "source/Core/Audio.cxx"
    "source/Core/WavefrontParser.cxx"
    "source/Math/Quaternion.cxx"
    "source/Physics/PhysicsSystem.cxx"
    "source/Physics/CollisionResolution.cxx"
    "source/Physics/ConvexHullFactory.cxx"
    "source/Physics/GJK.cxx"
    "source/Physics/CCD.cxx"
    "source/Physics/LightSource.cxx"
    "source/Physics/Fluid.cxx"
    "source/Physics/NSSolver.cxx"
    "source/UI/Window.cxx"
    "source/UI/UISystem.cxx"
    "source/UI/Widgets.cxx"
    "source/Render/Camera.cxx"
    "source/Render/Context.cxx"
    "source/Render/Framebuffer.cxx"
    "source/Render/FramebufferRenderTarget.cxx"
    "source/Render/MeshFactory.cxx"
    "source/Render/AssetLoader.cxx"
    "source/Render/SoftwareRenderer.cxx"
    "source/Render/PostProcessing.cxx"
    "source/Scripting/BytecodeVM.cxx"
    "source/Scripting/Compiler.cxx"
    "source/Scripting/ScriptBindings.cxx"
)

# Set up include directories for the library
# Public include directory allows consumers to #include <source/...>
target_include_directories(SolsticeEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/source>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Solstice
)

# Add system include directories and compiler options
if(MSVC)
    target_compile_options(SolsticeEngine PRIVATE /Zc:preprocessor)
    target_compile_options(SolsticeEngine PRIVATE /W0)  # Disable all warnings for now
    target_compile_definitions(SolsticeEngine PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    target_compile_definitions(SolsticeEngine PRIVATE _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
    target_compile_definitions(SolsticeEngine PRIVATE NOMINMAX)
    target_compile_definitions(SolsticeEngine PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(SolsticeEngine PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Define SOLSTICE_EXPORTS when building the DLL
    target_compile_definitions(SolsticeEngine PRIVATE SOLSTICE_EXPORTS)
endif()

# Set C++ standard features
target_compile_features(SolsticeEngine PRIVATE cxx_std_20)

# Add source groups for better IDE organization
source_group("Source" FILES ${SOURCES})
source_group("Include" FILES ${HEADERS})

# Set target properties for better IDE support
set_target_properties(SolsticeEngine PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Add BGFX (via 3rdparty/CMakeLists.txt)
add_subdirectory(3rdparty)

# Add SDL3
add_subdirectory(3rdparty/sdl3 EXCLUDE_FROM_ALL)

# Add Dear ImGui with SDL3 backend
add_subdirectory(3rdparty/imgui)

# Add SDL_mixer for audio
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build SDL_mixer as static library" FORCE)
set(SDLMIXER_VENDORED OFF CACHE BOOL "Don't use vendored dependencies" FORCE)
set(SDLMIXER_INSTALL OFF CACHE BOOL "Don't install SDL_mixer" FORCE)
set(SDLMIXER_TESTS OFF CACHE BOOL "Don't build SDL_mixer tests" FORCE)
# Disable optional codecs to simplify build (enable only what you need)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD_XMP OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI_FLUIDSYNTH OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_VORBISFILE OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_TREMOR OFF CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/sdl_mixer EXCLUDE_FROM_ALL)

# Link libraries to the engine (PUBLIC so consumers inherit them)
target_link_libraries(SolsticeEngine PUBLIC
    SDL3-shared
    bgfx
    imgui
    SDL3_mixer-static
    tinygltf
)

# ============================================================================
# Shader Compilation
# ============================================================================
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Collect shader source files for dependency tracking
file(GLOB SHADER_SOURCES "${CMAKE_SOURCE_DIR}/source/Shaders/*.sc")

add_custom_target(CompileShaders
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/compile_shaders.py
        --shaderc $<TARGET_FILE:shaderc>
        --input-dir ${CMAKE_SOURCE_DIR}/source/Shaders
        --output-dir ${CMAKE_SOURCE_DIR}/source/Shaders/bin
        --bgfx-include ${CMAKE_SOURCE_DIR}/3rdparty/bgfx/src
    DEPENDS shaderc ${SHADER_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling BGFX shaders..."
    VERBATIM
)

# Ensure shaders are compiled before the engine is built
add_dependencies(SolsticeEngine CompileShaders)

# ============================================================================
# Examples
# ============================================================================
add_subdirectory(example/PhysicsPlayground)
