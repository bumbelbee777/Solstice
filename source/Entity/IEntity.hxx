#pragma once

#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <string>

namespace Solstice::ECS {
enum class EntityKind {
    NeutralNPC,
    HostileNPC,
    Player,
    OtherPlayer,
    Item,
    Environment,
    Other
};

struct IEntity {
    EntityKind Kind;
    std::string Name;
    Math::Vec3 Position, Scale;
    Math::Matrix4 Transform;

    virtual ~IEntity() = default;

    virtual void OnUpdate(float DeltaTime) = 0;
    virtual void OnRender() = 0;

    virtual void OnKill() = 0;
    virtual void OnDamage() = 0;
    virtual void OnHeal() = 0;
};
}
