#include "SmmGraphEditor.hxx"
#include "../SmmFileOps.hxx"

#include <Parallax/ParallaxScene.hxx>

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace Smm::Editing {

namespace {

static bool SampleBindingAtTick(const Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& b, uint64_t tick,
    float& outV, std::string& err) {
    err.clear();
    if (b.kind == TrackBindingKind::SceneChannel) {
        if (b.index >= scene.GetChannels().size()) {
            err = "Bad channel.";
            return false;
        }
        const auto& ch = scene.GetChannels()[b.index];
        Solstice::Parallax::AttributeValue av =
            Solstice::Parallax::EvaluateChannel(scene, b.index, tick);
        if (ch.ValueType == Solstice::Parallax::AttributeType::Float) {
            if (const auto* f = std::get_if<float>(&av)) {
                outV = *f;
                return true;
            }
        }
        if (ch.ValueType == Solstice::Parallax::AttributeType::Vec3 && b.component >= 0 && b.component <= 2) {
            if (const auto* v = std::get_if<Solstice::Math::Vec3>(&av)) {
                outV = (b.component == 0) ? v->x : (b.component == 1) ? v->y : v->z;
                return true;
            }
        }
        err = "Could not evaluate channel.";
        return false;
    }
    if (b.index >= scene.GetMGTracks().size()) {
        err = "Bad MG track.";
        return false;
    }
    const auto& tr = scene.GetMGTracks()[b.index];
    Solstice::Parallax::AttributeValue av = std::monostate{};
    if (!tr.Keyframes.empty()) {
        if (tr.Keyframes.size() == 1) {
            av = tr.Keyframes[0].Value;
        } else if (tick <= tr.Keyframes.front().TimeTicks) {
            av = tr.Keyframes.front().Value;
        } else if (tick >= tr.Keyframes.back().TimeTicks) {
            av = tr.Keyframes.back().Value;
        } else {
            size_t i = 1;
            for (; i < tr.Keyframes.size(); ++i) {
                if (tr.Keyframes[i].TimeTicks >= tick) {
                    break;
                }
            }
            const auto& k0 = tr.Keyframes[i - 1];
            const auto& k1 = tr.Keyframes[i];
            const double span = static_cast<double>(k1.TimeTicks - k0.TimeTicks);
            float t01 = span > 0.0 ? static_cast<float>(static_cast<double>(tick - k0.TimeTicks) / span) : 0.f;
            if (tr.ValueType == Solstice::Parallax::AttributeType::Float) {
                const float fa = std::get_if<float>(&k0.Value) ? *std::get_if<float>(&k0.Value) : 0.f;
                const float fb = std::get_if<float>(&k1.Value) ? *std::get_if<float>(&k1.Value) : 0.f;
                av = Solstice::Parallax::AttributeValue{fa + (fb - fa) * t01};
            } else if (tr.ValueType == Solstice::Parallax::AttributeType::Vec3) {
                const auto* va = std::get_if<Solstice::Math::Vec3>(&k0.Value);
                const auto* vb = std::get_if<Solstice::Math::Vec3>(&k1.Value);
                if (va && vb) {
                    av = Solstice::Parallax::AttributeValue{Solstice::Math::Vec3(va->x + (vb->x - va->x) * t01,
                        va->y + (vb->y - va->y) * t01, va->z + (vb->z - va->z) * t01)};
                }
            } else {
                av = t01 < 0.5f ? k0.Value : k1.Value;
            }
        }
    }
    if (tr.ValueType == Solstice::Parallax::AttributeType::Float) {
        if (const auto* f = std::get_if<float>(&av)) {
            outV = *f;
            return true;
        }
    }
    if (tr.ValueType == Solstice::Parallax::AttributeType::Vec3 && b.component >= 0 && b.component <= 2) {
        if (const auto* v = std::get_if<Solstice::Math::Vec3>(&av)) {
            outV = (b.component == 0) ? v->x : (b.component == 1) ? v->y : v->z;
            return true;
        }
    }
    err = "Could not sample MG track.";
    return false;
}

} // namespace

void DrawGraphEditorSession(const char* windowTitle, bool* visible, AppSessionContext& ctx, GraphEditorState& graph,
    LibUI::Timeline::TimelineState& timeline, const std::vector<EditorTrackBinding>& bindings) {
    if (!ctx.scene || !ctx.sceneDirty) {
        return;
    }
    if (visible && !*visible) {
        return;
    }
    const char* title = (windowTitle && windowTitle[0]) ? windowTitle : "Graph Editor";
    if (!ImGui::Begin(title, visible)) {
        ImGui::End();
        return;
    }

    const uint64_t tick = ctx.timeTicks ? *ctx.timeTicks : timeline.playheadTick;

    ImGui::TextUnformatted("Track links (driver → driven). Bake copies a key at the playhead from driver to driven.");
    if (graph.selectedLink >= 0 && graph.selectedLink < static_cast<int>(graph.links.size())) {
        const GraphDriverLink& Ls = graph.links[static_cast<size_t>(graph.selectedLink)];
        if (Ls.driverTrackIndex < bindings.size() && Ls.drivenTrackIndex < bindings.size()) {
            float dv = 0.f;
            std::string se;
            if (SampleBindingAtTick(*ctx.scene, bindings[Ls.driverTrackIndex], tick, dv, se)) {
                const float out = dv * Ls.scale + Ls.offset;
                char buf[160]{};
                std::snprintf(buf, sizeof(buf), "At playhead: driver ≈ %.4f  →  scaled+offset ≈ %.4f (bake target)", dv, out);
                ImGui::TextDisabled("%s", buf);
            } else if (!se.empty()) {
                ImGui::TextDisabled("Driver sample: %s", se.c_str());
            }
        }
    }
    if (timeline.tracks.empty()) {
        ImGui::TextDisabled("No tracks.");
        ImGui::End();
        return;
    }

    if (ImGui::Button("Add link (driver=selected track, driven=next)")) {
        const int d = timeline.selectedTrack;
        if (d >= 0 && d + 1 < static_cast<int>(timeline.tracks.size())) {
            GraphDriverLink L{};
            L.driverTrackIndex = static_cast<uint32_t>(d);
            L.drivenTrackIndex = static_cast<uint32_t>(d + 1);
            graph.links.push_back(L);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove selected link") && graph.selectedLink >= 0 &&
        graph.selectedLink < static_cast<int>(graph.links.size())) {
        graph.links.erase(graph.links.begin() + graph.selectedLink);
        graph.selectedLink = -1;
    }

    ImGui::Separator();
    for (size_t i = 0; i < graph.links.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        GraphDriverLink& L = graph.links[i];
        if (ImGui::Selectable(("Link " + std::to_string(i)).c_str(), graph.selectedLink == static_cast<int>(i))) {
            graph.selectedLink = static_cast<int>(i);
        }
        int drv = static_cast<int>(L.driverTrackIndex);
        int drn = static_cast<int>(L.drivenTrackIndex);
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Driver row", &drv, 0, static_cast<int>(timeline.tracks.size()) - 1);
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Driven row", &drn, 0, static_cast<int>(timeline.tracks.size()) - 1);
        L.driverTrackIndex = static_cast<uint32_t>(drv);
        L.drivenTrackIndex = static_cast<uint32_t>(drn);
        ImGui::DragFloat("Scale", &L.scale, 0.01f, -10.f, 10.f);
        ImGui::DragFloat("Offset", &L.offset, 0.01f, -1000.f, 1000.f);
        if (ImGui::Button("Bake at playhead")) {
            if (L.driverTrackIndex < bindings.size() && L.drivenTrackIndex < bindings.size()) {
                float v = 0.f;
                std::string e;
                if (SampleBindingAtTick(*ctx.scene, bindings[L.driverTrackIndex], tick, v, e)) {
                    const float out = v * L.scale + L.offset;
                    Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                    if (BridgeAddKeyframeAtTick(*ctx.scene, bindings[L.drivenTrackIndex], tick, out, e)) {
                        *ctx.sceneDirty = true;
                        if (ctx.statusLine) {
                            *ctx.statusLine = "Graph: baked driver → driven at playhead.";
                        }
                    } else if (ctx.statusLine && !e.empty()) {
                        *ctx.statusLine = e;
                    }
                } else if (ctx.statusLine && !e.empty()) {
                    *ctx.statusLine = e;
                }
            }
        }
        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::End();
}

} // namespace Smm::Editing
