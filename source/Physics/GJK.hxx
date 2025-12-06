#pragma once

#include "../Math/Vector.hxx"
#include "RigidBody.hxx"
#include "CollisionResolution.hxx"
#include <vector>

namespace Solstice::Physics {

/// Simplex for GJK algorithm (1D, 2D, 3D, or 4D point set)
struct Simplex {
    Math::Vec3 Points[4];  // Support points in Minkowski difference
    int Count;             // Number of points (1-4)
    
    Simplex() : Count(0) {}
    
    void Add(const Math::Vec3& point) {
        if (Count < 4) {
            Points[Count++] = point;
        }
    }
    
    Math::Vec3& operator[](int index) { return Points[index]; }
    const Math::Vec3& operator[](int index) const { return Points[index]; }
    
    void Clear() { Count = 0; }
};

/// GJK Algorithm: Returns true if collision detected
/// direction: output direction for EPA (if collision)
bool GJK(const RigidBody& A, const RigidBody& B, Simplex& simplex);

/// EPA Algorithm: Computes penetration depth and normal
/// Requires simplex from GJK containing the origin
/// Returns true if successful, false if error
bool EPA(const Simplex& initialSimplex, const RigidBody& A, const RigidBody& B,
         Math::Vec3& outNormal, float& outPenetration);

/// Helper: Get support point for a rigid body in given direction (world space)
Math::Vec3 GetSupportPoint(const RigidBody& rb, const Math::Vec3& direction);

/// Helper: Get support point in Minkowski difference A - B
Math::Vec3 GetMinkowskiSupport(const RigidBody& A, const RigidBody& B, const Math::Vec3& direction);

/// Helper: Update simplex and search direction (returns true if origin enclosed)
bool UpdateSimplex(Simplex& simplex, Math::Vec3& direction);

} // namespace Solstice::Physics
