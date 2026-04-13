#pragma once

#include "IBackend.hxx"

namespace Solstice::Scripting::Backend {

/**
 * Backend_RISCV - RISC-V 64-bit JIT code generation backend
 */
class SOLSTICE_API Backend_RISCV : public IBackend {
public:
    Backend_RISCV();
    ~Backend_RISCV() override;

    CompiledFunction CompileFunction(const Program& program, size_t functionStartIP, BytecodeVM* vm) override;

    void* AllocateCodeMemory(size_t size) override;
    void FreeCodeMemory(void* ptr, size_t size) override;
    void FlushInstructionCache(void* ptr, size_t size) override;

    std::string GetArchitectureName() const override { return "RISC-V"; }
    bool IsSupported() const override;

private:
    bool m_Supported = false;
};

} // namespace Solstice::Scripting::Backend

