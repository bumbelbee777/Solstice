#pragma once

#include "../../../Solstice.hxx"
#include "../../VM/BytecodeVM.hxx"
#include <vector>
#include <memory>
#include <functional>

namespace Solstice::Scripting::Backend {

/**
 * IBackend - Interface for architecture-specific JIT code generation
 */
class SOLSTICE_API IBackend {
public:
    virtual ~IBackend() = default;

    // Architecture detection
    static std::unique_ptr<IBackend> CreateForCurrentArchitecture();

    // Code generation - trampoline that runs the function via VM (Phase 1 JIT)
    using CompiledFunction = std::function<Solstice::Scripting::Value(const std::vector<Solstice::Scripting::Value>&)>;
    virtual CompiledFunction CompileFunction(const Solstice::Scripting::Program& program, size_t functionStartIP, class BytecodeVM* vm) = 0;

    // Memory management
    virtual void* AllocateCodeMemory(size_t size) = 0;
    virtual void FreeCodeMemory(void* ptr, size_t size) = 0;
    virtual void FlushInstructionCache(void* ptr, size_t size) = 0;

    // Architecture info
    virtual std::string GetArchitectureName() const = 0;
    virtual bool IsSupported() const = 0;
};

} // namespace Solstice::Scripting::Backend

