#include "UtilityPluginHost.hxx"

#include <algorithm>
#include <filesystem>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
using PluginGetNameFn = const char*(__cdecl*)();
using PluginVoidFn = void(__cdecl*)();
#else
using PluginGetNameFn = const char* (*)();
using PluginVoidFn = void (*)();
#endif

namespace Solstice::UtilityPluginHost {

void UtilityPluginHost::InvokeOnUnload(DynamicLibrary& lib, const char* symbolOnUnload) {
    if (!symbolOnUnload) {
        return;
    }
    auto* fn = reinterpret_cast<PluginVoidFn>(lib.GetSymbol(symbolOnUnload));
    if (fn) {
        fn();
    }
}

void UtilityPluginHost::InvokeOnLoad(DynamicLibrary& lib, const char* symbolOnLoad) {
    if (!symbolOnLoad) {
        return;
    }
    auto* fn = reinterpret_cast<PluginVoidFn>(lib.GetSymbol(symbolOnLoad));
    if (fn) {
        fn();
    }
}

std::string UtilityPluginHost::ResolveDisplayName(DynamicLibrary& lib, const std::string& pathUtf8, const PluginAbiSymbols& abi) {
    if (abi.GetName) {
        auto* getName = reinterpret_cast<PluginGetNameFn>(lib.GetSymbol(abi.GetName));
        if (getName) {
            const char* n = getName();
            if (n && n[0]) {
                return std::string(n);
            }
        }
    }
    namespace fs = std::filesystem;
    return fs::path(pathUtf8).filename().string();
}

std::uint32_t UtilityPluginHost::Load(const std::string& pathUtf8, const PluginAbiSymbols& abi, std::string* outError) {
    DynamicLibrary lib;
    std::string errLocal;
    if (!lib.Load(pathUtf8, outError ? outError : &errLocal)) {
        return 0;
    }

    InvokeOnLoad(lib, abi.OnLoad);

    LoadedEntry ent;
    ent.Abi = abi;
    ent.PathUtf8 = lib.GetPath();
    ent.DisplayName = ResolveDisplayName(lib, ent.PathUtf8, abi);
    ent.Library = std::move(lib);

    std::lock_guard<std::mutex> lock(m_Mutex);
    ent.Id = m_NextId++;
    m_Modules.push_back(std::move(ent));
    return m_Modules.back().Id;
}

void UtilityPluginHost::LoadAllFromDirectory(const std::string& dirUtf8, const PluginAbiSymbols& abi,
    std::vector<std::pair<std::string, std::string>>& outFailures) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir(dirUtf8);
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
#if defined(_WIN32)
        std::string ext = entry.path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".dll") {
            continue;
        }
#else
        const auto ext = entry.path().extension().string();
        if (ext != ".so" && ext != ".dylib") {
            continue;
        }
#endif
        const std::string p = entry.path().string();
        std::string err;
        if (Load(p, abi, &err) == 0) {
            outFailures.push_back({p, err.empty() ? "load failed" : err});
        }
    }
}

bool UtilityPluginHost::Unload(std::uint32_t id) {
    if (id == 0) {
        return false;
    }
    LoadedEntry removed;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
        if (it == m_Modules.end()) {
            return false;
        }
        m_HotReloadPending.erase(id);
        removed = std::move(*it);
        m_Modules.erase(it);
    }
    InvokeOnUnload(removed.Library, removed.Abi.OnUnload);
    return true;
}

void UtilityPluginHost::UnloadAll() {
    std::vector<LoadedEntry> mods;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        mods = std::move(m_Modules);
        m_Modules.clear();
        m_HotReloadPending.clear();
    }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
        InvokeOnUnload(it->Library, it->Abi.OnUnload);
    }
}

bool UtilityPluginHost::IsLoaded(std::uint32_t id) const {
    if (id == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    return std::any_of(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
}

void* UtilityPluginHost::GetSymbol(std::uint32_t id, const char* symbolName) const {
    if (id == 0 || !symbolName) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        return nullptr;
    }
    return it->Library.GetSymbol(symbolName);
}

bool UtilityPluginHost::GetPath(std::uint32_t id, std::string* outPath) const {
    if (!outPath) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        return false;
    }
    *outPath = it->PathUtf8;
    return true;
}

std::size_t UtilityPluginHost::GetLoadedCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Modules.size();
}

void UtilityPluginHost::EnumerateModules(std::vector<ModuleSummary>& out) const {
    out.clear();
    std::lock_guard<std::mutex> lock(m_Mutex);
    out.reserve(m_Modules.size());
    for (const auto& m : m_Modules) {
        ModuleSummary s;
        s.Id = m.Id;
        s.PathUtf8 = m.PathUtf8;
        s.DisplayName = m.DisplayName;
        out.push_back(std::move(s));
    }
}

bool UtilityPluginHost::BeginHotReloadSession(std::uint32_t id, std::string* outError) {
    if (id == 0) {
        if (outError) {
            *outError = "Invalid plugin id";
        }
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
    if (it == m_Modules.end()) {
        if (outError) {
            *outError = "Unknown plugin id";
        }
        return false;
    }
    m_HotReloadPending.insert(id);
    return true;
}

bool UtilityPluginHost::CompleteHotReloadSession(std::uint32_t id, const std::string& replacementPathUtf8, const PluginAbiSymbols& abi,
    std::string* outError) {
    if (id == 0 || replacementPathUtf8.empty()) {
        if (outError) {
            *outError = "Invalid plugin id or empty replacement path";
        }
        return false;
    }

    std::string errLocal;
    std::string* err = outError ? outError : &errLocal;

    DynamicLibrary newLib;
    if (!newLib.Load(replacementPathUtf8, err)) {
        return false;
    }

    DynamicLibrary oldLib;
    PluginAbiSymbols oldAbi{};
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_HotReloadPending.find(id) == m_HotReloadPending.end()) {
            *err = "No active hot reload session for this id";
            return false;
        }
        const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
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

        oldAbi = it->Abi;
        oldLib = std::move(it->Library);
        it->Library = std::move(newLib);
        it->PathUtf8 = it->Library.GetPath();
        it->Abi = abi;
        it->DisplayName = ResolveDisplayName(it->Library, it->PathUtf8, abi);
        m_HotReloadPending.erase(id);
    }

    InvokeOnUnload(oldLib, oldAbi.OnUnload);

    DynamicLibrary* libForLoad = nullptr;
    const char* onLoadSym = abi.OnLoad;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        const auto it2 = std::find_if(m_Modules.begin(), m_Modules.end(), [id](const LoadedEntry& m) { return m.Id == id; });
        if (it2 != m_Modules.end()) {
            libForLoad = &it2->Library;
        }
    }
    if (libForLoad && onLoadSym) {
        InvokeOnLoad(*libForLoad, onLoadSym);
    }
    return true;
}

void UtilityPluginHost::AbortHotReloadSession(std::uint32_t id) {
    if (id == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_HotReloadPending.erase(id);
}

bool UtilityPluginHost::IsHotReloadPending(std::uint32_t id) const {
    if (id == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_HotReloadPending.find(id) != m_HotReloadPending.end();
}

} // namespace Solstice::UtilityPluginHost
