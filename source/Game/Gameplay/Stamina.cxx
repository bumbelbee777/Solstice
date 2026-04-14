#include "Gameplay/Stamina.hxx"
#include "../../Core/Debug/Debug.hxx"

namespace Solstice::Game {

void StaminaSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<Stamina>([&](ECS::EntityId entity, Stamina& stamina) {
        (void)entity;

        stamina.TimeSinceConsumption += DeltaTime;

        // Regenerate stamina
        if (stamina.CanRegenerate && stamina.TimeSinceConsumption >= stamina.RegenerationDelay) {
            stamina.CurrentStamina = std::min(stamina.MaxStamina,
                                             stamina.CurrentStamina + stamina.RegenerationRate * DeltaTime);
        }
    });
}

bool StaminaSystem::ConsumeStamina(ECS::Registry& Registry, ECS::EntityId Entity, float Amount) {
    if (!Registry.Has<Stamina>(Entity)) return false;

    Stamina& stamina = Registry.Get<Stamina>(Entity);
    if (stamina.CurrentStamina >= Amount) {
        stamina.CurrentStamina -= Amount;
        stamina.TimeSinceConsumption = 0.0f;
        return true;
    }
    return false;
}

bool StaminaSystem::ConsumeStaminaForSprint(ECS::Registry& Registry, ECS::EntityId Entity, float DeltaTime) {
    if (!Registry.Has<Stamina>(Entity)) return false;

    Stamina& stamina = Registry.Get<Stamina>(Entity);
    float consumption = stamina.SprintConsumptionRate * DeltaTime;
    return ConsumeStamina(Registry, Entity, consumption);
}

bool StaminaSystem::ConsumeStaminaForMelee(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Stamina>(Entity)) return false;

    Stamina& stamina = Registry.Get<Stamina>(Entity);
    return ConsumeStamina(Registry, Entity, stamina.MeleeConsumptionRate);
}

bool StaminaSystem::ConsumeStaminaForAbility(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Stamina>(Entity)) return false;

    Stamina& stamina = Registry.Get<Stamina>(Entity);
    return ConsumeStamina(Registry, Entity, stamina.AbilityConsumptionRate);
}

bool StaminaSystem::HasStamina(ECS::Registry& Registry, ECS::EntityId Entity, float Amount) {
    if (!Registry.Has<Stamina>(Entity)) return false;
    return Registry.Get<Stamina>(Entity).CurrentStamina >= Amount;
}

float StaminaSystem::GetStaminaPercent(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Stamina>(Entity)) return 0.0f;

    Stamina& stamina = Registry.Get<Stamina>(Entity);
    return stamina.MaxStamina > 0.0f ? stamina.CurrentStamina / stamina.MaxStamina : 0.0f;
}

float StaminaSystem::GetCurrentStamina(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Stamina>(Entity)) return 0.0f;
    return Registry.Get<Stamina>(Entity).CurrentStamina;
}

float StaminaSystem::GetMaxStamina(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<Stamina>(Entity)) return 0.0f;
    return Registry.Get<Stamina>(Entity).MaxStamina;
}

} // namespace Solstice::Game
