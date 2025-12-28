#include "Backend_RISCV.hxx"
#include "../../Core/Debug.hxx"
#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Solstice::Scripting::Backend {

Backend_RISCV::Backend_RISCV() {
    // Detect RISC-V architecture
    #if defined(__riscv) && __riscv_xlen == 64
    m_Supported = true;
    #else
    m_Supported = false;
    #endif
}

Backend_RISCV::~Backend_RISCV() = default;

bool Backend_RISCV::IsSupported() const {
    return m_Supported;
}

IBackend::CompiledFunction Backend_RISCV::CompileFunction(const Program& program, size_t functionStartIP) {
    // Placeholder: Return interpreter function for now
    return nullptr;
}

void* Backend_RISCV::AllocateCodeMemory(size_t size) {
    #if defined(_WIN32) || defined(_WIN64)
    return VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    #elif defined(__linux__) || defined(__APPLE__)
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
    #else
    return nullptr;
    #endif
}

void Backend_RISCV::FreeCodeMemory(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    VirtualFree(ptr, 0, MEM_RELEASE);
    #elif defined(__linux__) || defined(__APPLE__)
    munmap(ptr, size);
    #endif
}

void Backend_RISCV::FlushInstructionCache(void* ptr, size_t size) {
    if (!ptr) return;
    #if defined(_WIN32) || defined(_WIN64)
    ::FlushInstructionCache(GetCurrentProcess(), ptr, size);
    #elif defined(__linux__) || defined(__APPLE__)
    __builtin___clear_cache((char*)ptr, (char*)ptr + size);
    #endif
}

} // namespace Solstice::Scripting::Backend

