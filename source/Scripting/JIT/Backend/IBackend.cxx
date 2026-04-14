#include "IBackend.hxx"
#include "Backend_x64.hxx"
#include "Backend_ARM.hxx"
#include "Backend_RISCV.hxx"
#include <Core/Debug/Debug.hxx>

namespace Solstice::Scripting::Backend {

std::unique_ptr<IBackend> IBackend::CreateForCurrentArchitecture() {
    // Try x64 first (most common)
    auto x64Backend = std::make_unique<Backend_x64>();
    if (x64Backend->IsSupported()) {
        SIMPLE_LOG("JIT: Using x64 backend");
        return x64Backend;
    }

    // Try ARM64
    auto armBackend = std::make_unique<Backend_ARM>();
    if (armBackend->IsSupported()) {
        SIMPLE_LOG("JIT: Using ARM64 backend");
        return armBackend;
    }

    // Try RISC-V
    auto riscvBackend = std::make_unique<Backend_RISCV>();
    if (riscvBackend->IsSupported()) {
        SIMPLE_LOG("JIT: Using RISC-V backend");
        return riscvBackend;
    }

    SIMPLE_LOG("JIT: No supported backend found, JIT disabled");
    return nullptr;
}

} // namespace Solstice::Scripting::Backend

