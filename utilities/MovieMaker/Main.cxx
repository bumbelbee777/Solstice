// Solstice MovieMaker (SMM) — MVP: import assets, edit minimal PARALLAX scene, import/export .prlx.
// Video export: MG → OpenGL → ffmpeg (MP4/MOV).

#include "FfmpegUtil.hxx"
#include "PreviewTexture.hxx"
#include "VideoExport.hxx"
#include "Workflow/Workflow.hxx"
#include "LibUI/AssetBrowser/AssetBrowser.hxx"
#include "LibUI/Core/Core.hxx"
#include "LibUI/Widgets/Widgets.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"

#include <Parallax/MGRaster.hxx>
#include <Parallax/Parallax.hxx>
#include <Parallax/ParallaxScene.hxx>

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
};

static std::filesystem::path MovieMakerDefaultProjectPath() {
    const char* b = SDL_GetBasePath();
    if (b) {
        std::filesystem::path p(b);
        SDL_free((void*)b);
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

static void SaveMovieMakerProjectToPath(const std::filesystem::path& path, const MovieMakerProjectState& st) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }
    out << "{\"version\":2,\"exportPath\":\"" << EscapeJsonFragment(st.exportPath) << "\",\"importPath\":\""
        << EscapeJsonFragment(st.importPath) << "\",\"folderPath\":\"" << EscapeJsonFragment(st.folderPath)
        << "\",\"ffmpegExe\":\"" << EscapeJsonFragment(st.ffmpegExe) << "\",\"videoExportPath\":\""
        << EscapeJsonFragment(st.videoExportPath) << "\",\"videoWidth\":" << st.videoWidth << ",\"videoHeight\":"
        << st.videoHeight << ",\"videoFps\":" << st.videoFps << ",\"videoMp4\":" << (st.videoMp4 ? "true" : "false")
        << ",\"compressPrlx\":" << (st.compressPrlx ? "true" : "false")
        << ",\"videoStartTick\":" << st.videoStartTick << ",\"videoEndTick\":" << st.videoEndTick << ",\"recent\":[";
    for (size_t i = 0; i < st.recentPrlx.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << "\"" << EscapeJsonFragment(st.recentPrlx[i]) << "\"";
    }
    out << "]}\n";
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
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath) {
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
}

static void DrainPendingMovieMakerProject(char* exportPathBuf, size_t exportPathBufSize, char* importPathBuf,
    size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf, size_t ffmpegExeBufSize,
    char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH, uint32_t& videoFps,
    bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath) {
    std::optional<MovieMakerProjectState> pending;
    {
        std::lock_guard<std::mutex> lock(g_ProjectMutex);
        pending = std::move(g_PendingProjectApply);
    }
    if (pending) {
        ApplyMovieMakerProjectState(*pending, exportPathBuf, exportPathBufSize, importPathBuf, importPathBufSize,
            folderPathBuf, folderPathBufSize, ffmpegExeBuf, ffmpegExeBufSize, videoExportPathBuf, videoExportPathBufSize,
            videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx, recentPrlxPaths,
            activeProjectPath);
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

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window =
        SDL_CreateWindow("Solstice MovieMaker (SMM)", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (!SDL_GL_MakeCurrent(window, glContext)) {
        std::cerr << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI::Core::Initialize failed" << std::endl;
        return 1;
    }

    Solstice::Parallax::DevSessionAssetResolver resolver;
    std::vector<LibUI::AssetBrowser::Entry> assetEntries;
    auto scene = Solstice::Parallax::CreateScene(6000);
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
    Solstice::MovieMaker::PreviewTextureRgba mgPreviewTex{};
    Solstice::MovieMaker::PreviewTextureRgba smmScene3dPreviewTex{};
    std::vector<std::string> recentPrlxPaths;
    int elementSelected = -1;
    std::filesystem::path activeProjectPath = MovieMakerDefaultProjectPath();
    std::string lastFfmpegShellCommand;

    {
        MovieMakerProjectState boot;
        if (LoadMovieMakerProjectFromPath(activeProjectPath, boot)) {
            ApplyMovieMakerProjectState(boot, exportPathBuf, sizeof(exportPathBuf), importPathBuf, sizeof(importPathBuf),
                folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf), videoExportPathBuf,
                sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick,
                compressPrlx, recentPrlxPaths, activeProjectPath);
        }
    }

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            LibUI::Core::ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        DrainPendingMovieMakerProject(exportPathBuf, sizeof(exportPathBuf), importPathBuf, sizeof(importPathBuf),
            folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf), videoExportPathBuf,
            sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx,
            recentPrlxPaths, activeProjectPath);

        LibUI::Core::NewFrame();

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::Begin("SMMRoot", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::TextUnformatted("Solstice Movie Maker (SMM) — Parallax authoring");
        ImGui::Separator();

        if (ImGui::Button("Save project")) {
            MovieMakerProjectState st;
            st.exportPath = exportPathBuf;
            st.importPath = importPathBuf;
            st.folderPath = folderPathBuf;
            st.ffmpegExe = ffmpegExeBuf;
            st.videoExportPath = videoExportPathBuf;
            st.videoWidth = videoW;
            st.videoHeight = videoH;
            st.videoFps = videoFps;
            st.videoMp4 = videoMp4;
            st.videoStartTick = videoStartTick;
            st.videoEndTick = videoEndTick;
            st.compressPrlx = compressPrlx;
            st.recentPrlx = recentPrlxPaths;
            SaveMovieMakerProjectToPath(activeProjectPath, st);
            ffmpegLog = "Project saved to " + activeProjectPath.string() + "\n" + ffmpegLog;
        }
        ImGui::SameLine();
        if (ImGui::Button("Open project…")) {
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
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", activeProjectPath.filename().string().c_str());
        ImGui::Separator();

        if (LibUI::Icons::ToolbarButton(LibUI::Icons::Id::Import, "Import file")) {
            LibUI::FileDialogs::ShowOpenFile(window, "Import", [](std::optional<std::string> path) {
                if (path) {
                    QueuePath(std::move(*path));
                }
            });
        }
        ImGui::SameLine();
        ImGui::InputText("Import folder (path)", folderPathBuf, sizeof(folderPathBuf));
        ImGui::SameLine();
        if (ImGui::Button("Scan folder")) {
            std::filesystem::path p(folderPathBuf);
            if (std::filesystem::is_directory(p)) {
                ImportFolderRecursive(p, resolver, assetEntries);
            }
        }

        DrainImports(resolver, assetEntries);

        ImGui::Separator();
        int sel = assetSelected;
        LibUI::AssetBrowser::DrawPanel("Assets", assetEntries, &sel);
        assetSelected = sel;

        ImGui::Separator();
        ImGui::Text("Scene: %zu elements", scene->GetElements().size());
        if (ImGui::BeginListBox("Elements", ImVec2(-1, 100))) {
            for (size_t i = 0; i < scene->GetElements().size(); ++i) {
                const auto& el = scene->GetElements()[i];
                std::string label = std::to_string(i) + " — " + el.Name;
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
        }
        ImGui::Separator();
        if (!recentPrlxPaths.empty()) {
            ImGui::TextUnformatted("Recent .prlx");
            for (size_t ri = 0; ri < recentPrlxPaths.size(); ++ri) {
                ImGui::PushID(static_cast<int>(ri));
                if (ImGui::SmallButton(recentPrlxPaths[ri].c_str())) {
                    std::snprintf(importPathBuf, sizeof(importPathBuf), "%s", recentPrlxPaths[ri].c_str());
                }
                ImGui::PopID();
            }
            ImGui::Separator();
        }
        uint64_t dur = scene->GetTimelineDurationTicks();
        ImGui::InputScalar("Duration (ticks)", ImGuiDataType_U64, &dur);
        scene->SetTimelineDurationTicks(dur);
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
        uint32_t tps = scene->GetTicksPerSecond();
        if (ImGui::InputScalar("Ticks/sec", ImGuiDataType_U32, &tps)) {
            scene->SetTicksPerSecond(tps);
        }

        if (ImGui::Button("Add Light")) {
            Solstice::Parallax::AddElement(*scene, "LightElement", "Light", 0);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Camera")) {
            Solstice::Parallax::AddElement(*scene, "CameraElement", "Camera", 0);
        }

        uint64_t maxT = scene->GetTimelineDurationTicks() > 0 ? scene->GetTimelineDurationTicks() : 1;
        float tf = static_cast<float>(timeTicks) / static_cast<float>(maxT);
        if (ImGui::SliderFloat("Timeline", &tf, 0.f, 1.f)) {
            timeTicks = static_cast<uint64_t>(tf * static_cast<float>(maxT));
        }
        const double tpsD = static_cast<double>(std::max(1u, scene->GetTicksPerSecond()));
        ImGui::Text("Playhead: tick %llu / %llu (%.4f s)", static_cast<unsigned long long>(timeTicks),
            static_cast<unsigned long long>(maxT), static_cast<double>(timeTicks) / tpsD);
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
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("KF +1s")) {
            Solstice::MovieMaker::Workflow::ShiftSceneKeyframes(*scene,
                static_cast<int64_t>(scene->GetTicksPerSecond()));
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Scene schematic 3D (EvaluateScene + engine mesh / light preview)");
        static LibUI::Viewport::OrbitPanZoomState s_smmScene3dNav{};
        LibUI::Viewport::Frame sceneVp{};
        if (LibUI::Viewport::BeginHost("smm_scene_3d_viewport", ImVec2(-1, 200), true)) {
            if (LibUI::Viewport::PollFrame(sceneVp) && sceneVp.draw_list) {
                Solstice::Parallax::SceneEvaluationResult eval{};
                Solstice::Parallax::EvaluateScene(*scene, timeTicks, eval);
                const float aspect = std::max(sceneVp.size.y, 1.0f) > 0.f
                    ? sceneVp.size.x / std::max(sceneVp.size.y, 1.0f)
                    : 1.f;
                const int scW = std::max(2, static_cast<int>(sceneVp.size.x));
                const int scH = std::max(2, static_cast<int>(sceneVp.size.y));
                std::vector<Solstice::EditorEnginePreview::PreviewEntity> scEnts;
                scEnts.reserve(eval.ElementTransforms.size());
                for (const auto& et : eval.ElementTransforms) {
                    Solstice::EditorEnginePreview::PreviewEntity pe{};
                    pe.Position = et.Position;
                    const std::string_view st = Solstice::Parallax::GetElementSchema(*scene, et.Element);
                    if (st == "CameraElement") {
                        pe.Albedo = Solstice::Math::Vec3(1.f, 0.7f, 0.2f);
                    } else if (st == "ActorElement") {
                        pe.Albedo = Solstice::Math::Vec3(0.35f, 0.62f, 1.f);
                    } else {
                        pe.Albedo = Solstice::Math::Vec3(0.62f, 0.66f, 0.78f);
                    }
                    pe.HalfExtent = 0.28f;
                    scEnts.push_back(pe);
                }
                std::vector<Solstice::Physics::LightSource> mmLights;
                {
                    Solstice::Physics::LightSource sun{};
                    sun.Type = Solstice::Physics::LightSource::LightType::Directional;
                    sun.Position = Solstice::Math::Vec3(0.4f, 0.82f, 0.38f).Normalized();
                    sun.Color = Solstice::Math::Vec3(1.f, 0.96f, 0.88f);
                    sun.Intensity = 1.15f;
                    mmLights.push_back(sun);
                }
                for (const auto& ls : eval.LightStates) {
                    Solstice::Physics::LightSource pl{};
                    pl.Type = Solstice::Physics::LightSource::LightType::Point;
                    pl.Position = ls.Position;
                    pl.Color = Solstice::Math::Vec3(ls.Color.x, ls.Color.y, ls.Color.z);
                    pl.Intensity = std::max(0.35f, ls.Intensity);
                    pl.Range = 48.f;
                    mmLights.push_back(pl);
                }
                std::vector<std::byte> scCap;
                int scCw = 0;
                int scCh = 0;
                if (Solstice::EditorEnginePreview::CaptureOrbitRgb(s_smmScene3dNav, 0.f, 0.f, 0.f, 55.f, aspect, scW,
                        scH, scEnts.data(), scEnts.size(), mmLights.data(), mmLights.size(), scCap, scCw, scCh)) {
                    smmScene3dPreviewTex.SetSizeUpload(window, static_cast<uint32_t>(scCw), static_cast<uint32_t>(scCh),
                        scCap.data(), scCap.size());
                }
                if (smmScene3dPreviewTex.Valid()) {
                    LibUI::Viewport::DrawTextureLetterboxed(sceneVp.draw_list, smmScene3dPreviewTex.ImGuiTexId(),
                        sceneVp.min, sceneVp.max, static_cast<float>(smmScene3dPreviewTex.width),
                        static_cast<float>(smmScene3dPreviewTex.height));
                } else {
                    LibUI::Viewport::DrawCheckerboard(sceneVp.draw_list, sceneVp.min, sceneVp.max, 14.f,
                        IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
                }
                LibUI::Viewport::Mat4Col viewM{};
                LibUI::Viewport::Mat4Col projM{};
                LibUI::Viewport::ComputeOrbitViewProjectionColMajor(s_smmScene3dNav, 0.f, 0.f, 0.f, 55.f, aspect, 0.12f,
                    2048.f, viewM, projM);
                LibUI::Viewport::DrawXZGrid(sceneVp.draw_list, sceneVp.min, sceneVp.max, viewM, projM, 1.f,
                    IM_COL32(72, 72, 92, 200), 24);
                for (const auto& et : eval.ElementTransforms) {
                    std::string_view st = Solstice::Parallax::GetElementSchema(*scene, et.Element);
                    ImU32 col = IM_COL32(140, 200, 255, 255);
                    if (st == "CameraElement") {
                        col = IM_COL32(255, 210, 90, 255);
                    } else if (st == "ActorElement") {
                        col = IM_COL32(120, 220, 255, 255);
                    }
                    LibUI::Viewport::DrawWorldCrossXZ(sceneVp.draw_list, sceneVp.min, sceneVp.max, viewM, projM,
                        et.Position.x, et.Position.y, et.Position.z, 0.3f, col);
                }
                for (const auto& ls : eval.LightStates) {
                    ImVec2 sp{};
                    if (LibUI::Viewport::WorldToScreen(viewM, projM, ls.Position.x, ls.Position.y, ls.Position.z,
                            sceneVp.min, sceneVp.max, sp)) {
                        const ImU32 lcol = IM_COL32(
                            static_cast<int>(ls.Color.x * 255.f), static_cast<int>(ls.Color.y * 255.f),
                            static_cast<int>(ls.Color.z * 255.f), 255);
                        sceneVp.draw_list->AddCircleFilled(sp, 6.f, lcol);
                        sceneVp.draw_list->AddCircle(sp, 7.f, IM_COL32(255, 255, 255, 200));
                    }
                }
                LibUI::Viewport::ApplyOrbitPanZoom(s_smmScene3dNav, sceneVp);
                LibUI::Viewport::DrawViewportLabel(sceneVp.draw_list, sceneVp.min, sceneVp.max,
                    "Schematic 3D — bgfx + EvaluateScene lights", ImVec2(1.0f, 0.0f));
            }
            LibUI::Viewport::EndHost();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("MG compositing preview (2D CPU raster → GL; not a 3D orbit view)");
        LibUI::Viewport::Frame mgVp{};
        if (LibUI::Viewport::BeginHost("smm_mg_viewport", ImVec2(-1, 160), true)) {
            if (LibUI::Viewport::PollFrame(mgVp) && mgVp.draw_list) {
                const int iw = std::max(2, static_cast<int>(mgVp.size.x));
                const int ih = std::max(2, static_cast<int>(mgVp.size.y));
                std::vector<std::byte> rgba(static_cast<size_t>(iw) * static_cast<size_t>(ih) * 4);
                Solstice::Parallax::MGDisplayList mgList = Solstice::Parallax::EvaluateMG(*scene, timeTicks);
                Solstice::Parallax::RasterizeMGDisplayList(mgList, &resolver, static_cast<uint32_t>(iw),
                    static_cast<uint32_t>(ih), std::span<std::byte>(rgba.data(), rgba.size()));
                mgPreviewTex.SetSizeUpload(window, static_cast<uint32_t>(iw), static_cast<uint32_t>(ih), rgba.data(),
                    rgba.size());
                if (mgPreviewTex.Valid()) {
                    LibUI::Viewport::DrawTextureLetterboxed(mgVp.draw_list, mgPreviewTex.ImGuiTexId(), mgVp.min,
                        mgVp.max, static_cast<float>(iw), static_cast<float>(ih));
                } else {
                    LibUI::Viewport::DrawCheckerboard(mgVp.draw_list, mgVp.min, mgVp.max, 12.0f,
                        IM_COL32(38, 38, 48, 255), IM_COL32(26, 26, 34, 255));
                }
                char lbuf[192]{};
                std::snprintf(lbuf, sizeof(lbuf), "2D tick %llu  |  %dx%d",
                    static_cast<unsigned long long>(timeTicks), iw, ih);
                LibUI::Viewport::DrawViewportLabel(mgVp.draw_list, mgVp.min, mgVp.max, lbuf, ImVec2(1.0f, 0.0f));
            }
            LibUI::Viewport::EndHost();
        }

        ImGui::InputText("Export .prlx path", exportPathBuf, sizeof(exportPathBuf));
        ImGui::SameLine();
        ImGui::Checkbox("ZSTD compress", &compressPrlx);
        ImGui::SameLine();
        if (LibUI::Icons::ToolbarButton(LibUI::Icons::Id::Export, "Export")) {
            Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
            if (!Solstice::Parallax::SaveScene(*scene, exportPathBuf, compressPrlx, &err)) {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Export failed");
            } else {
                PushRecentPath(recentPrlxPaths, std::string(exportPathBuf));
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
                pst.recentPrlx = recentPrlxPaths;
                SaveMovieMakerProjectToPath(activeProjectPath, pst);
            }
        }

        ImGui::InputText("Import .prlx path", importPathBuf, sizeof(importPathBuf));
        ImGui::SameLine();
        if (LibUI::Icons::ToolbarButton(LibUI::Icons::Id::Open, "Import PARALLAX")) {
            Solstice::Parallax::ParallaxError err = Solstice::Parallax::ParallaxError::None;
            auto loaded = Solstice::Parallax::LoadScene(importPathBuf, &resolver, &err);
            if (loaded) {
                scene = std::move(loaded);
                PushRecentPath(recentPrlxPaths, std::string(importPathBuf));
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
                pst.recentPrlx = recentPrlxPaths;
                SaveMovieMakerProjectToPath(activeProjectPath, pst);
            }
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
                pst.recentPrlx = recentPrlxPaths;
                SaveMovieMakerProjectToPath(activeProjectPath, pst);
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

        ImGui::End();

        glClearColor(0.1f, 0.1f, 0.12f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        LibUI::Core::Render();
        SDL_GL_SwapWindow(window);
    }

    mgPreviewTex.Destroy();
    smmScene3dPreviewTex.Destroy();
    Solstice::EditorEnginePreview::Shutdown();
    LibUI::Core::Shutdown();
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
