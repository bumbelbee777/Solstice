#pragma once

#include <Entity/Registry.hxx>
#include <Render/Scene/Scene.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Render {

// Fast, minimal bridge to push physics positions into the Scene prior to rendering.
// Usage: call once per frame before SoftwareRenderer::RenderScene.
// With fixed-timestep physics, PhysicsSystem::GetSceneRenderBlendForSync() blends from the substep
// start snapshot toward the current pose so the world moves continuously while the camera uses variable dt.
inline void SyncPhysicsToScene(ECS::Registry& R, Scene& S) {
    const float tRaw = Solstice::Physics::PhysicsSystem::Instance().GetSceneRenderBlendForSync();
    const float t = std::clamp(tRaw, 0.0f, 1.0f);
    // Batch all position/rotation updates first, then update transforms once
    uint32_t updateCount = 0;
    R.ForEach<Physics::RigidBody>([&](ECS::EntityId, Physics::RigidBody& rb) {
        if (rb.RenderObjectID == InvalidObjectID) {
            return;
        }
        Math::Vec3 p = rb.Position;
        Math::Quaternion q = rb.Rotation;
        if (!rb.IsStatic && !rb.IsGrabbed && rb.HasRenderInterpolationSnapshot) {
            p = Math::Vec3::Lerp(rb.RenderInterpFromPos, rb.Position, t);
            q = Math::Quaternion::Slerp(rb.RenderInterpFromRot, rb.Rotation, t);
        }
        S.SetPosition(static_cast<SceneObjectID>(rb.RenderObjectID), p);
        S.SetRotation(static_cast<SceneObjectID>(rb.RenderObjectID), q);
        updateCount++;
    });

    if (updateCount > 0) {
        S.UpdateTransforms();
    }
}

} // namespace Solstice::Render
