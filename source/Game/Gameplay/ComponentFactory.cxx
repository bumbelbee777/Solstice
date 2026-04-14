#include "Gameplay/ComponentFactory.hxx"
#include "Core/Debug/Debug.hxx"

namespace Solstice::Game {

ECS::EntityId ComponentFactory::CreatePlayer(ECS::Registry& Registry, const Math::Vec3& Position,
                                             const std::string& Name) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Player});
    Registry.Add<ECS::PlayerTag>(entity, ECS::PlayerTag{});

    SIMPLE_LOG("ComponentFactory: Created player entity: " + Name);
    return entity;
}

ECS::EntityId ComponentFactory::CreateEnemy(ECS::Registry& Registry, const Math::Vec3& Position,
                                           const std::string& Name, ECS::EntityKind Kind) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{Kind});

    SIMPLE_LOG("ComponentFactory: Created enemy entity: " + Name);
    return entity;
}

Physics::RigidBody ComponentFactory::CreateRigidBodyBox(const Math::Vec3& Position, const Math::Vec3& HalfExtents,
                                                         float Mass, bool IsStatic) {
    Physics::RigidBody rb;
    rb.Position = Position;
    rb.Rotation = Math::Quaternion();
    rb.IsStatic = IsStatic;
    rb.SetMass(IsStatic ? 0.0f : Mass);
    rb.Type = Physics::ColliderType::Box;
    rb.HalfExtents = HalfExtents;
    rb.Friction = 0.5f;
    rb.Restitution = 0.0f;
    return rb;
}

Physics::RigidBody ComponentFactory::CreateRigidBodySphere(const Math::Vec3& Position, float Radius,
                                                          float Mass, bool IsStatic) {
    Physics::RigidBody rb;
    rb.Position = Position;
    rb.Rotation = Math::Quaternion();
    rb.IsStatic = IsStatic;
    rb.SetMass(IsStatic ? 0.0f : Mass);
    rb.Type = Physics::ColliderType::Sphere;
    rb.Radius = Radius;
    rb.Friction = 0.5f;
    rb.Restitution = 0.0f;
    return rb;
}

Physics::RigidBody ComponentFactory::CreateRigidBodyCapsule(const Math::Vec3& Position, float Height, float Radius,
                                                            float Mass, bool IsStatic) {
    Physics::RigidBody rb;
    rb.Position = Position;
    rb.Rotation = Math::Quaternion();
    rb.IsStatic = IsStatic;
    rb.SetMass(IsStatic ? 0.0f : Mass);
    rb.Type = Physics::ColliderType::Capsule;
    rb.CapsuleHeight = Height;
    rb.CapsuleRadius = Radius;
    rb.Friction = 0.5f;
    rb.Restitution = 0.0f;
    return rb;
}

ECS::EntityId ComponentFactory::CreateStaticBox(ECS::Registry& Registry, const Math::Vec3& Position,
                                               const Math::Vec3& HalfExtents, const std::string& Name) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Environment});

    auto rb = CreateRigidBodyBox(Position, HalfExtents, 0.0f, true);
    Registry.Add<Physics::RigidBody>(entity, rb);

    return entity;
}

ECS::EntityId ComponentFactory::CreateDynamicBox(ECS::Registry& Registry, const Math::Vec3& Position,
                                                const Math::Vec3& HalfExtents, float Mass,
                                                const std::string& Name) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Environment});

    auto rb = CreateRigidBodyBox(Position, HalfExtents, Mass, false);
    Registry.Add<Physics::RigidBody>(entity, rb);

    return entity;
}

ECS::EntityId ComponentFactory::CreateStaticSphere(ECS::Registry& Registry, const Math::Vec3& Position,
                                                   float Radius, const std::string& Name) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Environment});

    auto rb = CreateRigidBodySphere(Position, Radius, 0.0f, true);
    Registry.Add<Physics::RigidBody>(entity, rb);

    return entity;
}

ECS::EntityId ComponentFactory::CreateDynamicSphere(ECS::Registry& Registry, const Math::Vec3& Position,
                                                    float Radius, float Mass,
                                                    const std::string& Name) {
    ECS::EntityId entity = Registry.Create();

    Registry.Add<ECS::Transform>(entity, ECS::Transform{Position, Math::Vec3(1.0f, 1.0f, 1.0f)});
    Registry.Add<ECS::Name>(entity, ECS::Name{Name});
    Registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Environment});

    auto rb = CreateRigidBodySphere(Position, Radius, Mass, false);
    Registry.Add<Physics::RigidBody>(entity, rb);

    return entity;
}

} // namespace Solstice::Game
