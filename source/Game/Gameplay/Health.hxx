#pragma once

#include "../../Solstice.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Math/Vector.hxx"
#include <functional>

namespace Solstice::Game {

// Health component
struct Health {
    float CurrentHealth{100.0f};
    float MaxHealth{100.0f};
    float Armor{0.0f};
    float MaxArmor{100.0f};

    float RegenerationRate{0.0f}; // Health per second
    float RegenerationDelay{0.0f}; // Seconds before regeneration starts
    float TimeSinceDamage{0.0f};

    float InvincibilityDuration{0.0f};
    float InvincibilityTimer{0.0f};

    bool IsDead{false};
    bool CanRegenerate{true};
};

// Damage event component (temporary, processed and removed)
struct DamageEvent {
    float Damage{0.0f};
    Math::Vec3 Source{0.0f, 0.0f, 0.0f}; // Damage source position
    bool IsCritical{false};
    ECS::EntityId SourceEntity{0}; // Entity that caused damage
};

// Heal event component (temporary, processed and removed)
struct HealEvent {
    float Amount{0.0f};
};

// Death callback type
using DeathCallback = std::function<void(ECS::EntityId)>;

// Health system
class SOLSTICE_API HealthSystem {
public:
    // Update health system (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Apply damage to entity
    static void ApplyDamage(ECS::Registry& Registry, ECS::EntityId Entity, float Damage,
                           const Math::Vec3& Source = Math::Vec3(0, 0, 0),
                           ECS::EntityId SourceEntity = 0, bool IsCritical = false);

    // Apply healing to entity
    static void ApplyHealing(ECS::Registry& Registry, ECS::EntityId Entity, float Amount);

    // Check if entity is dead
    static bool IsDead(ECS::Registry& Registry, ECS::EntityId Entity);

    // Check if entity is invincible
    static bool IsInvincible(ECS::Registry& Registry, ECS::EntityId Entity);

    // Set invincibility
    static void SetInvincible(ECS::Registry& Registry, ECS::EntityId Entity, float Duration);

    // Register death callback
    static void RegisterDeathCallback(ECS::EntityId Entity, DeathCallback Callback);
    static void UnregisterDeathCallback(ECS::EntityId Entity);

private:
    static std::unordered_map<ECS::EntityId, DeathCallback> m_DeathCallbacks;
};

} // namespace Solstice::Game
