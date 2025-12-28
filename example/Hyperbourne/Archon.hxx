#pragma once

#include <Solstice.hxx>
#include <Game/Enemy.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Physics/RigidBody.hxx>
#include <Math/Vector.hxx>
#include <string>
#include <vector>

namespace Solstice::Hyperbourne {

// Archon types
enum class ArchonType {
    Cruciform,  // Black upside-down cross, swarming tactics
    Shardhive   // Larger, shifting diamond-cube, spawns Cruciforms
};

// Archon-specific component extending Enemy
struct Archon {
    ArchonType Type{ArchonType::Cruciform};
    
    // Attack state
    enum class AttackState {
        Idle,
        FiringOrbs,      // Firing three crimson orbs
        ChargingBeam,    // Charging up crimson beam
        FiringBeam       // Firing the beam
    };
    AttackState CurrentAttackState{AttackState::Idle};
    
    // Attack timers
    float OrbFireTimer{0.0f};
    int OrbsFired{0};
    float BeamChargeTimer{0.0f};
    float BeamFireTimer{0.0f};
    
    // Attack parameters
    float OrbFireInterval{0.5f};    // Time between orb shots
    float BeamChargeTime{2.0f};    // Time to charge beam
    float BeamFireTime{1.5f};      // Time beam is active
    
    // Spawn system (for Shardhives)
    float SpawnTimer{0.0f};
    float SpawnInterval{10.0f};    // Time between spawning Cruciforms
    int MaxSpawnedCruciforms{5};   // Maximum Cruciforms this Shardhive can spawn
    
    // Visual effects
    float WingAnimationTime{0.0f}; // For Shardhive wing animation
    float ShapeShiftTime{0.0f};    // For Shardhive shape shifting
    
    // Visual representation
    uint32_t SceneObjectID{0};     // Scene object ID for visual mesh (0 = invalid)
};

// Archon system for updating Archon behaviors
class SOLSTICE_API ArchonSystem {
public:
    // Update all Archons
    static void Update(ECS::Registry& Registry, float DeltaTime, ECS::EntityId PlayerEntity);
    
    // Spawn an Archon
    static ECS::EntityId SpawnArchon(ECS::Registry& Registry, const Math::Vec3& Position, ArchonType Type);
    
    // Spawn a Cruciform at position (used by Shardhives)
    static ECS::EntityId SpawnCruciform(ECS::Registry& Registry, const Math::Vec3& Position);
};

} // namespace Solstice::Hyperbourne

