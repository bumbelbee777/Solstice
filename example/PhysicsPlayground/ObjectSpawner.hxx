#pragma once

#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Entity/Registry.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <memory>
#include <map>
#include <cstdint>

namespace Solstice::Render {
    class MeshLibrary;
    class Camera;
}

namespace Solstice::Core {
    class MaterialLibrary;
}

namespace Solstice::Physics {
    class RigidBody;
}

namespace Solstice::ECS {
    class Registry;
}

namespace Solstice::PhysicsPlayground {

enum class ObjectType {
    Sphere,
    Cylinder,
    Tetrahedron,
    Cube,
    Pyramid,
    Torus,
    Icosphere
};

struct PhysicsObject {
    Render::SceneObjectID RenderID;
    ECS::EntityId EntityID;
};

class ObjectSpawner {
public:
    ObjectSpawner(
        Render::Scene& scene,
        Render::MeshLibrary& meshLibrary,
        Core::MaterialLibrary& materialLibrary,
        ECS::Registry& registry
    );

    PhysicsObject SpawnObject(ObjectType type, const Math::Vec3& position, const Math::Quaternion& rotation = Math::Quaternion());
    PhysicsObject SpawnObject(ObjectType type, const Math::Vec3& position, uint32_t materialID, const Math::Quaternion& rotation = Math::Quaternion());

    // Get mesh IDs for object types (creates if needed)
    uint32_t GetMeshID(ObjectType type);

    // Assign material to an object
    void AssignMaterial(Render::SceneObjectID objectID, uint32_t materialID);

private:
    void CreateMeshIfNeeded(ObjectType type);
    void SetupPhysicsBody(Physics::RigidBody& rb, ObjectType type, float size);

    Render::Scene& m_Scene;
    Render::MeshLibrary& m_MeshLibrary;
    Core::MaterialLibrary& m_MaterialLibrary;
    ECS::Registry& m_Registry;

    std::map<ObjectType, uint32_t> m_MeshIDs;
    uint32_t m_NextObjectID{1};
};

} // namespace Solstice::PhysicsPlayground
