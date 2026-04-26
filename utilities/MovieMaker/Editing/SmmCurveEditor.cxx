#include "SmmCurveEditor.hxx"
#include "../SmmFileOps.hxx"

#include <MinGfx/EasingFunction.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>

#include "LibUI/Timeline/TimelineModel.hxx"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

namespace Smm::Editing {

namespace {

static float ValueSpanMin(
    const LibUI::CurveEditor::CurveChannel& ch, float fallback, const LibUI::CurveEditor::CurveEditorState* cst) {
    if (cst && cst->valueFitOverride) {
        return cst->valueFitMin;
    }
    if (ch.keys.empty()) {
        return fallback - 1.f;
    }
    float m = ch.keys[0].value;
    for (const auto& k : ch.keys) {
        m = (std::min)(m, k.value);
    }
    return m - 0.5f;
}

static float ValueSpanMax(
    const LibUI::CurveEditor::CurveChannel& ch, float fallback, const LibUI::CurveEditor::CurveEditorState* cst) {
    if (cst && cst->valueFitOverride) {
        return cst->valueFitMax;
    }
    if (ch.keys.empty()) {
        return fallback + 1.f;
    }
    float m = ch.keys[0].value;
    for (const auto& k : ch.keys) {
        m = (std::max)(m, k.value);
    }
    return m + 0.5f;
}

static ImVec2 KeyToScreenPlotT(float tPlot01, float value, const ImVec2& plotMin, const ImVec2& plotSize, float vmin, float vmax) {
    const float tx = plotMin.x + std::clamp(tPlot01, 0.f, 1.f) * plotSize.x;
    const float ty = plotMin.y + (1.f - (value - vmin) / (std::max(vmax - vmin, 1e-4f))) * plotSize.y;
    return ImVec2(tx, ty);
}

static void TimeValueFromScreen(ImVec2 p, const ImVec2& plotMin, const ImVec2& plotSize, float vmin, float vmax, float& outT01,
    float& outVal) {
    outT01 = std::clamp((p.x - plotMin.x) / (std::max(plotSize.x, 1.f)), 0.f, 1.f);
    const float ny = std::clamp((p.y - plotMin.y) / (std::max(plotSize.y, 1.f)), 0.f, 1.f);
    outVal = vmin + (1.f - ny) * (vmax - vmin);
}

static float KeyTime01ToPlotX(
    const LibUI::CurveEditor::CurveKey& k, uint64_t fullDur, const LibUI::Timeline::TimelineState& tl) {
    const uint64_t dtot = (std::max)(fullDur, 1ull);
    const uint64_t tick = static_cast<uint64_t>(std::llround(static_cast<double>(k.time) * static_cast<double>(dtot)));
    if (!tl.nestedViewEnabled) {
        return k.time;
    }
    if (tl.nestedRangeEndTick <= tl.nestedRangeStartTick) {
        return k.time;
    }
    const uint64_t lo = tl.nestedRangeStartTick;
    const uint64_t span = (std::max)(tl.nestedRangeEndTick - lo, 1ull);
    if (tick <= lo) {
        return 0.f;
    }
    if (tick >= tl.nestedRangeEndTick) {
        return 1.f;
    }
    return static_cast<float>(static_cast<double>(tick - lo) / static_cast<double>(span));
}

static uint64_t PlotXToSceneTick(
    float plotT01, uint64_t fullDur, const LibUI::Timeline::TimelineState& tl) {
    const uint64_t dtot = (std::max)(fullDur, 1ull);
    plotT01 = std::clamp(plotT01, 0.f, 1.f);
    if (!tl.nestedViewEnabled || tl.nestedRangeEndTick <= tl.nestedRangeStartTick) {
        return static_cast<uint64_t>(std::llround(static_cast<double>(plotT01) * static_cast<double>(dtot)));
    }
    const uint64_t lo = tl.nestedRangeStartTick;
    const uint64_t span = (std::max)(tl.nestedRangeEndTick - lo, 1ull);
    uint64_t tick = lo + static_cast<uint64_t>(std::llround(static_cast<double>(plotT01) * static_cast<double>(span)));
    if (tick > dtot) {
        tick = dtot;
    }
    return tick;
}

static float PlayheadToPlotT01(
    const uint64_t* timeTicks, uint64_t fullDur, const LibUI::Timeline::TimelineState& tl) {
    if (!timeTicks) {
        return 0.f;
    }
    const uint64_t dtot = (std::max)(fullDur, 1ull);
    const uint64_t pt = (std::min)(*timeTicks, dtot);
    if (!tl.nestedViewEnabled || tl.nestedRangeEndTick <= tl.nestedRangeStartTick) {
        return static_cast<float>(static_cast<double>(pt) / static_cast<double>(dtot));
    }
    const uint64_t lo = tl.nestedRangeStartTick;
    const uint64_t span = (std::max)(tl.nestedRangeEndTick - lo, 1ull);
    if (pt <= lo) {
        return 0.f;
    }
    if (pt >= tl.nestedRangeEndTick) {
        return 1.f;
    }
    return static_cast<float>(static_cast<double>(pt - lo) / static_cast<double>(span));
}

static uint64_t SnapTimeToTick(
    const Solstice::Parallax::ParallaxScene& scene, const KeyframeEditUiState& kui, const uint64_t tickIn) {
    if (!kui.snapEnabled) {
        return tickIn;
    }
    if (kui.snapFps <= 0) {
        return tickIn;
    }
    const uint32_t tps = (std::max)(scene.GetTicksPerSecond(), 1u);
    const uint64_t step = (std::max)(uint64_t{1},
        static_cast<uint64_t>(tps) / static_cast<uint64_t>(static_cast<unsigned>(kui.snapFps)));
    return (tickIn + step / 2) / step * step;
}

static float CubicBezier1DValue(float p0, float c0, float c1, float p1, float u) {
    u = (std::clamp)(u, 0.f, 1.f);
    const float o = 1.f - u;
    return o * o * o * p0 + 3.f * o * o * u * c0 + 3.f * o * u * u * c1 + u * u * u * p1;
}

} // namespace

void DrawCurveEditorSession(const char* windowTitle, bool* visible, AppSessionContext& ctx,
    LibUI::CurveEditor::CurveEditorState& curves, LibUI::Timeline::TimelineState& timeline) {
    if (!ctx.scene || !ctx.bindings || !ctx.sceneDirty) {
        return;
    }
    if (visible && !*visible) {
        return;
    }
    const char* title = (windowTitle && windowTitle[0]) ? windowTitle : "Curve Editor";
    if (!ImGui::Begin(title, visible)) {
        ImGui::End();
        return;
    }

    if (timeline.selectedTrack >= 0 && timeline.selectedTrack < static_cast<int>(curves.channels.size())) {
        curves.selectedChannel = timeline.selectedTrack;
    }
    if (curves.selectedChannel >= 0 && curves.selectedChannel < static_cast<int>(curves.channels.size())) {
        timeline.selectedTrack = curves.selectedChannel;
    }

    curves.zoomX = std::clamp(curves.zoomX, 0.1f, 32.0f);
    curves.zoomY = std::clamp(curves.zoomY, 0.1f, 32.0f);
    ImGui::SliderFloat("Time zoom", &curves.zoomX, 0.1f, 32.0f, "%.2fx");
    ImGui::SliderFloat("Value zoom", &curves.zoomY, 0.1f, 32.0f, "%.2fx");
    if (ctx.keyframeEdit) {
        KeyframeEditUiState& ke = *ctx.keyframeEdit;
        ImGui::Checkbox("Snap key time to frame grid", &ke.snapEnabled);
        ImGui::SetNextItemWidth(220.f);
        ImGui::SliderInt("Frame rate (0 = any tick)", &ke.snapFps, 0, 60);
        if (ke.clipValid) {
            ImGui::TextDisabled("(clipboard: tick %llu  value %.4f — paste at playhead: Ctrl+V)",
                static_cast<unsigned long long>(ke.clipTick), static_cast<double>(ke.clipValue));
        } else {
            ImGui::TextDisabled("(select a key, Ctrl+C to copy)");
        }
    }
    ImGui::Separator();

    ImGui::TextUnformatted("Channels");
    for (size_t i = 0; i < curves.channels.size(); ++i) {
        LibUI::CurveEditor::CurveChannel& channel = curves.channels[i];
        ImGui::Checkbox(("##cv_" + channel.name).c_str(), &channel.visible);
        ImGui::SameLine();
        if (ImGui::Selectable(channel.name.c_str(), curves.selectedChannel == static_cast<int>(i))) {
            curves.selectedChannel = static_cast<int>(i);
            timeline.selectedTrack = static_cast<int>(i);
            curves.selectedKeyIndex = -1;
            curves.selectedKeyIndices.clear();
        }
    }

    if (curves.channels.empty() || curves.selectedChannel < 0 ||
        curves.selectedChannel >= static_cast<int>(curves.channels.size())) {
        ImGui::TextDisabled("No editable float/vec3 tracks. Add channels from Properties.");
        ImGui::End();
        return;
    }

    const size_t bindIdx = static_cast<size_t>(curves.selectedChannel);
    if (bindIdx >= ctx.bindings->size()) {
        ImGui::End();
        return;
    }
    const EditorTrackBinding& binding = (*ctx.bindings)[bindIdx];
    LibUI::CurveEditor::CurveChannel& active = curves.channels[bindIdx];
    const bool isFloatTrack = (binding.kind == TrackBindingKind::SceneChannel)
        ? (binding.index < ctx.scene->GetChannels().size()
                && ctx.scene->GetChannels()[binding.index].ValueType == Solstice::Parallax::AttributeType::Float
                && binding.component < 0)
        : (binding.index < ctx.scene->GetMGTracks().size()
                && ctx.scene->GetMGTracks()[binding.index].ValueType == Solstice::Parallax::AttributeType::Float
                && binding.component < 0);

    if (ctx.keyframePresets && !ctx.keyframePresets->empty()) {
        ImGui::TextUnformatted("Keyframe presets (INI in presets/Keyframe/)");
        static int sPresetIdx = 0;
        sPresetIdx = (std::clamp)(sPresetIdx, 0, static_cast<int>(ctx.keyframePresets->size()) - 1);
        std::string comboItems;
        for (const Smm::Keyframe::KeyframeCurvePreset& p : *ctx.keyframePresets) {
            const std::string& lab = p.DisplayName.empty() ? p.Id : p.DisplayName;
            comboItems.append(lab);
            comboItems.push_back('\0');
        }
        comboItems.push_back('\0');
        ImGui::SetNextItemWidth(360.f);
        ImGui::Combo("##smmkfp", &sPresetIdx, comboItems.c_str());
        if (ImGui::Button("Apply INI keyframe preset to selected##smmkfp2") && ctx.scene) {
            const Smm::Keyframe::KeyframeCurvePreset& pr = (*ctx.keyframePresets)[static_cast<size_t>(sPresetIdx)];
            std::vector<int> wk;
            if (!curves.selectedKeyIndices.empty()) {
                wk = curves.selectedKeyIndices;
            } else if (curves.selectedKeyIndex >= 0) {
                wk.push_back(curves.selectedKeyIndex);
            }
            if (!wk.empty() && pr.EaseIn <= 13) {
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                for (int kix : wk) {
                    uint64_t rt = 0;
                    float rv = 0.f;
                    uint8_t re = 0;
                    std::string ers;
                    if (!BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(kix), rt, rv, re, ers) || !ers.empty()) {
                        continue;
                    }
                    const auto eIn = static_cast<Solstice::Parallax::EasingType>(pr.EaseIn);
                    (void)BridgeSetKeyframeEasing(*ctx.scene, binding, static_cast<size_t>(kix), eIn, ers);
                    if (binding.kind == TrackBindingKind::SceneChannel) {
                        Solstice::Parallax::SetKeyframeEaseOut(*ctx.scene, binding.index, rt, pr.EaseOut);
                        Solstice::Parallax::SetKeyframeInterpolation(
                            *ctx.scene, binding.index, rt, static_cast<Solstice::Parallax::KeyframeInterpolation>(pr.Interp));
                        if (isFloatTrack && pr.Interp == 3) {
                            Solstice::Parallax::SetKeyframeBezierTangents(
                                *ctx.scene, binding.index, rt, pr.TangentOut, pr.TangentIn);
                        }
                    } else {
                        Solstice::Parallax::SetMGKeyframeEaseOut(*ctx.scene, binding.index, rt, pr.EaseOut);
                        Solstice::Parallax::SetMGKeyframeInterpolation(
                            *ctx.scene, binding.index, rt, static_cast<Solstice::Parallax::KeyframeInterpolation>(pr.Interp));
                        if (isFloatTrack && pr.Interp == 3) {
                            Solstice::Parallax::SetMGKeyframeBezierTangents(
                                *ctx.scene, binding.index, rt, pr.TangentOut, pr.TangentIn);
                        }
                    }
                }
                *ctx.sceneDirty = true;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: applied keyframe INI preset.";
                }
            }
        }
    }
    {
        static const char* kEasingItems[] = {"Linear", "EaseIn", "EaseOut", "EaseInOut", "Bounce", "Elastic", "Back", "Circ",
            "Expo", "Quad", "Cubic", "Quart", "Quint", "Bezier"};
        constexpr int kEasingCount = 14;
        static int sEaseEdit = 0;
        if (!active.keys.empty() && (curves.selectedKeyIndex >= 0 || !curves.selectedKeyIndices.empty())) {
            if (!curves.selectedKeyIndices.empty()) {
                const uint8_t e0 = active.keys[static_cast<size_t>(curves.selectedKeyIndices.front())].easing;
                bool mix = false;
                for (int ki : curves.selectedKeyIndices) {
                    if (ki < 0 || static_cast<size_t>(ki) >= active.keys.size()) {
                        continue;
                    }
                    if (active.keys[static_cast<size_t>(ki)].easing != e0) {
                        mix = true;
                        break;
                    }
                }
                if (mix) {
                    ImGui::TextDisabled("Selected keys have different easings; pick a preset and apply.");
                } else {
                    sEaseEdit = static_cast<int>(e0);
                }
            } else {
                sEaseEdit = static_cast<int>(active.keys[static_cast<size_t>(curves.selectedKeyIndex)].easing);
            }
        }
        sEaseEdit = (std::clamp)(sEaseEdit, 0, kEasingCount - 1);
        ImGui::TextUnformatted("Keyframe easing: segment into this key (same as Parallax channel/MG evaluation)");
        ImGui::SetNextItemWidth(300.f);
        ImGui::Combo("##smmkeyease", &sEaseEdit, kEasingItems, kEasingCount);
        ImGui::SameLine();
        if (ImGui::Button("Apply to selected##smmsetease") && ctx.scene) {
            std::vector<int> work;
            if (!curves.selectedKeyIndices.empty()) {
                work = curves.selectedKeyIndices;
            } else if (curves.selectedKeyIndex >= 0) {
                work.push_back(curves.selectedKeyIndex);
            }
            if (!work.empty() && sEaseEdit >= 0 && sEaseEdit < kEasingCount) {
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                std::string errE;
                const auto e = static_cast<Solstice::Parallax::EasingType>(sEaseEdit);
                for (int kix : work) {
                    (void)BridgeSetKeyframeEasing(*ctx.scene, binding, static_cast<size_t>(kix), e, errE);
                }
                *ctx.sceneDirty = true;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: set keyframe easing.";
                }
            }
        }
    }
    {
        if (curves.maxKeyframeCountHint > 20000u) {
            ImGui::TextColored(ImVec4(1.0f, 160.0f / 255.0f, 80.0f / 255.0f, 1.0f),
                "Performance: a synced track has >20k keyframes. Expect UI cost.");
        }
        ImGui::TextUnformatted("Value axis: zoom-to-fit (padding 10%%, overrides auto span for this view)");
        if (ImGui::Button("Zoom value to fit##smmvfit") && !active.keys.empty()) {
            float m = active.keys[0].value;
            float n = m;
            for (const auto& k : active.keys) {
                m = (std::min)(m, k.value);
                n = (std::max)(n, k.value);
            }
            const float pad = (n - m) * 0.1f + 0.0001f;
            curves.valueFitOverride = true;
            curves.valueFitMin = m - pad;
            curves.valueFitMax = n + pad;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset value fit (auto)##smmvfit0")) {
            curves.valueFitOverride = false;
        }
        static int sOutEase = 0; ///< 0=inherit 1..15=easing+1
        static int sInterp = 0;
        const char* inames = "Eased\0Hold/Step\0Linear\0Bezier (value cubic)\0\0";
        ImGui::TextUnformatted("Segment ease-out (leaving key; 0xFF=inherit) + interpolation (end key = active segment)");
        ImGui::SetNextItemWidth(200.f);
        ImGui::Combo("Interpolation##smmi", &sInterp, inames);
        sInterp = (std::min)(3, (std::max)(0, sInterp));
        static const char* kEaseOutMenu[] = {"Inherit (use next key ease-in on segment)", "Linear", "EaseIn", "EaseOut", "EaseInOut",
            "Bounce", "Elastic", "Back", "Circ", "Expo", "Quad", "Cubic", "Quart", "Quint", "Bezier"};
        ImGui::SetNextItemWidth(380.f);
        ImGui::Combo("Ease out##smmeo", &sOutEase, kEaseOutMenu, 15);
        if (ImGui::Button("Load from selected##smmsy")) {
            if (curves.selectedKeyIndex >= 0 && static_cast<size_t>(curves.selectedKeyIndex) < active.keys.size()) {
                sInterp = (std::min)(3, (std::max)(0, static_cast<int>(active.keys[static_cast<size_t>(curves.selectedKeyIndex)].interp)));
                sOutEase = (active.keys[static_cast<size_t>(curves.selectedKeyIndex)].easeOut == 0xFF)
                    ? 0
                    : 1 + static_cast<int>(active.keys[static_cast<size_t>(curves.selectedKeyIndex)].easeOut);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply out/ease+interp##smmi2") && ctx.scene) {
            std::vector<int> wk;
            if (!curves.selectedKeyIndices.empty()) {
                wk = curves.selectedKeyIndices;
            } else if (curves.selectedKeyIndex >= 0) {
                wk.push_back(curves.selectedKeyIndex);
            }
            if (!wk.empty() && sInterp >= 0 && sInterp <= 3) {
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                for (int kix : wk) {
                    uint64_t rt = 0;
                    float rv = 0.f;
                    uint8_t re = 0;
                    std::string ers;
                    if (!BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(kix), rt, rv, re, ers) || !ers.empty()) {
                        continue;
                    }
                    const uint8_t eOutB = (sOutEase == 0) ? uint8_t(0xFF) : static_cast<uint8_t>(sOutEase - 1);
                    if (binding.kind == TrackBindingKind::SceneChannel) {
                        Solstice::Parallax::SetKeyframeEaseOut(*ctx.scene, binding.index, rt, eOutB);
                        Solstice::Parallax::SetKeyframeInterpolation(
                            *ctx.scene, binding.index, rt, static_cast<Solstice::Parallax::KeyframeInterpolation>(sInterp));
                    } else {
                        Solstice::Parallax::SetMGKeyframeEaseOut(*ctx.scene, binding.index, rt, eOutB);
                        Solstice::Parallax::SetMGKeyframeInterpolation(
                            *ctx.scene, binding.index, rt, static_cast<Solstice::Parallax::KeyframeInterpolation>(sInterp));
                    }
                }
                *ctx.sceneDirty = true;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: set ease-out + interpolation on selected key(s).";
                }
            }
        }
        if (isFloatTrack) {
            static float sTin = 0.333f;
            static float sTout = 0.333f;
            ImGui::SliderFloat("Bezier tangent in##tgi", &sTin, 0.02f, 0.99f, "%.3f");
            ImGui::SameLine();
            ImGui::SliderFloat("tangent out##tgo", &sTout, 0.02f, 0.99f, "%.3f");
            ImGui::SameLine();
            if (ImGui::Button("Set tangents (Bezier)##tgset") && ctx.scene) {
                if (sInterp == 3 && (curves.selectedKeyIndex >= 0 || !curves.selectedKeyIndices.empty())) {
                    std::vector<int> wk;
                    if (!curves.selectedKeyIndices.empty()) {
                        wk = curves.selectedKeyIndices;
                    } else {
                        wk.push_back(curves.selectedKeyIndex);
                    }
                    Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                    for (int kix : wk) {
                        uint64_t rt = 0;
                        float rv = 0.f;
                        uint8_t re = 0;
                        std::string ers;
                        if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(kix), rt, rv, re, ers) && ers.empty()) {
                            if (binding.kind == TrackBindingKind::SceneChannel) {
                                Solstice::Parallax::SetKeyframeBezierTangents(*ctx.scene, binding.index, rt, sTout, sTin);
                            } else {
                                Solstice::Parallax::SetMGKeyframeBezierTangents(*ctx.scene, binding.index, rt, sTout, sTin);
                            }
                        }
                    }
                    *ctx.sceneDirty = true;
                }
            }
            if (ImGui::Button("Auto smooth (1/3 handles, Bezier+float)##autot") && ctx.scene) {
                std::vector<int> wk;
                if (!curves.selectedKeyIndices.empty()) {
                    wk = curves.selectedKeyIndices;
                } else if (curves.selectedKeyIndex >= 0) {
                    wk.push_back(curves.selectedKeyIndex);
                }
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                for (int kix : wk) {
                    uint64_t rt = 0;
                    float rv = 0.f;
                    uint8_t re = 0;
                    std::string ers;
                    if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(kix), rt, rv, re, ers) && ers.empty()) {
                        if (binding.kind == TrackBindingKind::SceneChannel) {
                            Solstice::Parallax::SetKeyframeInterpolation(
                                *ctx.scene, binding.index, rt, Solstice::Parallax::KeyframeInterpolation::Bezier);
                            Solstice::Parallax::SetKeyframeBezierTangents(
                                *ctx.scene, binding.index, rt, 1.f / 3.f, 1.f / 3.f);
                        } else {
                            Solstice::Parallax::SetMGKeyframeInterpolation(
                                *ctx.scene, binding.index, rt, Solstice::Parallax::KeyframeInterpolation::Bezier);
                            Solstice::Parallax::SetMGKeyframeBezierTangents(
                                *ctx.scene, binding.index, rt, 1.f / 3.f, 1.f / 3.f);
                        }
                    }
                }
                *ctx.sceneDirty = true;
            }
        }
    }
    ImGui::Separator();
    const ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 220.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1 = ImVec2(p0.x + canvasSize.x, p0.y + canvasSize.y);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 255));
    dl->AddRect(p0, p1, IM_COL32(80, 82, 96, 255));

    const float pad = 8.f;
    const ImVec2 plotMin = ImVec2(p0.x + pad, p0.y + pad);
    const ImVec2 plotMax = ImVec2(p1.x - pad, p1.y - pad);
    const ImVec2 plotSize = ImVec2(plotMax.x - plotMin.x, plotMax.y - plotMin.y);

    float vmin = ValueSpanMin(active, 0.f, &curves);
    float vmax = ValueSpanMax(active, 1.f, &curves);
    const float vcenter = 0.5f * (vmin + vmax);
    const float vhalf = 0.5f * (vmax - vmin) / curves.zoomY;
    vmin = vcenter - vhalf;
    vmax = vcenter + vhalf;

    const uint64_t dur = (std::max)(timeline.durationTicks, 1ull);
    for (int g = 0; g <= 4; ++g) {
        const float gx = plotMin.x + plotSize.x * (static_cast<float>(g) / 4.f);
        dl->AddLine(ImVec2(gx, plotMin.y), ImVec2(gx, plotMax.y), IM_COL32(50, 52, 64, 120), 1.f);
    }
    for (int g = 0; g <= 4; ++g) {
        const float gy = plotMin.y + plotSize.y * (static_cast<float>(g) / 4.f);
        dl->AddLine(ImVec2(plotMin.x, gy), ImVec2(plotMax.x, gy), IM_COL32(50, 52, 64, 120), 1.f);
    }

    if (ctx.timeTicks) {
        const float ph01 = PlayheadToPlotT01(ctx.timeTicks, dur, timeline);
        const float px = plotMin.x + std::clamp(ph01, 0.f, 1.f) * plotSize.x;
        dl->AddLine(ImVec2(px, plotMin.y), ImVec2(px, plotMax.y), IM_COL32(120, 170, 255, 200), 2.f);
    }

    std::sort(active.keys.begin(), active.keys.end(),
        [](const LibUI::CurveEditor::CurveKey& a, const LibUI::CurveEditor::CurveKey& b) { return a.time < b.time; });

    if (active.keys.size() >= 2) {
        constexpr int kSegSamples = 28;
        for (size_t i = 1; i < active.keys.size(); ++i) {
            const float va = active.keys[i - 1].value;
            const float vb = active.keys[i].value;
            const uint8_t eSeg = (active.keys[i - 1].easeOut != 0xFF) ? active.keys[i - 1].easeOut : active.keys[i].easing;
            const uint8_t iMode = active.keys[i].interp;
            const bool useBezierF = isFloatTrack && (iMode == 3u);
            const bool useHold = (iMode == 1u);
            const bool useLinear = (iMode == 2u);
            LibUI::CurveEditor::CurveKey probe;
            float prevT = 0.f;
            float prevV = 0.f;
            bool havePrev = false;
            for (int s = 0; s <= kSegSamples; ++s) {
                const float u = static_cast<float>(s) / static_cast<float>(kSegSamples);
                probe.time = active.keys[i - 1].time + (active.keys[i].time - active.keys[i - 1].time) * u;
                const float tplot = KeyTime01ToPlotX(probe, dur, timeline);
                float v = 0.f;
                if (useHold) {
                    v = (u < 1.f) ? va : vb;
                } else if (useLinear) {
                    v = va + (vb - va) * u;
                } else if (useBezierF) {
                    const float w0 = (std::clamp)(active.keys[i - 1].tangentOut, 0.02f, 0.99f);
                    const float w1 = (std::clamp)(active.keys[i].tangentIn, 0.02f, 0.99f);
                    const float dv = vb - va;
                    const float c0 = va + w0 * dv;
                    const float c1 = vb - w1 * dv;
                    v = CubicBezier1DValue(va, c0, c1, vb, u);
                } else {
                    const float w = Solstice::MinGfx::Ease(
                        u, static_cast<Solstice::MinGfx::EasingType>(eSeg), 1.f);
                    v = va + (vb - va) * w;
                }
                if (havePrev) {
                    const ImVec2 a = KeyToScreenPlotT(prevT, prevV, plotMin, plotSize, vmin, vmax);
                    const ImVec2 b = KeyToScreenPlotT(tplot, v, plotMin, plotSize, vmin, vmax);
                    dl->AddLine(a, b, IM_COL32(200, 200, 220, 200), 2.f);
                }
                havePrev = true;
                prevT = tplot;
                prevV = v;
            }
        }
    }

    for (size_t i = 0; i < active.keys.size(); ++i) {
        const float tPlot = KeyTime01ToPlotX(active.keys[i], dur, timeline);
        const ImVec2 c = KeyToScreenPlotT(tPlot, active.keys[i].value, plotMin, plotSize, vmin, vmax);
        const bool sel = !curves.selectedKeyIndices.empty()
            ? std::binary_search(
                  curves.selectedKeyIndices.begin(), curves.selectedKeyIndices.end(), static_cast<int>(i))
            : curves.selectedKeyIndex == static_cast<int>(i);
        dl->AddCircleFilled(c, sel ? 6.f : 5.f, IM_COL32(245, 184, 72, 255));
        dl->AddCircle(c, sel ? 6.f : 5.f, IM_COL32(40, 30, 10, 255), 0, 2.f);
    }

    ImGui::SetCursorScreenPos(p0);
    ImGui::InvisibleButton("curve_canvas", canvasSize);
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    static int dragKey = -1;
    static bool dragUndoPushed = false;
    if (ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        float bestD = 1e9f;
        int best = -1;
        for (size_t i = 0; i < active.keys.size(); ++i) {
            const float tPlot = KeyTime01ToPlotX(active.keys[i], dur, timeline);
            const ImVec2 c = KeyToScreenPlotT(tPlot, active.keys[i].value, plotMin, plotSize, vmin, vmax);
            const float d = std::sqrt((mouse.x - c.x) * (mouse.x - c.x) + (mouse.y - c.y) * (mouse.y - c.y));
            if (d < 10.f && d < bestD) {
                bestD = d;
                best = static_cast<int>(i);
            }
        }
        if (best >= 0) {
            if (ImGui::GetIO().KeyShift) {
                auto& m = curves.selectedKeyIndices;
                const auto it = std::find(m.begin(), m.end(), best);
                if (it != m.end()) {
                    m.erase(it);
                } else {
                    m.push_back(best);
                }
                std::sort(m.begin(), m.end());
                m.erase(std::unique(m.begin(), m.end()), m.end());
            } else {
                curves.selectedKeyIndices.clear();
                curves.selectedKeyIndex = best;
            }
            dragKey = best;
            dragUndoPushed = false;
        }
    }

    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        float t01 = 0.f;
        float val = 0.f;
        TimeValueFromScreen(mouse, plotMin, plotSize, vmin, vmax, t01, val);
        uint64_t tick = PlotXToSceneTick(t01, dur, timeline);
        if (ctx.keyframeEdit) {
            tick = SnapTimeToTick(*ctx.scene, *ctx.keyframeEdit, tick);
        }
        std::string err;
        Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
        if (BridgeAddKeyframeAtTick(*ctx.scene, binding, tick, val, err)) {
            *ctx.sceneDirty = true;
            if (ctx.statusLine) {
                *ctx.statusLine = "Curve: added keyframe.";
            }
            curves.selectedKeyIndex = -1;
            curves.selectedKeyIndices.clear();
        } else if (ctx.statusLine && !err.empty()) {
            *ctx.statusLine = err;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        dragKey = -1;
        dragUndoPushed = false;
    }

    if (dragKey >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && static_cast<size_t>(dragKey) < active.keys.size()) {
        if (!dragUndoPushed) {
            Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
            dragUndoPushed = true;
        }
        float t01 = 0.f;
        float val = 0.f;
        TimeValueFromScreen(mouse, plotMin, plotSize, vmin, vmax, t01, val);
        uint64_t newTick = PlotXToSceneTick(t01, dur, timeline);
        if (ctx.keyframeEdit) {
            newTick = SnapTimeToTick(*ctx.scene, *ctx.keyframeEdit, newTick);
        }
        std::string err;
        if (BridgeMoveKeyframe(*ctx.scene, binding, static_cast<size_t>(dragKey), newTick, val, err)) {
            *ctx.sceneDirty = true;
        } else if (ctx.statusLine && !err.empty()) {
            *ctx.statusLine = err;
        }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete) &&
        (!curves.selectedKeyIndices.empty() || curves.selectedKeyIndex >= 0)) {
        std::string err;
        Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
        if (!curves.selectedKeyIndices.empty()) {
            std::vector<uint64_t> byTick;
            byTick.reserve(curves.selectedKeyIndices.size());
            for (int idx : curves.selectedKeyIndices) {
                if (idx < 0) {
                    continue;
                }
                uint64_t rt = 0;
                float rv = 0.f;
                uint8_t re = 0;
                if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(idx), rt, rv, re, err)) {
                    byTick.push_back(rt);
                }
            }
            std::sort(byTick.begin(), byTick.end());
            byTick.erase(std::unique(byTick.begin(), byTick.end()), byTick.end());
            for (uint64_t tick : byTick) {
                if (binding.kind == TrackBindingKind::SceneChannel) {
                    Solstice::Parallax::RemoveKeyframe(*ctx.scene, binding.index, tick);
                } else {
                    Solstice::Parallax::RemoveMGKeyframe(*ctx.scene, binding.index, tick);
                }
                *ctx.sceneDirty = true;
            }
            curves.selectedKeyIndices.clear();
            curves.selectedKeyIndex = -1;
            if (ctx.statusLine) {
                *ctx.statusLine = "Curve: deleted keyframes.";
            }
        } else if (curves.selectedKeyIndex >= 0) {
            if (BridgeDeleteKeyframe(*ctx.scene, binding, static_cast<size_t>(curves.selectedKeyIndex), err)) {
                *ctx.sceneDirty = true;
                curves.selectedKeyIndex = -1;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: deleted keyframe.";
                }
            }
        }
    }

    {
        const ImGuiIO& iok = ImGui::GetIO();
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && iok.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) &&
            curves.selectedKeyIndex >= 0) {
            uint64_t rt = 0;
            float rv = 0.f;
            uint8_t re = 0;
            std::string er;
            if (BridgeReadKeyframe(
                    *ctx.scene, binding, static_cast<size_t>(curves.selectedKeyIndex), rt, rv, re, er) && ctx.keyframeEdit) {
                KeyframeEditUiState& ke = *ctx.keyframeEdit;
                ke.clipValid = true;
                ke.clipTick = rt;
                ke.clipValue = rv;
                ke.clipEasing = re;
                ke.clipSourceTrack = curves.selectedChannel;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: keyframe copied (Ctrl+V pastes at playhead).";
                }
            } else if (ctx.statusLine && !er.empty()) {
                *ctx.statusLine = er;
            }
        }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && iok.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) &&
            ctx.keyframeEdit && ctx.keyframeEdit->clipValid && ctx.timeTicks) {
            KeyframeEditUiState& ke = *ctx.keyframeEdit;
            uint64_t at = *ctx.timeTicks;
            if (ke.snapEnabled) {
                at = SnapTimeToTick(*ctx.scene, ke, at);
            }
            at = (std::min)(at, dur);
            std::string err;
            Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
            if (BridgeAddKeyframeAtTickEasing(
                    *ctx.scene, binding, at, ke.clipValue, static_cast<Solstice::Parallax::EasingType>(ke.clipEasing), err)) {
                *ctx.sceneDirty = true;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: keyframe pasted at playhead.";
                }
            } else if (ctx.statusLine && !err.empty()) {
                *ctx.statusLine = err;
            }
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + 4.f));
    {
        static int batchNudgeTimeTicks = 0;
        static float batchNudgeValue = 0.f;
        static float batchTimeScale = 1.f;
        ImGui::TextUnformatted("Batch (Shift+click multi-select on canvas)");
        ImGui::SetNextItemWidth(160.f);
        ImGui::InputInt("Nudge time (ticks)##batchnudge", &batchNudgeTimeTicks);
        ImGui::SameLine();
        if (ImGui::Button("Apply nudge time##batch") && !curves.selectedKeyIndices.empty() && ctx.scene) {
            std::string errB;
            std::vector<std::tuple<uint64_t, float, uint8_t>> data;
            data.reserve(curves.selectedKeyIndices.size());
            for (int idx : curves.selectedKeyIndices) {
                if (idx < 0) {
                    continue;
                }
                uint64_t rt = 0;
                float rv = 0.f;
                uint8_t re = 0;
                if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(idx), rt, rv, re, errB)) {
                    data.push_back({rt, rv, re});
                }
            }
            if (!data.empty()) {
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                std::vector<uint64_t> tkill;
                for (const auto& t : data) {
                    tkill.push_back(std::get<0>(t));
                }
                std::sort(tkill.begin(), tkill.end());
                tkill.erase(std::unique(tkill.begin(), tkill.end()), tkill.end());
                for (auto it = tkill.rbegin(); it != tkill.rend(); ++it) {
                    if (binding.kind == TrackBindingKind::SceneChannel) {
                        Solstice::Parallax::RemoveKeyframe(*ctx.scene, binding.index, *it);
                    } else {
                        Solstice::Parallax::RemoveMGKeyframe(*ctx.scene, binding.index, *it);
                    }
                }
                for (const auto& [ot, v, re] : data) {
                    int64_t nt = static_cast<int64_t>(ot) + static_cast<int64_t>(batchNudgeTimeTicks);
                    if (nt < 0) {
                        nt = 0;
                    }
                    uint64_t ntu = static_cast<uint64_t>(nt);
                    ntu = (std::min)(ntu, (std::max)(dur, 1ull));
                    (void)BridgeAddKeyframeAtTickEasing(
                        *ctx.scene, binding, ntu, v, static_cast<Solstice::Parallax::EasingType>(re), errB);
                }
                *ctx.sceneDirty = true;
                curves.selectedKeyIndices.clear();
                curves.selectedKeyIndex = -1;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: nudged time on selected keyframes.";
                }
            } else if (ctx.statusLine && !errB.empty()) {
                *ctx.statusLine = errB;
            }
        }
        ImGui::SetNextItemWidth(120.f);
        ImGui::InputFloat("Nudge value##batchnudgev", &batchNudgeValue, 0.01f, 0.1f, "%.4f");
        ImGui::SameLine();
        if (ImGui::Button("Apply nudge value##batch") && !curves.selectedKeyIndices.empty() && ctx.scene) {
            Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
            for (int idx : curves.selectedKeyIndices) {
                if (idx < 0) {
                    continue;
                }
                uint64_t rt = 0;
                float rv = 0.f;
                uint8_t re = 0;
                std::string err2;
                if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(idx), rt, rv, re, err2)) {
                    (void)BridgeMoveKeyframe(
                        *ctx.scene, binding, static_cast<size_t>(idx), rt, rv + batchNudgeValue, err2);
                }
            }
            *ctx.sceneDirty = true;
            if (ctx.statusLine) {
                *ctx.statusLine = "Curve: nudged value on selected keyframes.";
            }
        }
        ImGui::SetNextItemWidth(120.f);
        ImGui::InputFloat("Scale time (pivot: playhead)##btsc", &batchTimeScale, 0.01f, 0.1f, "%.3f");
        ImGui::SameLine();
        if (ImGui::Button("Apply scale##batch") && !curves.selectedKeyIndices.empty() && ctx.scene && ctx.timeTicks) {
            const uint64_t pivot = *ctx.timeTicks;
            std::string errB2;
            std::vector<std::tuple<uint64_t, float, uint8_t>> data2;
            for (int idx : curves.selectedKeyIndices) {
                if (idx < 0) {
                    continue;
                }
                uint64_t rt = 0;
                float rv = 0.f;
                uint8_t re = 0;
                if (BridgeReadKeyframe(*ctx.scene, binding, static_cast<size_t>(idx), rt, rv, re, errB2)) {
                    data2.push_back({rt, rv, re});
                }
            }
            if (!data2.empty() && errB2.empty()) {
                Smm::PushSceneUndoSnapshot(*ctx.scene, ctx.compressPrlx);
                std::vector<uint64_t> tkill2;
                for (const auto& t : data2) {
                    tkill2.push_back(std::get<0>(t));
                }
                std::sort(tkill2.begin(), tkill2.end());
                tkill2.erase(std::unique(tkill2.begin(), tkill2.end()), tkill2.end());
                for (auto it = tkill2.rbegin(); it != tkill2.rend(); ++it) {
                    if (binding.kind == TrackBindingKind::SceneChannel) {
                        Solstice::Parallax::RemoveKeyframe(*ctx.scene, binding.index, *it);
                    } else {
                        Solstice::Parallax::RemoveMGKeyframe(*ctx.scene, binding.index, *it);
                    }
                }
                for (const auto& [ot, v, re] : data2) {
                    int64_t rel = static_cast<int64_t>(ot) - static_cast<int64_t>(pivot);
                    double s = static_cast<double>(rel) * static_cast<double>(batchTimeScale);
                    int64_t nt = static_cast<int64_t>(pivot) + static_cast<int64_t>(s);
                    if (nt < 0) {
                        nt = 0;
                    }
                    uint64_t ntu = static_cast<uint64_t>(nt);
                    ntu = (std::min)(ntu, (std::max)(dur, 1ull));
                    (void)BridgeAddKeyframeAtTickEasing(
                        *ctx.scene, binding, ntu, v, static_cast<Solstice::Parallax::EasingType>(re), errB2);
                }
                *ctx.sceneDirty = true;
                curves.selectedKeyIndices.clear();
                curves.selectedKeyIndex = -1;
                if (ctx.statusLine) {
                    *ctx.statusLine = "Curve: scaled keyframe times (pivot: playhead).";
                }
            } else if (ctx.statusLine && !errB2.empty()) {
                *ctx.statusLine = errB2;
            }
        }
    }
    ImGui::TextDisabled("LMB drag | dbl-clk add | Del | Ctrl+C / Ctrl+V (paste at playhead) | time snap: options above");

    ImGui::End();
}

} // namespace Smm::Editing
