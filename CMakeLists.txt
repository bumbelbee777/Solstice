cmake_minimum_required(VERSION 3.20)
project(Solstice VERSION 0.1.0 LANGUAGES CXX)

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

# Build Solstice as a shared library (DLL on Windows)
add_library(SolsticeEngine SHARED
    "source/Init.cxx"
    "source/Core/Debug.cxx"
    "source/Core/Audio.cxx"
    "source/Core/WavefrontParser.cxx"
    "source/Core/Cache.cxx"
    "source/Core/MLP.cxx"
    "source/Core/CNN.cxx"
    "source/Core/NavMesh.cxx"
    "source/Core/FSM.cxx"
    "source/Core/AssetStream.cxx"
    "source/Core/Save.cxx"
    "source/Core/JSON.cxx"
    "source/Core/Base64.cxx"
    "source/Core/Material.cxx"
    "source/Core/AssetLoader.cxx"
    "source/Core/Profiler.cxx"
    "source/Core/ScopeTimer.cxx"
    "source/Arzachel/Seed.cxx"
    "source/Arzachel/Generator.cxx"
    "source/Arzachel/MeshData.cxx"
    "source/Arzachel/GeometryOps.cxx"
    "source/Arzachel/Constraints.cxx"
    "source/Arzachel/AssetBuilder.cxx"
    "source/Arzachel/AssetPresets.cxx"
    "source/Arzachel/MaterialPresets.cxx"
    "source/Arzachel/Polyhedra.cxx"
    "source/Arzachel/Skeleton.cxx"
    "source/Arzachel/SkinWeights.cxx"
    "source/Arzachel/RiggedMesh.cxx"
    "source/Arzachel/RiggingOps.cxx"
    "source/Arzachel/RiggedPrimitives.cxx"
    "source/Arzachel/MeshFactory.cxx"
    "source/Arzachel/ProceduralTexture.cxx"
    "source/Arzachel/MaterialSerializer.cxx"
    "source/Arzachel/Keyframe.cxx"
    "source/Arzachel/AnimationTrack.cxx"
    "source/Arzachel/AnimationClip.cxx"
    "source/Arzachel/AnimationOps.cxx"
    "source/Arzachel/GLTFImport.cxx"
    "source/Arzachel/GLTFExport.cxx"
    "source/Arzachel/TerrainGenerator.cxx"
    "source/Arzachel/StructureGenerator.cxx"
    "source/Arzachel/CharacterPrimitives.cxx"
    "source/Arzachel/MapSerializer.cxx"
    "source/Arzachel/Presets/Accessories/AccessoryPresets.cxx"
    "source/Arzachel/Presets/Weapons/WeaponPresets.cxx"
    "source/Arzachel/Presets/Bullets/BulletPresets.cxx"
    "source/Arzachel/Presets/Furniture/FurniturePresets.cxx"
    "source/Arzachel/Presets/Industrial/IndustrialPresets.cxx"
    "source/Arzachel/Presets/Architecture/ArchitecturePresets.cxx"
    "source/Arzachel/Presets/Characters/CharacterPresets.cxx"
    "source/Arzachel/Presets/LevelCompositions.cxx"
    "source/Game/GameBase.cxx"
    "source/Game/InputManager.cxx"
    "source/Game/SceneManager.cxx"
    "source/Game/ComponentFactory.cxx"
    "source/Game/GameState.cxx"
    "source/Game/FirstPersonController.cxx"
    "source/Game/ThirdPersonController.cxx"
    "source/Game/Health.cxx"
    "source/Game/Inventory.cxx"
    "source/Game/HUD.cxx"
    "source/Game/Enemy.cxx"
    "source/Game/Boss.cxx"
    "source/Game/LoadingScreen.cxx"
    "source/Game/PauseMenu.cxx"
    "source/Game/GamePreferences.cxx"
    "source/Game/MainMenu.cxx"
    "source/Game/Autosave.cxx"
    "source/Game/LevelSelector.cxx"
    "source/Game/Difficulty.cxx"
    "source/Game/SFXManager.cxx"
    "source/Game/Weapon.cxx"
    "source/Game/WeaponSwitcher.cxx"
    "source/Game/FPSMovement.cxx"
    "source/Game/SnowSystem.cxx"
    "source/Game/FPSGame.cxx"
    "source/Game/FPSSandbox.cxx"
    "source/Game/ThirdPersonGame.cxx"
    "source/Game/StrategyGame.cxx"
    "source/Game/LevelBase.cxx"
    "source/Game/StrategyCamera.cxx"
    "source/Game/NPC.cxx"
    "source/Game/AIBrain.cxx"
    "source/Game/Stamina.cxx"
    "source/Game/ScriptManager.cxx"
    "source/Game/ProceduralTextureManager.cxx"
    "source/Game/SceneBuilder.cxx"
    "source/Game/MaterialPresetManager.cxx"
    "source/Game/HUDPresets.cxx"
    "source/Math/Quaternion.cxx"
    "source/Physics/PhysicsSystem.cxx"
    "source/Physics/ReactPhysics3DBridge.cxx"
    "source/Physics/CollisionResolution.cxx"
    "source/Physics/IterativeSolver.cxx"
    "source/Physics/ShapeHelpers.cxx"
    "source/Physics/ContactGeneration.cxx"
    "source/Physics/ManifoldManager.cxx"
    "source/Physics/ContactClustering.cxx"
    "source/Physics/ConvexHullFactory.cxx"
    "source/Physics/GJK.cxx"
    "source/Physics/CCD.cxx"
    "source/Physics/LightSource.cxx"
    "source/Physics/Fluid.cxx"
    "source/Physics/NSSolver.cxx"
    "source/UI/Window.cxx"
    "source/UI/UISystem.cxx"
    "source/UI/Widgets.cxx"
    "source/UI/TextEffects.cxx"
    "source/UI/Transparency.cxx"
    "source/UI/ViewportUI.cxx"
    "source/UI/ImageLoader.cxx"
    "source/UI/Sprite.cxx"
    "source/UI/Animation.cxx"
    "source/UI/Transition.cxx"
    "source/UI/Primitives.cxx"
    "source/UI/VisualEffects.cxx"
    "source/UI/MotionGraphics.cxx"
    "source/Render/Camera.cxx"
    "source/Render/Context.cxx"
    "source/Render/Framebuffer.cxx"
    "source/Render/FramebufferRenderTarget.cxx"
    "source/Render/SoftwareRenderer.cxx"
    "source/Render/PostProcessing.cxx"
    "source/Render/Raytracing.cxx"
    "source/Render/VolumetricLighting.cxx"
    "source/Render/Skybox.cxx"
    "source/Render/ShaderLoader.cxx"
    "source/Render/ParticleSystem.cxx"
    "source/Render/ParticlePresets.cxx"
    "source/Render/TextureRegistry.cxx"
    "source/Render/RenderPipeline.cxx"
    "source/Render/SceneRenderer.cxx"
    "source/Render/ShadowRenderer.cxx"
    "source/Render/FramebufferManager.cxx"
    "source/Render/HDRProcessor.cxx"
    "source/Render/ShaderUniforms.cxx"
    "source/Scripting/BytecodeVM.cxx"
    "source/Scripting/Compiler.cxx"
    "source/Scripting/ScriptBindings.cxx"
    "source/Scripting/VMError.cxx"
    "source/Scripting/JIT.cxx"
    "source/Scripting/Backend/IBackend.cxx"
    "source/Scripting/Backend/Backend_x64.cxx"
    "source/Scripting/Backend/Backend_ARM.cxx"
    "source/Scripting/Backend/Backend_RISCV.cxx"
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
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/tinygltf
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bimg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/bx/include
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
    # Define git commit hash
    target_compile_definitions(SolsticeEngine PRIVATE SOLSTICE_BUILD_GIT_COMMIT="${GIT_COMMIT_HASH}")
else()
    # For non-MSVC compilers, also define git commit hash
    target_compile_definitions(SolsticeEngine PRIVATE SOLSTICE_BUILD_GIT_COMMIT="${GIT_COMMIT_HASH}")
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
    bimg
    bimg_decode
    bx
    imgui
    SDL3_mixer-static
    tinygltf
    ReactPhysics3D::reactphysics3d
)

# ============================================================================
# Shader Compilation
# ============================================================================
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Collect shader source files for dependency tracking
# CONFIGURE_DEPENDS forces CMake to re-check for new/removed files on each build
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/source/Shaders/*.sc")

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
# Examples
# ============================================================================
add_subdirectory(example/PhysicsPlayground)
add_subdirectory(example/Blizzard)
add_subdirectory(example/Hyperbourne)
add_subdirectory(example/Avalanche)
add_subdirectory(example/Disco)
