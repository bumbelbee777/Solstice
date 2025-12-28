#pragma once

#include "../Solstice.hxx"
#include "../Entity/Registry.hxx"
#include "../Entity/Transform.hxx"
#include "../Physics/RigidBody.hxx"
#include "../Render/Camera.hxx"
#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"

namespace Solstice::Game {

// First-person controller component
struct FirstPersonController {
    float MoveSpeed{5.0f};
    float SprintMultiplier{1.5f};
    float JumpForce{5.0f};
    float MouseSensitivity{0.1f};
    float CrouchHeight{0.5f};
    float NormalHeight{1.8f};

    bool IsGrounded{false};
    bool IsCrouching{false};
    bool IsSprinting{false};

    float HeadBobAmount{0.02f};
    float HeadBobSpeed{10.0f};
    float HeadBobTimer{0.0f};

    float CameraShakeAmount{0.0f};
    float CameraShakeDecay{5.0f};

    Math::Vec3 MoveDirection{0.0f, 0.0f, 0.0f};
};

// First-person controller system
class SOLSTICE_API FirstPersonControllerSystem {
public:
    // Update controller (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera);

    // Handle mouse input
    static void ProcessMouseInput(ECS::Registry& Registry, ECS::EntityId Entity, float MouseX, float MouseY);

    // Handle keyboard input
    static void ProcessKeyboardInput(ECS::Registry& Registry, ECS::EntityId Entity,
                                     const Math::Vec3& MoveDirection, bool Jump, bool Crouch, bool Sprint);

    // Apply camera effects
    static void ApplyCameraEffects(FirstPersonController& Controller, Render::Camera& Camera, float DeltaTime);
};

} // namespace Solstice::Game
