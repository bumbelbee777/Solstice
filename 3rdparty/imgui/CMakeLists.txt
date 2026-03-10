cmake_minimum_required(VERSION 3.20)
project(imgui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use CPM-fetched imgui source dir when set; otherwise vendored (CMAKE_CURRENT_SOURCE_DIR)
if(DEFINED imgui_SOURCE_DIR)
    set(IMGUI_ROOT "${imgui_SOURCE_DIR}")
else()
    set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Create ImGui static library
add_library(imgui STATIC
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp
)

# Set include directories
target_include_directories(imgui
    PUBLIC
        ${IMGUI_ROOT}
        ${IMGUI_ROOT}/backends
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
    ${IMGUI_ROOT}/imgui.cpp
    ${IMGUI_ROOT}/imgui_demo.cpp
    ${IMGUI_ROOT}/imgui_draw.cpp
    ${IMGUI_ROOT}/imgui_tables.cpp
    ${IMGUI_ROOT}/imgui_widgets.cpp
)
source_group("Source Files\\Backends" FILES
    ${IMGUI_ROOT}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.cpp
)
source_group("Header Files" FILES
    ${IMGUI_ROOT}/imgui.h
    ${IMGUI_ROOT}/imgui_internal.h
    ${IMGUI_ROOT}/imconfig.h
)
source_group("Header Files\\Backends" FILES
    ${IMGUI_ROOT}/backends/imgui_impl_sdl3.h
    ${IMGUI_ROOT}/backends/imgui_impl_opengl3.h
)
