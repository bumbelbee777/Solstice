#include "TestHarness.hxx"
#include "Scripting/VM/BytecodeVM.hxx"
#include "Scripting/Bindings/NativeBinding.hxx"

#include <cstdlib>
#include <cstring>

namespace {

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

int EnvInt(const char* name, int defaultValue) {
    const char* e = std::getenv(name);
    if (!e || e[0] == '\0') {
        return defaultValue;
    }
    char* end = nullptr;
    const long v = std::strtol(e, &end, 10);
    if (end == e || v < 1) {
        return defaultValue;
    }
    return static_cast<int>(v);
}

bool RunScriptStress() {
    using namespace Solstice::Scripting;

    const bool torture = EnvFlag("SOLSTICE_STRESS_TORTURE");
    const int outer = EnvInt("SOLSTICE_SCRIPT_STRESS_ROUNDS", torture ? 800 : 200);

    BytecodeVM vm;
    NativeBinding::Register<int64_t, int64_t, int64_t>(vm, "AddInts", [](int64_t a, int64_t b) {
        return a + b;
    });

    Program nativeProgram;
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(7));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(5));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(2));
    nativeProgram.Add(OpCode::CALL, std::string("AddInts"));
    nativeProgram.Add(OpCode::HALT);

    Program jitProgram;
    jitProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(3));
    jitProgram.Add(OpCode::CALL, static_cast<int64_t>(4));
    jitProgram.Add(OpCode::HALT);
    jitProgram.Add(OpCode::NOP);
    jitProgram.Add(OpCode::INC);
    jitProgram.Add(OpCode::RET);
    jitProgram.FunctionArity[4] = 1;

    for (int i = 0; i < outer; ++i) {
        vm.LoadProgram(nativeProgram);
        vm.Run();
        const Value r1 = vm.Pop();
        SOLSTICE_TEST_ASSERT(std::holds_alternative<int64_t>(r1) && std::get<int64_t>(r1) == 12, "native add");

        vm.LoadProgram(jitProgram);
        vm.EnableJIT();
        vm.Run();
        const Value r2 = vm.Pop();
        SOLSTICE_TEST_ASSERT(std::holds_alternative<int64_t>(r2) && std::get<int64_t>(r2) == 4, "jit inc");
    }

    SOLSTICE_TEST_PASS("Script stress rounds");
    return true;
}

} // namespace

int main() {
    if (!RunScriptStress()) {
        return 1;
    }
    return SolsticeTestMainResult("ScriptStressTest");
}
