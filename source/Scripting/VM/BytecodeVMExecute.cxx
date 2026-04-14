#include "BytecodeVM.hxx"
#include "VMError.hxx"
#include "../JIT/JIT.hxx"
#include "../Runtime/PtrRuntime.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <sstream>

namespace Solstice::Scripting {

std::pair<size_t, bool> BytecodeVM::ExecuteInstruction(const Instruction& inst, size_t currentIP) {
    size_t nextIP = currentIP + 1;
    bool shouldContinue = true;

    // Opcode dispatch: monolithic switch (tune with opcode jump tables if profiling shows benefit).
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

}
