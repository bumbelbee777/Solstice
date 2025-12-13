#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <variant>
#include <string>
#include <functional>
#include <unordered_map>
#include "../Solstice.hxx"

namespace Solstice::Scripting {

    enum class OpCode : uint8_t {
        NOP,
        
        // Stack Operations
        PUSH_CONST, // Push immediate value to stack
        POP,        // Pop from stack
        DUP,        // Duplicate top of stack

        // Register Operations
        MOV_REG,    // Move value from stack to register
        LOAD_REG,   // Push value from register to stack

        // Arithmetic
        ADD,
        SUB,
        MUL,
        DIV,
        INC,        // Increment top of stack or register

        // Logic
        EQ,
        NEQ,
        LT,
        GT,

        // Control Flow
        JMP,
        JMP_IF,     // Jump if top of stack is true
        CALL,       // Call script function (int operand) or native (string operand)
        RET,

        // Native / IO
        PRINT,      // Temporary for WriteLn
        
        HALT
    };

    using Value = std::variant<int64_t, double, std::string>;

    struct Instruction {
        OpCode Op;
        uint8_t RegisterIndex = 0; // For register ops
        Value Operand;             // For PUSH_CONST, JMP targets (as int), etc.
    };

    struct Program {
        std::vector<Instruction> Instructions;
        // Helper to add instruction
        void Add(OpCode op, Value operand = 0) {
            Instructions.push_back({op, 0, operand});
        }
        
        void AddReg(OpCode op, uint8_t reg) {
            Instructions.push_back({op, reg, 0});
        }
    };

    class SOLSTICE_API BytecodeVM {
    public:
        BytecodeVM();
        
        void LoadProgram(const Program& prog);
        void Run();
        
        // Native function registration
        using NativeFunc = std::function<Value(const std::vector<Value>& args)>;
        void RegisterNative(const std::string& name, NativeFunc func);

        void Push(Value v);
        Value Pop();
        
        // Helper to peek
        Value Peek(size_t offset = 0);

    private:
        Program m_Program;
        size_t m_IP = 0; // Instruction Pointer
        
        // Stack
        std::vector<Value> m_Stack;
        
        // Registers (R0-R15)
        std::array<Value, 16> m_Registers;
        
        // Call Stack (for function calls, stores return IP)
        std::vector<size_t> m_CallStack;

        // Native functions
        std::unordered_map<std::string, NativeFunc> m_Natives;
    };

}