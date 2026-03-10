#include "AIBrain.hxx"
#include "Enemy.hxx"
#include "Health.hxx"
#include "FPS/Weapon.hxx"
#include "../Core/Debug.hxx"
#include "../Entity/Transform.hxx"
#include "../Physics/RigidBody.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Game {

void AISystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<AIBrain>([&](ECS::EntityId entity, AIBrain& brain) {
        brain.DecisionTimer += DeltaTime;

        if (brain.DecisionTimer >= brain.DecisionInterval) {
            ExtractFeatures(Registry, entity, brain);
            MakeDecision(Registry, entity, brain);
            ApplyDecision(Registry, entity, brain);
            brain.DecisionTimer = 0.0f;
        }
    });
}

void AISystem::InitializeBrain(ECS::Registry& Registry, ECS::EntityId Entity, bool UseMLP) {
    if (!Registry.Has<AIBrain>(Entity)) return;

    AIBrain& brain = Registry.Get<AIBrain>(Entity);
    brain.UseMLP = UseMLP;

    if (UseMLP) {
        // Create MLP network: 8 inputs -> 16 -> 8 -> 4 outputs
        brain.MLPNetwork = std::make_unique<Core::MLP>();
        brain.MLPNetwork->AddLayer(8, 16, Core::ActivationType::ReLU);
        brain.MLPNetwork->AddLayer(16, 8, Core::ActivationType::ReLU);
        brain.MLPNetwork->AddLayer(8, 4, Core::ActivationType::Tanh);

        brain.InputFeatures.resize(8);
        brain.OutputActions.resize(4);
    } else {
        // Create CNN network for visual input
        brain.CNNNetwork = std::make_unique<Core::CNN>();
        brain.CNNNetwork->AddConvolution(3, 16, 3, 64, 64, 1, 1);
        brain.CNNNetwork->AddPooling(Core::PoolingType::Max, 2);
        brain.CNNNetwork->AddConvolution(16, 32, 3, 32, 32, 1, 1);
        brain.CNNNetwork->AddPooling(Core::PoolingType::Max, 2);

        brain.InputFeatures.resize(brain.CNNNetwork->GetInputSize());
        brain.OutputActions.resize(4);
    }
}

void AISystem::ExtractFeatures(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain) {
    // Extract features from environment
    Brain.DistanceToTarget = 0.0f;
    Brain.HealthPercent = 1.0f;
    Brain.AmmoPercent = 1.0f;
    Brain.ThreatLevel = 0.0f;

    // Get entity position
    Math::Vec3 position = Math::Vec3(0, 0, 0);
    if (Registry.Has<ECS::Transform>(Entity)) {
        position = Registry.Get<ECS::Transform>(Entity).Position;
    }

    // Get health
    if (Registry.Has<Health>(Entity)) {
        auto& health = Registry.Get<Health>(Entity);
        Brain.HealthPercent = health.CurrentHealth / health.MaxHealth;
    }

    // Get weapon/ammo
    if (Registry.Has<Weapon>(Entity)) {
        auto& weapon = Registry.Get<Weapon>(Entity);
        Brain.AmmoPercent = static_cast<float>(weapon.CurrentAmmo) / static_cast<float>(weapon.MaxAmmo);
    }

    // Get target distance
    if (Registry.Has<Enemy>(Entity)) {
        auto& enemy = Registry.Get<Enemy>(Entity);
        if (enemy.TargetEntity != 0 && Registry.Has<ECS::Transform>(enemy.TargetEntity)) {
            auto& targetTransform = Registry.Get<ECS::Transform>(enemy.TargetEntity);
            Brain.DistanceToTarget = (targetTransform.Position - position).Magnitude();
        }
    }

    // Calculate threat level (simplified)
    Brain.ThreatLevel = (1.0f - Brain.HealthPercent) * 0.5f;

    // Populate input features
    if (Brain.UseMLP && Brain.MLPNetwork) {
        Brain.InputFeatures[0] = Brain.DistanceToTarget / 100.0f; // Normalize
        Brain.InputFeatures[1] = Brain.HealthPercent;
        Brain.InputFeatures[2] = Brain.AmmoPercent;
        Brain.InputFeatures[3] = Brain.ThreatLevel;
        Brain.InputFeatures[4] = position.x / 100.0f; // Normalize
        Brain.InputFeatures[5] = position.y / 100.0f;
        Brain.InputFeatures[6] = position.z / 100.0f;
        Brain.InputFeatures[7] = Brain.EnvironmentalFactors.Magnitude() / 10.0f;
    }
}

void AISystem::MakeDecision(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain) {
    (void)Registry;
    (void)Entity;

    if (Brain.UseMLP && Brain.MLPNetwork && Brain.InputFeatures.size() >= 8) {
        UpdateMLP(Brain);
    } else if (!Brain.UseMLP && Brain.CNNNetwork) {
        UpdateCNN(Brain);
    } else if (Brain.UseBehaviorTree) {
        UpdateBehaviorTree(Registry, Entity, Brain);
    }
}

void AISystem::ApplyDecision(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain) {
    if (Brain.OutputActions.size() < 4) return;

    // Output actions: [move_forward, move_right, attack, retreat]
    float moveForward = Brain.OutputActions[0];
    float moveRight = Brain.OutputActions[1];
    float attack = Brain.OutputActions[2];
    float retreat = Brain.OutputActions[3];

    // Apply movement
    if (Registry.Has<Physics::RigidBody>(Entity)) {
        auto& rb = Registry.Get<Physics::RigidBody>(Entity);

        if (std::abs(moveForward) > 0.1f || std::abs(moveRight) > 0.1f) {
            Math::Vec3 moveDir(moveRight, 0, moveForward);
            moveDir = moveDir.Normalized();
            rb.Velocity.x = moveDir.x * 5.0f;
            rb.Velocity.z = moveDir.z * 5.0f;
        }
    }

    // Apply attack
    if (attack > 0.5f && Registry.Has<Weapon>(Entity)) {
        // Fire weapon
        Math::Vec3 direction = Math::Vec3(0, 0, 1); // Would calculate from target
        WeaponSystem::Fire(Registry, Entity, direction);
    }
}

void AISystem::UpdateMLP(AIBrain& Brain) {
    if (!Brain.MLPNetwork || Brain.InputFeatures.size() < 8) return;

    Brain.OutputActions.resize(4);
    Brain.MLPNetwork->Forward(Brain.InputFeatures.data(), Brain.OutputActions.data());
}

void AISystem::UpdateCNN(AIBrain& Brain) {
    if (!Brain.CNNNetwork) return;

    // CNN would process visual input (screenshot, etc.)
    // For now, just use MLP fallback
    Brain.OutputActions.resize(4, 0.0f);
}

void AISystem::UpdateBehaviorTree(ECS::Registry& Registry, ECS::EntityId Entity, AIBrain& Brain) {
    (void)Registry;
    (void)Entity;

    // Behavior tree fallback for deterministic behavior
    if (Brain.CurrentBehavior == "patrol") {
        Brain.OutputActions = {0.5f, 0.0f, 0.0f, 0.0f};
    } else if (Brain.CurrentBehavior == "chase") {
        Brain.OutputActions = {1.0f, 0.0f, 0.5f, 0.0f};
    } else if (Brain.CurrentBehavior == "retreat") {
        Brain.OutputActions = {0.0f, 0.0f, 0.0f, 1.0f};
    } else {
        Brain.OutputActions = {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

} // namespace Solstice::Game
