#include "Scripting/VM/BytecodeVM.hxx"
#include "Scripting/JIT/JIT.hxx"
#include "Scripting/Bindings/NativeBinding.hxx"
#include <cstdio>
#include <vector>

namespace {

int Fail(int code, const char* message) {
    std::fprintf(stderr, "[MoonwalkInteropTest] FAIL (%d): %s\n", code, message);
    return code;
}

struct Counter {
    int64_t Value{10};
    int64_t Add(int64_t amount) {
        Value += amount;
        return Value;
    }
};

} // namespace

int main() {
    using namespace Solstice::Scripting;

    BytecodeVM vm;
    Counter counter;

    NativeBinding::Register<int64_t, int64_t, int64_t>(vm, "AddInts", [](int64_t a, int64_t b) {
        return a + b;
    });
    vm.RegisterNative("Counter.Add", NativeBinding::BindMethod(&counter, &Counter::Add));

    Program nativeProgram;
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(7));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(5));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(2));
    nativeProgram.Add(OpCode::CALL, std::string("AddInts"));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(3));
    nativeProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(1));
    nativeProgram.Add(OpCode::CALL, std::string("Counter.Add"));
    nativeProgram.Add(OpCode::HALT);
    vm.LoadProgram(nativeProgram);
    vm.Run();

    const Value counterResult = vm.Pop();
    const Value addResult = vm.Pop();
    if (!std::holds_alternative<int64_t>(addResult) || std::get<int64_t>(addResult) != 12) {
        return Fail(1, "Typed native add binding should return 12");
    }
    if (!std::holds_alternative<int64_t>(counterResult) || std::get<int64_t>(counterResult) != 13) {
        return Fail(2, "Bound object method should update object state");
    }

    Program jitProgram;
    jitProgram.Add(OpCode::PUSH_CONST, static_cast<int64_t>(3));   // 0
    jitProgram.Add(OpCode::CALL, static_cast<int64_t>(4));         // 1
    jitProgram.Add(OpCode::HALT);                                  // 2
    jitProgram.Add(OpCode::NOP);                                   // 3
    jitProgram.Add(OpCode::INC);                                   // 4
    jitProgram.Add(OpCode::RET);                                   // 5
    jitProgram.FunctionArity[4] = 1;

    vm.LoadProgram(jitProgram);
    vm.EnableJIT();
    if (!vm.IsJITEnabled()) {
        return Fail(3, "JIT should be enabled on supported architecture");
    }
    vm.Run();
    Value jitResult = vm.Pop();
    if (!std::holds_alternative<int64_t>(jitResult) || std::get<int64_t>(jitResult) != 4) {
        return Fail(4, "Script call should return incremented value");
    }

    const JITStats stats = vm.GetJITStats();
    if (stats.FunctionCalls == 0) {
        return Fail(5, "JIT stats should record script function calls");
    }

    {
        Program leaf;
        leaf.Add(OpCode::PUSH_CONST, static_cast<int64_t>(11));
        leaf.Add(OpCode::PUSH_CONST, static_cast<int64_t>(31));
        leaf.Add(OpCode::ADD);
        leaf.Add(OpCode::RET);
        JIT jit;
        jit.Enable();
        if (jit.IsEnabled()) {
            BytecodeVM vmLeaf;
            for (int i = 0; i < 100; ++i) {
                jit.RecordFunctionCall(0);
            }
            auto compiled = jit.CompileHotFunction(&vmLeaf, leaf, 0);
            if (!compiled) {
                return Fail(6, "JIT should compile trivial leaf function when backend is active");
            }
            Value v = compiled(std::vector<Value>{});
            if (!std::holds_alternative<int64_t>(v) || std::get<int64_t>(v) != 42) {
                return Fail(7, "JIT leaf should return folded int constant");
            }
        }
    }

    std::printf("[MoonwalkInteropTest] PASS\n");
    return 0;
}
