#pragma once

#include "RigidBody.hxx"
#include "../Entity/Registry.hxx"

namespace Solstice::Physics {

class CCD {
public:
    static void PerformCCD(Solstice::ECS::Registry& registry, float dt);

    // Swept sphere vs static geometry
    // Returns TOI (time of impact) in [0, 1], or >1 if no collision
    static float SweptSphereCast(const RigidBody& sphere, const Math::Vec3& motion, const RigidBody& target);
};

} // namespace Solstice::Physics
