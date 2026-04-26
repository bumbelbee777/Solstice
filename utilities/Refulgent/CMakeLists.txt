cmake_minimum_required(VERSION 3.20)
project(Refulgent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(RefulgentOps STATIC
    RelicOps.cxx
)
target_include_directories(RefulgentOps PUBLIC
    "${CMAKE_SOURCE_DIR}/source"
    "${CMAKE_SOURCE_DIR}/utilities/Refulgent"
)
target_link_libraries(RefulgentOps PUBLIC Core)
if(MSVC)
    target_compile_definitions(RefulgentOps PUBLIC SOLSTICE_STATIC_LINK)
endif()

add_executable(Refulgent
    Main.cxx
    RefulgentGui.cxx
    RefulgentCli.cxx
)
target_include_directories(Refulgent PRIVATE
    "${CMAKE_SOURCE_DIR}/SDK"
    "${CMAKE_SOURCE_DIR}/utilities"
    "${CMAKE_SOURCE_DIR}/utilities/Refulgent"
)
target_link_libraries(Refulgent PRIVATE LibUI RefulgentOps)
if(MSVC)
    target_compile_options(Refulgent PRIVATE /FS)
    target_link_options(Refulgent PRIVATE /SUBSYSTEM:CONSOLE)
endif()
if(WIN32)
    target_link_libraries(Refulgent PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(Refulgent PRIVATE OpenGL::GL)
endif()

set_target_properties(Refulgent PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

if(WIN32)
    add_custom_command(TARGET Refulgent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:Refulgent>
        COMMENT "Copying SDL3.dll next to Refulgent"
    )
endif()

# Bundled UI fonts (Cascadia + Phosphor for LibUI::Icons) next to the executable (all platforms)
add_custom_command(TARGET Refulgent POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Refulgent>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/CascadiaCode-Regular.ttf"
        "$<TARGET_FILE_DIR:Refulgent>/fonts/CascadiaCode-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/Phosphor.ttf"
        "$<TARGET_FILE_DIR:Refulgent>/fonts/Phosphor.ttf"
    COMMENT "Copy bundled UI fonts (and Phosphor icon pack) next to Refulgent"
)
