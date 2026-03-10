#include "BytecodeVM.hxx"
#include "VMError.hxx"
#include "JIT.hxx"
#include "PtrRuntime.hxx"
#include "../Core/Async.hxx"
#include "../Core/Debug.hxx"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace Solstice::Scripting {

// Helper to extract int from Value (for use in VM)
namespace {
    int64_t GetIntFromValue(const Value& v) {
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v);
        if (std::holds_alternative<double>(v)) return (int64_t)std::get<double>(v);
        return 0;
    }
}

// Helper for serialization
struct ValueSerializer {
    std::vector<uint8_t>& Out;

    void operator()(int64_t v) {
        uint8_t type = 0; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(double v) {
        uint8_t type = 1; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const std::string& v) {
        uint8_t type = 2; Out.push_back(type);
        uint32_t len = (uint32_t)v.size();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.begin(), v.end());
    }
    void operator()(const Solstice::Math::Vec2& v) {
        uint8_t type = 3; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Vec3& v) {
        uint8_t type = 4; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Vec4& v) {
        uint8_t type = 5; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix2& v) {
        uint8_t type = 6; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix3& v) {
        uint8_t type = 7; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Matrix4& v) {
        uint8_t type = 8; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const Solstice::Math::Quaternion& v) {
        uint8_t type = 9; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        Out.insert(Out.end(), p, p + sizeof(v));
    }
    void operator()(const std::shared_ptr<Array>& v) { uint8_t type = 10; Out.push_back(type); }
    void operator()(const std::shared_ptr<Dictionary>& v) { uint8_t type = 11; Out.push_back(type); }
    void operator()(const std::shared_ptr<Set>& v) { uint8_t type = 12; Out.push_back(type); }
    void operator()(const EnumVal& v) {
        uint8_t type = 13; Out.push_back(type);
        uint32_t len = (uint32_t)v.enumName.size();
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.enumName.begin(), v.enumName.end());
        len = (uint32_t)v.variant.size();
        p = reinterpret_cast<const uint8_t*>(&len);
        Out.insert(Out.end(), p, p + sizeof(len));
        Out.insert(Out.end(), v.variant.begin(), v.variant.end());
        p = reinterpret_cast<const uint8_t*>(&v.discriminant);
        Out.insert(Out.end(), p, p + sizeof(v.discriminant));
    }
    void operator()(const ScriptFunc& v) {
        uint8_t type = 14; Out.push_back(type);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v.entryIP);
        Out.insert(Out.end(), p, p + sizeof(v.entryIP));
        // capture not serialized (closure state lost on load)
    }

    void operator()(const PtrValue& v) {
        // PtrValue is not persisted with its payload; we only record a type tag
        // and reconstruct a null pointer on load. This is sufficient because
        // pointer values are not expected as immediate constants in bytecode.
        uint8_t type = 15;
        Out.push_back(type);
    }
};

void Program::Serialize(std::vector<uint8_t>& out) const {
    uint32_t count = (uint32_t)Instructions.size();
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&count);
    out.insert(out.end(), p, p + sizeof(count));

    for (const auto& inst : Instructions) {
        out.push_back((uint8_t)inst.Op);
        out.push_back(inst.RegisterIndex);
        std::visit(ValueSerializer{out}, inst.Operand);
    }

    // Serialize EnumInfo
    uint32_t enumCount = (uint32_t)EnumInfo.size();
    p = reinterpret_cast<const uint8_t*>(&enumCount);
    out.insert(out.end(), p, p + sizeof(enumCount));
    for (const auto& [enumName, meta] : EnumInfo) {
        uint32_t len = (uint32_t)enumName.size();
        p = reinterpret_cast<const uint8_t*>(&len);
        out.insert(out.end(), p, p + sizeof(len));
        out.insert(out.end(), enumName.begin(), enumName.end());
        uint32_t varCount = (uint32_t)meta.variantToDiscriminant.size();
        p = reinterpret_cast<const uint8_t*>(&varCount);
        out.insert(out.end(), p, p + sizeof(varCount));
        for (const auto& [varName, disc] : meta.variantToDiscriminant) {
            len = (uint32_t)varName.size();
            p = reinterpret_cast<const uint8_t*>(&len);
            out.insert(out.end(), p, p + sizeof(len));
            out.insert(out.end(), varName.begin(), varName.end());
            p = reinterpret_cast<const uint8_t*>(&disc);
            out.insert(out.end(), p, p + sizeof(disc));
        }
    }

    // Serialize FunctionArity
    uint32_t arityCount = (uint32_t)FunctionArity.size();
    p = reinterpret_cast<const uint8_t*>(&arityCount);
    out.insert(out.end(), p, p + sizeof(arityCount));
    for (const auto& [ip, ar] : FunctionArity) {
        p = reinterpret_cast<const uint8_t*>(&ip);
        out.insert(out.end(), p, p + sizeof(ip));
        p = reinterpret_cast<const uint8_t*>(&ar);
        out.insert(out.end(), p, p + sizeof(ar));
    }
}

Program Program::Deserialize(const std::vector<uint8_t>& in) {
    Program prog;
    if (in.size() < 4) return prog;

    uint32_t count;
    std::memcpy(&count, in.data(), 4);
    size_t pos = 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (pos >= in.size()) break;
        OpCode op = (OpCode)in[pos++];
        uint8_t reg = in[pos++];
        uint8_t type = in[pos++];

        Value val;
        if (type == 0) {
            int64_t v; std::memcpy(&v, &in[pos], 8); pos += 8; val = v;
        } else if (type == 1) {
            double v; std::memcpy(&v, &in[pos], 8); pos += 8; val = v;
        } else if (type == 2) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string s((const char*)&in[pos], len); pos += len; val = s;
        } else if (type == 3) {
            Solstice::Math::Vec2 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 4) {
            Solstice::Math::Vec3 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 5) {
            Solstice::Math::Vec4 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 6) {
            Solstice::Math::Matrix2 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 7) {
            Solstice::Math::Matrix3 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 8) {
            Solstice::Math::Matrix4 v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 9) {
            Solstice::Math::Quaternion v; std::memcpy(&v, &in[pos], sizeof(v)); pos += sizeof(v); val = v;
        } else if (type == 13) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string enumName((const char*)&in[pos], len); pos += len;
            std::memcpy(&len, &in[pos], 4); pos += 4;
            std::string variant((const char*)&in[pos], len); pos += len;
            int64_t disc; std::memcpy(&disc, &in[pos], 8); pos += 8;
            val = EnumVal{enumName, variant, disc};
        } else if (type == 14) {
            size_t entryIP; std::memcpy(&entryIP, &in[pos], sizeof(entryIP)); pos += sizeof(entryIP);
            val = ScriptFunc{entryIP, nullptr};
        } else if (type == 15) {
            PtrValue ptr;
            val = ptr;
        }

        prog.Instructions.push_back({op, reg, val});
    }

    // Deserialize EnumInfo
    if (pos + sizeof(uint32_t) <= in.size()) {
        uint32_t enumCount;
        std::memcpy(&enumCount, &in[pos], 4); pos += 4;
        for (uint32_t e = 0; e < enumCount && pos < in.size(); ++e) {
            uint32_t len; std::memcpy(&len, &in[pos], 4); pos += 4;
            if (pos + len > in.size()) break;
            std::string enumName((const char*)&in[pos], len); pos += len;
            Program::EnumMetadata meta;
            uint32_t varCount; std::memcpy(&varCount, &in[pos], 4); pos += 4;
            for (uint32_t v = 0; v < varCount && pos < in.size(); ++v) {
                std::memcpy(&len, &in[pos], 4); pos += 4;
                if (pos + len > in.size()) break;
                std::string varName((const char*)&in[pos], len); pos += len;
                int64_t disc; std::memcpy(&disc, &in[pos], 8); pos += 8;
                meta.variantToDiscriminant[varName] = disc;
            }
            prog.EnumInfo[enumName] = meta;
        }
    }

    // Deserialize FunctionArity
    if (pos + sizeof(uint32_t) <= in.size()) {
        uint32_t arityCount;
        std::memcpy(&arityCount, &in[pos], 4); pos += 4;
        for (uint32_t a = 0; a < arityCount && pos + 8 + 8 <= in.size(); ++a) {
            size_t ip; std::memcpy(&ip, &in[pos], sizeof(ip)); pos += sizeof(ip);
            size_t ar; std::memcpy(&ar, &in[pos], sizeof(ar)); pos += sizeof(ar);
            prog.FunctionArity[ip] = ar;
        }
    }
    return prog;
}

BytecodeVM::BytecodeVM() {
    m_Registers.fill(0);
    m_JIT = std::make_unique<JIT>();
}

BytecodeVM::~BytecodeVM() = default;

// Helper to call destructors for an object (traverses inheritance chain in reverse)
void BytecodeVM::CallDestructors(const std::shared_ptr<Dictionary>& obj) {
    if (!obj || !obj->Has("__class__")) return;
    
    Value classVal = obj->Get("__class__");
    if (!std::holds_alternative<std::string>(classVal)) return;
    
    std::string className = std::get<std::string>(classVal);
    
    // Build destructor chain (derived to base)
    std::vector<std::string> destructorChain;
    std::string currentClass = className;
    while (!currentClass.empty() && m_Program.ClassInfo.find(currentClass) != m_Program.ClassInfo.end()) {
        const auto& classInfo = m_Program.ClassInfo.at(currentClass);
        if (classInfo.DestructorAddress != 0) {
            destructorChain.push_back(currentClass);
        }
        currentClass = classInfo.BaseClass;
    }
    
    // Call destructors in reverse order (derived first, then base)
    for (auto it = destructorChain.rbegin(); it != destructorChain.rend(); ++it) {
        std::string destructorName = *it + "::destructor";
        if (m_Program.Exports.find(destructorName) != m_Program.Exports.end()) {
            size_t destructorAddr = m_Program.Exports.at(destructorName);
            
            // Push object and call destructor
            Push(obj);
            size_t savedIP = m_IP;
            m_IP = destructorAddr;
            
            // Execute destructor (simplified - would need proper call stack management)
            try {
                while (m_IP < m_Program.Instructions.size()) {
                    const auto& destInst = m_Program.Instructions[m_IP];
                    if (destInst.Op == OpCode::RET) {
                        break;
                    }
                    ExecuteInstruction(destInst, m_IP);
                    m_IP++;
                }
            } catch (...) {
                // Ignore errors in destructor
            }
            
            m_IP = savedIP;
        }
    }
}

void BytecodeVM::LoadProgram(const Program& prog) {
    m_Program = prog;
    m_IP = 0;
    m_Stack.clear();
    m_CallStack.clear();
}

void BytecodeVM::ReloadProgram(const Program& prog) {
    Core::LockGuard Guard(m_VMLock);
    // Preserve registers but update instructions
    m_Program = prog;
    m_IP = 0;
    m_CallStack.clear();
    // m_Stack.clear(); // Maybe keep stack? Usually safer to clear for hot reload
}

void BytecodeVM::RequestYieldFrames(int64_t frames) {
    m_YieldRequest = YieldRequest{};
    m_YieldRequest->type = YieldRequest::Type::Frames;
    m_YieldRequest->framesLeft = (frames > 0) ? frames : 0;
}

void BytecodeVM::RequestYieldSeconds(double seconds) {
    m_YieldRequest = YieldRequest{};
    m_YieldRequest->type = YieldRequest::Type::Seconds;
    m_YieldRequest->secondsDelay = (seconds > 0.0) ? seconds : 0.0;
}

void BytecodeVM::RequestYieldEvent(const std::string& eventName) {
    m_YieldRequest = YieldRequest{};
    m_YieldRequest->type = YieldRequest::Type::Event;
    m_YieldRequest->eventName = eventName;
}

void BytecodeVM::RequestYieldUntil(ScriptFunc condition) {
    m_YieldRequest = YieldRequest{};
    m_YieldRequest->type = YieldRequest::Type::Condition;
    m_YieldRequest->conditionFunc = condition;
}

namespace {
    bool IsTruthy(const Value& v) {
        if (std::holds_alternative<int64_t>(v)) return std::get<int64_t>(v) != 0;
        if (std::holds_alternative<double>(v)) return std::get<double>(v) != 0.0;
        if (std::holds_alternative<std::string>(v)) return !std::get<std::string>(v).empty();
        return true; // other types (Vec, Dict, etc.) considered truthy
    }
}

bool BytecodeVM::RunCondition(const Program& program, ScriptFunc condition) {
    Program savedProgram = m_Program;
    m_Program = program;
    Value result = RunFunctionSlice(condition.entryIP, {});
    m_Program = savedProgram;
    return IsTruthy(result);
}

void BytecodeVM::saveStateTo(CoroutineState& out) {
    out.program = m_Program;
    out.IP = m_IP;
    out.stack = m_Stack;
    out.callStack = m_CallStack;
    out.registers = m_Registers;
    if (m_YieldRequest.has_value())
        out.yieldRequest = *m_YieldRequest;
}

void BytecodeVM::loadStateFrom(const CoroutineState& s) {
    m_Program = s.program;
    m_IP = s.IP;
    m_Stack = s.stack;
    m_CallStack = s.callStack;
    m_Registers = s.registers;
}

RunResult BytecodeVM::RunFromState(CoroutineState& state) {
    loadStateFrom(state);
    m_CurrentCoroutineState = &state;
    m_YieldRequest.reset();
    m_PendingYield = false;
    RunResult r = Run(&state);
    m_CurrentCoroutineState = nullptr;
    if (r == RunResult::Yielded)
        saveStateTo(state);
    return r;
}

Value BytecodeVM::RunFunctionSlice(size_t entryIP, const std::vector<Value>& args) {
    size_t savedIP = m_IP;
    std::vector<Value> savedStack = m_Stack;
    std::vector<size_t> savedCallStack = m_CallStack;

    m_Stack.clear();
    for (const auto& a : args) m_Stack.push_back(a);
    m_IP = entryIP;
    m_CallStack.clear();

    const size_t maxSteps = 100000;
    size_t steps = 0;
    while (m_IP < m_Program.Instructions.size() && steps < maxSteps) {
        const auto& inst = m_Program.Instructions[m_IP];
        if (inst.Op == OpCode::RET) {
            Value ret = m_Stack.empty() ? Value((int64_t)0) : m_Stack.back();
            m_IP = savedIP;
            m_Stack = savedStack;
            m_CallStack = savedCallStack;
            return ret;
        }
        auto [nextIP, shouldContinue] = ExecuteInstruction(inst, m_IP);
        if (!shouldContinue) break;
        m_IP = nextIP;
        steps++;
    }

    m_IP = savedIP;
    m_Stack = savedStack;
    m_CallStack = savedCallStack;
    return (int64_t)0;
}

void BytecodeVM::RegisterNative(const std::string& name, NativeFunc func) {
    m_Natives[name] = func;
}

void BytecodeVM::RegisterSystem(const SystemInfo& system) {
    Core::LockGuard Guard(m_VMLock);
    m_Systems.push_back(system);
}

void BytecodeVM::AddModule(const std::string& name, const Program& prog) {
    m_Modules[name] = prog;
}

bool BytecodeVM::HasModule(const std::string& name) const {
    Core::LockGuard Guard(m_VMLock);
    return m_Modules.find(name) != m_Modules.end();
}

const Program& BytecodeVM::GetModule(const std::string& name) const {
    auto it = m_Modules.find(name);
    if (it == m_Modules.end()) throw std::runtime_error("Module not found: " + name);
    return it->second;
}

void BytecodeVM::RegisterEventHandler(const std::string& eventName, ScriptFunc handler) {
    m_EventHandlers[eventName].push_back(handler);
}

void BytecodeVM::EmitEvent(const std::string& eventName, const std::vector<Value>& args) {
    // Note: Do not take m_VMLock here; may be called from within Run() (e.g. from a native). C++ callers must not emit concurrently with script execution.
    auto it = m_EventHandlers.find(eventName);
    if (it == m_EventHandlers.end()) return;
    for (const ScriptFunc& h : it->second) {
        try {
            RunFunctionSlice(h.entryIP, args);
        } catch (...) {
            // Log and continue to other handlers
        }
    }
}

void BytecodeVM::Push(Value v) {
    // Note: No lock needed - Push() is only called from ExecuteInstruction() which is called from Run() which already holds the lock
    m_Stack.push_back(v);
}

Value BytecodeVM::Pop() {
    // Note: No lock needed - Pop() is only called from ExecuteInstruction() which is called from Run() which already holds the lock
    if (m_Stack.empty()) throw std::runtime_error("Stack underflow");
    Value v = m_Stack.back();
    m_Stack.pop_back();
    return v;
}

Value BytecodeVM::Peek(size_t offset) {
    // Note: No lock needed - Peek() is only called from ExecuteInstruction() which is called from Run() which already holds the lock
    if (m_Stack.size() <= offset) throw std::runtime_error("Stack underflow");
    return m_Stack[m_Stack.size() - 1 - offset];
}

std::pair<size_t, bool> BytecodeVM::ExecuteInstruction(const Instruction& inst, size_t currentIP) {
    size_t nextIP = currentIP + 1;
    bool shouldContinue = true;

    switch (inst.Op) {
        case OpCode::NOP: break;
        case OpCode::HALT: shouldContinue = false; break;

        case OpCode::PUSH_CONST: Push(inst.Operand); break;
        case OpCode::POP: Pop(); break;
        case OpCode::DUP: Push(Peek()); break;

        case OpCode::JMP: {
            if (std::holds_alternative<int64_t>(inst.Operand)) {
                nextIP = (size_t)std::get<int64_t>(inst.Operand);
            }
            break;
        }
        case OpCode::JMP_IF: {
            Value cond = Pop();
            bool isTrue = false;
            if (std::holds_alternative<int64_t>(cond)) isTrue = std::get<int64_t>(cond) != 0;
            // else if (std::holds_alternative<double>(cond)) isTrue = std::get<double>(cond) != 0.0;

            if (isTrue) {
                if (std::holds_alternative<int64_t>(inst.Operand)) {
                    nextIP = (size_t)std::get<int64_t>(inst.Operand);
                }
            }
            break;
        }

        case OpCode::EQ: {
            Value b = Pop();
            Value a = Pop();
            Push((int64_t)(a == b ? 1 : 0));
            break;
        }
        case OpCode::NEQ: {
            Value b = Pop();
            Value a = Pop();
            Push((int64_t)(a != b ? 1 : 0));
            break;
        }
        case OpCode::LT: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push((int64_t)(std::get<int64_t>(a) < std::get<int64_t>(b) ? 1 : 0));
            } else {
                Push((int64_t)0); // Type mismatch or not supported
            }
            break;
        }
        case OpCode::GT: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push((int64_t)(std::get<int64_t>(a) > std::get<int64_t>(b) ? 1 : 0));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::LE: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push((int64_t)(std::get<int64_t>(a) <= std::get<int64_t>(b) ? 1 : 0));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::GE: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push((int64_t)(std::get<int64_t>(a) >= std::get<int64_t>(b) ? 1 : 0));
            } else {
                Push((int64_t)0);
            }
            break;
        }

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

        case OpCode::SUB: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) - std::get<int64_t>(b));
            } else {
                Push((int64_t)0); // Type mismatch
            }
            break;
        }

        case OpCode::MUL: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) * std::get<int64_t>(b));
            } else {
                Push((int64_t)0); // Type mismatch
            }
            break;
        }

        case OpCode::DIV: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                int64_t divisor = std::get<int64_t>(b);
                if (divisor == 0) {
                    throw std::runtime_error("Division by zero");
                }
                Push(std::get<int64_t>(a) / divisor);
            } else {
                Push((int64_t)0); // Type mismatch
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
        case OpCode::MOD: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                int64_t divisor = std::get<int64_t>(b);
                if (divisor == 0) {
                    throw std::runtime_error("Modulo by zero");
                }
                Push(std::get<int64_t>(a) % divisor);
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::POW: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                int64_t base = std::get<int64_t>(a);
                int64_t exp = std::get<int64_t>(b);
                int64_t result = 1;
                for (int64_t i = 0; i < exp; ++i) {
                    result *= base;
                }
                Push(result);
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::ABS: {
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a)) {
                int64_t val = std::get<int64_t>(a);
                Push(val < 0 ? -val : val);
            } else {
                Push(a);
            }
            break;
        }
        case OpCode::MIN: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::min(std::get<int64_t>(a), std::get<int64_t>(b)));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::MAX: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::max(std::get<int64_t>(a), std::get<int64_t>(b)));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::AND: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) & std::get<int64_t>(b));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::OR: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) | std::get<int64_t>(b));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::XOR: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) ^ std::get<int64_t>(b));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::NOT: {
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a)) {
                Push(~std::get<int64_t>(a));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::SHL: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) << std::get<int64_t>(b));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::SHR: {
            Value b = Pop();
            Value a = Pop();
            if (std::holds_alternative<int64_t>(a) && std::holds_alternative<int64_t>(b)) {
                Push(std::get<int64_t>(a) >> std::get<int64_t>(b));
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::STR_CONCAT: {
            Value b = Pop();
            Value a = Pop();
            std::string sa = std::holds_alternative<std::string>(a) ? std::get<std::string>(a) :
                            (std::holds_alternative<int64_t>(a) ? std::to_string(std::get<int64_t>(a)) : "");
            std::string sb = std::holds_alternative<std::string>(b) ? std::get<std::string>(b) :
                            (std::holds_alternative<int64_t>(b) ? std::to_string(std::get<int64_t>(b)) : "");
            Push(sa + sb);
            break;
        }
        case OpCode::STR_LEN: {
            Value a = Pop();
            if (std::holds_alternative<std::string>(a)) {
                Push((int64_t)std::get<std::string>(a).length());
            } else {
                Push((int64_t)0);
            }
            break;
        }
        case OpCode::STR_SUBSTR: {
            Value endVal = Pop();
            Value startVal = Pop();
            Value strVal = Pop();
            if (std::holds_alternative<std::string>(strVal) &&
                std::holds_alternative<int64_t>(startVal) &&
                std::holds_alternative<int64_t>(endVal)) {
                std::string str = std::get<std::string>(strVal);
                int64_t start = std::get<int64_t>(startVal);
                int64_t end = std::get<int64_t>(endVal);
                if (start >= 0 && end <= (int64_t)str.length() && start <= end) {
                    Push(str.substr((size_t)start, (size_t)(end - start)));
                } else {
                    Push(std::string(""));
                }
            } else {
                Push(std::string(""));
            }
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

                // Check for JIT-compiled function first
                if (m_JIT && m_JIT->IsEnabled()) {
                    // Record function call for hot path detection
                    m_JIT->RecordFunctionCall(currentIP);
                }

                if (m_Natives.count(name)) {
                    std::vector<Value> args;
                    args.reserve(argCount);
                    for (int64_t i = 0; i < argCount; ++i) args.emplace_back(Pop());
                    std::reverse(args.begin(), args.end());

                    Value ret = m_Natives[name](args);
                    if (m_YieldRequest.has_value()) {
                        Push((int64_t)0);
                        m_PendingYield = true;
                        nextIP = currentIP + 1;
                    } else {
                        Push(ret);
                    }
                } else if (name.find('.') != std::string::npos) {
                        // Module-qualified call: Module.Function
                        size_t dotPos = name.find('.');
                        std::string modName = name.substr(0, dotPos);
                        std::string funcName = name.substr(dotPos + 1);

                        if (m_Modules.count(modName)) {
                            const Program& mod = m_Modules[modName];
                            if (mod.Exports.count(funcName)) {
                                // Save current state (declared outside try for catch block access)
                                Program savedProgram = m_Program;
                                size_t savedIP = currentIP + 1; // Return to next instruction after CALL
                                std::vector<Value> savedStack = m_Stack;
                                std::vector<size_t> savedCallStack = m_CallStack;

                                try {
                                    // Arguments are already on the stack (pushed before CALL instruction)
                                    // The function expects them in order, so we keep them on the stack
                                    // Switch to module program and jump to function
                                    m_Program = mod;
                                    m_IP = mod.Exports.at(funcName);
                                    // Keep stack as-is (args are already there)

                                    // Set return point on call stack
                                    m_CallStack.push_back(savedIP);

                                    // Execute function using full instruction execution
                                    // Track return IP to know when to exit
                                    size_t returnIP = savedIP;
                                    bool funcRunning = true;
                                    int maxIterations = 10000; // Safety limit
                                    int iterations = 0;

                                    while (funcRunning && m_IP < m_Program.Instructions.size() && iterations < maxIterations) {
                                        iterations++;
                                        const auto& funcInst = m_Program.Instructions[m_IP];

                                        // Check for RET that returns to our caller
                                        if (funcInst.Op == OpCode::RET) {
                                            if (!m_CallStack.empty() && m_CallStack.back() == returnIP) {
                                                // Returning from module function to caller
                                                m_CallStack.pop_back();
                                                funcRunning = false;
                                                break;
                                            }
                                        }

                                        // Execute instruction (handles all opcodes including nested module calls)
                                        try {
                                        auto [funcNextIP, shouldContinue] = ExecuteInstruction(funcInst, m_IP);
                                        if (!shouldContinue || funcInst.Op == OpCode::HALT) {
                                            funcRunning = false;
                                        } else {
                                            m_IP = funcNextIP;
                                            }
                                        } catch (const std::exception& e) {
                                            SIMPLE_LOG("ERROR: Exception in module function " + modName + "." + funcName + " at iteration " + std::to_string(iterations) + ": " + std::string(e.what()));
                                            throw;
                                        } catch (...) {
                                            SIMPLE_LOG("ERROR: Unknown exception in module function " + modName + "." + funcName + " at iteration " + std::to_string(iterations));
                                            throw;
                                        }
                                    }

                                    if (iterations >= maxIterations) {
                                        throw std::runtime_error("Module function " + funcName + " exceeded iteration limit");
                                    }

                                    // Get return value (if any) or use 0
                                    Value retVal = m_Stack.empty() ? Value((int64_t)0) : m_Stack.back();

                                    // Restore program context
                                    m_Program = savedProgram;
                                    m_IP = savedIP;
                                    m_Stack = savedStack;
                                    m_CallStack = savedCallStack;

                                    // Push return value
                                    Push(retVal);
                                } catch (const std::exception& e) {
                                    // Restore state on error
                                    m_Program = savedProgram;
                                    m_IP = savedIP;
                                    m_Stack = savedStack;
                                    m_CallStack = savedCallStack;
                                    throw std::runtime_error("Error executing module function " + modName + "." + funcName + ": " + e.what());
                                }
                            } else {
                                throw std::runtime_error("Function " + funcName + " not found in module " + modName);
                            }
                        } else {
                            throw std::runtime_error("Module " + modName + " not found");
                        }
                    } else {
                        throw std::runtime_error("Unknown native function: " + name);
                    }
            } else if (std::holds_alternative<int64_t>(inst.Operand)) {
                // Script function call (within same program)
                size_t targetIP = (size_t)std::get<int64_t>(inst.Operand);

                // JIT path: if enabled and we have (or can compile) a trampoline, use it
                if (m_JIT && m_JIT->IsEnabled()) {
                    m_JIT->RecordFunctionCall(targetIP);
                    auto it = m_Program.FunctionArity.find(targetIP);
                    size_t arity = (it != m_Program.FunctionArity.end()) ? it->second : 0;
                    auto compiled = m_JIT->CompileHotFunction(this, m_Program, targetIP);
                    if (compiled && arity <= m_Stack.size()) {
                        std::vector<Value> args;
                        args.reserve(arity);
                        for (size_t i = 0; i < arity; ++i) args.push_back(Pop());
                        std::reverse(args.begin(), args.end());
                        Value result = compiled(args);
                        Push(result);
                        nextIP = currentIP + 1;
                        break;
                    }
                }

                m_CallStack.push_back(nextIP);
                nextIP = targetIP;
            }
            break;
        }

        case OpCode::CALL_VALUE: {
            Value funcVal = Pop();
            Value countVal = Pop();
            if (!std::holds_alternative<int64_t>(countVal)) {
                throw std::runtime_error("CALL_VALUE: argument count not int");
            }
            int64_t argCount = std::get<int64_t>(countVal);
            std::vector<Value> args;
            args.reserve(static_cast<size_t>(argCount));
            for (int64_t i = 0; i < argCount; ++i) {
                args.push_back(Pop());
            }
            std::reverse(args.begin(), args.end());
            if (std::holds_alternative<ScriptFunc>(funcVal)) {
                const auto& sf = std::get<ScriptFunc>(funcVal);
                for (const auto& a : args) Push(a);
                m_CallStack.push_back(nextIP);
                nextIP = sf.entryIP;
            } else {
                throw std::runtime_error("CALL_VALUE: expected script function value");
            }
            break;
        }

        case OpCode::RET: {
            if (m_CallStack.empty()) {
                shouldContinue = false;
            } else {
                nextIP = m_CallStack.back();
                m_CallStack.pop_back();
            }
            break;
        }

        case OpCode::IMPORT_MODULE: {
            if (std::holds_alternative<std::string>(inst.Operand)) {
                std::string name = std::get<std::string>(inst.Operand);
                if (m_Modules.count(name)) {
                    // For now, just mark as loaded or resolve symbols
                    // A more complete implementation would load the module's globals/functions
                }
            }
            break;
        }

        case OpCode::GET_ATTR: {
            Value objVal = Pop();
            if (std::holds_alternative<std::string>(inst.Operand)) {
                std::string attr = std::get<std::string>(inst.Operand);
                if (std::holds_alternative<std::shared_ptr<Dictionary>>(objVal)) {
                    auto obj = std::get<std::shared_ptr<Dictionary>>(objVal);
                    
                    // Check if attribute exists in object
                    if (obj->Has(attr)) {
                        Push(obj->Get(attr));
                        break;
                    }
                    
                    // Traverse inheritance chain
                    std::string className = "";
                    if (obj->Has("__class__")) {
                        Value classVal = obj->Get("__class__");
                        if (std::holds_alternative<std::string>(classVal)) {
                            className = std::get<std::string>(classVal);
                        }
                    }
                    
                    // Search up inheritance chain
                    std::string currentClass = className;
                    while (!currentClass.empty() && m_Program.ClassInfo.find(currentClass) != m_Program.ClassInfo.end()) {
                        const auto& classInfo = m_Program.ClassInfo.at(currentClass);
                        
                        // Check if base class has this attribute
                        if (!classInfo.BaseClass.empty()) {
                            // For now, attributes are stored in the object Dictionary
                            // In a full implementation, we'd check base class object
                            // This is a simplified version
                        }
                        
                        // Move to base class
                        currentClass = classInfo.BaseClass;
                    }
                    
                    // Not found in inheritance chain, return default
                    Push((int64_t)0);
                } else {
                    // Handle engine object attributes or other types
                    Push((int64_t)0);
                }
            }
            break;
        }

        case OpCode::SET_ATTR: {
            Value val = Pop();
            Value objVal = Pop();
            if (std::holds_alternative<std::string>(inst.Operand)) {
                std::string attr = std::get<std::string>(inst.Operand);
                if (std::holds_alternative<std::shared_ptr<Dictionary>>(objVal)) {
                    auto obj = std::get<std::shared_ptr<Dictionary>>(objVal);
                    
                    // Set attribute directly on object
                    // In a full implementation, we'd check if it's defined in a base class
                    obj->Set(attr, val);
                }
            }
            break;
        }

        case OpCode::NEW_OBJ: {
            if (std::holds_alternative<std::string>(inst.Operand)) {
                std::string className = std::get<std::string>(inst.Operand);
                // Argument count is on top of stack
                Value cntVal = Pop();
                int64_t argCount = std::holds_alternative<int64_t>(cntVal) ? std::get<int64_t>(cntVal) : 0;
                
                // Store args temporarily (they'll be used by constructor)
                std::vector<Value> args;
                for (int i = 0; i < argCount; ++i) {
                    args.insert(args.begin(), Pop()); // Reverse order to maintain correct order
                }

                // Create a new Dictionary representing the object for now
                auto obj = std::make_shared<Dictionary>();
                obj->Set("__class__", className);
                
                // Store args in object for constructor access
                if (argCount > 0) {
                    auto argsArray = std::make_shared<Array>();
                    for (const auto& arg : args) {
                        argsArray->Push(arg);
                    }
                    obj->Set("__constructor_args__", argsArray);
                }
                
                Push(obj);
                
                // Push args back on stack for constructor call (if constructor exists)
                // Constructor will be called by compiler-generated code after NEW_OBJ
                for (const auto& arg : args) {
                    Push(arg);
                }
            }
            break;
        }

        case OpCode::ARRAY_GET: {
            Value indexVal = Pop();
            Value arrayVal = Pop();
            if (std::holds_alternative<std::shared_ptr<Array>>(arrayVal) && std::holds_alternative<int64_t>(indexVal)) {
                auto arr = std::get<std::shared_ptr<Array>>(arrayVal);
                size_t index = (size_t)std::get<int64_t>(indexVal);
                Push(arr->Get(index));
            } else {
                Push((int64_t)0);
            }
            break;
        }

        case OpCode::ARRAY_SET: {
            Value value = Pop();
            Value indexVal = Pop();
            Value arrayVal = Pop();
            if (std::holds_alternative<std::shared_ptr<Array>>(arrayVal) && std::holds_alternative<int64_t>(indexVal)) {
                auto arr = std::get<std::shared_ptr<Array>>(arrayVal);
                size_t index = (size_t)std::get<int64_t>(indexVal);
                arr->Set(index, value);
            }
            break;
        }

        case OpCode::ARRAY_PUSH: {
            Value value = Pop();
            Value arrayVal = Pop();
            if (std::holds_alternative<std::shared_ptr<Array>>(arrayVal)) {
                auto arr = std::get<std::shared_ptr<Array>>(arrayVal);
                arr->Push(value);
            }
            break;
        }

        case OpCode::ARRAY_POP: {
            Value arrayVal = Pop();
            if (std::holds_alternative<std::shared_ptr<Array>>(arrayVal)) {
                auto arr = std::get<std::shared_ptr<Array>>(arrayVal);
                Push(arr->Pop());
            } else {
                Push((int64_t)0);
            }
            break;
        }

        // Pointer operations
        case OpCode::PTR_NEW: {
            Value payload = Pop();
            PtrValue ptr = MakePtr(payload);
            Push(ptr);
            break;
        }

        case OpCode::PTR_RESET: {
            Value v = Pop();
            if (!std::holds_alternative<PtrValue>(v)) {
                throw std::runtime_error("PTR_RESET: expected pointer value");
            }
            PtrValue ptr = std::get<PtrValue>(v);
            if (!PtrIsValid(ptr)) {
                throw std::runtime_error("PTR_RESET: pointer already freed");
            }
            PtrReset(ptr);
            // Return a dummy value so expressions using Ptr.Reset have a result.
            Push(static_cast<int64_t>(0));
            break;
        }

        case OpCode::PTR_GET: {
            Value v = Pop();
            if (!std::holds_alternative<PtrValue>(v)) {
                throw std::runtime_error("PTR_GET: expected pointer value");
            }
            PtrValue ptr = std::get<PtrValue>(v);
            if (!PtrIsValid(ptr)) {
                throw std::runtime_error("PTR_GET: dereference of freed pointer");
            }
            Push(PtrGetValue(ptr));
            break;
        }

        case OpCode::PTR_IS_VALID: {
            Value v = Pop();
            if (!std::holds_alternative<PtrValue>(v)) {
                Push(static_cast<int64_t>(0));
                break;
            }
            PtrValue ptr = std::get<PtrValue>(v);
            Push(static_cast<int64_t>(PtrIsValid(ptr) ? 1 : 0));
            break;
        }

        case OpCode::DELETE_VALUE: {
            Value v = Pop();
            if (std::holds_alternative<std::shared_ptr<Dictionary>>(v)) {
                auto obj = std::get<std::shared_ptr<Dictionary>>(v);
                CallDestructors(obj);
            }
            // Other value kinds are ignored by delete
            break;
        }

        case OpCode::YIELD: {
            m_YieldRequest = YieldRequest{};
            m_YieldRequest->type = YieldRequest::Type::Frames;
            m_YieldRequest->framesLeft = 1;
            m_PendingYield = true;
            nextIP = currentIP + 1;
            break;
        }

        default: break;
    }

    return {nextIP, shouldContinue};
}

RunResult BytecodeVM::Run(CoroutineState* outState) {
    Core::LockGuard Guard(m_VMLock);
    m_CurrentCoroutineState = outState;
    bool running = true;
    size_t maxInstructions = m_Program.Instructions.size() * 100; // Safety limit: max 100x program size
    size_t instructionCount = 0;
    m_Paused = false;

    while (running && m_IP < m_Program.Instructions.size() && instructionCount < maxInstructions) {
        // Check for pending yield (set by native or YIELD opcode)
        if (m_PendingYield) {
            if (outState) {
                saveStateTo(*outState);
                m_PendingYield = false;
                m_YieldRequest.reset();
                m_CurrentCoroutineState = nullptr;
                return RunResult::Yielded;
            }
            m_PendingYield = false;
            m_YieldRequest.reset();
        }

        // Check for breakpoints
        if (m_DebugMode && m_Breakpoints.find(m_IP) != m_Breakpoints.end()) {
            m_Paused = true;
            SIMPLE_LOG("Breakpoint hit at instruction " + std::to_string(m_IP));
            // In a real debugger, this would wait for user input
            break;
        }

        // Check for step mode
        if (m_DebugMode && m_StepMode) {
            m_Paused = true;
            m_StepMode = false;
            break;
        }

        instructionCount++;
        const auto& inst = m_Program.Instructions[m_IP];
        try {
        auto [nextIP, shouldContinue] = ExecuteInstruction(inst, m_IP);
        running = shouldContinue;
        m_IP = nextIP;
        } catch (const std::exception& e) {
            // Build detailed error with stack trace
            VMError error(std::string("Exception executing instruction ") + std::to_string(instructionCount) +
                        " at IP " + std::to_string(m_IP) + ": " + e.what(), m_IP);

            // Add call stack frames
            for (size_t i = 0; i < m_CallStack.size(); ++i) {
                VMError::StackFrame frame;
                frame.InstructionIndex = m_CallStack[i];
                auto debugIt = m_DebugInfo.find(frame.InstructionIndex);
                if (debugIt != m_DebugInfo.end()) {
                    frame.SourceFile = debugIt->second.SourceFile;
                    frame.LineNumber = debugIt->second.LineNumber;
                }
                error.AddStackFrame(frame);
            }

            // Add current frame
            VMError::StackFrame currentFrame;
            currentFrame.InstructionIndex = m_IP;
            auto debugIt = m_DebugInfo.find(m_IP);
            if (debugIt != m_DebugInfo.end()) {
                currentFrame.SourceFile = debugIt->second.SourceFile;
                currentFrame.LineNumber = debugIt->second.LineNumber;
            }
            error.AddStackFrame(currentFrame);

            m_LastError = error;
            SIMPLE_LOG("ERROR: " + std::string(error.what()));
            throw error;
        } catch (...) {
            VMError error("Unknown exception executing instruction " + std::to_string(instructionCount) +
                         " at IP " + std::to_string(m_IP), m_IP);
            m_LastError = error;
            SIMPLE_LOG("ERROR: " + std::string(error.what()));
            throw error;
        }
    }

    m_CurrentCoroutineState = nullptr;
    if (instructionCount >= maxInstructions) {
        throw std::runtime_error("BytecodeVM::Run() exceeded instruction limit (possible infinite loop)");
    }
    return RunResult::Completed;
}

void BytecodeVM::SetBreakpoint(size_t instructionIndex) {
    Core::LockGuard Guard(m_VMLock);
    m_Breakpoints.insert(instructionIndex);
}

void BytecodeVM::ClearBreakpoint(size_t instructionIndex) {
    Core::LockGuard Guard(m_VMLock);
    m_Breakpoints.erase(instructionIndex);
}

bool BytecodeVM::HasBreakpoint(size_t instructionIndex) const {
    Core::LockGuard Guard(m_VMLock);
    return m_Breakpoints.find(instructionIndex) != m_Breakpoints.end();
}

void BytecodeVM::Step() {
    Core::LockGuard Guard(m_VMLock);
    if (!m_DebugMode) {
        m_DebugMode = true;
    }
    m_StepMode = true;
    m_Paused = false;

    if (m_IP < m_Program.Instructions.size()) {
        const auto& inst = m_Program.Instructions[m_IP];
        try {
            auto [nextIP, shouldContinue] = ExecuteInstruction(inst, m_IP);
            m_IP = nextIP;
            if (!shouldContinue) {
                m_Paused = true;
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception in step: " + std::string(e.what()));
            throw;
        }
    }
}

std::vector<BytecodeVM::CallStackFrame> BytecodeVM::GetCallStack() const {
    Core::LockGuard Guard(m_VMLock);
    std::vector<CallStackFrame> frames;
    for (size_t i = 0; i < m_CallStack.size(); ++i) {
        CallStackFrame frame;
        frame.ReturnIP = m_CallStack[i];
        // Try to find function name from debug info
        auto it = m_DebugInfo.find(frame.ReturnIP);
        if (it != m_DebugInfo.end()) {
            frame.FunctionName = it->second.SourceFile;
        } else {
            frame.FunctionName = "Unknown";
        }
        frames.push_back(frame);
    }
    return frames;
}

void BytecodeVM::SetDebugInfo(size_t instructionIndex, const DebugInfo& info) {
    Core::LockGuard Guard(m_VMLock);
    m_DebugInfo[instructionIndex] = info;
}

BytecodeVM::DebugInfo BytecodeVM::GetDebugInfo(size_t instructionIndex) const {
    Core::LockGuard Guard(m_VMLock);
    auto it = m_DebugInfo.find(instructionIndex);
    if (it != m_DebugInfo.end()) {
        return it->second;
    }
    return DebugInfo();
}

void BytecodeVM::EnableJIT() {
    Core::LockGuard Guard(m_VMLock);
    if (m_JIT) {
        m_JIT->Enable();
    }
}

void BytecodeVM::DisableJIT() {
    Core::LockGuard Guard(m_VMLock);
    if (m_JIT) {
        m_JIT->Disable();
    }
}

bool BytecodeVM::IsJITEnabled() const {
    Core::LockGuard Guard(m_VMLock);
    return m_JIT != nullptr && m_JIT->IsEnabled();
}

}
