#pragma once

#include <Solstice.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Entity/EntityId.hxx>
#include <vector>
#include <cstdint>

namespace Solstice::Physics {

enum class PhysicsExecutionBackend : uint8_t {
    CPU = 0,
    GPU = 1,
    IntegratedGPU = 2,
};

enum class PhysicsTaskClass : uint8_t {
    SimulationCritical = 0,
    QueryBatch = 1,
    DebugExtraction = 2,
};

struct PhysicsDispatchDecision {
    PhysicsExecutionBackend Backend{PhysicsExecutionBackend::CPU};
    const char* FallbackReason{"DefaultCpuPolicy"};
};

struct PhysicsTaskSchedulerPolicy {
    bool PreferGpuForQueries{true};
    bool PreferIntegratedGpu{true};
    bool DeterministicCpuOnly{true};
};

class SOLSTICE_API PhysicsTaskScheduler {
public:
    void SetPolicy(const PhysicsTaskSchedulerPolicy& policy) { m_Policy = policy; }
    const PhysicsTaskSchedulerPolicy& GetPolicy() const { return m_Policy; }

    PhysicsDispatchDecision Decide(PhysicsTaskClass taskClass, bool gpuAvailable, bool integratedGpuAvailable) const {
        if (m_Policy.DeterministicCpuOnly) {
            return {PhysicsExecutionBackend::CPU, "DeterministicCpuOnly"};
        }

        if (taskClass == PhysicsTaskClass::SimulationCritical) {
            return {PhysicsExecutionBackend::CPU, "SimulationCriticalCpuPath"};
        }

        if (taskClass == PhysicsTaskClass::QueryBatch && m_Policy.PreferGpuForQueries && gpuAvailable) {
            return {PhysicsExecutionBackend::GPU, "QueryBatchGpuPreferred"};
        }

        if (m_Policy.PreferIntegratedGpu && integratedGpuAvailable) {
            return {PhysicsExecutionBackend::IntegratedGPU, "IntegratedGpuPreferred"};
        }

        if (gpuAvailable) {
            return {PhysicsExecutionBackend::GPU, "DiscreteGpuAvailable"};
        }

        return {PhysicsExecutionBackend::CPU, "NoGpuProvider"};
    }

private:
    PhysicsTaskSchedulerPolicy m_Policy{};
};

struct RaycastRequest {
    Math::Vec3 Origin{0.0f, 0.0f, 0.0f};
    Math::Vec3 Direction{0.0f, -1.0f, 0.0f};
    float MaxDistance{1000.0f};
};

struct RaycastHit {
    bool Hit{false};
    ECS::EntityId Entity{0};
    Math::Vec3 Point{0.0f, 0.0f, 0.0f};
    Math::Vec3 Normal{0.0f, 1.0f, 0.0f};
    float Distance{0.0f};
};

struct RaycastAllHit {
    ECS::EntityId Entity{0};
    Math::Vec3 Point{0.0f, 0.0f, 0.0f};
    Math::Vec3 Normal{0.0f, 1.0f, 0.0f};
    float Distance{0.0f};
};

struct PhysicsDebugLine {
    Math::Vec3 P1{0.0f, 0.0f, 0.0f};
    Math::Vec3 P2{0.0f, 0.0f, 0.0f};
    uint32_t Color1{0xFFFFFFFFu};
    uint32_t Color2{0xFFFFFFFFu};
};

struct PhysicsDebugTriangle {
    Math::Vec3 P1{0.0f, 0.0f, 0.0f};
    Math::Vec3 P2{0.0f, 0.0f, 0.0f};
    Math::Vec3 P3{0.0f, 0.0f, 0.0f};
    uint32_t Color1{0xFFFFFFFFu};
    uint32_t Color2{0xFFFFFFFFu};
    uint32_t Color3{0xFFFFFFFFu};
};

struct PhysicsDebugDrawData {
    std::vector<PhysicsDebugLine> Lines;
    std::vector<PhysicsDebugTriangle> Triangles;
};

// Placeholder primitives for later milestones.
struct VehicleConfig {
    float WheelBase{2.5f};
    float TrackWidth{1.6f};
    float Mass{1200.0f};
    float EngineForce{9000.0f};
    float BrakeForce{6000.0f};
    float MaxSteerAngleRadians{0.55f};
};

struct WheelState {
    float SuspensionLength{0.25f};
    float AngularSpeed{0.0f};
};

struct SoftBodyNode {
    Math::Vec3 Position{0.0f, 0.0f, 0.0f};
    Math::Vec3 PredictedPosition{0.0f, 0.0f, 0.0f};
    Math::Vec3 Velocity{0.0f, 0.0f, 0.0f};
    float InverseMass{1.0f};
};

struct SoftBodyConstraint {
    uint32_t NodeA{0};
    uint32_t NodeB{0};
    float RestLength{0.0f};
};

struct SoftBodyConfig {
    float NodeMass{1.0f};
    float Damping{0.05f};
    float StructuralStiffness{0.95f};
    float ShearStiffness{0.85f};
    float BendStiffness{0.6f};
    int SolverIterations{8};
    uint32_t GridWidth{6};
    uint32_t GridHeight{6};
    float NodeSpacing{0.25f};
    bool AnchorTopRow{true};
    bool EnableSelfCollision{false};
};

class SOLSTICE_API IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    virtual void Update(float dt) = 0;
    virtual void SyncToBackend() = 0;
    virtual void SyncFromBackend() = 0;
    virtual void SetSolverIterations(int velocityIterations, int positionIterations) = 0;

    virtual RaycastHit RaycastClosest(const RaycastRequest& request) = 0;
    virtual bool RaycastAny(const RaycastRequest& request) = 0;
    virtual std::vector<RaycastAllHit> RaycastAll(const RaycastRequest& request) = 0;
    virtual std::vector<ECS::EntityId> OverlapSphere(const Math::Vec3& center, float radius) = 0;
    virtual std::vector<ECS::EntityId> OverlapAabb(const Math::Vec3& min, const Math::Vec3& max) = 0;
    virtual PhysicsDebugDrawData BuildDebugDrawData() = 0;

    virtual bool SupportsVehicles() const = 0;
    virtual bool SupportsSoftBodies() const = 0;
    virtual void CreateVehicleStub(ECS::EntityId entityId, const VehicleConfig& config) = 0;
    virtual void CreateSoftBodyStub(ECS::EntityId entityId, const SoftBodyConfig& config) = 0;
};

} // namespace Solstice::Physics
