#include "Gameplay/Enemy.hxx"
#include "Gameplay/Health.hxx"
#include "Gameplay/Inventory.hxx"
#include "Core/Debug/Debug.hxx"
#include <cmath>
#include <random>

namespace Solstice::Game {

void EnemySystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Registry.ForEach<Enemy, ECS::Transform>([&](ECS::EntityId entity, Enemy& enemy, ECS::Transform& transform) {
        // Update attack timer
        if (enemy.AttackTimer > 0.0f) {
            enemy.AttackTimer -= DeltaTime;
            if (enemy.AttackTimer < 0.0f) {
                enemy.AttackTimer = 0.0f;
            }
        }

        // Update state
        UpdateState(Registry, entity, enemy, DeltaTime);

        // Update transform from physics if available
        if (Registry.Has<Physics::RigidBody>(entity)) {
            auto& rb = Registry.Get<Physics::RigidBody>(entity);
            transform.Position = rb.Position;
            transform.Matrix = Math::Matrix4::Translation(rb.Position) * rb.Rotation.ToMatrix();
        }
    });
}

void EnemySystem::SetTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity)) return;

    auto& enemy = Registry.Get<Enemy>(EnemyEntity);
    enemy.TargetEntity = TargetEntity;

    if (TargetEntity != 0 && Registry.Has<ECS::Transform>(TargetEntity)) {
        enemy.TargetPosition = Registry.Get<ECS::Transform>(TargetEntity).Position;
    }
}

EnemyState EnemySystem::GetState(ECS::Registry& Registry, ECS::EntityId EnemyEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity)) return EnemyState::Idle;
    return Registry.Get<Enemy>(EnemyEntity).CurrentState;
}

void EnemySystem::SetState(ECS::Registry& Registry, ECS::EntityId EnemyEntity, EnemyState State) {
    if (!Registry.Has<Enemy>(EnemyEntity)) return;

    auto& enemy = Registry.Get<Enemy>(EnemyEntity);
    enemy.CurrentState = State;
}

bool EnemySystem::CanSeeTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity) || !Registry.Has<ECS::Transform>(EnemyEntity)) return false;
    if (!Registry.Has<ECS::Transform>(TargetEntity)) return false;

    auto& enemy = Registry.Get<Enemy>(EnemyEntity);
    auto& enemyTransform = Registry.Get<ECS::Transform>(EnemyEntity);
    auto& targetTransform = Registry.Get<ECS::Transform>(TargetEntity);

    // Check distance
    Math::Vec3 toTarget = targetTransform.Position - enemyTransform.Position;
    float distance = toTarget.Magnitude();
    if (distance > enemy.AggroRange) return false;

    // Check vision cone (simplified - would use actual forward direction)
    // For now, just check distance
    return true;
}

bool EnemySystem::CanHearTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity) || !Registry.Has<ECS::Transform>(EnemyEntity)) return false;
    if (!Registry.Has<ECS::Transform>(TargetEntity)) return false;

    auto& enemy = Registry.Get<Enemy>(EnemyEntity);
    auto& enemyTransform = Registry.Get<ECS::Transform>(EnemyEntity);
    auto& targetTransform = Registry.Get<ECS::Transform>(TargetEntity);

    Math::Vec3 toTarget = targetTransform.Position - enemyTransform.Position;
    float distance = toTarget.Magnitude();
    return distance <= enemy.HearingRange;
}

float EnemySystem::GetDistanceToTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity) || !Registry.Has<ECS::Transform>(EnemyEntity)) return 0.0f;
    if (!Registry.Has<ECS::Transform>(TargetEntity)) return 0.0f;

    auto& enemyTransform = Registry.Get<ECS::Transform>(EnemyEntity);
    auto& targetTransform = Registry.Get<ECS::Transform>(TargetEntity);

    Math::Vec3 toTarget = targetTransform.Position - enemyTransform.Position;
    return toTarget.Magnitude();
}

void EnemySystem::ProcessDeath(ECS::Registry& Registry, ECS::EntityId EnemyEntity) {
    if (!Registry.Has<Enemy>(EnemyEntity)) return;

    auto& enemy = Registry.Get<Enemy>(EnemyEntity);

    // Drop loot based on drop table
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);

    if (Registry.Has<ECS::Transform>(EnemyEntity)) {
        auto& transform = Registry.Get<ECS::Transform>(EnemyEntity);

        for (const auto& [itemID, chance] : enemy.DropTable) {
            if (dis(gen) <= chance) {
                // In a real implementation, spawn item at enemy position
                SIMPLE_LOG("EnemySystem: Dropped item " + std::to_string(itemID) + " at position");
            }
        }
    }
}

void EnemySystem::UpdateState(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime) {
    // Check if enemy is dead
    if (HealthSystem::IsDead(Registry, Entity)) {
        Enemy.CurrentState = EnemyState::Dead;
        ProcessDeath(Registry, Entity);
        return;
    }

    // Update target position if target exists
    if (Enemy.TargetEntity != 0 && Registry.Has<ECS::Transform>(Enemy.TargetEntity)) {
        Enemy.TargetPosition = Registry.Get<ECS::Transform>(Enemy.TargetEntity).Position;
    }

    // State-specific updates
    switch (Enemy.CurrentState) {
        case EnemyState::Patrol:
            UpdatePatrol(Registry, Entity, Enemy, DeltaTime);
            break;
        case EnemyState::Chase:
            UpdateChase(Registry, Entity, Enemy, DeltaTime);
            break;
        case EnemyState::Attack:
            UpdateAttack(Registry, Entity, Enemy, DeltaTime);
            break;
        case EnemyState::Flee:
            UpdateFlee(Registry, Entity, Enemy, DeltaTime);
            break;
        default:
            break;
    }

    // State transitions
    if (Enemy.TargetEntity != 0) {
        float distance = GetDistanceToTarget(Registry, Entity, Enemy.TargetEntity);

        if (Enemy.CurrentState == EnemyState::Patrol || Enemy.CurrentState == EnemyState::Idle) {
            if (CanSeeTarget(Registry, Entity, Enemy.TargetEntity) || CanHearTarget(Registry, Entity, Enemy.TargetEntity)) {
                Enemy.CurrentState = EnemyState::Chase;
            }
        } else if (Enemy.CurrentState == EnemyState::Chase) {
            if (distance <= Enemy.AttackRange && Enemy.AttackTimer <= 0.0f) {
                Enemy.CurrentState = EnemyState::Attack;
            } else if (distance > Enemy.AggroRange * 2.0f) {
                Enemy.CurrentState = EnemyState::Patrol;
                Enemy.TargetEntity = 0;
            }
        } else if (Enemy.CurrentState == EnemyState::Attack) {
            if (distance > Enemy.AttackRange) {
                Enemy.CurrentState = EnemyState::Chase;
            }
        }
    }
}

void EnemySystem::UpdatePatrol(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    Math::Vec3 toTarget = Enemy.TargetPosition - rb.Position;

    if (toTarget.Magnitude() < 0.5f) {
        // Reached patrol point, pick new one
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> angleDis(0.0f, 3.14159265f * 2.0f);
        std::uniform_real_distribution<float> distDis(0.0f, Enemy.PatrolRadius);

        float angle = angleDis(gen);
        float dist = distDis(gen);
        Enemy.TargetPosition = Enemy.PatrolCenter + Math::Vec3(std::cos(angle) * dist, 0.0f, std::sin(angle) * dist);
    } else {
        // Move towards patrol point
        toTarget = toTarget.Normalized();
        rb.PendingForces.push_back(toTarget * Enemy.MoveSpeed * rb.Mass * 5.0f);
    }
}

void EnemySystem::UpdateChase(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;
    if (Enemy.TargetEntity == 0) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    Math::Vec3 toTarget = Enemy.TargetPosition - rb.Position;
    toTarget = toTarget.Normalized();

    rb.PendingForces.push_back(toTarget * Enemy.MoveSpeed * rb.Mass * 5.0f);
}

void EnemySystem::UpdateAttack(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime) {
    if (Enemy.TargetEntity == 0) return;
    if (Enemy.AttackTimer > 0.0f) return;

    // Perform attack
    float distance = GetDistanceToTarget(Registry, Entity, Enemy.TargetEntity);
    if (distance <= Enemy.AttackRange) {
        HealthSystem::ApplyDamage(Registry, Enemy.TargetEntity, Enemy.AttackDamage,
                                 Registry.Get<ECS::Transform>(Entity).Position, Entity);
        Enemy.AttackTimer = Enemy.AttackCooldown;
    }
}

void EnemySystem::UpdateFlee(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;
    if (Enemy.TargetEntity == 0) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);
    Math::Vec3 fromTarget = rb.Position - Enemy.TargetPosition;
    fromTarget = fromTarget.Normalized();

    rb.PendingForces.push_back(fromTarget * Enemy.MoveSpeed * rb.Mass * 5.0f);
}

} // namespace Solstice::Game
