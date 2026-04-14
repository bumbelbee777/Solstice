#include <Plugin/DynamicLibrary.hxx>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <utility>
#if defined(_WIN32)
#include <vector>
#endif

namespace Solstice::Plugin {

#if defined(_WIN32)
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::vector<wchar_t> buf(static_cast<size_t>(n));
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buf.data(), n);
    return std::wstring(buf.data());
}
#endif

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : m_Handle(other.m_Handle), m_Path(std::move(other.m_Path)) {
    other.m_Handle = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        Unload();
        m_Handle = other.m_Handle;
        m_Path = std::move(other.m_Path);
        other.m_Handle = nullptr;
    }
    return *this;
}

DynamicLibrary::~DynamicLibrary() {
    Unload();
}

bool DynamicLibrary::Load(const std::string& pathUtf8, std::string* outError) {
    Unload();
    if (pathUtf8.empty()) {
        if (outError) {
            *outError = "empty path";
        }
        return false;
    }
#if defined(_WIN32)
    const std::wstring wpath = Utf8ToWide(pathUtf8);
    if (wpath.empty()) {
        if (outError) {
            *outError = "invalid utf-8 path";
        }
        return false;
    }
    HMODULE mod = LoadLibraryW(wpath.c_str());
    if (!mod) {
        if (outError) {
            *outError = "LoadLibraryW failed";
        }
        return false;
    }
    m_Handle = mod;
#else
    void* lib = dlopen(pathUtf8.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        if (outError) {
            const char* err = dlerror();
            if (err) {
                *outError = err;
            } else {
                *outError = "dlopen failed";
            }
        }
        return false;
    }
    m_Handle = lib;
#endif
    m_Path = pathUtf8;
    return true;
}

void DynamicLibrary::Unload() {
    if (!m_Handle) {
        return;
    }
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(m_Handle));
#else
    dlclose(m_Handle);
#endif
    m_Handle = nullptr;
    m_Path.clear();
}

void* DynamicLibrary::GetSymbol(const char* symbolName) const {
    if (!m_Handle || !symbolName) {
        return nullptr;
    }
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), symbolName));
#else
    return dlsym(m_Handle, symbolName);
#endif
}

} // namespace Solstice::Plugin
