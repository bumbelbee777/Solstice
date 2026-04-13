#include "Backend_x64.hxx"
#include "../../Core/Debug.hxx"
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Solstice::Scripting::Backend {

Backend_x64::Backend_x64() {
    // Detect x64 architecture
    #if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    m_Supported = true;
    #else
    m_Supported = false;
    #endif
}

Backend_x64::~Backend_x64() = default;

bool Backend_x64::IsSupported() const {
    return m_Supported;
}

IBackend::CompiledFunction Backend_x64::CompileFunction(const Program& program, size_t functionStartIP, BytecodeVM* vm) {
    if (!vm) return nullptr;
    // Phase 1: trampoline that runs the function via VM (no native x64 codegen yet)
    return [vm, functionStartIP](const std::vector<Value>& args) -> Value {
        return vm->RunFunctionSlice(functionStartIP, args);
    };
}

void* Backend_x64::AllocateCodeMemory(size_t size) {
    #if defined(_WIN32) || defined(_WIN64)
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    #elif defined(__linux__) || defined(__APPLE__)
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
    #else
    return nullptr;
    #endif
}

void Backend_x64::FreeCodeMemory(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    VirtualFree(ptr, 0, MEM_RELEASE);
    #elif defined(__linux__) || defined(__APPLE__)
    munmap(ptr, size);
    #endif
}

void Backend_x64::FlushInstructionCache(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    ::FlushInstructionCache(GetCurrentProcess(), ptr, size);
    #elif defined(__linux__) || defined(__APPLE__)
    __builtin___clear_cache((char*)ptr, (char*)ptr + size);
    #endif
}

} // namespace Solstice::Scripting::Backend

