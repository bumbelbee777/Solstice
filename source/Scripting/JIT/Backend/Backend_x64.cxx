#include "Backend_x64.hxx"
#include "CodeBuffer.hxx"
#include "../../VM/BytecodeVM.hxx"
#include "../../../Core/Debug/Debug.hxx"
#include <cstring>
#include <memory>
#include <variant>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Solstice::Scripting::Backend {

namespace {

bool TryEmitLeafIntAdd(const Solstice::Scripting::Program& program, size_t functionStartIP, CodeBuffer& buf) {
    if (functionStartIP + 3 >= program.Instructions.size()) return false;
    const auto& a = program.Instructions[functionStartIP];
    const auto& b = program.Instructions[functionStartIP + 1];
    const auto& c = program.Instructions[functionStartIP + 2];
    const auto& d = program.Instructions[functionStartIP + 3];
    if (a.Op != Solstice::Scripting::OpCode::PUSH_CONST || b.Op != Solstice::Scripting::OpCode::PUSH_CONST ||
        c.Op != Solstice::Scripting::OpCode::ADD || d.Op != Solstice::Scripting::OpCode::RET) {
        return false;
    }
    if (!std::holds_alternative<int64_t>(a.Operand) || !std::holds_alternative<int64_t>(b.Operand)) return false;

    int64_t x = std::get<int64_t>(a.Operand);
    int64_t y = std::get<int64_t>(b.Operand);
    int64_t sum = x + y;

    buf.EmitMovRaxImm64(static_cast<uint64_t>(sum));
    buf.EmitRet();
    buf.PeepholeMachine();
    return true;
}

} // namespace

Backend_x64::Backend_x64() {
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

IBackend::CompiledFunction Backend_x64::CompileFunction(const Program& program, size_t functionStartIP,
                                                        BytecodeVM* vm) {
    if (!vm) return nullptr;

    if (m_Supported) {
        CodeBuffer buf;
        if (TryEmitLeafIntAdd(program, functionStartIP, buf) && buf.Size() > 0) {
            void* mem = AllocateCodeMemory(buf.Size());
            if (mem) {
                std::memcpy(mem, buf.Data(), buf.Size());
                FlushInstructionCache(mem, buf.Size());
                const size_t sz = buf.Size();
                auto memGuard = std::shared_ptr<void>(mem, [this, sz](void* p) { FreeCodeMemory(p, sz); });
                return [memGuard](const std::vector<Value>& args) -> Value {
                    (void)args;
                    using Fn = int64_t (*)();
                    auto* f = reinterpret_cast<Fn>(memGuard.get());
                    return Value{f()};
                };
            }
        }
    }

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
    (void)size;
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
