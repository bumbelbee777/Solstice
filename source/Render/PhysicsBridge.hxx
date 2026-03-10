#pragma once

#include <Entity/Registry.hxx>
#include <Render/Scene/Scene.hxx>
#include <Physics/RigidBody.hxx>

namespace Solstice::Render {

// Fast, minimal bridge to push physics positions into the Scene prior to rendering.
// Usage: call once per frame before SoftwareRenderer::RenderScene.
// Optimized: Batch updates and only sync objects that have moved
inline void SyncPhysicsToScene(ECS::Registry& R, Scene& S) {
    // Batch all position/rotation updates first, then update transforms once
    // This reduces the number of transform rebuilds
    uint32_t updateCount = 0;
    R.ForEach<Physics::RigidBody>([&](ECS::EntityId e, Physics::RigidBody& rb) {
        if (rb.RenderObjectID != InvalidObjectID) {
            S.SetPosition(static_cast<SceneObjectID>(rb.RenderObjectID), rb.Position);
            S.SetRotation(static_cast<SceneObjectID>(rb.RenderObjectID), rb.Rotation);
            updateCount++;
        }
    });

    // Rebuild world matrices and AABBs only once after bulk updates
    // This is more efficient than rebuilding after each individual update
    if (updateCount > 0) {
        S.UpdateTransforms();
    }
}

} // namespace Solstice::Render
