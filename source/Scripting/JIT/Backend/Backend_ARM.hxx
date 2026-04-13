#pragma once

#include "IBackend.hxx"

namespace Solstice::Scripting::Backend {

/**
 * Backend_ARM - ARM64/AArch64 JIT code generation backend
 */
class SOLSTICE_API Backend_ARM : public IBackend {
public:
    Backend_ARM();
    ~Backend_ARM() override;

    CompiledFunction CompileFunction(const Program& program, size_t functionStartIP, BytecodeVM* vm) override;

    void* AllocateCodeMemory(size_t size) override;
    void FreeCodeMemory(void* ptr, size_t size) override;
    void FlushInstructionCache(void* ptr, size_t size) override;

    std::string GetArchitectureName() const override { return "ARM64"; }
    bool IsSupported() const override;

private:
    bool m_Supported = false;
};

} // namespace Solstice::Scripting::Backend

