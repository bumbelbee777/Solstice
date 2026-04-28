#if defined(_WIN32)

#include <windows.h>
#include <dbghelp.h>
#include <eh.h>

#include <cstdio>
#include <cstring>
#include <exception>

namespace {

constexpr unsigned long kDbgPrintException = 0x40010006ul;
constexpr unsigned long kMsCppException = 0xE06D7363ul;
constexpr unsigned long kBreakpointException = 0x80000003ul;
static volatile LONG g_SymInitDone = 0;

void EarlyCrashWrite(const char* text) {
    if (!text || !text[0]) {
        return;
    }
    DWORD written = 0;
    const HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != nullptr && hErr != INVALID_HANDLE_VALUE) {
        (void)WriteFile(hErr, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    }

    char tmpPath[MAX_PATH * 2]{};
    const DWORD n = GetTempPathA(static_cast<DWORD>(sizeof(tmpPath)), tmpPath);
    if (n > 0 && n < sizeof(tmpPath) && strcat_s(tmpPath, sizeof(tmpPath), "JackhammerEarlyCrash.log") == 0) {
        const HANDLE hFile = CreateFileA(tmpPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            (void)WriteFile(hFile, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
            (void)CloseHandle(hFile);
        }
    }

    OutputDebugStringA(text);
}

void EarlyCrashWriteLine(const char* text) {
    EarlyCrashWrite(text);
    EarlyCrashWrite("\r\n");
}

void EarlyCrashDumpStack(const char* reason) {
    EarlyCrashWriteLine("==== Jackhammer early crash ====");
    if (reason && reason[0]) {
        EarlyCrashWrite(reason);
        EarlyCrashWrite("\r\n");
    }

    void* frames[64]{};
    const USHORT frameCount = CaptureStackBackTrace(0, 64, frames, nullptr);
    HANDLE proc = GetCurrentProcess();
    for (USHORT i = 0; i < frameCount; ++i) {
        char line[1024]{};
        DWORD64 displacement = 0;
        alignas(SYMBOL_INFO) unsigned char symBuf[sizeof(SYMBOL_INFO) + 512]{};
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        bool hasSym = false;
        if (InterlockedCompareExchange(&g_SymInitDone, 1, 1) == 1) {
            hasSym = SymFromAddr(proc, reinterpret_cast<DWORD64>(frames[i]), &displacement, sym) == TRUE;
        }
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(frames[i]), &mod)) {
            char modPath[MAX_PATH]{};
            const DWORD n = GetModuleFileNameA(mod, modPath, MAX_PATH);
            const char* baseName = modPath;
            if (n > 0) {
                if (const char* slash = std::strrchr(modPath, '\\')) {
                    baseName = slash + 1;
                }
            } else {
                baseName = "(unknown-module)";
            }
            const auto base = reinterpret_cast<uintptr_t>(mod);
            const auto addr = reinterpret_cast<uintptr_t>(frames[i]);
            if (hasSym && sym->Name[0] != '\0') {
                std::snprintf(line, sizeof(line), "#%u 0x%p  %s!%s+0x%llX  (%s+0x%llX)\r\n", static_cast<unsigned>(i),
                    frames[i], baseName, sym->Name, static_cast<unsigned long long>(displacement), baseName,
                    static_cast<unsigned long long>(addr >= base ? (addr - base) : 0ull));
            } else {
                std::snprintf(
                    line, sizeof(line), "#%u 0x%p  %s+0x%llX\r\n", static_cast<unsigned>(i), frames[i], baseName,
                    static_cast<unsigned long long>(addr >= base ? (addr - base) : 0ull));
            }
        } else {
            if (hasSym && sym->Name[0] != '\0') {
                std::snprintf(line, sizeof(line), "#%u 0x%p  %s+0x%llX\r\n", static_cast<unsigned>(i), frames[i], sym->Name,
                    static_cast<unsigned long long>(displacement));
            } else {
                std::snprintf(line, sizeof(line), "#%u 0x%p\r\n", static_cast<unsigned>(i), frames[i]);
            }
        }
        EarlyCrashWrite(line);
    }
}

LONG WINAPI JackhammerEarlyVectoredHandler(EXCEPTION_POINTERS* ep) {
    static volatile LONG inHandler = 0;
    if (InterlockedCompareExchange(&inHandler, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!ep || !ep->ExceptionRecord) {
        inHandler = 0;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const unsigned long code = static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode);
    if (code == kDbgPrintException || code == kMsCppException || code == kBreakpointException) {
        inHandler = 0;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char reason[256]{};
    std::snprintf(reason, sizeof(reason), "SEH code=0x%08lX addr=%p",
        code, ep->ExceptionRecord->ExceptionAddress);
    EarlyCrashDumpStack(reason);
    inHandler = 0;
    return EXCEPTION_CONTINUE_SEARCH;
}

void __cdecl JackhammerEarlyPureCallHandler() {
    EarlyCrashDumpStack("pure virtual function call");
}

void __cdecl JackhammerEarlyInvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    EarlyCrashDumpStack("invalid parameter handler");
}

void __cdecl JackhammerEarlyTerminateHandler() {
    EarlyCrashDumpStack("std::terminate");
    std::_Exit(3);
}

struct JackhammerEarlyCrashInit final {
    JackhammerEarlyCrashInit() {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
        char exePath[MAX_PATH]{};
        char searchPath[MAX_PATH * 2]{};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
            std::strncpy(searchPath, exePath, sizeof(searchPath) - 1);
            if (char* slash = std::strrchr(searchPath, '\\')) {
                *slash = '\0';
            }
            SymSetSearchPath(GetCurrentProcess(), searchPath);
        }
        if (SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
            InterlockedExchange(&g_SymInitDone, 1);
        }
        (void)AddVectoredExceptionHandler(1, JackhammerEarlyVectoredHandler);
        _set_purecall_handler(JackhammerEarlyPureCallHandler);
        _set_invalid_parameter_handler(JackhammerEarlyInvalidParameterHandler);
        std::set_terminate(JackhammerEarlyTerminateHandler);
        EarlyCrashWriteLine("[Jackhammer] Early crash diagnostics installed.");
    }
};

#pragma init_seg(compiler)
JackhammerEarlyCrashInit g_JackhammerEarlyCrashInit;

} // namespace

#endif // _WIN32
