#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "../Core/System/Async.hxx"
#include <functional>
#include "../Math/Vector.hxx"
#include "ReactPhysics3DBridge.hxx"
#include "CCD.hxx"
#include "CollisionResolution.hxx"
#include "IterativeSolver.hxx"
#include "ManifoldManager.hxx"

namespace Solstice::Physics {

// Forward declaration
class FluidSimulation;

class SOLSTICE_API PhysicsSystem {
public:
    static PhysicsSystem& Instance() {
        static PhysicsSystem instance;
        return instance;
    }

    void Start(Solstice::ECS::Registry& registry);
    void Stop();
    bool IsRunning() const { return m_Running; }
    Solstice::ECS::Registry* GetRegistry() const { return m_Registry; }
    bool IsBoundTo(const Solstice::ECS::Registry& registry) const { return m_Registry == &registry; }

    // Submits a physics step to the job system
    void UpdateAsync(float dt);

    // Synchronous update for simple integration
    void Update(float dt);

    // Solver configuration
    void SetVelocityIterations(int iterations);
    void SetPositionIterations(int iterations);
    void SetMaxStepDt(float maxDt) { m_MaxStepDt = maxDt; }
    float GetMaxStepDt() const { return m_MaxStepDt; }

    // Fluid simulation management
    void RegisterFluidSimulation(FluidSimulation* fluid);
    void UnregisterFluidSimulation(FluidSimulation* fluid);

    // Get the ReactPhysics3D bridge
    ReactPhysics3DBridge& GetBridge() { return m_Bridge; }

    // Check if a body would collide when moved from prevPos to targetPos
    // Returns true if collision detected, and outputs collision point and normal
    static bool CheckSweptCollision(RigidBody* Body, const Math::Vec3& PrevPos, const Math::Vec3& TargetPos,
                                    Math::Vec3& OutCollisionPoint, Math::Vec3& OutCollisionNormal);

private:
    PhysicsSystem() = default;

    void IntegrateVelocity(float dt);
    void IntegratePosition(float dt);
    void UpdateBroadphase();
    void ResolveCollisions();
    void UpdateFluidSimulations(float dt);
    void UpdateSleepState();
    // PerformCCD moved to CCD class


    // CCD helper functions moved to CCD class

    Solstice::ECS::Registry* m_Registry{nullptr};
    bool m_Running{false};

    // ReactPhysics3D bridge
    ReactPhysics3DBridge m_Bridge;

    // Iterative constraint solver (deprecated, kept for backward compatibility)
    CollisionResolution::IterativeSolver m_Solver;

    // Persistent manifold manager (deprecated, kept for backward compatibility)
    ManifoldManager m_ManifoldManager;

    // Bitmask broadphase grid (deprecated, using BVH now)
    static constexpr int GRID_SIZE = 64;
    static constexpr float GRID_CELL_SIZE = 2.0f;
    std::vector<uint64_t> m_BroadphaseMasks;

    // Fluid simulations
    std::vector<FluidSimulation*> m_FluidSimulations;

    float m_MaxStepDt{1.0f / 30.0f};

    // Functional integration strategy type
    using IntegrationStrategy = std::function<void(struct RigidBody&, float)>;

    // Deprecated: replaced by manifold collection + iterative solver
    void ResolveContact(struct RigidBody& A, struct RigidBody& B, const Math::Vec3& contactPoint, const Math::Vec3& n, float penetration, bool applyPositionalCorrection = true);
};

}
