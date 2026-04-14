#include "ObjectSpawner.hxx"
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Content/ConvexHullFactory.hxx>
#include <Core/Debug/Debug.hxx>
#include <Solstice.hxx>
#include <map>
#include <cmath>
#include <algorithm>

#include <Arzachel/MeshFactory.hxx>

namespace Solstice::PhysicsPlayground {

ObjectSpawner::ObjectSpawner(
    Render::Scene& scene,
    Render::MeshLibrary& meshLibrary,
    Core::MaterialLibrary& materialLibrary,
    ECS::Registry& registry
) : m_Scene(scene), m_MeshLibrary(meshLibrary), m_MaterialLibrary(materialLibrary), m_Registry(registry) {
}

uint32_t ObjectSpawner::GetMeshID(ObjectType type) {
    if (m_MeshIDs.find(type) == m_MeshIDs.end()) {
        CreateMeshIfNeeded(type);
    }
    return m_MeshIDs[type];
}

void ObjectSpawner::CreateMeshIfNeeded(ObjectType type) {
    if (m_MeshIDs.find(type) != m_MeshIDs.end()) {
        return; // Already created
    }

    std::unique_ptr<Render::Mesh> mesh;
    float defaultSize = 0.5f;

    switch (type) {
        case ObjectType::Sphere:
            mesh = Solstice::Arzachel::MeshFactory::CreateSphere(defaultSize, 16);
            break;
        case ObjectType::Cylinder:
            mesh = Solstice::Arzachel::MeshFactory::CreateCylinder(defaultSize, 1.0f, 16);
            break;
        case ObjectType::Tetrahedron:
            mesh = Solstice::Arzachel::MeshFactory::CreateTetrahedron(defaultSize);
            break;
        case ObjectType::Cube:
            mesh = Solstice::Arzachel::MeshFactory::CreateCube(1.0f);
            break;
        case ObjectType::Pyramid:
            mesh = Solstice::Arzachel::MeshFactory::CreatePyramid(1.0f, 1.0f);
            break;
        case ObjectType::Torus:
            mesh = Solstice::Arzachel::MeshFactory::CreateTorus(0.5f, 0.25f, 16, 8);
            break;
        case ObjectType::Icosphere:
            mesh = Solstice::Arzachel::MeshFactory::CreateIcosphere(defaultSize, 2);
            break;
    }

    if (mesh) {
        uint32_t meshID = m_MeshLibrary.AddMesh(std::move(mesh));
        m_MeshIDs[type] = meshID;
    }
}

PhysicsObject ObjectSpawner::SpawnObject(ObjectType type, const Math::Vec3& position, const Math::Quaternion& rotation) {
    return SpawnObject(type, position, 0, rotation); // Use default material (0)
}

PhysicsObject ObjectSpawner::SpawnObject(ObjectType type, const Math::Vec3& position, uint32_t materialID, const Math::Quaternion& rotation) {
    // Ensure mesh exists
    uint32_t meshID = GetMeshID(type);

    // For cylinders, always start upright (identity rotation)
    Math::Quaternion finalRotation = (type == ObjectType::Cylinder) ? Math::Quaternion() : rotation;

    // Create render object
    auto renderID = m_Scene.AddObject(meshID, position, finalRotation, Math::Vec3(1, 1, 1), Render::ObjectType_Dynamic);

    // Assign material if provided
    if (materialID != 0) {
        AssignMaterial(renderID, materialID);
    }

    // Create physics entity
    auto entityID = m_Registry.Create();
    auto& rb = m_Registry.Add<Physics::RigidBody>(entityID);
    rb.Position = position;
    rb.Rotation = finalRotation;

    // Setup physics based on type
    float size = 0.5f;
    SetupPhysicsBody(rb, type, size);
    rb.RenderObjectID = renderID;

    PhysicsObject obj;
    obj.RenderID = renderID;
    obj.EntityID = entityID;

    return obj;
}

void ObjectSpawner::AssignMaterial(Render::SceneObjectID objectID, uint32_t materialID) {
    uint32_t meshID = m_Scene.GetMeshID(objectID);
    Render::Mesh* mesh = m_MeshLibrary.GetMesh(meshID);
    if (mesh && !mesh->SubMeshes.empty()) {
        mesh->SubMeshes[0].MaterialID = materialID;
    }
}

void ObjectSpawner::SetupPhysicsBody(Physics::RigidBody& rb, ObjectType type, float size) {
    switch (type) {
        case ObjectType::Sphere: {
            rb.SetMass(1.0f);
            rb.Restitution = 0.6f;
            rb.Type = Physics::ColliderType::Sphere;
            rb.Radius = size;
            break;
        }
        case ObjectType::Cylinder: {
            // Use convex hull for cylinder to get proper flat ends (ReactPhysics3D uses capsule which has rounded ends)
            auto cylinderMesh = Solstice::Arzachel::MeshFactory::CreateCylinder(size, 1.0f, 16);
            auto convexHull = Physics::GenerateConvexHull(*cylinderMesh);
            float mass = 1.5f;
            float radius = size;
            float height = 1.0f;
            rb.SetMass(mass);
            float Ixx = mass / 12.0f * (3.0f * radius * radius + height * height);
            float Iyy = mass * radius * radius / 2.0f;
            float Izz = Ixx;
            rb.InverseInertiaTensor = Math::Vec3(1.0f / Ixx, 1.0f / Iyy, 1.0f / Izz);
            rb.Restitution = 0.3f;
            rb.Friction = 0.8f;  // Increased friction to prevent sliding and help it stay upright
            rb.AngularDrag = 0.6f;  // Increased angular drag to prevent tipping
            rb.LinearDamping = 0.1f;
            rb.Type = Physics::ColliderType::ConvexHull;  // Use ConvexHull instead of Cylinder
            rb.CylinderRadius = radius;  // Keep for reference
            rb.CylinderHeight = height;  // Keep for reference
            rb.Radius = std::sqrt(radius * radius + (height * 0.5f) * (height * 0.5f));
            if (convexHull && !convexHull->Vertices.empty()) {
                rb.HullVertices = convexHull->Vertices;
            }
            rb.Hull = convexHull;
            // Ensure cylinder starts upright with zero angular velocity
            rb.Rotation = Math::Quaternion();  // Identity quaternion (upright)
            rb.AngularVelocity = Math::Vec3(0, 0, 0);  // No initial rotation
            rb.Velocity = Math::Vec3(0, 0, 0);  // No initial velocity (let gravity do its work)
            break;
        }
        case ObjectType::Tetrahedron: {
            auto tetraMesh = Solstice::Arzachel::MeshFactory::CreateTetrahedron(size);
            auto convexHull = Physics::GenerateConvexHull(*tetraMesh);
            rb.SetMass(0.8f);
            float radius = std::sqrt(size * size + size * size + size * size);
            float I = 0.8f * radius * radius / 5.0f;
            float invI = 1.0f / I;
            rb.InverseInertiaTensor = Math::Vec3(invI, invI, invI);
            rb.Restitution = 0.1f;
            rb.Friction = 0.8f;
            rb.Type = Physics::ColliderType::Tetrahedron;
            if (convexHull && !convexHull->Vertices.empty()) {
                rb.HullVertices = convexHull->Vertices;
            }
            rb.Hull = convexHull;
            rb.Radius = radius;
            rb.EnableCCD = true;
            rb.CCDMotionThreshold = 1.0f;
            rb.AngularDrag = 0.25f;
            rb.LinearDamping = 0.1f;
            break;
        }
        case ObjectType::Cube: {
            rb.SetBoxInertia(1.0f, Math::Vec3(0.5f, 0.5f, 0.5f));
            rb.Restitution = 0.4f;
            rb.Type = Physics::ColliderType::Box;
            rb.HalfExtents = Math::Vec3(0.5f, 0.5f, 0.5f);
            break;
        }
        case ObjectType::Pyramid: {
            // Use convex hull for pyramid
            auto pyramidMesh = Solstice::Arzachel::MeshFactory::CreatePyramid(1.0f, 1.0f);
            auto convexHull = Physics::GenerateConvexHull(*pyramidMesh);
            rb.SetMass(1.0f);
            float radius = std::sqrt(0.5f * 0.5f + 0.5f * 0.5f + 1.0f * 1.0f);
            float I = 1.0f * radius * radius / 5.0f;
            float invI = 1.0f / I;
            rb.InverseInertiaTensor = Math::Vec3(invI, invI, invI);
            rb.Restitution = 0.3f;
            rb.Friction = 0.6f;
            rb.Type = Physics::ColliderType::ConvexHull;
            if (convexHull && !convexHull->Vertices.empty()) {
                rb.HullVertices = convexHull->Vertices;
            }
            rb.Hull = convexHull;
            rb.Radius = radius;
            break;
        }
        case ObjectType::Torus: {
            // Use convex hull for torus (complex shape)
            auto torusMesh = Solstice::Arzachel::MeshFactory::CreateTorus(0.5f, 0.25f, 16, 8);
            auto convexHull = Physics::GenerateConvexHull(*torusMesh);
            rb.SetMass(1.2f);
            float radius = 0.5f + 0.25f; // major + minor
            float I = 1.2f * radius * radius / 3.0f;
            float invI = 1.0f / I;
            rb.InverseInertiaTensor = Math::Vec3(invI, invI, invI);
            rb.Restitution = 0.4f;
            rb.Friction = 0.5f;
            rb.Type = Physics::ColliderType::ConvexHull;
            if (convexHull && !convexHull->Vertices.empty()) {
                rb.HullVertices = convexHull->Vertices;
            }
            rb.Hull = convexHull;
            rb.Radius = radius;
            break;
        }
        case ObjectType::Icosphere: {
            // Use sphere collider (close approximation)
            rb.SetMass(1.0f);
            rb.Restitution = 0.6f;
            rb.Type = Physics::ColliderType::Sphere;
            rb.Radius = size;
            break;
        }
    }
}

} // namespace Solstice::PhysicsPlayground
