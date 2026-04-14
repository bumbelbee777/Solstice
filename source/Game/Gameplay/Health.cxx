#include "Gameplay/Health.hxx"
#include "Core/Debug/Debug.hxx"
#include <unordered_map>
#include <algorithm>

namespace Solstice::Game {

std::unordered_map<ECS::EntityId, DeathCallback> HealthSystem::m_DeathCallbacks;

void HealthSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    // Process damage events
    Registry.ForEach<Health, DamageEvent>([&](ECS::EntityId entity, Health& health, DamageEvent& damageEvent) {
        if (health.InvincibilityTimer <= 0.0f) {
            // Apply armor reduction
            float actualDamage = damageEvent.Damage;
            if (health.Armor > 0.0f) {
                float armorReduction = actualDamage * 0.5f; // 50% damage reduction from armor
                if (health.Armor >= armorReduction) {
                    health.Armor -= armorReduction;
                    actualDamage -= armorReduction;
                } else {
                    actualDamage -= health.Armor;
                    health.Armor = 0.0f;
                }
            }

            if (damageEvent.IsCritical) {
                actualDamage *= 1.5f; // 50% more damage on critical
            }

            health.CurrentHealth -= actualDamage;
            health.TimeSinceDamage = 0.0f;

            if (health.CurrentHealth <= 0.0f) {
                health.CurrentHealth = 0.0f;
                health.IsDead = true;

                // Trigger death callback
                auto it = m_DeathCallbacks.find(entity);
                if (it != m_DeathCallbacks.end() && it->second) {
                    it->second(entity);
                }
            }
        }

        // Remove damage event
        Registry.Remove<DamageEvent>(entity);
    });

    // Process heal events
    Registry.ForEach<Health, HealEvent>([&](ECS::EntityId entity, Health& health, HealEvent& healEvent) {
        health.CurrentHealth += healEvent.Amount;
        if (health.CurrentHealth > health.MaxHealth) {
            health.CurrentHealth = health.MaxHealth;
        }
        Registry.Remove<HealEvent>(entity);
    });

    // Update health regeneration
    Registry.ForEach<Health>([&](ECS::EntityId entity, Health& health) {
        if (health.IsDead) return;

        // Update invincibility timer
        if (health.InvincibilityTimer > 0.0f) {
            health.InvincibilityTimer -= DeltaTime;
            if (health.InvincibilityTimer < 0.0f) {
                health.InvincibilityTimer = 0.0f;
            }
        }

        // Update time since damage
        health.TimeSinceDamage += DeltaTime;

        // Regenerate health if conditions are met
        if (health.CanRegenerate && health.RegenerationRate > 0.0f &&
            health.CurrentHealth < health.MaxHealth &&
            health.TimeSinceDamage >= health.RegenerationDelay) {
            health.CurrentHealth += health.RegenerationRate * DeltaTime;
            if (health.CurrentHealth > health.MaxHealth) {
                health.CurrentHealth = health.MaxHealth;
            }
        }
    });
}

void HealthSystem::ApplyDamage(ECS::Registry& Registry, ECS::EntityId Entity, float Damage,
                               const Math::Vec3& Source, ECS::EntityId SourceEntity, bool IsCritical) {
    if (!Registry.Has<Health>(Entity)) return;

    DamageEvent damageEvent;
    damageEvent.Damage = Damage;
    damageEvent.Source = Source;
    damageEvent.SourceEntity = SourceEntity;
    damageEvent.IsCritical = IsCritical;

    Registry.Add<DamageEvent>(Entity, damageEvent);
}

void HealthSystem::ApplyHealing(ECS::Registry& Registry, ECS::EntityId Entity, float Amount) {
    if (!Registry.Has<Health>(Entity)) return;

    HealEvent healEvent;
    healEvent.Amount = Amount;

    Registry.Add<HealEvent>(Entity, healEvent);
}

bool HealthSystem::IsDead(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Health>(Entity)) return false;
    return Registry.Get<Health>(Entity).IsDead;
}

bool HealthSystem::IsInvincible(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Health>(Entity)) return false;
    return Registry.Get<Health>(Entity).InvincibilityTimer > 0.0f;
}

void HealthSystem::SetInvincible(ECS::Registry& Registry, ECS::EntityId Entity, float Duration) {
    if (!Registry.Has<Health>(Entity)) return;

    auto& health = Registry.Get<Health>(Entity);
    health.InvincibilityTimer = Duration;
    health.InvincibilityDuration = Duration;
}

void HealthSystem::RegisterDeathCallback(ECS::EntityId Entity, DeathCallback Callback) {
    m_DeathCallbacks[Entity] = Callback;
}

void HealthSystem::UnregisterDeathCallback(ECS::EntityId Entity) {
    m_DeathCallbacks.erase(Entity);
}

} // namespace Solstice::Game
