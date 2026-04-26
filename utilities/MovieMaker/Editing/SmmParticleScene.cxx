#include "SmmParticleScene.hxx"

#include "SmmParticleEditor.hxx"

#include <Parallax/DevSessionAssetResolver.hxx>

#include <cmath>
#include <cstring>
#include <filesystem>

namespace Smm::Particles {
namespace {

constexpr const char* kSchema = "SmmParticleEmitterElement";
constexpr const char* kElementName = "SMM_ParticleEmitter";

void EnsureSchema(Solstice::Parallax::ParallaxScene& scene) {
    if (Solstice::Parallax::HasSchema(scene, kSchema)) {
        return;
    }
    Solstice::Parallax::RegisterSchema(scene, kSchema,
        {{"Enabled", Solstice::Parallax::AttributeType::Bool},
            {"AttachToSceneElement", Solstice::Parallax::AttributeType::Bool},
            {"AttachElementIndex", Solstice::Parallax::AttributeType::Int32},
            {"SpawnPerSec", Solstice::Parallax::AttributeType::Float},
            {"LifetimeSec", Solstice::Parallax::AttributeType::Float},
            {"VelMin", Solstice::Parallax::AttributeType::Vec3},
            {"VelMax", Solstice::Parallax::AttributeType::Vec3},
            {"StartSize", Solstice::Parallax::AttributeType::Float},
            {"EndSize", Solstice::Parallax::AttributeType::Float},
            {"Gravity", Solstice::Parallax::AttributeType::Vec3},
            {"LinearDrag", Solstice::Parallax::AttributeType::Float},
            {"MaxParticles", Solstice::Parallax::AttributeType::Int32},
            {"ColorStart", Solstice::Parallax::AttributeType::ColorRGBA},
            {"ColorEnd", Solstice::Parallax::AttributeType::ColorRGBA},
            {"UseImportedSprite", Solstice::Parallax::AttributeType::Bool},
            {"SpriteTexture", Solstice::Parallax::AttributeType::AssetHash},
            {"SpriteSourcePath", Solstice::Parallax::AttributeType::String}});
}

static bool GetBool(const Solstice::Parallax::AttributeValue& v, bool def) {
    if (const auto* p = std::get_if<bool>(&v)) {
        return *p;
    }
    return def;
}

static int32_t GetI32(const Solstice::Parallax::AttributeValue& v, int32_t def) {
    if (const auto* p = std::get_if<int32_t>(&v)) {
        return *p;
    }
    return def;
}

static float GetFloat(const Solstice::Parallax::AttributeValue& v, float def) {
    if (const auto* p = std::get_if<float>(&v)) {
        return *p;
    }
    return def;
}

static Solstice::Math::Vec3 GetVec3(const Solstice::Parallax::AttributeValue& v, const Solstice::Math::Vec3& def) {
    if (const auto* p = std::get_if<Solstice::Math::Vec3>(&v)) {
        return *p;
    }
    return def;
}

static Solstice::Math::Vec4 GetVec4(const Solstice::Parallax::AttributeValue& v, const Solstice::Math::Vec4& def) {
    if (const auto* p = std::get_if<Solstice::Math::Vec4>(&v)) {
        return *p;
    }
    return def;
}

static uint64_t GetU64(const Solstice::Parallax::AttributeValue& v, uint64_t def) {
    if (const auto* p = std::get_if<uint64_t>(&v)) {
        return *p;
    }
    return def;
}

static std::string GetString(const Solstice::Parallax::AttributeValue& v) {
    if (const auto* p = std::get_if<std::string>(&v)) {
        return *p;
    }
    return {};
}

} // namespace

bool SyncEditorToParallaxScene(Solstice::Parallax::ParallaxScene& scene, const Smm::Editing::ParticleEditorState& st,
    Solstice::Parallax::DevSessionAssetResolver& resolver, std::string& errOut) {
    errOut.clear();
    EnsureSchema(scene);
    Solstice::Parallax::ElementIndex ei = Solstice::Parallax::FindElement(scene, kElementName);
    if (ei == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
        ei = Solstice::Parallax::AddElement(scene, kSchema, kElementName, 0);
        if (ei == Solstice::Parallax::PARALLAX_INVALID_INDEX) {
            errOut = "Could not add SmmParticleEmitterElement to scene.";
            return false;
        }
    } else if (Solstice::Parallax::GetElementSchema(scene, ei) != kSchema) {
        errOut = "Scene element \"" + std::string(kElementName) + "\" exists but is not SmmParticleEmitterElement.";
        return false;
    }

    using AV = Solstice::Parallax::AttributeValue;
    Solstice::Parallax::SetAttribute(scene, ei, "Enabled", AV{st.enabled});
    Solstice::Parallax::SetAttribute(scene, ei, "AttachToSceneElement", AV{st.attachToSceneElement});
    Solstice::Parallax::SetAttribute(scene, ei, "AttachElementIndex", AV{static_cast<int32_t>(st.attachElementIndex)});
    Solstice::Parallax::SetAttribute(scene, ei, "SpawnPerSec", AV{st.spawnPerSec});
    Solstice::Parallax::SetAttribute(scene, ei, "LifetimeSec", AV{st.lifetimeSec});
    Solstice::Parallax::SetAttribute(scene, ei, "VelMin", AV{st.velMin});
    Solstice::Parallax::SetAttribute(scene, ei, "VelMax", AV{st.velMax});
    Solstice::Parallax::SetAttribute(scene, ei, "StartSize", AV{st.startSize});
    Solstice::Parallax::SetAttribute(scene, ei, "EndSize", AV{st.endSize});
    Solstice::Parallax::SetAttribute(scene, ei, "Gravity", AV{st.gravity});
    Solstice::Parallax::SetAttribute(scene, ei, "LinearDrag", AV{st.linearDrag});
    Solstice::Parallax::SetAttribute(scene, ei, "MaxParticles", AV{static_cast<int32_t>(st.maxParticles)});
    if (st.useColorGradient) {
        const int n = std::clamp(st.gradientStops, 2, Smm::Editing::ParticleEditorState::kMaxParticleGradient);
        const int li = n - 1;
        Solstice::Parallax::SetAttribute(scene, ei, "ColorStart",
            AV{Solstice::Math::Vec4(st.gradRgba[0][0], st.gradRgba[0][1], st.gradRgba[0][2], st.gradRgba[0][3])});
        Solstice::Parallax::SetAttribute(scene, ei, "ColorEnd", AV{Solstice::Math::Vec4(st.gradRgba[li][0], st.gradRgba[li][1],
                                                                            st.gradRgba[li][2], st.gradRgba[li][3])});
    } else {
        Solstice::Parallax::SetAttribute(scene, ei, "ColorStart",
            AV{Solstice::Math::Vec4(st.colorStart[0], st.colorStart[1], st.colorStart[2], st.colorStart[3])});
        Solstice::Parallax::SetAttribute(scene, ei, "ColorEnd",
            AV{Solstice::Math::Vec4(st.colorEnd[0], st.colorEnd[1], st.colorEnd[2], st.colorEnd[3])});
    }
    Solstice::Parallax::SetAttribute(scene, ei, "UseImportedSprite", AV{st.useImportedSprite});

    uint64_t spriteHash = 0;
    std::string spritePathStored;
    if (st.useImportedSprite && st.particleSpritePath[0] != '\0') {
        spritePathStored = st.particleSpritePath;
        if (const auto imported = resolver.ImportFile(std::filesystem::path(spritePathStored))) {
            spriteHash = *imported;
        }
    }
    Solstice::Parallax::SetAttribute(scene, ei, "SpriteTexture", AV{spriteHash});
    Solstice::Parallax::SetAttribute(scene, ei, "SpriteSourcePath", AV{std::move(spritePathStored)});
    return true;
}

void LoadParticleEditorFromScene(const Solstice::Parallax::ParallaxScene& scene, Smm::Editing::ParticleEditorState& st) {
    if (!Solstice::Parallax::HasSchema(scene, kSchema)) {
        return;
    }
    const Solstice::Parallax::ElementIndex ei = Solstice::Parallax::FindElement(scene, kElementName);
    if (ei == Solstice::Parallax::PARALLAX_INVALID_INDEX || Solstice::Parallax::GetElementSchema(scene, ei) != kSchema) {
        return;
    }

    auto ga = [&](const char* name) { return Solstice::Parallax::GetAttribute(scene, ei, name); };

    st.enabled = GetBool(ga("Enabled"), st.enabled);
    st.attachToSceneElement = GetBool(ga("AttachToSceneElement"), st.attachToSceneElement);
    st.attachElementIndex = static_cast<int>(GetI32(ga("AttachElementIndex"), static_cast<int32_t>(st.attachElementIndex)));
    st.spawnPerSec = GetFloat(ga("SpawnPerSec"), st.spawnPerSec);
    st.lifetimeSec = GetFloat(ga("LifetimeSec"), st.lifetimeSec);
    st.velMin = GetVec3(ga("VelMin"), st.velMin);
    st.velMax = GetVec3(ga("VelMax"), st.velMax);
    st.startSize = GetFloat(ga("StartSize"), st.startSize);
    st.endSize = GetFloat(ga("EndSize"), st.endSize);
    st.gravity = GetVec3(ga("Gravity"), st.gravity);
    st.linearDrag = GetFloat(ga("LinearDrag"), st.linearDrag);
    st.maxParticles = static_cast<int>(GetI32(ga("MaxParticles"), static_cast<int32_t>(st.maxParticles)));

    const Solstice::Math::Vec4 cs = GetVec4(ga("ColorStart"), Solstice::Math::Vec4(st.colorStart[0], st.colorStart[1],
                                                         st.colorStart[2], st.colorStart[3]));
    const Solstice::Math::Vec4 ce =
        GetVec4(ga("ColorEnd"), Solstice::Math::Vec4(st.colorEnd[0], st.colorEnd[1], st.colorEnd[2], st.colorEnd[3]));
    st.colorStart[0] = cs.x;
    st.colorStart[1] = cs.y;
    st.colorStart[2] = cs.z;
    st.colorStart[3] = cs.w;
    st.colorEnd[0] = ce.x;
    st.colorEnd[1] = ce.y;
    st.colorEnd[2] = ce.z;
    st.colorEnd[3] = ce.w;

    for (int c = 0; c < 4; ++c) {
        st.gradRgba[0][c] = st.colorStart[c];
        st.gradRgba[1][c] = st.colorEnd[c];
    }
    st.gradT[0] = 0.f;
    st.gradT[1] = 1.f;
    st.gradientStops = 2;
    st.gradStopsInited = true;
    st.useColorGradient = false;

    st.useImportedSprite = GetBool(ga("UseImportedSprite"), st.useImportedSprite);

    const std::string pathFromScene = GetString(ga("SpriteSourcePath"));
    if (!pathFromScene.empty()) {
        std::strncpy(st.particleSpritePath, pathFromScene.c_str(), sizeof(st.particleSpritePath) - 1);
        st.particleSpritePath[sizeof(st.particleSpritePath) - 1] = '\0';
    } else if (const uint64_t h = GetU64(ga("SpriteTexture"), 0); h != 0) {
        (void)h;
        // No stable path in file; preview may need a manual path until session imports the hash.
    }
}

} // namespace Smm::Particles
