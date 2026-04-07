cmake_minimum_required(VERSION 3.20)

# Linux/macOS: use ccache when installed (large speedup on rebuilds).
# Must be set before project()/enable_language() so the launcher applies to compiler detection.
if(UNIX AND NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
    find_program(SOLSTICE_CCACHE_EXECUTABLE ccache)
    if(SOLSTICE_CCACHE_EXECUTABLE)
        set(CMAKE_CXX_COMPILER_LAUNCHER "${SOLSTICE_CCACHE_EXECUTABLE}" CACHE STRING "C++ compiler launcher (ccache when found)")
        set(CMAKE_C_COMPILER_LAUNCHER "${SOLSTICE_CCACHE_EXECUTABLE}" CACHE STRING "C compiler launcher (ccache when found)")
    endif()
endif()

project(Solstice VERSION 0.1.0 LANGUAGES CXX)

# CMake 4 dropped compatibility defaults for projects declaring very old policy versions.
# Some transitive dependencies (for example zstd's build/cmake project) still rely on that.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0" AND NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "Minimum policy version for legacy subprojects")
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get git commit hash for versioning
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
    set(GIT_COMMIT_HASH "UNKNOWN")
endif()

# ============================================================================
# Dependencies via CPM (fetch at configure time; no manual 3rdparty clone)
# ============================================================================
# Prefer CPM_SOURCE_CACHE from the environment (CI or a persistent user cache).
# Example (bash): export CPM_SOURCE_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/solstice/cpm"
set(_solstice_cpm_source_cache "${CMAKE_SOURCE_DIR}/.cpm-cache")
if(DEFINED ENV{CPM_SOURCE_CACHE} AND NOT "$ENV{CPM_SOURCE_CACHE}" STREQUAL "")
    set(_solstice_cpm_source_cache "$ENV{CPM_SOURCE_CACHE}")
endif()
set(CPM_SOURCE_CACHE "${_solstice_cpm_source_cache}" CACHE PATH "CPM dependency source cache (override with env CPM_SOURCE_CACHE or -DCPM_SOURCE_CACHE=)")
include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

# BGFX stack: fetch sources only; 3rdparty/cmake builds them
CPMAddPackage(NAME bx GITHUB_REPOSITORY bkaradzic/bx GIT_TAG master DOWNLOAD_ONLY YES
    PATCHES ${CMAKE_SOURCE_DIR}/patches/bx-thread-msvc-setThreadName.patch
)
CPMAddPackage(NAME bimg GITHUB_REPOSITORY bkaradzic/bimg GIT_TAG master DOWNLOAD_ONLY YES)
CPMAddPackage(NAME bgfx GITHUB_REPOSITORY bkaradzic/bgfx GIT_TAG master DOWNLOAD_ONLY YES)

# SDL3: full CMake project
# SDL_TEST_LIBRARY OFF: avoids SDL3_test + libunwind pkg-config noise (SDL_TEST is not a CMake flag).
# SDL_PIPEWIRE OFF / SDL_HIDAPI_LIBUSB OFF: skip optional deps many dev images lack; ALSA/Pulse + evdev/hidraw remain.
CPMAddPackage(NAME SDL3 GITHUB_REPOSITORY libsdl-org/SDL GIT_TAG release-3.4.2
    OPTIONS
        "SDL_SHARED ON"
        "SDL_STATIC OFF"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_PIPEWIRE OFF"
        "SDL_HIDAPI_LIBUSB OFF"
    EXCLUDE_FROM_ALL YES
)

# Dear ImGui: fetch sources only; 3rdparty builds imgui target with SDL3+OpenGL3 backends
CPMAddPackage(NAME imgui GITHUB_REPOSITORY ocornut/imgui GIT_TAG v1.92.6 DOWNLOAD_ONLY YES)

# SDL_mixer: static lib, no optional codecs to keep build simple
CPMAddPackage(NAME SDL_mixer GITHUB_REPOSITORY libsdl-org/SDL_mixer GIT_TAG prerelease-3.1.2
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "SDLMIXER_VENDORED OFF"
        "SDLMIXER_INSTALL OFF"
        "SDLMIXER_TESTS OFF"
        "SDLMIXER_OPUS OFF"
        "SDLMIXER_FLAC_LIBFLAC OFF"
        "SDLMIXER_GME OFF"
        "SDLMIXER_MOD_XMP OFF"
        "SDLMIXER_MP3_MPG123 OFF"
        "SDLMIXER_MIDI_FLUIDSYNTH OFF"
        "SDLMIXER_VORBIS_VORBISFILE OFF"
        "SDLMIXER_VORBIS_TREMOR OFF"
        "SDLMIXER_WAVPACK OFF"
    EXCLUDE_FROM_ALL YES
)

# tinygltf: header-only; v2.5.0 CMake does not create a target, so provide one for include path
CPMAddPackage(NAME tinygltf GITHUB_REPOSITORY syoyo/tinygltf GIT_TAG v2.5.0
    OPTIONS
        "TINYGLTF_INSTALL OFF"
        "TINYGLTF_BUILD_LOADER_EXAMPLE OFF"
        "TINYGLTF_BUILD_GL_EXAMPLES OFF"
        "TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF"
        "TINYGLTF_BUILD_BUILDER_EXAMPLE OFF"
)
if(NOT TARGET tinygltf)
    add_library(tinygltf INTERFACE)
    target_include_directories(tinygltf INTERFACE ${tinygltf_SOURCE_DIR})
endif()

# ReactPhysics3D: library only (patch adds missing #include <chrono> for MSVC)
# Pin to v0.9.0 commit SHA so all platforms get the same source (avoids API mismatch on Linux)
CPMAddPackage(NAME reactphysics3d GITHUB_REPOSITORY DanielChappuis/reactphysics3d GIT_TAG 4bbbaa7c6e92942734eec696e23a2fad1f1cb8a1
    PATCHES ${CMAKE_SOURCE_DIR}/patches/reactphysics3d-DefaultLogger-chrono.patch
    OPTIONS
        "RP3D_COMPILE_LIBRARY ON"
        "RP3D_COMPILE_TESTBED OFF"
        "RP3D_COMPILE_TESTS OFF"
)

# zstd: decompression for RELIC one-shot assets (UI, global audio, cutscenes)
# SOURCE_SUBDIR required: zstd's CMake project lives in build/cmake, not repo root
CPMAddPackage(NAME zstd GITHUB_REPOSITORY facebook/zstd GIT_TAG v1.5.5
    SOURCE_SUBDIR build/cmake
    OPTIONS "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_SHARED OFF" "ZSTD_BUILD_STATIC ON")

# Point 3rdparty glue at CPM-fetched sources (for bgfx build)
set(BX_DIR "${bx_SOURCE_DIR}" CACHE STRING "Location of bx" FORCE)
set(BIMG_DIR "${bimg_SOURCE_DIR}" CACHE STRING "Location of bimg" FORCE)
set(BGFX_DIR "${bgfx_SOURCE_DIR}" CACHE STRING "Location of bgfx" FORCE)

# BGFX + ImGui glue (uses BX_DIR, BIMG_DIR, BGFX_DIR, imgui_SOURCE_DIR)
add_subdirectory(3rdparty)

# Engine source: per-directory static libs + SolsticeEngine SHARED (see source/CMakeLists.txt)
add_subdirectory(source)

# Link third-party libraries to the engine (PUBLIC so consumers inherit them)
target_link_libraries(SolsticeEngine PUBLIC
    SDL3-shared
    bgfx
    bimg
    bimg_decode
    bx
    imgui
    SDL3_mixer-static
    tinygltf
    ReactPhysics3D::reactphysics3d
    libzstd_static
)

# ============================================================================
# Shader Compilation
# ============================================================================
# tools/compile_shaders.py picks the profile per OS (Linux: spirv for Vulkan, Windows: s_5_0, macOS: metal).
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Collect shader source files for dependency tracking
# CONFIGURE_DEPENDS forces CMake to re-check for new/removed files on each build
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/Shaders/*.sc")

add_custom_target(CompileShaders
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/compile_shaders.py
        --shaderc $<TARGET_FILE:shaderc>
        --input-dir ${CMAKE_SOURCE_DIR}/source/Shaders
        --output-dir ${CMAKE_SOURCE_DIR}/source/Shaders/bin
        --bgfx-include ${bgfx_SOURCE_DIR}/src
    DEPENDS shaderc ${SHADER_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling BGFX shaders..."
    VERBATIM
)

# Ensure shaders are compiled before the engine is built
add_dependencies(SolsticeEngine CompileShaders)

# ============================================================================
# Helper function to copy shaders to executable directory
# ============================================================================
# Usage: copy_shaders_to_target(<target_name>)
# Copies compiled shaders from source/Shaders/bin/ to <target>'s output directory/shaders/
function(copy_shaders_to_target target_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMENT "Creating shaders directory for ${target_name}"
    )

    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/source/Shaders/bin
        $<TARGET_FILE_DIR:${target_name}>/shaders
        COMMENT "Copying shaders to ${target_name} directory"
    )
endfunction()

# ============================================================================
# Tests
# ============================================================================
add_subdirectory(tests)

# ============================================================================
# Utilities
# ============================================================================
add_subdirectory(utilities/LibUI)
add_subdirectory(utilities/Sharpon)
add_subdirectory(utilities/MovieMaker)
add_subdirectory(utilities/LevelEditor)

# ============================================================================
# Examples
# ============================================================================
add_subdirectory(example/PhysicsPlayground)
add_subdirectory(example/Blizzard)
add_subdirectory(example/Hyperbourne)
add_subdirectory(example/Avalanche)
add_subdirectory(example/Disco)
add_subdirectory(example/ThermalPlume)
