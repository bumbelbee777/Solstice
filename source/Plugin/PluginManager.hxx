#pragma once

#include <Plugin/DynamicLibrary.hxx>
#include <Plugin/Plugin.hxx>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace Solstice::Plugin {

struct LoadedNativeModule {
    std::uint32_t Id{0};
    std::string PathUtf8;
    DynamicLibrary Library;
    PluginState State{PluginState::Loaded};
};

class SOLSTICE_API PluginManager {
public:
    static PluginManager& Instance();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * Load a native shared library. Returns new plugin id, or 0 on failure.
     * Thread-safe.
     */
    std::uint32_t LoadNativeModule(const std::string& pathUtf8, std::string* outError = nullptr);

    bool UnloadNativeModule(std::uint32_t id);
    void UnloadAll();

    bool IsLoaded(std::uint32_t id) const;
    void* GetSymbol(std::uint32_t id, const char* symbolName) const;
    bool GetPath(std::uint32_t id, std::string* outPath) const;

    std::size_t GetLoadedCount() const;

    /**
     * Hot-reload: Begin marks the module for replacement; Complete loads the new DLL and swaps the handle
     * in place (plugin id is preserved). Begin/Complete must be paired. Abort cancels a pending session.
     * On Windows, replacement must be a different file than the currently loaded image (copy to a new path if needed).
     */
    bool BeginHotReloadSession(std::uint32_t id, std::string* outError = nullptr);
    bool CompleteHotReloadSession(std::uint32_t id, const std::string& replacementPathUtf8, std::string* outError = nullptr);
    void AbortHotReloadSession(std::uint32_t id);
    bool IsHotReloadPending(std::uint32_t id) const;

private:
    PluginManager() = default;
    ~PluginManager();

    mutable std::mutex m_Mutex;
    std::vector<LoadedNativeModule> m_Modules;
    std::uint32_t m_NextId{1};
    std::unordered_set<std::uint32_t> m_HotReloadPending;
};

} // namespace Solstice::Plugin
