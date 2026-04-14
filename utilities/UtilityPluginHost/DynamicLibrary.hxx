#pragma once

#include <string>

namespace Solstice::UtilityPluginHost {

class DynamicLibrary {
public:
    DynamicLibrary() = default;
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;
    ~DynamicLibrary();

    bool Load(const std::string& pathUtf8, std::string* outError = nullptr);
    void Unload();
    bool IsLoaded() const { return m_Handle != nullptr; }

    void* GetSymbol(const char* symbolName) const;

    const std::string& GetPath() const { return m_Path; }

private:
    void* m_Handle{nullptr};
    std::string m_Path;
};

} // namespace Solstice::UtilityPluginHost
