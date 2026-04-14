#include "ThirdPersonController.hxx"
#include "../../Core/Debug/Debug.hxx"
#include <cmath>

namespace Solstice::Game {

void ThirdPersonControllerSystem::Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera) {
    Registry.ForEach<ThirdPersonController, ECS::Transform>([&](ECS::EntityId entity, ThirdPersonController& controller, ECS::Transform& transform) {
        // Calculate desired camera position
        Math::Vec3 targetPos = transform.Position;
        targetPos.y += controller.CameraHeight;

        // Calculate camera offset based on angle and distance
        float angleRad = controller.CameraAngle * 3.14159265f / 180.0f;
        float horizontalDist = controller.CameraDistance * std::cos(angleRad);
        float verticalDist = controller.CameraDistance * std::sin(angleRad);

        // Apply shoulder offset
        Math::Vec3 right = Camera.Right;
        if (controller.ShoulderSide) {
            right = right * -1.0f; // Left shoulder
        }

        Math::Vec3 desiredPos = targetPos;
        desiredPos -= Camera.Front * horizontalDist;
        desiredPos += right * controller.ShoulderOffset;
        desiredPos.y += verticalDist;

        // Check collision if enabled
        if (controller.CollisionEnabled) {
            desiredPos = CheckCameraCollision(desiredPos, targetPos, controller.CollisionRadius);
        }

        // Set target position and update with interpolation
        Camera.SetTargetPosition(desiredPos);
        Camera.SetInterpolationSpeed(controller.FollowSpeed * 10.0f); // Scale to match old behavior
        Camera.Update(DeltaTime);

        // Make camera look at target
        Math::Vec3 lookDir = (targetPos - Camera.Position).Normalized();
        Camera.Front = lookDir;
        // UpdateCameraVectors is private, so we'll use ProcessMouseMovement(0,0) to trigger it
        Camera.ProcessMouseMovement(0.0f, 0.0f, false);
    });
}

void ThirdPersonControllerSystem::ProcessMouseInput(ECS::Registry& Registry, ECS::EntityId Entity, float MouseX, float MouseY) {
    if (!Registry.Has<ThirdPersonController>(Entity)) return;

    auto& controller = Registry.Get<ThirdPersonController>(Entity);

    // Adjust camera angle (vertical)
    controller.CameraAngle += MouseY * 0.1f;
    controller.CameraAngle = std::max(controller.MinAngle, std::min(controller.MaxAngle, controller.CameraAngle));

    // Adjust camera distance (scroll would be handled separately)
    // For now, mouse X could rotate around the target
}

void ThirdPersonControllerSystem::SetCameraDistance(ECS::Registry& Registry, ECS::EntityId Entity, float Distance) {
    if (!Registry.Has<ThirdPersonController>(Entity)) return;

    auto& controller = Registry.Get<ThirdPersonController>(Entity);
    controller.CameraDistance = std::max(controller.MinDistance, std::min(controller.MaxDistance, Distance));
}

void ThirdPersonControllerSystem::SwitchShoulder(ECS::Registry& Registry, ECS::EntityId Entity) {
    if (!Registry.Has<ThirdPersonController>(Entity)) return;

    auto& controller = Registry.Get<ThirdPersonController>(Entity);
    controller.ShoulderSide = !controller.ShoulderSide;
}

Math::Vec3 ThirdPersonControllerSystem::CheckCameraCollision(const Math::Vec3& CameraPos, const Math::Vec3& TargetPos, float Radius) {
    // Placeholder - would perform raycast or sphere cast from target to camera
    // For now, just return the camera position
    return CameraPos;
}

} // namespace Solstice::Game
