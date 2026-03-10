#include "SelectionSystem.hxx"
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Math/Matrix.hxx>
#include <algorithm>
#include <limits>

namespace Solstice::PhysicsPlayground {

SelectionSystem::SelectionSystem(Render::SoftwareRenderer& renderer)
    : m_Renderer(renderer) {
}

Math::Vec3 SelectionSystem::RaycastFromMouse(float mouseX, float mouseY, const Render::Camera& camera, int screenWidth, int screenHeight) {
    // Simplified: Use camera forward direction for now
    // TODO: Implement proper screen-to-world ray conversion
    // For now, this will pick objects in the center of the screen
    return camera.Front;
}

bool SelectionSystem::RayAABBIntersection(const Math::Vec3& rayOrigin, const Math::Vec3& rayDir, const Math::Vec3& aabbMin, const Math::Vec3& aabbMax, float& t) {
    float tmin = 0.0f;
    float tmax = std::numeric_limits<float>::max();

    // Check X axis
    float invDx = 1.0f / rayDir.x;
    float t0x = (aabbMin.x - rayOrigin.x) * invDx;
    float t1x = (aabbMax.x - rayOrigin.x) * invDx;
    if (invDx < 0.0f) std::swap(t0x, t1x);
    tmin = t0x > tmin ? t0x : tmin;
    tmax = t1x < tmax ? t1x : tmax;
    if (tmax < tmin) return false;

    // Check Y axis
    float invDy = 1.0f / rayDir.y;
    float t0y = (aabbMin.y - rayOrigin.y) * invDy;
    float t1y = (aabbMax.y - rayOrigin.y) * invDy;
    if (invDy < 0.0f) std::swap(t0y, t1y);
    tmin = t0y > tmin ? t0y : tmin;
    tmax = t1y < tmax ? t1y : tmax;
    if (tmax < tmin) return false;

    // Check Z axis
    float invDz = 1.0f / rayDir.z;
    float t0z = (aabbMin.z - rayOrigin.z) * invDz;
    float t1z = (aabbMax.z - rayOrigin.z) * invDz;
    if (invDz < 0.0f) std::swap(t0z, t1z);
    tmin = t0z > tmin ? t0z : tmin;
    tmax = t1z < tmax ? t1z : tmax;
    if (tmax < tmin) return false;

    t = tmin;
    return tmin > 0.0f;
}

Render::SceneObjectID SelectionSystem::PickObject(float mouseX, float mouseY, const Render::Camera& camera, const Render::Scene& scene, int screenWidth, int screenHeight) {
    Math::Vec3 rayOrigin = camera.Position;
    Math::Vec3 rayDir = camera.Front; // Use camera forward direction for now (simplified)
    rayDir = rayDir.Normalized();

    Render::SceneObjectID closestObject = Render::InvalidObjectID;
    float closestT = std::numeric_limits<float>::max();

    // Test against all objects in scene
    size_t objectCount = scene.GetObjectCount();
    const auto& aabbs = scene.GetBoundingBoxes();
    const auto& transforms = scene.GetTransforms();

    for (size_t i = 0; i < objectCount; ++i) {
        Render::SceneObjectID objID = static_cast<Render::SceneObjectID>(i);

        // Get object position and AABB
        Math::Vec3 objPos(transforms.PosX[i], transforms.PosY[i], transforms.PosZ[i]);
        Math::Vec3 aabbMin = aabbs.GetMin(i);
        Math::Vec3 aabbMax = aabbs.GetMax(i);

        // Transform AABB to world space (simplified - assumes no rotation/scale)
        aabbMin = aabbMin + objPos;
        aabbMax = aabbMax + objPos;

        float t;
        if (RayAABBIntersection(rayOrigin, rayDir, aabbMin, aabbMax, t)) {
            if (t < closestT) {
                closestT = t;
                closestObject = objID;
            }
        }
    }

    return closestObject;
}

void SelectionSystem::SelectObject(Render::SceneObjectID objectID) {
    if (objectID != Render::InvalidObjectID) {
        m_SelectedObjects.insert(objectID);
        UpdateRenderer();
    }
}

void SelectionSystem::DeselectObject(Render::SceneObjectID objectID) {
    m_SelectedObjects.erase(objectID);
    UpdateRenderer();
}

void SelectionSystem::ClearSelection() {
    m_SelectedObjects.clear();
    UpdateRenderer();
}

void SelectionSystem::ToggleSelection(Render::SceneObjectID objectID) {
    if (IsSelected(objectID)) {
        DeselectObject(objectID);
    } else {
        SelectObject(objectID);
    }
}

bool SelectionSystem::IsSelected(Render::SceneObjectID objectID) const {
    return m_SelectedObjects.find(objectID) != m_SelectedObjects.end();
}

void SelectionSystem::SetHoveredObject(Render::SceneObjectID objectID) {
    m_HoveredObject = objectID;
    UpdateRenderer();
}

void SelectionSystem::UpdateRenderer() {
    m_Renderer.SetSelectedObjects(m_SelectedObjects);
    m_Renderer.SetHoveredObject(m_HoveredObject);
}

} // namespace Solstice::PhysicsPlayground
