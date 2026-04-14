#pragma once

#include "../../Solstice.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/Transform.hxx"
#include "../../Physics/Dynamics/RigidBody.hxx"
#include <Render/Scene/Camera.hxx>
#include "../../Math/Vector.hxx"
#include "../../Math/Quaternion.hxx"

namespace Solstice::Game {

// Third-person controller component
struct ThirdPersonController {
    float CameraDistance{5.0f};
    float CameraHeight{2.0f};
    float CameraAngle{45.0f}; // Vertical angle in degrees
    float ShoulderOffset{0.5f}; // Left/right shoulder offset
    bool ShoulderSide{false}; // false = right, true = left

    float MinDistance{2.0f};
    float MaxDistance{10.0f};
    float MinAngle{10.0f};
    float MaxAngle{80.0f};

    float FollowSpeed{10.0f};
    float RotationSpeed{5.0f};

    bool CollisionEnabled{true};
    float CollisionRadius{0.3f};
};

// Third-person controller system
class SOLSTICE_API ThirdPersonControllerSystem {
public:
    // Update controller (call each frame)
    static void Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera);

    // Handle mouse input for camera orbit
    static void ProcessMouseInput(ECS::Registry& Registry, ECS::EntityId Entity, float MouseX, float MouseY);

    // Set camera distance
    static void SetCameraDistance(ECS::Registry& Registry, ECS::EntityId Entity, float Distance);

    // Switch shoulder side
    static void SwitchShoulder(ECS::Registry& Registry, ECS::EntityId Entity);

    // Handle camera collision with environment
    static Math::Vec3 CheckCameraCollision(const Math::Vec3& CameraPos, const Math::Vec3& TargetPos, float Radius);
};

} // namespace Solstice::Game
