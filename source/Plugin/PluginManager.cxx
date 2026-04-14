#include <Plugin/PluginManager.hxx>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace Solstice::Plugin {

PluginManager& PluginManager::Instance() {
    static PluginManager s_Instance;
    return s_Instance;
}

PluginManager::~PluginManager() {
    UnloadAll();
}

std::uint32_t PluginManager::LoadNativeModule(const std::string& pathUtf8, std::string* outError) {
    DynamicLibrary lib;
    std::string errLocal;
    if (!lib.Load(pathUtf8, outError ? outError : &errLocal)) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_Mutex);
    const std::uint32_t id = m_NextId++;
    LoadedNativeModule mod;
    mod.Id = id;
    mod.PathUtf8 = lib.GetPath();
    mod.Library = std::move(lib);
    mod.State = PluginState::Loaded;
    m_Modules.push_back(std::move(mod));
    return id;
}

bool PluginManager::UnloadNativeModule(std::uint32_t id) {
    if (id == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        return false;
    }
    m_HotReloadPending.erase(id);
    m_Modules.erase(it);
    return true;
}

void PluginManager::UnloadAll() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Modules.clear();
    m_HotReloadPending.clear();
}

bool PluginManager::IsLoaded(std::uint32_t id) const {
    if (id == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    return std::any_of(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
}

void* PluginManager::GetSymbol(std::uint32_t id, const char* symbolName) const {
    if (id == 0 || !symbolName) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        return nullptr;
    }
    return it->Library.GetSymbol(symbolName);
}

bool PluginManager::GetPath(std::uint32_t id, std::string* outPath) const {
    if (!outPath) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        return false;
    }
    *outPath = it->PathUtf8;
    return true;
}

std::size_t PluginManager::GetLoadedCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Modules.size();
}

bool PluginManager::BeginHotReloadSession(std::uint32_t id, std::string* outError) {
    if (id == 0) {
        if (outError) {
            *outError = "Invalid plugin id";
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        if (outError) {
            *outError = "Unknown plugin id";
        }
        return false;
    }
    m_HotReloadPending.insert(id);
    return true;
}

bool PluginManager::CompleteHotReloadSession(std::uint32_t id, const std::string& replacementPathUtf8, std::string* outError) {
    if (id == 0 || replacementPathUtf8.empty()) {
        if (outError) {
            *outError = "Invalid plugin id or empty replacement path";
        }
        return false;
    }

    std::string errLocal;
    std::string* err = outError ? outError : &errLocal;

    std::lock_guard<std::mutex> lock(m_Mutex);
    if (m_HotReloadPending.find(id) == m_HotReloadPending.end()) {
        *err = "No active hot reload session for this id";
        return false;
    }

    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedNativeModule& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        m_HotReloadPending.erase(id);
        *err = "Plugin id not found";
        return false;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path oldPath(it->PathUtf8);
    const fs::path newPath(replacementPathUtf8);
    if (fs::exists(oldPath, ec) && fs::exists(newPath, ec)) {
        ec.clear();
        if (fs::equivalent(oldPath, newPath, ec)) {
            *err = "Replacement path is the same file as the loaded module; use a different file path (copy the DLL first on Windows).";
            return false;
        }
    }

    DynamicLibrary newLib;
    if (!newLib.Load(replacementPathUtf8, err)) {
        return false;
    }

    it->Library = std::move(newLib);
    it->PathUtf8 = it->Library.GetPath();
    it->State = PluginState::Loaded;
    m_HotReloadPending.erase(id);
    return true;
}

void PluginManager::AbortHotReloadSession(std::uint32_t id) {
    if (id == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_HotReloadPending.erase(id);
}

bool PluginManager::IsHotReloadPending(std::uint32_t id) const {
    if (id == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_HotReloadPending.find(id) != m_HotReloadPending.end();
}

} // namespace Solstice::Plugin
