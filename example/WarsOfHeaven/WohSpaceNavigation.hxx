#pragma once

#include "WohShipTypes.hxx"
#include "WohSpaceEnvironment.hxx"
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Game/Match/MatchGameplayAuthoringCache.hxx>
#include <cmath>
#include <vector>

namespace Solstice::WarsOfHeaven {

/// Integrate linear motion: gravity from `bodies` + optional thruster input (world +Y up).
[[nodiscard]] inline Math::Vec3 StepShipNewtonian(const Math::Vec3& pos, WohShipKinematics& kin, float deltaTime,
                                                  const std::vector<PlanetaryGravitySource>& bodies, bool forwardThrust,
                                                  float baseThrust = 12.0f) {
    Math::Vec3 a = GravitationalAccelerationAt(pos, bodies);
    if (forwardThrust) {
        const float yrad = kin.YawDeg * 0.0174532924f;
        const float c = std::cos(yrad);
        const float s = std::sin(yrad);
        const Math::Vec3 forward{c, 0.0f, s};
        a = a + forward * (baseThrust * kin.LinearThrustScale);
    }
    kin.LinearVelocity = kin.LinearVelocity + a * deltaTime;
    // light drag so velocity doesn't run away without thrust
    kin.LinearVelocity = kin.LinearVelocity * (1.0f - 0.01f * deltaTime);
    return pos + kin.LinearVelocity * deltaTime;
}

/// Uses authored **celestial occluders** as **gravity** sources (Gm from radius scale) and optional explicit list.
[[nodiscard]] inline std::vector<PlanetaryGravitySource> BuildGravityFromAuthoring(
    const std::vector<Game::CelestialOccluderRuntime>& celestials) {
    std::vector<PlanetaryGravitySource> out;
    out.reserve(celestials.size());
    for (const auto& c : celestials) {
        PlanetaryGravitySource g;
        g.Center = c.Center;
        g.Radius = c.Radius;
        g.Gm = c.Radius * c.Radius * 0.4f; // game-tuned; larger bodies pull harder
        out.push_back(g);
    }
    return out;
}

/// Per-frame tick for the player ship entity (if it has the WoH components).
inline void UpdatePlayerShipIfPresent(ECS::Registry& reg, ECS::EntityId ship, float dt, bool keyForward,
                                      const std::vector<PlanetaryGravitySource>& gravity,
                                      const std::vector<AsteroidBeltRegion>& belts) {
    if (ship == 0 || !reg.Has<ECS::Transform>(ship) || !reg.Has<WohShipKinematics>(ship)) {
        return;
    }
    auto& t = reg.Get<ECS::Transform>(ship);
    auto& k = reg.Get<WohShipKinematics>(ship);
    t.Position = StepShipNewtonian(t.Position, k, dt, gravity, keyForward, 12.0f);

    const float stealth = StealthScaleFromAsteroidBelts(t.Position, belts);
    if (reg.Has<ShipHullHealth>(ship)) {
        // Drive Arzachel fragmentation intensity from inverse health + environment (denser = more "noise" to sensors).
        auto& h = reg.Get<ShipHullHealth>(ship);
        h.ArzachelFragmentation01 =
            (std::clamp)(1.0f - h.Health01(), 0.0f, 1.0f) * 0.85f + (1.0f - stealth) * 0.15f;
    }
    (void)stealth; // may also drive sensors UI
}

} // namespace Solstice::WarsOfHeaven
