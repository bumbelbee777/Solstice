cmake_minimum_required(VERSION 3.20)

# LibUI - Standalone UI library for editor tools
project(LibUI VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create LibUI as a shared library (DLL on Windows)
add_library(LibUI SHARED
    Core/Core.cxx
    Widgets/Widgets.cxx
    Graphics/Graphics.cxx
    Animation/Animation.cxx
)

# Set up include directories
# Include directory is set to parent (utilities) so consumers can use "LibUI/Core/Core.hxx"
target_include_directories(LibUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>
)

# Link against ImGui and SDL3
target_link_libraries(LibUI PUBLIC
    imgui
    SDL3-shared
)

# Platform-specific configuration
if(MSVC)
    target_compile_options(LibUI PRIVATE /W0)  # Disable warnings for now
    target_compile_definitions(LibUI PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
    target_compile_definitions(LibUI PRIVATE _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS)
    target_compile_definitions(LibUI PRIVATE NOMINMAX)
    target_compile_definitions(LibUI PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(LibUI PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Define LIBUI_EXPORTS when building the DLL
    target_compile_definitions(LibUI PRIVATE LIBUI_EXPORTS)
else()
    # For non-MSVC compilers
    target_compile_options(LibUI PRIVATE -Wno-unused-parameter)
endif()

# Set C++ standard features
target_compile_features(LibUI PRIVATE cxx_std_20)

# Set target properties
set_target_properties(LibUI PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Organize files in IDE
source_group("Core" FILES Core/Core.hxx Core/Core.cxx)
source_group("Widgets" FILES Widgets/Widgets.hxx Widgets/Widgets.cxx)
source_group("Graphics" FILES Graphics/Graphics.hxx Graphics/Graphics.cxx)
source_group("Animation" FILES Animation/Animation.hxx Animation/Animation.cxx)

