#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "../Math/Vector.hxx"
#include <string>

namespace Solstice::Game {

// Weapon types
enum class WeaponType {
    Pistol,
    Rifle,
    Shotgun,
    Sniper,
    Sword,
    Knife,
    Axe,
    Bow,
    Crossbow,
    Cannon,
    RocketLauncher,
    Railgun,
    COUNT
};

// Weapon component
struct Weapon {
    WeaponType Type{WeaponType::Rifle};
    std::string Name;

    // Damage and range
    float Damage{25.0f};
    float Range{100.0f};
    float FireRate{10.0f}; // Shots per second

    // Ammo (for ranged weapons)
    int CurrentAmmo{30};
    int MaxAmmo{30};
    int ReserveAmmo{90};
    int MaxReserveAmmo{90};
    float ReloadTime{2.0f};

    // Melee properties
    float MeleeRange{2.0f};
    float MeleeSwingTime{0.5f};
    float MeleeCooldown{1.0f};

    // Timing
    float LastFireTime{0.0f};
    float ReloadStartTime{0.0f};
    bool IsReloading{false};
    bool IsFiring{false};

    // Spread/accuracy
    float BaseSpread{0.02f};
    float MaxSpread{0.1f};
    float SpreadIncrease{0.01f};
    float CurrentSpread{0.02f};
    float SpreadDecayRate{0.05f};

    // Projectile properties
    float ProjectileSpeed{500.0f};
    bool IsHitscan{true}; // If false, uses projectiles

    // Visual/audio
    std::string FireSoundPath;
    std::string ReloadSoundPath;
    std::string EmptySoundPath;
};

// Weapon system
class SOLSTICE_API WeaponSystem {
public:
    // Update weapon (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Fire weapon
    static bool Fire(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec3& Direction);

    // Reload weapon
    static bool Reload(ECS::Registry& Registry, ECS::EntityId Entity);

    // Melee attack
    static bool MeleeAttack(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec3& Direction);

    // Check if weapon can fire
    static bool CanFire(ECS::Registry& Registry, ECS::EntityId Entity);

    // Check if weapon can reload
    static bool CanReload(ECS::Registry& Registry, ECS::EntityId Entity);

    // Get weapon stats
    static Weapon* GetWeapon(ECS::Registry& Registry, ECS::EntityId Entity);
    static int GetCurrentAmmo(ECS::Registry& Registry, ECS::EntityId Entity);
    static int GetReserveAmmo(ECS::Registry& Registry, ECS::EntityId Entity);

    // Add ammo
    static void AddAmmo(ECS::Registry& Registry, ECS::EntityId Entity, int Amount);

private:
    static void UpdateSpread(Weapon& Weapon, float DeltaTime);
    static void ProcessHitscan(ECS::Registry& Registry, ECS::EntityId Entity,
                              const Weapon& Weapon, const Math::Vec3& Direction);
    static void SpawnProjectile(ECS::Registry& Registry, ECS::EntityId Entity,
                               const Weapon& Weapon, const Math::Vec3& Direction);
    static void ProcessMeleeHit(ECS::Registry& Registry, ECS::EntityId Entity,
                               const Weapon& Weapon, const Math::Vec3& Direction);
};

} // namespace Solstice::Game
