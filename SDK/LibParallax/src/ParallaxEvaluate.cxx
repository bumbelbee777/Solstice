#include <Parallax/ParallaxScene.hxx>

#include <MinGfx/EasingFunction.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Parallax {

namespace {

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
    double span = static_cast<double>(k1.TimeTicks - k0.TimeTicks);
    float t01 = span > 0.0 ? static_cast<float>(static_cast<double>(timeTicks - k0.TimeTicks) / span) : 0.f;
    t01 = SampleEased(t01, k1.Easing);
    return LerpValue(ch.ValueType, k0.Value, k1.Value, t01);
}

void EvaluateScene(const ParallaxScene& scene, uint64_t timeTicks, SceneEvaluationResult& outResult) {
    outResult.ElementTransforms.clear();
    outResult.LightStates.clear();
    outResult.AudioStates.clear();
    outResult.ScriptOutputs.clear();
    outResult.MotionGraphics = EvaluateMG(scene, timeTicks);

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
                size_t idx = 0;
                for (size_t k = 0; k < tr.Keyframes.size(); ++k) {
                    if (tr.Keyframes[k].TimeTicks >= timeTicks) {
                        idx = k;
                        break;
                    }
                    idx = k;
                }
                if (tr.Keyframes.size() == 1) {
                    v = tr.Keyframes[0].Value;
                } else if (idx > 0) {
                    const auto& k0 = tr.Keyframes[idx - 1];
                    const auto& k1 = tr.Keyframes[idx];
                    double span = static_cast<double>(k1.TimeTicks - k0.TimeTicks);
                    float t01 = span > 0.0 ? static_cast<float>(static_cast<double>(timeTicks - k0.TimeTicks) / span) : 0.f;
                    t01 = SampleEased(t01, k1.Easing);
                    v = LerpValue(tr.ValueType, k0.Value, k1.Value, t01);
                } else {
                    v = tr.Keyframes[0].Value;
                }
            }
            e.Attributes[tr.PropertyName] = std::move(v);
        }
        list.Entries.push_back(std::move(e));
    }
    return list;
}

} // namespace Solstice::Parallax
