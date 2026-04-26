#include "LibUI/Tools/DiagLog.hxx"

#include <cstdio>
#include <cstring>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace LibUI::Tools {
namespace {

unsigned long DiagProcessId() {
#if defined(_WIN32)
    return static_cast<unsigned long>(GetCurrentProcessId());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

/// Single open + state for the optional diag file (static LibUI: one TU per executable).
std::FILE* DiagLogTempFilePtr() {
    static std::FILE* f = nullptr;
    static int state = 0; // 0=uninit, 1=ok, -1=fail
    if (state != 0) {
        return state == 1 ? f : nullptr;
    }
    state = -1;
    if (!EnvVarTruthy("SOLSTICE_DIAG_LOG")) {
        return nullptr;
    }
    std::error_code ec;
    const std::filesystem::path path = std::filesystem::temp_directory_path(ec) / "SolsticeTools_diag.txt";
    if (ec) {
        return nullptr;
    }
    const std::string pathStr = path.string();
#if defined(_MSC_VER)
    if (fopen_s(&f, pathStr.c_str(), "a") != 0) {
        f = nullptr;
        return nullptr;
    }
#else
    f = std::fopen(pathStr.c_str(), "a");
    if (f == nullptr) {
        return nullptr;
    }
#endif
    state = 1;
    std::fprintf(f, "\n--- SolsticeTools diag (pid=%lu) ---\n", DiagProcessId());
    std::fflush(f);
    return f;
}

} // namespace

LIBUI_API void DiagLogLine(std::string_view line) {
    if (line.empty()) {
        return;
    }
    if (std::FILE* tf = DiagLogTempFilePtr()) {
        std::fwrite(line.data(), 1, line.size(), tf);
        std::fputc('\n', tf);
        std::fflush(tf);
    }
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

LIBUI_API void DiagLogLine(const char* line) {
    if (!line) {
        return;
    }
    DiagLogLine(std::string_view(line));
}

} // namespace LibUI::Tools
