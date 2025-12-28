#include "Archon.hxx"
#include <Game/Enemy.hxx>
#include <Game/Health.hxx>
#include <Core/Debug.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::Hyperbourne {

void ArchonSystem::Update(ECS::Registry& Registry, float DeltaTime, ECS::EntityId PlayerEntity) {
    Registry.ForEach<Archon, Game::Enemy>([&](ECS::EntityId entity, Archon& archon, Game::Enemy& enemy) {
        // Get Transform component
        if (!Registry.Has<ECS::Transform>(entity)) return;
        auto& transform = Registry.Get<ECS::Transform>(entity);
        
        // Update attack state machine
        switch (archon.CurrentAttackState) {
            case Archon::AttackState::Idle:
                // Check if player is in range
                if (PlayerEntity != 0 && Registry.Has<ECS::Transform>(PlayerEntity)) {
                    auto& playerTransform = Registry.Get<ECS::Transform>(PlayerEntity);
                    Math::Vec3 toPlayer = playerTransform.Position - transform.Position;
                    float distance = toPlayer.Length();
                    
                    if (distance <= enemy.AttackRange) {
                        // Start attack sequence
                        archon.CurrentAttackState = Archon::AttackState::FiringOrbs;
                        archon.OrbsFired = 0;
                        archon.OrbFireTimer = 0.0f;
                    }
                }
                break;
                
            case Archon::AttackState::FiringOrbs:
                archon.OrbFireTimer += DeltaTime;
                if (archon.OrbFireTimer >= archon.OrbFireInterval) {
                    // Fire an orb (visual effect would be created here)
                    archon.OrbsFired++;
                    archon.OrbFireTimer = 0.0f;
                    
                    if (archon.OrbsFired >= 3) {
                        // All orbs fired, start charging beam
                        archon.CurrentAttackState = Archon::AttackState::ChargingBeam;
                        archon.BeamChargeTimer = 0.0f;
                    }
                }
                break;
                
            case Archon::AttackState::ChargingBeam:
                archon.BeamChargeTimer += DeltaTime;
                if (archon.BeamChargeTimer >= archon.BeamChargeTime) {
                    // Beam charged, start firing
                    archon.CurrentAttackState = Archon::AttackState::FiringBeam;
                    archon.BeamFireTimer = 0.0f;
                }
                break;
                
            case Archon::AttackState::FiringBeam:
                archon.BeamFireTimer += DeltaTime;
                // Apply damage to player if in beam path (simplified)
                if (PlayerEntity != 0 && Registry.Has<Game::Health>(PlayerEntity)) {
                    auto& playerHealth = Registry.Get<Game::Health>(PlayerEntity);
                    // Damage per second while in beam
                    playerHealth.CurrentHealth -= enemy.AttackDamage * DeltaTime;
                    playerHealth.CurrentHealth = std::max(0.0f, playerHealth.CurrentHealth);
                }
                
                if (archon.BeamFireTimer >= archon.BeamFireTime) {
                    // Beam finished, return to idle
                    archon.CurrentAttackState = Archon::AttackState::Idle;
                }
                break;
        }
        
        // Update Shardhive spawn system
        if (archon.Type == ArchonType::Shardhive) {
            archon.SpawnTimer += DeltaTime;
            if (archon.SpawnTimer >= archon.SpawnInterval) {
                // Spawn a Cruciform
                SpawnCruciform(Registry, transform.Position + Math::Vec3(0, 2.0f, 0));
                archon.SpawnTimer = 0.0f;
            }
            
            // Update wing animation
            archon.WingAnimationTime += DeltaTime;
            
            // Update shape shifting animation
            archon.ShapeShiftTime += DeltaTime;
        }
        
        // Update enemy movement (swarming for Cruciforms)
        if (archon.Type == ArchonType::Cruciform && PlayerEntity != 0) {
            // Swarming behavior: move towards player but coordinate with nearby Cruciforms
            if (Registry.Has<ECS::Transform>(PlayerEntity)) {
                auto& playerTransform = Registry.Get<ECS::Transform>(PlayerEntity);
                Math::Vec3 toPlayer = playerTransform.Position - transform.Position;
                float distance = toPlayer.Length();
                
                if (distance > 0.1f) {
                    Math::Vec3 direction = toPlayer / distance;
                    // Apply swarming offset (avoid clustering)
                    // Find nearby Cruciforms and push away slightly
                    Math::Vec3 swarmOffset(0, 0, 0);
                    Registry.ForEach<Archon, ECS::Transform>([&](ECS::EntityId otherEntity, Archon& otherArchon, ECS::Transform& otherTransform) {
                        if (otherEntity != entity && otherArchon.Type == ArchonType::Cruciform) {
                            Math::Vec3 toOther = otherTransform.Position - transform.Position;
                            float otherDist = toOther.Length();
                            if (otherDist < 5.0f && otherDist > 0.1f) {
                                // Push away from nearby Cruciforms
                                swarmOffset = swarmOffset - (toOther / otherDist) * (1.0f / otherDist);
                            }
                        }
                    });
                    
                    // Combine player direction with swarm offset
                    direction = direction + swarmOffset * 0.3f;
                    direction = direction.Normalized();
                    
                    // Update enemy target position for movement system
                    enemy.TargetPosition = transform.Position + direction * enemy.MoveSpeed * DeltaTime;
                }
            }
        }
    });
}

ECS::EntityId ArchonSystem::SpawnArchon(ECS::Registry& Registry, const Math::Vec3& Position, ArchonType Type) {
    ECS::EntityId entity = Registry.Create();
    
    // Add Transform
    ECS::Transform transform;
    transform.Position = Position;
    transform.Scale = Math::Vec3(1, 1, 1);
    transform.Matrix = Math::Matrix4::Identity();
    Registry.Add<ECS::Transform>(entity, transform);
    
    // Add Enemy component
    Game::Enemy enemy;
    enemy.CurrentState = Game::EnemyState::Idle;
    enemy.AggroRange = (Type == ArchonType::Shardhive) ? 25.0f : 15.0f;
    enemy.AttackRange = (Type == ArchonType::Shardhive) ? 20.0f : 12.0f;
    enemy.MoveSpeed = (Type == ArchonType::Shardhive) ? 2.0f : 5.0f; // Shardhives slower
    enemy.AttackDamage = (Type == ArchonType::Shardhive) ? 20.0f : 10.0f;
    Registry.Add<Game::Enemy>(entity, enemy);
    
    // Add Archon component
    Archon archon;
    archon.Type = Type;
    if (Type == ArchonType::Shardhive) {
        archon.SpawnInterval = 10.0f;
        archon.MaxSpawnedCruciforms = 5;
    }
    Registry.Add<Archon>(entity, archon);
    
    // Add Health
    Game::Health health;
    health.CurrentHealth = (Type == ArchonType::Shardhive) ? 200.0f : 50.0f;
    health.MaxHealth = health.CurrentHealth;
    Registry.Add<Game::Health>(entity, health);
    
    // Add Physics RigidBody
    Physics::RigidBody rb;
    rb.Position = Position;
    rb.Rotation = Math::Quaternion();
    rb.IsStatic = false;
    rb.SetMass((Type == ArchonType::Shardhive) ? 100.0f : 10.0f);
    rb.Type = Physics::ColliderType::Box;
    rb.HalfExtents = (Type == ArchonType::Shardhive) ? Math::Vec3(1.5f, 1.5f, 1.5f) : Math::Vec3(0.5f, 1.0f, 0.5f);
    rb.Friction = 0.5f;
    rb.Restitution = 0.0f;
    Registry.Add<Physics::RigidBody>(entity, rb);
    
    SIMPLE_LOG("ArchonSystem: Spawned " + std::string(Type == ArchonType::Shardhive ? "Shardhive" : "Cruciform") + " at (" +
               std::to_string(Position.x) + ", " + std::to_string(Position.y) + ", " + std::to_string(Position.z) + ")");
    
    return entity;
}

ECS::EntityId ArchonSystem::SpawnCruciform(ECS::Registry& Registry, const Math::Vec3& Position) {
    return SpawnArchon(Registry, Position, ArchonType::Cruciform);
}

} // namespace Solstice::Hyperbourne

