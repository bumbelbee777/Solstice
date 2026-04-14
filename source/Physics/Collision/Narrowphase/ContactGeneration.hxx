#pragma once

#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Collision/Narrowphase/CollisionResolution.hxx>
#include <Physics/Collision/Narrowphase/GJK.hxx>
#include <Math/Vector.hxx>
#include <vector>

namespace Solstice::Physics {

/// Contact generation functions for all collision types
namespace ContactGeneration {

/// Generate contacts for a body colliding with ground plane (y=0)
/// Returns true if contacts were generated
bool GenerateGroundContacts(RigidBody& body, const Math::Vec3& groundNormal, float groundDistance,
                            CollisionResolution::ContactManifold& outManifold);

/// Generate contacts between two bodies
/// Returns true if contacts were generated
bool GenerateBodyContacts(RigidBody* A, RigidBody* B, CollisionResolution::ContactManifold& outManifold);

/// Separating Axis Theorem for Oriented Bounding Boxes
/// Returns true if collision detected
bool TestSATBoxVsBox(const RigidBody& A, const RigidBody& B,
                     Math::Vec3& outNormal, float& outPenetration,
                     std::vector<CollisionResolution::ContactPoint>& outContacts);

} // namespace ContactGeneration

} // namespace Solstice::Physics
