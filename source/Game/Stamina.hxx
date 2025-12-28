#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include <functional>

namespace Solstice::Game {

// Stamina component
struct Stamina {
    float CurrentStamina{100.0f};
    float MaxStamina{100.0f};

    float RegenerationRate{20.0f}; // Stamina per second
    float RegenerationDelay{2.0f}; // Seconds before regeneration starts
    float TimeSinceConsumption{0.0f};

    // Consumption rates
    float SprintConsumptionRate{30.0f}; // Per second
    float MeleeConsumptionRate{25.0f}; // Per attack
    float AbilityConsumptionRate{50.0f}; // Per ability

    bool CanRegenerate{true};
};

// Stamina system
class SOLSTICE_API StaminaSystem {
public:
    // Update stamina (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Consume stamina
    static bool ConsumeStamina(ECS::Registry& Registry, ECS::EntityId Entity, float Amount);
    static bool ConsumeStaminaForSprint(ECS::Registry& Registry, ECS::EntityId Entity, float DeltaTime);
    static bool ConsumeStaminaForMelee(ECS::Registry& Registry, ECS::EntityId Entity);
    static bool ConsumeStaminaForAbility(ECS::Registry& Registry, ECS::EntityId Entity);

    // Check if entity has enough stamina
    static bool HasStamina(ECS::Registry& Registry, ECS::EntityId Entity, float Amount);
    static float GetStaminaPercent(ECS::Registry& Registry, ECS::EntityId Entity);

    // Get stamina
    static float GetCurrentStamina(ECS::Registry& Registry, ECS::EntityId Entity);
    static float GetMaxStamina(ECS::Registry& Registry, ECS::EntityId Entity);
};

} // namespace Solstice::Game
