# Avalanche (Physics Stress Demo)
add_executable(Avalanche
    Main.cxx
)

target_link_libraries(Avalanche
    SolsticeEngine
)

# Set output directory
set_target_properties(Avalanche PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${SOLSTICE_EXECUTABLE_OUTPUT_DIR}"
)

# MSVC static analysis (if available)
if(MSVC)
    target_compile_options(Avalanche PRIVATE
        /analyze
    )
endif()
