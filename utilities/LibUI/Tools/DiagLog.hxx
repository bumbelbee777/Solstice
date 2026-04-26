#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if !defined(LIBUI_API)
#if defined(LIBUI_STATIC)
#define LIBUI_API
#elif defined(_WIN32) || defined(_WIN64)
#ifdef LIBUI_EXPORTS
#define LIBUI_API __declspec(dllexport)
#else
#define LIBUI_API __declspec(dllimport)
#endif
#else
#define LIBUI_API
#endif
#endif

namespace LibUI::Tools {

/// Reads process env (Win32: `GetEnvironmentVariableA` matches what PowerShell sets; `getenv` can disagree).
#if defined(_WIN32)
inline bool EnvVarTruthy(const char* name) {
    char buf[128];
    const DWORD n = GetEnvironmentVariableA(name, buf, static_cast<DWORD>(sizeof(buf)));
    if (n == 0) {
        return false;
    }
    if (n >= sizeof(buf)) {
        return true;
    }
    if (buf[0] == '\0') {
        return false;
    }
    if (buf[0] == '0' && buf[1] == '\0') {
        return false;
    }
    return true;
}
#else
inline bool EnvVarTruthy(const char* name) {
    const char* e = std::getenv(name);
    return e && e[0] != '\0' && e[0] != '0';
}
#endif

/// Writes one line to **stdout** and, when `SOLSTICE_DIAG_LOG=1`, appends to `SolsticeTools_diag.txt` under the temp dir.
/// Implemented in `DiagLog.cxx` (single TU in LibUI) to avoid duplicate static state across EXE/DLL.
LIBUI_API void DiagLogLine(std::string_view line);
LIBUI_API void DiagLogLine(const char* line);

} // namespace LibUI::Tools
