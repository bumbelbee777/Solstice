#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "../Entity/Transform.hxx"
#include "../Entity/Name.hxx"
#include "../Entity/Kind.hxx"
#include "../Entity/PlayerTag.hxx"
#include "../Physics/RigidBody.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include <string>

namespace Solstice::Game {

class SOLSTICE_API ComponentFactory {
public:
    // Player entity creation
    static ECS::EntityId CreatePlayer(ECS::Registry& Registry, const Math::Vec3& Position = Math::Vec3(0, 0, 0),
                                      const std::string& Name = "Player");

    // Enemy entity creation
    static ECS::EntityId CreateEnemy(ECS::Registry& Registry, const Math::Vec3& Position = Math::Vec3(0, 0, 0),
                                     const std::string& Name = "Enemy", ECS::EntityKind Kind = ECS::EntityKind::HostileNPC);

    // Physics body creation helpers
    static Physics::RigidBody CreateRigidBodyBox(const Math::Vec3& Position, const Math::Vec3& HalfExtents,
                                                 float Mass = 1.0f, bool IsStatic = false);
    static Physics::RigidBody CreateRigidBodySphere(const Math::Vec3& Position, float Radius,
                                                   float Mass = 1.0f, bool IsStatic = false);
    static Physics::RigidBody CreateRigidBodyCapsule(const Math::Vec3& Position, float Height, float Radius,
                                                     float Mass = 1.0f, bool IsStatic = false);

    // Common object presets
    static ECS::EntityId CreateStaticBox(ECS::Registry& Registry, const Math::Vec3& Position,
                                         const Math::Vec3& HalfExtents, const std::string& Name = "StaticBox");
    static ECS::EntityId CreateDynamicBox(ECS::Registry& Registry, const Math::Vec3& Position,
                                          const Math::Vec3& HalfExtents, float Mass = 1.0f,
                                          const std::string& Name = "DynamicBox");
    static ECS::EntityId CreateStaticSphere(ECS::Registry& Registry, const Math::Vec3& Position,
                                            float Radius, const std::string& Name = "StaticSphere");
    static ECS::EntityId CreateDynamicSphere(ECS::Registry& Registry, const Math::Vec3& Position,
                                            float Radius, float Mass = 1.0f,
                                            const std::string& Name = "DynamicSphere");
};

} // namespace Solstice::Game
