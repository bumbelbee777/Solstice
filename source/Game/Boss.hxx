#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "../Entity/Transform.hxx"
#include "../Math/Vector.hxx"
#include "Enemy.hxx"
#include "Health.hxx"
#include <string>
#include <vector>
#include <unordered_map>

namespace Solstice::Game {

// Boss phase
struct BossPhase {
    float HealthThreshold{0.0f}; // Health percentage (0.0-1.0) to trigger this phase
    std::string PhaseName;
    float AttackDamageMultiplier{1.0f};
    float MoveSpeedMultiplier{1.0f};
    float SpecialAttackCooldown{5.0f};
    bool IsActive{false};
};

// Special attack
struct SpecialAttack {
    std::string Name;
    float Cooldown{10.0f};
    float Timer{0.0f};
    float Damage{50.0f};
    float Range{15.0f};
    bool IsReady{false};
};

// Boss component
struct Boss {
    std::vector<BossPhase> Phases;
    uint32_t CurrentPhase{0};

    std::vector<SpecialAttack> SpecialAttacks;

    float DamageResistance{0.0f}; // 0.0 = no resistance, 1.0 = immune
    float MaxHealthMultiplier{5.0f}; // Bosses have more health

    bool HasIntroSequence{false};
    bool IntroComplete{false};
    bool HasOutroSequence{false};
    bool OutroComplete{false};

    // Weak points (positions relative to boss)
    std::vector<Math::Vec3> WeakPoints;
    float WeakPointDamageMultiplier{2.0f};
};

// Boss system
class SOLSTICE_API BossSystem {
public:
    // Update boss (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Check phase transitions
    static void CheckPhaseTransitions(ECS::Registry& Registry, ECS::EntityId BossEntity);

    // Execute special attack
    static void ExecuteSpecialAttack(ECS::Registry& Registry, ECS::EntityId BossEntity, uint32_t AttackIndex);

    // Get current phase
    static uint32_t GetCurrentPhase(ECS::Registry& Registry, ECS::EntityId BossEntity);

    // Check if boss is in specific phase
    static bool IsInPhase(ECS::Registry& Registry, ECS::EntityId BossEntity, uint32_t PhaseIndex);

    // Process boss death
    static void ProcessDeath(ECS::Registry& Registry, ECS::EntityId BossEntity);

    // Render boss health bar (separate from regular HUD)
    static void RenderBossHealthBar(ECS::Registry& Registry, ECS::EntityId BossEntity,
                                   float ScreenWidth, float ScreenHeight);

private:
    static void UpdateSpecialAttacks(Boss& Boss, float DeltaTime);
    static void UpdatePhase(Boss& Boss, Health& Health);
};

} // namespace Solstice::Game
