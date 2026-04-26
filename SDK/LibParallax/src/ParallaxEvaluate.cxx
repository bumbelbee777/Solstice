#include <Parallax/ParallaxScene.hxx>

#include <Arzachel/FacialAnimation.hxx>
#include <Arzachel/Seed.hxx>

#include <MinGfx/EasingFunction.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Parallax {

namespace {

ChannelIndex FindElementChannel(const ParallaxScene& scene, ElementIndex element, std::string_view attributeName) {
    const auto& channels = scene.GetChannels();
    for (ChannelIndex ci = 0; ci < channels.size(); ++ci) {
        if (channels[ci].Element == element && channels[ci].AttributeName == attributeName) {
            return ci;
        }
    }
    return PARALLAX_INVALID_INDEX;
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

} // namespace

AttributeValue EvaluateChannel(const ParallaxScene& scene, ChannelIndex channel, uint64_t timeTicks) {
    (void)scene;
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
    size_t i = 1;
    for (; i < kfs.size(); ++i) {
        if (kfs[i].TimeTicks >= timeTicks) {
            break;
        }
    }
    const auto& k0 = kfs[i - 1];
    const auto& k1 = kfs[i];
    return InterpolatePair(ch.ValueType, k0, k1, timeTicks);
}

void EvaluateScene(const ParallaxScene& scene, uint64_t timeTicks, SceneEvaluationResult& outResult) {
    outResult.ElementTransforms.clear();
    outResult.LightStates.clear();
    outResult.AudioStates.clear();
    outResult.FluidVolumes.clear();
    outResult.ScriptOutputs.clear();
    outResult.EnvironmentSkybox.reset();
    outResult.ActorArzachelAuthoring.clear();
    outResult.ActorFacialPoses.clear();
    outResult.MotionGraphics = EvaluateMG(scene, timeTicks);

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
        if (st == "CameraElement" || st == "ActorElement") {
            Math::Vec3 pos{0, 0, 0};
            AttributeValue posAttr = GetAttribute(scene, ei, "Position");
            if (auto* p = std::get_if<Math::Vec3>(&posAttr)) {
                pos = *p;
            }
            ElementTransform et;
            et.Element = ei;
            et.Position = pos;
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
            outResult.ActorArzachelAuthoring.push_back(std::move(ar));

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
            if (ChannelIndex chN = FindElementChannel(scene, ei, kChannelFacialMoodName); chN != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chN, timeTicks);
                if (const auto* s = std::get_if<std::string>(&v)) {
                    moodName = *s;
                }
            }
            if (ChannelIndex chW = FindElementChannel(scene, ei, kChannelFacialMoodWeight); chW != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chW, timeTicks);
                if (const auto* f = std::get_if<float>(&v)) {
                    moodWeight = *f;
                }
            }

            std::string visemeId;
            float visemeStrength = 0.f;
            if (ChannelIndex chV = FindElementChannel(scene, ei, kChannelFacialVisemeId); chV != PARALLAX_INVALID_INDEX) {
                AttributeValue v = EvaluateChannel(scene, chV, timeTicks);
                if (const auto* s = std::get_if<std::string>(&v)) {
                    visemeId = *s;
                }
            }
            if (ChannelIndex chVs = FindElementChannel(scene, ei, kChannelFacialVisemeWeight);
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
    }

}

MGDisplayList EvaluateMG(const ParallaxScene& scene, uint64_t timeTicks) {
    MGDisplayList list;
    list.CompositeMode = scene.GetMGCompositeMode();
    list.GlobalAlpha = scene.GetMGGlobalAlpha();
    for (const auto& mg : scene.GetMGElements()) {
        MGDisplayList::Entry e;
        if (mg.SchemaIndex < scene.GetSchemas().size()) {
            e.SchemaType = scene.GetSchemas()[mg.SchemaIndex].TypeName;
        }
        e.Blend = BlendMode::Over;
        e.Alpha = 1.0f;
        for (const auto& kv : mg.Attributes) {
            e.Attributes[kv.first] = kv.second;
        }
        uint32_t t0 = mg.FirstTrackIndex;
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
        list.Entries.push_back(std::move(e));
    }
    return list;
}

} // namespace Solstice::Parallax
