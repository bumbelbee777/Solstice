#pragma once

#include "../SolsticeExport.hxx"
#include "../Entity/Registry.hxx"
#include "../Core/Async.hxx"
#include <functional>
#include "../Math/Vector.hxx"
#include "CCD.hxx"
#include "CollisionResolution.hxx"

namespace Solstice::Physics {

class SOLSTICE_API PhysicsSystem {
public:
    static PhysicsSystem& Instance() {
        static PhysicsSystem instance;
        return instance;
    }

    void Start(Solstice::ECS::Registry& registry);
    void Stop();
    
    // Submits a physics step to the job system
    void UpdateAsync(float dt);
    
    // Synchronous update for simple integration
    void Update(float dt);
    
    // Solver configuration
    void SetVelocityIterations(int iterations) { m_Solver.SetVelocityIterations(iterations); }
    void SetPositionIterations(int iterations) { m_Solver.SetPositionIterations(iterations); }

private:
    PhysicsSystem() = default;
    
    void IntegrateVelocity(float dt);
    void IntegratePosition(float dt);
    void UpdateBroadphase();
    void ResolveCollisions();
    // PerformCCD moved to CCD class
    
    // SAT helper functions
    bool TestSATBoxVsBox(const struct RigidBody& A, const struct RigidBody& B,
                         Math::Vec3& outNormal, float& outPenetration,
                         std::vector<CollisionResolution::ContactPoint>& outContacts);
    
    // CCD helper functions moved to CCD class

    Solstice::ECS::Registry* m_Registry{nullptr};
    bool m_Running{false};
    
    // Iterative constraint solver
    CollisionResolution::IterativeSolver m_Solver;
    
    // Bitmask broadphase grid (deprecated, using BVH now)
    static constexpr int GRID_SIZE = 64;
    static constexpr float GRID_CELL_SIZE = 2.0f;
    std::vector<uint64_t> m_BroadphaseMasks;
    
    // Functional integration strategy type
    using IntegrationStrategy = std::function<void(struct RigidBody&, float)>;
    
    // Deprecated: replaced by manifold collection + iterative solver
    void ResolveContact(struct RigidBody& A, struct RigidBody& B, const Math::Vec3& contactPoint, const Math::Vec3& n, float penetration, bool applyPositionalCorrection = true);
};

}
