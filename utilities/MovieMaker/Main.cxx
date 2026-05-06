// Solstice MovieMaker (SMM) — MVP: import assets, edit minimal PARALLAX scene, import/export .prlx.
// Video export: MG → OpenGL → ffmpeg (MP4/MOV).

#include "FfmpegUtil.hxx"
#include "LibUI/Graphics/PreviewTexture.hxx"
#include "Media/SmmAudio.hxx"
#include "Media/SmmImage.hxx"
#include "SmmGltf.hxx"
#include "SmmFileOps.hxx"
#include "SmmKeyframePresets.hxx"
#include "SmmMidiParse.hxx"
#include "SmmTimelineRangePresets.hxx"
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
#include "LibUI/Layout/SplitPane.hxx"
#include "LibUI/Widgets/Widgets.hxx"
#include "LibUI/FileDialogs/FileDialogs.hxx"
#include "LibUI/Icons/Icons.hxx"
#include "LibUI/Shell/GlWindow.hxx"
#include "LibUI/Theme/Theme.hxx"
#include "LibUI/Timeline/TimelineModel.hxx"
#include "LibUI/Timeline/TimelineWidget.hxx"
#include "LibUI/Workspace/PanelRegistry.hxx"
#include "LibUI/Tools/DiagLog.hxx"
#include "LibUI/Tools/PathInputBrowse.hxx"
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
#include <cmath>
#include <cstdio>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <functional>
#include <cstring>
#include <csignal>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
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

static void CollectAssetHashesForElement(
    const Solstice::Parallax::ParallaxScene& scene, const Solstice::Parallax::ParallaxScene::ElementNode& el,
    std::unordered_set<uint64_t>& outHashes) {
    if (el.SchemaIndex >= scene.GetSchemas().size()) {
        return;
    }
    const auto& schema = scene.GetSchemas()[el.SchemaIndex];
    for (const auto& ad : schema.Attributes) {
        if (ad.Type != Solstice::Parallax::AttributeType::AssetHash) {
            continue;
        }
        auto it = el.Attributes.find(ad.Name);
        if (it == el.Attributes.end()) {
            continue;
        }
        if (const auto* h = std::get_if<uint64_t>(&it->second); h && *h != 0ull) {
            outHashes.insert(*h);
        }
    }
}

static void CollectAssetHashesForMG(
    const Solstice::Parallax::ParallaxScene& scene, const Solstice::Parallax::MGElementRecord& mg,
    std::unordered_set<uint64_t>& outHashes) {
    if (mg.SchemaIndex >= scene.GetSchemas().size()) {
        return;
    }
    const auto& schema = scene.GetSchemas()[mg.SchemaIndex];
    for (const auto& ad : schema.Attributes) {
        if (ad.Type != Solstice::Parallax::AttributeType::AssetHash) {
            continue;
        }
        auto it = mg.Attributes.find(ad.Name);
        if (it == mg.Attributes.end()) {
            continue;
        }
        if (const auto* h = std::get_if<uint64_t>(&it->second); h && *h != 0ull) {
            outHashes.insert(*h);
        }
    }
}

static void PrepareSceneEmbeddedAssets(
    Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver) {
    scene.GetEmbeddedAssets().clear();
    auto& pathTable = scene.GetPathTable();
    for (const auto& kv : resolver.GetPathBindings()) {
        pathTable[kv.first] = kv.second;
    }

    std::unordered_set<uint64_t> wantedHashes;
    for (const auto& el : scene.GetElements()) {
        CollectAssetHashesForElement(scene, el, wantedHashes);
    }
    for (const auto& mg : scene.GetMGElements()) {
        CollectAssetHashesForMG(scene, mg, wantedHashes);
    }
    for (const auto& kv : pathTable) {
        if (kv.second != 0ull) {
            wantedHashes.insert(kv.second);
        }
    }

    for (const auto& h : wantedHashes) {
        Solstice::Parallax::AssetData data{};
        if (resolver.Resolve(h, data)) {
            scene.GetEmbeddedAssets()[h] = std::move(data);
        }
    }
}

std::vector<std::pair<std::string, std::string>> g_MovieMakerPluginLoadErrors;
static Solstice::UtilityPluginHost::UtilityPluginHost& MovieMakerPlugins() {
    static Solstice::UtilityPluginHost::UtilityPluginHost host;
    return host;
}

static std::vector<Smm::Keyframe::KeyframeCurvePreset> g_smmKeyframePresets;
static std::vector<Smm::Timeline::RangePreset> g_smmRangePresets;
static const char* g_smmCrashStage = "startup";
static uint64_t g_smmCrashFrame = 0;

static void SmmSetCrashStage(const char* stage) {
    g_smmCrashStage = stage ? stage : "(null)";
}

#if defined(_WIN32)
static void SmmLogNativeStackTrace(const char* reason) {
    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    (void)SymInitialize(process, nullptr, TRUE);

    void* frames[64]{};
    const USHORT frameCount = CaptureStackBackTrace(0, 64, frames, nullptr);
    LibUI::Tools::DiagLogLine(std::string("[SMM][CRASH] ") + (reason ? reason : "unknown"));
    LibUI::Tools::DiagLogLine(
        "[SMM][CRASH] stage=" + std::string(g_smmCrashStage ? g_smmCrashStage : "(null)")
        + " frame=" + std::to_string(static_cast<unsigned long long>(g_smmCrashFrame)));

    for (USHORT i = 0; i < frameCount; ++i) {
        DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
        char storage[sizeof(SYMBOL_INFO) + 512]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 511;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol)) {
            LibUI::Tools::DiagLogLine(
                "[SMM][STACK] #" + std::to_string(i) + " " + symbol->Name + " +0x" + std::to_string(displacement));
        } else {
            LibUI::Tools::DiagLogLine("[SMM][STACK] #" + std::to_string(i) + " 0x" + std::to_string(address));
        }
    }
}

LONG WINAPI SmmUnhandledExceptionFilter(EXCEPTION_POINTERS* info) {
    uint32_t code = info && info->ExceptionRecord ? static_cast<uint32_t>(info->ExceptionRecord->ExceptionCode) : 0u;
    const std::string msg = "Unhandled SEH exception code=0x" + std::to_string(code);
    SmmLogNativeStackTrace(msg.c_str());
    return EXCEPTION_EXECUTE_HANDLER;
}

void SmmSignalCrashHandler(int sig) {
    const std::string msg = "Signal " + std::to_string(sig);
    SmmLogNativeStackTrace(msg.c_str());
    std::_Exit(128 + sig);
}

void SmmInstallCrashDiagnostics() {
    SetUnhandledExceptionFilter(SmmUnhandledExceptionFilter);
    std::signal(SIGABRT, SmmSignalCrashHandler);
    std::signal(SIGSEGV, SmmSignalCrashHandler);
}
#else
void SmmInstallCrashDiagnostics() {}
#endif

static std::vector<std::filesystem::path> SmmBuildPresetSearchRoots(const std::filesystem::path& smmJsonPath) {
    std::vector<std::filesystem::path> roots;
    const char* bp = SDL_GetBasePath();
    if (bp) {
        roots.push_back(std::filesystem::path(bp) / "presets");
#if !defined(_WIN32)
        SDL_free((void*)bp);
#endif
    }
    if (!smmJsonPath.empty()) {
        roots.push_back(smmJsonPath.parent_path() / "presets");
    }
    return roots;
}

static void ReloadSmmWorkspaceIniPresets(const std::filesystem::path& smmJsonPath) {
    const std::vector<std::filesystem::path> r = SmmBuildPresetSearchRoots(smmJsonPath);
    Smm::Keyframe::ScanCurvePresetsFromRoots(r, g_smmKeyframePresets);
    Smm::Timeline::ScanRangePresetsFromRoots(r, g_smmRangePresets);
}

static void SmmOnReloadIniPresets(void* user) {
    if (user) {
        ReloadSmmWorkspaceIniPresets(*static_cast<std::filesystem::path*>(user));
    }
}

void LoadMovieMakerPlugins() {
    g_MovieMakerPluginLoadErrors.clear();
    try {
        const char* base = SDL_GetBasePath();
        std::filesystem::path dir = base ? std::filesystem::path(base) / "plugins" : std::filesystem::path("plugins");
        if (base) {
#if !defined(_WIN32)
            SDL_free((void*)base);
#endif
        }
        Solstice::UtilityPluginHost::PluginAbiSymbols abi{};
        abi.GetName = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_GETNAME;
        abi.OnLoad = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONLOAD;
        abi.OnUnload = SOLSTICE_UTILITY_ABI_MOVIE_MAKER_ONUNLOAD;
        auto& plugins = MovieMakerPlugins();
        plugins.UnloadAll();
        plugins.LoadAllFromDirectory(dir.string(), abi, g_MovieMakerPluginLoadErrors);
    } catch (const std::exception& ex) {
        g_MovieMakerPluginLoadErrors.push_back({"plugins/", std::string("Reload failed: ") + ex.what()});
    } catch (...) {
        g_MovieMakerPluginLoadErrors.push_back({"plugins/", "Reload failed: unknown exception."});
    }
}

void MovieMakerPluginsDrawPanel(bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    LibUI::Widgets::SetNextWindowSize(ImVec2(440, 240), ImGuiCond_FirstUseEver);
    if (LibUI::Widgets::BeginWindow("Plugins##SMM", pOpen)) {
        LibUI::Widgets::Text("Native plugins: ./plugins next to MovieMaker");
        if (LibUI::Widgets::Button("Reload##mmplug")) {
            LoadMovieMakerPlugins();
        }
        LibUI::Widgets::Separator();
        if (!g_MovieMakerPluginLoadErrors.empty()) {
            for (const auto& fe : g_MovieMakerPluginLoadErrors) {
                ImGui::BulletText("%s\n  %s", fe.first.c_str(), fe.second.c_str());
            }
            LibUI::Widgets::Separator();
        }
        std::vector<Solstice::UtilityPluginHost::ModuleSummary> mods;
        MovieMakerPlugins().EnumerateModules(mods);
        if (mods.empty()) {
            LibUI::Widgets::Text("No plugins loaded.");
        }
        for (const auto& m : mods) {
            ImGui::BulletText("%s — %s", m.DisplayName.c_str(), m.PathUtf8.c_str());
        }
    }
    LibUI::Widgets::EndWindow();
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

Solstice::Parallax::ChannelIndex EnsureElementChannel(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex element,
    std::string_view attribute, Solstice::Parallax::AttributeType type) {
    Solstice::Parallax::ChannelIndex ch = FindChannelForAttribute(scene, element, attribute, type);
    if (ch == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        ch = Solstice::Parallax::AddChannel(scene, element, attribute, type);
    }
    return ch;
}

uint32_t FindMGTrack(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, std::string_view property) {
    if (mgIndex >= scene.GetMGElements().size()) {
        return Solstice::Parallax::PARALLAX_INVALID_INDEX;
    }
    const auto& mg = scene.GetMGElements()[mgIndex];
    const auto& tracks = scene.GetMGTracks();
    for (uint32_t i = 0; i < mg.TrackCount; ++i) {
        const uint32_t ti = mg.FirstTrackIndex + i;
        if (ti >= tracks.size()) {
            break;
        }
        if (tracks[ti].PropertyName == property) {
            return ti;
        }
    }
    return Solstice::Parallax::PARALLAX_INVALID_INDEX;
}

uint32_t EnsureMGTrack(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, std::string_view property,
    Solstice::Parallax::AttributeType type) {
    uint32_t ti = FindMGTrack(scene, mgIndex, property);
    if (ti == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        ti = Solstice::Parallax::AddMGTrack(scene, mgIndex, property, type, Solstice::Parallax::EasingType::Linear);
    }
    return ti;
}

void AddFloatKeys(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex element, std::string_view attribute,
    std::initializer_list<std::pair<uint64_t, float>> keys) {
    const Solstice::Parallax::ChannelIndex ch =
        EnsureElementChannel(scene, element, attribute, Solstice::Parallax::AttributeType::Float);
    if (ch == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        return;
    }
    for (const auto& [t, v] : keys) {
        Solstice::Parallax::AddKeyframe(scene, ch, t, Solstice::Parallax::AttributeValue{v}, Solstice::Parallax::EasingType::EaseInOut);
    }
}

void AddVec3Keys(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex element, std::string_view attribute,
    std::initializer_list<std::pair<uint64_t, Solstice::Math::Vec3>> keys) {
    const Solstice::Parallax::ChannelIndex ch =
        EnsureElementChannel(scene, element, attribute, Solstice::Parallax::AttributeType::Vec3);
    if (ch == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        return;
    }
    for (const auto& [t, v] : keys) {
        Solstice::Parallax::AddKeyframe(scene, ch, t, Solstice::Parallax::AttributeValue{v}, Solstice::Parallax::EasingType::EaseInOut);
    }
}

void AddMgFloatKeys(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, std::string_view property,
    std::initializer_list<std::pair<uint64_t, float>> keys) {
    const uint32_t ti = EnsureMGTrack(scene, mgIndex, property, Solstice::Parallax::AttributeType::Float);
    if (ti == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        return;
    }
    for (const auto& [t, v] : keys) {
        Solstice::Parallax::AddMGKeyframe(scene, ti, t, Solstice::Parallax::AttributeValue{v}, Solstice::Parallax::EasingType::EaseInOut);
    }
}

void AddMgVec4Keys(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::MGIndex mgIndex, std::string_view property,
    std::initializer_list<std::pair<uint64_t, Solstice::Math::Vec4>> keys) {
    const uint32_t ti = EnsureMGTrack(scene, mgIndex, property, Solstice::Parallax::AttributeType::Vec4);
    if (ti == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        return;
    }
    for (const auto& [t, v] : keys) {
        Solstice::Parallax::AddMGKeyframe(scene, ti, t, Solstice::Parallax::AttributeValue{v}, Solstice::Parallax::EasingType::EaseInOut);
    }
}

void AppendU16BE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
}

void AppendU32BE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
}

void AppendU16LE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void AppendU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

void AppendVarLen(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t buf[5]{};
    int n = 0;
    buf[n++] = static_cast<uint8_t>(v & 0x7Fu);
    while ((v >>= 7u) != 0u) {
        buf[n++] = static_cast<uint8_t>(0x80u | (v & 0x7Fu));
    }
    while (n > 0) {
        out.push_back(buf[--n]);
    }
}

std::vector<uint8_t> BuildShowcaseMidiBytes() {
    std::vector<uint8_t> track;
    auto push = [&](uint8_t b) { track.push_back(b); };
    auto noteOn = [&](uint32_t dt, uint8_t note, uint8_t vel) {
        AppendVarLen(track, dt);
        push(0x90u);
        push(note);
        push(vel);
    };
    auto noteOff = [&](uint32_t dt, uint8_t note) {
        AppendVarLen(track, dt);
        push(0x80u);
        push(note);
        push(0u);
    };

    // 120 BPM tempo, warm synth patch, then a short heroic phrase.
    AppendVarLen(track, 0);
    push(0xFFu);
    push(0x51u);
    push(0x03u);
    push(0x07u);
    push(0xA1u);
    push(0x20u);

    AppendVarLen(track, 0);
    push(0xC0u);
    push(0x50u);

    const uint8_t phrase[] = {60, 64, 67, 72, 71, 67, 64, 60};
    constexpr size_t phraseCount = sizeof(phrase) / sizeof(phrase[0]);
    for (size_t i = 0; i < phraseCount; ++i) {
        noteOn(0, phrase[i], 98u);
        noteOff(i == phraseCount - 1 ? 960u : 480u, phrase[i]);
    }

    AppendVarLen(track, 0);
    push(0xFFu);
    push(0x2Fu);
    push(0x00u);

    std::vector<uint8_t> smf;
    smf.reserve(track.size() + 22);
    smf.push_back('M');
    smf.push_back('T');
    smf.push_back('h');
    smf.push_back('d');
    AppendU32BE(smf, 6u);
    AppendU16BE(smf, 0u);
    AppendU16BE(smf, 1u);
    AppendU16BE(smf, 480u);
    smf.push_back('M');
    smf.push_back('T');
    smf.push_back('r');
    smf.push_back('k');
    AppendU32BE(smf, static_cast<uint32_t>(track.size()));
    smf.insert(smf.end(), track.begin(), track.end());
    return smf;
}

std::vector<uint8_t> BuildShowcaseWavBytes() {
    constexpr int sampleRate = 44100;
    constexpr int channels = 1;
    constexpr int bitsPerSample = 16;
    constexpr float durationSec = 8.0f;
    const int totalSamples = static_cast<int>(durationSec * static_cast<float>(sampleRate));
    std::vector<int16_t> pcm(static_cast<size_t>(totalSamples));
    const float notesHz[] = {261.63f, 329.63f, 392.00f, 523.25f, 493.88f, 392.00f, 329.63f, 261.63f};
    const int noteSamples = sampleRate / 2; // 0.5s each
    for (int i = 0; i < totalSamples; ++i) {
        const int ni = (i / noteSamples) % static_cast<int>(sizeof(notesHz) / sizeof(notesHz[0]));
        const float hz = notesHz[ni];
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        const float envPos = static_cast<float>(i % noteSamples) / static_cast<float>(noteSamples);
        const float env = std::clamp(1.0f - envPos * 0.85f, 0.15f, 1.0f);
        const float s = std::sin(2.0f * 3.14159265f * hz * t) * 0.28f * env;
        pcm[static_cast<size_t>(i)] = static_cast<int16_t>(std::clamp(s * 32767.0f, -32767.0f, 32767.0f));
    }
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    std::vector<uint8_t> wav;
    wav.reserve(44u + dataBytes);
    wav.push_back('R');
    wav.push_back('I');
    wav.push_back('F');
    wav.push_back('F');
    AppendU32LE(wav, 36u + dataBytes);
    wav.push_back('W');
    wav.push_back('A');
    wav.push_back('V');
    wav.push_back('E');
    wav.push_back('f');
    wav.push_back('m');
    wav.push_back('t');
    wav.push_back(' ');
    AppendU32LE(wav, 16u);
    AppendU16LE(wav, 1u);
    AppendU16LE(wav, static_cast<uint16_t>(channels));
    AppendU32LE(wav, static_cast<uint32_t>(sampleRate));
    AppendU32LE(wav, static_cast<uint32_t>(sampleRate * channels * (bitsPerSample / 8)));
    AppendU16LE(wav, static_cast<uint16_t>(channels * (bitsPerSample / 8)));
    AppendU16LE(wav, static_cast<uint16_t>(bitsPerSample));
    wav.push_back('d');
    wav.push_back('a');
    wav.push_back('t');
    wav.push_back('a');
    AppendU32LE(wav, dataBytes);
    const auto* p = reinterpret_cast<const uint8_t*>(pcm.data());
    wav.insert(wav.end(), p, p + dataBytes);
    return wav;
}

bool WriteShowcaseWav(const std::filesystem::path& path, std::string& errOut) {
    errOut.clear();
    std::error_code ec;
    if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            errOut = "Could not create WAV output folder: " + ec.message();
            return false;
        }
    }
    const std::vector<uint8_t> bytes = BuildShowcaseWavBytes();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        errOut = "Could not open WAV output path: " + path.string();
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        errOut = "Failed to write WAV bytes: " + path.string();
        return false;
    }
    return true;
}

bool WriteShowcaseMidi(const std::filesystem::path& path, std::string& errOut) {
    errOut.clear();
    std::error_code ec;
    if (const std::filesystem::path parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            errOut = "Could not create MIDI output folder: " + ec.message();
            return false;
        }
    }
    const std::vector<uint8_t> bytes = BuildShowcaseMidiBytes();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        errOut = "Could not open MIDI output path: " + path.string();
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        errOut = "Failed to write MIDI bytes: " + path.string();
        return false;
    }
    return true;
}

void BootstrapShowcaseScene(std::unique_ptr<Solstice::Parallax::ParallaxScene>& scenePtr, Smm::Editing::ParticleEditorState& particleState,
    Smm::Authoring::SessionState& authoring, Solstice::Parallax::DevSessionAssetResolver& resolver,
    const std::filesystem::path& projectPath, std::string& statusLine) {
    scenePtr = Solstice::Parallax::CreateScene(6000);
    if (!scenePtr) {
        statusLine = "Showcase initialization failed: could not allocate scene.";
        return;
    }
    Solstice::Parallax::ParallaxScene& scene = *scenePtr;
    const uint64_t tps = scene.GetTicksPerSecond();
    const uint64_t s1 = tps;
    scene.SetTimelineDurationTicks(16ull * s1);

    const Solstice::Parallax::ElementIndex cam =
        Solstice::Parallax::AddElement(scene, "CameraElement", "Showcase Camera", 0);
    const Solstice::Parallax::ElementIndex beamLight =
        Solstice::Parallax::AddElement(scene, "LightElement", "Golden Beam Light", 0);
    const Solstice::Parallax::ElementIndex fillLight =
        Solstice::Parallax::AddElement(scene, "LightElement", "Shard Fill Light", 0);
    const Solstice::Parallax::ElementIndex pyramid =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Summoned Pyramid", 0);
    const Solstice::Parallax::ElementIndex fragA =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Fragment A", 0);
    const Solstice::Parallax::ElementIndex fragB =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Fragment B", 0);
    const Solstice::Parallax::ElementIndex fragC =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Fragment C", 0);
    const Solstice::Parallax::ElementIndex fragD =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Fragment D", 0);
    const Solstice::Parallax::ElementIndex cube =
        Solstice::Parallax::AddElement(scene, "ActorElement", "Final Black Cube", 0);
    const Solstice::Parallax::ElementIndex audio =
        Solstice::Parallax::AddElement(scene, "AudioSourceElement", "Showcase Melody", 0);

    Solstice::Parallax::SetAttribute(scene, cam, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 2.6f, 9.f}});
    Solstice::Parallax::SetAttribute(scene, cam, "Target", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 1.3f, 0.f}});
    Solstice::Parallax::SetAttribute(scene, cam, "FovDegrees", Solstice::Parallax::AttributeValue{48.f});
    Solstice::Parallax::SetAttribute(scene, cam, "Near", Solstice::Parallax::AttributeValue{0.1f});
    Solstice::Parallax::SetAttribute(scene, cam, "Far", Solstice::Parallax::AttributeValue{3000.f});

    Solstice::Parallax::SetAttribute(scene, beamLight, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 6.4f, 0.f}});
    Solstice::Parallax::SetAttribute(scene, beamLight, "Color", Solstice::Parallax::AttributeValue{Solstice::Math::Vec4{1.f, 0.86f, 0.24f, 1.f}});
    Solstice::Parallax::SetAttribute(scene, beamLight, "Radius", Solstice::Parallax::AttributeValue{24.f});
    Solstice::Parallax::SetAttribute(scene, beamLight, "CastShadows", Solstice::Parallax::AttributeValue{true});
    Solstice::Parallax::SetAttribute(scene, beamLight, "Intensity", Solstice::Parallax::AttributeValue{0.f});

    Solstice::Parallax::SetAttribute(scene, fillLight, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 2.2f, -2.5f}});
    Solstice::Parallax::SetAttribute(scene, fillLight, "Color", Solstice::Parallax::AttributeValue{Solstice::Math::Vec4{1.f, 0.45f, 0.1f, 1.f}});
    Solstice::Parallax::SetAttribute(scene, fillLight, "Radius", Solstice::Parallax::AttributeValue{18.f});
    Solstice::Parallax::SetAttribute(scene, fillLight, "Intensity", Solstice::Parallax::AttributeValue{0.f});

    Solstice::Parallax::SetAttribute(scene, pyramid, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, -7.f, 0.f}});
    Solstice::Parallax::SetAttribute(scene, cube, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, -8.f, 0.f}});
    Solstice::Parallax::SetAttribute(scene, audio, "Position", Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 1.f, 0.f}});
    Solstice::Parallax::SetAttribute(scene, audio, "Volume", Solstice::Parallax::AttributeValue{0.9f});
    Solstice::Parallax::SetAttribute(scene, audio, "Pitch", Solstice::Parallax::AttributeValue{1.f});

    AddVec3Keys(scene, cam, "Position", {{0ull, {0.f, 2.6f, 9.f}}, {8ull * s1, {0.f, 2.1f, 6.9f}}, {16ull * s1, {0.f, 2.0f, 5.8f}}});
    AddVec3Keys(scene, cam, "Target", {{0ull, {0.f, 1.2f, 0.f}}, {8ull * s1, {0.f, 1.0f, 0.f}}, {16ull * s1, {0.f, 0.8f, 0.f}}});

    AddFloatKeys(scene, beamLight, "Intensity",
        {{0ull, 0.f}, {2ull * s1, 18.f}, {8ull * s1, 16.f}, {10ull * s1, 32.f}, {12ull * s1, 8.f}, {16ull * s1, 4.f}});
    AddFloatKeys(scene, fillLight, "Intensity", {{0ull, 0.f}, {9ull * s1, 3.f}, {10ull * s1, 14.f}, {12ull * s1, 2.f}, {16ull * s1, 1.f}});

    // Summon from beam, hold as a pyramid, drop, then disappear on explosion.
    AddVec3Keys(scene, pyramid, "Position",
        {{0ull, {0.f, -7.f, 0.f}}, {2ull * s1, {0.f, -7.f, 0.f}}, {6ull * s1, {0.f, 2.6f, 0.f}}, {8ull * s1, {0.f, 3.2f, 0.f}},
            {9ull * s1, {0.f, 0.7f, 0.f}}, {10ull * s1, {0.f, -7.f, 0.f}}});

    // Explosion shards: emerge at impact, spread, then reassemble.
    AddVec3Keys(scene, fragA, "Position",
        {{0ull, {0.f, -8.f, 0.f}}, {9ull * s1, {0.1f, 0.8f, 0.1f}}, {11ull * s1, {-2.4f, 2.8f, 1.7f}}, {14ull * s1, {0.f, 0.7f, 0.f}},
            {16ull * s1, {0.f, 0.7f, 0.f}}});
    AddVec3Keys(scene, fragB, "Position",
        {{0ull, {0.f, -8.f, 0.f}}, {9ull * s1, {-0.1f, 0.8f, -0.1f}}, {11ull * s1, {2.3f, 3.0f, -1.4f}},
            {14ull * s1, {0.f, 0.7f, 0.f}}, {16ull * s1, {0.f, 0.7f, 0.f}}});
    AddVec3Keys(scene, fragC, "Position",
        {{0ull, {0.f, -8.f, 0.f}}, {9ull * s1, {0.f, 0.9f, 0.f}}, {11ull * s1, {1.5f, 2.5f, 2.2f}}, {14ull * s1, {0.f, 0.7f, 0.f}},
            {16ull * s1, {0.f, 0.7f, 0.f}}});
    AddVec3Keys(scene, fragD, "Position",
        {{0ull, {0.f, -8.f, 0.f}}, {9ull * s1, {0.f, 0.9f, 0.f}}, {11ull * s1, {-1.8f, 2.2f, -2.0f}}, {14ull * s1, {0.f, 0.7f, 0.f}},
            {16ull * s1, {0.f, 0.7f, 0.f}}});

    AddVec3Keys(scene, cube, "Position",
        {{0ull, {0.f, -8.f, 0.f}}, {13ull * s1, {0.f, -8.f, 0.f}}, {14ull * s1, {0.f, 0.7f, 0.f}}, {16ull * s1, {0.f, 0.7f, 0.f}}});

    const Solstice::Parallax::MGIndex mgRoot =
        Solstice::Parallax::AddMGElement(scene, "MotionGraphicsRootElement", "Showcase MG Root", Solstice::Parallax::PARALLAX_INVALID_INDEX);
    if (mgRoot != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        auto& mg = scene.GetMGElements()[mgRoot];
        mg.Attributes["CompositeAlpha"] = Solstice::Parallax::AttributeValue{1.0f};
        mg.Attributes["ChromaticAberration"] = Solstice::Parallax::AttributeValue{0.003f};
        mg.Attributes["GradeExposure"] = Solstice::Parallax::AttributeValue{0.92f};
        mg.Attributes["GradeSaturation"] = Solstice::Parallax::AttributeValue{1.1f};
        mg.Attributes["GradeContrast"] = Solstice::Parallax::AttributeValue{1.08f};
    }

    const Solstice::Parallax::MGIndex mgCaption = Solstice::Parallax::AddMGElement(
        scene, "MGTextElement", "Showcase Caption", Solstice::Parallax::PARALLAX_INVALID_INDEX);
    if (mgCaption != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        auto& mg = scene.GetMGElements()[mgCaption];
        mg.Attributes["Text"] = Solstice::Parallax::AttributeValue{
            std::string("Golden beam summons a pyramid. Impact shatters to shards. Shards reassemble into a black cube.")};
        mg.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{42.f, 42.f}};
        mg.Attributes["Color"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec4{1.f, 0.93f, 0.72f, 0.95f}};
        mg.Attributes["Depth"] = Solstice::Parallax::AttributeValue{5.f};
        AddMgFloatKeys(scene, mgCaption, "Depth", {{0ull, 5.f}, {10ull * s1, 8.f}, {16ull * s1, 5.f}});
    }

    const Solstice::Parallax::MGIndex mgAura =
        Solstice::Parallax::AddMGElement(scene, "MGSpriteElement", "Beam Aura Card", Solstice::Parallax::PARALLAX_INVALID_INDEX);
    if (mgAura != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        auto& mg = scene.GetMGElements()[mgAura];
        mg.Attributes["MGProjectionMode"] = Solstice::Parallax::AttributeValue{int32_t{1}};
        mg.Attributes["WorldPosition"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 3.0f, 0.f}};
        mg.Attributes["WorldScale"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.8f, 6.2f, 0.8f}};
        mg.Attributes["Color"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec4{1.f, 0.83f, 0.2f, 0.7f}};
        mg.Attributes["Depth"] = Solstice::Parallax::AttributeValue{-2.f};
        AddMgVec4Keys(scene, mgAura, "Color",
            {{0ull, {1.f, 0.80f, 0.18f, 0.0f}}, {2ull * s1, {1.f, 0.83f, 0.2f, 0.85f}}, {12ull * s1, {0.1f, 0.1f, 0.1f, 0.75f}},
                {16ull * s1, {0.05f, 0.05f, 0.05f, 0.85f}}});
    }

    particleState = {};
    particleState.enabled = true;
    particleState.attachToSceneElement = true;
    particleState.attachElementIndex = static_cast<int>(beamLight);
    particleState.spawnPerSec = 220.f;
    particleState.lifetimeSec = 2.1f;
    particleState.velMin = Solstice::Math::Vec3{-0.9f, 0.5f, -0.9f};
    particleState.velMax = Solstice::Math::Vec3{0.9f, 2.3f, 0.9f};
    particleState.startSize = 0.14f;
    particleState.endSize = 0.02f;
    particleState.gravity = Solstice::Math::Vec3{0.f, -0.65f, 0.f};
    particleState.linearDrag = 0.16f;
    particleState.maxParticles = 7000;
    particleState.useColorGradient = true;
    particleState.gradStopsInited = true;
    particleState.gradientStops = 4;
    particleState.gradT[0] = 0.f;
    particleState.gradT[1] = 0.22f;
    particleState.gradT[2] = 0.68f;
    particleState.gradT[3] = 1.f;
    particleState.gradRgba[0][0] = 1.f;
    particleState.gradRgba[0][1] = 0.95f;
    particleState.gradRgba[0][2] = 0.65f;
    particleState.gradRgba[0][3] = 1.f;
    particleState.gradRgba[1][0] = 1.f;
    particleState.gradRgba[1][1] = 0.76f;
    particleState.gradRgba[1][2] = 0.2f;
    particleState.gradRgba[1][3] = 0.95f;
    particleState.gradRgba[2][0] = 0.4f;
    particleState.gradRgba[2][1] = 0.25f;
    particleState.gradRgba[2][2] = 0.1f;
    particleState.gradRgba[2][3] = 0.35f;
    particleState.gradRgba[3][0] = 0.06f;
    particleState.gradRgba[3][1] = 0.06f;
    particleState.gradRgba[3][2] = 0.06f;
    particleState.gradRgba[3][3] = 0.f;
    particleState.ribbonTrails = true;
    particleState.ribbonSegments = 10;
    particleState.burstPending = 240;

    std::string particleSyncErr;
    (void)Smm::Particles::SyncEditorToParallaxScene(scene, particleState, resolver, particleSyncErr);

    authoring.Prefabs.clear();
    authoring.AssetDb.clear();
    authoring.LipsyncStubs.clear();
    authoring.CinematicView.ChromaticAberrationStrength = 0.0042f;
    authoring.CinematicView.ChromaticAberrationDepthScale = 1.18f;
    authoring.CinematicView.ChromaticCenterU = 0.50f;
    authoring.CinematicView.ChromaticCenterV = 0.48f;
    authoring.CinematicView.SmearFrameStrength = 0.072f;
    authoring.CinematicView.ScreenFogDither = 0.12f;

    authoring.Prefabs.push_back(
        {"showcase_camera", "Showcase Camera", "CameraElement", "FovDegrees=48;Near=0.1;Far=3000;Position=0,2.6,9;Target=0,1.2,0"});
    authoring.Prefabs.push_back({"showcase_beam", "Golden Beam Light", "LightElement",
        "Position=0,6.4,0;Color=1,0.86,0.24,1;Intensity=18;Radius=24;CastShadows=1"});
    authoring.Prefabs.push_back({"showcase_cube", "Final Black Cube", "ActorElement",
        "Position=0,0.7,0;ArzachelRigidBodyDamage=0;ArzachelDestructionAnimPreset=ReassembleFromShards"});

    Smm::Authoring::LipsyncLineStub callout;
    callout.Label = "showcase_callout";
    callout.StartTick = 0;
    callout.EndTick = 5ull * s1;
    callout.Text = "Golden beam summon sequence online.";
    callout.Strength = 1.0f;
    callout.PhoneticMode = 2;
    authoring.LipsyncStubs.push_back(std::move(callout));

    std::string midiErr;
    std::string wavErr;
    try {
        const std::filesystem::path projectDir =
            projectPath.parent_path().empty() ? std::filesystem::current_path() : projectPath.parent_path();
        const std::filesystem::path wavPath = projectDir / "smm_showcase_melody.wav";
        if (WriteShowcaseWav(wavPath, wavErr)) {
            if (const auto audioHash = resolver.ImportFile(wavPath)) {
                Solstice::Parallax::SetAttribute(scene, audio, "AudioAsset", Solstice::Parallax::AttributeValue{*audioHash});
                Smm::Authoring::AssetDbEntry wa;
                wa.Hash = *audioHash;
                wa.PathHint = wavPath.filename().string();
                wa.Tags = "music,showcase,wav";
                wa.Kind = "audio";
                wa.Notes = "Auto-generated showcase melody waveform.";
                authoring.AssetDb.push_back(std::move(wa));
            }
        }
    } catch (const std::exception& ex) {
        wavErr = ex.what();
    } catch (...) {
        wavErr = "unknown filesystem error while creating showcase WAV";
    }
    bool midiOk = false;
    try {
        const std::filesystem::path projectDir =
            projectPath.parent_path().empty() ? std::filesystem::current_path() : projectPath.parent_path();
        const std::filesystem::path midiPath = projectDir / "smm_showcase_melody.mid";
        if (WriteShowcaseMidi(midiPath, midiErr)) {
            Smm::Midi::ParseResult midiParsed{};
            if (Smm::Midi::ParseStandardMidiFile(midiPath, midiParsed, midiErr)) {
                authoring.MidiConductor.SourcePathUtf8 = midiPath.string();
                authoring.MidiConductor.TicksPerQuarter = midiParsed.TicksPerQuarter;
                authoring.MidiConductor.MicrosecondsPerQuarter = midiParsed.MicrosecondsPerQuarter;
                authoring.MidiConductor.BeatTicks = midiParsed.BeatTicks;
                Smm::Authoring::AssetDbEntry midiAsset;
                midiAsset.Hash = 0;
                midiAsset.PathHint = midiPath.filename().string();
                midiAsset.Tags = "music,showcase,midi";
                midiAsset.Kind = "midi";
                midiAsset.Notes = "Auto-generated SMM showcase melody (path reference).";
                authoring.AssetDb.push_back(std::move(midiAsset));
                midiOk = true;
            }
        }
    } catch (const std::exception& ex) {
        midiErr = ex.what();
    } catch (...) {
        midiErr = "unknown filesystem error while creating showcase MIDI";
    }
    if (!midiOk) {
        // Fallback beat grid so conductor features are still demonstrated without filesystem dependency.
        authoring.MidiConductor.SourcePathUtf8 = "(generated fallback)";
        authoring.MidiConductor.TicksPerQuarter = 480u;
        authoring.MidiConductor.MicrosecondsPerQuarter = 500000u;
        authoring.MidiConductor.BeatTicks.clear();
        for (uint32_t bt = 0; bt <= 480u * 8u; bt += 480u) {
            authoring.MidiConductor.BeatTicks.push_back(bt);
        }
    }
    if (!particleSyncErr.empty()) {
        statusLine = "Showcase initialized with particle warning: " + particleSyncErr;
    } else if (!wavErr.empty()) {
        statusLine = "Showcase initialized with audio warning: " + wavErr;
    } else if (!midiErr.empty()) {
        statusLine = "Showcase initialized with MIDI warning: " + midiErr;
    } else {
        statusLine = "Loaded SMM showcase: beam summon, drop/explosion/reassembly, particles, and generated MIDI melody.";
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
    /// Background `.prlx` recovery snapshot interval (seconds) while the scene is dirty.
    uint32_t recoveryIntervalSec = 60;
    /// 0 = pure 2D MG workflow, 1 = unified world MG workflow.
    uint32_t mgWorkflowMode = 0;
    /// In pure 2D MG workflow, disable 3D background capture when true.
    bool pure2DDisable3DBackground = true;
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
            << ",\"recoveryIntervalSec\":" << st.recoveryIntervalSec
            << ",\"mgWorkflowMode\":" << st.mgWorkflowMode
            << ",\"pure2DDisable3DBackground\":" << (st.pure2DDisable3DBackground ? "true" : "false")
            << ",\"recent\":[";
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
    uint32_t mgMode = st.mgWorkflowMode;
    if (ParseJsonKeyU32(j, "mgWorkflowMode", mgMode)) {
        st.mgWorkflowMode = (std::min)(mgMode, 1u);
    }
    bool pure2DNo3D = st.pure2DDisable3DBackground;
    if (ParseJsonKeyBool(j, "pure2DDisable3DBackground", pure2DNo3D)) {
        st.pure2DDisable3DBackground = pure2DNo3D;
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
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, uint32_t& recoveryIntervalSecOut,
    uint32_t& mgWorkflowModeOut, bool& pure2DDisable3DBackgroundOut) {
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
    mgWorkflowModeOut = (std::min)(st.mgWorkflowMode, 1u);
    pure2DDisable3DBackgroundOut = st.pure2DDisable3DBackground;
}

/// Returns true when a pending MovieMaker project JSON was applied (buffers `mgWorkflowModeOut` / `pure2DDisable3DBackgroundOut`
/// are only updated in that case).
static bool DrainPendingMovieMakerProject(char* exportPathBuf, size_t exportPathBufSize, char* importPathBuf,
    size_t importPathBufSize, char* folderPathBuf, size_t folderPathBufSize, char* ffmpegExeBuf, size_t ffmpegExeBufSize,
    char* videoExportPathBuf, size_t videoExportPathBufSize, uint32_t& videoW, uint32_t& videoH, uint32_t& videoFps,
    bool& videoMp4, uint64_t& videoStartTick, uint64_t& videoEndTick, bool& compressPrlx,
    std::vector<std::string>& recentPrlxPaths, std::filesystem::path& activeProjectPath, uint32_t& recoveryIntervalSecOut,
    uint32_t& mgWorkflowModeOut, bool& pure2DDisable3DBackgroundOut) {
    std::optional<MovieMakerProjectState> pending;
    {
        std::lock_guard<std::mutex> lock(g_ProjectMutex);
        pending = std::move(g_PendingProjectApply);
    }
    if (pending) {
        ApplyMovieMakerProjectState(*pending, exportPathBuf, exportPathBufSize, importPathBuf, importPathBufSize,
            folderPathBuf, folderPathBufSize, ffmpegExeBuf, ffmpegExeBufSize, videoExportPathBuf, videoExportPathBufSize,
            videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick, compressPrlx, recentPrlxPaths,
            activeProjectPath, recoveryIntervalSecOut, mgWorkflowModeOut, pure2DDisable3DBackgroundOut);
        return true;
    }
    return false;
}

static std::string SmmBuildVideoExportFailureReport(const Solstice::MovieMaker::VideoExportParams& p, const std::string& errMsg,
    const char* exceptionWhat) {
    const std::time_t now = std::time(nullptr);
    char tbuf[72] = {};
    std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::ostringstream o;
    o << "SMM video export — failure report\n";
    o << "Time (local): " << tbuf << "\n";
    o << "Resolution: " << p.width << " x " << p.height << " @ " << p.fps << " fps\n";
    o << "Ticks: start=" << p.startTick << " end=" << p.endTick << " (0 end = scene duration)\n";
    o << "Output: " << p.outputPath << "\n";
    o << "ffmpeg: " << p.ffmpegExecutable << "\n";
    o << "Container: " << (p.container == Solstice::MovieMaker::VideoContainer::Mp4 ? "mp4" : "mov") << "\n";
    o << "---\n"
      << errMsg << "\n";
    if (exceptionWhat && exceptionWhat[0] != '\0') {
        o << "std::exception::what(): " << exceptionWhat << "\n";
    }
    return o.str();
}

static bool SmmRunVideoExportTry(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    SDL_Window* window, const Solstice::MovieMaker::VideoExportParams& p, std::string& errOut, std::string& detailOut,
    const std::function<void(float)>& progress) {
    errOut.clear();
    detailOut.clear();
    try {
        const bool ok = Solstice::MovieMaker::ExportParallaxSceneToVideo(scene, resolver, window, p, errOut, progress);
        if (!ok) {
            detailOut = SmmBuildVideoExportFailureReport(p, errOut, nullptr);
        }
        return ok;
    } catch (const std::exception& e) {
        errOut = std::string("Unhandled C++ exception in video export: ") + e.what();
        detailOut = SmmBuildVideoExportFailureReport(p, errOut, e.what());
        return false;
    } catch (...) {
        errOut = "Unhandled non-standard C++ exception in video export.";
        detailOut = SmmBuildVideoExportFailureReport(p, errOut, "(non-standard exception)");
        return false;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    SmmInstallCrashDiagnostics();
    SmmSetCrashStage("main.begin");

    SmmSetCrashStage("sdl.init");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SmmSetCrashStage("window.create");
    LibUI::Shell::GlWindow glWindow;
    if (!LibUI::Shell::CreateUtilityGlWindow(
            glWindow, "Solstice Movie Maker — Technology Preview 1", 1280, 720, SDL_WINDOW_RESIZABLE, 1)) {
        std::cerr << "CreateUtilityGlWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_Window* window = glWindow.window;

    SmmSetCrashStage("libui.init");
    if (!LibUI::Core::Initialize(window)) {
        std::cerr << "LibUI::Core::Initialize failed" << std::endl;
        LibUI::Shell::DestroyUtilityGlWindow(glWindow);
        SDL_Quit();
        return 1;
    }
    SmmSetCrashStage("audio.init");
    if (!Solstice::EditorAudio::Init()) {
        LibUI::Tools::DiagLogLine("[SMM] EditorAudio::Init failed (waveform / scrub preview will be disabled).");
    }
    LibUI::Tools::DiagLogLine("[SMM] LibUI initialized.");

#if defined(_WIN32)
    const bool loadMovieMakerPlugins = !LibUI::Tools::EnvVarTruthy("SOLSTICE_SMM_DISABLE_PLUGINS");
#else
    const bool loadMovieMakerPlugins = true;
#endif
    SmmSetCrashStage("plugins.load");
    if (loadMovieMakerPlugins) {
        LoadMovieMakerPlugins();
        LibUI::Tools::DiagLogLine("[SMM] Plugins loaded.");
    } else {
        LibUI::Tools::DiagLogLine("[SMM] Plugins skipped (SOLSTICE_SMM_DISABLE_PLUGINS=1).");
    }

    Solstice::Parallax::DevSessionAssetResolver resolver;
    std::vector<LibUI::AssetBrowser::Entry> assetEntries;
    SmmSetCrashStage("scene.create");
    auto scene = Solstice::Parallax::CreateScene(6000);
    Solstice::MovieMaker::UI::Panels::ResetUnifiedViewportEnginePreviewWarmup();
    LibUI::Tools::DiagLogLine("[SMM] Scene created.");
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Post-scene init begin.");
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
    std::string videoExportLastDetail;
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
    uint32_t smmWorkflowModeU32 = 0;
    bool smmPure2DDisable3DBackground = true;
    std::vector<Solstice::MovieMaker::VideoExportParams> smmVideoRenderQueue;
    std::optional<std::string> pendingVideoImportPath;
    std::optional<Solstice::MovieMaker::VideoExportParams> pendingVideoExportStart;
    bool pendingVideoRenderQueueRun = false;
    Solstice::MovieMaker::IncrementalVideoExportSession* activeVideoExportSession = nullptr;
    Solstice::MovieMaker::VideoExportParams activeVideoExportJob{};
    bool activeVideoExportIsQueue = false;
    size_t activeVideoExportQueueIndex = 0;

    {
        MovieMakerProjectState boot;
        if (LoadMovieMakerProjectFromPath(activeProjectPath, boot)) {
            ApplyMovieMakerProjectState(boot, exportPathBuf, sizeof(exportPathBuf), importPathBuf, sizeof(importPathBuf),
                folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf), videoExportPathBuf,
                sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick,
                compressPrlx, recentPrlxPaths, activeProjectPath, smmRecoveryIntervalSecU32, smmWorkflowModeU32,
                smmPure2DDisable3DBackground);
        }
    }
    smmWorkspace.unifiedMgWorkflowMode
        = (smmWorkflowModeU32 == 1u) ? Smm::UI::UnifiedMgWorkflowMode::Unified3D : Smm::UI::UnifiedMgWorkflowMode::Pure2D;
    smmWorkspace.pure2DDisable3DBackground = smmPure2DDisable3DBackground;
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Boot project state loaded.");

    Smm::Authoring::SessionState smmAuthoringSession;
    {
        std::string authErr;
        (void)Smm::Authoring::LoadSessionAuthoring(Smm::Authoring::AuthoringSidecarPathForProject(activeProjectPath), smmAuthoringSession, &authErr);
    }
    const bool hasAuthoringData = !smmAuthoringSession.AssetDb.empty() || !smmAuthoringSession.Prefabs.empty()
        || !smmAuthoringSession.LipsyncStubs.empty() || !smmAuthoringSession.MidiConductor.SourcePathUtf8.empty()
        || !smmAuthoringSession.MidiConductor.BeatTicks.empty();
    if (!hasAuthoringData) {
        BootstrapShowcaseScene(scene, smmWorkspace.particleEditorState, smmAuthoringSession, resolver, activeProjectPath, smmStatus);
    } else {
        Smm::Particles::LoadParticleEditorFromScene(*scene, smmWorkspace.particleEditorState);
    }
    ReloadSmmWorkspaceIniPresets(activeProjectPath);
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Authoring session + presets loaded.");

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
        pst.mgWorkflowMode = (smmWorkspace.unifiedMgWorkflowMode == Smm::UI::UnifiedMgWorkflowMode::Unified3D) ? 1u : 0u;
        pst.pure2DDisable3DBackground = smmWorkspace.pure2DDisable3DBackground;
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
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda persistMovieMakerProjectFields ready.");

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
            PrepareSceneEmbeddedAssets(*scene, resolver);
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
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda requestSaveMovieMakerProject ready.");

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
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda requestOpenMovieMakerProject ready.");

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
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda requestExportParallaxScene ready.");

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
        smmWorkspace.viewportSelectedElements.clear();
        if (elementSelected >= 0) {
            smmWorkspace.viewportSelectedElements.insert(elementSelected);
        }
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
        return true;
    };
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda performPrlxImportFromBuffers ready.");

    auto commitNewParallaxScene = [&]() {
        Smm::ClearSceneUndo();
        Smm::Editing::ResetParticleEditUndo();
        BootstrapShowcaseScene(scene, smmWorkspace.particleEditorState, smmAuthoringSession, resolver, activeProjectPath, smmStatus);
        Solstice::MovieMaker::UI::Panels::ResetUnifiedViewportEnginePreviewWarmup();
        smmWorkspace.enginePreviewSessionDisabled = false;
        timeTicks = 0;
        elementSelected = scene->GetElements().empty() ? -1 : 0;
        mgElementSelected = scene->GetMGElements().empty() ? -1 : 0;
        smmWorkspace.viewportSelectedElements.clear();
        if (elementSelected >= 0) {
            smmWorkspace.viewportSelectedElements.insert(elementSelected);
        }
        sceneDirty = true;
        ffmpegLog = "New SMM showcase scene.\n" + ffmpegLog;
    };

    auto requestNewParallaxScene = [&]() {
        if (sceneDirty) {
            mmUnsavedPrompt = MmUnsavedKind::NewScene;
        } else {
            commitNewParallaxScene();
        }
    };
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Lambda requestNewParallaxScene ready.");

    std::string smmSdlBasePathUtf8;
    {
        const char* sdlB = SDL_GetBasePath();
        if (sdlB) {
            smmSdlBasePathUtf8 = sdlB;
#if !defined(_WIN32)
            SDL_free((void*)sdlB);
#endif
        }
    }
#if defined(_WIN32)
    const bool smmEnableRecovery = !LibUI::Tools::EnvVarTruthy("SOLSTICE_SMM_DISABLE_RECOVERY");
#else
    const bool smmEnableRecovery = true;
#endif
    LibUI::Tools::DiagLogLine(std::string("[SMM][TRACE] Recovery mode: ") + (smmEnableRecovery ? "enabled" : "disabled"));
    const std::filesystem::path smmRecoveryDir = smmEnableRecovery
        ? Solstice::EditorAudio::FileRecovery::RecoveryDir(smmSdlBasePathUtf8.empty() ? nullptr : smmSdlBasePathUtf8.c_str(), "smm")
        : std::filesystem::path{};
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Recovery dir resolved.");
    bool smmPrlxRecoveryOpen
        = smmEnableRecovery && !Solstice::EditorAudio::FileRecovery::List(smmRecoveryDir, "prlx").empty();
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Recovery list scanned.");
    double smmRecoveryAccumSec = 0.0;

    auto tryWritePrlxRecoverySnapshot = [&]() {
        if (!smmEnableRecovery) {
            return;
        }
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
        smmWorkspace.viewportSelectedElements.clear();
        if (elementSelected >= 0) {
            smmWorkspace.viewportSelectedElements.insert(elementSelected);
        }
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
        sceneDirty = true;
        smmStatus = "Restored Parallax scene from recovery autosave.";
        return true;
    };

    bool running = true;
    LibUI::Tools::DiagLogLine("[SMM][TRACE] Entering main loop.");
    while (running) {
        ++g_smmCrashFrame;
        SmmSetCrashStage("frame.begin");
        static bool firstFrameTrace = true;
        if (firstFrameTrace) {
            LibUI::Tools::DiagLogLine("[SMM] Entering first frame.");
            firstFrameTrace = false;
        }
        SmmSetCrashStage("frame.poll_events");
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

        SmmSetCrashStage("frame.drain_pending_project");
        const bool smmPendingProjectApplied = DrainPendingMovieMakerProject(exportPathBuf, sizeof(exportPathBuf),
            importPathBuf, sizeof(importPathBuf), folderPathBuf, sizeof(folderPathBuf), ffmpegExeBuf, sizeof(ffmpegExeBuf),
            videoExportPathBuf, sizeof(videoExportPathBuf), videoW, videoH, videoFps, videoMp4, videoStartTick, videoEndTick,
            compressPrlx, recentPrlxPaths, activeProjectPath, smmRecoveryIntervalSecU32, smmWorkflowModeU32,
            smmPure2DDisable3DBackground);
        if (smmPendingProjectApplied) {
            smmWorkspace.unifiedMgWorkflowMode = (smmWorkflowModeU32 == 1u) ? Smm::UI::UnifiedMgWorkflowMode::Unified3D
                                                                           : Smm::UI::UnifiedMgWorkflowMode::Pure2D;
            smmWorkspace.pure2DDisable3DBackground = smmPure2DDisable3DBackground;
        }
        static std::filesystem::path s_smmAuthProject;
        if (s_smmAuthProject != activeProjectPath) {
            s_smmAuthProject = activeProjectPath;
            (void)Smm::Authoring::LoadSessionAuthoring(
                Smm::Authoring::AuthoringSidecarPathForProject(activeProjectPath), smmAuthoringSession, nullptr);
            ReloadSmmWorkspaceIniPresets(activeProjectPath);
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
                PrepareSceneEmbeddedAssets(*scene, resolver);
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

        auto tryStartVideoExport = [&](const Solstice::MovieMaker::VideoExportParams& vep, bool fromQueue) {
            std::string particleSceneSync{};
            (void)Smm::Particles::SyncEditorToParallaxScene(*scene, smmWorkspace.particleEditorState, resolver, particleSceneSync);
            Solstice::MovieMaker::VideoExportParticleSettings xps{};
            xps.viewportOrbitForMatch = &smmWorkspace.unifiedViewportCamera;
            if (smmWorkspace.particleEditorState.enabled) {
                xps.particleEditor = &smmWorkspace.particleEditorState;
                xps.emitterWorldManual = smmWorkspace.manualParticleEmitterWorld;
                xps.emitterWorld = smmWorkspace.manualParticleEmitterWorldVec;
            }
            std::string beginErr;
            if (!Solstice::MovieMaker::BeginParallaxSceneVideoExport(
                    *scene, resolver, window, vep, activeVideoExportSession, beginErr, &xps)) {
                videoExportLastDetail = SmmBuildVideoExportFailureReport(vep, beginErr, nullptr);
                videoExportLog = "Failed to start export job.\n\n" + beginErr + "\n\n---\n" + videoExportLastDetail;
                return false;
            }
            activeVideoExportIsQueue = fromQueue;
            activeVideoExportJob = vep;
            std::string lastCmdDiag
                = "ffmpeg: \"" + vep.ffmpegExecutable + "\" (rawvideo pipe -> " + vep.outputPath + ")";
            videoExportLog = "Started export job.\n" + lastCmdDiag + "\n";
            videoExportLastDetail.clear();
            return true;
        };

        if (!activeVideoExportSession && pendingVideoExportStart) {
            const Solstice::MovieMaker::VideoExportParams vep = *pendingVideoExportStart;
            pendingVideoExportStart.reset();
            (void)tryStartVideoExport(vep, false);
        }
        if (!activeVideoExportSession && pendingVideoRenderQueueRun) {
            pendingVideoRenderQueueRun = false;
            activeVideoExportQueueIndex = 0;
            if (smmVideoRenderQueue.empty()) {
                videoExportLog = "Render queue is empty.\n";
            } else {
                (void)tryStartVideoExport(smmVideoRenderQueue[0], true);
            }
        }
        if (activeVideoExportSession) {
            float pr = 0.f;
            bool done = false;
            std::string stepErr;
            const bool ok = Solstice::MovieMaker::StepParallaxSceneVideoExport(
                *activeVideoExportSession, *scene, resolver, window, pr, done, stepErr);
            if (!ok) {
                videoExportLastDetail = SmmBuildVideoExportFailureReport(activeVideoExportJob, stepErr, nullptr);
                videoExportLog = "Export failed.\n\n" + stepErr + "\n\n---\n" + videoExportLastDetail;
                Solstice::MovieMaker::CancelParallaxSceneVideoExport(activeVideoExportSession);
                activeVideoExportIsQueue = false;
                smmVideoRenderQueue.clear();
            } else {
                const int p = static_cast<int>(pr * 100.f);
                if (activeVideoExportIsQueue && activeVideoExportQueueIndex < smmVideoRenderQueue.size()) {
                    videoExportLog = "Queue job " + std::to_string(activeVideoExportQueueIndex + 1) + "/"
                        + std::to_string(smmVideoRenderQueue.size()) + ": " + smmVideoRenderQueue[activeVideoExportQueueIndex].outputPath
                        + " - " + std::to_string(p) + "%\n";
                } else {
                    videoExportLog = "Encoding... " + std::to_string(p) + "%\n";
                }
                if (done) {
                    if (!stepErr.empty()) {
                        videoExportLog += "\nExport finished with warning:\n" + stepErr + "\n";
                    }
                    Solstice::MovieMaker::CancelParallaxSceneVideoExport(activeVideoExportSession);
                    if (activeVideoExportIsQueue) {
                        activeVideoExportQueueIndex++;
                        if (activeVideoExportQueueIndex < smmVideoRenderQueue.size()) {
                            (void)tryStartVideoExport(smmVideoRenderQueue[activeVideoExportQueueIndex], true);
                        } else {
                            activeVideoExportIsQueue = false;
                            smmVideoRenderQueue.clear();
                            videoExportLog = "Render queue completed.\n" + videoExportLog;
                        }
                    } else {
                        activeVideoExportIsQueue = false;
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
                        pst.mgWorkflowMode = (smmWorkspace.unifiedMgWorkflowMode == Smm::UI::UnifiedMgWorkflowMode::Unified3D) ? 1u : 0u;
                        pst.pure2DDisable3DBackground = smmWorkspace.pure2DDisable3DBackground;
                        pst.recentPrlx = recentPrlxPaths;
                        (void)SaveMovieMakerProjectToPath(activeProjectPath, pst);
                    }
                }
            }
        }

        SmmSetCrashStage("frame.newframe");
        LibUI::Core::NewFrame();

        ImGuiIO& io_mm = ImGui::GetIO();

        smmRecoveryAccumSec += (double)io_mm.DeltaTime;
        if (sceneDirty && smmRecoveryAccumSec >= static_cast<double>(smmRecoveryIntervalSecU32)) {
            smmRecoveryAccumSec = 0.0;
            tryWritePrlxRecoverySnapshot();
        }

        SmmSetCrashStage("frame.imgui_root");
        ImGuiViewport* vp = ImGui::GetMainViewport();
        LibUI::Widgets::SetNextWindowPos(vp->Pos);
        LibUI::Widgets::SetNextWindowSize(vp->Size);
        LibUI::Widgets::BeginWindow("SMMRoot", nullptr,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        if (LibUI::Widgets::BeginMenuBar()) {
            if (LibUI::Widgets::BeginMenu("File")) {
                if (LibUI::Widgets::MenuItem("New Scene")) {
                    requestNewParallaxScene();
                }
                if (LibUI::Widgets::MenuItem("Save Project", "Ctrl+S")) {
                    requestSaveMovieMakerProject();
                }
                if (LibUI::Widgets::MenuItem("Open Project...")) {
                    requestOpenMovieMakerProject();
                }
                if (LibUI::Widgets::MenuItem("Import glTF to selected Actor...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::QueueGltfImportPath(std::move(*path));
                            }
                        },
                        Smm::kGltfFilters);
                }
                if (LibUI::Widgets::MenuItem("Export selected Actor glTF...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export selected glTF asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::QueueGltfExportPath(std::move(*path));
                            }
                        },
                        Smm::kGltfFilters);
                }
                if (LibUI::Widgets::MenuItem("Import raster to selected MG sprite...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import raster texture", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Image::QueueRasterImportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                }
                if (LibUI::Widgets::MenuItem("Export selected MG sprite texture...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export MG sprite Texture bytes", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Image::QueueRasterExportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterExportFilters));
                }
                if (LibUI::Widgets::MenuItem("Import audio to selected Audio source...")) {
                    LibUI::FileDialogs::ShowOpenFile(
                        window, "Import audio asset", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Audio::QueueAudioImportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioImportFilters));
                }
                if (LibUI::Widgets::MenuItem("Import video to session...")) {
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
                if (LibUI::Widgets::MenuItem("Write recovery snapshot now")) {
                    tryWritePrlxRecoverySnapshot();
                }
                if (LibUI::Widgets::MenuItem("Export selected Audio source...")) {
                    LibUI::FileDialogs::ShowSaveFile(
                        window, "Export AudioAsset bytes", [](std::optional<std::string> path) {
                            if (path) {
                                Smm::Audio::QueueAudioExportPath(std::move(*path));
                            }
                        },
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioExportFilters));
                }
                if (LibUI::Widgets::MenuItem("Export PARALLAX scene...")) {
                    requestExportParallaxScene();
                }
                if (LibUI::Widgets::MenuItem("Export...")) {
                    showExportWindow = true;
                }
                if (!recentPrlxPaths.empty()) {
                    LibUI::Widgets::Separator();
                    LibUI::Widgets::TextDisabled("Recent .prlx");
                    for (size_t ri = 0; ri < recentPrlxPaths.size(); ++ri) {
                        ImGui::PushID(static_cast<int>(ri));
                        if (LibUI::Widgets::MenuItem(recentPrlxPaths[ri].c_str())) {
                            std::snprintf(importPathBuf, sizeof(importPathBuf), "%s", recentPrlxPaths[ri].c_str());
                        }
                        ImGui::PopID();
                    }
                }
                LibUI::Widgets::EndMenu();
            }
            if (LibUI::Widgets::BeginMenu("Edit")) {
                const bool particleFocus = Smm::Editing::IsParticleEditPanelFocused();
                const bool canUndo = particleFocus ? Smm::Editing::CanParticleEditUndo() : Smm::g_sceneByteUndo.CanUndo();
                const bool canRedo = particleFocus ? Smm::Editing::CanParticleEditRedo() : Smm::g_sceneByteUndo.CanRedo();
                if (LibUI::Widgets::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
                    if (particleFocus) {
                        if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, true)) {
                            smmStatus = "Particle: undo.";
                        }
                    } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, true, elementSelected, timeTicks, false, 0, 0)) {
                        smmStatus = "Undo (scene).";
                        sceneDirty = true;
                    }
                }
                if (LibUI::Widgets::MenuItem("Redo", "Ctrl+Y / Ctrl+Shift+Z", false, canRedo)) {
                    if (particleFocus) {
                        if (Smm::Editing::ApplyParticleEditUndo(smmWorkspace.particleEditorState, false)) {
                            smmStatus = "Particle: redo.";
                        }
                    } else if (Smm::SceneUndoRedoApply(*scene, compressPrlx, false, elementSelected, timeTicks, false, 0, 0)) {
                        smmStatus = "Redo (scene).";
                        sceneDirty = true;
                    }
                }
                LibUI::Widgets::EndMenu();
            }
            if (LibUI::Widgets::BeginMenu("View")) {
                LibUI::Widgets::MenuItem("Curve editor", nullptr, &smmWorkspace.showCurveEditorPanel);
                LibUI::Widgets::MenuItem("Graph editor", nullptr, &smmWorkspace.showGraphEditorPanel);
                LibUI::Widgets::MenuItem("Particles", nullptr, &smmWorkspace.showParticleEditorPanel);
                LibUI::Widgets::MenuItem("Fluid volumes", nullptr, &smmWorkspace.showFluidVolumesPanel);
                LibUI::Widgets::Separator();
                if (LibUI::Widgets::MenuItem("Plugins")) {
                    showMmPluginsPanel = true;
                }
                LibUI::Widgets::EndMenu();
            }
            if (LibUI::Widgets::BeginMenu("Help")) {
                if (LibUI::Widgets::MenuItem("About")) {
                    showMmAboutPanel = true;
                }
                LibUI::Widgets::EndMenu();
            }
            LibUI::Widgets::EndMenuBar();
        }

        if (mmUnsavedPrompt != MmUnsavedKind::None) {
            LibUI::Widgets::OpenPopup("MM_Unsaved");
        }
        if (LibUI::Widgets::BeginPopupModal("MM_Unsaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            const char* line = "Discard unsaved Parallax scene edits?";
            if (mmUnsavedPrompt == MmUnsavedKind::QuitApp) {
                line = "Quit with unsaved Parallax scene edits?";
            } else if (mmUnsavedPrompt == MmUnsavedKind::NewScene) {
                line = "Discard edits and create a new scene?";
            } else if (mmUnsavedPrompt == MmUnsavedKind::ImportPrlx) {
                line = "Discard edits and import the selected .prlx?";
            }
            LibUI::Widgets::Text(line);
            if (LibUI::Widgets::Button("Discard", ImVec2(120, 0))) {
                const MmUnsavedKind k = mmUnsavedPrompt;
                mmUnsavedPrompt = MmUnsavedKind::None;
                LibUI::Widgets::CloseCurrentPopup();
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
            LibUI::Widgets::SameLine();
            if (LibUI::Widgets::Button("Cancel", ImVec2(120, 0))) {
                mmUnsavedPrompt = MmUnsavedKind::None;
                mmPendingImportPath.reset();
                LibUI::Widgets::CloseCurrentPopup();
            }
            LibUI::Widgets::EndPopup();
        }

        if (smmPrlxRecoveryOpen) {
            LibUI::Widgets::OpenPopup("SMM_PrlxRecovery");
        }
        if (LibUI::Widgets::BeginPopupModal("SMM_PrlxRecovery", &smmPrlxRecoveryOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            LibUI::Widgets::Text("A local PARALLAX autosave (recovery) is available. Restore it?");
            if (LibUI::Widgets::Button("Restore", ImVec2(140, 0))) {
                std::vector<std::byte> buf;
                std::string re;
                if (Solstice::EditorAudio::FileRecovery::ReadLatest(smmRecoveryDir, "prlx", buf, &re)) {
                    if (restorePrlxFromRecoveryBytes(std::span<const std::byte>(buf.data(), buf.size()))) {
                        Solstice::EditorAudio::FileRecovery::ClearMatchingPrefix(smmRecoveryDir, "prlx");
                        smmPrlxRecoveryOpen = false;
                        LibUI::Widgets::CloseCurrentPopup();
                    }
                } else {
                    smmStatus = "Could not read recovery: " + re;
                }
            }
            LibUI::Widgets::SameLine();
            if (LibUI::Widgets::Button("Dismiss", ImVec2(140, 0))) {
                smmPrlxRecoveryOpen = false;
                LibUI::Widgets::CloseCurrentPopup();
            }
            LibUI::Widgets::EndPopup();
        }

        DrainImports(resolver, assetEntries);
        Smm::DrainPendingGltfOps(*scene, resolver, elementSelected, smmStatus, compressPrlx, sceneDirty);
        Smm::Image::DrainPendingRasterOps(
            *scene, resolver, mgElementSelected, smmFitSpriteSizeOnImageImport, smmStatus, compressPrlx, sceneDirty);
        Smm::Audio::DrainPendingAudioAssetOps(*scene, resolver, elementSelected, smmStatus, compressPrlx, sceneDirty);
        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());

        uint64_t maxT = scene->GetTimelineDurationTicks() > 0 ? scene->GetTimelineDurationTicks() : 1;

        if (!io_mm.WantTextInput && io_mm.KeyCtrl && LibUI::Widgets::IsKeyPressed(ImGuiKey_S, false)) {
            requestSaveMovieMakerProject();
        }

        if (!io_mm.WantTextInput) {
            const uint64_t maxTk = scene->GetTimelineDurationTicks() > 0 ? scene->GetTimelineDurationTicks() : 1;
            if (LibUI::Widgets::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
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

        if (LibUI::Layout::BeginTwoPaneFixedLeftTable("SMM_MainLayout", 330.0f)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("SMM_LeftPane", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None);
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
                    if (LibUI::Widgets::BeginListBox("Elements", ImVec2(-1, 110))) {
                        for (size_t i = 0; i < scene->GetElements().size(); ++i) {
                            const auto& el = scene->GetElements()[i];
                            std::string label = std::to_string(i) + " - " + el.Name;
                            bool sel = (elementSelected == static_cast<int>(i));
                            if (LibUI::Widgets::Selectable(label.c_str(), sel)) {
                                elementSelected = static_cast<int>(i);
                                smmWorkspace.viewportSelectedElements.clear();
                                smmWorkspace.viewportSelectedElements.insert(elementSelected);
                            }
                        }
                        LibUI::Widgets::EndListBox();
                    }
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Duplicate, "Duplicate") && elementSelected >= 0 &&
                        static_cast<size_t>(elementSelected) < scene->GetElements().size()) {
                        const auto& el = scene->GetElements()[static_cast<size_t>(elementSelected)];
                        const auto& schemas = scene->GetSchemas();
                        std::string typeName =
                            el.SchemaIndex < schemas.size() ? schemas[el.SchemaIndex].TypeName : std::string("LightElement");
                        Solstice::Parallax::AddElement(*scene, typeName, el.Name + " Copy", el.Parent);
                        sceneDirty = true;
                    }
                    if (elementSelected >= 0 && static_cast<size_t>(elementSelected) < scene->GetElements().size()) {
                        LibUI::Widgets::Text("Channel attribute");
                        LibUI::Widgets::SetNextItemWidth(-1.0f);
                        LibUI::Widgets::InputText("##ChannelAttribute", channelAttrBuf, sizeof(channelAttrBuf));
                        LibUI::Widgets::Text("Channel value type");
                        LibUI::Widgets::SetNextItemWidth(-1.0f);
                        ImGui::Combo("##ChannelValueType", &channelValueTypeCombo, "float\0vec3\0\0");
                        if (LibUI::Widgets::Button("Add keyframe at playhead")) {
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
                    LibUI::Widgets::Text("Duration (ticks)");
                    LibUI::Widgets::SetNextItemWidth(-1.0f);
                    if (LibUI::Widgets::InputScalar("##DurationTicks", ImGuiDataType_U64, &dur)) {
                        scene->SetTimelineDurationTicks(dur);
                        sceneDirty = true;
                    }
                    uint32_t tps = scene->GetTicksPerSecond();
                    LibUI::Widgets::Text("Ticks/sec");
                    LibUI::Widgets::SetNextItemWidth(-1.0f);
                    if (LibUI::Widgets::InputScalar("##TicksPerSecond", ImGuiDataType_U32, &tps)) {
                        scene->SetTicksPerSecond(tps);
                        sceneDirty = true;
                    }
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "Light")) {
                        Solstice::Parallax::AddElement(*scene, "LightElement", "Light", 0);
                        sceneDirty = true;
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "Camera")) {
                        Solstice::Parallax::AddElement(*scene, "CameraElement", "Camera", 0);
                        sceneDirty = true;
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "Actor")) {
                        Solstice::Parallax::AddElement(*scene, "ActorElement", "Actor", 0);
                        sceneDirty = true;
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "Audio")) {
                        const Solstice::Parallax::ElementIndex ai =
                            Solstice::Parallax::AddElement(*scene, "AudioSourceElement", "Audio", 0);
                        if (ai != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                            Solstice::Parallax::SetAttribute(*scene, ai, "Volume", Solstice::Parallax::AttributeValue{1.0f});
                            Solstice::Parallax::SetAttribute(*scene, ai, "Pitch", Solstice::Parallax::AttributeValue{1.0f});
                            elementSelected = static_cast<int>(ai);
                            smmWorkspace.viewportSelectedElements.clear();
                            smmWorkspace.viewportSelectedElements.insert(elementSelected);
                            sceneDirty = true;
                        }
                    }
                    LibUI::Widgets::Separator();
                    LibUI::Widgets::Text("Motion graphics");
                    Smm::DrawMg2DCompTools(*scene, mgElementSelected, sceneDirty, compressPrlx, smmNominalCompW, smmNominalCompH);
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::New, "MG Sprite")) {
                        const Solstice::Parallax::MGIndex idx = Solstice::Parallax::AddMGElement(*scene, "MGSpriteElement", "Sprite",
                            Solstice::Parallax::PARALLAX_INVALID_INDEX);
                        if (idx != Solstice::Parallax::PARALLAX_INVALID_INDEX) {
                            auto& rec = scene->GetMGElements()[idx];
                            rec.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(16.f, 16.f)};
                            rec.Attributes["Size"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2(256.f, 256.f)};
                            rec.Attributes["MGProjectionMode"] = Solstice::Parallax::AttributeValue{int32_t{0}};
                            rec.Attributes["WorldPosition"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{0.f, 1.2f, 0.f}};
                            rec.Attributes["WorldScale"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec3{1.f, 1.f, 1.f}};
                            rec.Attributes["WorldPitchDeg"] = Solstice::Parallax::AttributeValue{0.f};
                            rec.Attributes["WorldYawDeg"] = Solstice::Parallax::AttributeValue{0.f};
                            rec.Attributes["WorldRollDeg"] = Solstice::Parallax::AttributeValue{0.f};
                            rec.Attributes["AttachToElement"] = Solstice::Parallax::AttributeValue{false};
                            rec.Attributes["AttachElementIndex"] = Solstice::Parallax::AttributeValue{int32_t{-1}};
                            rec.Attributes["CastShadows"] = Solstice::Parallax::AttributeValue{true};
                            mgElementSelected = static_cast<int>(idx);
                            sceneDirty = true;
                        }
                    }
                    LibUI::Widgets::Checkbox("Fit MG sprite Size to imported image dims", &smmFitSpriteSizeOnImageImport);
                    if (LibUI::Widgets::BeginListBox("MG elements", ImVec2(-1, 72))) {
                        for (size_t i = 0; i < scene->GetMGElements().size(); ++i) {
                            const auto& mg = scene->GetMGElements()[i];
                            std::string_view st{};
                            if (mg.SchemaIndex < scene->GetSchemas().size()) {
                                st = scene->GetSchemas()[mg.SchemaIndex].TypeName;
                            }
                            const std::string label = std::to_string(i) + " - " + mg.Name + " (" + std::string(st) + ")";
                            const bool sel = (mgElementSelected == static_cast<int>(i));
                            if (LibUI::Widgets::Selectable(label.c_str(), sel)) {
                                mgElementSelected = static_cast<int>(i);
                            }
                        }
                        LibUI::Widgets::EndListBox();
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
                            if (LibUI::Widgets::DragFloat("MG Depth (draw order)", &depth, 0.25f, -1.0e6f, 1.0e6f)) {
                                mgMut.Attributes["Depth"] = Solstice::Parallax::AttributeValue{depth};
                                sceneDirty = true;
                            }
                            if (LibUI::Widgets::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                                ImGui::SetTooltip("2D compositing order only: lower values draw first (behind); higher values draw "
                                                  "last (in front). Animate with a Depth track if needed.");
                            }
                        }
                        if (st == "MGSpriteElement") {
                            int32_t projMode = 0;
                            if (const auto itMode = mgRow.Attributes.find("MGProjectionMode"); itMode != mgRow.Attributes.end()) {
                                if (const auto* im = std::get_if<int32_t>(&itMode->second)) {
                                    projMode = *im;
                                }
                            }
                            int projModeCombo = static_cast<int>(projMode);
                            if (ImGui::Combo("Projection workflow", &projModeCombo, "Screen 2D\0Unified world 3D\0")) {
                                Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                projMode = std::clamp<int32_t>(static_cast<int32_t>(projModeCombo), 0, 1);
                                mgMut.Attributes["MGProjectionMode"] = Solstice::Parallax::AttributeValue{projMode};
                                sceneDirty = true;
                            }
                            if (projMode == 0 && LibUI::Widgets::Button("Convert selected sprite to unified 3D")) {
                                Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                mgMut.Attributes["MGProjectionMode"] = Solstice::Parallax::AttributeValue{int32_t{1}};
                                const Solstice::Math::Vec2 pos2 = [&]() {
                                    const auto it = mgMut.Attributes.find("Position");
                                    if (it != mgMut.Attributes.end()) {
                                        if (const auto* p = std::get_if<Solstice::Math::Vec2>(&it->second)) {
                                            return *p;
                                        }
                                    }
                                    return Solstice::Math::Vec2{16.f, 16.f};
                                }();
                                const Solstice::Math::Vec2 size2 = [&]() {
                                    const auto it = mgMut.Attributes.find("Size");
                                    if (it != mgMut.Attributes.end()) {
                                        if (const auto* p = std::get_if<Solstice::Math::Vec2>(&it->second)) {
                                            return *p;
                                        }
                                    }
                                    return Solstice::Math::Vec2{256.f, 256.f};
                                }();
                                mgMut.Attributes["WorldPosition"] = Solstice::Parallax::AttributeValue{
                                    Solstice::Math::Vec3{pos2.x * 0.01f, 1.2f, pos2.y * 0.01f}};
                                mgMut.Attributes["WorldScale"] = Solstice::Parallax::AttributeValue{
                                    Solstice::Math::Vec3{std::max(0.1f, std::abs(size2.x) * 0.01f), std::max(0.1f, std::abs(size2.y) * 0.01f), 1.f}};
                                mgMut.Attributes["WorldPitchDeg"] = Solstice::Parallax::AttributeValue{0.f};
                                mgMut.Attributes["WorldYawDeg"] = Solstice::Parallax::AttributeValue{0.f};
                                mgMut.Attributes["WorldRollDeg"] = Solstice::Parallax::AttributeValue{0.f};
                                mgMut.Attributes["CastShadows"] = Solstice::Parallax::AttributeValue{true};
                                mgMut.Attributes["AttachToElement"] = Solstice::Parallax::AttributeValue{false};
                                mgMut.Attributes["AttachElementIndex"] = Solstice::Parallax::AttributeValue{int32_t{-1}};
                                sceneDirty = true;
                            }

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
                            if (LibUI::Widgets::DragFloat2("Sprite display Size", szPair, 1.f, 2.f, 4096.f)) {
                                std::string err;
                                if (Smm::Image::TrySetSpriteDisplaySize(*scene,
                                        static_cast<Solstice::Parallax::MGIndex>(mgElementSelected), szPair[0], szPair[1], err)) {
                                    sceneDirty = true;
                                }
                            }

                            if (projMode == 1) {
                                Solstice::Math::Vec3 wp{0.f, 1.25f, 0.f};
                                if (const auto itP = mgRow.Attributes.find("WorldPosition"); itP != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&itP->second)) {
                                        wp = *p;
                                    }
                                }
                                float wpArr[3] = {wp.x, wp.y, wp.z};
                                if (LibUI::Widgets::DragFloat3("World position", wpArr, 0.02f, -1.0e6f, 1.0e6f)) {
                                    Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                    mgMut.Attributes["WorldPosition"] = Solstice::Parallax::AttributeValue{
                                        Solstice::Math::Vec3{wpArr[0], wpArr[1], wpArr[2]}};
                                    sceneDirty = true;
                                }
                                Solstice::Math::Vec3 ws{1.f, 1.f, 1.f};
                                if (const auto itS = mgRow.Attributes.find("WorldScale"); itS != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&itS->second)) {
                                        ws = *p;
                                    }
                                }
                                float wsArr[3] = {ws.x, ws.y, ws.z};
                                if (LibUI::Widgets::DragFloat3("World scale", wsArr, 0.01f, 0.05f, 256.f)) {
                                    Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                    mgMut.Attributes["WorldScale"] = Solstice::Parallax::AttributeValue{
                                        Solstice::Math::Vec3{wsArr[0], wsArr[1], wsArr[2]}};
                                    sceneDirty = true;
                                }
                                float pitch = 0.f;
                                float yaw = 0.f;
                                float roll = 0.f;
                                if (const auto it = mgRow.Attributes.find("WorldPitchDeg"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<float>(&it->second)) {
                                        pitch = *p;
                                    }
                                }
                                if (const auto it = mgRow.Attributes.find("WorldYawDeg"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<float>(&it->second)) {
                                        yaw = *p;
                                    }
                                }
                                if (const auto it = mgRow.Attributes.find("WorldRollDeg"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<float>(&it->second)) {
                                        roll = *p;
                                    }
                                }
                                if (LibUI::Widgets::DragFloat("World pitch (deg)", &pitch, 0.25f, -360.f, 360.f)
                                    || LibUI::Widgets::DragFloat("World yaw (deg)", &yaw, 0.25f, -360.f, 360.f)
                                    || LibUI::Widgets::DragFloat("World roll (deg)", &roll, 0.25f, -360.f, 360.f)) {
                                    Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                    mgMut.Attributes["WorldPitchDeg"] = Solstice::Parallax::AttributeValue{pitch};
                                    mgMut.Attributes["WorldYawDeg"] = Solstice::Parallax::AttributeValue{yaw};
                                    mgMut.Attributes["WorldRollDeg"] = Solstice::Parallax::AttributeValue{roll};
                                    sceneDirty = true;
                                }
                                bool castShadows = true;
                                if (const auto it = mgRow.Attributes.find("CastShadows"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<bool>(&it->second)) {
                                        castShadows = *p;
                                    }
                                }
                                if (LibUI::Widgets::Checkbox("Cast shadows (unified)", &castShadows)) {
                                    Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                    mgMut.Attributes["CastShadows"] = Solstice::Parallax::AttributeValue{castShadows};
                                    sceneDirty = true;
                                }
                                bool attach = false;
                                int32_t attachIdx = -1;
                                if (const auto it = mgRow.Attributes.find("AttachToElement"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<bool>(&it->second)) {
                                        attach = *p;
                                    }
                                }
                                if (const auto it = mgRow.Attributes.find("AttachElementIndex"); it != mgRow.Attributes.end()) {
                                    if (const auto* p = std::get_if<int32_t>(&it->second)) {
                                        attachIdx = *p;
                                    }
                                }
                                if (LibUI::Widgets::Checkbox("Attach to scene element", &attach)) {
                                    Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                    mgMut.Attributes["AttachToElement"] = Solstice::Parallax::AttributeValue{attach};
                                    sceneDirty = true;
                                }
                                if (attach) {
                                    std::vector<const char*> items;
                                    std::vector<int> itemToElement;
                                    static std::vector<std::string> labels;
                                    labels.clear();
                                    labels.reserve(scene->GetElements().size());
                                    for (size_t ei = 0; ei < scene->GetElements().size(); ++ei) {
                                        const std::string_view est = Solstice::Parallax::GetElementSchema(*scene, static_cast<Solstice::Parallax::ElementIndex>(ei));
                                        if (est != "ActorElement" && est != "CameraElement" && est != "LightElement") {
                                            continue;
                                        }
                                        labels.push_back(std::to_string(ei) + ": " + scene->GetElements()[ei].Name);
                                        items.push_back(labels.back().c_str());
                                        itemToElement.push_back(static_cast<int>(ei));
                                    }
                                    int currentItem = -1;
                                    for (size_t ii = 0; ii < itemToElement.size(); ++ii) {
                                        if (itemToElement[ii] == attachIdx) {
                                            currentItem = static_cast<int>(ii);
                                            break;
                                        }
                                    }
                                    if (!items.empty() && ImGui::Combo("Attach target", &currentItem, items.data(), static_cast<int>(items.size()))) {
                                        Smm::PushSceneUndoSnapshot(*scene, compressPrlx);
                                        const int32_t nextAttach = (currentItem >= 0 && static_cast<size_t>(currentItem) < itemToElement.size())
                                            ? static_cast<int32_t>(itemToElement[static_cast<size_t>(currentItem)])
                                            : int32_t{-1};
                                        mgMut.Attributes["AttachElementIndex"] = Solstice::Parallax::AttributeValue{nextAttach};
                                        sceneDirty = true;
                                    }
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
                        LibUI::Widgets::Separator();
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
                    if (LibUI::Widgets::Button("Import File")) {
                        LibUI::FileDialogs::ShowOpenFile(window, "Import", [](std::optional<std::string> path) {
                            if (path) {
                                QueuePath(std::move(*path));
                            }
                        });
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Widgets::Button("Import glTF")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import glTF asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::QueueGltfImportPath(std::move(*path));
                                }
                            },
                            Smm::kGltfFilters);
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Widgets::Button("Export glTF")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export selected glTF asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::QueueGltfExportPath(std::move(*path));
                                }
                            },
                            Smm::kGltfFilters);
                    }
                    if (LibUI::Widgets::Button("Import raster")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import raster texture", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Image::QueueRasterImportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Widgets::Button("Export raster")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export MG sprite Texture bytes", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Image::QueueRasterExportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterExportFilters));
                    }
                    if (LibUI::Widgets::Button("Import audio")) {
                        LibUI::FileDialogs::ShowOpenFile(
                            window, "Import audio asset", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Audio::QueueAudioImportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioImportFilters));
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Widgets::Button("Export audio")) {
                        LibUI::FileDialogs::ShowSaveFile(
                            window, "Export AudioAsset bytes", [](std::optional<std::string> path) {
                                if (path) {
                                    Smm::Audio::QueueAudioExportPath(std::move(*path));
                                }
                            },
                            std::span<const LibUI::FileDialogs::FileFilter>(Smm::Audio::kAudioExportFilters));
                    }
                    ImGui::InputText("Import folder (path)", folderPathBuf, sizeof(folderPathBuf));
                    if (LibUI::Widgets::Button("Scan folder")) {
                        std::filesystem::path p(folderPathBuf);
                        if (std::filesystem::is_directory(p)) {
                            ImportFolderRecursive(p, resolver, assetEntries);
                        }
                    }
                    int sel = assetSelected;
                    LibUI::AssetBrowser::DrawPanel("Assets", assetEntries, &sel);
                    assetSelected = sel;
                    ImGui::InputText("Import .prlx path", importPathBuf, sizeof(importPathBuf));
                    if (LibUI::Widgets::Button("Import PARALLAX")) {
                        if (sceneDirty) {
                            mmPendingImportPath = std::string(importPathBuf);
                            mmUnsavedPrompt = MmUnsavedKind::ImportPrlx;
                        } else {
                            performPrlxImportFromBuffers();
                        }
                    }
                    LibUI::Widgets::SameLine();
                    if (LibUI::Widgets::Button("Export PARALLAX")) {
                        requestExportParallaxScene();
                    }
                    if (!smmStatus.empty()) {
                        LibUI::Widgets::Separator();
                        ImGui::TextWrapped("%s", smmStatus.c_str());
                    }
                    LibUI::Widgets::EndTabItem();
                }
                LibUI::Widgets::EndTabBar();
            }
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild(
                "SMM_MainPane", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushID("SMM_ViewerToolbarScope");
            if (ImGui::BeginTable("SMM_ViewerToolbar", 3,
                    ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn("viewer", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("middle", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 520.0f);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Viewer");
                ImGui::TableNextColumn();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Settings, "Overlays")) {
                    LibUI::Widgets::OpenPopup("SMM_ViewportOverlaysPopup");
                }
                LibUI::Widgets::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Mesh, "Material")) {
                    LibUI::Widgets::OpenPopup("SMM_ViewportMaterialPopup");
                }
                LibUI::Widgets::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Shortcuts, "Ranges")) {
                    LibUI::Widgets::OpenPopup("SMM_TimelineRangesPopup");
                }
                ImGui::TableNextColumn();
                LibUI::Widgets::SetNextItemWidth(120.0f);
                ImGui::SliderFloat("MG overlay", &smmWorkspace.mgOverlayAlpha, 0.f, 1.f, "%.2f");
                LibUI::Widgets::SameLine();
                int mgWorkflowMode = static_cast<int>(smmWorkspace.unifiedMgWorkflowMode);
                LibUI::Widgets::SetNextItemWidth(210.0f);
                if (ImGui::Combo(
                        "##smmmgworkflow", &mgWorkflowMode, "MG workflow: Pure 2D\0MG workflow: Unified 3D\0")) {
                    smmWorkspace.unifiedMgWorkflowMode
                        = static_cast<Smm::UI::UnifiedMgWorkflowMode>(std::clamp(mgWorkflowMode, 0, 1));
                    sceneDirty = true;
                }
                if (smmWorkspace.unifiedMgWorkflowMode == Smm::UI::UnifiedMgWorkflowMode::Pure2D) {
                    LibUI::Widgets::SameLine();
                    LibUI::Widgets::Checkbox("Pure2D: disable 3D", &smmWorkspace.pure2DDisable3DBackground);
                }
                LibUI::Widgets::SameLine();
                const char* projLabels[] = {"Perspective", "Ortho top", "Ortho front", "Ortho side"};
                int projMode = static_cast<int>(smmWorkspace.unifiedViewportCamera.projection);
                LibUI::Widgets::SetNextItemWidth(145.0f);
                if (ImGui::Combo("##smmproj", &projMode, projLabels, IM_ARRAYSIZE(projLabels))) {
                    smmWorkspace.unifiedViewportCamera.projection = static_cast<LibUI::Viewport::OrbitProjectionMode>(projMode);
                }
                LibUI::Widgets::SameLine();
                if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Reload, "Reset")) {
                    smmWorkspace.unifiedViewportCamera = {};
                }
                ImGui::EndTable();
            }

            if (ImGui::BeginPopup("SMM_ViewportOverlaysPopup")) {
                LibUI::Widgets::Checkbox("Fluid AABB overlay", &smmWorkspace.showFluidVolumeOverlay);
                LibUI::Widgets::Checkbox("Framing guides (unified view)", &smmWorkspace.showViewportFramingGuides);
                ImGui::EndPopup();
            }
            if (ImGui::BeginPopup("SMM_ViewportMaterialPopup")) {
                LibUI::Widgets::Checkbox("Preview .smat on cubes", &smmWorkspace.previewUseSmat);
                LibUI::Widgets::SameLine();
                LibUI::Widgets::Checkbox("Actors only##smat", &smmWorkspace.previewSmatActorsOnly);
                LibUI::Widgets::SameLine();
                LibUI::Widgets::Checkbox("Selected element only##smat", &smmWorkspace.previewSmatSelectedOnly);
                LibUI::Tools::InputPathOpenBrowseRowHint(90.0f, "##smatpath", ".smat path (UTF-8)", smmWorkspace.previewSmatPath,
                    sizeof(smmWorkspace.previewSmatPath), window, "Open Solstice material", "Browse##smat",
                    std::span<const LibUI::FileDialogs::FileFilter>(kSmatFileFilters));
                LibUI::Widgets::Checkbox("Preview raster maps on cubes", &smmWorkspace.previewBindMaterialMaps);
                ImGui::SliderFloat("Schematic baked AO (low-poly)", &smmWorkspace.schematicBakedAO, 0.0f, 1.0f, "%.2f");
                ImGui::TextDisabled("Uses the same actor / selection filter as .smat above.");
                if (smmWorkspace.previewBindMaterialMaps) {
                    LibUI::Tools::InputPathOpenBrowseRowHint(88.0f, "##mapAlbedo", "Albedo map (RGBA)",
                        smmWorkspace.previewMaterialAlbedoPath, sizeof(smmWorkspace.previewMaterialAlbedoPath), window,
                        "Open albedo texture", "Browse##mapAlbedo",
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                    LibUI::Tools::InputPathOpenBrowseRowHint(88.0f, "##mapNormal", "Normal map (optional)",
                        smmWorkspace.previewMaterialNormalPath, sizeof(smmWorkspace.previewMaterialNormalPath), window,
                        "Open normal map", "Browse##mapNormal",
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                    LibUI::Tools::InputPathOpenBrowseRowHint(88.0f, "##mapRough", "Roughness map (optional)",
                        smmWorkspace.previewMaterialRoughnessPath, sizeof(smmWorkspace.previewMaterialRoughnessPath), window,
                        "Open roughness texture", "Browse##mapRough",
                        std::span<const LibUI::FileDialogs::FileFilter>(Smm::Image::kRasterImportFilters));
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
            static bool s_timelineDockOpen = true;
            const float dockHeaderH = ImGui::GetFrameHeightWithSpacing() + 4.0f;
            const float availMainH = ImGui::GetContentRegionAvail().y;
            const float timelineDockH = s_timelineDockOpen
                ? std::clamp(availMainH * 0.28f, 132.0f, 210.0f)
                : dockHeaderH;
            const float statusBarH = ImGui::GetTextLineHeightWithSpacing() + 4.0f;
            const float availableForViewport = ImGui::GetContentRegionAvail().y - timelineDockH - statusBarH - 2.0f;
            const float viewportHeight = std::max(180.0f, availableForViewport);
            if (ImGui::BeginChild("SMM_ViewportArea", ImVec2(0.0f, viewportHeight), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings)) {
                if (const char* undoMsg = Smm::GetPendingUndoSnapshotMessage()) {
                    if (!LibUI::Widgets::DrawInlineAlert(
                            "smm_undooom", undoMsg, LibUI::Widgets::InlineAlertSeverity::Warning)) {
                        Smm::ClearPendingUndoSnapshotMessage();
                    }
                }
                if (smmWorkspace.enginePreviewLastError[0] != '\0') {
                    if (!LibUI::Widgets::DrawInlineAlert(
                            "smm_engprev", smmWorkspace.enginePreviewLastError, LibUI::Widgets::InlineAlertSeverity::Error)) {
                        smmWorkspace.enginePreviewLastError[0] = '\0';
                    }
                }
#if defined(_WIN32)
                const bool enableUnifiedViewport = !LibUI::Tools::EnvVarTruthy("SOLSTICE_SMM_DISABLE_ENGINE_PREVIEW");
#else
                const bool enableUnifiedViewport = true;
#endif
                if (!enableUnifiedViewport) {
                    ImGui::TextDisabled("Unified viewport safe mode active (Windows).");
                    ImGui::TextDisabled("Unset SOLSTICE_SMM_DISABLE_ENGINE_PREVIEW to re-enable live viewport path.");
                    LibUI::Viewport::Frame safeVp{};
                    if (LibUI::Viewport::BeginHost("smm_unified_viewport_safe", ImVec2(-1, viewportHeight - 8.0f), true)) {
                        if (LibUI::Viewport::PollFrame(safeVp) && safeVp.draw_list) {
                            LibUI::Viewport::DrawCheckerboard(
                                safeVp.draw_list, safeVp.min, safeVp.max, 14.f, IM_COL32(32, 32, 42, 255), IM_COL32(24, 24, 30, 255));
                        }
                        LibUI::Viewport::EndHost();
                    }
                } else {
                Solstice::Math::Vec3 particleEmitter{0.f, 1.2f, 0.f};
                if (smmWorkspace.manualParticleEmitterWorld) {
                    particleEmitter = smmWorkspace.manualParticleEmitterWorldVec;
                } else if (smmWorkspace.particleEditorState.attachToSceneElement && elementSelected >= 0) {
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
                uv.primaryElementIndex = &elementSelected;
                uv.viewportSelectedElements = &smmWorkspace.viewportSelectedElements;
                uv.selectedMgElementIndex = &mgElementSelected;
                uv.compressPrlxForUndo = compressPrlx;
                uv.sceneDirty = &sceneDirty;
                uv.manualParticleEmitterWorld = &smmWorkspace.manualParticleEmitterWorld;
                uv.manualParticleEmitterWorldVec = &smmWorkspace.manualParticleEmitterWorldVec;
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
                uv.lowPolyAOPreview = smmWorkspace.schematicBakedAO;
                uv.enginePreviewErrorSink = smmWorkspace.enginePreviewLastError;
                uv.enginePreviewErrorSinkBytes = sizeof(smmWorkspace.enginePreviewLastError);
                uv.enginePreviewSessionDisabled = &smmWorkspace.enginePreviewSessionDisabled;
                uv.mgWorkflowMode = static_cast<int>(smmWorkspace.unifiedMgWorkflowMode);
                uv.disable3DInPure2D = smmWorkspace.pure2DDisable3DBackground;
                uv.cinematicView3D = &smmAuthoringSession.CinematicView;
                Solstice::MovieMaker::UI::Panels::DrawUnifiedViewportPanel(window, *scene, resolver, timeTicks,
                    smmScene3dPreviewTex, viewportHeight - 8.0f, &smmWorkspace.particleEditorState, &smmParticleSpriteTex,
                    particleEmitter, smmWorkspace.mgOverlayAlpha, uv);
                }
            }
            ImGui::EndChild();
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
            smmSession.reloadIniPresets = &SmmOnReloadIniPresets;
            smmSession.reloadIniPresetsUser = &activeProjectPath;
            smmWorkspace.timelineState.playheadTick = timeTicks;
            Smm::Editing::BridgeSyncFromScene(*scene, smmWorkspace.timelineState, smmWorkspace.curveEditorState, smmBindings);
            LibUI::Timeline::TimelineClampNestedRange(smmWorkspace.timelineState);
            if (ImGui::BeginPopup("SMM_TimelineRangesPopup")) {
                ImGui::TextDisabled("Nested sub-timeline: zooms track/curve to a [start,end) tick window.");
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
                if (!g_smmRangePresets.empty()) {
                    ImGui::TextUnformatted("Shot / act ranges (INI presets/Timeline)");
                    static int sSmmRng = 0;
                    sSmmRng = (std::clamp)(sSmmRng, 0, static_cast<int>(g_smmRangePresets.size()) - 1);
                    std::string rngItems;
                    for (const Smm::Timeline::RangePreset& p : g_smmRangePresets) {
                        rngItems += (p.DisplayName.empty() ? p.Id : p.DisplayName);
                        rngItems.push_back('\0');
                    }
                    rngItems.push_back('\0');
                    ImGui::SetNextItemWidth(300.f);
                    ImGui::Combo("##smmRngC", &sSmmRng, rngItems.c_str());
                    const Smm::Timeline::RangePreset& rng = g_smmRangePresets[static_cast<size_t>(sSmmRng)];
                    if (!rng.Description.empty() || !rng.Author.empty() || !rng.Tags.empty()) {
                        if (!rng.Description.empty()) {
                            ImGui::TextDisabled("%s", rng.Description.c_str());
                        }
                        if (!rng.Author.empty() || !rng.Tags.empty()) {
                            std::string meta;
                            if (!rng.Author.empty()) {
                                meta = "Author: " + rng.Author;
                            }
                            if (!rng.Tags.empty()) {
                                if (!meta.empty()) {
                                    meta += "  ·  ";
                                }
                                meta += "Tags: " + rng.Tags;
                            }
                            ImGui::TextDisabled("%s", meta.c_str());
                        }
                    }
                    if (ImGui::SmallButton("Apply to nested sub-range##smmRng1")) {
                        smmWorkspace.timelineState.nestedViewEnabled = true;
                        smmWorkspace.timelineState.nestedRangeStartTick = rng.StartTick;
                        smmWorkspace.timelineState.nestedRangeEndTick = rng.EndTick;
                        LibUI::Timeline::TimelineClampNestedRange(smmWorkspace.timelineState);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Jump playhead to range start##smmRng2")) {
                        timeTicks = rng.StartTick;
                        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(timeTicks, scene->GetTimelineDurationTicks());
                    }
                }
                ImGui::EndPopup();
            }
            smmPanelReg.DrawPanels();
            if (ImGui::BeginChild("SMM_TimelineDock", ImVec2(0.0f, timelineDockH), true,
                    ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
                if (ImGui::SmallButton(s_timelineDockOpen ? "v Timeline & Tracks" : "> Timeline & Tracks")) {
                    s_timelineDockOpen = !s_timelineDockOpen;
                }
                ImGui::PopStyleVar();
                if (s_timelineDockOpen) {
                    const float timelineInnerH = std::max(72.0f, ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 2.0f);
                    if (LibUI::Timeline::DrawAnimationTimeline(
                            "SMM_AnimationTimeline", smmWorkspace.timelineState, ImVec2(0.0f, timelineInnerH))) {
                        timeTicks = Solstice::MovieMaker::Workflow::ClampPlayhead(
                            smmWorkspace.timelineState.playheadTick, scene->GetTimelineDurationTicks());
                    }

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 1.0f));
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Prev, "-1t")) {
                        if (timeTicks > 0) {
                            --timeTicks;
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Step playhead back by 1 tick");
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Next, "+1t")) {
                        if (timeTicks < maxT) {
                            ++timeTicks;
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Step playhead forward by 1 tick");
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Prev, "-1s")) {
                        const uint64_t step = scene->GetTicksPerSecond();
                        timeTicks = (timeTicks > step) ? (timeTicks - step) : 0;
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Next, "+1s")) {
                        const uint64_t step = scene->GetTicksPerSecond();
                        timeTicks = std::min(maxT, timeTicks + step);
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Prev, "|<")) {
                        Solstice::MovieMaker::Workflow::JumpPlayheadToStart(timeTicks, scene->GetTimelineDurationTicks());
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Next, ">|")) {
                        Solstice::MovieMaker::Workflow::JumpPlayheadToEnd(timeTicks, scene->GetTimelineDurationTicks());
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Validate, "Snap")) {
                        timeTicks = Solstice::MovieMaker::Workflow::SnapTickToWholeSeconds(timeTicks, scene->GetTicksPerSecond());
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Prev, "KF-1s")) {
                        Solstice::MovieMaker::Workflow::ShiftSceneKeyframes(*scene,
                            -static_cast<int64_t>(scene->GetTicksPerSecond()));
                        sceneDirty = true;
                    }
                    ImGui::SameLine();
                    if (LibUI::Icons::SmallButtonWithIcon(LibUI::Icons::Id::Next, "KF+1s")) {
                        Solstice::MovieMaker::Workflow::ShiftSceneKeyframes(*scene,
                            static_cast<int64_t>(scene->GetTicksPerSecond()));
                        sceneDirty = true;
                    }
                    ImGui::PopStyleVar();
                }
            }
            ImGui::EndChild();
            ImGui::EndChild();
            LibUI::Layout::EndTwoPaneFixedLeftTable();
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
                        PrepareSceneEmbeddedAssets(*scene, resolver);
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
                ImGui::TextUnformatted("Presets (resolution / aspect)");
                {
                    const auto setWh = [&](uint32_t w, uint32_t h) {
                        videoW = w;
                        videoH = h;
                    };
                    if (ImGui::SmallButton("720p 16:9##vp")) {
                        setWh(1280, 720);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("1080p 16:9##vp")) {
                        setWh(1920, 1080);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("1440p 16:9##vp")) {
                        setWh(2560, 1440);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("4K 16:9##vp")) {
                        setWh(3840, 2160);
                    }
                }
                {
                    if (ImGui::SmallButton("9:16 1080 (vertical)##vp")) {
                        videoW = 1080u;
                        videoH = 1920u;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("9:16 720##vp")) {
                        videoW = 720u;
                        videoH = 1280u;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("9:16 4K##vp")) {
                        videoW = 2160u;
                        videoH = 3840u;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("1:1 1080##vp")) {
                        videoW = 1080u;
                        videoH = 1080u;
                    }
                }
                if (ImGui::SmallButton("4:3 XGA 1024x768##vp")) {
                    videoW = 1024u;
                    videoH = 768u;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Cinema 2.39:1 1920x800##vp")) {
                    videoW = 1920u;
                    videoH = 800u;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Nominal 2D comp (Properties)##vp")) {
                    videoW = static_cast<uint32_t>(std::max(16, static_cast<int>(std::lround(smmNominalCompW))));
                    videoH = static_cast<uint32_t>(std::max(16, static_cast<int>(std::lround(smmNominalCompH))));
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                    ImGui::SetTooltip("Uses the same nominal comp size as the MG 2D tools (authoring only; not scene bounds).");
                }
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
            if (activeVideoExportSession) {
                videoExportLog = "Export already running. Wait until it finishes.\n";
            } else {
                pendingVideoExportStart = std::move(vep);
                videoExportLog = "Queued export job. It will run after this UI frame.\n";
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
                if (activeVideoExportSession) {
                    videoExportLog = "Export already running. Wait until it finishes.\n";
                } else {
                    pendingVideoRenderQueueRun = true;
                    videoExportLog = "Queued render queue run. It will start after this UI frame.\n";
                }
            }
        }
        if (ImGui::Button("Copy video export log##vecpy")) {
            ImGui::SetClipboardText(videoExportLog.c_str());
        }
        ImGui::SameLine();
        if (!videoExportLastDetail.empty()) {
            if (ImGui::Button("Copy failure report (full detail)##vecpy2")) {
                ImGui::SetClipboardText(videoExportLastDetail.c_str());
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(last failure)");
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
            std::error_code tec;
            const std::filesystem::path tdir = std::filesystem::temp_directory_path(tec);
            std::filesystem::path tmp = (tec ? std::filesystem::current_path() : tdir) / "smm_ffmpeg_test.mp4";
            std::string outPath = tmp.generic_string();
            std::string args =
                "-y -hide_banner -loglevel warning -f lavfi -i testsrc=duration=1:size=160x120:rate=1 -pix_fmt yuv420p "
                "-c:v mpeg4 -q:v 4 \"";
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

        LibUI::Widgets::EndWindow();

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

        SmmSetCrashStage("frame.render");
        LibUI::Core::Render();
        SmmSetCrashStage("frame.swap");
        SDL_GL_SwapWindow(window);
    }

    SmmSetCrashStage("shutdown.textures");
    smmScene3dPreviewTex.Destroy();
    smmParticleSpriteTex.Destroy();
    SmmSetCrashStage("shutdown.preview");
    Solstice::EditorEnginePreview::Shutdown();
    SmmSetCrashStage("shutdown.audio");
    Solstice::EditorAudio::Shutdown();
    SmmSetCrashStage("shutdown.libui");
    LibUI::Core::Shutdown();
    SmmSetCrashStage("shutdown.window");
    LibUI::Shell::DestroyUtilityGlWindow(glWindow);
    SmmSetCrashStage("shutdown.sdl");
    SDL_Quit();
    return 0;
}
