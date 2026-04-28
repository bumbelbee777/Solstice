#pragma once

#include <Math/Vector.hxx>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Solstice::WarsOfHeaven {

/// Point-mass + radius (radius for display/collision; gravity uses **Gm** only).
struct PlanetaryGravitySource {
    Math::Vec3 Center{};
    float Radius{100.0f};
    /// Gravitational parameter in **game** units m³/s²; tune with ship thrust.
    float Gm{5.0e4f};
};

/// Dense belt: ship inside AABB gets reduced sensor cross-section.
struct AsteroidBeltRegion {
    Math::Vec3 BoundsMin{};
    Math::Vec3 BoundsMax{};
    /// Effective signature scale while inside (multiplies IR/radar) — lower = more stealth in clutter.
    float StealthScale{0.5f};
};

/// Sum Newtonian accelerations from point masses (1/r², capped near center).
[[nodiscard]] inline Math::Vec3 GravitationalAccelerationAt(const Math::Vec3& position,
                                                         const std::vector<PlanetaryGravitySource>& bodies) {
    Math::Vec3 a{};
    for (const auto& p : bodies) {
        Math::Vec3 r = p.Center - position;
        float d2 = r.x * r.x + r.y * r.y + r.z * r.z;
        d2 = (std::max)(d2, p.Radius * p.Radius * 0.5f);
        const float d = std::sqrt(d2);
        Math::Vec3 u{r.x / d, r.y / d, r.z / d};
        const float g = p.Gm / d2;
        a = a + u * g;
    }
    return a;
}

/// True if a sphere (planet) blocks the segment **observer** → **target** (e.g. sensors / weapons LOS).
[[nodiscard]] inline bool IsOccludedByPlanet(const Math::Vec3& observer, const Math::Vec3& target,
                                             const Math::Vec3& planetCenter, float planetRadius) {
    const Math::Vec3 ab = target - observer;
    const float abLen2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (abLen2 < 1e-12f) {
        return false;
    }
    const Math::Vec3 ac = planetCenter - observer;
    float t = (ac.x * ab.x + ac.y * ab.y + ac.z * ab.z) / abLen2;
    t = (std::clamp)(t, 0.0f, 1.0f);
    const Math::Vec3 closest{observer.x + ab.x * t, observer.y + ab.y * t, observer.z + ab.z * t};
    const float dx = closest.x - planetCenter.x;
    const float dy = closest.y - planetCenter.y;
    const float dz = closest.z - planetCenter.z;
    return (dx * dx + dy * dy + dz * dz) < planetRadius * planetRadius;
}

/// 1.0 = open space; <1 inside dense **asteroid** AABBs (stack multipliers if multiple overlap, product).
[[nodiscard]] inline float StealthScaleFromAsteroidBelts(const Math::Vec3& shipPos,
                                                        const std::vector<AsteroidBeltRegion>& belts) {
    float mul = 1.0f;
    for (const auto& b : belts) {
        if (shipPos.x >= b.BoundsMin.x && shipPos.x <= b.BoundsMax.x && shipPos.y >= b.BoundsMin.y &&
            shipPos.y <= b.BoundsMax.y && shipPos.z >= b.BoundsMin.z && shipPos.z <= b.BoundsMax.z) {
            mul *= b.StealthScale;
        }
    }
    return (std::clamp)(mul, 0.0f, 1.0f);
}

} // namespace Solstice::WarsOfHeaven
