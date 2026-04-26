#pragma once

#include <Solstice/NativeLoad/DynamicLibrary.hxx>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace Solstice::UtilityPluginHost {

using DynamicLibrary = NativeLoad::DynamicLibrary;

struct ModuleSummary {
    std::uint32_t Id{0};
    std::string PathUtf8;
    std::string DisplayName;
};

/// Optional C symbols resolved after load (GetName recommended for display label).
struct PluginAbiSymbols {
    const char* GetName{nullptr};
    const char* OnLoad{nullptr};
    const char* OnUnload{nullptr};
};

/**
 * Loads native plugin DLLs (.dll / .so / .dylib) for desktop utilities.
 * Thread-safe. Hot-reload: BeginHotReloadSession then CompleteHotReloadSession with a different file path
 * (on Windows the replacement must not be the same path as the loaded image).
 */
class UtilityPluginHost {
public:
    UtilityPluginHost() = default;
    UtilityPluginHost(const UtilityPluginHost&) = delete;
    UtilityPluginHost& operator=(const UtilityPluginHost&) = delete;

    /**
     * Load one module and invoke OnLoad if exported. Returns plugin id, or 0 on failure.
     */
    std::uint32_t Load(const std::string& pathUtf8, const PluginAbiSymbols& abi, std::string* outError = nullptr);

    /**
     * Scan `dir` for native modules; attempts load for each matching extension.
     * Appends (path, message) pairs to outFailures for files that failed to load.
     */
    void LoadAllFromDirectory(const std::string& dirUtf8, const PluginAbiSymbols& abi,
        std::vector<std::pair<std::string, std::string>>& outFailures);

    bool Unload(std::uint32_t id);
    void UnloadAll();

    bool IsLoaded(std::uint32_t id) const;
    void* GetSymbol(std::uint32_t id, const char* symbolName) const;
    bool GetPath(std::uint32_t id, std::string* outPath) const;
    std::size_t GetLoadedCount() const;

    void EnumerateModules(std::vector<ModuleSummary>& out) const;

    bool BeginHotReloadSession(std::uint32_t id, std::string* outError = nullptr);
    bool CompleteHotReloadSession(std::uint32_t id, const std::string& replacementPathUtf8, const PluginAbiSymbols& abi,
        std::string* outError = nullptr);
    void AbortHotReloadSession(std::uint32_t id);
    bool IsHotReloadPending(std::uint32_t id) const;

private:
    struct LoadedEntry {
        std::uint32_t Id{0};
        std::string PathUtf8;
        std::string DisplayName;
        PluginAbiSymbols Abi{};
        DynamicLibrary Library;
    };

    static void InvokeOnUnload(DynamicLibrary& lib, const char* symbolOnUnload);
    static void InvokeOnLoad(DynamicLibrary& lib, const char* symbolOnLoad);
    static std::string ResolveDisplayName(DynamicLibrary& lib, const std::string& pathUtf8, const PluginAbiSymbols& abi);

    mutable std::mutex m_Mutex;
    std::vector<LoadedEntry> m_Modules;
    std::uint32_t m_NextId{1};
    std::unordered_set<std::uint32_t> m_HotReloadPending;
};

} // namespace Solstice::UtilityPluginHost
