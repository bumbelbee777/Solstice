#include "Export/SharedPreviewMapping.hxx"

#include <Arzachel/FacialAnimation.hxx>

#include "LibUI/Viewport/Viewport.hxx"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>

namespace Solstice::MovieMaker::Export {

namespace {

Solstice::Math::Vec3 RotateForwardByQuaternion(const Solstice::Math::Quaternion& qIn) {
    const Solstice::Math::Quaternion q = qIn.Normalized();
    const float vx = 0.f;
    const float vy = 0.f;
    const float vz = 1.f;
    const float tx = 2.f * (q.y * vz - q.z * vy);
    const float ty = 2.f * (q.z * vx - q.x * vz);
    const float tz = 2.f * (q.x * vy - q.y * vx);
    Solstice::Math::Vec3 out{};
    out.x = vx + q.w * tx + (q.y * tz - q.z * ty);
    out.y = vy + q.w * ty + (q.z * tx - q.x * tz);
    out.z = vz + q.w * tz + (q.x * ty - q.y * tx);
    return out.Normalized();
}

Solstice::Math::Vec3 ReadVec3(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, const Solstice::Math::Vec3& fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&v)) {
        return *p;
    }
    return fallback;
}

float ReadFloat(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, float fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* f = std::get_if<float>(&v)) {
        return *f;
    }
    return fallback;
}

Solstice::Math::Vec4 ReadVec4(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, const Solstice::Math::Vec4& fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<Solstice::Math::Vec4>(&v)) {
        return *p;
    }
    return fallback;
}

bool ReadBool(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key, bool fallback) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<bool>(&v)) {
        return *p;
    }
    return fallback;
}

std::string ReadString(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    std::string_view key) {
    const Solstice::Parallax::AttributeValue v = Solstice::Parallax::GetAttribute(scene, ei, key);
    if (const auto* p = std::get_if<std::string>(&v)) {
        return *p;
    }
    return {};
}

void ReadEulerDegrees(const Solstice::Parallax::ParallaxScene& scene, Solstice::Parallax::ElementIndex ei,
    float& outPitch, float& outYaw, float& outRoll) {
    outPitch = ReadFloat(scene, ei, "PitchDeg", ReadFloat(scene, ei, "PitchDegrees", 0.f));
    outYaw = ReadFloat(scene, ei, "YawDeg", ReadFloat(scene, ei, "YawDegrees", 0.f));
    outRoll = ReadFloat(scene, ei, "RollDeg", ReadFloat(scene, ei, "RollDegrees", 0.f));
}

float MouthOpenHint(const Solstice::Parallax::SceneEvaluationResult& ev, Solstice::Parallax::ElementIndex ei) {
    for (const Solstice::Parallax::ActorFacialPose& fp : ev.ActorFacialPoses) {
        if (fp.Element != ei) {
            continue;
        }
        float m = 0.f;
        const auto jaw = fp.BoneDeltasByName.find("jaw");
        if (jaw != fp.BoneDeltasByName.end()) {
            m = (std::max)(m, std::clamp(-jaw->second.Translation.y * 12.f, 0.f, 1.f));
        }
        const uint32_t jh = Solstice::Arzachel::MorphNameHash("jaw_open");
        const auto mj = fp.MorphWeights.find(jh);
        if (mj != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(mj->second, 0.f, 1.f));
        }
        const uint32_t bh = Solstice::Arzachel::MorphNameHash("blink");
        const auto blink = fp.MorphWeights.find(bh);
        if (blink != fp.MorphWeights.end()) {
            m = (std::max)(m, std::clamp(blink->second * 0.25f, 0.f, 0.15f));
        }
        return m;
    }
    return 0.f;
}

} // namespace

void BuildPreviewEntitiesAndLights(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::SceneEvaluationResult& eval, PreviewMappingResult& result) {
    result.Entities.clear();
    result.Lights.clear();
    result.Entities.reserve(eval.ElementTransforms.size() + eval.MGWorldSprites.size());

    for (const auto& et : eval.ElementTransforms) {
        Solstice::EditorEnginePreview::PreviewEntity pe{};
        pe.Position = et.Position;
        const std::string_view schemaType = Solstice::Parallax::GetElementSchema(scene, et.Element);
        if (schemaType == "CameraElement") {
            pe.Albedo = Solstice::Math::Vec3(1.f, 0.7f, 0.2f);
        } else if (schemaType == "ActorElement") {
            pe.Albedo = Solstice::Math::Vec3(0.35f, 0.62f, 1.f);
        } else {
            pe.Albedo = Solstice::Math::Vec3(0.62f, 0.66f, 0.78f);
        }
        pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        // Phase 2 evaluator parity: prefer the evaluated scale, fall back to attribute.
        const Solstice::Math::Vec3 evalScale = et.Scale;
        const Solstice::Math::Vec3 attrScale = ReadVec3(scene, et.Element, "Scale",
            Solstice::Math::Vec3{1.f, 1.f, 1.f});
        pe.Scale = (evalScale.x != 0.f || evalScale.y != 0.f || evalScale.z != 0.f) ? evalScale : attrScale;
        if (schemaType == "ActorElement") {
            const float talk = MouthOpenHint(eval, et.Element);
            pe.Scale.y = pe.Scale.y * (1.f + 0.22f * talk);
            const std::string smat = ReadString(scene, et.Element, "PreviewSmatPath");
            if (!smat.empty()) {
                std::strncpy(pe.PreviewSmatPath, smat.c_str(), sizeof(pe.PreviewSmatPath) - 1);
                pe.PreviewSmatPath[sizeof(pe.PreviewSmatPath) - 1] = '\0';
            }
            const std::string albedoPath = ReadString(scene, et.Element, "PreviewAlbedoTexturePath");
            if (!albedoPath.empty()) {
                std::strncpy(pe.PreviewAlbedoTexturePath, albedoPath.c_str(), sizeof(pe.PreviewAlbedoTexturePath) - 1);
                pe.PreviewAlbedoTexturePath[sizeof(pe.PreviewAlbedoTexturePath) - 1] = '\0';
            }
            const std::string normalPath = ReadString(scene, et.Element, "PreviewNormalTexturePath");
            if (!normalPath.empty()) {
                std::strncpy(pe.PreviewNormalTexturePath, normalPath.c_str(), sizeof(pe.PreviewNormalTexturePath) - 1);
                pe.PreviewNormalTexturePath[sizeof(pe.PreviewNormalTexturePath) - 1] = '\0';
            }
            const std::string roughPath = ReadString(scene, et.Element, "PreviewRoughnessTexturePath");
            if (!roughPath.empty()) {
                std::strncpy(pe.PreviewRoughnessTexturePath, roughPath.c_str(), sizeof(pe.PreviewRoughnessTexturePath) - 1);
                pe.PreviewRoughnessTexturePath[sizeof(pe.PreviewRoughnessTexturePath) - 1] = '\0';
            }
            pe.BakedAOPreview = std::clamp(ReadFloat(scene, et.Element, "PreviewAOMultiply", 0.42f), 0.f, 1.f);
        }
        ReadEulerDegrees(scene, et.Element, pe.PitchDeg, pe.YawDeg, pe.RollDeg);
        result.Entities.push_back(pe);
    }
    for (const auto& mgw : eval.MGWorldSprites) {
        Solstice::EditorEnginePreview::PreviewEntity pe{};
        pe.Position = mgw.Position;
        pe.Albedo = Solstice::Math::Vec3(
            std::clamp(mgw.Color.x, 0.f, 1.f), std::clamp(mgw.Color.y, 0.f, 1.f), std::clamp(mgw.Color.z, 0.f, 1.f));
        pe.Scale = mgw.Scale;
        pe.PitchDeg = mgw.PitchDeg;
        pe.YawDeg = mgw.YawDeg;
        pe.RollDeg = mgw.RollDeg;
        pe.HalfExtent = 0.5f;
        pe.UseQuadProxy = true;
        pe.CastShadows = mgw.CastShadows;
        result.Entities.push_back(pe);
    }
    for (const auto& fv : eval.FluidVolumes) {
        if (!fv.Enabled) {
            continue;
        }
        const Solstice::Math::Vec3 bmin{
            (std::min)(fv.BoundsMin.x, fv.BoundsMax.x),
            (std::min)(fv.BoundsMin.y, fv.BoundsMax.y),
            (std::min)(fv.BoundsMin.z, fv.BoundsMax.z),
        };
        const Solstice::Math::Vec3 bmax{
            (std::max)(fv.BoundsMin.x, fv.BoundsMax.x),
            (std::max)(fv.BoundsMin.y, fv.BoundsMax.y),
            (std::max)(fv.BoundsMin.z, fv.BoundsMax.z),
        };
        const Solstice::Math::Vec3 center = (bmin + bmax) * 0.5f;
        const Solstice::Math::Vec3 half = (bmax - bmin) * 0.5f;
        Solstice::EditorEnginePreview::PreviewEntity pe{};
        pe.Position = center;
        pe.Albedo = Solstice::Math::Vec3{0.36f, 0.74f, 1.0f};
        pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        constexpr float kHe = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        pe.Scale = Solstice::Math::Vec3{
            (std::max)(half.x / (std::max)(kHe, 1e-3f), 0.15f),
            (std::max)(half.y / (std::max)(kHe, 1e-3f), 0.15f),
            (std::max)(half.z / (std::max)(kHe, 1e-3f), 0.15f),
        };
        pe.CastShadows = false;
        result.Entities.push_back(pe);
    }
    for (const auto& sb : eval.SoftBodies) {
        if (!sb.Enabled) {
            continue;
        }
        Solstice::EditorEnginePreview::PreviewEntity pe{};
        pe.Position = sb.Origin;
        pe.Albedo = Solstice::Math::Vec3{0.66f, 1.0f, 0.66f};
        pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        const float wx = static_cast<float>((std::max)(1, sb.GridWidth));
        const float wy = static_cast<float>((std::max)(1, sb.GridHeight));
        const float sp = (std::max)(sb.NodeSpacing, 0.05f);
        constexpr float kHe = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        pe.Scale = Solstice::Math::Vec3{
            (std::max)((0.5f * wx * sp) / (std::max)(kHe, 1e-3f), 0.2f),
            (std::max)((0.1f * sp) / (std::max)(kHe, 1e-3f), 0.08f),
            (std::max)((0.5f * wy * sp) / (std::max)(kHe, 1e-3f), 0.2f),
        };
        pe.CastShadows = false;
        result.Entities.push_back(pe);
    }
    for (const auto& vh : eval.Vehicles) {
        if (!vh.Enabled) {
            continue;
        }
        Solstice::EditorEnginePreview::PreviewEntity pe{};
        pe.Position = vh.Origin;
        pe.Albedo = Solstice::Math::Vec3{1.0f, 0.72f, 0.3f};
        pe.HalfExtent = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        constexpr float kHe = Solstice::EditorEnginePreview::kSchematicPreviewHalfExtent;
        const float halfLen = (std::max)(vh.WheelBase * 0.5f, 0.4f);
        const float halfWid = (std::max)(vh.TrackWidth * 0.5f, 0.3f);
        pe.Scale = Solstice::Math::Vec3{
            (std::max)(halfWid / (std::max)(kHe, 1e-3f), 0.25f),
            (std::max)(0.35f / (std::max)(kHe, 1e-3f), 0.2f),
            (std::max)(halfLen / (std::max)(kHe, 1e-3f), 0.35f),
        };
        pe.CastShadows = true;
        result.Entities.push_back(pe);
    }

    const bool skyEnabled = ReadBool(scene, 0, "SkyboxEnabled", false);
    const float skyBrightness = std::max(0.f, ReadFloat(scene, 0, "SkyboxBrightness", 1.f));
    const float skyYawDeg = ReadFloat(scene, 0, "SkyboxYawDegrees", 0.f);
    const Solstice::Math::Vec4 ambient = ReadVec4(scene, 0, "AmbientColor",
        Solstice::Math::Vec4{0.06f, 0.07f, 0.09f, 1.f});
    const float ambientIntensity = std::max(0.f, ReadFloat(scene, 0, "AmbientIntensity", 0.25f));
    {
        Solstice::Physics::LightSource sun{};
        sun.Type = Solstice::Physics::LightSource::LightType::Directional;
        const float yaw = skyYawDeg * 0.017453292f;
        sun.Position = Solstice::Math::Vec3(std::sin(yaw) * 0.45f, 0.82f, std::cos(yaw) * 0.45f).Normalized();
        sun.Color = Solstice::Math::Vec3(
            std::clamp(ambient.x + 0.94f, 0.f, 1.f),
            std::clamp(ambient.y + 0.90f, 0.f, 1.f),
            std::clamp(ambient.z + 0.84f, 0.f, 1.f));
        sun.Intensity = (skyEnabled ? 1.15f * (0.5f + 0.5f * skyBrightness) : 1.15f) + 0.7f * ambientIntensity;
        result.Lights.push_back(sun);
    }
    for (const auto& ls : eval.LightStates) {
        Solstice::Physics::LightSource pl{};
        pl.Type = Solstice::Physics::LightSource::LightType::Point;
        pl.Position = ls.Position;
        pl.Color = Solstice::Math::Vec3(ls.Color.x, ls.Color.y, ls.Color.z);
        pl.Intensity = std::max(0.35f, ls.Intensity);
        pl.Range = 48.f;
        result.Lights.push_back(pl);
    }
}

PreviewCameraPose ResolvePreviewCameraPose(const Solstice::Parallax::ParallaxScene& scene,
    const Solstice::Parallax::SceneEvaluationResult& eval) {
    PreviewCameraPose out;
    for (const auto& et : eval.ElementTransforms) {
        if (Solstice::Parallax::GetElementSchema(scene, et.Element) != "CameraElement") {
            continue;
        }
        const Solstice::Math::Vec3 eye = et.Position;
        Solstice::Math::Vec3 dir = RotateForwardByQuaternion(et.Rotation);
        if (dir.Magnitude() <= 1e-4f) {
            dir = Solstice::Math::Vec3{0.f, 0.f, 1.f};
        }
        const Solstice::Math::Vec3 authoredTarget = ReadVec3(scene, et.Element, "Target", eye + dir * 8.f);
        const Solstice::Math::Vec3 eyeToTarget = authoredTarget - eye;
        if (eyeToTarget.Magnitude() > 0.05f) {
            dir = eyeToTarget.Normalized();
        }
        const float dist = std::clamp(eyeToTarget.Magnitude() > 0.05f ? eyeToTarget.Magnitude() : 8.f, 0.5f, 4096.f);
        out.Found = true;
        out.NavDistance = dist;
        out.NavYaw = std::atan2(dir.x, dir.z);
        out.NavPitch = std::asin(std::clamp(dir.y, -1.f, 1.f));
        out.NavPanX = 0.f;
        out.NavPanY = 0.f;
        out.Target = eye + dir * dist;
        out.FovYDeg = std::clamp(ReadFloat(scene, et.Element, "FovDegrees", 55.f), 1.f, 179.f);
        return out;
    }
    return out;
}

} // namespace Solstice::MovieMaker::Export
