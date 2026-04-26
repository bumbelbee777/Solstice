#include "Media/SmmAudio.hxx"
#include "SmmFileOps.hxx"

#include <Parallax/ParallaxScene.hxx>
#include <Solstice/EditorAudio/EditorAudio.hxx>

#include <imgui.h>

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Smm::Audio {

const LibUI::FileDialogs::FileFilter kAudioImportFilters[1] = {
    {"Audio", "wav;ogg;oga;flac;mp3;aiff;aif;opus"},
};

const LibUI::FileDialogs::FileFilter kAudioExportFilters[1] = {
    {"Audio (raw session bytes)", "wav"},
};

namespace {
std::mutex g_AudioMutex;
std::optional<std::string> g_PendingAudioImport;
std::optional<std::string> g_PendingAudioExport;
} // namespace

void QueueAudioImportPath(std::string pathUtf8) {
    std::lock_guard<std::mutex> lock(g_AudioMutex);
    g_PendingAudioImport = std::move(pathUtf8);
}

void QueueAudioExportPath(std::string pathUtf8) {
    std::lock_guard<std::mutex> lock(g_AudioMutex);
    g_PendingAudioExport = std::move(pathUtf8);
}

bool TrySetAudioSourceMix(Solstice::Parallax::ParallaxScene& scene, int elementSelected, float volume, float pitch,
    std::string& errOut) {
    errOut.clear();
    if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
        errOut = "No timeline element selected.";
        return false;
    }
    const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
    if (Solstice::Parallax::GetElementSchema(scene, el) != "AudioSourceElement") {
        errOut = "Select an AudioSourceElement.";
        return false;
    }
    Solstice::Parallax::SetAttribute(scene, el, "Volume", Solstice::Parallax::AttributeValue{volume});
    Solstice::Parallax::SetAttribute(scene, el, "Pitch", Solstice::Parallax::AttributeValue{pitch});
    return true;
}

void DrainPendingAudioAssetOps(Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::DevSessionAssetResolver& resolver,
    int elementSelected, std::string& statusLine, bool compressPrlx, bool& sceneDirty) {
    std::optional<std::string> imp;
    std::optional<std::string> exp;
    {
        std::lock_guard<std::mutex> lock(g_AudioMutex);
        imp = std::move(g_PendingAudioImport);
        exp = std::move(g_PendingAudioExport);
    }

    if (imp) {
        if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
            statusLine = "Select an AudioSourceElement before importing audio.";
        } else {
            const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
            if (Solstice::Parallax::GetElementSchema(scene, el) != "AudioSourceElement") {
                statusLine = "Select an AudioSourceElement (add Audio) to assign an AudioAsset.";
            } else if (const auto h = resolver.ImportFile(std::filesystem::path(*imp))) {
                Smm::PushSceneUndoSnapshot(scene, compressPrlx);
                Solstice::Parallax::SetAttribute(scene, el, "AudioAsset", Solstice::Parallax::AttributeValue{*h});
                sceneDirty = true;
                statusLine = "Imported audio to selected Audio source: " + *imp;
            } else {
                statusLine = "Audio import failed: could not read file: " + *imp;
            }
        }
    }

    if (exp) {
        if (elementSelected < 0 || static_cast<size_t>(elementSelected) >= scene.GetElements().size()) {
            statusLine = "Select an AudioSourceElement before exporting audio.";
        } else {
            const Solstice::Parallax::ElementIndex el = static_cast<Solstice::Parallax::ElementIndex>(elementSelected);
            if (Solstice::Parallax::GetElementSchema(scene, el) != "AudioSourceElement") {
                statusLine = "Select an AudioSourceElement to export its AudioAsset bytes.";
            } else {
                const Solstice::Parallax::AttributeValue av = Solstice::Parallax::GetAttribute(scene, el, "AudioAsset");
                const uint64_t* ph = std::get_if<uint64_t>(&av);
                if (!ph || *ph == 0) {
                    statusLine = "Selected Audio source has no AudioAsset.";
                } else {
                    Solstice::Parallax::AssetData ad{};
                    if (!resolver.Resolve(*ph, ad) || ad.Bytes.empty()) {
                        statusLine = "AudioAsset is not in the current session.";
                    } else {
                        const std::filesystem::path dstPath(*exp);
                        std::error_code ec;
                        if (!dstPath.parent_path().empty()) {
                            std::filesystem::create_directories(dstPath.parent_path(), ec);
                        }
                        std::ofstream out(dstPath, std::ios::binary | std::ios::trunc);
                        if (!out) {
                            statusLine = "Failed to open audio export path: " + *exp;
                        } else {
                            out.write(reinterpret_cast<const char*>(ad.Bytes.data()),
                                static_cast<std::streamsize>(ad.Bytes.size()));
                            if (!out) {
                                statusLine = "Failed to write audio export.";
                            } else {
                                statusLine = "Exported AudioAsset session bytes: " + *exp;
                            }
                        }
                    }
                }
            }
        }
    }
}

void DrawAudioSourceClipPreview(
    Solstice::Parallax::DevSessionAssetResolver& resolver, std::uint64_t audioContentHash, float deltaSeconds, std::string& statusLine) {
    if (!Solstice::EditorAudio::IsReady()) {
        ImGui::TextDisabled("Editor audio not initialized; waveform preview is unavailable.");
        return;
    }
    if (audioContentHash == 0) {
        return;
    }

    static std::uint64_t s_CachedKey = 0;
    static Solstice::EditorAudio::DecodedPcm s_Pcm;
    static std::vector<Solstice::EditorAudio::PeakMinMax> s_Peaks;
    static Solstice::EditorAudio::ScrubPlayer s_Player;
    static std::string s_LastDecodeError;

    if (s_CachedKey != audioContentHash) {
        s_CachedKey = audioContentHash;
        s_Pcm.Clear();
        s_Peaks.clear();
        s_Player.Stop();
        s_Player.Close();
        s_LastDecodeError.clear();
        Solstice::Parallax::AssetData ad{};
        if (!resolver.Resolve(audioContentHash, ad) || ad.Bytes.empty()) {
            s_LastDecodeError = "Could not resolve AudioAsset for waveform.";
        } else {
            const std::span<const std::byte> sp(
                reinterpret_cast<const std::byte*>(ad.Bytes.data()), ad.Bytes.size());
            if (!s_Pcm.DecodeMemory(sp, &s_LastDecodeError)) {
                s_LastDecodeError = "Decode: " + s_LastDecodeError;
            } else {
                s_Peaks = Solstice::EditorAudio::BuildMinMaxPeaks(s_Pcm, 200);
                (void)s_Player.LoadPcm(s_Pcm, &s_LastDecodeError);
            }
        }
    }

    if (!s_LastDecodeError.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.4f, 1.f), "%s", s_LastDecodeError.c_str());
        return;
    }
    if (s_Peaks.empty() || s_Pcm.samplesMono.empty()) {
        return;
    }

    ImGui::Text("Duration ~ %.2f s", s_Pcm.DurationSeconds());
    if (ImGui::Button("Play")) {
        s_Player.Seek01(0.f);
        s_Player.Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        s_Player.Stop();
    }
    ImGui::SameLine();
    if (s_Player.IsPlaying()) {
        ImGui::TextDisabled("Playing…");
    }

    const float w = std::max(32.f, ImGui::GetContentRegionAvail().x);
    const float h = 56.f;
    ImGui::InvisibleButton("##SmmAudioWave", ImVec2(w, h));
    const ImVec2 p0 = ImGui::GetItemRectMin();
    const ImVec2 p1 = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to scrub, click to seek.");
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        const float t = (w > 0.f) ? (m.x - p0.x) / w : 0.f;
        const float tcl = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        s_Player.PumpScrub(tcl, deltaSeconds);
    } else {
        s_Player.PumpPlay(deltaSeconds);
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        const float t = (w > 0.f) ? (m.x - p0.x) / w : 0.f;
        const float tcl = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        s_Player.Seek01(tcl);
    }
    {
        const ImU32 colFg = ImGui::GetColorU32(ImGuiCol_PlotLines);
        const ImU32 colPh = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg));
        const float midY = p0.y + h * 0.5f;
        const float amp = h * 0.45f;
        for (size_t i = 0; i < s_Peaks.size(); ++i) {
            const float t0 = static_cast<float>(i) / static_cast<float>(s_Peaks.size());
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(s_Peaks.size());
            const float x0 = p0.x + t0 * w;
            const float x1 = p0.x + t1 * w;
            const float xc = 0.5f * (x0 + x1);
            const auto& pk = s_Peaks[i];
            const float y0 = midY + pk.min * amp;
            const float y1 = midY + pk.max * amp;
            dl->AddLine(ImVec2(xc, y0), ImVec2(xc, y1), colFg, 1.0f);
        }
        float ph01 = s_Player.Playhead01();
        if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const ImVec2 m = ImGui::GetIO().MousePos;
            const float t = (w > 0.f) ? (m.x - p0.x) / w : 0.f;
            ph01 = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
        }
        const float x = p0.x + ph01 * w;
        dl->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), colPh, 2.0f);
    }
}

} // namespace Smm::Audio
