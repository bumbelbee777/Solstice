// Solstice MovieMaker (SMM) — MVP: import assets, edit minimal PARALLAX scene, import/export .prlx.
// Video export: MG → OpenGL → ffmpeg (MP4/MOV).

#include "FfmpegUtil.hxx"
#include "LibUI/Graphics/PreviewTexture.hxx"
#include "Media/SmmAudio.hxx"
#include "Media/SmmImage.hxx"
#include "SmmGltf.hxx"
#include "SmmFileOps.hxx"
#include "SmmKeyframePresets.hxx"
#include "SmmMg2DPanel.hxx"
#include "SmmParallaxAuthoringPanel.hxx"
#include "SmmSessionAuthoring.hxx"
#include "Editing/SmmCurveGraphBridge.hxx"
#include "Editing/SmmParticleEditor.hxx"
#include "Editing/SmmParticleScene.hxx"
#include "UI/Panels/ComingSoonPanels.hxx"
#include "UI/Panels/PreviewPanels.hxx"
#include "UI/WorkspaceState.hxx"
#include "VideoExport.hxx"
#include "Workflow/Workflow.hxx"
#include "LibUI/AssetBrowser/AssetBrowser.hxx"
#include "LibUI/Core/Core.hxx"
#include "LibUI/Docking/Docking.hxx"
#include "LibUI/Widgets/Widgets.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Shell/GlWindow.hxx"
#include "LibUI/Theme/Theme.hxx"
#include "LibUI/Timeline/TimelineModel.hxx"
#include "LibUI/Timeline/TimelineWidget.hxx"
#include "LibUI/Workspace/PanelRegistry.hxx"
#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"

#include <Parallax/MGRaster.hxx>
#include <Parallax/Parallax.hxx>
#include <Parallax/ParallaxEditorHelpers.hxx>
#include <Parallax/ParallaxScene.hxx>

#include "UtilityPluginAbi.hxx"
#include "UtilityPluginHost.hxx"

#include <Solstice/EditorAudio/EditorAudio.hxx>
#include <Solstice/EditorAudio/EditorRecovery.hxx>

#include <Math/Vector.hxx>
#include <Physics/Lighting/LightSource.hxx>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

const LibUI::FileDialogs::FileFilter kSmatFileFilters[] = {
    {"Solstice material (.smat)", "smat"},
};

std::mutex g_PendingMutex;
std::vector<std::string> g_PendingImportPaths;

void QueuePath(std::string p) {
    std::lock_guard<std::mutex> lock(g_PendingMutex);
    g_PendingImportPaths.push_back(std::move(p));
}

void DrainImports(Solstice::Parallax::DevSessionAssetResolver& resolver, std::vector<LibUI::AssetBrowser::Entry>& browser) {
    std::vector<std::string> paths;
    {
        std::lock_guard<std::mutex> lock(g_PendingMutex);
        paths.swap(g_PendingImportPaths);
    }
    for (const auto& p : paths) {
        if (resolver.ImportFile(p)) {
            LibUI::AssetBrowser::Entry e;
            e.DisplayName = std::filesystem::path(p).filename().string();
            e.Hash = resolver.HashFromPath(p);
            browser.push_back(std::move(e));
        }
    }
}

Solstice::UtilityPluginHost::UtilityPluginHost g_MovieMakerPlugins;
std::vector<std::pair<std::string, std::string>> g_MovieMakerPluginLoadErrors;

static void ReloadSmmKeyframePresets(
    const std::filesystem::path& smmJsonPath, std::vector<Smm::Keyframe::KeyframeCurvePreset>& out) {
    std::vector<std::filesystem::path> roots;
    const char* bp = SDL_GetBasePath();
    if (bp) {
        roots.push_back(std::filesystem::path(bp) / "presets");
        SDL_free((void*)bp);
    }
    if (!smmJsonPath.empty()) {
        roots.push_back(smmJsonPath.parent_path() / "presets");
    }
    Smm::Keyframe::ScanCurvePresetsFromRoots(roots, out);
}

static std::vector<Smm::Keyframe::KeyframeCurvePreset> g_smmKeyframePresets;

void LoadMovieMakerPlugins() {
    g_MovieMakerPlugins.UnloadAll();
    g_MovieMakerPluginLoadErrors.clear();
    const char* base = SDL_GetBasePath();
    std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
    if (base) {
        SDL_free((void*)base);
    }
    Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
    abi.GetName = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_GETNAME;
    abi.OnLoad = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONLOAD;
    abi.OnUnload = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONUNLOAD;
    g_MovieMakerPlugins.LoadAllFromDirectory(dir.string(), abi, g_MovieMakerPluginLoadErrors);
}

void MovieMakerPluginsDrawPanel(bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(440, 240), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Plugins##SMM", pOpen)) {
        ImGui::TextUnformatted("Native plugins: ./plugins next to MovieMaker");
        if (ImGui::Button("Reload##mmplug")) {
            LoadMovieMakerPlugins();
        }
        ImGui::Separator();
        if (!g_MovieMakerPluginLoadErrors.empty()) {
            for (const auto& fe : g_MovieMakerPluginLoadErrors) {
                ImGui::BulletText("%s\n  %s", fe.first.c_str(), fe.second.c_str());
            }
            ImGui::Separator();
        }
        std::vector<Solstice::UtilityPluginHost::ModuleSummary> mods;
        g_MovieMakerPlugins.EnumerateModules(mods);
        if (mods.empty()) {
            ImGui::TextUnformatted("No plugins loaded.");
        }
        for (const auto& m : mods) {
            ImGui::BulletText("%s — %s", m.DisplayName.c_str(), m.PathUtf8.c_str());
        }
    }
    ImGui::End();
}

Solstice::Parallax::ChannelIndex FindChannelForAttribute(Solstice::Parallax::ParallaxScene& scene,
    Solstice::Parallax::ElementIndex element, std::string_view attribute, Solstice::Parallax::AttributeType type) {
    const auto& channels = scene.GetChannels();
    for (size_t i = 0; i < channels.size(); ++i) {
        if (channels[i].Element == element && channels[i].AttributeName == attribute && channels[i].ValueType == type) {
            return static_cast<Solstice::Parallax::ChannelIndex>(i);
        }
    }
    return Solstice::Parallax::PARALLAX_INVALID_INDEX;
}

void PushRecentPath(std::vector<std::string>& recent, const std::string& p, size_t maxN = 8) {
    auto it = std::find(recent.begin(), recent.end(), p);
    if (it != recent.end()) {
        recent.erase(it);
    }
    recent.insert(recent.begin(), p);
    if (recent.size() > maxN) {
        recent.resize(maxN);
    }
    LibUI::Core::RecentPathPush(p.c_str());
}

struct MovieMakerProjectState {
    std::string exportPath;
    std::string importPath;
    std::string folderPath;
    std::string ffmpegExe;
    std::vector<std::string> recentPrlx;
    std::optional<std::string> loadedFromPath;
    std::string videoExportPath;
    uint32_t videoWidth = 1280;
    uint32_t videoHeight = 720;
    uint32_t videoFps = 30;
    bool videoMp4 = true;
    uint64_t videoStartTick = 0;
    uint64_t videoEndTick = 0;
    bool compressPrlx = false;
    /// Background `.prlx` recovery snapshot interval (seconds) while the scene is dirty.
    uint32_t recoveryIntervalSec = 60;
};

static std::filesystem::path MovieMakerDefaultProjectPath() {
    const char* b = SDL_GetBasePath();
    if (b) {
        std::filesystem::path p(b);
        return p / "solstice_moviemaker_project.smm.json";
    }
    return std::filesystem::path("solstice_moviemaker_project.smm.json");
}

static std::string EscapeJsonFragment(std::string_view s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"') {
            o += '\\';
        }
        o += c;
    }
    return o;
}

static std::string TrimOuterQuotesAndWhitespace(std::string s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

static std::filesystem::path ResolveSceneSavePath(std::string_view requestedPath, const std::filesystem::path& projectPath) {
    const std::string clean = TrimOuterQuotesAndWhitespace(std::string(requestedPath));
    std::filesystem::path scenePath = clean.empty() ? std::filesystem::path("scene.prlx") : std::filesystem::path(clean);
    if (scenePath.is_relative()) {
        const std::filesystem::path base = projectPath.parent_path().empty() ? std::filesystem::current_path() : projectPath.parent_path();
        scenePath = base / scenePath;
    }
    return scenePath;
}

static bool SaveMovieMakerProjectToPath(const std::filesystem::path& path, const MovieMakerProjectState& st,
    std::string* error = nullptr) {
    try {
        std::error_code ec;
        const std::filesystem::path parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent, ec);
            if (ec) {
                if (error) {
                    *error = "Failed to create project directory: " + ec.message();
                }
                return false;
            }
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error) {
                *error = "Failed to open project file for writing: " + path.string();
            }
            return false;
        }

        out << "{\"version\":1,\"exportPath\":\"" << EscapeJsonFragment(st.exportPath) << "\",\"importPath\":\""
            << EscapeJsonFragment(st.importPath) << "\",\"folderPath\":\"" << EscapeJsonFragment(st.folderPath)
            << "\",\"ffmpegExe\":\"" << EscapeJsonFragment(st.ffmpegExe) << "\",\"videoExportPath\":\""
            << EscapeJsonFragment(st.videoExportPath) << "\",\"videoWidth\":" << st.videoWidth << ",\"videoHeight\":"
            << st.videoHeight << ",\"videoFps\":" << st.videoFps << ",\"videoMp4\":" << (st.videoMp4 ? "true" : "false")
            << ",\"compressPrlx\":" << (st.compressPrlx ? "true" : "false")
            << ",\"videoStartTick\":" << st.videoStartTick << ",\"videoEndTick\":" << st.videoEndTick
            << ",\"recoveryIntervalSec\":" << st.recoveryIntervalSec << ",\"recent\":[";
        for (size_t i = 0; i < st.recentPrlx.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            out << "\"" << EscapeJsonFragment(st.recentPrlx[i]) << "\"";
        }
        out << "]}\n";
        out.flush();
        if (!out) {
            if (error) {
                *error = "Failed while writing project file: " + path.string();
            }
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (error) {
            *error = e.what();
        }
        return false;
    }
}

static bool ParseJsonKeyU32(const std::string& j, const char* key, uint32_t& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    uint64_t v = 0;
    while (p < j.size() && j[p] >= '0' && j[p] <= '9') {
        v = v * 10ull + static_cast<uint64_t>(j[p] - '0');
        ++p;
    }
    if (v > 0xffffffffull) {
        return false;
    }
    out = static_cast<uint32_t>(v);
    return true;
}

static bool ParseJsonKeyU64(const std::string& j, const char* key, uint64_t& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    out = 0;
    while (p < j.size() && j[p] >= '0' && j[p] <= '9') {
        out = out * 10ull + static_cast<uint64_t>(j[p] - '0');
        ++p;
    }
    return true;
}

static bool ParseJsonKeyBool(const std::string& j, const char* key, bool& out) {
    std::string pat = std::string("\"") + key + "\":";
    size_t p = j.find(pat);
    if (p == std::string::npos) {
        return false;
    }
    p += pat.size();
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) {
        ++p;
    }
    if (j.compare(p, 4, "true") == 0) {
        out = true;
        return true;
    }
    if (j.compare(p, 5, "false") == 0) {
        out = false;
        return true;
    }
    return false;
}

static bool ParseMovieMakerProjectJson(const std::string& j, MovieMakerProjectState& st) {
    auto getStr = [&](const char* key) -> std::optional<std::string> {
        std::string k = std::string("\"") + key + "\":\"";
        size_t pos = j.find(k);
        if (pos == std::string::npos) {
            return std::nullopt;
        }
        pos += k.size();
        std::string out;
        while (pos < j.size()) {
            char c = j[pos++];
            if (c == '"') {
                break;
            }
            if (c == '\\' && pos < j.size()) {
                c = j[pos++];
            }
            out += c;
        }
        return out;
    };
    if (auto v = getStr("exportPath")) {
        st.exportPath = std::move(*v);
    }
    if (auto v = getStr("importPath")) {
        st.importPath = std::move(*v);
    }
    if (auto v = getStr("folderPath")) {
        st.folderPath = std::move(*v);
    }
    if (auto v = getStr("ffmpegExe")) {
        st.ffmpegExe = std::move(*v);
    }
    if (auto v = getStr("videoExportPath")) {
        st.videoExportPath = std::move(*v);
    }
    uint32_t vw = st.videoWidth;
    if (ParseJsonKeyU32(j, "videoWidth", vw)) {
        st.videoWidth = vw;
    }
    uint32_t vh = st.videoHeight;
    if (ParseJsonKeyU32(j, "videoHeight", vh)) {
        st.videoHeight = vh;
    }
    uint32_t vf = st.videoFps;
    if (ParseJsonKeyU32(j, "videoFps", vf)) {
        st.videoFps = vf;
    }
    bool vmp4 = st.videoMp4;
    if (ParseJsonKeyBool(j, "videoMp4", vmp4)) {
        st.videoMp4 = vmp4;
    }
    uint64_t vst = st.videoStartTick;
    if (ParseJsonKeyU64(j, "videoStartTick", vst)) {
        st.videoStartTick = vst;
    }
    uint64_t vet = st.videoEndTick;
    if (ParseJsonKeyU64(j, "videoEndTick", vet)) {
        st.videoEndTick = vet;
    }
    bool cpr = st.compressPrlx;
    if (ParseJsonKeyBool(j, "compressPrlx", cpr)) {
        st.compressPrlx = cpr;
    }
    uint32_t ris = st.recoveryIntervalSec;
    if (ParseJsonKeyU32(j, "recoveryIntervalSec", ris)) {
        st.recoveryIntervalSec = (std::max)(10u, (std::min)(ris, 3600u));
    }
    st.recentPrlx.clear();
    size_t rpos = j.find("\"recent\":[");
    if (rpos != std::string::npos) {
        rpos += 10;
        while (rpos < j.size() && j[rpos] != ']') {
            if (j[rpos] == '"') {
                ++rpos;
                std::string item;
                while (rpos < j.size()) {
                    char c = j[rpos++];
                    if (c == '"') {
                        break;
                    }
                    if (c == '\\' && rpos < j.size()) {
                        c = j[rpos++];
                    }
                    item += c;
                }
                if (!item.empty()) {
                    st.recentPrlx.push_back(std::move(item));
                }
            } else {
                ++rpos;
            }
        }
    }
    return true;
}

static bool LoadMovieMakerProjectFromPath(const std::filesystem::path& path, MovieMakerProjectState& st) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return ParseMovieMakerProjectJson(oss.str(), st);
}

std::mutex g_ProjectMutex;
std::optional<MovieMakerProjectState> g_PendingProjectApply;
std::optional<std::filesystem::path> g_PendingProjectSaveAsPath;
std::optional<std::filesystem::path> g_PendingPrlxExportPath;

#ifdef _WIN32
static void CopyAsciiToSystemClipboard(const std::string& s) {
    if (!OpenClipboard(nullptr)) {
        return;
    }
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s.size() + 1);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            std::memcpy(p, s.c_str(), s.size() + 1);
            GlobalUnlock(h);
            SetClipboardData(CF_TEXT, h);
        }
    }
    CloseClipboard();
}
#endif

void ImportFolderRecursive(const std::filesystem::path& dir, Solstice::Parallax::DevSessionAssetResolver& resolver,
                           std::vector<LibUI::AssetBrowser::Entry>& browser) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (resolver.ImportFile(entry.path())) {
            LibUI::AssetBrowser::Entry e;
            e.DisplayName = entry.path().filename().string();
            e.Hash = resolver.HashFromPath(entry.path().string());
            browser.push_back(std::move(e));
        }
    }
}

static const LibUI::FileDialogs::FileFilter kSmmProjectFilters[] = {
    {"MovieMaker project", "json"},
    {"All", "*"},
};

static void ApplyMovieMakerProjectState(MovieMakerProjectState& st, char* exportPathBuf, size_t exportPathBufSize,
    char* importPathBuf, size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf,
    size_t ffmpegExeBufSize, char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH,
    uint32_t& videoFps, bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, uint32_t& recoveryIntervalSecOut) {
    std::snprintf(exportPathBuf, exportPathBufSize, "%s", st.exportPath.c_str());
    std::snprintf(importPathBuf, importPathBufSize, "%s", st.importPath.c_str());
    std::snprintf(folderPathBuf, folderPathBufSize, "%s", st.folderPath.c_str());
    std::snprintf(ffmpegExeBuf, ffmpegExeBufSize, "%s", st.ffmpegExe.c_str());
    std::snprintf(videoExportPathBuf, videoExportPathBufSize, "%s", st.videoExportPath.c_str());
    videoW = st.videoWidth;
    videoH = st.videoHeight;
    videoFps = st.videoFps;
    videoMp4 = st.videoMp4;
    videoStartTick = st.videoStartTick;
    videoEndTick = st.videoEndTick;
    compressPrlx = st.compressPrlx;
    recentPrlxPaths = std::move(st.recentPrlx);
    if (st.loadedFromPath.has_value()) {
        activeProjectPath = std::filesystem::path(*st.loadedFromPath);
    }
    recoveryIntervalSecOut = (std::max)(10u, (std::min)(st.recoveryIntervalSec, 3600u));
}

static void DrainPendingMovieMakerProject(char* exportPathBuf, size_t exportPathBufSize, char* importPathBuf,
    size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf, size_t ffmpegExeBufSize,
    char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH, uint32_t& videoFps,
    bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, uint32_t& recoveryIntervalSecOut) {
    std::optional<MovieMakerProjectState> pending;
    {
        std::lock_guard<std::mutex> lock(g_ProjectMutex);
        pending = std::move(g_PendingProjectApply);
    }
    if (pending) {
        ApplyMovieMakerProjectState(*pending, exportPathBuf, exportPathBufSize, importPathBuf, importPathBufSize,
            folderPathBuf, folderPathBufSize, ffmpegExeBuf, ffmpegExeBufSize, videoExportPathBuf, videoExportPathBufSize,
            videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx, recentPrlxPaths,
            activeProjectPath, recoveryIntervalSecOut);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    LibUI::Shell::GlWindow glWindow;
    if (!LibUI::Shell::CreateUtilityGlWindow(
            glWindow, "Solstice Movie Maker — Technology Preview 1", 1280, 720, SDL_WINDOW_RESIZABLE, 1)) {
        std::cerr << "CreateUtilityGlWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = glWindow.window;

    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI::Core::Initialize failed" << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(glWindow);
        SDL_Quit();
        return 1;
    }
    if (!Solstice::EditorAudio::Init()) {
        LibUI::Tools::DiagLogLine("[SMM] EditorAudio::Init failed (waveform / scrub preview will be disabled).");
    }
    LibUI::Tools::DiagLogLine("[SMM] LibUI initialized.");

    LoadMovieMakerPlugins();
    LibUI::Tools::DiagLogLine("[SMM] Plugins loaded.");

    Solstice::Parallax::DevSessionAssetResolver resolver;
    std::vector<LibUI::AssetBrowser::Entry> assetEntries;
    auto scene = Solstice::Parallax::CreateScene(6000);
    LibUI::Tools::DiagLogLine("[SMM] Scene created.");
    int assetSelected = -1;
    uint64_t timeTicks = 0;
    char folderPathBuf[512] = "";
    char exportPathBuf[1024] = "scene.prlx";
    char importPathBuf[1024] = "scene.prlx";
    char ffmpegExeBuf[512] = "ffmpeg";
    char videoExportPathBuf[1024] = "export.mp4";
    uint32_t videoW = 1280;
    uint32_t videoH = 720;
    uint32_t videoFps = 30;
    bool videoMp4 = true;
    uint64_t videoStartTick = 0;
    uint64_t videoEndTick = 0;
    bool compressPrlx = false;
    std::string videoExportLog;
    std::string ffmpegLog; // read-only display via LibUI multiline helper
    std::string smmStatus;
    LibUI::Graphics::PreviewTextureRgba smmScene3dPreviewTex{};
    LibUI::Graphics::PreviewTextureRgba smmParticleSpriteTex{};
    Smm::UI::WorkspaceState smmWorkspace{};
    std::vector<Smm::Editing::EditorTrackBinding> smmBindings{};
    Smm::Editing::AppSessionContext smmSession{};
    LibUI::Workspace::PanelRegistry smmPanelReg{};
    Smm::UI::Panels::RegisterPlaceholderPanels(smmPanelReg, smmWorkspace, &smmSession);
    std::vector<std::string> recentPrlxPaths;
    int elementSelected = -1;
    int mgElementSelected = -1;
    bool smmFitSpriteSizeOnImageImport = true;
    bool sceneDirty = false;
    bool applySfmTheme = true;
    bool showExportWindow = false;
    bool showMmPluginsPanel = false;
    bool showMmAboutPanel = false;
    enum class MmUnsavedKind { None, QuitApp, NewScene, ImportPrlx };
    MmUnsavedKind mmUnsavedPrompt = MmUnsavedKind::None;
    std::optional<std::string> mmPendingImportPath;
    char channelAttrBuf[128] = "Intensity";
    int channelValueTypeCombo = 0;
    std::filesystem::path activeProjectPath = MovieMakerDefaultProjectPath();
    float smmNominalCompW = 1920.f;
    float smmNominalCompH = 1080.f;
    bool projectPathChosen = false;
    std::string lastFfmpegShellCommand;
    uint32_t smmRecoveryIntervalSecU32 = 60;
    std::vector<Solstice::MovieMaker::VideoExportParams> smmVideoRenderQueue;
    std::optional<std::string> pendingVideoImportPath;

    {
        MovieMakerProjectState boot;
        if (LoadMovieMakerProjectFromPath(activeProjectPath, boot)) {
            ApplyMovieMakerProjectState(boot, exportPathBuf, sizeof(exportPathBuf), importPathBuf, sizeof(importPathBuf),
                folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf), videoExportPathBuf,
                sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick,
                compressPrlx, recentPrlxPaths, activeProjectPath, smmRecoveryIntervalSecU32);
        }
    }

    Smm::Authoring::SessionState smmAuthoringSession;
    {
        std::string authErr;
        (void)Smm::Authoring::LoadSessionAuthoring(Smm::Authoring::AuthoringSidecarPathForProject(activeProjectPath), smmAuthoringSession, &authErr);
    }
    ReloadSmmKeyframePresets(activeProjectPath, g_smmKeyframePresets);

    auto persistMovieMakerProjectFields = [&](std::string* error = nullptr) -> bool {
        MovieMakerProjectState pst;
        pst.exportPath = exportPathBuf;
        pst.importPath = importPathBuf;
        pst.folderPath = folderPathBuf;
        pst.ffmpegExe = ffmpegExeBuf;
        pst.videoExportPath = videoExportPathBuf;
        pst.videoWidth = videoW;
        pst.videoHeight = videoH;
        pst.videoFps = videoFps;
        pst.videoMp4 = videoMp4;
        pst.videoStartTick = videoStartTick;
        pst.videoEndTick = videoEndTick;
        pst.compressPrlx = compressPrlx;
        pst.recoveryIntervalSec = smmRecoveryIntervalSecU32;
        pst.recentPrlx = recentPrlxPaths;
        if (!SaveMovieMakerProjectToPath(activeProjectPath, pst, error)) {
            return false;
        }
        {
            std::string authErr;
            (void)Smm::Authoring::SaveSessionAuthoring(
                Smm::Authoring::AuthoringSidecarPathForProject(activeProjectPath), smmAuthoringSession, &authErr);
        }
        return true;
    };

    auto requestSaveMovieMakerProject = [&]() {
        if (!projectPathChosen) {
            LibUI::FileDialogs::ShowSaveFile(
                window, "Save MovieMaker project",
                [](std::optional<std::string> path) {
                    if (!path.has_value()) {
                        return;
                    }
                    std::lock_guard<std::mutex> lock(g_ProjectMutex);
                    g_PendingProjectSaveAsPath = std::filesystem::path(*path);
                },
                std::span<const LibUI::FileDialogs::FileFilter>(kSmmProjectFilters));
            return;
        }

        std::string saveError;
        const std::filesystem::path sceneSavePath = ResolveSceneSavePath(exportPathBuf, activeProjectPath);
        std::snprintf(exportPathBuf, sizeof(exportPathBuf), "%s", sceneSavePath.string().c_str());
        bool ok = persistMovieMakerProjectFields(&saveError);

        if (ok) {
            std::error_code dirError;
            const std::filesystem::path sceneParent = sceneSavePath.parent_path();
            if (!sceneParent.empty()) {
                std::filesystem::create_directories(sceneParent, dirError);
            }
            if (dirError) {
                ok = false;
                saveError = "Failed to create scene directory: " + dirError.message();
            }
        }

        if (ok) {
            {
                std::string particleSyncErr;
                (void)Smm::Particles::SyncEditorToParallaxScene(
                    *scene, smmWorkspace.particleEditorState, resolver, particleSyncErr);
            }
            Solstice::Parallax::ParallaxError sceneError = Solstice::Parallax::ParallaxError::None;
            if (Solstice::Parallax::SaveScene(*scene, sceneSavePath, compressPrlx, &sceneError)) {
                sceneDirty = false;
                PushRecentPath(recentPrlxPaths, sceneSavePath.string());
                std::string recentSaveError;
                (void)persistMovieMakerProjectFields(&recentSaveError);
            } else {
                ok = false;
                saveError = "Scene save failed with ParallaxError " + std::to_string(static_cast<int>(sceneError));
            }
        }

        if (ok) {
            ffmpegLog = "Saved project " + activeProjectPath.string() + " and scene " + sceneSavePath.string() + "\n" + ffmpegLog;
        } else {
            ffmpegLog = "Save failed: " + saveError + "\n" + ffmpegLog;
        }
    };

    auto requestOpenMovieMakerProject = [&]() {
        LibUI::FileDialogs::ShowOpenFile(
            window, "Open MovieMaker project",
            [](std::optional<std::string> path) {
                if (!path.has_value()) {
                    return;
                }
                MovieMakerProjectState st;
                std::string pth = std::move(path.value());
                if (!LoadMovieMakerProjectFromPath(std::filesystem::path(pth), st)) {
                    return;
                }
                st.loadedFromPath = std::move(pth);
                std::lock_guard<std::mutex> lock(g_ProjectMutex);
                g_PendingProjectApply = std::move(st);
            },
            std::span<const LibUI::FileDialogs::FileFilter>(kSmmProjectFilters));
    };

    auto requestExportParallaxScene = [&]() {
        LibUI::FileDialogs::ShowSaveFile(
            window, "Export PARALLAX scene",
            [](std::optional<std::string> path) {
                if (!path.has_value()) {
                    return;
                }
                std::lock_guard<std::mutex> lock(g_ProjectMutex);
                g_PendingPrlxExportPath = std::filesystem::path(*path);
            });
    };

    auto performPrlxImportFromBuffers = [&]() -> bool {
        Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
        auto loaded = Solstice::Parallax::LoadScene(importPathBuf, &resolver, &err);
        if (!loaded) {
            return false;
        }
        Smm::ClearSceneUndo();
        Smm::Editing::ResetParticleEditUndo();
        scene = std::move(loaded);
        Smm::Particles::LoadParticleEditorFromScene(*scene, smmWorkspace.particleEditorState);
        PushRecentPath(recentPrlxPaths, std::string(importPathBuf));
        (void)persistMovieMakerProjectFields();
        sceneDirty = false;
        elementSelected = scene->GetElements().empty() ? -1 : 0;
        mgElementSelected = scene->GetMGElements().empty() ? -1 : 0;
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
        return true;
    };

    auto commitNewParallaxScene = [&]() {
        Smm::ClearSceneUndo();
        Smm::Editing::ResetParticleEditUndo();
        scene = Solstice::Parallax::CreateScene(6000);
        timeTicks = 0;
        elementSelected = scene->GetElements().empty() ? -1 : 0;
        mgElementSelected = scene->GetMGElements().empty() ? -1 : 0;
        sceneDirty = false;
        ffmpegLog = "New Parallax scene.\n" + ffmpegLog;
    };

    auto requestNewParallaxScene = [&]() {
        if (sceneDirty) {
            mmUnsavedPrompt = MmUnsavedKind::NewScene;
        } else {
            commitNewParallaxScene();
        }
    };

    std::string smmSdlBasePathUtf8;
    {
        const char* sdlB = SDL_GetBasePath();
        if (sdlB) {
            smmSdlBasePathUtf8 = sdlB;
            SDL_free((void*)sdlB);
        }
    }
    const std::filesystem::path smmRecoveryDir = Solstice::EditorAudio::FileRecovery::RecoveryDir(
        smmSdlBasePathUtf8.empty() ? nullptr : smmSdlBasePathUtf8.c_str(), "smm");
    bool smmPrlxRecoveryOpen = !Solstice::EditorAudio::FileRecovery::List(smmRecoveryDir, "prlx").empty();
    double smmRecoveryAccumSec = 0.0;

    auto tryWritePrlxRecoverySnapshot = [&]() {
        if (!scene) {
            return;
        }
        std::vector<std::byte> prlxBytes;
        std::string particleSyncErrR;
        (void)Smm::Particles::SyncEditorToParallaxScene(*scene, smmWorkspace.particleEditorState, resolver, particleSyncErrR);
        Solstice::Parallax::ParallaxError ser = Solstice::Parallax::ParallaxError::None;
        if (Solstice::Parallax::SaveSceneToBytes(*scene, prlxBytes, compressPrlx, &ser) && !prlxBytes.empty()) {
            std::string wre;
            (void)Solstice::EditorAudio::FileRecovery::WriteSnapshot(
                smmRecoveryDir, "prlx", std::span<const std::byte>(prlxBytes.data(), prlxBytes.size()), &wre);
            smmPrlxRecoveryOpen = !Solstice::EditorAudio::FileRecovery::List(smmRecoveryDir, "prlx").empty();
            if (!wre.empty()) {
                smmStatus = "Recovery: " + wre;
            } else {
                smmStatus = "Recovery snapshot written to " + smmRecoveryDir.string();
            }
        }
    };

    auto restorePrlxFromRecoveryBytes = [&](std::span<const std::byte> bytes) -> bool {
        Solstice::Parallax::ParallaxError perr = Solstice::Parallax::ParallaxError::None;
        if (!Solstice::Parallax::LoadSceneFromBytes(*scene, bytes, &perr)) {
            smmStatus = "Autosave restore failed: ParallaxError " + std::to_string(static_cast<int>(perr)) + ".";
            return false;
        }
        Smm::Particles::LoadParticleEditorFromScene(*scene, smmWorkspace.particleEditorState);
        elementSelected = scene->GetElements().empty() ? -1 : 0;
        mgElementSelected = scene->GetMGElements().empty() ? -1 : 0;
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
        sceneDirty = true;
        smmStatus = "Restored Parallax scene from recovery autosave.";
        return true;
    };

    bool running = true;
    while (running) {
        static bool firstFrameTrace = true;
        if (firstFrameTrace) {
            LibUI::Tools::DiagLogLine("[SMM] Entering first frame.");
            firstFrameTrace = false;
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            LibUI::Core::ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) {
                if (sceneDirty) {
                    mmUnsavedPrompt = MmUnsavedKind::QuitApp;
                } else {
                    running = false;
                }
            }
        }

        DrainPendingMovieMakerProject(exportPathBuf, sizeof(exportPathBuf), importPathBuf, sizeof(importPathBuf),
            folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf), videoExportPathBuf,
            sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx,
            recentPrlxPaths, activeProjectPath, smmRecoveryIntervalSecU32);
        static std::filesystem::path s_smmAuthProject;
        if (s_smmAuthProject != activeProjectPath) {
            s_smmAuthProject = activeProjectPath;
            (void)Smm::Authoring::LoadSessionAuthoring(
                Smm::Authoring::AuthoringSidecarPathForProject(activeProjectPath), smmAuthoringSession, nullptr);
            ReloadSmmKeyframePresets(activeProjectPath, g_smmKeyframePresets);
        }
        if (pendingVideoImportPath) {
            const std::string p = std::move(*pendingVideoImportPath);
            pendingVideoImportPath.reset();
            if (resolver.ImportFile(std::filesystem::path(p))) {
                smmStatus = "Video bytes imported to dev session: " + p;
            } else {
                smmStatus = "Video import failed (unreadable path?): " + p;
            }
        }
        if (activeProjectPath != MovieMakerDefaultProjectPath()) {
            projectPathChosen = true;
        }
        std::optional<std::filesystem::path> pendingSaveAsPath;
        {
            std::lock_guard<std::mutex> lock(g_ProjectMutex);
            pendingSaveAsPath = std::move(g_PendingProjectSaveAsPath);
            g_PendingProjectSaveAsPath.reset();
        }
        if (pendingSaveAsPath) {
            activeProjectPath = std::move(*pendingSaveAsPath);
            projectPathChosen = true;
            requestSaveMovieMakerProject();
        }
        std::optional<std::filesystem::path> pendingPrlxExportPath;
        {
            std::lock_guard<std::mutex> lock(g_ProjectMutex);
            pendingPrlxExportPath = std::move(g_PendingPrlxExportPath);
            g_PendingPrlxExportPath.reset();
        }
        if (pendingPrlxExportPath) {
            const std::filesystem::path outPath = ResolveSceneSavePath(pendingPrlxExportPath->string(), activeProjectPath);
            std::error_code ec;
            if (!outPath.parent_path().empty()) {
                std::filesystem::create_directories(outPath.parent_path(), ec);
            }
            if (ec) {
                smmStatus = "PARALLAX export failed: " + ec.message();
            } else {
                Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
                {
                    std::string particleSyncErr;
                    (void)Smm::Particles::SyncEditorToParallaxScene(
                        *scene, smmWorkspace.particleEditorState, resolver, particleSyncErr);
                }
                if (Solstice::Parallax::SaveScene(*scene, outPath, compressPrlx, &err)) {
                    std::snprintf(exportPathBuf, sizeof(exportPathBuf), "%s", outPath.string().c_str());
                    PushRecentPath(recentPrlxPaths, outPath.string());
                    sceneDirty = false;
                    smmStatus = "Exported PARALLAX scene: " + outPath.string();
                    (void)persistMovieMakerProjectFields();
                } else {
                    smmStatus = "PARALLAX export failed with ParallaxError " + std::to_string(static_cast<int>(err));
                }
            }
        }

        LibUI::Core::NewFrame();

        ImGuiIO& io_mm = ImGui::GetIO();

        smmRecoveryAccumSec += (double)io_mm.DeltaTime;
        if (sceneDirty && smmRecoveryAccumSec >= static_cast<double>(smmRecoveryIntervalSecU32)) {
            smmRecoveryAccumSec = 0.0;
            tryWritePrlxRecoverySnapshot();
        }

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::Begin("SMMRoot", nullptr,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene")) {
                    requestNewParallaxScene();
                }
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                    requestSaveMovieMakerProject();
                }
                if (ImGui::MenuItem("Open Project...")) {
                    requestOpenMovieMakerProject();
                }
                if (ImGui::MenuItem("Import glTF to selected Actor...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::QueueGltfImportPath(std::move(*path));
                            }
                        },
                        Smm::kGltfFilters);
                }
                if (ImGui::MenuItem("Export selected Actor glTF...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export selected glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::QueueGltfExportPath(std::move(*path));
                            }
                        },
                        Smm::kGltfFilters);
                }
                if (ImGui::MenuItem("Import raster to selected MG sprite...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import raster texture", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Image::QueueRasterImportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                }
                if (ImGui::MenuItem("Export selected MG sprite texture...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export MG sprite Texture bytes", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Image::QueueRasterExportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterExportFilters));
                }
                if (ImGui::MenuItem("Import audio to selected Audio source...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import audio asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Audio::QueueAudioImportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioImportFilters));
                }
                if (ImGui::MenuItem("Import video to session...")) {
                    static const LibUI::FileDialogs::FileFilter kSmmVideoImportFilters[] = {
                        {"MP4 or MOV", "mp4,mov"},
                        {"All", "*"},
                    };
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import video to session (bytes + hash; reference for pipelines / compositing tools)",
                        [&pendingVideoImportPath](std::optional<std::string> path) {
                            if (path) {
                                pendingVideoImportPath = std::move(*path);
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(kSmmVideoImportFilters));
                }
                if (ImGui::MenuItem("Write recovery snapshot now")) {
                    tryWritePrlxRecoverySnapshot();
                }
                if (ImGui::MenuItem("Export selected Audio source...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export AudioAsset bytes", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Audio::QueueAudioExportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioExportFilters));
                }
                if (ImGui::MenuItem("Export PARALLAX scene...")) {
                    requestExportParallaxScene();
                }
                if (ImGui::MenuItem("Export...")) {
                    showExportWindow = true;
                }
                if (!recentPrlxPaths.empty()) {
                    ImGui::Separator();
                    ImGui::TextDisabled("Recent .prlx");
                    for (size_t ri = 0; ri < recentPrlxPaths.size(); ++ri) {
                        ImGui::PushID(static_cast<int>(ri));
                        if (ImGui::MenuItem(recentPrlxPaths[ri].c_str())) {
                            std::snprintf(importPathBuf, sizeof(importPathBuf), "%s", recentPrlxPaths[ri].c_str());
                        }
                        ImGui::PopID();
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                const bool particleFocus = Smm::Editing::IsParticleEditPanelFocused();
                const bool canUndo = particleFocus ? Smm::Editing::CanParticleEditUndo() : Smm::g_sceneByteUndo.CanUndo();
                const bool canRedo = particleFocus ? Smm::Editing::CanParticleEditRedo() : Smm::g_sceneByteUndo.CanRedo();
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                    if (particleFocus) {
                        if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, true)) {
                            smmStatus = "Particle: undo.";
                        }
                    } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, true, elementSelected, timeTicks, false, 0, 0)) {
                        smmStatus = "Undo (scene).";
                        sceneDirty = true;
                    }
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, canRedo)) {
                    if (particleFocus) {
                        if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, false)) {
                            smmStatus = "Particle: redo.";
                        }
                    } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, false, elementSelected, timeTicks, false, 0, 0)) {
                        smmStatus = "Redo (scene).";
                        sceneDirty = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                ImGui::MenuItem("Curve editor", nullptr, &smmWorkspace.showCurveEditorPanel);
                ImGui::MenuItem("Graph editor", nullptr, &smmWorkspace.showGraphEditorPanel);
                ImGui::MenuItem("Particles", nullptr, &smmWorkspace.showParticleEditorPanel);
                ImGui::MenuItem("Fluid volumes", nullptr, &smmWorkspace.showFluidVolumesPanel);
                ImGui::Separator();
                if (ImGui::MenuItem("Plugins")) {
                    showMmPluginsPanel = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) {
                    showMmAboutPanel = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (mmUnsavedPrompt != MmUnsavedKind::None) {
            ImGui::OpenPopup("MM_Unsaved");
        }
        if (ImGui::BeginPopupModal("MM_Unsaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const char* line = "Discard unsaved Parallax scene edits?";
            if (mmUnsavedPrompt == MmUnsavedKind::QuitApp) {
                line = "Quit with unsaved Parallax scene edits?";
            } else if (mmUnsavedPrompt == MmUnsavedKind::NewScene) {
                line = "Discard edits and create a new scene?";
            } else if (mmUnsavedPrompt == MmUnsavedKind::ImportPrlx) {
                line = "Discard edits and import the selected .prlx?";
            }
            ImGui::TextUnformatted(line);
            if (ImGui::Button("Discard", ImVec2(120, 0))) {
                const MmUnsavedKind k = mmUnsavedPrompt;
                mmUnsavedPrompt = MmUnsavedKind::None;
                ImGui::CloseCurrentPopup();
                if (k == MmUnsavedKind::QuitApp) {
                    running = false;
                } else if (k == MmUnsavedKind::NewScene) {
                    commitNewParallaxScene();
                } else if (k == MmUnsavedKind::ImportPrlx) {
                    if (mmPendingImportPath) {
                        std::snprintf(importPathBuf, sizeof(importPathBuf), "%s", mmPendingImportPath->c_str());
                        mmPendingImportPath.reset();
                        performPrlxImportFromBuffers();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                mmUnsavedPrompt = MmUnsavedKind::None;
                mmPendingImportPath.reset();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (smmPrlxRecoveryOpen) {
            ImGui::OpenPopup("SMM_PrlxRecovery");
        }
        if (ImGui::BeginPopupModal("SMM_PrlxRecovery", &smmPrlxRecoveryOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("A local PARALLAX autosave (recovery) is available. Restore it?");
            if (ImGui::Button("Restore", ImVec2(140, 0))) {
                std::vector<std::byte> buf;
                std::string re;
                if (Solstice::EditorAudio::FileRecovery::ReadLatest(smmRecoveryDir, "prlx", buf, &re)) {
                    if (restorePrlxFromRecoveryBytes(std::span<const std::byte>(buf.data(), buf.size()))) {
                        Solstice::EditorAudio::FileRecovery::ClearMatchingPrefix(smmRecoveryDir, "prlx");
                        smmPrlxRecoveryOpen = false;
                        ImGui::CloseCurrentPopup();
                    }
                } else {
                    smmStatus = "Could not read recovery: " + re;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Dismiss", ImVec2(140, 0))) {
                smmPrlxRecoveryOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        DrainImports(resolver, assetEntries);
        Smm::DrainPendingGltfOps(*scene, resolver, elementSelected, smmStatus, compressPrlx, sceneDirty);
        Smm::Image::DrainPendingRasterOps(
            *scene, resolver, mgElementSelected, smmFitSpriteSizeOnImageImport, smmStatus, compressPrlx, sceneDirty);
        Smm::Audio::DrainPendingAudioAssetOps(*scene, resolver, elementSelected, smmStatus, compressPrlx, sceneDirty);
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());

        uint64_t maxT = scene->GetTimelineDurationTicks() > 0 ? scene->GetTimelineDurationTicks() : 1;

        if (!io_mm.WantTextInput && io_mm.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
            requestSaveMovieMakerProject();
        }

        if (!io_mm.WantTextInput) {
            const uint64_t maxTk = scene->GetTimelineDurationTicks() > 0 ? scene->GetTimelineDurationTicks() : 1;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
                timeTicks = (timeTicks > 0) ? (timeTicks - 1) : 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
                timeTicks = std::min(maxTk, timeTicks + 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
                Solstice::MovieMaker::Workflow::JumpPlayheadToStart(timeTicks, scene->GetTimelineDurationTicks());
            }
            if (ImGui::IsKeyPressed(ImGuiKey_End, false)) {
                Solstice::MovieMaker::Workflow::JumpPlayheadToEnd(timeTicks, scene->GetTimelineDurationTicks());
            }
            const bool pFocus = Smm::Editing::IsParticleEditPanelFocused();
            if (io_mm.KeyCtrl && !io_mm.KeyAlt) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io_mm.KeyShift) {
                        if (pFocus) {
                            if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, false)) {
                                smmStatus = "Particle: redo.";
                            }
                        } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, false, elementSelected, timeTicks, false, 0, 0)) {
                            smmStatus = "Redo (scene).";
                            sceneDirty = true;
                        }
                    } else {
                        if (pFocus) {
                            if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, true)) {
                                smmStatus = "Particle: undo.";
                            }
                        } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, true, elementSelected, timeTicks, false, 0, 0)) {
                            smmStatus = "Undo (scene).";
                            sceneDirty = true;
                        }
                    }
                } else if (ImGui::IsKeyPressed(ImGuiKey_Y, false) && !io_mm.KeyShift) {
                    if (pFocus) {
                        if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, false)) {
                            smmStatus = "Particle: redo.";
                        }
                    } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, false, elementSelected, timeTicks, false, 0, 0)) {
                        smmStatus = "Redo (scene).";
                        sceneDirty = true;
                    }
                }
            }
        }

        if (ImGui::BeginTable("SMM_MainLayout", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 330.0f);
            ImGui::TableSetupColumn("Main", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextColumn();
            if (LibUI::Widgets::BeginTabBar("SMM_LeftTabs")) {
                if (LibUI::Widgets::BeginTabItem("Properties")) {
                    Solstice::Parallax::ParallaxSceneSummary sum{};
                    Solstice::Parallax::GetParallaxSceneSummary(*scene, sum);
                    ImGui::TextWrapped("Scene: %zu elements, %zu channels, %zu MG elements | %u tps, %llu ticks", sum.ElementCount,
                        sum.ChannelCount, sum.MGElementCount, sum.TicksPerSecond,
                        static_cast<unsigned long long>(sum.TimelineDurationTicks));
                    std::vector<Solstice::Parallax::ParallaxValidationMessage> val;
                    Solstice::Parallax::ValidateParallaxSceneEditing(*scene, val);
                    if (!val.empty() && ImGui::CollapsingHeader("Scene checks")) {
                        for (const auto& m : val) {
                            ImGui::BulletText("%s", m.Text.c_str());
                        }
                    }
                    Smm::DrawParallaxRootEnvironment(*scene, window, compressPrlx, sceneDirty);
                    if (ImGui::BeginListBox("Elements", ImVec2(-1, 110))) {
                        for (size_t i = 0; i < scene->GetElements().size(); ++i) {
                            const auto& el = scene->GetElements()[i];
                            std::string label = std::to_string(i) + " - " + el.Name;
                            bool sel = (elementSelected == static_cast<int>(i));
                            if (ImGui::Selectable(label.c_str(), sel)) {
                                elementSelected = static_cast<int>(i);
                            }
                        }
                        ImGui::EndListBox();
                    }
                    if (ImGui::Button("Duplicate selected") && elementSelected >= 0 &&
                        static_cast<size_t>(elementSelected) < scene->GetElements().size()) {
                        const auto& el = scene->GetElements()[static_cast<size_t>(elementSelected)];
                        const auto& schemas = scene->GetSchemas();
                        std::string typeName =
                            el.SchemaIndex < schemas.size() ? schemas[el.SchemaIndex].TypeName : std::string("LightElement");
                        Solstice::Parallax::AddElement(*scene, typeName, el.Name + " Copy", el.Parent);
                        sceneDirty = true;
                    }
                    if (elementSelected >= 0 && static_cast<size_t>(elementSelected) < scene->GetElements().size()) {
                        ImGui::TextUnformatted("Channel attribute");
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::InputText("##ChannelAttribute", channelAttrBuf, sizeof(channelAttrBuf));
                        ImGui::TextUnformatted("Channel value type");
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::Combo("##ChannelValueType", &channelValueTypeCombo, "float\0vec3\0\0");
                        if (ImGui::Button("Add keyframe at playhead")) {
                            const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
                            const Solstice::Parallax::AttributeType at = channelValueTypeCombo == 0
                                ? Solstice::Parallax::AttributeType::Float
                                : Solstice::Parallax::AttributeType::Vec3;
                            Solstice::Parallax::ChannelIndex ch = FindChannelForAttribute(*scene, el, channelAttrBuf, at);
                            if (ch == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                                ch = Solstice::Parallax::AddChannel(*scene, el, channelAttrBuf, at);
                            }
                            if (ch != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                                if (at == Solstice::Parallax::AttributeType::Float) {
                                    Solstice::Parallax::AddKeyframe(*scene, ch, timeTicks, Solstice::Parallax::AttributeValue{1.0f});
                                } else {
                                    Solstice::Parallax::AddKeyframe(*scene, ch, timeTicks,
                                        Solstice::Parallax::AttributeValue{Solstice::Math::Vec3(0.f, 0.f, 0.f)});
                                }
                                sceneDirty = true;
                            }
                        }
                    }
                    uint64_t dur = scene->GetTimelineDurationTicks();
                    ImGui::TextUnformatted("Duration (ticks)");
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputScalar("##DurationTicks", ImGuiDataType_U64, &dur)) {
                        scene->SetTimelineDurationTicks(dur);
                        sceneDirty = true;
                    }
                    uint32_t tps = scene->GetTicksPerSecond();
                    ImGui::TextUnformatted("Ticks/sec");
                    ImGui::SetNextItemWidth(-1.0f);
                    if (ImGui::InputScalar("##TicksPerSecond", ImGuiDataType_U32, &tps)) {
                        scene->SetTicksPerSecond(tps);
                        sceneDirty = true;
                    }
                    if (ImGui::Button("Add Light")) {
                        Solstice::Parallax::AddElement(*scene, "LightElement", "Light", 0);
                        sceneDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Camera")) {
                        Solstice::Parallax::AddElement(*scene, "CameraElement", "Camera", 0);
                        sceneDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Actor")) {
                        Solstice::Parallax::AddElement(*scene, "ActorElement", "Actor", 0);
                        sceneDirty = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add Audio")) {
                        const Solstice::Parallax::ElementIndex ai =
                            Solstice::Parallax::AddElement(*scene, "AudioSourceElement", "Audio", 0);
                        if (ai != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                            Solstice::Parallax::SetAttribute(*scene, ai, "Volume", Solstice::Parallax::AttributeValue{1.0f});
                            Solstice::Parallax::SetAttribute(*scene, ai, "Pitch", Solstice::Parallax::AttributeValue{1.0f});
                            elementSelected = static_cast<int>(ai);
                            sceneDirty = true;
                        }
                    }
                    ImGui::Separator();
                    ImGui::TextUnformatted("Motion graphics");
                    Smm::DrawMg2DCompTools(*scene, mgElementSelected, sceneDirty, compressPrlx, smmNominalCompW, smmNominalCompH);
                    if (ImGui::Button("Add MG Sprite")) {
                        const Solstice::Parallax::MGIndex idx = Solstice::Parallax::AddMGElement(*scene, "MGSpriteElement", "Sprite",
                            Solstice::Parallax::PARALLAX_INVALID_INDEX);
                        if (idx != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                            auto& rec = scene->GetMGElements()[idx];
                            rec.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(16.f, 16.f)};
                            rec.Attributes["Size"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(256.f, 256.f)};
                            mgElementSelected = static_cast<int>(idx);
                            sceneDirty = true;
                        }
                    }
                    ImGui::Checkbox("Fit MG sprite Size to imported image dims", &smmFitSpriteSizeOnImageImport);
                    if (ImGui::BeginListBox("MG elements", ImVec2(-1, 72))) {
                        for (size_t i = 0; i < scene->GetMGElements().size(); ++i) {
                            const auto& mg = scene->GetMGElements()[i];
                            std::string_view st{};
                            if (mg.SchemaIndex < scene->GetSchemas().size()) {
                                st = scene->GetSchemas()[mg.SchemaIndex].TypeName;
                            }
                            const std::string label = std::to_string(i) + " - " + mg.Name + " (" + std::string(st) + ")";
                            const bool sel = (mgElementSelected == static_cast<int>(i));
                            if (ImGui::Selectable(label.c_str(), sel)) {
                                mgElementSelected = static_cast<int>(i);
                            }
                        }
                        ImGui::EndListBox();
                    }
                    if (mgElementSelected >= 0 && static_cast<size_t>(mgElementSelected) < scene->GetMGElements().size()) {
                        auto& mgMut = scene->GetMGElements()[static_cast<size_t>(mgElementSelected)];
                        const auto& mgRow = mgMut;
                        std::string_view st{};
                        if (mgRow.SchemaIndex < scene->GetSchemas().size()) {
                            st = scene->GetSchemas()[mgRow.SchemaIndex].TypeName;
                        }
                        if (st == "MGSpriteElement" || st == "MGTextElement") {
                            float depth = 0.f;
                            if (const auto itD = mgRow.Attributes.find("Depth"); itD != mgRow.Attributes.end()) {
                                if (const auto* fd = std::get_if<float>(&itD->second)) {
                                    depth = *fd;
                                }
                            }
                            if (ImGui::DragFloat("MG Depth (draw order)", &depth, 0.25f, -1.0e6f, 1.0e6f)) {
                                mgMut.Attributes["Depth"] = Solstice::Parallax::AttributeValue{depth};
                                sceneDirty = true;
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                                ImGui::SetTooltip("2D compositing order only: lower values draw first (behind); higher values draw "
                                                  "last (in front). Animate with a Depth track if needed.");
                            }
                        }
                        if (st == "MGSpriteElement") {
                            float szw = 256.f;
                            float szh = 256.f;
                            const auto itS = mgRow.Attributes.find("Size");
                            if (itS != mgRow.Attributes.end()) {
                                if (const auto* v2 = std::get_if<Solstice::Math::Vec2>(&itS->second)) {
                                    szw = v2->x;
                                    szh = v2->y;
                                }
                            }
                            float szPair[2] = {szw, szh};
                            if (ImGui::DragFloat2("Sprite display Size", szPair, 1.f, 2.f, 4096.f)) {
                                std::string err;
                                if (Smm::Image::TrySetSpriteDisplaySize(*scene,
                                        static_cast<Solstice::Parallax::MGIndex>(mgElementSelected), szPair[0], szPair[1], err)) {
                                    sceneDirty = true;
                                }
                            }
                        }
                    }
                    if (elementSelected >= 0 && static_cast<size_t>(elementSelected) < scene->GetElements().size()) {
                        if (Solstice::Parallax::GetElementSchema(*scene, static_cast<Solstice::Parallax::ElementIndex>(elementSelected)) ==
                            "AudioSourceElement") {
                            float vol = 1.f;
                            float pit = 1.f;
                            const Solstice::Parallax::ElementIndex ael =
                                static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
                            const Solstice::Parallax::AttributeValue volAttr =
                                Solstice::Parallax::GetAttribute(*scene, ael, "Volume");
                            const Solstice::Parallax::AttributeValue pitAttr =
                                Solstice::Parallax::GetAttribute(*scene, ael, "Pitch");
                            if (const auto* fv = std::get_if<float>(&volAttr)) {
                                vol = *fv;
                            }
                            if (const auto* fp = std::get_if<float>(&pitAttr)) {
                                pit = *fp;
                            }
                            {
                                const Solstice::Parallax::AttributeValue au =
                                    Solstice::Parallax::GetAttribute(*scene, ael, "AudioAsset");
                                if (const uint64_t* h = std::get_if<uint64_t>(&au)) {
                                    ImGui::TextDisabled("AudioAsset: hash 0x%llX  (import replaces bytes on this source)",
                                        static_cast<unsigned long long>(*h));
                                    if (*h != 0) {
                                        Smm::Audio::DrawAudioSourceClipPreview(resolver, *h, io_mm.DeltaTime, smmStatus);
                                    }
                                } else {
                                    ImGui::TextColored(
                                        ImVec4(0.9f, 0.55f, 0.4f, 1.f), "No audio asset: use File/Assets → Import audio.");
                                }
                            }
                            ImGui::TextDisabled("Plays in-game from Parallax; MovieMaker stores the clip as session `AudioAsset`.");
                            bool audioMixChanged = false;
                            audioMixChanged |= ImGui::SliderFloat("Audio volume", &vol, 0.f, 4.f);
                            audioMixChanged |= ImGui::SliderFloat("Audio pitch", &pit, 0.25f, 4.f);
                            if (audioMixChanged) {
                                std::string err;
                                Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                if (Smm::Audio::TrySetAudioSourceMix(*scene, elementSelected, vol, pit, err)) {
                                    sceneDirty = true;
                                } else if (!err.empty()) {
                                    smmStatus = err;
                                }
                            }
                        }
                        Smm::DrawActorArzachelFields(*scene, elementSelected, compressPrlx, sceneDirty);
                    }
                    if (!smmStatus.empty()) {
                        ImGui::Separator();
                        ImGui::TextWrapped("%s", smmStatus.c_str());
                    }
                    LibUI::Widgets::EndTabItem();
                }
                if (LibUI::Widgets::BeginTabItem("Authoring")) {
                    Smm::Authoring::DrawAuthoringSessionTab("SmmAuthoring", smmAuthoringSession, *scene, resolver, assetEntries, assetSelected,
                        sceneDirty, elementSelected, mgElementSelected, smmStatus, activeProjectPath, compressPrlx, timeTicks, scene->GetTicksPerSecond());
                    LibUI::Widgets::EndTabItem();
                }
                if (LibUI::Widgets::BeginTabItem("Assets")) {
                    if (ImGui::Button("Import File")) {
                        LibUI::FileDialogs::ShowOpenFile(window, "Import", [](std::optional<std::string> path) {
                            if (path) {
                                QueuePath(std::move(*path));
                            }
                        });
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Import glTF")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import glTF asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::QueueGltfImportPath(std::move(*path));
                                }
                            },
                            Smm::kGltfFilters);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Export glTF")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export selected glTF asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::QueueGltfExportPath(std::move(*path));
                                }
                            },
                            Smm::kGltfFilters);
                    }
                    if (ImGui::Button("Import raster")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import raster texture", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Image::QueueRasterImportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Export raster")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export MG sprite Texture bytes", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Image::QueueRasterExportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterExportFilters));
                    }
                    if (ImGui::Button("Import audio")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import audio asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Audio::QueueAudioImportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioImportFilters));
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Export audio")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export AudioAsset bytes", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Audio::QueueAudioExportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioExportFilters));
                    }
                    ImGui::InputText("Import folder (path)", folderPathBuf, sizeof(folderPathBuf));
                    if (ImGui::Button("Scan folder")) {
                        std::filesystem::path p(folderPathBuf);
                        if (std::filesystem::is_directory(p)) {
                            ImportFolderRecursive(p, resolver, assetEntries);
                        }
                    }
                    int sel = assetSelected;
                    LibUI::AssetBrowser::DrawPanel("Assets", assetEntries, &sel);
                    assetSelected = sel;
                    ImGui::InputText("Import .prlx path", importPathBuf, sizeof(importPathBuf));
                    if (ImGui::Button("Import PARALLAX")) {
                        if (sceneDirty) {
                            mmPendingImportPath = std::string(importPathBuf);
                            mmUnsavedPrompt = MmUnsavedKind::ImportPrlx;
                        } else {
                            performPrlxImportFromBuffers();
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Export PARALLAX")) {
                        requestExportParallaxScene();
                    }
                    if (!smmStatus.empty()) {
                        ImGui::Separator();
                        ImGui::TextWrapped("%s", smmStatus.c_str());
                    }
                    LibUI::Widgets::EndTabItem();
                }
                LibUI::Widgets::EndTabBar();
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted("Viewer");
            ImGui::SameLine();
            ImGui::SliderFloat("MG overlay", &smmWorkspace.mgOverlayAlpha, 0.f, 1.f, "%.2f");
            ImGui::SameLine();
            const char* projLabels[] = {"Perspective", "Ortho top", "Ortho front", "Ortho side"};
            int projMode = static_cast<int>(smmWorkspace.unifiedViewportCamera.projection);
            ImGui::SetNextItemWidth(150);
            if (ImGui::Combo("##smmproj", &projMode, projLabels, IM_ARRAYSIZE(projLabels))) {
                smmWorkspace.unifiedViewportCamera.projection = static_cast<LibUI::Viewport::OrbitProjectionMode>(projMode);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset view")) {
                smmWorkspace.unifiedViewportCamera = {};
            }
            ImGui::Checkbox("Preview .smat on cubes", &smmWorkspace.previewUseSmat);
            ImGui::SameLine();
            ImGui::Checkbox("Actors only##smat", &smmWorkspace.previewSmatActorsOnly);
            ImGui::SameLine();
            ImGui::Checkbox("Selected element only##smat", &smmWorkspace.previewSmatSelectedOnly);
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
            ImGui::InputTextWithHint("##smatpath", ".smat path (UTF-8)", smmWorkspace.previewSmatPath,
                sizeof(smmWorkspace.previewSmatPath));
            ImGui::SameLine();
            if (ImGui::Button("Browse##smat")) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Open Solstice material", [&smmWorkspace](std::optional<std::string> path) {
                        if (path) {
                            std::strncpy(smmWorkspace.previewSmatPath, path->c_str(),
                                sizeof(smmWorkspace.previewSmatPath) - 1);
                            smmWorkspace.previewSmatPath[sizeof(smmWorkspace.previewSmatPath) - 1] = '\0';
                        }
                    },
                    std::span<const LibUI::FileDialogs::FileFilter>(kSmatFileFilters));
            }
            ImGui::Checkbox("Preview raster maps on cubes", &smmWorkspace.previewBindMaterialMaps);
            ImGui::Checkbox("Fluid AABB overlay", &smmWorkspace.showFluidVolumeOverlay);
            ImGui::Checkbox("Framing guides (unified view)", &smmWorkspace.showViewportFramingGuides);
            ImGui::TextDisabled("Uses the same actor / selection filter as .smat above.");
            auto drawMapRow = [&](const char* inputId, const char* hint, char* pathBuf, size_t pathCap, const char* browseId,
                                  const char* dialogTitle) {
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 88.0f);
                ImGui::InputTextWithHint(inputId, hint, pathBuf, pathCap);
                ImGui::SameLine();
                if (ImGui::Button(browseId)) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, dialogTitle,
                        [pathBuf, pathCap](std::optional<std::string> path) {
                            if (path) {
                                std::strncpy(pathBuf, path->c_str(), pathCap - 1);
                                pathBuf[pathCap - 1] = '\0';
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                }
            };
            drawMapRow("##mapAlbedo", "Albedo map (RGBA)", smmWorkspace.previewMaterialAlbedoPath,
                sizeof(smmWorkspace.previewMaterialAlbedoPath), "Browse##mapAlbedo", "Open albedo texture");
            drawMapRow("##mapNormal", "Normal map (optional)", smmWorkspace.previewMaterialNormalPath,
                sizeof(smmWorkspace.previewMaterialNormalPath), "Browse##mapNormal", "Open normal map");
            drawMapRow("##mapRough", "Roughness map (optional)", smmWorkspace.previewMaterialRoughnessPath,
                sizeof(smmWorkspace.previewMaterialRoughnessPath), "Browse##mapRough", "Open roughness texture");
            const float timelineHeight = 152.0f;
            const float transportReserve = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y * 3.0f;
            const float controlsReserve = timelineHeight + transportReserve;
            const float availableForViewport = ImGui::GetContentRegionAvail().y - controlsReserve;
            const float viewportHeight = std::max(180.0f, availableForViewport);
            if (ImGui::BeginChild("SMM_ViewportArea", ImVec2(0.0f, viewportHeight), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings)) {
                if (const char* undoMsg = Smm::GetPendingUndoSnapshotMessage()) {
                    ImGui::TextColored(ImVec4(1.f, 0.45f, 0.35f, 1.f), "%s", undoMsg);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Dismiss##undooom")) {
                        Smm::ClearPendingUndoSnapshotMessage();
                    }
                }
                if (smmWorkspace.enginePreviewLastError[0] != '\0') {
                    ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", smmWorkspace.enginePreviewLastError);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Dismiss##engprev")) {
                        smmWorkspace.enginePreviewLastError[0] = '\0';
                    }
                }
                Solstice::Math::Vec3 particleEmitter{0.f, 1.2f, 0.f};
                if (smmWorkspace.particleEditorState.attachToSceneElement && elementSelected >= 0) {
                    Solstice::Parallax::SceneEvaluationResult evPos{};
                    Solstice::Parallax::EvaluateScene(*scene, timeTicks, evPos);
                    for (const auto& et : evPos.ElementTransforms) {
                        if (static_cast<int>(et.Element) == elementSelected) {
                            particleEmitter = et.Position;
                            break;
                        }
                    }
                }
                Solstice::MovieMaker::UI::Panels::UnifiedViewportSettings uv{};
                uv.camera = &smmWorkspace.unifiedViewportCamera;
                uv.selectedElementIndex = elementSelected;
                uv.previewSmatUtf8 = smmWorkspace.previewSmatPath;
                uv.usePreviewSmat = smmWorkspace.previewUseSmat;
                uv.smatActorsOnly = smmWorkspace.previewSmatActorsOnly;
                uv.smatSelectedOnly = smmWorkspace.previewSmatSelectedOnly;
                uv.bindPreviewMaterialMaps = smmWorkspace.previewBindMaterialMaps;
                uv.previewMaterialAlbedoUtf8 = smmWorkspace.previewMaterialAlbedoPath;
                uv.previewMaterialNormalUtf8 = smmWorkspace.previewMaterialNormalPath;
                uv.previewMaterialRoughnessUtf8 = smmWorkspace.previewMaterialRoughnessPath;
                uv.onViewportPickElement = [&elementSelected](int elementIndex) { elementSelected = elementIndex; };
                uv.showFluidVolumeOverlay = smmWorkspace.showFluidVolumeOverlay;
                uv.showFramingGuides = smmWorkspace.showViewportFramingGuides;
                uv.enginePreviewErrorSink = smmWorkspace.enginePreviewLastError;
                uv.enginePreviewErrorSinkBytes = sizeof(smmWorkspace.enginePreviewLastError);
                Solstice::MovieMaker::UI::Panels::DrawUnifiedViewportPanel(window, *scene, resolver, timeTicks,
                    smmScene3dPreviewTex, viewportHeight - 8.0f, &smmWorkspace.particleEditorState, &smmParticleSpriteTex,
                    particleEmitter, smmWorkspace.mgOverlayAlpha, uv);
            }
            ImGui::EndChild();
            ImGui::Separator();
            smmSession.mainWindow = window;
            smmSession.scene = scene.get();
            smmSession.resolver = &resolver;
            smmSession.particleEditor = &smmWorkspace.particleEditorState;
            smmSession.bindings = &smmBindings;
            smmSession.compressPrlx = compressPrlx;
            smmSession.sceneDirty = &sceneDirty;
            smmSession.timeTicks = &timeTicks;
            smmSession.statusLine = &smmStatus;
            smmSession.keyframeEdit = &smmWorkspace.keyframeEditState;
            smmSession.keyframePresets = &g_smmKeyframePresets;
            smmWorkspace.timelineState.playheadTick = timeTicks;
            Smm::Editing::BridgeSyncFromScene(*scene, smmWorkspace.timelineState, smmWorkspace.curveEditorState, smmBindings);
            if (LibUI::Timeline::DrawAnimationTimeline(
                    "SMM_AnimationTimeline", smmWorkspace.timelineState, ImVec2(0.0f, timelineHeight))) {
                timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(
                    smmWorkspace.timelineState.playheadTick, scene->GetTimelineDurationTicks());
            }
            LibUI::Timeline::TimelineClampNestedRange(smmWorkspace.timelineState);
            ImGui::TextDisabled("Nested sub-timeline: zooms the track/curve to a [start,end) tick window for detailed edits.");
            ImGui::Checkbox("Nested sub-range (timeline + curve)##nstd", &smmWorkspace.timelineState.nestedViewEnabled);
            uint64_t nest0 = smmWorkspace.timelineState.nestedRangeStartTick;
            uint64_t nest1 = smmWorkspace.timelineState.nestedRangeEndTick;
            ImGui::SetNextItemWidth(200.f);
            ImGui::InputScalar("Nested start tick##nstd", ImGuiDataType_U64, &nest0);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200.f);
            ImGui::InputScalar("Nested end (exclusive)##nstd", ImGuiDataType_U64, &nest1);
            smmWorkspace.timelineState.nestedRangeStartTick = nest0;
            smmWorkspace.timelineState.nestedRangeEndTick = nest1;
            if (ImGui::SmallButton("Nest: start = playhead##nstd")) {
                smmWorkspace.timelineState.nestedRangeStartTick = timeTicks;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Nest: end = scene duration##nstd")) {
                smmWorkspace.timelineState.nestedRangeEndTick = scene->GetTimelineDurationTicks();
            }
            smmPanelReg.DrawPanels();
            if (ImGui::Button("-1 tick")) {
                if (timeTicks > 0) {
                    --timeTicks;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("+1 tick")) {
                if (timeTicks < maxT) {
                    ++timeTicks;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("-1 s")) {
                const uint64_t step = scene->GetTicksPerSecond();
                timeTicks = (timeTicks > step) ? (timeTicks - step) : 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("+1 s")) {
                const uint64_t step = scene->GetTicksPerSecond();
                timeTicks = std::min(maxT, timeTicks + step);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("|< start")) {
                Solstice::MovieMaker::Workflow::JumpPlayheadToStart(timeTicks, scene->GetTimelineDurationTicks());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("end >|")) {
                Solstice::MovieMaker::Workflow::JumpPlayheadToEnd(timeTicks, scene->GetTimelineDurationTicks());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Snap s")) {
                timeTicks = Solstice::MovieMaker::Workflow::SnapTickToWholeSeconds(timeTicks, scene->GetTicksPerSecond());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("KF -1s")) {
                Solstice::MovieMaker::Workflow::ShiftSceneKeyframes(*scene,
                    -static_cast<int64_t>(scene->GetTicksPerSecond()));
                sceneDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("KF +1s")) {
                Solstice::MovieMaker::Workflow::ShiftSceneKeyframes(*scene,
                    static_cast<int64_t>(scene->GetTicksPerSecond()));
                sceneDirty = true;
            }
            ImGui::EndTable();
        }

        if (showExportWindow) {
            if (ImGui::Begin("SMM Export", &showExportWindow)) {
                ImGui::TextDisabled("Background recovery: periodic .prlx snapshots to the recovery store while the scene is dirty.");
                {
                    int ri = static_cast<int>(smmRecoveryIntervalSecU32);
                    if (ImGui::SliderInt("Recovery interval (sec)", &ri, 10, 600)) {
                        smmRecoveryIntervalSecU32 = static_cast<uint32_t>(ri);
                    }
                }
                ImGui::TextDisabled("Use File / Write recovery snapshot now for a manual on-demand write.");
                ImGui::Separator();
                ImGui::TextDisabled("Parallax scene file: Export writes .prlx.");
                ImGui::InputText("Export .prlx path", exportPathBuf, sizeof(exportPathBuf));
                ImGui::SameLine();
                ImGui::Checkbox("ZSTD compress", &compressPrlx);
                ImGui::SameLine();
                if (ImGui::Button("Export .prlx")) {
                    const std::filesystem::path outPath = ResolveSceneSavePath(exportPathBuf, activeProjectPath);
                    std::error_code ec;
                    if (!outPath.parent_path().empty()) {
                        std::filesystem::create_directories(outPath.parent_path(), ec);
                    }
                    Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
                    if (ec) {
                        smmStatus = "PARALLAX export failed: " + ec.message();
                    } else {
                        std::string particleSyncErr;
                        (void)Smm::Particles::SyncEditorToParallaxScene(
                            *scene, smmWorkspace.particleEditorState, resolver, particleSyncErr);
                        if (!Solstice::Parallax::SaveScene(*scene, outPath, compressPrlx, &err)) {
                            smmStatus =
                                "PARALLAX export failed with ParallaxError " + std::to_string(static_cast<int>(err));
                        } else {
                            std::snprintf(exportPathBuf, sizeof(exportPathBuf), "%s", outPath.string().c_str());
                            sceneDirty = false;
                            PushRecentPath(recentPrlxPaths, outPath.string());
                            smmStatus = "Exported PARALLAX scene: " + outPath.string();
                        }
                    }
                }
                if (!smmStatus.empty()) {
                    ImGui::TextWrapped("%s", smmStatus.c_str());
                }

                ImGui::Separator();
                ImGui::TextUnformatted("Export video (Parallax MG)");
                ImGui::InputText("Video output file", videoExportPathBuf, sizeof(videoExportPathBuf));
        int vwI = static_cast<int>(videoW);
        int vhI = static_cast<int>(videoH);
        int vfpsI = static_cast<int>(videoFps);
        if (ImGui::InputInt("Width", &vwI)) {
            videoW = static_cast<uint32_t>(std::max(16, std::min(8192, vwI)));
        }
        if (ImGui::InputInt("Height", &vhI)) {
            videoH = static_cast<uint32_t>(std::max(16, std::min(8192, vhI)));
        }
        if (ImGui::InputInt("FPS", &vfpsI)) {
            videoFps = static_cast<uint32_t>(std::max(1, std::min(240, vfpsI)));
        }
        int vcCombo = videoMp4 ? 0 : 1;
        const char* vcItems[] = {"MP4", "MOV"};
        if (ImGui::Combo("Container", &vcCombo, vcItems, 2)) {
            videoMp4 = (vcCombo == 0);
        }
        ImGui::InputScalar("Start tick", ImGuiDataType_U64, &videoStartTick);
        ImGui::InputScalar("End tick (0 = timeline end)", ImGuiDataType_U64, &videoEndTick);
        if (ImGui::Button("Export video now")) {
            Solstice::MovieMaker::VideoExportParams vep;
            vep.outputPath = videoExportPathBuf;
            vep.ffmpegExecutable = ffmpegExeBuf;
            vep.width = videoW;
            vep.height = videoH;
            vep.fps = videoFps;
            vep.startTick = videoStartTick;
            vep.endTick = videoEndTick;
            vep.container = videoMp4 ? Solstice::MovieMaker::VideoContainer::Mp4 : Solstice::MovieMaker::VideoContainer::Mov;
            std::string err;
            videoExportLog.clear();
            std::string lastCmdDiag = "ffmpeg: \"" + std::string(ffmpegExeBuf) + "\" (rawvideo pipe → " + std::string(videoExportPathBuf) + ")";
            const bool ok = Solstice::MovieMaker::ExportParallaxSceneToVideo(*scene, resolver, window, vep, err,
                [&](float pr) { videoExportLog = "Encoding… " + std::to_string(static_cast<int>(pr * 100.f)) + "%\n" + lastCmdDiag + "\n"; });
            if (!ok) {
                videoExportLog = lastCmdDiag + "\n\n" + err;
            } else {
                videoExportLog = "Export finished: " + std::string(videoExportPathBuf);
                if (!err.empty()) {
                    videoExportLog += "\n\nNote:\n" + err;
                }
                videoExportLog += "\n";
                MovieMakerProjectState pst;
                pst.exportPath = exportPathBuf;
                pst.importPath = importPathBuf;
                pst.folderPath = folderPathBuf;
                pst.ffmpegExe = ffmpegExeBuf;
                pst.videoExportPath = videoExportPathBuf;
                pst.videoWidth = videoW;
                pst.videoHeight = videoH;
                pst.videoFps = videoFps;
                pst.videoMp4 = videoMp4;
                pst.videoStartTick = videoStartTick;
                pst.videoEndTick = videoEndTick;
                pst.compressPrlx = compressPrlx;
                pst.recoveryIntervalSec = smmRecoveryIntervalSecU32;
                pst.recentPrlx = recentPrlxPaths;
                SaveMovieMakerProjectToPath(activeProjectPath, pst);
            }
        }
        if (ImGui::Button("Add current settings to render queue##rq")) {
            Solstice::MovieMaker::VideoExportParams qv;
            qv.outputPath = videoExportPathBuf;
            qv.ffmpegExecutable = ffmpegExeBuf;
            qv.width = videoW;
            qv.height = videoH;
            qv.fps = videoFps;
            qv.startTick = videoStartTick;
            qv.endTick = videoEndTick;
            qv.container = videoMp4 ? Solstice::MovieMaker::VideoContainer::Mp4 : Solstice::MovieMaker::VideoContainer::Mov;
            smmVideoRenderQueue.push_back(std::move(qv));
        }
        if (!smmVideoRenderQueue.empty()) {
            ImGui::Text("Queued: %zu", smmVideoRenderQueue.size());
            for (size_t qi = 0; qi < smmVideoRenderQueue.size(); ++qi) {
                ImGui::PushID(static_cast<int>(qi));
                ImGui::BulletText("%s  (%ux%u @%u) ticks %llu-%llu", smmVideoRenderQueue[qi].outputPath.c_str(),
                    smmVideoRenderQueue[qi].width, smmVideoRenderQueue[qi].height, smmVideoRenderQueue[qi].fps,
                    static_cast<unsigned long long>(smmVideoRenderQueue[qi].startTick),
                    static_cast<unsigned long long>(smmVideoRenderQueue[qi].endTick));
                if (ImGui::SmallButton("Remove")) {
                    smmVideoRenderQueue.erase(smmVideoRenderQueue.begin() + qi);
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Run render queue (sequential)##rq")) {
                for (const Solstice::MovieMaker::VideoExportParams& job : smmVideoRenderQueue) {
                    std::string err2;
                    const bool runOk = Solstice::MovieMaker::ExportParallaxSceneToVideo(*scene, resolver, window, job, err2,
                        [&](float pr) {
                            videoExportLog = "Queue: " + job.outputPath + " — " + std::to_string(static_cast<int>(pr * 100.f)) + "%\n";
                        });
                    if (!runOk) {
                        videoExportLog = "Render queue failed: " + job.outputPath + "\n" + err2;
                        break;
                    }
                }
                smmVideoRenderQueue.clear();
                if (videoExportLog.find("failed") == std::string::npos) {
                    videoExportLog = "Render queue completed.\n" + videoExportLog;
                }
            }
        }
        LibUI::Widgets::InputTextMultiline("##videoexportlog", videoExportLog, ImVec2(-1, 120), ImGuiInputTextFlags_ReadOnly);

        ImGui::Separator();
        ImGui::TextUnformatted("FFmpeg (optional CLI)");
#ifdef SOLSTICE_HAVE_FFMPEG_CLI
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1), "ffmpeg found at CMake configure (PATH).");
#else
        ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.4f, 1),
            "ffmpeg was not on PATH at CMake configure; set ffmpeg executable below or extend PATH and reconfigure.");
#endif
        ImGui::InputText("ffmpeg executable", ffmpegExeBuf, sizeof(ffmpegExeBuf));
        if (ImGui::Button("Probe (ffmpeg -version)")) {
            ffmpegLog.clear();
            SolsticeFfmpegRunResult r = SolsticeRunProcessCapture(std::string(ffmpegExeBuf), "-version");
            ffmpegLog = "exit " + std::to_string(r.ExitCode) + "\n" + r.Output;
        }
        ImGui::SameLine();
        if (ImGui::Button("Test encode (1s testsrc → temp MP4)")) {
            ffmpegLog.clear();
            std::filesystem::path tmp = std::filesystem::temp_directory_path() / "smm_ffmpeg_test.mp4";
            std::string outPath = tmp.generic_string();
            std::string args =
                "-y -hide_banner -loglevel warning -f lavfi -i testsrc=duration=1:size=160x120:rate=1 -pix_fmt yuv420p "
                "-c:v libx264 \"";
            args += outPath;
            args += "\"";
            lastFfmpegShellCommand = "\"" + std::string(ffmpegExeBuf) + "\" " + args;
            SolsticeFfmpegRunResult r = SolsticeRunProcessCapture(std::string(ffmpegExeBuf), args);
            ffmpegLog = "exit " + std::to_string(r.ExitCode) + "\n" + r.Output;
            if (r.ExitCode == 0) {
                ffmpegLog += "Wrote: " + outPath + "\n";
            }
        }
        ImGui::SameLine();
#ifdef _WIN32
        if (ImGui::Button("Copy last ffmpeg command") && !lastFfmpegShellCommand.empty()) {
            CopyAsciiToSystemClipboard(lastFfmpegShellCommand);
            ffmpegLog = "(Copied command to clipboard)\n" + ffmpegLog;
        }
#else
        ImGui::BeginDisabled(lastFfmpegShellCommand.empty());
        if (ImGui::Button("Copy last ffmpeg command (see log)") && !lastFfmpegShellCommand.empty()) {
            ffmpegLog = lastFfmpegShellCommand + "\n" + ffmpegLog;
        }
        ImGui::EndDisabled();
#endif
        if (!lastFfmpegShellCommand.empty()) {
            ImGui::TextWrapped("Shell: %s", lastFfmpegShellCommand.c_str());
        }
                LibUI::Widgets::InputTextMultiline("##ffmpeglog", ffmpegLog, ImVec2(-1, 96), ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::End();
        }

        ImGui::End();

        MovieMakerPluginsDrawPanel(&showMmPluginsPanel);
        if (showMmAboutPanel) {
            ImGui::SetNextWindowSize(ImVec2(440, 200), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("About Solstice Movie Maker", &showMmAboutPanel)) {
                ImGui::TextUnformatted("Technology Preview 1");
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Authoring tool for Parallax (.prlx): timeline, motion graphics preview, optional ffmpeg export. "
                    "Project settings live in .smm.json; the Parallax scene is imported/exported separately.");
                ImGui::Separator();
                ImGui::TextUnformatted("Unified viewport: Shift+click to pick element; F focuses camera on selected. "
                                       "View → Fluid volumes for Parallax-persisted NS-style fluid boxes (overlay toggle in preview).");
            }
            ImGui::End();
        }

        glClearColor(0.1f, 0.1f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        LibUI::Core::Render();
        SDL_GL_SwapWindow(window);
    }

    smmScene3dPreviewTex.Destroy();
    smmParticleSpriteTex.Destroy();
    Solstice::EditorEnginePreview::Shutdown();
    Solstice::EditorAudio::Shutdown();
    LibUI::Core::Shutdown();
    LibUI::Shell::DestroyUtilityGlWindow(glWindow);
    SDL_Quit();
    return 0;
}
