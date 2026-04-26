cmake_minimum_required(VERSION 3.20)
project(Sharpon)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create the executable
add_executable(Sharpon
    "Main.cxx"
    "PluginLoader.cxx"
    "SharponConsole.cxx"
    "SharponEmbeddedShell.cxx"
    "SharponProfiler.cxx"
)

target_include_directories(Sharpon PRIVATE "${CMAKE_SOURCE_DIR}/SDK" "${CMAKE_SOURCE_DIR}/utilities")

# Link against LibUI and OpenGL
target_link_libraries(Sharpon PRIVATE LibUI LibFileWatch LibSettingsStore LibEditorAudio UtilityPluginHost)

# Link OpenGL on Windows
if(WIN32)
    target_link_libraries(Sharpon PRIVATE opengl32 shell32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(Sharpon PRIVATE OpenGL::GL)
    find_library(SHARPON_UTIL_LIB util)
    if(SHARPON_UTIL_LIB)
        target_link_libraries(Sharpon PRIVATE ${SHARPON_UTIL_LIB})
    endif()
endif()

# Set C++ standard
set_target_properties(Sharpon PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy SDL3.dll
    add_custom_command(TARGET Sharpon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:Sharpon>
        COMMENT "Copying SDL3.dll to executable directory"
    )

    # Copy SolsticeEngine.dll (if it exists)
    add_custom_command(TARGET Sharpon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SolsticeEngine>
        $<TARGET_FILE_DIR:Sharpon>
        COMMENT "Copying SolsticeEngine.dll to executable directory"
    )
endif()

add_custom_command(TARGET Sharpon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Sharpon>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/CascadiaCode-Regular.ttf"
        "$<TARGET_FILE_DIR:Sharpon>/fonts/CascadiaCode-Regular.ttf"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/fonts/Phosphor.ttf"
        "$<TARGET_FILE_DIR:Sharpon>/fonts/Phosphor.ttf"
    COMMENT "Copy bundled UI fonts next to Sharpon"
)

solstice_utility_postbuild_package(Sharpon "tools/packaging/Sharpon.fst")
