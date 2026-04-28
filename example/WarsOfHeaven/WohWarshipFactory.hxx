#pragma once

#include "WohWarshipPresets.hxx"

#include <Entity/MatchGameplay.hxx>
#include <Entity/Kind.hxx>
#include <Entity/Name.hxx>
#include <Entity/PlayerTag.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Core/Debug/Debug.hxx>
#include <Math/Vector.hxx>
#include <string>

namespace Solstice::WarsOfHeaven {

/// Create a controllable warship in the **game** (not engine `ComponentFactory`).
[[nodiscard]] inline ECS::EntityId CreateWarship(ECS::Registry& Registry, std::uint32_t MatchPlayerId, ECS::TeamId Team,
                                                 WarshipClass Class, const Math::Vec3& Position, const std::string& Name) {
    const ECS::EntityId e = Registry.Create();
    Registry.Add<ECS::Transform>(e, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(e, ECS::Name{Name});
    Registry.Add<ECS::Kind>(e, ECS::Kind{ECS::EntityKind::Player});
    Registry.Add<ECS::PlayerTag>(e, ECS::PlayerTag{});
    Registry.Add<ECS::MatchPlayerIdComponent>(e, ECS::MatchPlayerIdComponent{MatchPlayerId, Team});
    Registry.Add<WarshipHullState>(e, DefaultHullStateForClass(Class));
    Registry.Add<WarshipWeaponLoadout>(e, DefaultLoadoutForClass(Class));
    Registry.Add<ShipHullHealth>(e, DefaultHullHealthForClass(Class));
    WohShipKinematics kin{};
    kin.LinearThrustScale = GetWarshipClassDef(Class).LinearAccelScale;
    Registry.Add<WohShipKinematics>(e, kin);
    SIMPLE_LOG("WoH: warship " + Name + " class=" + std::to_string(static_cast<int>(Class)));
    return e;
}

/// Team red / PvE hostiles: no `PlayerTag` — used for juggernaut / kill-streak tests.
[[nodiscard]] inline ECS::EntityId CreateHostileWarship(ECS::Registry& Registry, std::uint32_t MatchPlayerId, ECS::TeamId Team,
                                                       WarshipClass Class, const Math::Vec3& Position, const std::string& Name) {
    const ECS::EntityId e = Registry.Create();
    Registry.Add<ECS::Transform>(e, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(e, ECS::Name{Name});
    Registry.Add<ECS::Kind>(e, ECS::Kind{ECS::EntityKind::HostileNPC});
    Registry.Add<ECS::MatchPlayerIdComponent>(e, ECS::MatchPlayerIdComponent{MatchPlayerId, Team});
    Registry.Add<WarshipHullState>(e, DefaultHullStateForClass(Class));
    Registry.Add<WarshipWeaponLoadout>(e, DefaultLoadoutForClass(Class));
    Registry.Add<ShipHullHealth>(e, DefaultHullHealthForClass(Class));
    WohShipKinematics kin{};
    kin.LinearThrustScale = GetWarshipClassDef(Class).LinearAccelScale;
    Registry.Add<WohShipKinematics>(e, kin);
    SIMPLE_LOG("WoH: hostile warship " + Name + " class=" + std::to_string(static_cast<int>(Class)));
    return e;
}

} // namespace Solstice::WarsOfHeaven
