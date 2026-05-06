#include "LibUI/Tools/Win32ExceptionDiag.hxx"
#include "LibUI/Core/Core.hxx"

#if defined(_WIN32)

#include <dbghelp.h>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

namespace LibUI::Tools {

namespace {

char s_CrashLogName[128] = "SolsticeCrash.log";
char s_FullDumpEnvName[128] = "SOLSTICE_FULL_DUMP";
char s_CrashDumpPrefix[128] = "Solstice";

void RefreshCrashConfigFromCore() {
    const LibUI::Core::RuntimeConfig cfg = LibUI::Core::GetRuntimeConfig();
    strncpy_s(s_CrashLogName, sizeof(s_CrashLogName), cfg.CrashLogFilename.c_str(), _TRUNCATE);
    strncpy_s(s_FullDumpEnvName, sizeof(s_FullDumpEnvName), cfg.FullDumpEnvVar.c_str(), _TRUNCATE);
    strncpy_s(s_CrashDumpPrefix, sizeof(s_CrashDumpPrefix), cfg.CrashDumpPrefix.c_str(), _TRUNCATE);
}

void WriteCrashChunk(const char* data, int len) {
    if (len <= 0) {
        return;
    }
    DWORD written = 0;
    const HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    if (hErr != nullptr && hErr != INVALID_HANDLE_VALUE) {
        WriteFile(hErr, data, static_cast<DWORD>(len), &written, nullptr);
    }
    char tmpPath[MAX_PATH * 2]{};
    const DWORD n = GetTempPathA(static_cast<DWORD>(sizeof(tmpPath)), tmpPath);
    if (n > 0 && n < sizeof(tmpPath) && strcat_s(tmpPath, sizeof(tmpPath), s_CrashLogName) == 0) {
        const HANDLE hFile = CreateFileA(tmpPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, data, static_cast<DWORD>(len), &written, nullptr);
            CloseHandle(hFile);
        }
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
    if (!EnvTruthy(s_FullDumpEnvName) || !ep) {
        return;
    }
    char path[MAX_PATH * 2]{};
    DWORD plen = GetTempPathA(static_cast<DWORD>(sizeof(path)), path);
    if (plen == 0 || plen >= sizeof(path)) {
        return;
    }
    char suffix[64]{};
    _snprintf_s(suffix, sizeof(suffix), _TRUNCATE, "%s_%08lx.dmp", s_CrashDumpPrefix,
        static_cast<unsigned long>(GetTickCount()));
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

static std::string s_UtilitySehTag = "Solstice";
static PVOID s_VectoredHandlerHandle = nullptr;
static volatile LONG s_ShutdownMode = 0;

bool IsBenignFirstChanceCode(DWORD code) {
    switch (code) {
    case 0x40010006u: // DBG_PRINTEXCEPTION_C
    case 0x406D1388u: // MSVC thread naming exception
    case 0x80000003u: // breakpoint (assert/debug trap)
    case 0xE06D7363u: // C++ exception
        return true;
    default:
        return false;
    }
}

LONG WINAPI Win32VectoredCrashLogger(struct _EXCEPTION_POINTERS* ep) {
    if (InterlockedCompareExchange(&s_ShutdownMode, 0, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!ep || !ep->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (IsBenignFirstChanceCode(ep->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    static volatile LONG once = 0;
    if (InterlockedCompareExchange(&once, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char buf[384]{};
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s VECTORED FATAL: code=0x%08lX addr=%p\r\n",
        s_UtilitySehTag.empty() ? "Solstice" : s_UtilitySehTag.c_str(),
        static_cast<unsigned long>(ep->ExceptionRecord->ExceptionCode), ep->ExceptionRecord->ExceptionAddress);
    WriteCrashLine(buf);
    Win32LogExceptionStack(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI Win32UtilityTopLevelExceptionFilter(_EXCEPTION_POINTERS* ep) {
    if (InterlockedCompareExchange(&s_ShutdownMode, 0, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (!ep || !ep->ExceptionRecord) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (IsBenignFirstChanceCode(ep->ExceptionRecord->ExceptionCode)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    char buf[384]{};
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%s FATAL: code=0x%08lX addr=%p\r\n",
        s_UtilitySehTag.empty() ? "Solstice" : s_UtilitySehTag.c_str(),
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
    RefreshCrashConfigFromCore();
    if (appLabelUtf8 && appLabelUtf8[0] != '\0') {
        s_UtilitySehTag = appLabelUtf8;
    } else {
        s_UtilitySehTag = LibUI::Core::GetRuntimeConfig().AppDisplayName;
    }
    SetUnhandledExceptionFilter(Win32UtilityTopLevelExceptionFilter);
}

LIBUI_API void Win32InitCrashDiagnostics() {
    RefreshCrashConfigFromCore();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    if (!s_VectoredHandlerHandle) {
        s_VectoredHandlerHandle = AddVectoredExceptionHandler(1, Win32VectoredCrashLogger);
    }
}

LIBUI_API void Win32CrashDiagEnterShutdownMode() {
    InterlockedExchange(&s_ShutdownMode, 1);
}

LIBUI_API void Win32LogExceptionStack(_EXCEPTION_POINTERS* ep) {
    WriteCrashLine("Call stack:\r\n");
    WriteMiniDumpMaybe(ep);
    LogStackX64(ep);
}

} // namespace LibUI::Tools

#endif // _WIN32
