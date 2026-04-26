#include "SmmCurveGraphBridge.hxx"

#include <Parallax/ParallaxScene.hxx>

#include <cstddef>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Smm::Editing {

namespace {

static bool BindingUsesFloatLane(Solstice::Parallax::AttributeType t, int component) {
    if (t == Solstice::Parallax::AttributeType::Float) {
        return component < 0;
    }
    if (t == Solstice::Parallax::AttributeType::Vec3) {
        return component >= 0 && component <= 2;
    }
    return false;
}

static bool KeyframeGetFloat(const Solstice::Parallax::AttributeValue& v, Solstice::Parallax::AttributeType t, int component,
    float& out) {
    if (t == Solstice::Parallax::AttributeType::Float) {
        if (const auto* f = std::get_if<float>(&v)) {
            out = *f;
            return true;
        }
    }
    if (t == Solstice::Parallax::AttributeType::Vec3 && component >= 0 && component <= 2) {
        if (const auto* vec = std::get_if<Solstice::Math::Vec3>(&v)) {
            out = (component == 0) ? vec->x : (component == 1) ? vec->y : vec->z;
            return true;
        }
    }
    return false;
}

static Solstice::Parallax::AttributeValue PatchOrCreateValue(const Solstice::Parallax::AttributeValue& prior,
    Solstice::Parallax::AttributeType t, int component, float value) {
    if (t == Solstice::Parallax::AttributeType::Float) {
        return Solstice::Parallax::AttributeValue{value};
    }
    Solstice::Math::Vec3 v{0.f, 0.f, 0.f};
    if (const auto* pv = std::get_if<Solstice::Math::Vec3>(&prior)) {
        v = *pv;
    }
    if (component == 0) {
        v.x = value;
    } else if (component == 1) {
        v.y = value;
    } else {
        v.z = value;
    }
    return Solstice::Parallax::AttributeValue{v};
}

static std::string MakeChannelTrackName(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::ChannelRecord& ch, int component) {
    const auto& els = scene.GetElements();
    const std::string elName =
        ch.Element < els.size() ? els[ch.Element].Name : ("Element " + std::to_string(ch.Element));
    std::string suffix;
    if (ch.ValueType == Solstice::Parallax::AttributeType::Vec3 && component >= 0 && component <= 2) {
        suffix = std::string(".") + ("XYZ"[component]);
    }
    return elName + "." + ch.AttributeName + suffix;
}

static std::string MakeMgTrackName(const Solstice::Parallax::MGTrackRecord& tr, int component) {
    std::string suffix;
    if (tr.ValueType == Solstice::Parallax::AttributeType::Vec3 && component >= 0 && component <= 2) {
        suffix = std::string(".") + ("XYZ"[component]);
    }
    return std::string("MG.") + tr.PropertyName + suffix;
}

static const Solstice::Parallax::ChannelRecord* GetChannel(const Solstice::Parallax::ParallaxScene& scene, uint32_t idx) {
    const auto& ch = scene.GetChannels();
    if (idx >= ch.size()) {
        return nullptr;
    }
    return &ch[idx];
}

static const Solstice::Parallax::MGTrackRecord* GetMgTrack(const Solstice::Parallax::ParallaxScene& scene, uint32_t idx) {
    const auto& tr = scene.GetMGTracks();
    if (idx >= tr.size()) {
        return nullptr;
    }
    return &tr[idx];
}

} // namespace

void BridgeSyncFromScene(const Solstice::Parallax::ParallaxScene& scene, LibUI::Timeline::TimelineState& timeline,
    LibUI::CurveEditor::CurveEditorState& curves, std::vector<EditorTrackBinding>& bindings) {
    const int prevSelTrack = timeline.selectedTrack;
    const uint64_t prevPlay = timeline.playheadTick;

    timeline.durationTicks = std::max<uint64_t>(scene.GetTimelineDurationTicks(), 1);
    timeline.playheadTick = std::min(prevPlay, timeline.durationTicks);
    LibUI::Timeline::TimelineClampNestedRange(timeline);
    timeline.tracks.clear();
    curves.channels.clear();
    curves.selectedKeyIndices.clear();
    curves.maxKeyframeCountHint = 0;
    bindings.clear();

    const auto& channels = scene.GetChannels();
    for (size_t i = 0; i < channels.size(); ++i) {
        const auto& ch = channels[i];
        if (!BindingUsesFloatLane(ch.ValueType, ch.ValueType == Solstice::Parallax::AttributeType::Vec3 ? 0 : -1)) {
            continue;
        }
        if (ch.ValueType == Solstice::Parallax::AttributeType::Float) {
            EditorTrackBinding b{TrackBindingKind::SceneChannel, static_cast<uint32_t>(i), -1};
            LibUI::Timeline::Track tr;
            tr.name = MakeChannelTrackName(scene, ch, -1);
            tr.keyTicks.reserve(ch.Keyframes.size());
            LibUI::CurveEditor::CurveChannel cc;
            cc.name = tr.name;
            cc.visible = true;
            cc.keys.reserve(ch.Keyframes.size());
            const float denom = static_cast<float>(std::max<uint64_t>(timeline.durationTicks, 1ull));
            for (const auto& kf : ch.Keyframes) {
                tr.keyTicks.push_back(kf.TimeTicks);
                float v = 0.f;
                (void)KeyframeGetFloat(kf.Value, ch.ValueType, -1, v);
                cc.keys.push_back(LibUI::CurveEditor::CurveKey{static_cast<float>(kf.TimeTicks) / denom, v, kf.Easing,
                    kf.EaseOut, kf.Interp, kf.TangentIn, kf.TangentOut});
            }
            timeline.tracks.push_back(std::move(tr));
            curves.channels.push_back(std::move(cc));
            bindings.push_back(b);
        } else if (ch.ValueType == Solstice::Parallax::AttributeType::Vec3) {
            for (int comp = 0; comp < 3; ++comp) {
                EditorTrackBinding b{TrackBindingKind::SceneChannel, static_cast<uint32_t>(i), comp};
                LibUI::Timeline::Track tr;
                tr.name = MakeChannelTrackName(scene, ch, comp);
                tr.keyTicks.reserve(ch.Keyframes.size());
                LibUI::CurveEditor::CurveChannel cc;
                cc.name = tr.name;
                cc.visible = true;
                cc.keys.reserve(ch.Keyframes.size());
                const float denom = static_cast<float>(std::max<uint64_t>(timeline.durationTicks, 1ull));
                for (const auto& kf : ch.Keyframes) {
                    tr.keyTicks.push_back(kf.TimeTicks);
                    float v = 0.f;
                    (void)KeyframeGetFloat(kf.Value, ch.ValueType, comp, v);
                    cc.keys.push_back(LibUI::CurveEditor::CurveKey{static_cast<float>(kf.TimeTicks) / denom, v, kf.Easing,
                        kf.EaseOut, kf.Interp, kf.TangentIn, kf.TangentOut});
                }
                timeline.tracks.push_back(std::move(tr));
                curves.channels.push_back(std::move(cc));
                bindings.push_back(b);
            }
        }
    }

    const auto& mgTracks = scene.GetMGTracks();
    for (size_t i = 0; i < mgTracks.size(); ++i) {
        const auto& tr = mgTracks[i];
        if (!BindingUsesFloatLane(tr.ValueType, tr.ValueType == Solstice::Parallax::AttributeType::Vec3 ? 0 : -1)) {
            continue;
        }
        if (tr.ValueType == Solstice::Parallax::AttributeType::Float) {
            EditorTrackBinding b{TrackBindingKind::MgTrack, static_cast<uint32_t>(i), -1};
            LibUI::Timeline::Track trow;
            trow.name = MakeMgTrackName(tr, -1);
            trow.keyTicks.reserve(tr.Keyframes.size());
            LibUI::CurveEditor::CurveChannel cc;
            cc.name = trow.name;
            cc.visible = true;
            cc.keys.reserve(tr.Keyframes.size());
            const float denom = static_cast<float>(std::max<uint64_t>(timeline.durationTicks, 1ull));
            for (const auto& kf : tr.Keyframes) {
                trow.keyTicks.push_back(kf.TimeTicks);
                float v = 0.f;
                (void)KeyframeGetFloat(kf.Value, tr.ValueType, -1, v);
                cc.keys.push_back(LibUI::CurveEditor::CurveKey{static_cast<float>(kf.TimeTicks) / denom, v, kf.Easing,
                    kf.EaseOut, kf.Interp, kf.TangentIn, kf.TangentOut});
            }
            timeline.tracks.push_back(std::move(trow));
            curves.channels.push_back(std::move(cc));
            bindings.push_back(b);
        } else if (tr.ValueType == Solstice::Parallax::AttributeType::Vec3) {
            for (int comp = 0; comp < 3; ++comp) {
                EditorTrackBinding b{TrackBindingKind::MgTrack, static_cast<uint32_t>(i), comp};
                LibUI::Timeline::Track trow;
                trow.name = MakeMgTrackName(tr, comp);
                trow.keyTicks.reserve(tr.Keyframes.size());
                LibUI::CurveEditor::CurveChannel cc;
                cc.name = trow.name;
                cc.visible = true;
                cc.keys.reserve(tr.Keyframes.size());
                const float denom = static_cast<float>(std::max<uint64_t>(timeline.durationTicks, 1ull));
                for (const auto& kf : tr.Keyframes) {
                    trow.keyTicks.push_back(kf.TimeTicks);
                    float v = 0.f;
                    (void)KeyframeGetFloat(kf.Value, tr.ValueType, comp, v);
                    cc.keys.push_back(LibUI::CurveEditor::CurveKey{static_cast<float>(kf.TimeTicks) / denom, v, kf.Easing,
                        kf.EaseOut, kf.Interp, kf.TangentIn, kf.TangentOut});
                }
                timeline.tracks.push_back(std::move(trow));
                curves.channels.push_back(std::move(cc));
                bindings.push_back(b);
            }
        }
    }

    for (const auto& cc : curves.channels) {
        curves.maxKeyframeCountHint = (std::max)(curves.maxKeyframeCountHint, cc.keys.size());
    }

    if (timeline.tracks.empty()) {
        timeline.selectedTrack = -1;
        curves.selectedChannel = -1;
        curves.selectedKeyIndex = -1;
        return;
    }

    if (prevSelTrack >= 0 && prevSelTrack < static_cast<int>(timeline.tracks.size())) {
        timeline.selectedTrack = prevSelTrack;
    } else {
        timeline.selectedTrack = 0;
    }
    curves.selectedChannel = timeline.selectedTrack;
    if (curves.selectedKeyIndex >= static_cast<int>(curves.channels[static_cast<size_t>(curves.selectedChannel)].keys.size())) {
        curves.selectedKeyIndex = -1;
    }
}

bool BridgeMoveKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    uint64_t newTick, float newValue, std::string& errOut) {
    errOut.clear();
    newTick = std::min(newTick, std::max<uint64_t>(scene.GetTimelineDurationTicks(), 1ull));

    if (binding.kind == TrackBindingKind::SceneChannel) {
        const Solstice::Parallax::ChannelRecord* ch = GetChannel(scene, binding.index);
        if (!ch) {
            errOut = "Invalid channel index.";
            return false;
        }
        if (!BindingUsesFloatLane(ch->ValueType, binding.component)) {
            errOut = "Unsupported channel type for curve edit.";
            return false;
        }
        auto& kfs = scene.GetChannels()[binding.index].Keyframes;
        if (sortedKeyIndex >= kfs.size()) {
            errOut = "Keyframe index out of range.";
            return false;
        }
        std::vector<size_t> order(kfs.size());
        for (size_t i = 0; i < kfs.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
        const size_t realIdx = order[sortedKeyIndex];
        const uint64_t oldTick = kfs[realIdx].TimeTicks;
        const Solstice::Parallax::AttributeValue patched =
            PatchOrCreateValue(kfs[realIdx].Value, ch->ValueType, binding.component, newValue);
        const uint8_t easing = kfs[realIdx].Easing;
        const uint8_t eOut = kfs[realIdx].EaseOut;
        const uint8_t interp = kfs[realIdx].Interp;
        const float tOut = kfs[realIdx].TangentOut;
        const float tIn = kfs[realIdx].TangentIn;
        Solstice::Parallax::RemoveKeyframe(scene, binding.index, oldTick);
        Solstice::Parallax::AddKeyframe(scene, binding.index, newTick, patched, static_cast<Solstice::Parallax::EasingType>(easing));
        Solstice::Parallax::SetKeyframeEaseOut(scene, binding.index, newTick, eOut);
        Solstice::Parallax::SetKeyframeInterpolation(
            scene, binding.index, newTick, static_cast<Solstice::Parallax::KeyframeInterpolation>(interp));
        Solstice::Parallax::SetKeyframeBezierTangents(scene, binding.index, newTick, tOut, tIn);
        return true;
    }

    const Solstice::Parallax::MGTrackRecord* tr = GetMgTrack(scene, binding.index);
    if (!tr) {
        errOut = "Invalid MG track index.";
        return false;
    }
    if (!BindingUsesFloatLane(tr->ValueType, binding.component)) {
        errOut = "Unsupported MG track type for curve edit.";
        return false;
    }
    auto& kfs = scene.GetMGTracks()[binding.index].Keyframes;
    if (sortedKeyIndex >= kfs.size()) {
        errOut = "MG keyframe index out of range.";
        return false;
    }
    std::vector<size_t> order(kfs.size());
    for (size_t i = 0; i < kfs.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
    const size_t realIdx = order[sortedKeyIndex];
    const uint64_t oldTick = kfs[realIdx].TimeTicks;
    const Solstice::Parallax::AttributeValue patched =
        PatchOrCreateValue(kfs[realIdx].Value, tr->ValueType, binding.component, newValue);
    const uint8_t easing = kfs[realIdx].Easing;
    const uint8_t eOut = kfs[realIdx].EaseOut;
    const uint8_t interp = kfs[realIdx].Interp;
    const float tOut = kfs[realIdx].TangentOut;
    const float tIn = kfs[realIdx].TangentIn;
    Solstice::Parallax::RemoveMGKeyframe(scene, binding.index, oldTick);
    Solstice::Parallax::AddMGKeyframe(
        scene, binding.index, newTick, patched, static_cast<Solstice::Parallax::EasingType>(easing));
    Solstice::Parallax::SetMGKeyframeEaseOut(scene, binding.index, newTick, eOut);
    Solstice::Parallax::SetMGKeyframeInterpolation(
        scene, binding.index, newTick, static_cast<Solstice::Parallax::KeyframeInterpolation>(interp));
    Solstice::Parallax::SetMGKeyframeBezierTangents(scene, binding.index, newTick, tOut, tIn);
    return true;
}

bool BridgeAddKeyframeAtTick(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, uint64_t tick,
    float value, std::string& errOut) {
    errOut.clear();
    tick = std::min(tick, std::max<uint64_t>(scene.GetTimelineDurationTicks(), 1ull));
    if (binding.kind == TrackBindingKind::SceneChannel) {
        const Solstice::Parallax::ChannelRecord* ch = GetChannel(scene, binding.index);
        if (!ch) {
            errOut = "Invalid channel.";
            return false;
        }
        Solstice::Parallax::AttributeValue prior = std::monostate{};
        if (!ch->Keyframes.empty()) {
            prior = ch->Keyframes.back().Value;
        } else {
            const Solstice::Parallax::AttributeValue base =
                Solstice::Parallax::GetAttribute(scene, ch->Element, ch->AttributeName);
            prior = base;
        }
        const Solstice::Parallax::AttributeValue v = PatchOrCreateValue(prior, ch->ValueType, binding.component, value);
        Solstice::Parallax::AddKeyframe(scene, binding.index, tick, v);
        return true;
    }
    const Solstice::Parallax::MGTrackRecord* tr = GetMgTrack(scene, binding.index);
    if (!tr) {
        errOut = "Invalid MG track.";
        return false;
    }
    Solstice::Parallax::AttributeValue prior = std::monostate{};
    if (!tr->Keyframes.empty()) {
        prior = tr->Keyframes.back().Value;
    }
    const Solstice::Parallax::AttributeValue v = PatchOrCreateValue(prior, tr->ValueType, binding.component, value);
    Solstice::Parallax::AddMGKeyframe(scene, binding.index, tick, v, Solstice::Parallax::EasingType::Linear);
    return true;
}

bool BridgeAddKeyframeAtTickEasing(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, uint64_t tick,
    float value, Solstice::Parallax::EasingType ease, std::string& errOut) {
    errOut.clear();
    tick = std::min(tick, std::max<uint64_t>(scene.GetTimelineDurationTicks(), 1ull));
    if (binding.kind == TrackBindingKind::SceneChannel) {
        const Solstice::Parallax::ChannelRecord* ch = GetChannel(scene, binding.index);
        if (!ch) {
            errOut = "Invalid channel.";
            return false;
        }
        Solstice::Parallax::AttributeValue prior = std::monostate{};
        if (!ch->Keyframes.empty()) {
            prior = ch->Keyframes.back().Value;
        } else {
            const Solstice::Parallax::AttributeValue base =
                Solstice::Parallax::GetAttribute(scene, ch->Element, ch->AttributeName);
            prior = base;
        }
        const Solstice::Parallax::AttributeValue v = PatchOrCreateValue(prior, ch->ValueType, binding.component, value);
        Solstice::Parallax::AddKeyframe(scene, binding.index, tick, v, ease);
        return true;
    }
    const Solstice::Parallax::MGTrackRecord* tr = GetMgTrack(scene, binding.index);
    if (!tr) {
        errOut = "Invalid MG track.";
        return false;
    }
    Solstice::Parallax::AttributeValue prior = std::monostate{};
    if (!tr->Keyframes.empty()) {
        prior = tr->Keyframes.back().Value;
    }
    const Solstice::Parallax::AttributeValue v = PatchOrCreateValue(prior, tr->ValueType, binding.component, value);
    Solstice::Parallax::AddMGKeyframe(scene, binding.index, tick, v, ease);
    return true;
}

bool BridgeDeleteKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    std::string& errOut) {
    errOut.clear();
    if (binding.kind == TrackBindingKind::SceneChannel) {
        const auto* ch = GetChannel(scene, binding.index);
        if (!ch || sortedKeyIndex >= ch->Keyframes.size()) {
            errOut = "Invalid key.";
            return false;
        }
        auto& kfs = scene.GetChannels()[binding.index].Keyframes;
        std::vector<size_t> order(kfs.size());
        for (size_t i = 0; i < kfs.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
        const uint64_t tick = kfs[order[sortedKeyIndex]].TimeTicks;
        Solstice::Parallax::RemoveKeyframe(scene, binding.index, tick);
        return true;
    }
    const auto* tr = GetMgTrack(scene, binding.index);
    if (!tr || sortedKeyIndex >= tr->Keyframes.size()) {
        errOut = "Invalid MG key.";
        return false;
    }
    auto& kfs = scene.GetMGTracks()[binding.index].Keyframes;
    std::vector<size_t> order(kfs.size());
    for (size_t i = 0; i < kfs.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
    const uint64_t tick = kfs[order[sortedKeyIndex]].TimeTicks;
    Solstice::Parallax::RemoveMGKeyframe(scene, binding.index, tick);
    return true;
}

bool BridgeReadKeyframe(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    uint64_t& outTick, float& outValue, uint8_t& outEasing, std::string& errOut) {
    errOut.clear();
    if (binding.kind == TrackBindingKind::SceneChannel) {
        const auto* ch = GetChannel(scene, binding.index);
        if (!ch || sortedKeyIndex >= ch->Keyframes.size()) {
            errOut = "Invalid key.";
            return false;
        }
        const auto& kfs = scene.GetChannels()[binding.index].Keyframes;
        std::vector<size_t> order(kfs.size());
        for (size_t i = 0; i < kfs.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
        const size_t realIdx = order[sortedKeyIndex];
        outTick = kfs[realIdx].TimeTicks;
        if (!KeyframeGetFloat(kfs[realIdx].Value, ch->ValueType, binding.component, outValue)) {
            errOut = "Could not read keyframe value.";
            return false;
        }
        outEasing = kfs[realIdx].Easing;
        return true;
    }
    const auto* tr = GetMgTrack(scene, binding.index);
    if (!tr || sortedKeyIndex >= tr->Keyframes.size()) {
        errOut = "Invalid MG key.";
        return false;
    }
    const auto& kfs = scene.GetMGTracks()[binding.index].Keyframes;
    std::vector<size_t> order(kfs.size());
    for (size_t i = 0; i < kfs.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
    const size_t realIdx = order[sortedKeyIndex];
    outTick = kfs[realIdx].TimeTicks;
    if (!KeyframeGetFloat(kfs[realIdx].Value, tr->ValueType, binding.component, outValue)) {
        errOut = "Could not read MG keyframe value.";
        return false;
    }
    outEasing = kfs[realIdx].Easing;
    return true;
}

bool BridgeSetKeyframeEasing(Solstice::Parallax::ParallaxScene& scene, const EditorTrackBinding& binding, size_t sortedKeyIndex,
    Solstice::Parallax::EasingType ease, std::string& errOut) {
    errOut.clear();
    if (binding.kind == TrackBindingKind::SceneChannel) {
        const auto* ch = GetChannel(scene, binding.index);
        if (!ch || sortedKeyIndex >= ch->Keyframes.size()) {
            errOut = "Invalid key.";
            return false;
        }
        if (!BindingUsesFloatLane(ch->ValueType, binding.component)) {
            errOut = "Unsupported channel for easing edit.";
            return false;
        }
        const auto& kfs = scene.GetChannels()[binding.index].Keyframes;
        std::vector<size_t> order(kfs.size());
        for (size_t i = 0; i < kfs.size(); ++i) {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
        const uint64_t tick = kfs[order[sortedKeyIndex]].TimeTicks;
        Solstice::Parallax::SetKeyframeEasing(scene, binding.index, tick, ease);
        return true;
    }
    const auto* tr = GetMgTrack(scene, binding.index);
    if (!tr || sortedKeyIndex >= tr->Keyframes.size()) {
        errOut = "Invalid MG key.";
        return false;
    }
    if (!BindingUsesFloatLane(tr->ValueType, binding.component)) {
        errOut = "Unsupported MG track for easing edit.";
        return false;
    }
    const auto& kfs = scene.GetMGTracks()[binding.index].Keyframes;
    std::vector<size_t> order(kfs.size());
    for (size_t i = 0; i < kfs.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return kfs[a].TimeTicks < kfs[b].TimeTicks; });
    const uint64_t tick = kfs[order[sortedKeyIndex]].TimeTicks;
    Solstice::Parallax::SetMGKeyframeEasing(scene, binding.index, tick, ease);
    return true;
}

} // namespace Smm::Editing
