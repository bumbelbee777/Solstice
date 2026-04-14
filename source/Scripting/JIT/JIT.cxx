#include "JIT.hxx"
#include "../../Core/Debug/Debug.hxx"

namespace Solstice::Scripting {

JIT::JIT() {
    m_Backend = Backend::IBackend::CreateForCurrentArchitecture();
}

JIT::~JIT() = default;

void JIT::SetBackend(std::unique_ptr<Backend::IBackend> backend) {
    m_Backend = std::move(backend);
}

void JIT::Enable() {
    if (m_Backend && m_Backend->IsSupported()) {
        m_Enabled = true;
        SIMPLE_LOG("JIT: Enabled for " + m_Backend->GetArchitectureName());
    } else {
        m_Enabled = false;
        SIMPLE_LOG("JIT: Cannot enable - no supported backend");
    }
}

void JIT::Disable() {
    m_Enabled = false;
    m_CompiledFunctions.clear();
}

void JIT::RecordFunctionCall(size_t functionIP) {
    if (!m_Enabled) return;
    m_Stats.FunctionCalls++;
    m_FunctionCallCounts[functionIP]++;
}

bool JIT::IsHotPath(size_t functionIP) const {
    if (!m_Enabled) return false;
    auto it = m_FunctionCallCounts.find(functionIP);
    return it != m_FunctionCallCounts.end() && it->second >= HOT_PATH_THRESHOLD;
}

Backend::IBackend::CompiledFunction JIT::CompileHotFunction(BytecodeVM* vm, const Program& program, size_t functionIP) {
    if (!m_Enabled) {
        m_Stats.FallbackDisabled++;
        return nullptr;
    }
    if (!m_Backend) {
        m_Stats.FallbackBackendUnavailable++;
        return nullptr;
    }

    // Check if already compiled
    auto it = m_CompiledFunctions.find(functionIP);
    if (it != m_CompiledFunctions.end()) {
        return it->second;
    }

    // Only compile when hot (call count >= threshold)
    if (!IsHotPath(functionIP)) {
        m_Stats.FallbackNotHot++;
        return nullptr;
    }

    // Compile function (trampoline that calls vm->RunFunctionSlice)
    m_Stats.CompileAttempts++;
    auto compiled = m_Backend->CompileFunction(program, functionIP, vm);
    if (compiled) {
        m_CompiledFunctions[functionIP] = compiled;
        m_Stats.CompileSuccesses++;
        SIMPLE_LOG("JIT: Compiled function at IP " + std::to_string(functionIP));
    }

    return compiled;
}

void JIT::InvalidateAll() {
    m_CompiledFunctions.clear();
    m_FunctionCallCounts.clear();
}

} // namespace Solstice::Scripting

