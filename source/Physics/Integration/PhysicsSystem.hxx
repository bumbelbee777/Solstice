#pragma once

#include <Solstice.hxx>
#include <Entity/Registry.hxx>
#include <Core/System/Async.hxx>
#include <functional>
#include <Math/Vector.hxx>
#include <Physics/Integration/ReactPhysics3DBridge.hxx>
#include <Physics/Integration/PhysicsBackend.hxx>
#include <Physics/Collision/Broadphase/CCD.hxx>
#include <Physics/Collision/Narrowphase/CollisionResolution.hxx>
#include <Physics/Dynamics/IterativeSolver.hxx>
#include <Physics/Dynamics/SoftBody.hxx>
#include <Physics/Dynamics/Vehicle.hxx>
#include <Physics/Collision/Narrowphase/ManifoldManager.hxx>

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
    const ReactPhysics3DBridge& GetBridge() const { return m_Bridge; }
    IPhysicsBackend& GetBackend() { return m_Bridge; }
    const IPhysicsBackend& GetBackend() const { return m_Bridge; }

    // Backend-neutral query APIs
    RaycastHit RaycastClosest(const RaycastRequest& request);
    bool RaycastAny(const RaycastRequest& request);
    std::vector<RaycastAllHit> RaycastAll(const RaycastRequest& request);
    std::vector<ECS::EntityId> OverlapSphere(const Math::Vec3& center, float radius);
    std::vector<ECS::EntityId> OverlapAabb(const Math::Vec3& min, const Math::Vec3& max);
    PhysicsDebugDrawData GetDebugDrawData();

    // Scheduler policy
    void SetTaskSchedulerPolicy(const PhysicsTaskSchedulerPolicy& policy) { m_TaskScheduler.SetPolicy(policy); }
    const PhysicsTaskSchedulerPolicy& GetTaskSchedulerPolicy() const { return m_TaskScheduler.GetPolicy(); }

    // Future hooks (stubs)
    bool SupportsVehicles() const { return true; }
    bool SupportsSoftBodies() const { return true; }
    void CreateVehicleStub(ECS::EntityId entityId, const VehicleConfig& config);
    void CreateSoftBodyStub(ECS::EntityId entityId, const SoftBodyConfig& config);

    /// Blends from the per-substep snapshot toward the current simulated pose when building the render scene
    /// (see Render::SyncPhysicsToScene). Fixed-step games set this from the frame's physics accumulator; default 1.
    void SetSceneRenderBlendForSync(float t) { m_SceneRenderBlendT = t; }
    float GetSceneRenderBlendForSync() const { return m_SceneRenderBlendT; }

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
    void UpdateVehicles(float dt);
    void UpdateSoftBodies(float dt);
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
    float m_FixedStepDt{1.0f / 60.0f};
    float m_StepAccumulator{0.0f};
    int m_MaxSubStepsPerFrame{4};

    // Render: blend from substep snapshot to current pose (0..1, typically 1 - accumulator / fixedStep).
    float m_SceneRenderBlendT{1.0f};
    PhysicsTaskScheduler m_TaskScheduler{};

    // Functional integration strategy type
    using IntegrationStrategy = std::function<void(struct RigidBody&, float)>;

    // Deprecated: replaced by manifold collection + iterative solver
    void ResolveContact(struct RigidBody& A, struct RigidBody& B, const Math::Vec3& contactPoint, const Math::Vec3& n, float penetration, bool applyPositionalCorrection = true);
};

}
