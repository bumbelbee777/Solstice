#pragma once

#include "../../Solstice.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/Transform.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include "../../Core/Logic/FSM.hxx"
#include "../../Math/Vector.hxx"
#include <string>
#include <vector>
#include <unordered_map>

namespace Solstice::Game {

// Enemy AI states
enum class EnemyState {
    Idle,
    Patrol,
    Chase,
    Attack,
    Flee,
    Dead
};

// Enemy component
struct Enemy {
    EnemyState CurrentState{EnemyState::Idle};
    float AggroRange{10.0f};
    float AttackRange{2.0f};
    float PatrolRadius{5.0f};
    float MoveSpeed{3.0f};
    float AttackCooldown{1.0f};
    float AttackTimer{0.0f};
    float AttackDamage{10.0f};

    Math::Vec3 PatrolCenter{0.0f, 0.0f, 0.0f};
    Math::Vec3 TargetPosition{0.0f, 0.0f, 0.0f};
    ECS::EntityId TargetEntity{0};

    float DetectionAngle{90.0f}; // Vision cone angle in degrees
    float HearingRange{15.0f};

    // Drop table (ItemID -> drop chance 0.0-1.0)
    std::unordered_map<uint32_t, float> DropTable;

    // FSM for state management
    std::shared_ptr<Core::State> StateMachine;
};

// Enemy system
class SOLSTICE_API EnemySystem {
public:
    // Update enemy AI (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime);

    // Set enemy target
    static void SetTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity);

    // Get enemy state
    static EnemyState GetState(ECS::Registry& Registry, ECS::EntityId EnemyEntity);

    // Set enemy state
    static void SetState(ECS::Registry& Registry, ECS::EntityId EnemyEntity, EnemyState State);

    // Check if enemy can see target
    static bool CanSeeTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity);

    // Check if enemy can hear target
    static bool CanHearTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity);

    // Get distance to target
    static float GetDistanceToTarget(ECS::Registry& Registry, ECS::EntityId EnemyEntity, ECS::EntityId TargetEntity);

    // Process enemy death (drop loot, etc.)
    static void ProcessDeath(ECS::Registry& Registry, ECS::EntityId EnemyEntity);

private:
    static void UpdateState(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime);
    static void UpdatePatrol(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime);
    static void UpdateChase(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime);
    static void UpdateAttack(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime);
    static void UpdateFlee(ECS::Registry& Registry, ECS::EntityId Entity, Enemy& Enemy, float DeltaTime);
};

} // namespace Solstice::Game
