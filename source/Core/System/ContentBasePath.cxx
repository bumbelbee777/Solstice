#include "ContentBasePath.hxx"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cstdlib>
#include <unistd.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <cstdint>
#endif

#include <filesystem>

namespace Solstice::Core {

std::filesystem::path GetExecutableDirectory() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buf).parent_path();
#elif defined(__linux__)
    char path[PATH_MAX]{};
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) {
        return {};
    }
    path[len] = '\0';
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    char pathBuf[1024];
    std::uint32_t sz = sizeof(pathBuf);
    if (_NSGetExecutablePath(pathBuf, &sz) != 0) {
        return {};
    }
    return std::filesystem::path(pathBuf).parent_path();
#else
    return {};
#endif
}

} // namespace Solstice::Core
