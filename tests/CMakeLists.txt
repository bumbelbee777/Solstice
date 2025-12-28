# Thread Safety Test
add_executable(ThreadSafetyTest
    ThreadSafetyTest.cxx
)

target_link_libraries(ThreadSafetyTest
    SolsticeEngine
)

# Set output directory
set_target_properties(ThreadSafetyTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Thread sanitizer support (if available)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(ENABLE_THREAD_SANITIZER "Enable ThreadSanitizer for thread safety tests" OFF)
    if(ENABLE_THREAD_SANITIZER)
        target_compile_options(ThreadSafetyTest PRIVATE
            -fsanitize=thread
            -g
        )
        target_link_options(ThreadSafetyTest PRIVATE
            -fsanitize=thread
        )
        message(STATUS "ThreadSanitizer enabled for ThreadSafetyTest")
    endif()
endif()

# MSVC static analysis (if available)
if(MSVC)
    target_compile_options(ThreadSafetyTest PRIVATE
        /analyze
    )
endif()

# Math Safety Test
add_executable(MathSafetyTest
    MathSafetyTest.cxx
)
target_link_libraries(MathSafetyTest
    SolsticeEngine
)
set_target_properties(MathSafetyTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Job System Stress Test
add_executable(JobSystemStressTest
    JobSystemStressTest.cxx
)
target_link_libraries(JobSystemStressTest
    SolsticeEngine
)
set_target_properties(JobSystemStressTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
