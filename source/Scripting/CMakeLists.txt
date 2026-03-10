# Scripting: bytecode VM, compiler, JIT, bindings
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Scripting STATIC
    BytecodeVM.cxx
    PtrRuntime.cxx
    MemoryAnalysis.cxx
    Compiler.cxx
    ScriptBindings.cxx
    VMError.cxx
    JIT.cxx
    Backend/IBackend.cxx
    Backend/Backend_x64.cxx
    Backend/Backend_ARM.cxx
    Backend/Backend_RISCV.cxx
)
target_include_directories(Scripting PUBLIC ${SOURCE_DIR})
target_link_libraries(Scripting PUBLIC Core Math Physics)
if(MSVC)
    target_compile_options(Scripting PRIVATE /Zc:preprocessor /W0)
    target_compile_definitions(Scripting PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
