cmake_minimum_required(VERSION 3.20)
project(Sharpon)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create the executable
add_executable(Sharpon
    "Main.cxx"
)

# Link against LibUI and OpenGL
target_link_libraries(Sharpon PRIVATE LibUI)

# Link OpenGL on Windows
if(WIN32)
    target_link_libraries(Sharpon PRIVATE opengl32)
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(Sharpon PRIVATE OpenGL::GL)
endif()

# Set C++ standard
set_target_properties(Sharpon PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy DLLs to the executable directory on Windows
if(WIN32)
    # Copy LibUI.dll
    add_custom_command(TARGET Sharpon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:LibUI>
        $<TARGET_FILE_DIR:Sharpon>
        COMMENT "Copying LibUI.dll to executable directory"
    )

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

