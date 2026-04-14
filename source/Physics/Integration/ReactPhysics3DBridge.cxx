#include <Physics/Integration/ReactPhysics3DBridge.hxx>
#include <Core/Debug/Debug.hxx>
#include <Entity/EntityId.hxx>
#include <Physics/Collision/Narrowphase/ConvexHull.hxx>  // Must be included before using ConvexHull
#include <Physics/Collision/Narrowphase/ShapeHelpers.hxx>  // For TransformPoint helper
#include <reactphysics3d/configuration.h>
#include <reactphysics3d/mathematics/Transform.h>
#include <reactphysics3d/mathematics/Vector3.h>
#include <reactphysics3d/mathematics/Quaternion.h>
#include <reactphysics3d/collision/shapes/SphereShape.h>
#include <reactphysics3d/collision/shapes/BoxShape.h>
#include <reactphysics3d/collision/shapes/CapsuleShape.h>
#include <reactphysics3d/collision/shapes/ConvexMeshShape.h>
#include <reactphysics3d/collision/PolygonVertexArray.h>
#include <reactphysics3d/collision/PolyhedronMesh.h>

namespace Solstice::Physics {

ReactPhysics3DBridge::ReactPhysics3DBridge() {
    m_PhysicsCommon = std::make_unique<reactphysics3d::PhysicsCommon>();

    // Configure world settings
    reactphysics3d::PhysicsWorld::WorldSettings settings;
    settings.gravity = reactphysics3d::Vector3(0, -9.81, 0);
    settings.defaultFrictionCoefficient = 0.5f;
    settings.defaultBounciness = 0.5f;
    settings.isSleepingEnabled = true;
    // Increase position iterations for better stability, especially for tetrahedrons
    settings.defaultPositionSolverNbIterations = 8;
    // Increase velocity iterations for better collision response
    settings.defaultVelocitySolverNbIterations = 10;

    m_PhysicsWorld = m_PhysicsCommon->createPhysicsWorld(settings);
}

ReactPhysics3DBridge::~ReactPhysics3DBridge() {
    Shutdown();
}

void ReactPhysics3DBridge::Initialize(ECS::Registry& registry) {
    m_Registry = &registry;
}

void ReactPhysics3DBridge::Shutdown() {
    // Clean up all bodies (always, even if m_Registry is null — maps must be drained before destroyPhysicsWorld)
    std::vector<ECS::EntityId> entitiesToRemove;
    entitiesToRemove.reserve(m_EntityToBody.size());
    for (const auto& [entityId, _] : m_EntityToBody) {
        entitiesToRemove.push_back(entityId);
    }
    for (ECS::EntityId entityId : entitiesToRemove) {
        RemoveBody(entityId);
    }

    // Destroy physics world
    if (m_PhysicsWorld && m_PhysicsCommon) {
        m_PhysicsCommon->destroyPhysicsWorld(m_PhysicsWorld);
        m_PhysicsWorld = nullptr;
    }

    m_EntityToBody.clear();
    m_BodyToEntity.clear();
    m_BodyToShape.clear();
    m_Registry = nullptr;
}

void ReactPhysics3DBridge::SyncToReactPhysics3D() {
    if (!m_Registry || !m_PhysicsWorld) return;

    // Destroyed entities / removed RigidBody components do not appear in ForEach<RigidBody>, but we may
    // still hold a ReactPhysics3D body in m_EntityToBody. Remove those orphans before the sync pass.
    std::vector<ECS::EntityId> stale;
    stale.reserve(m_EntityToBody.size());
    for (const auto& kv : m_EntityToBody) {
        const ECS::EntityId entityId = kv.first;
        if (!m_Registry->Valid(entityId) || !m_Registry->Has<RigidBody>(entityId)) {
            stale.push_back(entityId);
        }
    }
    for (ECS::EntityId entityId : stale) {
        RemoveBody(entityId);
    }

    // Single pass: new RigidBody components get bodies; existing mappings get property updates
    m_Registry->ForEach<RigidBody>([this](ECS::EntityId entityId, RigidBody& rigidBody) {
        auto it = m_EntityToBody.find(entityId);
        if (it == m_EntityToBody.end()) {
            CreateBody(entityId, rigidBody);
        } else {
            UpdateBodyProperties(it->second, rigidBody);
        }
    });
}

void ReactPhysics3DBridge::SyncFromReactPhysics3D() {
    if (!m_Registry || !m_PhysicsWorld) return;

    for (const auto& [entityId, rp3dBody] : m_EntityToBody) {
        if (!m_Registry->Valid(entityId)) {
            continue;
        }

        if (RigidBody* solsticeBody = m_Registry->TryGet<RigidBody>(entityId)) {
            UpdateRigidBodyProperties(*solsticeBody, rp3dBody);
        }
    }
}

void ReactPhysics3DBridge::Update(float dt) {
    if (!m_PhysicsWorld) return;
    m_PhysicsWorld->update(static_cast<reactphysics3d::decimal>(dt));
}

void ReactPhysics3DBridge::CreateBody(ECS::EntityId entityId, RigidBody& rigidBody) {
    if (!m_PhysicsWorld || !m_PhysicsCommon) return;

    // Convert transform
    reactphysics3d::Transform transform(
        ToRP3D(rigidBody.Position),
        ToRP3D(rigidBody.Rotation)
    );

    // Create the rigid body
    reactphysics3d::RigidBody* rp3dBody = m_PhysicsWorld->createRigidBody(transform);
    if (!rp3dBody) return;

    // Set body type
    if (rigidBody.IsStatic) {
        rp3dBody->setType(reactphysics3d::BodyType::STATIC);
    } else {
        rp3dBody->setType(reactphysics3d::BodyType::DYNAMIC);
    }

    // Create and add collision shape
    reactphysics3d::CollisionShape* shape = CreateCollisionShape(rigidBody);
    if (shape) {
        reactphysics3d::Transform shapeTransform = reactphysics3d::Transform::identity();
        reactphysics3d::Collider* collider = rp3dBody->addCollider(shape, shapeTransform);

        if (collider) {
            // Set material properties
            reactphysics3d::Material& material = collider->getMaterial();
            material.setFrictionCoefficient(rigidBody.Friction);
            material.setBounciness(rigidBody.Restitution);
        }

        m_BodyToShape[rp3dBody] = shape;
    }

    // Set mass
    if (!rigidBody.IsStatic && rigidBody.Mass > 0.0f) {
        rp3dBody->setMass(static_cast<reactphysics3d::decimal>(rigidBody.Mass));
    }

    // Set velocities
    rp3dBody->setLinearVelocity(ToRP3D(rigidBody.Velocity));
    rp3dBody->setAngularVelocity(ToRP3D(rigidBody.AngularVelocity));

    // Set damping
    rp3dBody->setLinearDamping(static_cast<reactphysics3d::decimal>(rigidBody.LinearDamping));
    rp3dBody->setAngularDamping(static_cast<reactphysics3d::decimal>(rigidBody.AngularDrag));

    // Enable/disable gravity
    rp3dBody->enableGravity(rigidBody.GravityScale > 0.0f);

    // Store mappings
    m_EntityToBody[entityId] = rp3dBody;
    m_BodyToEntity[rp3dBody] = entityId;
}

void ReactPhysics3DBridge::RemoveBody(ECS::EntityId entityId) {
    auto it = m_EntityToBody.find(entityId);
    if (it == m_EntityToBody.end()) return;

    reactphysics3d::RigidBody* rp3dBody = it->second;

    // Shapes must outlive colliders: destroyRigidBody() calls removeAllColliders(), which still
    // touches the CollisionShape. Match ReactPhysics3D testbed order (body first, then shape).
    reactphysics3d::CollisionShape* shape = nullptr;
    reactphysics3d::PolyhedronMesh* convexPolyMesh = nullptr;
    auto shapeIt = m_BodyToShape.find(rp3dBody);
    if (shapeIt != m_BodyToShape.end()) {
        shape = shapeIt->second;
        if (auto* convex = dynamic_cast<reactphysics3d::ConvexMeshShape*>(shape)) {
            auto meshIt = m_ConvexShapeToPolyhedronMesh.find(convex);
            if (meshIt != m_ConvexShapeToPolyhedronMesh.end()) {
                convexPolyMesh = meshIt->second;
                m_ConvexShapeToPolyhedronMesh.erase(meshIt);
            }
        }
        m_BodyToShape.erase(shapeIt);
    }

    if (m_PhysicsWorld) {
        m_PhysicsWorld->destroyRigidBody(rp3dBody);
    }

    if (shape) {
        if (auto* sphere = dynamic_cast<reactphysics3d::SphereShape*>(shape)) {
            m_PhysicsCommon->destroySphereShape(sphere);
        } else if (auto* box = dynamic_cast<reactphysics3d::BoxShape*>(shape)) {
            m_PhysicsCommon->destroyBoxShape(box);
        } else if (auto* capsule = dynamic_cast<reactphysics3d::CapsuleShape*>(shape)) {
            m_PhysicsCommon->destroyCapsuleShape(capsule);
        } else if (auto* convex = dynamic_cast<reactphysics3d::ConvexMeshShape*>(shape)) {
            m_PhysicsCommon->destroyConvexMeshShape(convex);
            if (convexPolyMesh) {
                m_PhysicsCommon->destroyPolyhedronMesh(convexPolyMesh);
            }
        }
    }

    m_EntityToBody.erase(it);
    m_BodyToEntity.erase(rp3dBody);
}

reactphysics3d::Vector3 ReactPhysics3DBridge::ToRP3D(const Math::Vec3& v) {
    return reactphysics3d::Vector3(
        static_cast<reactphysics3d::decimal>(v.x),
        static_cast<reactphysics3d::decimal>(v.y),
        static_cast<reactphysics3d::decimal>(v.z)
    );
}

Math::Vec3 ReactPhysics3DBridge::FromRP3D(const reactphysics3d::Vector3& v) {
    return Math::Vec3(
        static_cast<float>(v.x),
        static_cast<float>(v.y),
        static_cast<float>(v.z)
    );
}

reactphysics3d::Quaternion ReactPhysics3DBridge::ToRP3D(const Math::Quaternion& q) {
    // Note: Solstice uses (w, x, y, z), ReactPhysics3D uses (x, y, z, w)
    return reactphysics3d::Quaternion(
        static_cast<reactphysics3d::decimal>(q.x),
        static_cast<reactphysics3d::decimal>(q.y),
        static_cast<reactphysics3d::decimal>(q.z),
        static_cast<reactphysics3d::decimal>(q.w)
    );
}

Math::Quaternion ReactPhysics3DBridge::FromRP3D(const reactphysics3d::Quaternion& q) {
    // Note: ReactPhysics3D uses (x, y, z, w), Solstice uses (w, x, y, z)
    return Math::Quaternion(
        static_cast<float>(q.w),
        static_cast<float>(q.x),
        static_cast<float>(q.y),
        static_cast<float>(q.z)
    );
}

reactphysics3d::CollisionShape* ReactPhysics3DBridge::CreateCollisionShape(const RigidBody& rigidBody) {
    if (!m_PhysicsCommon) return nullptr;

    switch (rigidBody.Type) {
        case ColliderType::Sphere: {
            return m_PhysicsCommon->createSphereShape(
                static_cast<reactphysics3d::decimal>(rigidBody.Radius)
            );
        }

        case ColliderType::Box: {
            reactphysics3d::Vector3 halfExtents = ToRP3D(rigidBody.HalfExtents);
            return m_PhysicsCommon->createBoxShape(halfExtents);
        }

        case ColliderType::Capsule: {
            return m_PhysicsCommon->createCapsuleShape(
                static_cast<reactphysics3d::decimal>(rigidBody.CapsuleRadius),
                static_cast<reactphysics3d::decimal>(rigidBody.CapsuleHeight)
            );
        }

        case ColliderType::Cylinder: {
            // ReactPhysics3D doesn't have a cylinder shape, use capsule as closest match
            return m_PhysicsCommon->createCapsuleShape(
                static_cast<reactphysics3d::decimal>(rigidBody.CylinderRadius),
                static_cast<reactphysics3d::decimal>(rigidBody.CylinderHeight)
            );
        }

        case ColliderType::ConvexHull:
        case ColliderType::Tetrahedron: {
            // ReactPhysics3D v0.9 uses PolygonVertexArray + PolyhedronMesh (no VertexArray/createConvexMesh)
            const ConvexHull* hull = nullptr;
            if (rigidBody.Hull && rigidBody.Hull.get() && !rigidBody.Hull->Vertices.empty() && !rigidBody.Hull->Faces.empty()) {
                hull = rigidBody.Hull.get();
            }

            if (hull) {
                const std::vector<Math::Vec3>& vertices = hull->Vertices;
                const std::vector<HullFace>& faces = hull->Faces;

                // Build flat index array and polygon face descriptors for PolygonVertexArray
                std::vector<reactphysics3d::uint32> indices;
                std::vector<reactphysics3d::PolygonVertexArray::PolygonFace> rp3dFaces;
                indices.reserve(vertices.size());  // approximate
                rp3dFaces.reserve(faces.size());

                reactphysics3d::uint32 indexBase = 0;
                for (const HullFace& face : faces) {
                    for (reactphysics3d::uint32 idx : face.VertexIndices) {
                        indices.push_back(idx);
                    }
                    rp3dFaces.push_back({ static_cast<reactphysics3d::uint32>(face.VertexIndices.size()), indexBase });
                    indexBase += static_cast<reactphysics3d::uint32>(face.VertexIndices.size());
                }

                if (!indices.empty() && !rp3dFaces.empty()) {
                    reactphysics3d::PolygonVertexArray polygonVertexArray(
                        static_cast<reactphysics3d::uint32>(vertices.size()),
                        vertices.data(),
                        static_cast<reactphysics3d::uint32>(sizeof(Math::Vec3)),
                        indices.data(),
                        sizeof(reactphysics3d::uint32),
                        static_cast<reactphysics3d::uint32>(rp3dFaces.size()),
                        rp3dFaces.data(),
                        reactphysics3d::PolygonVertexArray::VertexDataType::VERTEX_FLOAT_TYPE,
                        reactphysics3d::PolygonVertexArray::IndexDataType::INDEX_INTEGER_TYPE
                    );

                    reactphysics3d::PolyhedronMesh* polyhedronMesh = m_PhysicsCommon->createPolyhedronMesh(&polygonVertexArray);
                    if (polyhedronMesh) {
                        reactphysics3d::ConvexMeshShape* convexShape = m_PhysicsCommon->createConvexMeshShape(polyhedronMesh);
                        if (convexShape) {
                            m_ConvexShapeToPolyhedronMesh[convexShape] = polyhedronMesh;
                            return convexShape;
                        }
                        m_PhysicsCommon->destroyPolyhedronMesh(polyhedronMesh);
                    }
                }
            }
            break;
        }

        case ColliderType::Triangle:
            // ReactPhysics3D doesn't have a single triangle shape, would need triangle mesh
            // For now, skip
            break;
    }

    return nullptr;
}

void ReactPhysics3DBridge::UpdateBodyProperties(reactphysics3d::RigidBody* rp3dBody, const RigidBody& solsticeBody) {
    if (!rp3dBody) return;

    // Update transform
    reactphysics3d::Transform transform(
        ToRP3D(solsticeBody.Position),
        ToRP3D(solsticeBody.Rotation)
    );
    rp3dBody->setTransform(transform);

    // Update velocities - CRITICAL: Always set velocity, even if zero
    // This ensures ReactPhysics3D respects our velocity changes
    rp3dBody->setLinearVelocity(ToRP3D(solsticeBody.Velocity));
    rp3dBody->setAngularVelocity(ToRP3D(solsticeBody.AngularVelocity));

    // For dynamic bodies, ensure velocity is actually applied
    // ReactPhysics3D might ignore velocity changes if body is sleeping or if damping is too high
    if (!solsticeBody.IsStatic && solsticeBody.Mass > 0.0f) {
        // Wake up the body if it's sleeping (so velocity changes take effect)
        rp3dBody->setIsSleeping(false);
    }

    // Update mass
    if (!solsticeBody.IsStatic && solsticeBody.Mass > 0.0f) {
        rp3dBody->setMass(static_cast<reactphysics3d::decimal>(solsticeBody.Mass));
    }

    // Update body type
    if (solsticeBody.IsStatic) {
        rp3dBody->setType(reactphysics3d::BodyType::STATIC);
    } else {
        rp3dBody->setType(reactphysics3d::BodyType::DYNAMIC);
        // For dynamic bodies, ensure they're awake and can move
        rp3dBody->setIsSleeping(false);
    }

    // Update damping
    rp3dBody->setLinearDamping(static_cast<reactphysics3d::decimal>(solsticeBody.LinearDamping));
    rp3dBody->setAngularDamping(static_cast<reactphysics3d::decimal>(solsticeBody.AngularDrag));

    // Update gravity
    rp3dBody->enableGravity(solsticeBody.GravityScale > 0.0f);

    // Update gravity
    rp3dBody->enableGravity(solsticeBody.GravityScale > 0.0f);

    // Update material properties (if collider exists)
    if (rp3dBody->getNbColliders() > 0) {
        reactphysics3d::Collider* collider = rp3dBody->getCollider(0);
        if (collider) {
            reactphysics3d::Material& material = collider->getMaterial();
            material.setFrictionCoefficient(solsticeBody.Friction);
            material.setBounciness(solsticeBody.Restitution);
        }
    }
}

void ReactPhysics3DBridge::UpdateRigidBodyProperties(RigidBody& solsticeBody, reactphysics3d::RigidBody* rp3dBody) {
    if (!rp3dBody) return;

    // Update transform
    reactphysics3d::Transform transform = rp3dBody->getTransform();
    solsticeBody.Position = FromRP3D(transform.getPosition());
    solsticeBody.Rotation = FromRP3D(transform.getOrientation());

    // Update velocities
    solsticeBody.Velocity = FromRP3D(rp3dBody->getLinearVelocity());
    solsticeBody.AngularVelocity = FromRP3D(rp3dBody->getAngularVelocity());

    // Update mass
    solsticeBody.Mass = static_cast<float>(rp3dBody->getMass());
    if (solsticeBody.Mass > 0.0f) {
        solsticeBody.InverseMass = 1.0f / solsticeBody.Mass;
    } else {
        solsticeBody.InverseMass = 0.0f;
        solsticeBody.IsStatic = true;
    }

    // Update sleep state
    solsticeBody.IsAsleep = (rp3dBody->getType() == reactphysics3d::BodyType::STATIC) ||
                             rp3dBody->isSleeping();

    // Special stabilization for tetrahedrons to prevent edge balancing
    // Early exit conditions to avoid expensive checks when not needed
    if (solsticeBody.Type == ColliderType::Tetrahedron && !solsticeBody.IsStatic && !solsticeBody.IsAsleep) {
        float velMag = solsticeBody.Velocity.Magnitude();
        float angVelMag = solsticeBody.AngularVelocity.Magnitude();

        // Early exit: skip if moving too fast or too high/low
        if (velMag >= 0.1f || angVelMag >= 0.2f || solsticeBody.Position.y >= 0.5f || solsticeBody.Position.y <= -0.1f) {
            return;
        }

        // Check if tetrahedron is on an edge (unstable)
        const std::vector<Math::Vec3>* verts = nullptr;
        if (!solsticeBody.HullVertices.empty()) {
            verts = &solsticeBody.HullVertices;
        } else if (solsticeBody.Hull && solsticeBody.Hull.get() && !solsticeBody.Hull->Vertices.empty()) {
            verts = &solsticeBody.Hull->Vertices;
        }

        if (verts && !verts->empty()) {
            int belowGroundCount = 0;
            const float groundThreshold = 0.1f;
            // Early exit optimization: stop counting once we know it's stable (3+ vertices)
            for (const auto& v : *verts) {
                // Transform vertex to world space using the helper function
                Math::Vec3 worldPos = TransformPoint(v, solsticeBody.Position, solsticeBody.Rotation);
                if (worldPos.y <= groundThreshold) {
                    belowGroundCount++;
                    // Early exit: if 3+ vertices are below ground, it's stable (on a face)
                    if (belowGroundCount >= 3) {
                        return;
                    }
                }
            }

            // If on edge (2 vertices) or vertex (1 vertex), apply stabilization
            if (belowGroundCount <= 2) {
                // Apply small downward force and angular correction to tip onto face
                reactphysics3d::Vector3 downForce(0, -0.5f, 0);
                rp3dBody->applyWorldForceAtCenterOfMass(downForce);

                // Apply small torque to rotate onto a face
                reactphysics3d::Vector3 torque(
                    (solsticeBody.Position.x > 0 ? -0.1f : 0.1f),
                    0.0f,
                    (solsticeBody.Position.z > 0 ? -0.1f : 0.1f)
                );
                rp3dBody->applyWorldTorque(torque);
            }
        }
    }
}

void ReactPhysics3DBridge::SetBodyTransform(ECS::EntityId entityId, const Math::Vec3& position, const Math::Quaternion& rotation) {
    if (!m_Registry || !m_PhysicsWorld) return;

    auto it = m_EntityToBody.find(entityId);
    if (it == m_EntityToBody.end()) return;

    reactphysics3d::RigidBody* rp3dBody = it->second;
    if (!rp3dBody) return;

    // Directly set transform in ReactPhysics3D (bypasses physics integration)
    reactphysics3d::Transform transform(
        ToRP3D(position),
        ToRP3D(rotation)
    );
    rp3dBody->setTransform(transform);

    // Also zero out velocities to prevent physics from interfering
    rp3dBody->setLinearVelocity(reactphysics3d::Vector3(0, 0, 0));
    rp3dBody->setAngularVelocity(reactphysics3d::Vector3(0, 0, 0));

    // Wake up the body to ensure changes take effect
    rp3dBody->setIsSleeping(false);

    // Update the RigidBody component to match
    if (RigidBody* solsticeBody = m_Registry->TryGet<RigidBody>(entityId)) {
        solsticeBody->Position = position;
        solsticeBody->Rotation = rotation;
        solsticeBody->Velocity = Math::Vec3(0, 0, 0);
        solsticeBody->AngularVelocity = Math::Vec3(0, 0, 0);
    }
}

} // namespace Solstice::Physics
