#pragma once

#include "../Solstice.hxx"
#include "FirstPersonController.hxx"
#include "../Entity/Registry.hxx"
#include "../Physics/RigidBody.hxx"
#include "../Render/Camera.hxx"
#include "../Math/Vector.hxx"
#include <string>

namespace Solstice::Game {

// FPS movement component extending FirstPersonController
struct FPSMovement : public FirstPersonController {
    // Movement parameters
    float GroundAcceleration{20.0f};
    float GroundFriction{6.0f};
    float AirAcceleration{1.0f}; // Lower for air strafing
    float AirFriction{0.0f}; // No friction in air (preserves momentum)
    float MaxGroundSpeed{7.0f};
    float MaxAirSpeed{10.0f}; // Can exceed ground speed with bhopping

    // Bunny hopping
    bool EnableBunnyHopping{true};
    float BhopSpeedGain{1.1f}; // Speed multiplier per successful hop

    // Ground detection
    float GroundCheckDistance{0.5f}; // Increased for better ground detection
    float GroundCheckRadius{0.3f};
    bool IsGrounded{false};
    float GroundNormalY{1.0f}; // Y component of ground normal

    // Jump mechanics
    float CoyoteTime{0.1f}; // Time after leaving ground where jump still works
    float CoyoteTimer{0.0f};
    float JumpBufferTime{0.1f}; // Time before landing where jump input is buffered
    float JumpBufferTimer{0.0f};
    bool JumpInput{false};

    // Slope handling
    float MaxSlopeAngle{45.0f}; // Degrees
    bool CanJumpOnSlope{true};

    // Slide mechanics (optional)
    bool EnableSliding{false};
    float SlideSpeed{15.0f};
    float SlideFriction{2.0f};
    bool IsSliding{false};

    // Velocity clamping
    float MaxVelocity{50.0f}; // Absolute max to prevent exploits
    bool ClampVelocity{true};

    // Movement presets
    std::string MovementPreset{"Quake"}; // Quake, CS, Default

    // Internal: desired horizontal velocity for post-physics re-application
    Math::Vec3 DesiredHorizontalVelocity{0, 0, 0};

    // Snow movement properties
    float SnowDepth{0.0f};                    // Current snow depth at player position (meters)
    float SnowResistanceMultiplier{0.0f};    // Velocity damping factor (0.0-1.0)
    float SnowFrictionMultiplier{1.0f};       // Friction adjustment (1.0 = normal, >1.0 = more friction)
    bool EnableSnowMovement{true};            // Toggle for snow effects
};

// Forward declaration (we're already in Solstice::Game namespace)
class SnowSystem;

// FPS movement system
class SOLSTICE_API FPSMovementSystem {
public:
    // Update movement (call each frame)
    // SnowSystem is optional - if provided, snow movement modifiers will be applied
    static void Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera, SnowSystem* SnowSys = nullptr);

    // Process input
    static void ProcessInput(ECS::Registry& Registry, ECS::EntityId Entity,
                            const Math::Vec3& MoveDirection, bool Jump, bool Crouch, bool Sprint);

    // Apply movement presets
    static void ApplyPreset(FPSMovement& Movement, const std::string& PresetName);

    // Helper to calculate wish direction (public for post-physics velocity re-application)
    static Math::Vec3 GetWishDirection(const Math::Vec3& MoveInput, const Math::Vec3& Forward, const Math::Vec3& Right);

private:
    static void UpdateGroundDetection(ECS::Registry& Registry, ECS::EntityId Entity, FPSMovement& Movement);
    static void UpdateMovement(ECS::Registry& Registry, ECS::EntityId Entity, FPSMovement& Movement,
                              const Render::Camera& Camera, float DeltaTime, SnowSystem* SnowSys = nullptr);
    static void ApplyGroundMovement(Physics::RigidBody& RB, FPSMovement& Movement,
                                   const Math::Vec3& WishDir, float DeltaTime, SnowSystem* SnowSys = nullptr);
    static void ApplyAirMovement(Physics::RigidBody& RB, FPSMovement& Movement,
                                const Math::Vec3& WishDir, const Math::Vec3& Forward, const Math::Vec3& Right,
                                float DeltaTime);
    static void ApplyBunnyHop(Physics::RigidBody& RB, FPSMovement& Movement, float DeltaTime);
};

} // namespace Solstice::Game
