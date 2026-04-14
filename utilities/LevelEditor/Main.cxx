// Jackhammer — Solstice Level Editor: .smf (Solstice Map Format v1) authoring.
// BSP / octree editing and engine load are future work.

#include "LibUI/Core/Core.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Viewport/Viewport.hxx"
#include "LibUI/Viewport/ViewportMath.hxx"
#include "EditorEnginePreview/EditorEnginePreview.hxx"
#include "MovieMaker/PreviewTexture.hxx"

#include <Math/Vector.hxx>
#include <Physics/Lighting/LightSource.hxx>

#include <imgui.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <Smf/SmfBinary.hxx>
#include <Smf/SmfUtil.hxx>
#include <SolsticeAPI/V1/Common.h>
#include <SolsticeAPI/V1/Smf.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

using Solstice::Smf::SmfAttributeType;
using Solstice::Smf::SmfDefaultValueForType;
using Solstice::Smf::SmfAttributeTypeLabel;
using Solstice::Smf::SmfEntity;
using Solstice::Smf::SmfErrorMessage;
using Solstice::Smf::SmfFileHeader;
using Solstice::Smf::SmfMakeEntity;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfProperty;
using Solstice::Smf::SmfQuaternion;
using Solstice::Smf::SmfVec2;
using Solstice::Smf::SmfVec3;
using Solstice::Smf::SmfVec4;
using Solstice::Smf::SmfMatrix4;
using Solstice::Smf::SmfValue;

std::mutex g_FileOpMutex;
std::optional<std::string> g_PendingOpenPath;
std::optional<std::string> g_PendingSavePath;

void QueueOpenPath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingOpenPath = std::move(p);
}

void QueueSavePath(std::string p) {
    std::lock_guard<std::mutex> lock(g_FileOpMutex);
    g_PendingSavePath = std::move(p);
}

static const LibUI::FileDialogs::FileFilter kSmfFilters[] = {
    {"Solstice Map", "smf"},
    {"All", "*"},
};

#ifdef _WIN32
static std::string EngineDllSmfValidateSummary(const std::vector<std::byte>& bytes) {
    HMODULE mod = LoadLibraryA("SolsticeEngine.dll");
    if (!mod) {
        return "Engine DLL not found next to LevelEditor (optional check skipped).";
    }
    using Fn = SolsticeV1_ResultCode (*)(const void*, size_t, char*, size_t);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(mod, "SolsticeV1_SmfValidateBinary"));
    if (!fn) {
        FreeLibrary(mod);
        return "SolsticeV1_SmfValidateBinary not exported.";
    }
    char err[512];
    err[0] = '\0';
    SolsticeV1_ResultCode r = fn(bytes.data(), bytes.size(), err, sizeof(err));
    FreeLibrary(mod);
    if (r == SolsticeV1_ResultSuccess) {
        return "Engine SolsticeV1_SmfValidateBinary: OK.";
    }
    return std::string("Engine validate: ") + (err[0] ? err : "failed");
}
#else
static std::string EngineDllSmfValidateSummary(const std::vector<std::byte>&) {
    return "Engine DLL cross-check is Windows-only in this build.";
}
#endif

static void ApplyNewMapTemplate(SmfMap& map, std::optional<std::string>& currentPath, SmfFileHeader& lastHeader, int& selectedEntity,
    bool& dirty, std::string& status) {
    map.Clear();
    SmfEntity ws = SmfMakeEntity("worldspawn", "WorldSettings");
    ws.Properties.push_back({"gravity", SmfAttributeType::Float, SmfValue{9.81f}});
    map.Entities.push_back(std::move(ws));
    SmfEntity lamp = SmfMakeEntity("light_0", "Light");
    lamp.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{0.f, 2.f, 0.f}}});
    lamp.Properties.push_back({"style", SmfAttributeType::String, SmfValue{std::string("point")}});
    map.Entities.push_back(std::move(lamp));
    currentPath.reset();
    lastHeader = {};
    selectedEntity = 0;
    dirty = true;
    status = "New map from template.";
}

void UpdateWindowTitle(SDL_Window* window, const std::optional<std::string>& currentPath, bool dirty) {
    std::string title = "Jackhammer — Solstice Level Editor";
    if (currentPath) {
        title += " — ";
        title += *currentPath;
    }
    if (dirty) {
        title += " *";
    }
    SDL_SetWindowTitle(window, title.c_str());
}

void DrainPendingFileOps(SmfMap& map, std::optional<std::string>& currentPath, SmfFileHeader& hdr, std::string& status,
    bool& dirty, int& selectedEntity, bool compressSmf) {
    std::optional<std::string> openPath;
    std::optional<std::string> savePath;
    {
        std::lock_guard<std::mutex> lock(g_FileOpMutex);
        openPath = std::move(g_PendingOpenPath);
        savePath = std::move(g_PendingSavePath);
    }

    if (openPath) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::LoadSmfFromFile(std::filesystem::path(*openPath), map, &hdr, &err)) {
            status = std::string("Open failed: ") + SmfErrorMessage(err);
        } else {
            currentPath = *openPath;
            selectedEntity = map.Entities.empty() ? -1 : 0;
            dirty = false;
            status = "Loaded " + *openPath;
            LibUI::Core::RecentPathPush(openPath->c_str());
        }
    }

    if (savePath) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToFile(std::filesystem::path(*savePath), map, &err, compressSmf)) {
            status = std::string("Save failed: ") + SmfErrorMessage(err);
        } else {
            currentPath = *savePath;
            dirty = false;
            LibUI::Core::RecentPathPush(savePath->c_str());
            std::vector<std::byte> bytes;
            if (Solstice::Smf::SaveSmfToBytes(map, bytes, &err, compressSmf) &&
                Solstice::Smf::LoadSmfFromBytes(map, bytes, &hdr, &err)) {
                status = "Saved " + *savePath;
            } else {
                status = "Saved file but failed to refresh header view.";
            }
        }
    }
}

void EditSmfPropertyValue(SmfProperty& prop, bool& dirty) {
    ImGui::PushID(static_cast<int>(reinterpret_cast<uintptr_t>(&prop)));

    switch (prop.Type) {
    case SmfAttributeType::Bool: {
        bool b = std::get_if<bool>(&prop.Value) ? *std::get_if<bool>(&prop.Value) : false;
        if (ImGui::Checkbox("##val", &b)) {
            prop.Value = b;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Int32: {
        int v = std::get_if<int32_t>(&prop.Value) ? *std::get_if<int32_t>(&prop.Value) : 0;
        if (ImGui::InputInt("##val", &v)) {
            prop.Value = static_cast<int32_t>(v);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Int64: {
        long long v = std::get_if<int64_t>(&prop.Value) ? static_cast<long long>(*std::get_if<int64_t>(&prop.Value)) : 0LL;
        if (ImGui::InputScalar("##val", ImGuiDataType_S64, &v)) {
            prop.Value = static_cast<int64_t>(v);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Float: {
        float v = std::get_if<float>(&prop.Value) ? *std::get_if<float>(&prop.Value) : 0.f;
        if (ImGui::DragFloat("##val", &v, 0.01f)) {
            prop.Value = v;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Double: {
        double v = std::get_if<double>(&prop.Value) ? *std::get_if<double>(&prop.Value) : 0.0;
        if (ImGui::InputDouble("##val", &v)) {
            prop.Value = v;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec2: {
        SmfVec2 a = std::get_if<SmfVec2>(&prop.Value) ? *std::get_if<SmfVec2>(&prop.Value) : SmfVec2{};
        float f[2] = {a.x, a.y};
        if (ImGui::DragFloat2("##val", f, 0.01f)) {
            prop.Value = SmfVec2{f[0], f[1]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec3: {
        SmfVec3 a = std::get_if<SmfVec3>(&prop.Value) ? *std::get_if<SmfVec3>(&prop.Value) : SmfVec3{};
        float f[3] = {a.x, a.y, a.z};
        if (ImGui::DragFloat3("##val", f, 0.01f)) {
            prop.Value = SmfVec3{f[0], f[1], f[2]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Vec4:
    case SmfAttributeType::ColorRGBA: {
        SmfVec4 a = std::get_if<SmfVec4>(&prop.Value) ? *std::get_if<SmfVec4>(&prop.Value) : SmfVec4{};
        float f[4] = {a.x, a.y, a.z, a.w};
        if (ImGui::DragFloat4("##val", f, 0.01f)) {
            prop.Value = SmfVec4{f[0], f[1], f[2], f[3]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Quaternion: {
        SmfQuaternion q = std::get_if<SmfQuaternion>(&prop.Value) ? *std::get_if<SmfQuaternion>(&prop.Value)
                                                                    : SmfQuaternion{};
        float f[4] = {q.x, q.y, q.z, q.w};
        if (ImGui::DragFloat4("##val", f, 0.001f)) {
            prop.Value = SmfQuaternion{f[0], f[1], f[2], f[3]};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::Matrix4: {
        SmfMatrix4 m = std::get_if<SmfMatrix4>(&prop.Value) ? *std::get_if<SmfMatrix4>(&prop.Value) : SmfMatrix4{};
        bool changed = false;
        if (ImGui::TreeNode("matrix")) {
            for (int r = 0; r < 4; ++r) {
                float row[4] = {m.m[static_cast<size_t>(r)][0], m.m[static_cast<size_t>(r)][1],
                    m.m[static_cast<size_t>(r)][2], m.m[static_cast<size_t>(r)][3]};
                if (ImGui::DragFloat4(("##r" + std::to_string(r)).c_str(), row, 0.01f)) {
                    for (int c = 0; c < 4; ++c) {
                        m.m[static_cast<size_t>(r)][static_cast<size_t>(c)] = row[c];
                    }
                    changed = true;
                }
            }
            ImGui::TreePop();
        }
        if (changed) {
            prop.Value = m;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::String: {
        std::string s = std::get_if<std::string>(&prop.Value) ? *std::get_if<std::string>(&prop.Value) : std::string{};
        char sbuf[2048];
        std::snprintf(sbuf, sizeof(sbuf), "%s", s.c_str());
        if (ImGui::InputText("##val", sbuf, sizeof(sbuf))) {
            prop.Value = std::string(sbuf);
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::AssetHash:
    case SmfAttributeType::ArzachelSeed: {
        unsigned long long h = std::get_if<uint64_t>(&prop.Value) ? *std::get_if<uint64_t>(&prop.Value) : 0ull;
        if (ImGui::InputScalar("##val", ImGuiDataType_U64, &h, nullptr, nullptr, "%016llX")) {
            prop.Value = h;
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::BlobOpaque:
    case SmfAttributeType::SkeletonPose: {
        const auto* blob = std::get_if<std::vector<std::byte>>(&prop.Value);
        const size_t n = blob ? blob->size() : 0;
        ImGui::Text("%zu bytes (raw)", n);
        if (ImGui::SmallButton("Clear##blob")) {
            prop.Value = std::vector<std::byte>{};
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::ElementRef: {
        uint32_t r = std::get_if<uint32_t>(&prop.Value) ? *std::get_if<uint32_t>(&prop.Value) : 0u;
        int ir = static_cast<int>(r);
        if (ImGui::InputInt("##val", &ir)) {
            prop.Value = static_cast<uint32_t>(std::max(0, ir));
            dirty = true;
        }
        break;
    }
    case SmfAttributeType::EasingType:
    case SmfAttributeType::TransitionBlendMode: {
        int u = std::get_if<uint8_t>(&prop.Value) ? static_cast<int>(*std::get_if<uint8_t>(&prop.Value)) : 0;
        if (ImGui::SliderInt("##val", &u, 0, 255)) {
            prop.Value = static_cast<uint8_t>(u);
            dirty = true;
        }
        break;
    }
    default:
        ImGui::TextUnformatted("(unsupported)");
        break;
    }

    ImGui::PopID();
}

const SmfVec3* TryGetEntityOriginVec3(const SmfEntity& ent) {
    for (const auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            if (auto* v = std::get_if<SmfVec3>(&pr.Value)) {
                return v;
            }
        }
    }
    return nullptr;
}

bool SetEntityOriginVec3(SmfEntity& ent, const SmfVec3& pos) {
    for (auto& pr : ent.Properties) {
        if ((pr.Key == "origin" || pr.Key == "position") && pr.Type == SmfAttributeType::Vec3) {
            pr.Value = SmfValue{pos};
            return true;
        }
    }
    ent.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{pos}});
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed" << std::endl;
        return -1;
    }

    SDL_Window* window =
        SDL_CreateWindow("Jackhammer — Solstice Level Editor", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed" << std::endl;
        SDL_Quit();
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "OpenGL context failed" << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_SetSwapInterval(1);

    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI init failed" << std::endl;
        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    Solstice::MovieMaker::PreviewTextureRgba jackhammerEnginePreviewTex{};

    SmfMap map;
    std::optional<std::string> currentPath;
    SmfFileHeader lastHeader{};
    std::string status = "Ready.";
    bool dirty = false;
    int selectedEntity = -1;
    std::string lastValidateCodec = "(not run yet)";
    std::string lastValidateEngine = "(not run yet)";
    char entityNameFilter[256] = "";
    bool compressSmf = false;

    if (argc >= 2) {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        std::filesystem::path p(argv[1]);
        if (Solstice::Smf::LoadSmfFromFile(p, map, &lastHeader, &err)) {
            currentPath = p.string();
            selectedEntity = map.Entities.empty() ? -1 : 0;
            status = "Loaded " + currentPath.value();
            LibUI::Core::RecentPathPush(p.string().c_str());
        } else {
            status = std::string("Failed to open argument: ") + SmfErrorMessage(err);
        }
    }

    auto doNew = [&]() {
        map.Clear();
        currentPath.reset();
        lastHeader = {};
        selectedEntity = -1;
        dirty = false;
        status = "New map.";
    };

    auto doSaveToDisk = [&](const std::string& pathStr) -> bool {
        Solstice::Smf::SmfError err = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToFile(std::filesystem::path(pathStr), map, &err, compressSmf)) {
            status = std::string("Save failed: ") + SmfErrorMessage(err);
            return false;
        }
        currentPath = pathStr;
        dirty = false;
        LibUI::Core::RecentPathPush(pathStr.c_str());
        std::vector<std::byte> bytes;
        if (Solstice::Smf::SaveSmfToBytes(map, bytes, &err, compressSmf)) {
            Solstice::Smf::LoadSmfFromBytes(map, bytes, &lastHeader, &err);
        }
        status = "Saved " + pathStr;
        return true;
    };

    auto runValidate = [&]() {
        std::vector<std::byte> bytes;
        Solstice::Smf::SmfError verr = Solstice::Smf::SmfError::None;
        if (!Solstice::Smf::SaveSmfToBytes(map, bytes, &verr)) {
            lastValidateCodec = std::string("Serialize failed: ") + SmfErrorMessage(verr);
            lastValidateEngine = "(skipped — no serialized bytes)";
            status = lastValidateCodec;
            return;
        }
        SmfMap tmp;
        Solstice::Smf::SmfError err2 = Solstice::Smf::SmfError::None;
        SmfFileHeader h2{};
        if (!Solstice::Smf::LoadSmfFromBytes(tmp, bytes, &h2, &err2)) {
            lastValidateCodec = std::string("Round-trip parse failed: ") + SmfErrorMessage(err2);
            lastValidateEngine = "(skipped — LibSmf round-trip did not succeed)";
            status = lastValidateCodec;
            return;
        }
        lastValidateCodec = "LibSmf codec: OK (serialize → parse round-trip).";
        lastValidateEngine = EngineDllSmfValidateSummary(bytes);
        status = lastValidateCodec + " " + lastValidateEngine;
    };

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            LibUI::Core::ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        DrainPendingFileOps(map, currentPath, lastHeader, status, dirty, selectedEntity, compressSmf);

        UpdateWindowTitle(window, currentPath, dirty);

        LibUI::Core::NewFrame();

        ImGuiIO& io = ImGui::GetIO();
        const bool allowShortcuts = !io.WantTextInput;
        if (allowShortcuts) {
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) {
                doNew();
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) {
                LibUI::FileDialogs::ShowOpenFile(
                    window, "Open map", [](std::optional<std::string> path) {
                        if (path) {
                            QueueOpenPath(std::move(*path));
                        }
                    },
                    kSmfFilters);
            }
            if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) {
                if (currentPath) {
                    doSaveToDisk(*currentPath);
                } else {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Save map as", [](std::optional<std::string> path) {
                            if (path) {
                                QueueSavePath(std::move(*path));
                            }
                        },
                        kSmfFilters);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) {
                runValidate();
            }
        }

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::Begin("JackhammerRoot", nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Validate map", "F7")) {
                    runValidate();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("New", "Ctrl+N")) {
                    doNew();
                }
                if (ImGui::MenuItem("New from template")) {
                    ApplyNewMapTemplate(map, currentPath, lastHeader, selectedEntity, dirty, status);
                }
                if (ImGui::MenuItem("Open…", "Ctrl+O")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Open map", [](std::optional<std::string> path) {
                            if (path) {
                                QueueOpenPath(std::move(*path));
                            }
                        },
                        kSmfFilters);
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    if (currentPath) {
                        doSaveToDisk(*currentPath);
                    } else {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Save map as", [](std::optional<std::string> path) {
                                if (path) {
                                    QueueSavePath(std::move(*path));
                                }
                            },
                            kSmfFilters);
                    }
                }
                if (ImGui::MenuItem("Save As…")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Save map as", [](std::optional<std::string> path) {
                            if (path) {
                                QueueSavePath(std::move(*path));
                            }
                        },
                        kSmfFilters);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::TextUnformatted("Jackhammer — .smf v1");
        ImGui::Separator();

        if (ImGui::Button("New")) {
            doNew();
        }
        ImGui::SameLine();
        if (ImGui::Button("Template")) {
            ApplyNewMapTemplate(map, currentPath, lastHeader, selectedEntity, dirty, status);
        }
        ImGui::SameLine();
        if (ImGui::Button("Open…")) {
            LibUI::FileDialogs::ShowOpenFile(
                window, "Open map", [](std::optional<std::string> path) {
                    if (path) {
                        QueueOpenPath(std::move(*path));
                    }
                },
                kSmfFilters);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (currentPath) {
                doSaveToDisk(*currentPath);
            } else {
                LibUI::FileDialogs::ShowSaveFile(
                    window, "Save map as", [](std::optional<std::string> path) {
                        if (path) {
                            QueueSavePath(std::move(*path));
                        }
                    },
                    kSmfFilters);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As…")) {
            LibUI::FileDialogs::ShowSaveFile(
                window, "Save map as", [](std::optional<std::string> path) {
                    if (path) {
                        QueueSavePath(std::move(*path));
                    }
                },
                kSmfFilters);
        }
        ImGui::SameLine();
        ImGui::Checkbox("ZSTD compress", &compressSmf);
        ImGui::SameLine();
        if (ImGui::Button("Validate map")) {
            runValidate();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(F7)");

        if (currentPath) {
            ImGui::Text("File: %s", currentPath->c_str());
        } else {
            ImGui::TextUnformatted("File: (unsaved)");
        }
        ImGui::TextUnformatted(status.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("LibSmf: %s", lastValidateCodec.c_str());
        ImGui::TextWrapped("Engine DLL: %s", lastValidateEngine.c_str());

        if (LibUI::Core::RecentPathGetCount() > 0) {
            if (ImGui::CollapsingHeader("Recent paths (LibUI)", ImGuiTreeNodeFlags_DefaultOpen)) {
                const int rc = LibUI::Core::RecentPathGetCount();
                for (int ri = 0; ri < rc; ++ri) {
                    const char* rp = LibUI::Core::RecentPathGet(ri);
                    if (!rp || !rp[0]) {
                        continue;
                    }
                    ImGui::PushID(ri);
                    if (ImGui::SmallButton("Open")) {
                        QueueOpenPath(std::string(rp));
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(rp);
                    ImGui::PopID();
                }
            }
        }

        ImGui::Separator();

        ImGui::BeginChild("split", ImVec2(0, 0), false, ImGuiWindowFlags_NoSavedSettings);

        ImGui::BeginChild("left", ImVec2(280, 0), true);
        ImGui::TextUnformatted("Entities");
        ImGui::InputText("Filter by name", entityNameFilter, sizeof(entityNameFilter));
        if (ImGui::Button("Add")) {
            const int n = static_cast<int>(map.Entities.size());
            map.Entities.push_back(SmfMakeEntity("entity_" + std::to_string(n), "Entity"));
            selectedEntity = static_cast<int>(map.Entities.size()) - 1;
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate") && selectedEntity >= 0 &&
            selectedEntity < static_cast<int>(map.Entities.size())) {
            map.Entities.push_back(map.Entities[static_cast<size_t>(selectedEntity)]);
            selectedEntity = static_cast<int>(map.Entities.size()) - 1;
            dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove") && selectedEntity >= 0 &&
            selectedEntity < static_cast<int>(map.Entities.size())) {
            ImGui::OpenPopup("ConfirmDeleteEntity");
        }

        if (ImGui::BeginPopupModal("ConfirmDeleteEntity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (selectedEntity >= 0 && selectedEntity < static_cast<int>(map.Entities.size())) {
                const std::string& nm = map.Entities[static_cast<size_t>(selectedEntity)].Name;
                ImGui::Text("Remove entity '%s'?", nm.empty() ? "(unnamed)" : nm.c_str());
            } else {
                ImGui::TextUnformatted("No entity selected.");
            }
            if (ImGui::Button("Delete", ImVec2(120, 0)) && selectedEntity >= 0
                && selectedEntity < static_cast<int>(map.Entities.size())) {
                map.Entities.erase(map.Entities.begin() + selectedEntity);
                if (map.Entities.empty()) {
                    selectedEntity = -1;
                } else {
                    selectedEntity = std::min(selectedEntity, static_cast<int>(map.Entities.size()) - 1);
                }
                dirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        if (ImGui::BeginTable("entList", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            for (int i = 0; i < static_cast<int>(map.Entities.size()); ++i) {
                const auto& e = map.Entities[static_cast<size_t>(i)];
                std::string lname = e.Name.empty() ? "(unnamed)" : e.Name;
                std::string lfilter(entityNameFilter);
                if (!lfilter.empty()) {
                    std::string ln = lname;
                    std::transform(ln.begin(), ln.end(), ln.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    std::transform(lfilter.begin(), lfilter.end(), lfilter.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (ln.find(lfilter) == std::string::npos) {
                        continue;
                    }
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                const bool sel = (selectedEntity == i);
                if (ImGui::Selectable(e.Name.empty() ? "(unnamed)" : e.Name.c_str(), sel, ImGuiSelectableFlags_SpanAllColumns)) {
                    selectedEntity = i;
                }
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(e.ClassName.empty() ? "?" : e.ClassName.c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("right", ImVec2(0, 0), true);
        if (selectedEntity >= 0 && selectedEntity < static_cast<int>(map.Entities.size())) {
            SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
            ImGui::Text("Entity %d", selectedEntity);
            char nameBuf[512];
            char classBuf[512];
            std::snprintf(nameBuf, sizeof(nameBuf), "%s", ent.Name.c_str());
            std::snprintf(classBuf, sizeof(classBuf), "%s", ent.ClassName.c_str());
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                ent.Name = nameBuf;
                dirty = true;
            }
            if (ImGui::InputText("Class", classBuf, sizeof(classBuf))) {
                ent.ClassName = classBuf;
                dirty = true;
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Properties");
            if (ImGui::Button("Add property")) {
                ent.Properties.push_back({"newKey", SmfAttributeType::Float, SmfDefaultValueForType(SmfAttributeType::Float)});
                dirty = true;
            }

            for (size_t pi = 0; pi < ent.Properties.size(); ++pi) {
                SmfProperty& prop = ent.Properties[pi];
                ImGui::PushID(static_cast<int>(pi));
                char keyBuf[256];
                std::snprintf(keyBuf, sizeof(keyBuf), "%s", prop.Key.c_str());
                ImGui::InputText("Key", keyBuf, sizeof(keyBuf));
                if (std::string(keyBuf) != prop.Key) {
                    prop.Key = keyBuf;
                    dirty = true;
                }

                if (ImGui::BeginCombo("Type", SmfAttributeTypeLabel(prop.Type))) {
                    for (int ti = 0; ti <= 18; ++ti) {
                        const auto tt = static_cast<SmfAttributeType>(ti);
                        const bool typeMatch = (prop.Type == tt);
                        if (ImGui::Selectable(SmfAttributeTypeLabel(tt), typeMatch)) {
                            prop.Type = tt;
                            prop.Value = SmfDefaultValueForType(tt);
                            dirty = true;
                        }
                        if (typeMatch) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    ent.Properties.erase(ent.Properties.begin() + static_cast<ptrdiff_t>(pi));
                    dirty = true;
                    ImGui::PopID();
                    break;
                }

                EditSmfPropertyValue(prop, dirty);
                ImGui::Separator();
                ImGui::PopID();
            }
        } else {
            ImGui::TextUnformatted("Select an entity or add one.");
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Path table (RELIC)");
        if (ImGui::Button("Add path")) {
            map.PathTable.push_back({"", 0});
            dirty = true;
        }
        for (size_t pi = 0; pi < map.PathTable.size(); ++pi) {
            ImGui::PushID(static_cast<int>(pi + 4096));
            char pbuf[512];
            std::snprintf(pbuf, sizeof(pbuf), "%s", map.PathTable[pi].first.c_str());
            ImGui::InputText("##path", pbuf, sizeof(pbuf));
            if (std::string(pbuf) != map.PathTable[pi].first) {
                map.PathTable[pi].first = pbuf;
                dirty = true;
            }
            ImGui::SameLine();
            unsigned long long h = map.PathTable[pi].second;
            ImGui::SetNextItemWidth(180);
            if (ImGui::InputScalar("##hash", ImGuiDataType_U64, &h, nullptr, nullptr, "%016llX")) {
                map.PathTable[pi].second = h;
                dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove##pt")) {
                map.PathTable.erase(map.PathTable.begin() + static_cast<ptrdiff_t>(pi));
                dirty = true;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        ImGui::EndChild();
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextUnformatted("Engine viewport (bgfx mesh preview + orbit + XZ grid; entity origins)");
        static LibUI::Viewport::OrbitPanZoomState s_engineViewportNav{};
        static bool s_showAllEntityMarkers = true;
        if (ImGui::Button("Persp")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::Perspective;
        }
        ImGui::SameLine();
        if (ImGui::Button("Top")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoTop;
        }
        ImGui::SameLine();
        if (ImGui::Button("Front")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoFront;
        }
        ImGui::SameLine();
        if (ImGui::Button("Side")) {
            s_engineViewportNav.projection = LibUI::Viewport::OrbitProjectionMode::OrthoSide;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset view")) {
            LibUI::Viewport::ResetOrbitPanZoom(s_engineViewportNav);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Focus sel") && selectedEntity >= 0 &&
            static_cast<size_t>(selectedEntity) < map.Entities.size()) {
            const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(selectedEntity)]);
            if (o) {
                LibUI::Viewport::FocusOrbitOnTarget(s_engineViewportNav, o->x, o->y, o->z, 0.f, 0.f, 0.f);
            }
        }
        ImGui::SameLine();
        ImGui::Checkbox("All origins", &s_showAllEntityMarkers);

        LibUI::Viewport::Frame engineVp{};
        if (LibUI::Viewport::BeginHost("jh_engine_viewport", ImVec2(-1, 200), true)) {
            if (LibUI::Viewport::PollFrame(engineVp) && engineVp.draw_list) {
                const float aspect =
                    std::max(engineVp.size.y, 1.0f) > 0.f ? engineVp.size.x / std::max(engineVp.size.y, 1.0f) : 1.f;
                const int engW = std::max(2, static_cast<int>(engineVp.size.x));
                const int engH = std::max(2, static_cast<int>(engineVp.size.y));
                std::vector<Solstice::EditorEnginePreview::PreviewEntity> engEnts;
                engEnts.reserve(map.Entities.size());
                for (size_t ei = 0; ei < map.Entities.size(); ++ei) {
                    const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[ei]);
                    if (!o) {
                        continue;
                    }
                    Solstice::EditorEnginePreview::PreviewEntity pe{};
                    pe.Position = Solstice::Math::Vec3(o->x, o->y, o->z);
                    const bool isSel = selectedEntity >= 0 && static_cast<size_t>(selectedEntity) == ei;
                    pe.Albedo =
                        isSel ? Solstice::Math::Vec3(1.f, 0.72f, 0.22f) : Solstice::Math::Vec3(0.52f, 0.56f, 0.72f);
                    pe.HalfExtent = isSel ? 0.42f : 0.3f;
                    engEnts.push_back(pe);
                }
                Solstice::Physics::LightSource sun{};
                sun.Type = Solstice::Physics::LightSource::LightType::Directional;
                sun.Position = Solstice::Math::Vec3(0.42f, 0.84f, 0.36f).Normalized();
                sun.Color = Solstice::Math::Vec3(1.f, 0.96f, 0.88f);
                sun.Intensity = 1.2f;
                const Solstice::Physics::LightSource* lightPtr = &sun;
                size_t lightCount = 1;
                std::vector<std::byte> capRgba;
                int cw = 0;
                int ch = 0;
                if (Solstice::EditorEnginePreview::CaptureOrbitRgb(s_engineViewportNav, 0.f, 0.f, 0.f, 55.f, aspect,
                        engW, engH, engEnts.data(), engEnts.size(), lightPtr, lightCount, capRgba, cw, ch)) {
                    jackhammerEnginePreviewTex.SetSizeUpload(window, static_cast<uint32_t>(cw), static_cast<uint32_t>(ch),
                        capRgba.data(), capRgba.size());
                }
                if (jackhammerEnginePreviewTex.Valid()) {
                    LibUI::Viewport::DrawTextureLetterboxed(engineVp.draw_list, jackhammerEnginePreviewTex.ImGuiTexId(),
                        engineVp.min, engineVp.max, static_cast<float>(jackhammerEnginePreviewTex.width),
                        static_cast<float>(jackhammerEnginePreviewTex.height));
                } else {
                    LibUI::Viewport::DrawCheckerboard(engineVp.draw_list, engineVp.min, engineVp.max, 14.f,
                        IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
                }
                LibUI::Viewport::Mat4Col viewM{};
                LibUI::Viewport::Mat4Col projM{};
                LibUI::Viewport::ComputeOrbitViewProjectionColMajor(s_engineViewportNav, 0.f, 0.f, 0.f, 55.f, aspect,
                    0.12f, 2048.f, viewM, projM);
                LibUI::Viewport::DrawXZGrid(engineVp.draw_list, engineVp.min, engineVp.max, viewM, projM, 1.f,
                    IM_COL32(72, 72, 92, 200), 24);
                if (s_showAllEntityMarkers) {
                    for (size_t ei = 0; ei < map.Entities.size(); ++ei) {
                        const SmfVec3* o = TryGetEntityOriginVec3(map.Entities[ei]);
                        if (!o) {
                            continue;
                        }
                        const bool isSel =
                            selectedEntity >= 0 && static_cast<size_t>(selectedEntity) == ei;
                        if (isSel) {
                            continue;
                        }
                        LibUI::Viewport::DrawWorldCrossXZ(engineVp.draw_list, engineVp.min, engineVp.max, viewM, projM,
                            o->x, o->y, o->z, 0.2f, IM_COL32(120, 120, 140, 180));
                    }
                }
                if (selectedEntity >= 0 && static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    const auto& ent = map.Entities[static_cast<size_t>(selectedEntity)];
                    const SmfVec3* origin = TryGetEntityOriginVec3(ent);
                    if (origin) {
                        LibUI::Viewport::DrawWorldCrossXZ(engineVp.draw_list, engineVp.min, engineVp.max, viewM, projM,
                            origin->x, origin->y, origin->z, 0.35f, IM_COL32(255, 200, 64, 255));
                    }
                }

                if (engineVp.hovered && ImGui::IsKeyPressed(ImGuiKey_F, false) && selectedEntity >= 0 &&
                    static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    const SmfVec3* fo = TryGetEntityOriginVec3(map.Entities[static_cast<size_t>(selectedEntity)]);
                    if (fo) {
                        LibUI::Viewport::FocusOrbitOnTarget(s_engineViewportNav, fo->x, fo->y, fo->z, 0.f, 0.f, 0.f);
                    }
                }

                if (engineVp.hovered && io.KeyCtrl && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && selectedEntity >= 0 &&
                    static_cast<size_t>(selectedEntity) < map.Entities.size()) {
                    const ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                    if (drag.x * drag.x + drag.y * drag.y < 25.f) {
                        float hitX = 0.f;
                        float hitZ = 0.f;
                        if (LibUI::Viewport::ScreenToXZPlane(viewM, projM, engineVp.min, engineVp.max,
                                ImGui::GetMousePos(), 0.f, hitX, hitZ)) {
                            SmfEntity& ent = map.Entities[static_cast<size_t>(selectedEntity)];
                            const SmfVec3* cur = TryGetEntityOriginVec3(ent);
                            const float keepY = cur ? cur->y : 0.f;
                            SetEntityOriginVec3(ent, SmfVec3{hitX, keepY, hitZ});
                            dirty = true;
                        }
                    }
                }

                LibUI::Viewport::ApplyOrbitPanZoom(s_engineViewportNav, engineVp);
                const char* projName = "persp";
                switch (s_engineViewportNav.projection) {
                case LibUI::Viewport::OrbitProjectionMode::Perspective:
                    projName = "persp";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoTop:
                    projName = "ortho top";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoFront:
                    projName = "ortho front";
                    break;
                case LibUI::Viewport::OrbitProjectionMode::OrthoSide:
                    projName = "ortho side";
                    break;
                }
                char navBuf[256]{};
                std::snprintf(navBuf, sizeof(navBuf),
                    "%s | yaw %.2f pitch %.2f dist %.2f | pan %.1f, %.1f | LMB orbit | Alt+LMB/MMB pan | wheel zoom | "
                    "F focus | Ctrl+LMB place XZ @ y=0",
                    projName, static_cast<double>(s_engineViewportNav.yaw),
                    static_cast<double>(s_engineViewportNav.pitch), static_cast<double>(s_engineViewportNav.distance),
                    static_cast<double>(s_engineViewportNav.pan_x), static_cast<double>(s_engineViewportNav.pan_y));
                LibUI::Viewport::DrawViewportLabel(engineVp.draw_list, engineVp.min, engineVp.max, navBuf,
                    ImVec2(1.0f, 0.0f));
            }
            LibUI::Viewport::EndHost();
        }

        ImGui::Separator();
        ImGui::Text("Sections (bytes): str=%u geom=%u bsp=%u ent=%u sec=%u phys=%u script=%u trig=%u path=%u",
            lastHeader.StringTableSize, lastHeader.GeometrySize, lastHeader.BspSize, lastHeader.EntitySize,
            lastHeader.SectorSize, lastHeader.PhysicsSize, lastHeader.ScriptSize, lastHeader.TriggerSize,
            lastHeader.PathTableSize);

        ImGui::End();

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        LibUI::Core::Render();
        SDL_GL_SwapWindow(window);
    }

    jackhammerEnginePreviewTex.Destroy();
    Solstice::EditorEnginePreview::Shutdown();
    LibUI::Core::Shutdown();
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
