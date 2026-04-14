#pragma once

#include <Solstice.hxx>
#include <string>

namespace Solstice::Plugin {

/**
 * RAII wrapper for a native shared library (DLL / .so / .dylib).
 */
class SOLSTICE_API DynamicLibrary {
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

} // namespace Solstice::Plugin
