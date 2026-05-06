#include <Parallax/ParallaxScene.hxx>

#include <Arzachel/FacialAnimation.hxx>
#include <Arzachel/Seed.hxx>

#include <MinGfx/EasingFunction.hxx>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Solstice::Parallax {

namespace {

/// Composite key (element index << 32) | hash(attribute name) used by `ScanFromScene` to build a
/// per-call index without holding non-owning string_view references across calls. The collision
/// risk is bounded by the simple wide-hash and explicit fallback to the linear scan when the
/// builder isn't constructed (small/old call sites).
struct ChannelIndexMap {
    std::unordered_map<std::uint64_t, ChannelIndex> ByElementAttr;

    static std::uint64_t MakeKey(ElementIndex e, std::string_view attr) noexcept {
        std::uint64_t h = 1469598103934665603ull; // FNV-1a 64
        for (char c : attr) {
            h ^= static_cast<std::uint8_t>(c);
            h *= 1099511628211ull;
        }
        h ^= static_cast<std::uint64_t>(e) * 0x9E3779B97F4A7C15ull;
        return h;
    }

    void ScanFromScene(const ParallaxScene& scene) {
        const auto& channels = scene.GetChannels();
        ByElementAttr.clear();
        ByElementAttr.reserve(channels.size() * 2);
        for (ChannelIndex ci = 0; ci < channels.size(); ++ci) {
            ByElementAttr.emplace(MakeKey(channels[ci].Element, channels[ci].AttributeName), ci);
        }
    }

    ChannelIndex Find(ElementIndex element, std::string_view attributeName) const noexcept {
        const auto it = ByElementAttr.find(MakeKey(element, attributeName));
        if (it == ByElementAttr.end()) {
            return PARALLAX_INVALID_INDEX;
        }
        return it->second;
    }
};

ChannelIndex FindElementChannel(const ParallaxScene& scene, ElementIndex element, std::string_view attributeName) {
    const auto& channels = scene.GetChannels();
    for (ChannelIndex ci = 0; ci < channels.size(); ++ci) {
        if (channels[ci].Element == element && channels[ci].AttributeName == attributeName) {
            return ci;
        }
    }
    return PARALLAX_INVALID_INDEX;
}

ChannelIndex FindElementChannelIndexed(const ChannelIndexMap& idx, const ParallaxScene& scene, ElementIndex element,
    std::string_view attributeName) {
    if (idx.ByElementAttr.empty()) {
        return FindElementChannel(scene, element, attributeName);
    }
    return idx.Find(element, attributeName);
}

float SampleEased(float t01, uint8_t easingByte) {
    auto e = static_cast<MinGfx::EasingType>(easingByte);
    return MinGfx::Ease(t01, e, 1.0f);
}

template<typename T>
T Lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

AttributeValue LerpValue(AttributeType ty, const AttributeValue& a, const AttributeValue& b, float t) {
    switch (ty) {
    case AttributeType::Float: {
        float fa = std::get_if<float>(&a) ? *std::get_if<float>(&a) : 0.f;
        float fb = std::get_if<float>(&b) ? *std::get_if<float>(&b) : 0.f;
        return fa + (fb - fa) * t;
    }
    case AttributeType::Vec3: {
        Math::Vec3 va = std::get_if<Math::Vec3>(&a) ? *std::get_if<Math::Vec3>(&a) : Math::Vec3{};
        Math::Vec3 vb = std::get_if<Math::Vec3>(&b) ? *std::get_if<Math::Vec3>(&b) : Math::Vec3{};
        return Math::Vec3(Lerp(va.x, vb.x, t), Lerp(va.y, vb.y, t), Lerp(va.z, vb.z, t));
    }
    case AttributeType::Vec4:
    case AttributeType::ColorRGBA: {
        Math::Vec4 va = std::get_if<Math::Vec4>(&a) ? *std::get_if<Math::Vec4>(&a) : Math::Vec4{};
        Math::Vec4 vb = std::get_if<Math::Vec4>(&b) ? *std::get_if<Math::Vec4>(&b) : Math::Vec4{};
        return Math::Vec4(Lerp(va.x, vb.x, t), Lerp(va.y, vb.y, t), Lerp(va.z, vb.z, t), Lerp(va.w, vb.w, t));
    }
    default:
        return t < 0.5f ? a : b;
    }
}

static uint8_t SegmentEaseInByte(const KeyframeRecord& k0, const KeyframeRecord& k1) {
    return (k0.EaseOut != 0xFF) ? k0.EaseOut : k1.Easing;
}

static float FloatVal(const AttributeValue& v) {
    if (const auto* f = std::get_if<float>(&v)) {
        return *f;
    }
    return 0.f;
}

static float CubicBezier1DValue(float p0, float c0, float c1, float p1, float u) {
    u = (std::clamp)(u, 0.f, 1.f);
    const float o = 1.f - u;
    return o * o * o * p0 + 3.f * o * o * u * c0 + 3.f * o * u * u * c1 + u * u * u * p1;
}

static AttributeValue InterpolatePair(AttributeType ty, const KeyframeRecord& k0, const KeyframeRecord& k1, uint64_t timeTicks) {
    if (k1.TimeTicks == k0.TimeTicks) {
        return k1.Value;
    }
    const uint64_t t0 = k0.TimeTicks;
    const uint64_t t1 = k1.TimeTicks;
    if (timeTicks < t0) {
        return k0.Value;
    }
    if (timeTicks > t1) {
        return k1.Value;
    }
    const double span = static_cast<double>(t1 - t0);
    const float u = span > 0.0 ? static_cast<float>(static_cast<double>(timeTicks - t0) / span) : 0.f;
    const auto i1 = static_cast<KeyframeInterpolation>(k1.Interp);
    if (i1 == KeyframeInterpolation::Hold) {
        if (timeTicks < t1) {
            return k0.Value;
        }
        return k1.Value;
    }
    if (i1 == KeyframeInterpolation::Linear) {
        return LerpValue(ty, k0.Value, k1.Value, u);
    }
    if (ty == AttributeType::Float && i1 == KeyframeInterpolation::Bezier) {
        const float p0 = FloatVal(k0.Value);
        const float p1 = FloatVal(k1.Value);
        const float w0 = (std::clamp)(k0.TangentOut, 0.02f, 0.99f);
        const float w1 = (std::clamp)(k1.TangentIn, 0.02f, 0.99f);
        const float dv = p1 - p0;
        const float c0 = p0 + w0 * dv;
        const float c1 = p1 - w1 * dv;
        const float ev = CubicBezier1DValue(p0, c0, c1, p1, u);
        return AttributeValue{ev};
    }
    {
        const uint8_t e = SegmentEaseInByte(k0, k1);
        const float t01 = SampleEased(u, e);
        return LerpValue(ty, k0.Value, k1.Value, t01);
    }
}

static MGDisplayList::Entry BuildMGEval(
    const ParallaxScene& scene, const MGElementRecord& mg, const std::string_view& schemaName, uint64_t timeTicks) {
    MGDisplayList::Entry e;
    e.SchemaType = std::string(schemaName);
    e.Blend = BlendMode::Over;
    e.Alpha = 1.0f;
    for (const auto& kv : mg.Attributes) {
        e.Attributes[kv.first] = kv.second;
    }
    const uint32_t t0 = mg.FirstTrackIndex;
    for (uint32_t ti = 0; ti < mg.TrackCount && t0 + ti < scene.GetMGTracks().size(); ++ti) {
        const auto& tr = scene.GetMGTracks()[t0 + ti];
        AttributeValue v = std::monostate{};
        if (!tr.Keyframes.empty()) {
            const auto& kfs = tr.Keyframes;
            if (kfs.size() == 1) {
                v = kfs[0].Value;
            } else {
                auto it = std::lower_bound(kfs.begin(), kfs.end(), timeTicks,
                    [](const KeyframeRecord& a, uint64_t t) { return a.TimeTicks < t; });
                if (it == kfs.end()) {
                    const auto& k0 = kfs[kfs.size() - 2];
                    const auto& k1 = kfs[kfs.size() - 1];
                    v = InterpolatePair(tr.ValueType, k0, k1, timeTicks);
                } else if (it == kfs.begin()) {
                    v = kfs[0].Value;
                } else {
                    const auto& k1 = *it;
                    const auto& k0 = *(it - 1);
                    v = InterpolatePair(tr.ValueType, k0, k1, timeTicks);
                }
            }
        }
        e.Attributes[tr.PropertyName] = std::move(v);
    }
    return e;
}

static float AttrFloatInEntry(const MGDisplayList::Entry& e, const char* k, float d) {
    const auto it = e.Attributes.find(k);
    if (it == e.Attributes.end()) {
        return d;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return d;
}

static float MgAttrFloat(const MGElementRecord& mg, std::string_view key, float fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return fallback;
}

static int32_t MgAttrInt(const MGElementRecord& mg, std::string_view key, int32_t fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<int32_t>(&it->second)) {
        return *v;
    }
    return fallback;
}

static bool MgAttrBool(const MGElementRecord& mg, std::string_view key, bool fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<bool>(&it->second)) {
        return *v;
    }
    return fallback;
}

static Math::Vec3 MgAttrVec3(const MGElementRecord& mg, std::string_view key, const Math::Vec3& fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<Math::Vec3>(&it->second)) {
        return *v;
    }
    return fallback;
}

static Math::Vec4 MgAttrVec4(const MGElementRecord& mg, std::string_view key, const Math::Vec4& fallback) {
    const auto it = mg.Attributes.find(std::string(key));
    if (it == mg.Attributes.end()) {
        return fallback;
    }
    if (const auto* v = std::get_if<Math::Vec4>(&it->second)) {
        return *v;
    }
    return fallback;
}

static MGDisplayList EvaluateMGImpl(const ParallaxScene& scene, uint64_t timeTicks) {
    const auto& schemas = scene.GetSchemas();
    const auto& mgs = scene.GetMGElements();
    uint64_t evalTick = timeTicks;
    for (const auto& mg : mgs) {
        if (mg.SchemaIndex >= schemas.size()) {
            continue;
        }
        if (schemas[mg.SchemaIndex].TypeName != "MotionGraphicsRootElement") {
            continue;
        }
        const MGDisplayList::Entry e = BuildMGEval(
            scene, mg, std::string_view(schemas[mg.SchemaIndex].TypeName), timeTicks);
        const float s = (std::max)(0.f, AttrFloatInEntry(e, "MGTimeScale", 1.f));
        const double t = static_cast<double>(timeTicks) * static_cast<double>(s);
        evalTick = t <= 0.0 ? 0u : (t >= static_cast<double>(UINT64_MAX) ? UINT64_MAX : static_cast<uint64_t>(t));
        break;
    }
    MGDisplayList list;
    list.CompositeMode = scene.GetMGCompositeMode();
    list.GlobalAlpha = scene.GetMGGlobalAlpha();
    for (const auto& mg : mgs) {
        if (mg.SchemaIndex >= schemas.size()) {
            list.Entries.push_back(MGDisplayList::Entry{});
            continue;
        }
        const std::string_view st = std::string_view(schemas[mg.SchemaIndex].TypeName);
        list.Entries.push_back(BuildMGEval(scene, mg, st, evalTick));
    }
    for (const auto& e : list.Entries) {
        if (e.SchemaType != "MotionGraphicsRootElement") {
            continue;
        }
        const float ax = (std::max)(0.f, AttrFloatInEntry(e, "ScreenShakeAmpX", 0.f));
        const float ay = (std::max)(0.f, AttrFloatInEntry(e, "ScreenShakeAmpY", 0.f));
        const float fr = AttrFloatInEntry(e, "ScreenShakeFrequency", 1.f);
        const float ph = AttrFloatInEntry(e, "ScreenShakePhase", 0.f);
        const float t = static_cast<float>(timeTicks) * 0.01f;
        list.Post.ScreenShakeX = ax * std::sin(fr * t + ph);
        list.Post.ScreenShakeY = ay * std::cos(fr * t + ph * 0.5f);
        list.Post.ChromaticAberrationPx = (std::max)(0.f, AttrFloatInEntry(e, "ChromaticAberration", 0.f));
        list.Post.GradeExposure = (std::max)(0.f, AttrFloatInEntry(e, "GradeExposure", 1.f));
        list.Post.GradeSaturation = (std::max)(0.f, AttrFloatInEntry(e, "GradeSaturation", 1.f));
        list.Post.GradeContrast = (std::max)(0.01f, AttrFloatInEntry(e, "GradeContrast", 1.f));
        list.Post.GradeLift = AttrFloatInEntry(e, "GradeLift", 0.f);
        break;
    }
    return list;
}

} // namespace

AttributeValue EvaluateChannel(const ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks) {
    if (channel == PARALLAX_INVALID_INDEX || channel >= scene.GetChannels().size()) {
        return std::monostate{};
    }
    const auto& ch = scene.GetChannels()[channel];
    if (ch.Keyframes.empty()) {
        return std::monostate{};
    }
    if (ch.Keyframes.size() == 1) {
        return ch.Keyframes[0].Value;
    }
    const auto& kfs = ch.Keyframes;
    if (timeTicks <= kfs.front().TimeTicks) {
        return kfs.front().Value;
    }
    if (timeTicks >= kfs.back().TimeTicks) {
        return kfs.back().Value;
    }
    // Binary search: keyframes are stored in ascending TimeTicks order.
    auto it = std::lower_bound(kfs.begin(), kfs.end(), timeTicks,
        [](const KeyframeRecord& a, uint64_t t) { return a.TimeTicks < t; });
    if (it == kfs.begin()) {
        return kfs.front().Value;
    }
    if (it == kfs.end()) {
        const auto& k0 = kfs[kfs.size() - 2];
        const auto& k1 = kfs[kfs.size() - 1];
        return InterpolatePair(ch.ValueType, k0, k1, timeTicks);
    }
    const auto& k1 = *it;
    const auto& k0 = *(it - 1);
    return InterpolatePair(ch.ValueType, k0, k1, timeTicks);
}

void EvaluateScene(const ParallaxScene& scene, uint64_t timeTicks, SceneEvaluationResult& outResult) {
    outResult.ElementTransforms.clear();
    outResult.LightStates.clear();
    outResult.AudioStates.clear();
    outResult.FluidVolumes.clear();
    outResult.MGWorldSprites.clear();
    outResult.Vehicles.clear();
    outResult.ScriptOutputs.clear();
    outResult.EnvironmentSkybox.reset();
    outResult.ActorArzachelAuthorings.clear();
    outResult.ActorFacialPoses.clear();
    outResult.MotionGraphics = EvaluateMG(scene, timeTicks);

    // Build a single (Element, AttributeName) -> ChannelIndex map for the duration of this call.
    // Eliminates repeated O(N_channels) scans inside facial sampling on dense scenes.
    ChannelIndexMap chanIdx;
    chanIdx.ScanFromScene(scene);

    if (!scene.GetElements().empty()) {
        const std::string_view st0 = GetElementSchema(scene, 0);
        if (st0 == "SceneRoot") {
            SkyboxAuthoringState sk{};
            {
                AttributeValue a = GetAttribute(scene, 0, "SkyboxEnabled");
                if (const auto* b = std::get_if<bool>(&a)) {
                    sk.Enabled = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, 0, "SkyboxBrightness");
                if (const auto* f = std::get_if<float>(&a)) {
                    sk.Brightness = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, 0, "SkyboxYawDegrees");
                if (const auto* f = std::get_if<float>(&a)) {
                    sk.YawDegrees = *f;
                }
            }
            static const char* kFaceKeys[6] = {"SkyboxFacePosX", "SkyboxFaceNegX", "SkyboxFacePosY", "SkyboxFaceNegY",
                "SkyboxFacePosZ", "SkyboxFaceNegZ"};
            for (int i = 0; i < 6; ++i) {
                AttributeValue a = GetAttribute(scene, 0, kFaceKeys[static_cast<size_t>(i)]);
                if (const auto* s = std::get_if<std::string>(&a)) {
                    sk.FacePaths[static_cast<size_t>(i)] = *s;
                }
            }
            outResult.EnvironmentSkybox = std::move(sk);
        }
    }

    for (ElementIndex ei = 0; ei < scene.GetElements().size(); ++ei) {
        std::string_view st = GetElementSchema(scene, ei);
        if (st == "CameraElement" || st == "ActorElement" || st == "AudioSourceElement") {
            Math::Vec3 pos{0, 0, 0};
            {
                AttributeValue posAttr = GetAttribute(scene, ei, "Position");
                if (auto* p = std::get_if<Math::Vec3>(&posAttr)) {
                    pos = *p;
                }
            }
            // Channel-driven Position override (Phase 2 evaluator parity for camera/actor animation).
            if (ChannelIndex chPos = FindElementChannelIndexed(chanIdx, scene, ei, "Position");
                chPos != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chPos, timeTicks);
                if (const auto* p = std::get_if<Math::Vec3>(&v)) {
                    pos = *p;
                }
            }

            // Rotation: prefer authored channel, then attribute, then degrees triplet.
            Math::Quaternion rot{1, 0, 0, 0};
            {
                AttributeValue rotAttr = GetAttribute(scene, ei, "Rotation");
                if (const auto* q = std::get_if<Math::Quaternion>(&rotAttr)) {
                    rot = *q;
                }
            }
            if (ChannelIndex chRot = FindElementChannelIndexed(chanIdx, scene, ei, "Rotation");
                chRot != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chRot, timeTicks);
                if (const auto* q = std::get_if<Math::Quaternion>(&v)) {
                    rot = *q;
                }
            }
            // Euler triplet fallback (PitchDeg/YawDeg/RollDeg or *Degrees synonyms).
            auto readF = [&](const char* k, float dflt) -> float {
                AttributeValue a = GetAttribute(scene, ei, k);
                if (const auto* f = std::get_if<float>(&a)) {
                    return *f;
                }
                return dflt;
            };
            const float pitchDeg = readF("PitchDeg", readF("PitchDegrees", 0.f));
            const float yawDeg = readF("YawDeg", readF("YawDegrees", 0.f));
            const float rollDeg = readF("RollDeg", readF("RollDegrees", 0.f));
            const bool hasEulerAttr = std::abs(pitchDeg) > 1e-5f || std::abs(yawDeg) > 1e-5f || std::abs(rollDeg) > 1e-5f;
            if (hasEulerAttr && rot.w >= 0.99999f && std::abs(rot.x) < 1e-6f && std::abs(rot.y) < 1e-6f
                && std::abs(rot.z) < 1e-6f) {
                constexpr float kDeg = 3.14159265f / 180.f;
                rot = Math::Quaternion::FromEuler(pitchDeg * kDeg, yawDeg * kDeg, rollDeg * kDeg);
            }

            // Scale: attribute first, then channel.
            Math::Vec3 scl{1.f, 1.f, 1.f};
            {
                AttributeValue sAttr = GetAttribute(scene, ei, "Scale");
                if (const auto* p = std::get_if<Math::Vec3>(&sAttr)) {
                    scl = *p;
                }
            }
            if (ChannelIndex chScl = FindElementChannelIndexed(chanIdx, scene, ei, "Scale");
                chScl != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chScl, timeTicks);
                if (const auto* p = std::get_if<Math::Vec3>(&v)) {
                    scl = *p;
                }
            }

            ElementTransform et;
            et.Element = ei;
            et.Position = pos;
            et.Rotation = rot;
            et.Scale = scl;
            outResult.ElementTransforms.push_back(et);
        }
        if (st == "ActorElement") {
            ActorArzachelAuthoring ar{};
            ar.Element = ei;
            {
                AttributeValue a = GetAttribute(scene, ei, "ArzachelRigidBodyDamage");
                if (const auto* f = std::get_if<float>(&a)) {
                    ar.RigidBodyDamage = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "LodDistanceHigh");
                if (const auto* f = std::get_if<float>(&a)) {
                    ar.LodDistanceHigh = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "LodDistanceLow");
                if (const auto* f = std::get_if<float>(&a)) {
                    ar.LodDistanceLow = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ArzachelAnimationClipPreset");
                if (const auto* s = std::get_if<std::string>(&a)) {
                    ar.AnimationClipPreset = *s;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ArzachelDestructionAnimPreset");
                if (const auto* s = std::get_if<std::string>(&a)) {
                    ar.DestructionAnimPreset = *s;
                }
            }
            outResult.ActorArzachelAuthorings.push_back(std::move(ar));

            ActorFacialPose face{};
            face.Element = ei;
            int64_t facialSeedI64 = 0;
            {
                AttributeValue a = GetAttribute(scene, ei, "FacialVariationSeed");
                if (const auto* i = std::get_if<int64_t>(&a)) {
                    facialSeedI64 = *i;
                }
            }
            bool procBlink = true;
            {
                AttributeValue a = GetAttribute(scene, ei, "EnableProceduralBlink");
                if (const auto* b = std::get_if<bool>(&a)) {
                    procBlink = *b;
                }
            }
            bool procSaccade = true;
            {
                AttributeValue a = GetAttribute(scene, ei, "EnableProceduralSaccade");
                if (const auto* b = std::get_if<bool>(&a)) {
                    procSaccade = *b;
                }
            }

            std::string moodName = "neutral";
            float moodWeight = 0.f;
            if (ChannelIndex chN = FindElementChannelIndexed(chanIdx, scene, ei, kChannelFacialMoodName);
                chN != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chN, timeTicks);
                if (const auto* s = std::get_if<std::string>(&v)) {
                    moodName = *s;
                }
            }
            if (ChannelIndex chW = FindElementChannelIndexed(chanIdx, scene, ei, kChannelFacialMoodWeight);
                chW != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chW, timeTicks);
                if (const auto* f = std::get_if<float>(&v)) {
                    moodWeight = *f;
                }
            }

            std::string visemeId;
            float visemeStrength = 0.f;
            if (ChannelIndex chV = FindElementChannelIndexed(chanIdx, scene, ei, kChannelFacialVisemeId);
                chV != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chV, timeTicks);
                if (const auto* s = std::get_if<std::string>(&v)) {
                    visemeId = *s;
                }
            }
            if (ChannelIndex chVs = FindElementChannelIndexed(chanIdx, scene, ei, kChannelFacialVisemeWeight);
                chVs != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chVs, timeTicks);
                if (const auto* f = std::get_if<float>(&v)) {
                    visemeStrength = *f;
                }
            }

            Arzachel::ExpressionStack stack{};
            if (const Arzachel::Expression* ex = Arzachel::BuiltinExpressionByName(moodName)) {
                stack.Expressions.push_back({ex, moodWeight});
            }
            stack.VisemeId = std::move(visemeId);
            stack.VisemeStrength = visemeStrength;

            const uint32_t tps = std::max(1u, scene.GetTicksPerSecond());
            const float timeSec = static_cast<float>(timeTicks) / static_cast<float>(tps);
            const Arzachel::Seed facialSeed(static_cast<uint64_t>(facialSeedI64));
            static const Arzachel::VisemeSet kVis = Arzachel::VisemeSet::Standard();
            Arzachel::EvaluateFacialAtTimeToMaps(face.BoneDeltasByName, face.MorphWeights, stack, kVis, timeSec, facialSeed,
                procBlink, procSaccade);
            outResult.ActorFacialPoses.push_back(std::move(face));
        }
        if (st == "LightElement") {
            LightState ls;
            ls.Element = ei;
            {
                AttributeValue a = GetAttribute(scene, ei, "Position");
                if (auto* p = std::get_if<Math::Vec3>(&a)) {
                    ls.Position = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Color");
                if (auto* c = std::get_if<Math::Vec4>(&a)) {
                    ls.Color = *c;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Intensity");
                if (auto* in = std::get_if<float>(&a)) {
                    ls.Intensity = *in;
                }
            }
            outResult.LightStates.push_back(ls);
        }
        if (st == "AudioSourceElement") {
            AudioSourceState as;
            as.Element = ei;
            {
                AttributeValue a = GetAttribute(scene, ei, "Volume");
                if (auto* v = std::get_if<float>(&a)) {
                    as.Volume = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Pitch");
                if (auto* p = std::get_if<float>(&a)) {
                    as.Pitch = *p;
                }
            }
            outResult.AudioStates.push_back(as);
        }
        if (st == "SmmFluidVolumeElement") {
            FluidVolumeState fv;
            fv.Element = ei;
            if (ei < scene.GetElements().size()) {
                fv.Name = scene.GetElements()[ei].Name;
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Enabled");
                if (const auto* b = std::get_if<bool>(&a)) {
                    fv.Enabled = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "EnableMacCormack");
                if (const auto* b = std::get_if<bool>(&a)) {
                    fv.EnableMacCormack = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "EnableBoussinesq");
                if (const auto* b = std::get_if<bool>(&a)) {
                    fv.EnableBoussinesq = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "VolumeVisualizationClip");
                if (const auto* b = std::get_if<bool>(&a)) {
                    fv.VolumeVisualizationClip = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "BoundsMin");
                if (const auto* v = std::get_if<Math::Vec3>(&a)) {
                    fv.BoundsMin = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "BoundsMax");
                if (const auto* v = std::get_if<Math::Vec3>(&a)) {
                    fv.BoundsMax = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ResolutionX");
                if (const auto* in = std::get_if<int32_t>(&a)) {
                    fv.Nx = *in;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ResolutionY");
                if (const auto* in = std::get_if<int32_t>(&a)) {
                    fv.Ny = *in;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ResolutionZ");
                if (const auto* in = std::get_if<int32_t>(&a)) {
                    fv.Nz = *in;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Diffusion");
                if (const auto* f = std::get_if<float>(&a)) {
                    fv.Diffusion = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Viscosity");
                if (const auto* f = std::get_if<float>(&a)) {
                    fv.Viscosity = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ReferenceDensity");
                if (const auto* f = std::get_if<float>(&a)) {
                    fv.ReferenceDensity = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "PressureRelaxationIterations");
                if (const auto* in = std::get_if<int32_t>(&a)) {
                    fv.PressureRelaxationIterations = *in;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "BuoyancyStrength");
                if (const auto* f = std::get_if<float>(&a)) {
                    fv.BuoyancyStrength = *f;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Prandtl");
                if (const auto* f = std::get_if<float>(&a)) {
                    fv.Prandtl = *f;
                }
            }
            outResult.FluidVolumes.push_back(std::move(fv));
        }
        if (st == "SmmSoftBodyElement") {
            SoftBodyState sb;
            sb.Element = ei;
            if (ei < scene.GetElements().size()) {
                sb.Name = scene.GetElements()[ei].Name;
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Enabled");
                if (const auto* b = std::get_if<bool>(&a)) {
                    sb.Enabled = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "AnchorTopRow");
                if (const auto* b = std::get_if<bool>(&a)) {
                    sb.AnchorTopRow = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Origin");
                if (const auto* v = std::get_if<Math::Vec3>(&a)) {
                    sb.Origin = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "GridWidth");
                if (const auto* v = std::get_if<int32_t>(&a)) {
                    sb.GridWidth = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "GridHeight");
                if (const auto* v = std::get_if<int32_t>(&a)) {
                    sb.GridHeight = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "NodeSpacing");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.NodeSpacing = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "NodeMass");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.NodeMass = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Damping");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.Damping = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "StructuralStiffness");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.StructuralStiffness = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "ShearStiffness");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.ShearStiffness = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "BendStiffness");
                if (const auto* v = std::get_if<float>(&a)) {
                    sb.BendStiffness = *v;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "SolverIterations");
                if (const auto* v = std::get_if<int32_t>(&a)) {
                    sb.SolverIterations = *v;
                }
            }
            outResult.SoftBodies.push_back(std::move(sb));
        }
        if (st == "SmmVehicleElement") {
            VehicleState v;
            v.Element = ei;
            if (ei < scene.GetElements().size()) {
                v.Name = scene.GetElements()[ei].Name;
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Enabled");
                if (const auto* b = std::get_if<bool>(&a)) {
                    v.Enabled = *b;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Origin");
                if (const auto* p = std::get_if<Math::Vec3>(&a)) {
                    v.Origin = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "WheelBase");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.WheelBase = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "TrackWidth");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.TrackWidth = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "Mass");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.Mass = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "EngineForce");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.EngineForce = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "BrakeForce");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.BrakeForce = *p;
                }
            }
            {
                AttributeValue a = GetAttribute(scene, ei, "MaxSteerAngleRadians");
                if (const auto* p = std::get_if<float>(&a)) {
                    v.MaxSteerAngleRadians = *p;
                }
            }
            outResult.Vehicles.push_back(std::move(v));
        }
    }

    const auto& mgs = scene.GetMGElements();
    for (MGIndex mi = 0; mi < mgs.size(); ++mi) {
        const MGElementRecord& mg = mgs[mi];
        if (mg.SchemaIndex >= scene.GetSchemas().size()) {
            continue;
        }
        if (scene.GetSchemas()[mg.SchemaIndex].TypeName != "MGSpriteElement") {
            continue;
        }
        if (MgAttrInt(mg, "MGProjectionMode", 0) != 1) {
            continue;
        }

        MGWorldSpriteState ws{};
        ws.MGElement = mi;
        ws.Position = MgAttrVec3(mg, "WorldPosition", Math::Vec3{0.f, 1.25f, 0.f});
        ws.Scale = MgAttrVec3(mg, "WorldScale", Math::Vec3{1.f, 1.f, 1.f});
        ws.PitchDeg = MgAttrFloat(mg, "WorldPitchDeg", 0.f);
        ws.YawDeg = MgAttrFloat(mg, "WorldYawDeg", 0.f);
        ws.RollDeg = MgAttrFloat(mg, "WorldRollDeg", MgAttrFloat(mg, "RotationZ", 0.f) * (180.0f / 3.14159265f));
        ws.Color = MgAttrVec4(mg, "Color", Math::Vec4{1.f, 1.f, 1.f, 1.f});
        ws.CastShadows = MgAttrBool(mg, "CastShadows", true);
        ws.AttachToElement = MgAttrBool(mg, "AttachToElement", false);
        if (ws.AttachToElement) {
            const int32_t attachIdx = MgAttrInt(mg, "AttachElementIndex", -1);
            if (attachIdx >= 0) {
                ws.AttachElement = static_cast<ElementIndex>(attachIdx);
                for (const ElementTransform& et : outResult.ElementTransforms) {
                    if (et.Element != ws.AttachElement) {
                        continue;
                    }
                    ws.Position.x += et.Position.x;
                    ws.Position.y += et.Position.y;
                    ws.Position.z += et.Position.z;
                    break;
                }
            }
        }
        outResult.MGWorldSprites.push_back(ws);
    }

}

MGDisplayList EvaluateMG(const ParallaxScene& scene, uint64_t timeTicks) {
    return EvaluateMGImpl(scene, timeTicks);
}

} // namespace Solstice::Parallax
