#pragma once

#include <Entity/MatchGameplay.hxx>
#include <Entity/EntityId.hxx>
#include <Math/Vector.hxx>

#include <array>
#include <cstdint>
#include <string>

namespace Solstice::WarsOfHeaven {

/// Weapon and launch families for loadout and replication.
enum class WarshipWeaponKind : std::uint8_t {
    Missile = 0,
    Coilgun = 1,
    Railgun = 2,
    PointDefense = 3,
    VLS = 4,
};

enum class WarshipClass : std::uint8_t {
    Corvette = 0,
    Frigate,
    Destroyer,
    Cruiser,
    Carrier,
    Count
};

enum class WarshipSignatureAbility : std::uint8_t {
    Afterburn = 0,
    CIWSFlood,
    TorpedoSalvo,
    SpinalLance,
    AirWing,
};

inline constexpr std::size_t kWarshipMaxHardpoints = 8u;

struct WarshipWeaponLoadout {
    std::uint8_t HardpointCount{2};
    std::array<WarshipWeaponKind, kWarshipMaxHardpoints> Kinds{};

    WarshipWeaponKind Primary{WarshipWeaponKind::Coilgun};
    WarshipWeaponKind Secondary{WarshipWeaponKind::Missile};
};

struct WarshipHullState {
    WarshipClass Class{WarshipClass::Corvette};
    WarshipSignatureAbility LastSignature{WarshipSignatureAbility::Afterburn};
    float SignatureCooldownRemaining{0.0f};
};

/// **Variable** hull: `Max` set from `WarshipClassDef::HullPoints` (game balance); `Current` per entity.
struct ShipHullHealth {
    float Current{100.0f};
    float Max{100.0f};
    /// Arzachel damage / fragmentation intensity 0 = pristine, 1 = full kinetic fragmentation on hull mesh.
    float ArzachelFragmentation01{0.0f};

    void ApplyDamage(float amount) {
        if (Current > 0.0f) {
            Current -= amount;
        }
        if (Current < 0.0f) {
            Current = 0.0f;
        }
    }

    float Health01() const { return (Max > 0.0f) ? (Current / Max) : 0.0f; }
};

/// Newtonian-ish ship for WoH: integrated in game **before** any optional `Physics::RigidBody` host sync.
struct WohShipKinematics {
    Math::Vec3 LinearVelocity{};
    /// Ship forward (yaw) and pitch in radians (camera uses these).
    float YawDeg{0.0f};
    float PitchDeg{0.0f};
    float LinearThrustScale{1.0f};
};

} // namespace Solstice::WarsOfHeaven
