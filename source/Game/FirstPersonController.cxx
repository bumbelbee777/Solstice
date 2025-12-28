#include "FirstPersonController.hxx"
#include "Core/Debug.hxx"
#include <cmath>

namespace Solstice::Game {

void FirstPersonControllerSystem::Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera) {
    Registry.ForEach<FirstPersonController, ECS::Transform>([&](ECS::EntityId entity, FirstPersonController& controller, ECS::Transform& transform) {
        // Update movement if physics body exists
        if (Registry.Has<Physics::RigidBody>(entity)) {
            auto& rb = Registry.Get<Physics::RigidBody>(entity);

            // Apply movement
            if (controller.MoveDirection.Magnitude() > 0.01f) {
                Math::Vec3 moveDir = controller.MoveDirection.Normalized();
                float speed = controller.MoveSpeed;
                if (controller.IsSprinting) {
                    speed *= controller.SprintMultiplier;
                }

                // Apply force in movement direction (relative to camera)
                Math::Vec3 forward = Camera.Front;
                forward.y = 0.0f; // Remove vertical component
                forward = forward.Normalized();

                Math::Vec3 right = Camera.Right;
                right.y = 0.0f;
                right = right.Normalized();

                Math::Vec3 worldMoveDir = (forward * moveDir.z + right * moveDir.x) * speed;
                rb.PendingForces.push_back(worldMoveDir * rb.Mass * 10.0f);
            }

            // Handle jumping
            if (controller.IsGrounded && controller.MoveDirection.y > 0.0f) {
                rb.Velocity.y = controller.JumpForce;
                controller.IsGrounded = false;
            }

            // Handle crouching
            if (controller.IsCrouching) {
                rb.CapsuleHeight = controller.CrouchHeight;
            } else {
                rb.CapsuleHeight = controller.NormalHeight;
            }

            // Update transform from physics
            transform.Position = rb.Position;
            transform.Matrix = Math::Matrix4::Translation(rb.Position) * rb.Rotation.ToMatrix();
        } else {
            // Direct movement without physics
            Math::Vec3 moveDir = controller.MoveDirection;
            float speed = controller.MoveSpeed;
            if (controller.IsSprinting) {
                speed *= controller.SprintMultiplier;
            }

            transform.Position += moveDir * speed * DeltaTime;
            transform.Matrix = Math::Matrix4::Translation(transform.Position);
        }

        // Update camera position
        Camera.Position = transform.Position;
        if (controller.IsCrouching) {
            Camera.Position.y -= (controller.NormalHeight - controller.CrouchHeight) * 0.5f;
        }

        // Apply camera effects
        ApplyCameraEffects(controller, Camera, DeltaTime);
    });
}

void FirstPersonControllerSystem::ProcessMouseInput(ECS::Registry& Registry, ECS::EntityId Entity, float MouseX, float MouseY) {
    if (!Registry.Has<FirstPersonController>(Entity)) return;

    auto& controller = Registry.Get<FirstPersonController>(Entity);
    // Mouse input is typically handled by the Camera directly
    // This is a placeholder for controller-specific mouse processing
}

void FirstPersonControllerSystem::ProcessKeyboardInput(ECS::Registry& Registry, ECS::EntityId Entity,
                                                       const Math::Vec3& MoveDirection, bool Jump, bool Crouch, bool Sprint) {
    if (!Registry.Has<FirstPersonController>(Entity)) return;

    auto& controller = Registry.Get<FirstPersonController>(Entity);
    controller.MoveDirection = MoveDirection;
    controller.MoveDirection.y = Jump ? 1.0f : 0.0f;
    controller.IsCrouching = Crouch;
    controller.IsSprinting = Sprint;
}

void FirstPersonControllerSystem::ApplyCameraEffects(FirstPersonController& Controller, Render::Camera& Camera, float DeltaTime) {
    // Head bobbing
    if (Controller.MoveDirection.Magnitude() > 0.01f && Controller.IsGrounded) {
        Controller.HeadBobTimer += DeltaTime * Controller.HeadBobSpeed;
        float bobOffset = std::sin(Controller.HeadBobTimer) * Controller.HeadBobAmount;
        Camera.Position.y += bobOffset;
    }

    // Camera shake decay
    if (Controller.CameraShakeAmount > 0.0f) {
        Controller.CameraShakeAmount -= Controller.CameraShakeDecay * DeltaTime;
        if (Controller.CameraShakeAmount < 0.0f) {
            Controller.CameraShakeAmount = 0.0f;
        }

        // Apply random shake offset
        // In a real implementation, this would use a random number generator
        // For now, we'll use a simple sine-based shake
        float shakeX = std::sin(Controller.HeadBobTimer * 20.0f) * Controller.CameraShakeAmount;
        float shakeY = std::cos(Controller.HeadBobTimer * 20.0f) * Controller.CameraShakeAmount;
        Camera.Position += Camera.Right * shakeX + Camera.Up * shakeY;
    }
}

} // namespace Solstice::Game
