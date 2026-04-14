#include "BytecodeVM.hxx"
#include "VMError.hxx"
#include "../JIT/JIT.hxx"
#include "../Runtime/PtrRuntime.hxx"
#include "../../Core/System/Async.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace Solstice::Scripting {

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
    if (m_JIT) {
        m_JIT->InvalidateAll();
    }
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

JITStats BytecodeVM::GetJITStats() const {
    Core::LockGuard Guard(m_VMLock);
    if (!m_JIT) {
        return {};
    }
    return m_JIT->GetStats();
}

}
