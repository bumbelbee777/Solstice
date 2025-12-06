#pragma once

#include <Entity/Registry.hxx>
#include <Render/Scene.hxx>
#include <Physics/RigidBody.hxx>

namespace Solstice::Render {

// Fast, minimal bridge to push physics positions into the Scene prior to rendering.
// Usage: call once per frame before SoftwareRenderer::RenderScene.
inline void SyncPhysicsToScene(ECS::Registry& R, Scene& S) {
    // Update dynamic objects' positions from physics bodies using the RenderObjectID stored in RigidBody
    R.ForEach<Physics::RigidBody>([&](ECS::EntityId e, Physics::RigidBody& rb) {
        if (rb.RenderObjectID != InvalidObjectID) {
            S.SetPosition(static_cast<SceneObjectID>(rb.RenderObjectID), rb.Position);
            S.SetRotation(static_cast<SceneObjectID>(rb.RenderObjectID), rb.Rotation);
        }
    });
    // Rebuild world matrices and AABBs only once after bulk updates
    S.UpdateTransforms();
}

} // namespace Solstice::Render
