#pragma once

#include <string>

namespace Solstice::NativeLoad {

/**
 * RAII wrapper for a native shared library (DLL / .so / .dylib).
 * UTF-8 paths on all platforms (wide Win32 APIs under the hood on Windows).
 */
class DynamicLibrary {
public:
    DynamicLibrary() = default;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;
    ~DynamicLibrary();

    /** Load from UTF-8 path. On failure, optionally writes a short message into outError. */
    bool Load(const std::string& pathUtf8, std::string* outError = nullptr);
    void Unload();
    bool IsLoaded() const { return m_Handle != nullptr; }

    /** Returns nullptr if missing or not loaded. */
    void* GetSymbol(const char* symbolName) const;

    const std::string& GetPath() const { return m_Path; }

private:
    void* m_Handle{nullptr};
    std::string m_Path;
};

} // namespace Solstice::NativeLoad
