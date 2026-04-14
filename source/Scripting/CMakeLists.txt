# Scripting: bytecode VM, compiler, JIT, bindings
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
add_library(Scripting STATIC
    VM/BytecodeVM.cxx
    VM/BytecodeVMExecute.cxx
    VM/ProgramSerialize.cxx
    VM/VMError.cxx
    Runtime/PtrRuntime.cxx
    Analysis/MemoryAnalysis.cxx
    Analysis/StaticTypes.cxx
    Compiler/Compiler.cxx
    Compiler/Parser.cxx
    Compiler/Optimize.cxx
    Bindings/ScriptBindings.cxx
    JIT/JIT.cxx
    JIT/Backend/IBackend.cxx
    JIT/Backend/Backend_x64.cxx
    JIT/Backend/Backend_ARM.cxx
    JIT/Backend/Backend_RISCV.cxx
)
target_include_directories(Scripting PUBLIC ${SOURCE_DIR})
target_link_libraries(Scripting PUBLIC Core Math Physics)
if(MSVC)
    target_compile_options(Scripting PRIVATE /Zc:preprocessor /W0 /FS)
    target_compile_definitions(Scripting PRIVATE SOLSTICE_EXPORTS _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
