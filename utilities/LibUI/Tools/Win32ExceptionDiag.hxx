#pragma once

#include "DiagLog.hxx"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace LibUI::Tools {

#if defined(_WIN32)

/// Install a minimal top-level SEH filter (async-safe logging + ``Win32LogExceptionStack``). Call after
/// ``Win32InitCrashDiagnostics`` so stack resolution is ready. ``appLabelUtf8`` prefixes the fatal line (e.g. ``"MovieMaker"``).
LIBUI_API void Win32InstallUtilityTopLevelFilter(const char* appLabelUtf8);

/// Initializes dbghelp symbol resolution (call once near startup, before relying on ``Win32LogExceptionStack``).
LIBUI_API void Win32InitCrashDiagnostics();

/// Async-safe enough for a top-level SEH filter: uses stack buffers, `WriteFile` + `OutputDebugStringA` only.
/// Does not call `DiagLogLine`. Optional full minidump when env `SOLSTICE_FULL_DUMP` is truthy.
LIBUI_API void Win32LogExceptionStack(struct _EXCEPTION_POINTERS* ep);

#else

inline void Win32InitCrashDiagnostics() {}
inline void Win32LogExceptionStack(void*) {}

#endif

} // namespace LibUI::Tools
