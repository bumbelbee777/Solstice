#pragma once

#include <Solstice.hxx>
#include <Entity/Registry.hxx>
#include <Physics/Collision/Narrowphase/ConvexHull.hxx>  // Must be included before RigidBody.hxx for full ConvexHull definition
#include <Physics/Dynamics/RigidBody.hxx>
#include <reactphysics3d/engine/PhysicsCommon.h>
#include <reactphysics3d/engine/PhysicsWorld.h>
#include <reactphysics3d/body/RigidBody.h>
#include <reactphysics3d/collision/shapes/SphereShape.h>
#include <reactphysics3d/collision/shapes/BoxShape.h>
#include <reactphysics3d/collision/shapes/CapsuleShape.h>
#include <reactphysics3d/collision/shapes/ConvexMeshShape.h>
#include <reactphysics3d/collision/Collider.h>
#include <unordered_map>
#include <memory>
namespace Solstice::Physics {

namespace detail {
struct Rp3dSyncCache {
    Math::Vec3 Pos{};
    Math::Vec3 LinVel{};
    Math::Vec3 AngVel{};
    Math::Quaternion Rot{};
    float Mass{1.f};
    float Friction{0.5f};
    float Restitution{0.5f};
    float LinDamp{};
    float AngDamp{};
    float GravScale{1.f};
    bool IsStatic{false};
    bool GravityOn{true};
};
} // namespace detail

/**
 * Bridge class that manages ReactPhysics3D physics world and synchronizes
 * it with Solstice's RigidBody components.
 */
class SOLSTICE_API ReactPhysics3DBridge {
public:
    ReactPhysics3DBridge();
    ~ReactPhysics3DBridge();

    // Non-copyable
    ReactPhysics3DBridge(const ReactPhysics3DBridge&) = delete;
    ReactPhysics3DBridge& operator=(const ReactPhysics3DBridge&) = delete;

    /**
     * Initialize the bridge with a registry
     */
    void Initialize(ECS::Registry& registry);

    /**
     * Shutdown and cleanup
     */
    void Shutdown();

    /**
     * Sync RigidBody components to ReactPhysics3D bodies (before physics step)
     */
    void SyncToReactPhysics3D();

    /**
     * Sync ReactPhysics3D bodies back to RigidBody components (after physics step)
     */
    void SyncFromReactPhysics3D();

    /**
     * Update the physics world (call ReactPhysics3D's update)
     */
    void Update(float dt);

    /**
     * Create a ReactPhysics3D body for a RigidBody component
     */
    void CreateBody(ECS::EntityId entityId, RigidBody& rigidBody);

    /**
     * Remove a ReactPhysics3D body for a RigidBody component
     */
    void RemoveBody(ECS::EntityId entityId);

    /**
     * Get the ReactPhysics3D physics world
     */
    reactphysics3d::PhysicsWorld* GetPhysicsWorld() const { return m_PhysicsWorld; }

    /**
     * Directly set position and rotation for a body (bypasses physics integration)
     * Used for grabbed objects to ensure smooth, frame-rate independent movement
     */
    void SetBodyTransform(ECS::EntityId entityId, const Math::Vec3& position, const Math::Quaternion& rotation);

private:
    /**
     * Convert Solstice Vector3 to ReactPhysics3D Vector3
     */
    static reactphysics3d::Vector3 ToRP3D(const Math::Vec3& v);

    /**
     * Convert ReactPhysics3D Vector3 to Solstice Vector3
     */
    static Math::Vec3 FromRP3D(const reactphysics3d::Vector3& v);

    /**
     * Convert Solstice Quaternion to ReactPhysics3D Quaternion
     */
    static reactphysics3d::Quaternion ToRP3D(const Math::Quaternion& q);

    /**
     * Convert ReactPhysics3D Quaternion to Solstice Quaternion
     */
    static Math::Quaternion FromRP3D(const reactphysics3d::Quaternion& q);

    /**
     * Create a ReactPhysics3D collision shape from a RigidBody
     */
    reactphysics3d::CollisionShape* CreateCollisionShape(const RigidBody& rigidBody);

    /**
     * Update a ReactPhysics3D body's properties from a RigidBody
     */
    void UpdateBodyProperties(ECS::EntityId entityId, reactphysics3d::RigidBody* rp3dBody, const RigidBody& solsticeBody);

    /**
     * Update a RigidBody's properties from a ReactPhysics3D body
     */
    void UpdateRigidBodyProperties(RigidBody& solsticeBody, reactphysics3d::RigidBody* rp3dBody);

    ECS::Registry* m_Registry{nullptr};
    std::unique_ptr<reactphysics3d::PhysicsCommon> m_PhysicsCommon;
    reactphysics3d::PhysicsWorld* m_PhysicsWorld{nullptr};

    // Map Solstice EntityId to ReactPhysics3D RigidBody
    std::unordered_map<ECS::EntityId, reactphysics3d::RigidBody*> m_EntityToBody;

    // Map ReactPhysics3D RigidBody to Solstice EntityId (for reverse lookup)
    std::unordered_map<reactphysics3d::RigidBody*, ECS::EntityId> m_BodyToEntity;

    // Track which shapes we created (for cleanup)
    std::unordered_map<reactphysics3d::RigidBody*, reactphysics3d::CollisionShape*> m_BodyToShape;

    // Track polyhedron meshes for convex shapes (must destroy after shape)
    std::unordered_map<reactphysics3d::ConvexMeshShape*, reactphysics3d::PolyhedronMesh*> m_ConvexShapeToPolyhedronMesh;

    std::unordered_map<ECS::EntityId, detail::Rp3dSyncCache> m_SyncToCache;
};

} // namespace Solstice::Physics
