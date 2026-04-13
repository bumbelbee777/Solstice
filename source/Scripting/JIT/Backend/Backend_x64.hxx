#pragma once

#include "IBackend.hxx"

namespace Solstice::Scripting::Backend {

/**
 * Backend_x64 - x86-64 JIT code generation backend
 */
class SOLSTICE_API Backend_x64 : public IBackend {
public:
    Backend_x64();
    ~Backend_x64() override;

    CompiledFunction CompileFunction(const Program& program, size_t functionStartIP, BytecodeVM* vm) override;

    void* AllocateCodeMemory(size_t size) override;
    void FreeCodeMemory(void* ptr, size_t size) override;
    void FlushInstructionCache(void* ptr, size_t size) override;

    std::string GetArchitectureName() const override { return "x64"; }
    bool IsSupported() const override;

private:
    // Placeholder for future implementation
    bool m_Supported = false;
};

} // namespace Solstice::Scripting::Backend

