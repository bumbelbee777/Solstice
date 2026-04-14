#include "Backend_ARM.hxx"
#include "../../../Core/Debug/Debug.hxx"
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Solstice::Scripting::Backend {

Backend_ARM::Backend_ARM() {
    // Detect ARM64 architecture
    #if defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
    m_Supported = true;
    #else
    m_Supported = false;
    #endif
}

Backend_ARM::~Backend_ARM() = default;

bool Backend_ARM::IsSupported() const {
    return m_Supported;
}

IBackend::CompiledFunction Backend_ARM::CompileFunction(const Program& program, size_t functionStartIP, BytecodeVM* vm) {
    (void)program;
    if (!vm) return nullptr;
    // Architecture-parity fallback while native ARM64 codegen is developed.
    return [vm, functionStartIP](const std::vector<Value>& args) -> Value {
        return vm->RunFunctionSlice(functionStartIP, args);
    };
}

void* Backend_ARM::AllocateCodeMemory(size_t size) {
    #if defined(_WIN32) || defined(_WIN64)
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    #elif defined(__linux__) || defined(__APPLE__)
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
    #else
    return nullptr;
    #endif
}

void Backend_ARM::FreeCodeMemory(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    VirtualFree(ptr, 0, MEM_RELEASE);
    #elif defined(__linux__) || defined(__APPLE__)
    munmap(ptr, size);
    #endif
}

void Backend_ARM::FlushInstructionCache(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    ::FlushInstructionCache(GetCurrentProcess(), ptr, size);
    #elif defined(__linux__) || defined(__APPLE__)
    __builtin___clear_cache((char*)ptr, (char*)ptr + size);
    #endif
}

} // namespace Solstice::Scripting::Backend

