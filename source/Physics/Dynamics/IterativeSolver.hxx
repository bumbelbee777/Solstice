#pragma once

#include <Physics/Collision/Narrowphase/CollisionResolution.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <vector>
#include <cstddef>
#include <unordered_map>

namespace Solstice::Physics {
namespace CollisionResolution {

/// Iterative constraint solver using Sequential Impulse method
class IterativeSolver {
public:
    IterativeSolver();
    ~IterativeSolver();

    /// Initialize solver with contact manifolds
    void Initialize(const std::vector<ContactManifold>& manifolds, float dt);

    /// Apply cached impulses from previous frame
    void WarmStart();

    /// Store current impulses to cache for next frame
    void StoreImpulses();

    /// Solve velocity constraints (normal and friction impulses) - single iteration
    void SolveVelocity();

    /// Solve position constraints (Baumgarte stabilization)
    void SolvePosition();

    /// Clear all constraints
    void Clear();

    /// Set solver parameters
    void SetVelocityIterations(int iterations) { m_VelocityIterations = iterations; }
    void SetPositionIterations(int iterations) { m_PositionIterations = iterations; }
    void SetBaumgarteCoefficient(float coeff) { m_BaumgarteCoefficient = coeff; }
    void SetRelaxationFactor(float factor) { m_RelaxationFactor = factor; }

    /// Get solver parameters
    int GetVelocityIterations() const { return m_VelocityIterations; }
    int GetPositionIterations() const { return m_PositionIterations; }

    void SetSingleThreaded(bool v) { m_ForceSingleThreaded = v; }
    bool IsSingleThreaded() const { return m_ForceSingleThreaded; }

private:
    struct CachedImpulse {
        float NormalImpulse = 0.0f;
        float TangentImpulse1 = 0.0f;
        float TangentImpulse2 = 0.0f;
        int LifeTime = 0;
    };

    std::vector<ContactConstraint> m_Constraints;
    std::unordered_map<ContactID, CachedImpulse, ContactIDHash> m_ImpulseCache;

    int m_VelocityIterations{8};      // Reduced from 10
    int m_PositionIterations{2};       // Reduced from 3
    float m_BaumgarteCoefficient{0.2f};
    float m_RelaxationFactor{1.0f};
    float m_DeltaTime{0.016f};
    bool m_ForceSingleThreaded{false};

    float ComputeEffectiveMass(const RigidBody& bodyA, const RigidBody& bodyB,
                               const Math::Vec3& rA, const Math::Vec3& rB,
                               const Math::Vec3& direction);

    Math::Vec3 ApplyWorldInertia(const RigidBody& rb, const Math::Vec3& v);

    void ComputeTangentBasis(const Math::Vec3& normal, Math::Vec3& tangent1, Math::Vec3& tangent2);
};

} // namespace CollisionResolution
} // namespace Solstice::Physics

