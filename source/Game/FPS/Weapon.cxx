#include "Weapon.hxx"
#include "../Gameplay/Health.hxx"
#include "../Gameplay/SFXManager.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Entity/Transform.hxx"
#include <cmath>
#include <algorithm>

namespace Solstice::Game {

void WeaponSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<Weapon>([&](ECS::EntityId entity, Weapon& weapon) {
        // Update spread decay
        UpdateSpread(weapon, DeltaTime);

        // Update reload
        if (weapon.IsReloading) {
            float currentTime = weapon.ReloadStartTime + DeltaTime;
            if (currentTime >= weapon.ReloadTime) {
                // Reload complete
                int ammoNeeded = weapon.MaxAmmo - weapon.CurrentAmmo;
                int ammoToAdd = std::min(ammoNeeded, weapon.ReserveAmmo);
                weapon.CurrentAmmo += ammoToAdd;
                weapon.ReserveAmmo -= ammoToAdd;
                weapon.IsReloading = false;
                weapon.ReloadStartTime = 0.0f;
            }
        }

        // Reset firing state
        weapon.IsFiring = false;
    });
}

bool WeaponSystem::Fire(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec3& Direction) {
    if (!Registry.Has<Weapon>(Entity)) return false;

    Weapon& weapon = Registry.Get<Weapon>(Entity);

    // Check if can fire
    if (!CanFire(Registry, Entity)) {
        return false;
    }

    // Check fire rate
    float currentTime = weapon.LastFireTime + 1.0f / weapon.FireRate;
    if (currentTime < weapon.LastFireTime) {
        return false; // Too soon to fire
    }

    // Determine if ranged or melee
    bool isRanged = (weapon.Type != WeaponType::Sword &&
                     weapon.Type != WeaponType::Knife &&
                     weapon.Type != WeaponType::Axe);

    if (isRanged) {
        // Ranged weapon
        if (weapon.CurrentAmmo <= 0) {
            // Play empty sound
            if (!weapon.EmptySoundPath.empty()) {
                SFXManager::Instance().PlaySound(weapon.EmptySoundPath, SFXCategory::Combat, 0.5f, false);
            }
            return false;
        }

        // Consume ammo
        weapon.CurrentAmmo--;
        weapon.LastFireTime = currentTime;
        weapon.IsFiring = true;

        // Increase spread
        weapon.CurrentSpread = std::min(weapon.MaxSpread,
                                        weapon.CurrentSpread + weapon.SpreadIncrease);

        // Play fire sound
        if (!weapon.FireSoundPath.empty()) {
            Math::Vec3 position = Math::Vec3(0, 0, 0);
            if (Registry.Has<ECS::Transform>(Entity)) {
                position = Registry.Get<ECS::Transform>(Entity).Position;
            }
            SFXManager::Instance().OnWeaponFire(position);
        }

        // Process hit/launch projectile
        if (weapon.IsHitscan) {
            ProcessHitscan(Registry, Entity, weapon, Direction);
        } else {
            SpawnProjectile(Registry, Entity, weapon, Direction);
        }
    } else {
        // Melee weapon
        return MeleeAttack(Registry, Entity, Direction);
    }

    return true;
}

bool WeaponSystem::Reload(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!CanReload(Registry, Entity)) {
        return false;
    }

    Weapon& weapon = Registry.Get<Weapon>(Entity);
    weapon.IsReloading = true;
    weapon.ReloadStartTime = 0.0f; // Will be updated in Update

    // Play reload sound
    if (!weapon.ReloadSoundPath.empty()) {
        SFXManager::Instance().OnWeaponReload();
    }

    return true;
}

bool WeaponSystem::MeleeAttack(ECS::Registry& Registry, ECS::EntityId Entity, const Math::Vec3& Direction) {
    if (!Registry.Has<Weapon>(Entity)) return false;

    Weapon& weapon = Registry.Get<Weapon>(Entity);

    // Check cooldown
    float currentTime = weapon.LastFireTime + weapon.MeleeCooldown;
    if (currentTime < weapon.LastFireTime) {
        return false; // Still on cooldown
    }

    weapon.LastFireTime = currentTime;
    weapon.IsFiring = true;

    // Process melee hit
    ProcessMeleeHit(Registry, Entity, weapon, Direction);

    return true;
}

bool WeaponSystem::CanFire(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Weapon>(Entity)) return false;

    Weapon& weapon = Registry.Get<Weapon>(Entity);

    if (weapon.IsReloading) return false;

    // Check if ranged or melee
    bool isRanged = (weapon.Type != WeaponType::Sword &&
                     weapon.Type != WeaponType::Knife &&
                     weapon.Type != WeaponType::Axe);

    if (isRanged) {
        return weapon.CurrentAmmo > 0;
    } else {
        // Melee weapons can always attack (cooldown checked in MeleeAttack)
        return true;
        }
}

bool WeaponSystem::CanReload(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Weapon>(Entity)) return false;

    Weapon& weapon = Registry.Get<Weapon>(Entity);

    if (weapon.IsReloading) return false;
    if (weapon.CurrentAmmo >= weapon.MaxAmmo) return false;
    if (weapon.ReserveAmmo <= 0) return false;

    return true;
}

Weapon* WeaponSystem::GetWeapon(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (Registry.Has<Weapon>(Entity)) {
        return &Registry.Get<Weapon>(Entity);
    }
    return nullptr;
}

int WeaponSystem::GetCurrentAmmo(ECS::Registry& Registry, ECS::EntityId Entity) {
    Weapon* weapon = GetWeapon(Registry, Entity);
    return weapon ? weapon->CurrentAmmo : 0;
}

int WeaponSystem::GetReserveAmmo(ECS::Registry& Registry, ECS::EntityId Entity) {
    Weapon* weapon = GetWeapon(Registry, Entity);
    return weapon ? weapon->ReserveAmmo : 0;
}

void WeaponSystem::AddAmmo(ECS::Registry& Registry, ECS::EntityId Entity, int Amount) {
    Weapon* weapon = GetWeapon(Registry, Entity);
    if (weapon) {
        weapon->ReserveAmmo = std::min(weapon->MaxReserveAmmo, weapon->ReserveAmmo + Amount);
    }
}

void WeaponSystem::UpdateSpread(Weapon& Weapon, float DeltaTime) {
    if (Weapon.CurrentSpread > Weapon.BaseSpread) {
        Weapon.CurrentSpread = std::max(Weapon.BaseSpread,
                                        Weapon.CurrentSpread - Weapon.SpreadDecayRate * DeltaTime);
    }
}

void WeaponSystem::ProcessHitscan(ECS::Registry& Registry, ECS::EntityId Entity,
                                  const Weapon& Weapon, const Math::Vec3& Direction) {
    // Get entity position
    Math::Vec3 origin = Math::Vec3(0, 0, 0);
    if (Registry.Has<ECS::Transform>(Entity)) {
        origin = Registry.Get<ECS::Transform>(Entity).Position;
    }

    // Apply spread to direction
    Math::Vec3 spreadDir = Direction;
    // TODO: Apply spread based on Weapon.CurrentSpread

    // Raycast to find hit
    // This would integrate with physics system for raycasting
    // For now, just apply damage to entities in range

    Registry.ForEach<Health, ECS::Transform>([&](ECS::EntityId targetEntity, Health& health, ECS::Transform& transform) {
        if (targetEntity == Entity) return; // Don't hit self

        Math::Vec3 toTarget = transform.Position - origin;
        float distance = toTarget.Magnitude();

        if (distance <= Weapon.Range) {
            // Check if direction aligns with target
            Math::Vec3 dirToTarget = toTarget.Normalized();
            float dot = dirToTarget.Dot(spreadDir);

            // Simple cone check (would use proper raycast in production)
            if (dot > 0.9f) {
                HealthSystem::ApplyDamage(Registry, targetEntity, Weapon.Damage, origin, Entity);
            }
        }
    });
}

void WeaponSystem::SpawnProjectile(ECS::Registry& Registry, ECS::EntityId Entity,
                                   const Weapon& Weapon, const Math::Vec3& Direction) {
    // This would spawn a projectile entity
    // For now, just process as hitscan
    ProcessHitscan(Registry, Entity, Weapon, Direction);
}

void WeaponSystem::ProcessMeleeHit(ECS::Registry& Registry, ECS::EntityId Entity,
                                   const Weapon& Weapon, const Math::Vec3& Direction) {
    // Get entity position
    Math::Vec3 origin = Math::Vec3(0, 0, 0);
    if (Registry.Has<ECS::Transform>(Entity)) {
        origin = Registry.Get<ECS::Transform>(Entity).Position;
    }

    // Check for entities in melee range
    Registry.ForEach<Health, ECS::Transform>([&](ECS::EntityId targetEntity, Health& health, ECS::Transform& transform) {
        if (targetEntity == Entity) return; // Don't hit self

        Math::Vec3 toTarget = transform.Position - origin;
        float distance = toTarget.Magnitude();

        if (distance <= Weapon.MeleeRange) {
            // Check if direction aligns with target
            Math::Vec3 dirToTarget = toTarget.Normalized();
            float dot = dirToTarget.Dot(Direction);

            if (dot > 0.7f) { // 45 degree cone
                HealthSystem::ApplyDamage(Registry, targetEntity, Weapon.Damage, origin, Entity);
            }
        }
    });
}

} // namespace Solstice::Game
