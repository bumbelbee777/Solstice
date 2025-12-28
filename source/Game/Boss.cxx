#include "Boss.hxx"
#include "Health.hxx"
#include "Enemy.hxx"
#include "HUD.hxx"
#include "Core/Debug.hxx"
#include <imgui.h>
#include <algorithm>

namespace Solstice::Game {

void BossSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<Boss, Health>([&](ECS::EntityId entity, Boss& boss, Health& health) {
        // Update special attacks
        UpdateSpecialAttacks(boss, DeltaTime);

        // Check phase transitions
        CheckPhaseTransitions(Registry, entity);

        // Update phase
        UpdatePhase(boss, health);

        // Boss uses Enemy system for base AI
        EnemySystem::Update(Registry, DeltaTime);
    });
}

void BossSystem::CheckPhaseTransitions(ECS::Registry& Registry, ECS::EntityId BossEntity) {
    if (!Registry.Has<Boss>(BossEntity) || !Registry.Has<Health>(BossEntity)) return;

    auto& boss = Registry.Get<Boss>(BossEntity);
    auto& health = Registry.Get<Health>(BossEntity);

    float healthPercent = health.CurrentHealth / health.MaxHealth;

    // Check if we need to transition to next phase
    for (size_t i = 0; i < boss.Phases.size(); ++i) {
        if (i > boss.CurrentPhase && healthPercent <= boss.Phases[i].HealthThreshold) {
            // Transition to new phase
            if (boss.CurrentPhase < boss.Phases.size()) {
                boss.Phases[boss.CurrentPhase].IsActive = false;
            }
            boss.CurrentPhase = static_cast<uint32_t>(i);
            boss.Phases[boss.CurrentPhase].IsActive = true;

            SIMPLE_LOG("BossSystem: Boss transitioned to phase: " + boss.Phases[boss.CurrentPhase].PhaseName);

            // Apply phase modifiers to enemy
            if (Registry.Has<Enemy>(BossEntity)) {
                auto& enemy = Registry.Get<Enemy>(BossEntity);
                enemy.AttackDamage *= boss.Phases[boss.CurrentPhase].AttackDamageMultiplier;
                enemy.MoveSpeed *= boss.Phases[boss.CurrentPhase].MoveSpeedMultiplier;
            }
        }
    }
}

void BossSystem::ExecuteSpecialAttack(ECS::Registry& Registry, ECS::EntityId BossEntity, uint32_t AttackIndex) {
    if (!Registry.Has<Boss>(BossEntity)) return;

    auto& boss = Registry.Get<Boss>(BossEntity);
    if (AttackIndex >= boss.SpecialAttacks.size()) return;

    auto& attack = boss.SpecialAttacks[AttackIndex];
    if (!attack.IsReady) return;

    // Execute special attack
    if (boss.CurrentPhase < boss.Phases.size() && boss.Phases[boss.CurrentPhase].IsActive) {
        // Find targets in range
        if (Registry.Has<ECS::Transform>(BossEntity)) {
            auto& bossTransform = Registry.Get<ECS::Transform>(BossEntity);

            // Damage all entities in range (simplified)
            Registry.ForEach<Health, ECS::Transform>([&](ECS::EntityId target, Health& targetHealth, ECS::Transform& targetTransform) {
                if (target == BossEntity) return; // Don't damage self

                Math::Vec3 toTarget = targetTransform.Position - bossTransform.Position;
                float distance = toTarget.Magnitude();

                if (distance <= attack.Range) {
                    HealthSystem::ApplyDamage(Registry, target, attack.Damage, bossTransform.Position, BossEntity);
                }
            });
        }

        attack.Timer = attack.Cooldown;
        attack.IsReady = false;

        SIMPLE_LOG("BossSystem: Executed special attack: " + attack.Name);
    }
}

uint32_t BossSystem::GetCurrentPhase(ECS::Registry& Registry, ECS::EntityId BossEntity) {
    if (!Registry.Has<Boss>(BossEntity)) return 0;
    return Registry.Get<Boss>(BossEntity).CurrentPhase;
}

bool BossSystem::IsInPhase(ECS::Registry& Registry, ECS::EntityId BossEntity, uint32_t PhaseIndex) {
    if (!Registry.Has<Boss>(BossEntity)) return false;
    auto& boss = Registry.Get<Boss>(BossEntity);
    return boss.CurrentPhase == PhaseIndex && PhaseIndex < boss.Phases.size() && boss.Phases[PhaseIndex].IsActive;
}

void BossSystem::ProcessDeath(ECS::Registry& Registry, ECS::EntityId BossEntity) {
    if (!Registry.Has<Boss>(BossEntity)) return;

    auto& boss = Registry.Get<Boss>(BossEntity);

    // Process enemy death (drops loot)
    EnemySystem::ProcessDeath(Registry, BossEntity);

    // Boss-specific death handling
    if (boss.HasOutroSequence && !boss.OutroComplete) {
        // Trigger outro sequence
        boss.OutroComplete = true;
        SIMPLE_LOG("BossSystem: Boss death - outro sequence triggered");
    }
}

void BossSystem::RenderBossHealthBar(ECS::Registry& Registry, ECS::EntityId BossEntity,
                                     float ScreenWidth, float ScreenHeight) {
    if (!Registry.Has<Boss>(BossEntity) || !Registry.Has<Health>(BossEntity)) return;

    auto& boss = Registry.Get<Boss>(BossEntity);
    auto& health = Registry.Get<Health>(BossEntity);

    if (health.IsDead) return;

    float barWidth = ScreenWidth * 0.6f;
    float barHeight = 40.0f;
    float barX = (ScreenWidth - barWidth) * 0.5f;
    float barY = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(barX, barY));
    ImGui::SetNextWindowSize(ImVec2(barWidth, barHeight));
    ImGui::Begin("BossHealthBar", nullptr,
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoInputs);

    float healthPercent = health.CurrentHealth / health.MaxHealth;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 min = ImVec2(barX, barY);
    ImVec2 max = ImVec2(barX + barWidth, barY + barHeight);

    // Background
    drawList->AddRectFilled(min, max, IM_COL32(30, 30, 30, 255));

    // Health bar
    ImVec2 healthMax = ImVec2(barX + barWidth * healthPercent, barY + barHeight);
    drawList->AddRectFilled(min, healthMax, IM_COL32(200, 50, 50, 255));

    // Phase indicator
    if (boss.CurrentPhase < boss.Phases.size()) {
        std::string phaseText = "Phase: " + boss.Phases[boss.CurrentPhase].PhaseName;
        drawList->AddText(ImVec2(barX + 10.0f, barY + 10.0f), IM_COL32(255, 255, 255, 255), phaseText.c_str());
    }

    // Border
    drawList->AddRect(min, max, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

    ImGui::End();
}

void BossSystem::UpdateSpecialAttacks(Boss& Boss, float DeltaTime) {
    for (auto& attack : Boss.SpecialAttacks) {
        if (attack.Timer > 0.0f) {
            attack.Timer -= DeltaTime;
            if (attack.Timer <= 0.0f) {
                attack.Timer = 0.0f;
                attack.IsReady = true;
            }
        }
    }
}

void BossSystem::UpdatePhase(Boss& Boss, Health& Health) {
    // Phase-specific updates would go here
    // For example, enabling/disabling certain attacks based on phase
}

} // namespace Solstice::Game
