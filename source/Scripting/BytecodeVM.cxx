#include "BytecodeVM.hxx"
#include <iostream>
#include <stdexcept>

namespace Solstice::Scripting {

BytecodeVM::BytecodeVM() {
    m_Registers.fill(0);
}

void BytecodeVM::LoadProgram(const Program& prog) {
    m_Program = prog;
    m_IP = 0;
    m_Stack.clear();
    m_CallStack.clear();
}

void BytecodeVM::RegisterNative(const std::string& name, NativeFunc func) {
    m_Natives[name] = func;
}

void BytecodeVM::Push(Value v) {
    m_Stack.push_back(v);
}

Value BytecodeVM::Pop() {
    if (m_Stack.empty()) throw std::runtime_error("Stack underflow");
    Value v = m_Stack.back();
    m_Stack.pop_back();
    return v;
}

Value BytecodeVM::Peek(size_t offset) {
    if (m_Stack.size() <= offset) throw std::runtime_error("Stack underflow");
    return m_Stack[m_Stack.size() - 1 - offset];
}

void BytecodeVM::Run() {
    bool running = true;
    while (running && m_IP < m_Program.Instructions.size()) {
        const auto& inst = m_Program.Instructions[m_IP];
        m_IP++;

        switch (inst.Op) {
            case OpCode::NOP: break;
            case OpCode::HALT: running = false; break;
            
            case OpCode::PUSH_CONST: Push(inst.Operand); break;
            case OpCode::POP: Pop(); break;
            case OpCode::DUP: Push(Peek()); break;

            case OpCode::MOV_REG: {
                if (inst.RegisterIndex < m_Registers.size()) {
                    m_Registers[inst.RegisterIndex] = Pop();
                }
                break;
            }
            case OpCode::LOAD_REG: {
                if (inst.RegisterIndex < m_Registers.size()) {
                    Push(m_Registers[inst.RegisterIndex]);
                }
                break;
            }

            case OpCode::ADD: {
                Value b = Pop();
                Value a = Pop();
                if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                    Push(std::get<int64_t>(a) + std::get<int64_t>(b));
                } else {
                    // String concat
                    std::string sa = std::holds_alternative<std::string>(a) ? std::get<std::string>(a) : std::to_string(std::get<int64_t>(a));
                    std::string sb = std::holds_alternative<std::string>(b) ? std::get<std::string>(b) : std::to_string(std::get<int64_t>(b));
                    Push(sa + sb);
                }
                break;
            }
            
            case OpCode::INC: {
                Value a = Pop();
                if (std::holds_alternative<int64_t>(a)) {
                    Push(std::get<int64_t>(a) + 1);
                } else {
                    Push(a);
                }
                break;
            }

            case OpCode::PRINT: {
                Value v = Pop();
                if (std::holds_alternative<int64_t>(v)) std::cout << std::get<int64_t>(v) << "\n";
                else if (std::holds_alternative<std::string>(v)) std::cout << std::get<std::string>(v) << "\n";
                break;
            }

            case OpCode::CALL: {
                if (std::holds_alternative<std::string>(inst.Operand)) {
                    std::string name = std::get<std::string>(inst.Operand);
                    // Argument count is on top of stack
                    Value cntVal = Pop();
                    if (!std::holds_alternative<int64_t>(cntVal)) {
                        throw std::runtime_error("CALL argument count not int");
                    }
                    int64_t argCount = std::get<int64_t>(cntVal);
                    std::vector<Value> args;
                    args.reserve(argCount);
                    for (int64_t i = 0; i < argCount; ++i) {
                        args.emplace_back(Pop());
                    }
                    std::reverse(args.begin(), args.end());
                    if (m_Natives.count(name)) {
                        Value ret = m_Natives[name](args);
                        if (ret.index() != 0 || std::holds_alternative<std::string>(ret) || std::holds_alternative<double>(ret)) {
                            Push(ret);
                        }
                    } else {
                        throw std::runtime_error("Unknown native function: " + name);
                    }
                } else if (std::holds_alternative<int64_t>(inst.Operand)) {
                    m_CallStack.push_back(m_IP);
                    m_IP = (size_t)std::get<int64_t>(inst.Operand);
                }
                break;
            }
            
            case OpCode::RET: {
                if (m_CallStack.empty()) {
                    running = false;
                } else {
                    m_IP = m_CallStack.back();
                    m_CallStack.pop_back();
                }
                break;
            }
            
            default: break;
        }
    }
}

}
