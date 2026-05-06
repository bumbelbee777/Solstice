#include "PortalLightPropagation.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Render {
namespace PortalLightPropagation {

namespace {

struct Globals {
    bool Active{false};
    Math::Matrix4 WorldToPartner{Math::Matrix4::Identity()};
    float Strength{0.f};
};

Globals& State() {
    static Globals G;
    return G;
}

Math::Vec3 WarpDirection(const Math::Vec3& dirNorm, const Math::Matrix4& T) {
    const Math::Vec4 hv = T * Math::Vec4(dirNorm.x, dirNorm.y, dirNorm.z, 0.f);
    Math::Vec3 w{hv.x, hv.y, hv.z};
    if (w.Dot(w) < 1e-14f) return dirNorm;
    return w.Normalized();
}

Math::Vec3 BlendDirection(const Math::Vec3& aNorm, const Math::Vec3& bNorm, float t) {
    t = std::clamp(t, 0.f, 1.f);
    const Math::Vec3 mixed{aNorm.x * (1.f - t) + bNorm.x * t, aNorm.y * (1.f - t) + bNorm.y * t,
        aNorm.z * (1.f - t) + bNorm.z * t};
    if (mixed.Dot(mixed) < 1e-14f) return aNorm;
    return mixed.Normalized();
}

} // namespace

void ConfigureActivePortal(const Math::Matrix4& worldToPartnerWorld, float strengthScale) {
    Globals& G = State();
    G.Strength = std::clamp(strengthScale, 0.f, 2.f);
    G.WorldToPartner = worldToPartnerWorld;
    G.Active = G.Strength > 1e-5f &&
               std::abs(worldToPartnerWorld.Determinant()) > static_cast<float>(1e-8);
}

void ClearActivePortal() {
    State() = Globals{};
}

std::vector<Solstice::Physics::LightSource> PrepareLights(const std::vector<Solstice::Physics::LightSource>& lights) {
    const Globals& G = State();
    std::vector<Solstice::Physics::LightSource> out = lights;
    if (!G.Active || G.Strength <= 0.f) return out;

    const float directionalBlend =
        std::clamp(G.Strength, 0.f, 1.f); // mixes toward topo-mapped directions
    const float ghostBrightness = std::clamp(0.2f + 0.65f * G.Strength * G.Strength, 0.05f, 0.92f);

    const bool hadDirectional =
        std::any_of(out.begin(), out.end(), [](const Solstice::Physics::LightSource& L) {
            return L.Type == Solstice::Physics::LightSource::LightType::Directional;
        });

    if (!hadDirectional) {
        Solstice::Physics::LightSource fallback{};
        fallback.Type = Solstice::Physics::LightSource::LightType::Directional;
        const Math::Vec3 fallbackDir = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized();
        fallback.Position =
            BlendDirection(fallbackDir, WarpDirection(fallbackDir, G.WorldToPartner), directionalBlend);
        fallback.Color = Math::Vec3(1.0f, 0.95f, 0.9f);
        fallback.Intensity = 1.5f;
        fallback.Hue = 0.f;
        fallback.Attenuation = 0.001f;
        out.push_back(fallback);
    } else {
        for (Solstice::Physics::LightSource& L : out) {
            if (L.Type != Solstice::Physics::LightSource::LightType::Directional) continue;
            const Math::Vec3 d = L.Position.Normalized();
            L.Position = BlendDirection(d, WarpDirection(d, G.WorldToPartner), directionalBlend);
        }
    }

    for (const Solstice::Physics::LightSource& src : lights) {
        if (src.Type != Solstice::Physics::LightSource::LightType::Point &&
            src.Type != Solstice::Physics::LightSource::LightType::Spot) {
            continue;
        }
        const Math::Vec4 hp = G.WorldToPartner * Math::Vec4(src.Position.x, src.Position.y, src.Position.z, 1.0f);
        Solstice::Physics::LightSource ghost = src;
        ghost.Position = Math::Vec3(hp.x, hp.y, hp.z);
        ghost.Intensity *= ghostBrightness;
        out.push_back(ghost);
    }

    return out;
}

} // namespace PortalLightPropagation

} // namespace Solstice::Render
