#pragma once

#include "../../Solstice.hxx"
#include "../VM/BytecodeVM.hxx"
#include "Backend/IBackend.hxx"
#include <unordered_map>
#include <memory>

namespace Solstice::Scripting {

struct JITStats {
    uint64_t FunctionCalls = 0;
    uint64_t CompileAttempts = 0;
    uint64_t CompileSuccesses = 0;
    uint64_t FallbackDisabled = 0;
    uint64_t FallbackNotHot = 0;
    uint64_t FallbackBackendUnavailable = 0;
};

/**
 * JIT - Just-In-Time compiler for hot path optimization
 */
class SOLSTICE_API JIT {
public:
    JIT();
    ~JIT();

    void SetBackend(std::unique_ptr<Backend::IBackend> backend);
    void Enable();
    void Disable();
    bool IsEnabled() const { return m_Enabled; }

    // Hot path detection
    void RecordFunctionCall(size_t functionIP);
    bool IsHotPath(size_t functionIP) const;

    // Compilation - returns trampoline that calls vm->RunFunctionSlice (Phase 1)
    Backend::IBackend::CompiledFunction CompileHotFunction(BytecodeVM* vm, const Solstice::Scripting::Program& program, size_t functionIP);
    void InvalidateAll();
    const JITStats& GetStats() const { return m_Stats; }

private:
    bool m_Enabled = false;
    std::unique_ptr<Backend::IBackend> m_Backend;
    std::unordered_map<size_t, uint32_t> m_FunctionCallCounts;
    std::unordered_map<size_t, Backend::IBackend::CompiledFunction> m_CompiledFunctions;
    JITStats m_Stats;
    static constexpr uint32_t HOT_PATH_THRESHOLD = 100; // Calls before JIT compilation
};

} // namespace Solstice::Scripting

