#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <variant>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include "../../Solstice.hxx"
#include "../../Core/System/Async.hxx"
#include "../../Math/Vector.hxx"
#include "../../Math/Matrix.hxx"
#include "../../Math/Quaternion.hxx"
#include "VMError.hxx"

// Forward declarations
namespace Solstice::ECS { class Registry; }
namespace Solstice::Scripting { class JIT; }
namespace Solstice::Scripting { struct JITStats; }

namespace Solstice::Scripting {

    // Forward declare collection types (defined below)
    class Array;
    class Dictionary;
    class Set;

    // Forward declare pointer header used by Ptr<T>-style values
    struct PtrHeader;
    using PtrValue = std::shared_ptr<PtrHeader>;

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
        MOD,        // Modulo
        POW,        // Power
        ABS,        // Absolute value
        MIN,        // Minimum
        MAX,        // Maximum
        INC,        // Increment top of stack or register

        // Logic
        EQ,
        NEQ,
        LT,
        GT,
        LE,        // Less than or equal
        GE,        // Greater than or equal

        // Bitwise
        AND,       // Bitwise AND
        OR,        // Bitwise OR
        XOR,       // Bitwise XOR
        NOT,       // Bitwise NOT
        SHL,       // Shift left
        SHR,       // Shift right

        // Control Flow
        JMP,
        JMP_IF,     // Jump if top of stack is true
        CALL,       // Call script function (int operand) or native (string operand)
        CALL_VALUE, // Pop function value, pop arg count, pop args, call (script or native)
        RET,

        // Object & Module Operations
        IMPORT_MODULE, // Import a module by name (string operand)
        GET_ATTR,      // Get attribute from object (string operand)
        SET_ATTR,      // Set attribute on object (string operand)
        NEW_OBJ,       // Instantiate a class (string operand)

        // Array Operations
        ARRAY_GET,  // Get element at index
        ARRAY_SET,  // Set element at index
        ARRAY_PUSH, // Push value to end
        ARRAY_POP,  // Pop value from end

        // String Operations
        STR_CONCAT, // Concatenate strings
        STR_LEN,    // String length
        STR_SUBSTR, // Substring

        YIELD,       // Coroutine yield (operand: optional payload)

        // Pointer operations
        PTR_NEW,      // Create Ptr from value on stack
        PTR_RESET,    // Logically free/reset Ptr on stack
        PTR_GET,      // Dereference Ptr to underlying payload
        PTR_IS_VALID, // Push 1 if Ptr is currently alive, else 0

        // Destruction
        DELETE_VALUE, // Call destructors for a value (classes), then discard it

        HALT
    };

    // Enum value for pattern matching and construction
    struct EnumVal {
        std::string enumName;
        std::string variant;
        int64_t discriminant = 0;
        bool operator==(const EnumVal& o) const {
            return enumName == o.enumName && variant == o.variant && discriminant == o.discriminant;
        }
    };

    // Forward declare closure env (defined after Value so it can hold Value)
    struct ClosureEnv;

    // Script function value (for first-class functions and lambdas)
    struct ScriptFunc {
        size_t entryIP = 0;
        std::shared_ptr<ClosureEnv> capture; // nullptr = no closure
        bool operator==(const ScriptFunc& o) const {
            return entryIP == o.entryIP && capture == o.capture;
        }
    };

    using Value = std::variant<
        int64_t, double, std::string,
        Solstice::Math::Vec2, Solstice::Math::Vec3, Solstice::Math::Vec4,
        Solstice::Math::Matrix2, Solstice::Math::Matrix3, Solstice::Math::Matrix4,
        Solstice::Math::Quaternion,
        std::shared_ptr<Array>,
        std::shared_ptr<Dictionary>,
        std::shared_ptr<Set>,
        PtrValue,
        EnumVal,
        ScriptFunc
    >;

    // Closure environment (holds captured values; defined after Value)
    struct ClosureEnv {
        std::unordered_map<std::string, Value> data;
    };

    // Runtime header for Ptr<T>-style values.
    // Uses shared_ptr semantics for lifetime, plus an explicit alive flag
    // so that explicit resets can be tracked and analyzed.
    struct PtrHeader {
        uint64_t Id = 0;
        bool Alive = true;
        Value Payload = (int64_t)0;
    };

    struct Instruction {
        OpCode Op;
        uint8_t RegisterIndex = 0; // For register ops
        Value Operand;             // For PUSH_CONST, JMP targets (as int), etc.
    };

    struct Program {
        std::vector<Instruction> Instructions;
        std::unordered_map<std::string, size_t> Exports;
        
        // Class metadata for inheritance
        struct ClassMetadata {
            std::string BaseClass;
            size_t ConstructorAddress = 0;
            size_t DestructorAddress = 0;
        };
        std::unordered_map<std::string, ClassMetadata> ClassInfo;

        // Enum metadata: enum name -> (variant name -> discriminant)
        struct EnumMetadata {
            std::unordered_map<std::string, int64_t> variantToDiscriminant;
        };
        std::unordered_map<std::string, EnumMetadata> EnumInfo;

        // Function arity for JIT and CALL_VALUE (entry IP -> number of parameters)
        std::unordered_map<size_t, size_t> FunctionArity;

        // Compile-time type hints (register index -> type string, e.g. Ptr.Player)
        std::unordered_map<uint8_t, std::string> RegisterTypes;
        // Ptr.* lowered ops: instruction index -> register that supplied the pointer (best-effort)
        std::unordered_map<size_t, uint8_t> PtrOperandRegs;

        // Helper to add instruction
        void Add(OpCode op, Value operand = 0) {
            Instructions.push_back({op, 0, operand});
        }

        void AddReg(OpCode op, uint8_t reg) {
            Instructions.push_back({op, reg, 0});
        }

        void Serialize(std::vector<uint8_t>& out) const;
        static Program Deserialize(const std::vector<uint8_t>& in);
    };

    // --- Coroutine support ---
    enum class RunResult { Completed, Yielded };

    struct YieldRequest {
        enum class Type { None, Frames, Seconds, Event, Condition };
        Type type = Type::None;
        int64_t framesLeft = 0;
        double resumeAtTime = 0.0;  // For Seconds: set by runner as currentTime + secondsDelay
        double secondsDelay = 0.0;  // For Seconds: delay requested by script
        std::string eventName;
        ScriptFunc conditionFunc;   // For Condition: script function () -> truthy when ready to resume
        bool isDue(uint64_t currentFrame, double currentTime) const {
            if (type == Type::None) return true;
            if (type == Type::Frames) return framesLeft <= 0;
            if (type == Type::Seconds) return currentTime >= resumeAtTime;
            if (type == Type::Condition) return false; // Evaluated by runner via RunCondition
            return false; // Event: not time-based, handled by EmitEvent
        }
    };

    struct CoroutineState {
        Program program;
        size_t IP = 0;
        std::vector<Value> stack;
        std::vector<size_t> callStack;
        std::array<Value, 16> registers{};
        YieldRequest yieldRequest;
        bool isDue(uint64_t currentFrame, double currentTime) const {
            return yieldRequest.isDue(currentFrame, currentTime);
        }
        void tick(uint64_t currentFrame, double currentTime) {
            if (yieldRequest.type == YieldRequest::Type::Frames && yieldRequest.framesLeft > 0)
                yieldRequest.framesLeft--;
        }
    };

    class SOLSTICE_API BytecodeVM {
    public:
        BytecodeVM();
        ~BytecodeVM();

        void LoadProgram(const Program& prog);
        void ReloadProgram(const Program& prog);
        const Program& GetProgram() const { return m_Program; }

        // Coroutine support: run until HALT or yield. If outState is non-null and execution yields, state is written to *outState.
        RunResult Run(CoroutineState* outState);
        // Run with no coroutine output (backward compatible); same as Run(nullptr).
        void Run() { (void)Run(nullptr); }

        // Run from a saved coroutine state; returns Completed or Yielded. If Yielded, state is updated.
        RunResult RunFromState(CoroutineState& state);

        // Called by native functions to request yield (e.g. WaitFrames, WaitSeconds)
        void RequestYieldFrames(int64_t frames);
        void RequestYieldSeconds(double seconds);
        void RequestYieldEvent(const std::string& eventName);
        void RequestYieldUntil(ScriptFunc condition);

        // Native function registration
        using NativeFunc = std::function<Value(const std::vector<Value>& args)>;
        void RegisterNative(const std::string& name, NativeFunc func);

        void Push(Value v);
        Value Pop();

        // Helper to peek
        Value Peek(size_t offset = 0);

        // Registry access
        void SetRegistry(ECS::Registry* registry) { m_Registry = registry; }
        ECS::Registry* GetRegistry() const { return m_Registry; }

        // Module management
        void AddModule(const std::string& name, const Program& prog);
        bool HasModule(const std::string& name) const;
        const Program& GetModule(const std::string& name) const;

        // Event/callback system: register script handlers and emit from script or C++
        void RegisterEventHandler(const std::string& eventName, ScriptFunc handler);
        void EmitEvent(const std::string& eventName, const std::vector<Value>& args);

        // Run condition function in given program context; returns true if result is truthy. Used by WaitUntil.
        bool RunCondition(const Program& program, ScriptFunc condition);

        // System registration
        struct SystemInfo {
            std::string Name;
            size_t FunctionAddress;
            std::vector<std::string> ComponentNames;
        };
        void RegisterSystem(const SystemInfo& system);
        const std::vector<SystemInfo>& GetSystems() const {
            Core::LockGuard Guard(m_VMLock);
            return m_Systems;
        }

        // Debugging support
        void SetDebugMode(bool enabled) { m_DebugMode = enabled; }
        bool IsDebugMode() const { return m_DebugMode; }
        void SetBreakpoint(size_t instructionIndex);
        void ClearBreakpoint(size_t instructionIndex);
        bool HasBreakpoint(size_t instructionIndex) const;
        void Step(); // Execute single instruction in debug mode
        struct CallStackFrame {
            size_t ReturnIP;
            std::string FunctionName;
        };
        std::vector<CallStackFrame> GetCallStack() const;

        // Debug info
        struct DebugInfo {
            size_t LineNumber = 0;
            std::string SourceFile;
            std::string SourceLine;
        };
        void SetDebugInfo(size_t instructionIndex, const DebugInfo& info);
        DebugInfo GetDebugInfo(size_t instructionIndex) const;

        // Error handling
        VMError GetVMError() const { return m_LastError; }
        void ClearError() { m_LastError = VMError("", 0); }

        // JIT compilation
        void EnableJIT();
        void DisableJIT();
        bool IsJITEnabled() const;
        JITStats GetJITStats() const;
        
        // Destructor management
        void CallDestructors(const std::shared_ptr<Dictionary>& obj);

        // Run a function slice (for JIT trampoline): push args, run from entryIP until RET, return result
        Value RunFunctionSlice(size_t entryIP, const std::vector<Value>& args);

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

        // ECS Registry
        ECS::Registry* m_Registry = nullptr;

        // Modules
        std::unordered_map<std::string, Program> m_Modules;

        // Event handlers: event name -> list of script function handlers
        std::unordered_map<std::string, std::vector<ScriptFunc>> m_EventHandlers;

        // Registered systems
        std::vector<SystemInfo> m_Systems;

        // Thread safety
        mutable Core::Spinlock m_VMLock;

        // Debugging
        bool m_DebugMode = false;
        std::unordered_set<size_t> m_Breakpoints;
        std::unordered_map<size_t, DebugInfo> m_DebugInfo;
        bool m_StepMode = false;
        bool m_Paused = false;

        // Error handling
        VMError m_LastError = VMError("", 0);

        // JIT compilation
        std::unique_ptr<class JIT> m_JIT;

        // Coroutine yield (set by natives, checked after CALL)
        std::optional<YieldRequest> m_YieldRequest;
        bool m_PendingYield = false;
        CoroutineState* m_CurrentCoroutineState = nullptr;

        void saveStateTo(CoroutineState& out);
        void loadStateFrom(const CoroutineState& s);

        // Helper method to execute a single instruction
        // Returns: (nextIP, shouldContinue)
        std::pair<size_t, bool> ExecuteInstruction(const Instruction& inst, size_t currentIP);
    };

    // Collection wrapper classes
    class SOLSTICE_API Array {
    public:
        std::vector<Value> Data;

        Array() = default;
        explicit Array(size_t size) : Data(size) {}
        Array(std::initializer_list<Value> init) : Data(init) {}

        Value Get(size_t index) const {
            if (index >= Data.size()) return (int64_t)0;
            return Data[index];
        }

        void Set(size_t index, const Value& value) {
            if (index >= Data.size()) Data.resize(index + 1);
            Data[index] = value;
        }

        void Push(const Value& value) {
            Data.push_back(value);
        }

        Value Pop() {
            if (Data.empty()) return (int64_t)0;
            Value v = Data.back();
            Data.pop_back();
            return v;
        }

        size_t Length() const { return Data.size(); }

        void Clear() { Data.clear(); }

        void Insert(size_t index, const Value& value) {
            if (index > Data.size()) Data.resize(index);
            Data.insert(Data.begin() + index, value);
        }

        void Remove(size_t index) {
            if (index < Data.size()) {
                Data.erase(Data.begin() + index);
            }
        }

        Array Slice(size_t start, size_t end) const {
            Array result;
            size_t actualEnd = std::min(end, Data.size());
            if (start < Data.size()) {
                result.Data.assign(Data.begin() + start, Data.begin() + actualEnd);
            }
            return result;
        }

        int64_t IndexOf(const Value& value) const {
            for (size_t i = 0; i < Data.size(); ++i) {
                if (Data[i] == value) return (int64_t)i;
            }
            return -1;
        }
    };

    class SOLSTICE_API Dictionary {
    public:
        std::unordered_map<std::string, Value> Data;

        Dictionary() = default;

        Value Get(const std::string& key) const {
            auto it = Data.find(key);
            if (it != Data.end()) return it->second;
            return (int64_t)0;
        }

        void Set(const std::string& key, const Value& value) {
            Data[key] = value;
        }

        bool Has(const std::string& key) const {
            return Data.find(key) != Data.end();
        }

        void Remove(const std::string& key) {
            Data.erase(key);
        }

        std::shared_ptr<Array> Keys() const {
            auto arr = std::make_shared<Array>();
            for (const auto& [key, _] : Data) {
                arr->Push(key);
            }
            return arr;
        }

        std::shared_ptr<Array> Values() const {
            auto arr = std::make_shared<Array>();
            for (const auto& [_, value] : Data) {
                arr->Push(value);
            }
            return arr;
        }

        size_t Size() const { return Data.size(); }

        void Clear() { Data.clear(); }
    };

    // Hash function for Value (needed for Set)
    struct ValueHash {
        size_t operator()(const Value& v) const {
            size_t h = std::hash<size_t>{}(v.index());
            if (std::holds_alternative<int64_t>(v)) {
                return h ^ (std::hash<int64_t>{}(std::get<int64_t>(v)) << 1);
            } else if (std::holds_alternative<double>(v)) {
                return h ^ (std::hash<double>{}(std::get<double>(v)) << 1);
            } else if (std::holds_alternative<std::string>(v)) {
                return h ^ (std::hash<std::string>{}(std::get<std::string>(v)) << 1);
            } else if (std::holds_alternative<Solstice::Math::Vec2>(v)) {
                const auto& vec = std::get<Solstice::Math::Vec2>(v);
                return h ^ (std::hash<float>{}(vec.x) << 1) ^ (std::hash<float>{}(vec.y) << 2);
            } else if (std::holds_alternative<Solstice::Math::Vec3>(v)) {
                const auto& vec = std::get<Solstice::Math::Vec3>(v);
                return h ^ (std::hash<float>{}(vec.x) << 1) ^ (std::hash<float>{}(vec.y) << 2) ^ (std::hash<float>{}(vec.z) << 3);
            } else if (std::holds_alternative<Solstice::Math::Vec4>(v)) {
                const auto& vec = std::get<Solstice::Math::Vec4>(v);
                return h ^ (std::hash<float>{}(vec.x) << 1) ^ (std::hash<float>{}(vec.y) << 2) ^ (std::hash<float>{}(vec.z) << 3) ^ (std::hash<float>{}(vec.w) << 4);
            } else if (std::holds_alternative<Solstice::Math::Quaternion>(v)) {
                const auto& q = std::get<Solstice::Math::Quaternion>(v);
                return h ^ (std::hash<float>{}(q.w) << 1) ^ (std::hash<float>{}(q.x) << 2) ^ (std::hash<float>{}(q.y) << 3) ^ (std::hash<float>{}(q.z) << 4);
            } else if (std::holds_alternative<std::shared_ptr<Array>>(v)) {
                return h ^ std::hash<void*>{}(std::get<std::shared_ptr<Array>>(v).get());
            } else if (std::holds_alternative<std::shared_ptr<Dictionary>>(v)) {
                return h ^ std::hash<void*>{}(std::get<std::shared_ptr<Dictionary>>(v).get());
            } else if (std::holds_alternative<std::shared_ptr<Set>>(v)) {
                return h ^ std::hash<void*>{}(std::get<std::shared_ptr<Set>>(v).get());
            } else if (std::holds_alternative<PtrValue>(v)) {
                const auto& ptr = std::get<PtrValue>(v);
                uint64_t id = ptr ? ptr->Id : 0;
                return h ^ (std::hash<uint64_t>{}(id) << 1);
            } else if (std::holds_alternative<EnumVal>(v)) {
                const auto& e = std::get<EnumVal>(v);
                return h ^ std::hash<std::string>{}(e.enumName) ^ (std::hash<std::string>{}(e.variant) << 1) ^ (std::hash<int64_t>{}(e.discriminant) << 2);
            } else if (std::holds_alternative<ScriptFunc>(v)) {
                const auto& f = std::get<ScriptFunc>(v);
                return h ^ std::hash<size_t>{}(f.entryIP) ^ std::hash<void*>{}(f.capture.get());
            }
            return h;
        }
    };

    class SOLSTICE_API Set {
    public:
        std::unordered_set<Value, ValueHash> Data;

        Set() = default;
        Set(std::initializer_list<Value> init) : Data(init) {}

        void Add(const Value& value) {
            Data.insert(value);
        }

        void Remove(const Value& value) {
            Data.erase(value);
        }

        bool Contains(const Value& value) const {
            return Data.find(value) != Data.end();
        }

        size_t Size() const { return Data.size(); }

        void Clear() { Data.clear(); }

        std::shared_ptr<Set> Union(const Set& other) const {
            auto result = std::make_shared<Set>();
            result->Data = Data;
            result->Data.insert(other.Data.begin(), other.Data.end());
            return result;
        }

        std::shared_ptr<Set> Intersection(const Set& other) const {
            auto result = std::make_shared<Set>();
            for (const auto& value : Data) {
                if (other.Data.find(value) != other.Data.end()) {
                    result->Data.insert(value);
                }
            }
            return result;
        }
    };

}
