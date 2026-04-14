#include "PluginLoader.hxx"

#include <SDL3/SDL.h>
#include <imgui.h>

#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

std::vector<SharponLoadedPlugin> g_Loaded;
std::vector<std::pair<std::string, std::string>> g_PluginLoadErrors;

#ifdef _WIN32
static std::string Win32LoadErrorMessage() {
    DWORD err = GetLastError();
    if (err == 0) {
        return "unknown error";
    }
    char* buf = nullptr;
    const DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, nullptr);
    std::string out;
    if (n && buf) {
        out.assign(buf, buf + n);
    } else {
        out = "code " + std::to_string(static_cast<unsigned long>(err));
    }
    if (buf) {
        LocalFree(buf);
    }
    while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) {
        out.pop_back();
    }
    return out;
}
#endif

#ifdef _WIN32
using SharponPluginGetNameFn = const char*(__cdecl*)();
using SharponPluginVoidFn = void(__cdecl*)();
#else
using SharponPluginGetNameFn = const char* (*)();
using SharponPluginVoidFn = void (*)();
#endif

void TryLoad(const std::filesystem::path& dllPath) {
#ifdef _WIN32
    HMODULE m = LoadLibraryW(dllPath.wstring().c_str());
    if (!m) {
        g_PluginLoadErrors.push_back({dllPath.string(), Win32LoadErrorMessage()});
        return;
    }
    auto* getName = reinterpret_cast<SharponPluginGetNameFn>(GetProcAddress(m, "SharponPlugin_GetName"));
    auto* onLoad = reinterpret_cast<SharponPluginVoidFn>(GetProcAddress(m, "SharponPlugin_OnLoad"));
#else
    void* m = dlopen(dllPath.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m) {
        const char* e = dlerror();
        g_PluginLoadErrors.push_back({dllPath.string(), e ? e : "dlopen failed"});
        return;
    }
    auto* getName = reinterpret_cast<SharponPluginGetNameFn>(dlsym(m, "SharponPlugin_GetName"));
    auto* onLoad = reinterpret_cast<SharponPluginVoidFn>(dlsym(m, "SharponPlugin_OnLoad"));
#endif
    SharponLoadedPlugin p;
    p.Path = dllPath.string();
    p.Module = m;
    if (getName) {
        const char* n = getName();
        p.Name = n ? n : dllPath.filename().string();
    } else {
        p.Name = dllPath.filename().string();
    }
    if (onLoad) {
        onLoad();
    }
    g_Loaded.push_back(std::move(p));
}

} // namespace

void SharponPlugins_LoadDefault() {
    SharponPlugins_UnloadAll();
    g_PluginLoadErrors.clear();
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
    if (base) {
        SDL_free((void*)base);
    }

    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
#ifdef _WIN32
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
        TryLoad(entry.path());
    }
}

void SharponPlugins_UnloadAll() {
    for (auto it = g_Loaded.rbegin(); it != g_Loaded.rend(); ++it) {
#ifdef _WIN32
        auto* onUnload = reinterpret_cast<SharponPluginVoidFn>(GetProcAddress(static_cast<HMODULE>(it->Module), "SharponPlugin_OnUnload"));
#else
        auto* onUnload = reinterpret_cast<SharponPluginVoidFn>(dlsym(it->Module, "SharponPlugin_OnUnload"));
#endif
        if (onUnload) {
            onUnload();
        }
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(it->Module));
#else
        dlclose(it->Module);
#endif
    }
    g_Loaded.clear();
}

void SharponPlugins_DrawPanel(bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(420, 220), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Plugins", pOpen)) {
        ImGui::TextUnformatted("Loaded from ./plugins next to Sharpon.exe");
        if (ImGui::Button("Reload")) {
            SharponPlugins_LoadDefault();
        }
        ImGui::Separator();
        if (!g_PluginLoadErrors.empty()) {
            ImGui::TextUnformatted("Failed to load (see path + system error):");
            for (const auto& fe : g_PluginLoadErrors) {
                ImGui::BulletText("%s\n  %s", fe.first.c_str(), fe.second.c_str());
            }
            ImGui::Separator();
        }
        if (g_Loaded.empty()) {
            ImGui::TextUnformatted("No plugins loaded.");
        }
        for (size_t i = 0; i < g_Loaded.size(); ++i) {
            ImGui::BulletText("%s — %s", g_Loaded[i].Name.c_str(), g_Loaded[i].Path.c_str());
        }
    }
    ImGui::End();
}
