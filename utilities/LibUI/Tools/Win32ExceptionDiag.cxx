#include "LibUI/Tools/Win32ExceptionDiag.hxx"

#if defined(_WIN32)

#include <dbghelp.h>

#include <cstdio>
#include <cstdint>
#include <cstring>

namespace LibUI::Tools {

namespace {

void WriteCrashChunk(const char* data, int len) {
    if (len <= 0) {
        return;
    }
    DWORD written = 0;
    const HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != nullptr && hErr != INVALID_HANDLE_VALUE) {
        WriteFile(hErr, data, static_cast<DWORD>(len), &written, nullptr);
    }
    OutputDebugStringA(data);
}

void WriteCrashLine(const char* line) {
    WriteCrashChunk(line, static_cast<int>(strlen(line)));
}

bool EnvTruthy(const char* name) {
    char buf[32]{};
    const DWORD n = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(sizeof(buf)));
    if (n == 0 || n >= sizeof(buf)) {
        return false;
    }
    if (buf[0] == '0' && buf[1] == '\0') {
        return false;
    }
    for (DWORD i = 0; i < n && buf[i]; ++i) {
        const unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            return true;
        }
    }
    return false;
}

#if defined(_M_X64)

void WriteMiniDumpMaybe(_EXCEPTION_POINTERS* ep) {
    if (!EnvTruthy("SOLSTICE_FULL_DUMP") || !ep) {
        return;
    }
    char path[MAX_PATH * 2]{};
    DWORD plen = GetTempPathA(static_cast<DWORD>(sizeof(path)), path);
    if (plen == 0 || plen >= sizeof(path)) {
        return;
    }
    char suffix[64]{};
    _snprintf_s(suffix, sizeof(suffix), _TRUNCATE, "Solstice_%08lx.dmp", static_cast<unsigned long>(GetTickCount()));
    if (strcat_s(path, sizeof(path), suffix) != 0) {
        return;
    }

    const HANDLE hf = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = ep;
    mei.ClientPointers = FALSE;

    const BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hf,
        MiniDumpNormal,
        ep ? &mei : nullptr,
        nullptr,
        nullptr);
    CloseHandle(hf);
    if (ok) {
        char msg[512]{};
        _snprintf_s(msg, sizeof(msg), _TRUNCATE, "Wrote minidump: %s\r\n", path);
        WriteCrashLine(msg);
    }
}

void LogStackX64(_EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ContextRecord) {
        WriteCrashLine("(no ContextRecord)\r\n");
        return;
    }

    HANDLE const hProc = GetCurrentProcess();
    HANDLE const hThread = GetCurrentThread();

    CONTEXT ctx{};
    std::memcpy(&ctx, ep->ContextRecord, sizeof(CONTEXT));

    STACKFRAME64 frame{};
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrFrame.Offset = ctx.Rbp ? ctx.Rbp : ctx.Rsp;

    DWORD machine = IMAGE_FILE_MACHINE_AMD64;

    alignas(SYMBOL_INFO) unsigned char symBuf[sizeof(SYMBOL_INFO) + 512]{};
    auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 512;

    constexpr int kMaxFrames = 48;
    for (int depth = 0; depth < kMaxFrames; ++depth) {
        if (!StackWalk64(
                machine,
                hProc,
                hThread,
                &frame,
                &ctx,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr)) {
            break;
        }

        const DWORD64 addr = frame.AddrPC.Offset;
        if (addr == 0) {
            break;
        }

        DWORD64 disp = 0;
        char line[640]{};
        void* const pAddr = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
        if (SymFromAddr(hProc, addr, &disp, sym) && sym->Name[0] != '\0') {
            IMAGEHLP_LINE64 il{};
            il.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD dline = 0;
            if (SymGetLineFromAddr64(hProc, addr, &dline, &il) && il.FileName) {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d  %s + 0x%llX  (%s:%lu)\r\n", depth, sym->Name,
                    static_cast<unsigned long long>(disp), il.FileName, static_cast<unsigned long>(il.LineNumber));
            } else {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d  %s + 0x%llX\r\n", depth, sym->Name,
                    static_cast<unsigned long long>(disp));
            }
        } else {
            HMODULE mod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCSTR>(addr), &mod)) {
                char modPath[MAX_PATH]{};
                if (GetModuleFileNameA(mod, modPath, MAX_PATH) > 0) {
                    const char* baseName = strrchr(modPath, '\\');
                    baseName = baseName ? baseName + 1 : modPath;
                    const auto base = reinterpret_cast<DWORD64>(mod);
                    _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d  %p  %s+0x%llX\r\n", depth, pAddr, baseName,
                        static_cast<unsigned long long>(addr - base));
                } else {
                    _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d  %p\r\n", depth, pAddr);
                }
            } else {
                _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d  %p\r\n", depth, pAddr);
            }
        }
        WriteCrashLine(line);
    }
}

#else

void WriteMiniDumpMaybe(_EXCEPTION_POINTERS* ep) {
    (void)ep;
}

void LogStackX64(_EXCEPTION_POINTERS* ep) {
    (void)ep;
    WriteCrashLine("  (stack walk available on x64 Windows builds only)\r\n");
}

#endif

} // namespace

static const char* s_UtilitySehTag = "Solstice";

LONG WINAPI Win32UtilityTopLevelExceptionFilter(_EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char buf[384]{};
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s FATAL: code=0x%08lX addr=%p\r\n",
        s_UtilitySehTag ? s_UtilitySehTag : "Solstice",
        static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode), ep->ExceptionRecord->ExceptionAddress);
    OutputDebugStringA(buf);
    DWORD written = 0;
    const HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != nullptr && hErr != INVALID_HANDLE_VALUE) {
        WriteFile(hErr, buf, static_cast<DWORD>(strlen(buf)), &written, nullptr);
    }
    Win32LogExceptionStack(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

LIBUI_API void Win32InstallUtilityTopLevelFilter(const char* appLabelUtf8) {
    s_UtilitySehTag = (appLabelUtf8 && appLabelUtf8[0] != '\0') ? appLabelUtf8 : "Solstice";
    SetUnhandledExceptionFilter(Win32UtilityTopLevelExceptionFilter);
}

LIBUI_API void Win32InitCrashDiagnostics() {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
}

LIBUI_API void Win32LogExceptionStack(_EXCEPTION_POINTERS* ep) {
    WriteCrashLine("Call stack:\r\n");
    WriteMiniDumpMaybe(ep);
    LogStackX64(ep);
}

} // namespace LibUI::Tools

#endif // _WIN32
