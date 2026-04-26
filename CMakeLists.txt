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

# Ninja + MSVC: tools from PATH (e.g. llvm-mingw) can be chosen instead of MSVC link.exe / lib.exe.
# - Wrong linker: try_compile with cl fails ("%1 is not a valid Win32 application").
# - Wrong archiver: static libs get MSVC flags (/nologo, /out:) passed to GNU/llvm ar → link fails.
# Prefer MSVC link.exe and lib.exe when the VS toolchain environment is active (CMake Tools / vcvars).
if(CMAKE_HOST_WIN32 AND CMAKE_GENERATOR MATCHES "Ninja")
    if(DEFINED ENV{VCToolsInstallDir})
        set(_solstice_need_msvc_link FALSE)
        if(NOT DEFINED CMAKE_LINKER)
            set(_solstice_need_msvc_link TRUE)
        else()
            get_filename_component(_solstice_link_base "${CMAKE_LINKER}" NAME_WE)
            string(TOLOWER "${_solstice_link_base}" _solstice_link_base)
            if(_solstice_link_base STREQUAL "ld")
                set(_solstice_need_msvc_link TRUE)
            endif()
        endif()
        set(_solstice_need_msvc_ar FALSE)
        if(NOT DEFINED CMAKE_AR)
            set(_solstice_need_msvc_ar TRUE)
        else()
            get_filename_component(_solstice_ar_base "${CMAKE_AR}" NAME_WE)
            string(TOLOWER "${_solstice_ar_base}" _solstice_ar_base)
            if(_solstice_ar_base STREQUAL "ar" OR _solstice_ar_base STREQUAL "llvm-ar")
                set(_solstice_need_msvc_ar TRUE)
            endif()
        endif()
        foreach(_pair IN ITEMS Hostx64/x64 Hostx86/x86 Hostarm64/arm64 Hostx64/x86)
            set(_solstice_link_try "$ENV{VCToolsInstallDir}/bin/${_pair}/link.exe")
            set(_solstice_lib_try "$ENV{VCToolsInstallDir}/bin/${_pair}/lib.exe")
            if(EXISTS "${_solstice_link_try}" AND EXISTS "${_solstice_lib_try}")
                if(_solstice_need_msvc_link)
                    set(CMAKE_LINKER "${_solstice_link_try}" CACHE FILEPATH "MSVC link.exe (avoid GNU ld on PATH)" FORCE)
                endif()
                if(_solstice_need_msvc_ar)
                    set(CMAKE_AR "${_solstice_lib_try}" CACHE FILEPATH "MSVC lib.exe (avoid GNU/llvm ar on PATH)" FORCE)
                endif()
                break()
            endif()
        endforeach()
    endif()
endif()

project(Solstice VERSION 1.0.0 LANGUAGES C CXX)

include("${CMAKE_SOURCE_DIR}/cmake/SolsticeBuildOptions.cmake")

option(SOLSTICE_BUILD_UTILITIES "Build LibUI, Sharpon, LevelEditor, MovieMaker, and Refulgent" ON)
option(SOLSTICE_PACKAGE_UTILITIES_POSTBUILD "After each utility build, run tools/package_executables.py (zip in build/packages)" OFF)
if(SOLSTICE_PACKAGE_UTILITIES_POSTBUILD)
    message(STATUS "Utility post-build packaging: ON (zips: ${SOLSTICE_PACKAGES_DIR}/*-<Config>.zip)")
endif()

# enable_language() can re-resolve CMAKE_AR / CMAKE_LINKER from PATH after the pre-project() hints
# above, so llvm-mingw ar/ld can win again. Re-pin MSVC lib.exe / link.exe once MSVC is known.
if(CMAKE_HOST_WIN32 AND CMAKE_GENERATOR MATCHES "Ninja" AND MSVC)
    if(DEFINED ENV{VCToolsInstallDir})
        foreach(_pair IN ITEMS Hostx64/x64 Hostx86/x86 Hostarm64/arm64 Hostx64/x86)
            set(_solstice_link_try "$ENV{VCToolsInstallDir}/bin/${_pair}/link.exe")
            set(_solstice_lib_try "$ENV{VCToolsInstallDir}/bin/${_pair}/lib.exe")
            if(EXISTS "${_solstice_link_try}" AND EXISTS "${_solstice_lib_try}")
                get_filename_component(_solstice_link_base "${CMAKE_LINKER}" NAME_WE)
                string(TOLOWER "${_solstice_link_base}" _solstice_link_base)
                if(NOT _solstice_link_base STREQUAL "link")
                    set(CMAKE_LINKER "${_solstice_link_try}" CACHE FILEPATH "MSVC link.exe (avoid GNU ld on PATH)" FORCE)
                endif()
                get_filename_component(_solstice_ar_base "${CMAKE_AR}" NAME_WE)
                string(TOLOWER "${_solstice_ar_base}" _solstice_ar_base)
                if(NOT _solstice_ar_base STREQUAL "lib")
                    set(CMAKE_AR "${_solstice_lib_try}" CACHE FILEPATH "MSVC lib.exe (avoid GNU/llvm ar on PATH)" FORCE)
                endif()
                break()
            endif()
        endforeach()
    endif()
endif()

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
# CPM source cache: do not set if already in Cache (e.g. -D, CMake preset). Otherwise:
#   ENV{CPM_SOURCE_CACHE} (CI / shared user cache) else <source tree>/.cpm-cache
# Example: export CPM_SOURCE_CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/solstice/cpm"
if(NOT CPM_SOURCE_CACHE)
    if(DEFINED ENV{CPM_SOURCE_CACHE} AND NOT "$ENV{CPM_SOURCE_CACHE}" STREQUAL "")
        set(CPM_SOURCE_CACHE "$ENV{CPM_SOURCE_CACHE}" CACHE PATH "CPM dependency source cache")
    else()
        set(CPM_SOURCE_CACHE "${CMAKE_SOURCE_DIR}/.cpm-cache" CACHE PATH "CPM dependency source cache")
    endif()
endif()
include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

# BGFX stack: fetch sources only; 3rdparty/cmake builds them
# Pin bx (not floating master) so MSVC thread-name patch stays byte-stable across upstream edits.
CPMAddPackage(NAME bx GITHUB_REPOSITORY bkaradzic/bx GIT_TAG c6131ec6761abf4b5b8e83a9fd0348023c86028e DOWNLOAD_ONLY YES
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

# Protobuf: required by GameNetworkingSockets for generated protocol code
# GIT_SUBMODULES "" avoids cloning third_party/googletest/benchmark; not used with BUILD_TESTS OFF
# and fixes configure when GitHub is unreachable for submodule URLs.
CPMAddPackage(NAME protobuf GITHUB_REPOSITORY protocolbuffers/protobuf GIT_TAG v3.21.12
    GIT_SHALLOW TRUE
    GIT_SUBMODULES ""
    OPTIONS
        "protobuf_BUILD_TESTS OFF"
        "protobuf_BUILD_CONFORMANCE OFF"
        "protobuf_BUILD_EXAMPLES OFF"
        "protobuf_INSTALL ON"
        "protobuf_MSVC_STATIC_RUNTIME OFF"
    EXCLUDE_FROM_ALL YES
)
find_package(Protobuf REQUIRED CONFIG)
if(NOT COMMAND protobuf_generate_cpp)
    function(protobuf_generate_cpp SRCS HDRS)
        if(NOT TARGET protobuf::protoc)
            message(FATAL_ERROR "protobuf::protoc target is required for protobuf_generate_cpp compatibility")
        endif()

        set(_protobuf_generated_srcs)
        set(_protobuf_generated_hdrs)
        foreach(_protobuf_proto IN LISTS ARGN)
            get_filename_component(_protobuf_abs "${_protobuf_proto}" ABSOLUTE)
            get_filename_component(_protobuf_proto_dir "${_protobuf_abs}" DIRECTORY)
            get_filename_component(_protobuf_filename "${_protobuf_abs}" NAME)
            get_filename_component(_protobuf_name "${_protobuf_abs}" NAME_WE)
            set(_protobuf_cc "${CMAKE_CURRENT_BINARY_DIR}/${_protobuf_name}.pb.cc")
            set(_protobuf_h "${CMAKE_CURRENT_BINARY_DIR}/${_protobuf_name}.pb.h")
            add_custom_command(
                OUTPUT "${_protobuf_cc}" "${_protobuf_h}"
                COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}"
                COMMAND "$<TARGET_FILE:protobuf::protoc>"
                ARGS
                    "--cpp_out=${CMAKE_CURRENT_BINARY_DIR}"
                    "-I" "${_protobuf_proto_dir}"
                    "${_protobuf_filename}"
                DEPENDS "${_protobuf_abs}" protobuf::protoc
                WORKING_DIRECTORY "${_protobuf_proto_dir}"
                VERBATIM
            )
            list(APPEND _protobuf_generated_srcs "${_protobuf_cc}")
            list(APPEND _protobuf_generated_hdrs "${_protobuf_h}")
        endforeach()

        set(${SRCS} "${_protobuf_generated_srcs}" PARENT_SCOPE)
        set(${HDRS} "${_protobuf_generated_hdrs}" PARENT_SCOPE)
    endfunction()
endif()
if(WIN32)
    set(SOLSTICE_GNS_CRYPTO "BCrypt")
    set(SOLSTICE_GNS_CRYPTO25519 "Reference")
else()
    set(SOLSTICE_GNS_CRYPTO "OpenSSL")
    set(SOLSTICE_GNS_CRYPTO25519 "OpenSSL")
    find_package(OpenSSL REQUIRED)
endif()
set(Protobuf_USE_STATIC_LIBS ON CACHE BOOL "Force static protobuf linkage for GameNetworkingSockets" FORCE)

CPMAddPackage(NAME GameNetworkingSockets GITHUB_REPOSITORY ValveSoftware/GameNetworkingSockets GIT_TAG v1.4.1
    OPTIONS
        "BUILD_STATIC_LIB ON"
        "BUILD_SHARED_LIB OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_TESTS OFF"
        "BUILD_TOOLS OFF"
        "USE_CRYPTO ${SOLSTICE_GNS_CRYPTO}"
        "USE_CRYPTO25519 ${SOLSTICE_GNS_CRYPTO25519}"
        "Protobuf_USE_STATIC_LIBS ON"
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
# CUSTOM_CACHE_KEY: bump when the PATCHES file changes so CPM re-fetches/applies (otherwise a hit
# in CPM_SOURCE_CACHE skips PATCH_COMMAND and leaves stale unpatched sources).
CPMAddPackage(NAME reactphysics3d GITHUB_REPOSITORY DanielChappuis/reactphysics3d GIT_TAG 4bbbaa7c6e92942734eec696e23a2fad1f1cb8a1
    CUSTOM_CACHE_KEY chrono-includes-v2
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

# Phased CI builds: cmake --build build --target solstice_deps (then SolsticeEngine, tests, utilities).
# Splits heavy third-party compiles from the engine to lower peak RAM on memory-constrained Linux runners.
add_custom_target(solstice_deps COMMENT "Third-party libraries (build first in CI)")
foreach(
    _sd IN
    ITEMS bx
          bimg
          bimg_decode
          bgfx
          shaderc
          imgui
          SDL3-shared
          libzstd_static
)
    if(TARGET ${_sd})
        add_dependencies(solstice_deps ${_sd})
    endif()
endforeach()
if(TARGET ReactPhysics3D::reactphysics3d)
    add_dependencies(solstice_deps ReactPhysics3D::reactphysics3d)
elseif(TARGET reactphysics3d)
    add_dependencies(solstice_deps reactphysics3d)
endif()
if(TARGET GameNetworkingSockets::static)
    add_dependencies(solstice_deps GameNetworkingSockets::static)
endif()
if(TARGET SDL3_mixer-static)
    add_dependencies(solstice_deps SDL3_mixer-static)
elseif(TARGET SDL3_mixer::SDL3_mixer-static)
    add_dependencies(solstice_deps SDL3_mixer::SDL3_mixer-static)
endif()
if(TARGET protobuf::libprotobuf)
    add_dependencies(solstice_deps protobuf::libprotobuf)
elseif(TARGET libprotobuf)
    add_dependencies(solstice_deps libprotobuf)
endif()

# LibSmf — Solstice Map Format (.smf) v1 I/O (SolsticeEngine V1 validate API; LibParallax uses shared wire codec)
add_subdirectory(SDK/LibSmf)

# LibNativeLoad — UTF-8 native module load (shared by Plugin + UtilityPluginHost)
add_subdirectory(SDK/LibNativeLoad)

# LibFileWatch — debounced polling file watcher for authoring tools
add_subdirectory(SDK/LibFileWatch)

# LibSettingsStore — shared JSON settings files for authoring tools
add_subdirectory(SDK/LibSettingsStore)

# FFmpeg (optional): libav for file video decode in source/Multimedia (FindFFmpeg from SDL3 cmake tree).
# Use FindFFmpeg.cmake from SDL3 as MODULE, not a stray FFmpegConfig.cmake on CPATH/Prefix:
# the latter can set FFmpeg_FOUND without creating FFmpeg::SDL::SWSCALE, breaking Multimedia.
option(SOLSTICE_WITH_FFMPEG "Build FFmpeg-backed video decode (Multimedia)" ON)
set(SOLSTICE_HAVE_FFMPEG 0)
if(SOLSTICE_WITH_FFMPEG)
    list(INSERT CMAKE_MODULE_PATH 0 "${SDL3_SOURCE_DIR}/cmake")
    set(_SolsticePrevFFmpegComponents "${FFmpeg_FIND_COMPONENTS}")
    set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
    find_package(FFmpeg MODULE QUIET)
    set(FFmpeg_FIND_COMPONENTS "${_SolsticePrevFFmpegComponents}")
    unset(_SolsticePrevFFmpegComponents)
    # Config-mode FFmpeg packages can leave FFmpeg_FOUND set without our imported targets; avoid half-enabled builds.
    if(FFmpeg_FOUND)
        foreach(_solstice_ffmpeg_c IN ITEMS SWSCALE AVUTIL AVFORMAT AVCODEC)
            if(NOT TARGET "FFmpeg::SDL::${_solstice_ffmpeg_c}")
                set(FFmpeg_FOUND FALSE)
                break()
            endif()
        endforeach()
        unset(_solstice_ffmpeg_c)
    endif()
    if(FFmpeg_FOUND)
        set(SOLSTICE_HAVE_FFMPEG 1)
        message(STATUS "FFmpeg: Multimedia video decode enabled")
    else()
        message(STATUS "FFmpeg (libav) not found or incomplete — Multimedia video decode disabled")
    endif()
endif()

# Engine source: per-directory static libs + SolsticeEngine SHARED (see source/CMakeLists.txt)
add_subdirectory(source)

# Optional: small tools (e.g. regenerate example/JhSmfProject for Jackhammer)
option(SOLSTICE_BUILD_JH_SMF_DEMO_GEN "Build JhSmfProjectGen (Jackhammer demo folder)" OFF)
if(SOLSTICE_BUILD_JH_SMF_DEMO_GEN)
    add_subdirectory(tools)
endif()

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
# Outputs are listed explicitly so CMake only runs the step when a .sc / varying / shaderc / script
# is newer than a produced .bin, or a .bin is missing (add_custom_target alone always ran every build).
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include("${CMAKE_SOURCE_DIR}/cmake/PackageUtilities.cmake")

file(GLOB SOLSTICE_SHADER_SC CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/Shaders/*.sc")
file(GLOB SOLSTICE_SHADER_VARYING CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/Shaders/varying*.def.sc")
set(SOLSTICE_COMPILE_SHADERS_PY "${CMAKE_SOURCE_DIR}/tools/compile_shaders.py")

# Expected .bin paths — rules must stay aligned with tools/compile_shaders.py (vs_/fs_/cs_ only; skip varyings).
set(SOLSTICE_SHADER_BIN_OUTPUTS "")
foreach(_sc IN LISTS SOLSTICE_SHADER_SC)
    get_filename_component(_stem ${_sc} NAME_WE)
    string(TOLOWER "${_stem}" _lower)
    if(_lower MATCHES "varying")
        continue()
    endif()
    if(NOT (_lower MATCHES "^vs_" OR _lower MATCHES "^fs_" OR _lower MATCHES "^cs_"))
        continue()
    endif()
    list(APPEND SOLSTICE_SHADER_BIN_OUTPUTS "${CMAKE_SOURCE_DIR}/source/Shaders/bin/${_stem}.bin")
endforeach()

if(NOT SOLSTICE_SHADER_BIN_OUTPUTS)
    message(FATAL_ERROR "No BGFX shader outputs listed (expected vs_/fs_/cs_ *.sc under source/Shaders).")
endif()

add_custom_command(
    OUTPUT ${SOLSTICE_SHADER_BIN_OUTPUTS}
    COMMAND ${Python3_EXECUTABLE} ${SOLSTICE_COMPILE_SHADERS_PY}
        --shaderc $<TARGET_FILE:shaderc>
        --input-dir ${CMAKE_SOURCE_DIR}/source/Shaders
        --output-dir ${CMAKE_SOURCE_DIR}/source/Shaders/bin
        --bgfx-include ${bgfx_SOURCE_DIR}/src
    DEPENDS
        shaderc
        ${SOLSTICE_SHADER_SC}
        ${SOLSTICE_SHADER_VARYING}
        ${SOLSTICE_COMPILE_SHADERS_PY}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Compiling BGFX shaders (stale outputs only)..."
    VERBATIM
)

add_custom_target(CompileShaders DEPENDS ${SOLSTICE_SHADER_BIN_OUTPUTS})

# Ensure shaders are compiled before the engine is built
add_dependencies(SolsticeEngine CompileShaders)

# ============================================================================
# Helper function to copy shaders to executable directory
# ============================================================================
# Usage: copy_shaders_to_target(<target_name>)
# Copies compiled shaders from source/Shaders/bin/ to <target>'s output directory/shaders/
function(copy_shaders_to_target target_name)
    # Ensure shader compilation runs before any target that consumes copied shader binaries.
    if(TARGET CompileShaders)
        add_dependencies(${target_name} CompileShaders)
    endif()

    # copy_directory fails if the source path does not exist; create it first.
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_SOURCE_DIR}/source/Shaders/bin
        COMMENT "Ensuring source shader directory exists for ${target_name}"
    )

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
# LibParallax (tests link this; must be configured before add_subdirectory(tests))
# ============================================================================
add_subdirectory(SDK/LibParallax)

# ============================================================================
# Tests (CTest; targets + labels live in tests/CMakeLists.txt)
# ============================================================================
enable_testing()
add_subdirectory(tests)

# ============================================================================
# Utilities
# ============================================================================
if(SOLSTICE_BUILD_UTILITIES)
    add_subdirectory(utilities/LibUI)
    add_subdirectory(utilities/EditorEnginePreview)
    add_subdirectory(utilities/UtilityPluginHost)
    add_subdirectory(utilities/LibEditorAudio)
    add_subdirectory(utilities/Sharpon)
    add_subdirectory(utilities/MovieMaker)
    add_subdirectory(utilities/LevelEditor)
    add_subdirectory(utilities/Refulgent)

    add_custom_target(solstice_utilities COMMENT "Utilities (LibUI, editor tools, …)")
    foreach(_su IN ITEMS LibUI EditorEnginePreview UtilityPluginHost LibEditorAudio Sharpon MovieMaker LevelEditor Refulgent)
        if(TARGET ${_su})
            add_dependencies(solstice_utilities ${_su})
        endif()
    endforeach()
endif()

# ============================================================================
# Examples
# ============================================================================
option(SOLSTICE_BUILD_EXAMPLES "Build example games under example/" ON)
if(SOLSTICE_BUILD_EXAMPLES)
    add_subdirectory(example/PhysicsPlayground)
    add_subdirectory(example/Blizzard)
    add_subdirectory(example/Hyperbourne)
    add_subdirectory(example/Avalanche)
    add_subdirectory(example/Disco)
    add_subdirectory(example/ThermalPlume)
    add_subdirectory(example/MotionFusion)
endif()
