#pragma once

#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include "RigidBody.hxx"
#include <vector>
#include <cstddef>
#include <unordered_map>
#include <functional>

namespace Solstice::Physics {

// Forward declarations
struct RigidBody;

namespace CollisionResolution {
/// Contact point representing a single point of contact between two bodies
struct ContactPoint {
    Math::Vec3 Position;           // World space position of contact
    Math::Vec3 Normal;              // Contact normal (from B to A)
    float Penetration;              // Penetration depth
    Math::Vec3 LocalPointA;         // Contact point in A's local space
    Math::Vec3 LocalPointB;         // Contact point in B's local space
    
    ContactPoint() 
        : Position(0,0,0), Normal(0,1,0), Penetration(0.0f),
          LocalPointA(0,0,0), LocalPointB(0,0,0) {}
};

/// Contact manifold representing all contact points between two rigid bodies
struct ContactManifold {
    RigidBody* BodyA{nullptr};
    RigidBody* BodyB{nullptr};
    std::vector<ContactPoint> Contacts;
    float Friction{0.5f};           // Combined friction coefficient
    float Restitution{0.5f};        // Combined restitution coefficient
    
    ContactManifold() = default;
    
    void AddContact(const ContactPoint& contact) {
        // Limit to 4 contacts per manifold for stability
        if (Contacts.size() < 4) {
            Contacts.push_back(contact);
        }
    }
    
    void Clear() {
        Contacts.clear();
    }
};

// Simple hash for contact identification
struct ContactID {
    RigidBody* BodyA;
    RigidBody* BodyB;
    // In a full engine, we'd use feature IDs. For now, we'll rely on the fact 
    // that we usually have persistent manifold pointers or we can hash positions roughly.
    // Actually, since we rebuild manifolds, we need a way to match them.
    // Let's use Body pair + approximate local position on A.
    // But for box-box, we have multiple contacts.
    // Let's use an index or ID from the manifold generation if possible.
    // For this implementation, we will use a simple spatial hash of the contact point relative to Body A.
    size_t FeatureHash; 

    bool operator==(const ContactID& other) const {
        return BodyA == other.BodyA && BodyB == other.BodyB && FeatureHash == other.FeatureHash;
    }
};

struct ContactIDHash {
    size_t operator()(const ContactID& id) const {
        // Simple hash combination
        size_t h1 = std::hash<RigidBody*>{}(id.BodyA);
        size_t h2 = std::hash<RigidBody*>{}(id.BodyB);
        size_t h3 = id.FeatureHash;
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

/// Contact constraint for iterative solver
struct ContactConstraint {
    RigidBody* BodyA{nullptr};
    RigidBody* BodyB{nullptr};
    
    Math::Vec3 ContactPoint;        // World space contact point
    Math::Vec3 Normal;              // Contact normal (B to A)
    float Penetration;              // Penetration depth
    
    // Contact space basis
    Math::Vec3 Tangent1;            // First tangent direction
    Math::Vec3 Tangent2;            // Second tangent direction
    
    // Relative position vectors from body centers to contact point
    Math::Vec3 RA;
    Math::Vec3 RB;
    
    // Effective mass for normal and tangent directions
    float NormalMass;
    float TangentMass1;
    float TangentMass2;
    
    // Velocity bias for restitution and Baumgarte stabilization
    float VelocityBias;
    
    // Accumulated impulses for warm starting
    float NormalImpulse{0.0f};
    float TangentImpulse1{0.0f};
    float TangentImpulse2{0.0f};
    
    // Material properties
    float Friction{0.5f};
    float Restitution{0.5f};

    // ID for caching
    ContactID ID;
    
    ContactConstraint() 
        : ContactPoint(0,0,0), Normal(0,1,0), Penetration(0.0f),
          Tangent1(1,0,0), Tangent2(0,0,1),
          RA(0,0,0), RB(0,0,0),
          NormalMass(0.0f), TangentMass1(0.0f), TangentMass2(0.0f),
          VelocityBias(0.0f) {}
};

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
    
    /// Solve velocity constraints (normal and friction impulses)
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
    
private:
    struct CachedImpulse {
        float NormalImpulse = 0.0f;
        float TangentImpulse1 = 0.0f;
        float TangentImpulse2 = 0.0f;
        int LifeTime = 0; // For cache eviction
    };

    std::vector<ContactConstraint> m_Constraints;
    std::unordered_map<ContactID, CachedImpulse, ContactIDHash> m_ImpulseCache;
    
    // Solver parameters
    int m_VelocityIterations{10};      // Number of velocity constraint iterations
    int m_PositionIterations{3};       // Number of position constraint iterations
    float m_BaumgarteCoefficient{0.2f}; // Position correction strength (0.1-0.3)
    float m_RelaxationFactor{1.0f};     // Relaxation factor for SOR (1.0 = Gauss-Seidel)
    float m_DeltaTime{0.016f};          // Physics timestep
    bool m_ForceSingleThreaded{false};   // If true, force single-threaded solver for debugging
    
    // Helper: Compute effective mass for a constraint direction
    float ComputeEffectiveMass(const RigidBody& bodyA, const RigidBody& bodyB,
                               const Math::Vec3& rA, const Math::Vec3& rB,
                               const Math::Vec3& direction);
    
    // Helper: Apply world-space inertia tensor
    Math::Vec3 ApplyWorldInertia(const RigidBody& rb, const Math::Vec3& v);
    
    // Helper: Build orthonormal basis from normal
    void ComputeTangentBasis(const Math::Vec3& normal, Math::Vec3& tangent1, Math::Vec3& tangent2);
public:
    /// Debug helper: force single-threaded execution (can also be toggled via SOLSTICE_SINGLE_THREAD_SOLVER env var)
    void SetSingleThreaded(bool v) { m_ForceSingleThreaded = v; }
    bool IsSingleThreaded() const { return m_ForceSingleThreaded; }
};
} // namespace CollisionResolution
} // namespace Solstice::Physics
