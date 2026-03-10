#pragma once

#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Math/Vector.hxx>
#include <set>
#include <limits>

namespace Solstice::Render {
    class SoftwareRenderer;
}

namespace Solstice::PhysicsPlayground {

class SelectionSystem {
public:
    SelectionSystem(Render::SoftwareRenderer& renderer);

    // Mouse picking
    Render::SceneObjectID PickObject(float mouseX, float mouseY, const Render::Camera& camera, const Render::Scene& scene, int screenWidth, int screenHeight);

    // Selection management
    void SelectObject(Render::SceneObjectID objectID);
    void DeselectObject(Render::SceneObjectID objectID);
    void ClearSelection();
    void ToggleSelection(Render::SceneObjectID objectID);
    bool IsSelected(Render::SceneObjectID objectID) const;
    const std::set<Render::SceneObjectID>& GetSelectedObjects() const { return m_SelectedObjects; }

    // Hover management
    void SetHoveredObject(Render::SceneObjectID objectID);
    Render::SceneObjectID GetHoveredObject() const { return m_HoveredObject; }
    void ClearHover() { m_HoveredObject = Render::InvalidObjectID; }

    // Update renderer with current selection/hover state
    void UpdateRenderer();

private:
    Math::Vec3 RaycastFromMouse(float mouseX, float mouseY, const Render::Camera& camera, int screenWidth, int screenHeight);
    bool RayAABBIntersection(const Math::Vec3& rayOrigin, const Math::Vec3& rayDir, const Math::Vec3& aabbMin, const Math::Vec3& aabbMax, float& t);

    Render::SoftwareRenderer& m_Renderer;
    std::set<Render::SceneObjectID> m_SelectedObjects;
    Render::SceneObjectID m_HoveredObject{Render::InvalidObjectID};
};

} // namespace Solstice::PhysicsPlayground
