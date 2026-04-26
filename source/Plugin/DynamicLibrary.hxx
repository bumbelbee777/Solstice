#pragma once

#include <Solstice.hxx>
#include <Solstice/NativeLoad/DynamicLibrary.hxx>
#include <string>

namespace Solstice::Plugin {

/**
 * RAII wrapper for a native shared library (DLL / .so / .dylib).
 * Forwards to SDK LibNativeLoad (composition preserves SOLSTICE_API on this translation unit).
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
    bool IsLoaded() const { return m_Impl.IsLoaded(); }

    /** Returns nullptr if missing or not loaded. */
    void* GetSymbol(const char* symbolName) const;

    const std::string& GetPath() const { return m_Impl.GetPath(); }

private:
    NativeLoad::DynamicLibrary m_Impl;
};

} // namespace Solstice::Plugin
