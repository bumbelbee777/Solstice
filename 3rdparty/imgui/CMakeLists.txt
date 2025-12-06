cmake_minimum_required(VERSION 3.20)
project(imgui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create ImGui static library
add_library(imgui STATIC
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_sdl3.cpp
)

# Set include directories
target_include_directories(imgui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# Link SDL3 headers (required for SDL3 backend)
if(NOT TARGET SDL3::Headers)
    find_package(SDL3 REQUIRED COMPONENTS Headers)
endif()
target_link_libraries(imgui PUBLIC SDL3::Headers)

# Optional: Add compiler definitions for ImGui
# Commented out to allow access to font atlas Build() method
#target_compile_definitions(imgui PUBLIC
#    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#)

# Organize files in IDE
source_group("Source Files" FILES
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
)
source_group("Source Files\\Backends" FILES
    backends/imgui_impl_sdl3.cpp
)
source_group("Header Files" FILES
    imgui.h
    imgui_internal.h
    imconfig.h
)
source_group("Header Files\\Backends" FILES
    backends/imgui_impl_sdl3.h
)
