#include "FPSMovement.hxx"
#include "SnowSystem.hxx"
#include "../Core/Debug.hxx"
#include "../Physics/PhysicsSystem.hxx"
#include "../Physics/ReactPhysics3DBridge.hxx"
#include <cmath>
#include <algorithm>

namespace Solstice::Game {

void FPSMovementSystem::Update(ECS::Registry& Registry, float DeltaTime, Render::Camera& Camera, SnowSystem* SnowSys) {
    Registry.ForEach<FPSMovement, ECS::Transform>([&](ECS::EntityId entity, FPSMovement& movement, ECS::Transform& transform) {
        if (!Registry.Has<Physics::RigidBody>(entity)) return;

        auto& rb = Registry.Get<Physics::RigidBody>(entity);

        // Update snow depth if snow system is available
        if (SnowSys != nullptr && movement.EnableSnowMovement) {
            movement.SnowDepth = SnowSys->CalculateSnowDepth(rb.Position);
            movement.SnowResistanceMultiplier = SnowSys->GetSnowResistance(movement.SnowDepth);
            movement.SnowFrictionMultiplier = SnowSys->GetSnowFriction(movement.SnowDepth);
        } else {
            movement.SnowDepth = 0.0f;
            movement.SnowResistanceMultiplier = 0.0f;
            movement.SnowFrictionMultiplier = 1.0f;
        }

        // Update ground detection
        UpdateGroundDetection(Registry, entity, movement);

        // Update movement (modifies velocity)
        UpdateMovement(Registry, entity, movement, Camera, DeltaTime, SnowSys);

        // CRITICAL: Sync velocity to ReactPhysics3D IMMEDIATELY after setting it
        // This ensures ReactPhysics3D has the correct velocity before physics runs
        Physics::ReactPhysics3DBridge& MovementBridge = Physics::PhysicsSystem::Instance().GetBridge();

        // Apply snow resistance to velocity before syncing (if snow is enabled)
        if (movement.EnableSnowMovement && movement.SnowResistanceMultiplier > 0.0f) {
            Math::Vec3 horizontalVel = rb.Velocity;
            horizontalVel.y = 0.0f;
            float speed = horizontalVel.Magnitude();
            if (speed > 0.01f) {
                // Apply resistance: reduce velocity based on snow depth
                float resistance = movement.SnowResistanceMultiplier;
                horizontalVel = horizontalVel * (1.0f - resistance);
                rb.Velocity.x = horizontalVel.x;
                rb.Velocity.z = horizontalVel.z;
            }
        }

        MovementBridge.SyncToReactPhysics3D();

        // Store desired horizontal velocity AFTER movement update for post-physics re-application
        Math::Vec3 desiredHorizontalVel = rb.Velocity;
        desiredHorizontalVel.y = 0.0f;

        // If no movement input, ensure velocity is zeroed
        if (movement.MoveDirection.Magnitude() < 0.01f) {
            desiredHorizontalVel = Math::Vec3(0, 0, 0);
            rb.Velocity.x = 0.0f;
            rb.Velocity.z = 0.0f;
            // Sync the zeroed velocity
            MovementBridge.SyncToReactPhysics3D();
        }

        // Store desired velocity in movement component for post-physics re-application
        movement.DesiredHorizontalVelocity = desiredHorizontalVel;

        // Update coyote time and jump buffer
        if (movement.IsGrounded) {
            movement.CoyoteTimer = movement.CoyoteTime;
        } else {
            movement.CoyoteTimer -= DeltaTime;
        }

        if (movement.JumpInput) {
            movement.JumpBufferTimer = movement.JumpBufferTime;
        } else {
            movement.JumpBufferTimer -= DeltaTime;
        }

        // Handle jumping
        if (movement.JumpBufferTimer > 0.0f && (movement.IsGrounded || movement.CoyoteTimer > 0.0f)) {
            // Check slope
            bool canJump = movement.CanJumpOnSlope || movement.GroundNormalY > 0.9f; // ~25 degrees

            if (canJump) {
                rb.Velocity.y = movement.JumpForce;
                movement.IsGrounded = false;
                movement.CoyoteTimer = 0.0f;
                movement.JumpBufferTimer = 0.0f;
                movement.JumpInput = false;

                // Apply bunny hop speed gain
                if (movement.EnableBunnyHopping && !movement.IsGrounded) {
                    Math::Vec3 horizontalVel = rb.Velocity;
                    horizontalVel.y = 0.0f;
                    float speed = horizontalVel.Magnitude();
                    if (speed > 0.1f) {
                        horizontalVel = horizontalVel.Normalized() * std::min(speed * movement.BhopSpeedGain, movement.MaxAirSpeed);
                        rb.Velocity.x = horizontalVel.x;
                        rb.Velocity.z = horizontalVel.z;
                    }
                }
            }
        }

        // Sync velocity to ReactPhysics3D immediately after modifying it
        Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
        Bridge.SyncToReactPhysics3D();

        // Update desired velocity after movement update (in case it changed)
        Math::Vec3 currentHorizontalVel = rb.Velocity;
        currentHorizontalVel.y = 0.0f;
        movement.DesiredHorizontalVelocity = currentHorizontalVel;

    // Apply snow sinking effect to position (visual/feel only)
    Math::Vec3 finalPosition = rb.Position;
    if (SnowSys != nullptr && movement.EnableSnowMovement && movement.IsGrounded && movement.SnowDepth > 0.01f) {
            float horizontalSpeed = Math::Vec3(rb.Velocity.x, 0, rb.Velocity.z).Magnitude();
            float sinkOffset = SnowSys->GetSinkOffset(movement.SnowDepth, horizontalSpeed);
            finalPosition.y -= sinkOffset;
        }

        // Update transform from physics
        transform.Position = finalPosition;
        transform.Matrix = Math::Matrix4::Translation(finalPosition) * rb.Rotation.ToMatrix();

        // Update camera position
        Camera.Position = finalPosition;
        if (movement.IsCrouching) {
            Camera.Position.y -= (movement.NormalHeight - movement.CrouchHeight) * 0.5f;
        }

        // Apply camera effects from FirstPersonController
        FirstPersonControllerSystem::ApplyCameraEffects(movement, Camera, DeltaTime);

        // Clamp velocity to prevent exploits
        if (movement.ClampVelocity) {
            float speed = rb.Velocity.Magnitude();
            if (speed > movement.MaxVelocity) {
                rb.Velocity = rb.Velocity.Normalized() * movement.MaxVelocity;
                // Re-sync after clamping
                Bridge.SyncToReactPhysics3D();
            }
        }
    });
}

void FPSMovementSystem::ProcessInput(ECS::Registry& Registry, ECS::EntityId Entity,
                                     const Math::Vec3& MoveDirection, bool Jump, bool Crouch, bool Sprint) {
    if (!Registry.Has<FPSMovement>(Entity)) return;

    FPSMovement& movement = Registry.Get<FPSMovement>(Entity);
    movement.MoveDirection = MoveDirection;
    movement.JumpInput = Jump;
    movement.IsCrouching = Crouch;
    movement.IsSprinting = Sprint;
}

void FPSMovementSystem::ApplyPreset(FPSMovement& Movement, const std::string& PresetName) {
    Movement.MovementPreset = PresetName;

    if (PresetName == "Quake") {
        Movement.GroundAcceleration = 20.0f;
        Movement.GroundFriction = 6.0f;
        Movement.AirAcceleration = 1.0f;
        Movement.AirFriction = 0.0f;
        Movement.MaxGroundSpeed = 7.0f;
        Movement.MaxAirSpeed = 10.0f;
        Movement.EnableBunnyHopping = true;
        Movement.BhopSpeedGain = 1.1f;
    } else if (PresetName == "CS") {
        Movement.GroundAcceleration = 5.5f;
        Movement.GroundFriction = 5.5f;
        Movement.AirAcceleration = 0.0f;
        Movement.AirFriction = 0.0f;
        Movement.MaxGroundSpeed = 5.5f;
        Movement.MaxAirSpeed = 5.5f;
        Movement.EnableBunnyHopping = false;
    } else {
        // Default
        Movement.GroundAcceleration = 20.0f;
        Movement.GroundFriction = 6.0f;
        Movement.AirAcceleration = 1.0f;
        Movement.AirFriction = 0.0f;
        Movement.MaxGroundSpeed = 7.0f;
        Movement.MaxAirSpeed = 10.0f;
        Movement.EnableBunnyHopping = true;
    }
}

void FPSMovementSystem::UpdateGroundDetection(ECS::Registry& Registry, ECS::EntityId Entity, FPSMovement& Movement) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);

    // Improved ground check: check against static terrain collision boxes
    float capsuleBottom = rb.Position.y - rb.CapsuleHeight * 0.5f - rb.CapsuleRadius;
    float groundY = -1000.0f; // Default to very low if no ground found
    bool foundGround = false;

    // Check all static rigid bodies (terrain, etc.) for ground collision
    Registry.ForEach<Physics::RigidBody>([&](ECS::EntityId otherEntity, Physics::RigidBody& otherRB) {
        // Skip self and non-static bodies
        if (otherEntity == Entity || !otherRB.IsStatic) return;

        // Check if player is within XZ bounds of this collision box
        if (otherRB.Type == Physics::ColliderType::Box) {
            Math::Vec3 playerPos = rb.Position;
            Math::Vec3 boxMin = otherRB.Position - otherRB.HalfExtents;
            Math::Vec3 boxMax = otherRB.Position + otherRB.HalfExtents;

            // Check XZ overlap
            if (playerPos.x >= boxMin.x && playerPos.x <= boxMax.x &&
                playerPos.z >= boxMin.z && playerPos.z <= boxMax.z) {
                // Player is above this terrain box
                float boxTop = boxMax.y;
                if (boxTop > groundY && capsuleBottom <= boxTop + Movement.GroundCheckDistance) {
                    groundY = boxTop;
                    foundGround = true;
                }
            }
        }
    });

    // Check if player is on ground
    if (foundGround) {
    Movement.IsGrounded = (capsuleBottom - groundY) < Movement.GroundCheckDistance;
    Movement.GroundNormalY = 1.0f; // Would be from raycast normal
    } else {
        Movement.IsGrounded = false;
        Movement.GroundNormalY = 1.0f;
    }
}

void FPSMovementSystem::UpdateMovement(ECS::Registry& Registry, ECS::EntityId Entity, FPSMovement& Movement,
                                       const Render::Camera& Camera, float DeltaTime, SnowSystem* SnowSys) {
    if (!Registry.Has<Physics::RigidBody>(Entity)) return;

    auto& rb = Registry.Get<Physics::RigidBody>(Entity);

    // Get camera directions
    Math::Vec3 forward = Camera.Front;
    forward.y = 0.0f;
    if (forward.Magnitude() < 0.01f) {
        // Fallback if camera not initialized - use default forward
        forward = Math::Vec3(0, 0, 1);
    } else {
    forward = forward.Normalized();
    }

    Math::Vec3 right = Camera.Right;
    right.y = 0.0f;
    if (right.Magnitude() < 0.01f) {
        // Fallback if camera not initialized - use default right
        right = Math::Vec3(1, 0, 0);
    } else {
    right = right.Normalized();
    }

    // Get wish direction
    Math::Vec3 wishDir = GetWishDirection(Movement.MoveDirection, forward, right);

    // If no movement input, still apply friction but skip acceleration
    if (Movement.MoveDirection.Magnitude() < 0.01f) {
        // Apply friction to stop sliding
        if (Movement.IsGrounded) {
            Math::Vec3 horizontalVel = rb.Velocity;
            horizontalVel.y = 0.0f;
            float currentSpeed = horizontalVel.Magnitude();
            if (currentSpeed > 0.01f) {
                float friction = Movement.GroundFriction * DeltaTime * 10.0f;
                if (currentSpeed > friction) {
                    horizontalVel = horizontalVel.Normalized() * (currentSpeed - friction);
                } else {
                    horizontalVel = Math::Vec3(0, 0, 0);
                }
                rb.Velocity.x = horizontalVel.x;
                rb.Velocity.z = horizontalVel.z;
                Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
                Bridge.SyncToReactPhysics3D();
            }
        }
        return;
    }

    // If wishDir is invalid but we have input, use raw input direction as fallback
    if (wishDir.Magnitude() < 0.01f && Movement.MoveDirection.Magnitude() > 0.01f) {
        // Fallback: use world-space movement (forward = +Z, right = +X)
        wishDir = Math::Vec3(Movement.MoveDirection.x, 0, Movement.MoveDirection.z);
        float mag = wishDir.Magnitude();
        if (mag > 0.01f) {
            wishDir = wishDir / mag; // Normalize manually
        } else {
            // Ultimate fallback: just use forward direction
            wishDir = Math::Vec3(0, 0, 1);
        }
    }

    // DEBUG: Force movement if input detected (temporary test)
    if (Movement.MoveDirection.Magnitude() > 0.01f && wishDir.Magnitude() < 0.01f) {
        // Emergency fallback - just move in input direction
        wishDir = Math::Vec3(Movement.MoveDirection.x, 0, Movement.MoveDirection.z);
        float mag = wishDir.Magnitude();
        if (mag > 0.01f) {
            wishDir = wishDir / mag;
        } else {
            wishDir = Math::Vec3(0, 0, 1);
        }
    }

    // DEBUG: Force ground movement if we have input (temporary test)
    // This ensures movement works even if ground detection fails
    bool forceGroundMovement = (Movement.MoveDirection.Magnitude() > 0.01f && wishDir.Magnitude() > 0.01f);

    // Apply snow sinking effect if snow system is available
    if (SnowSys != nullptr && Movement.EnableSnowMovement && Movement.IsGrounded) {
        float horizontalSpeed = Math::Vec3(rb.Velocity.x, 0, rb.Velocity.z).Magnitude();
        float sinkOffset = SnowSys->GetSinkOffset(Movement.SnowDepth, horizontalSpeed);
        // Adjust position slightly downward based on snow depth
        // This is a visual/feel effect - actual collision stays at ground level
        // We'll apply this after physics, so just store it for now
    }

    if (Movement.IsGrounded || forceGroundMovement) {
        ApplyGroundMovement(rb, Movement, wishDir, DeltaTime, SnowSys);
    } else {
        ApplyAirMovement(rb, Movement, wishDir, forward, right, DeltaTime);

        if (Movement.EnableBunnyHopping) {
            ApplyBunnyHop(rb, Movement, DeltaTime);
        }
    }
}

void FPSMovementSystem::ApplyGroundMovement(Physics::RigidBody& RB, FPSMovement& Movement,
                                            const Math::Vec3& WishDir, float DeltaTime, SnowSystem* SnowSys) {
    Math::Vec3 horizontalVel = RB.Velocity;
    horizontalVel.y = 0.0f;
    float currentSpeed = horizontalVel.Magnitude();

    // Apply snow friction modifier
    float effectiveFriction = Movement.GroundFriction;
    if (Movement.EnableSnowMovement && Movement.SnowFrictionMultiplier > 1.0f) {
        effectiveFriction = Movement.GroundFriction * Movement.SnowFrictionMultiplier;
    }

    // CRITICAL: Always apply movement if we have a valid wish direction
    if (WishDir.Magnitude() > 0.01f) {
        // Accelerate towards wish direction
        // Snow reduces effective acceleration (resistance applied later)
        float accel = Movement.GroundAcceleration;
        Math::Vec3 accelVec = WishDir * accel * DeltaTime;
        horizontalVel += accelVec;

        // Let acceleration work naturally - don't force minimum velocity
        // The acceleration will build up speed over time

        // Clamp to max ground speed (snow resistance will further reduce this)
        float speed = horizontalVel.Magnitude();
        if (speed > Movement.MaxGroundSpeed) {
            horizontalVel = horizontalVel.Normalized() * Movement.MaxGroundSpeed;
        }
    } else {
        // Apply friction when no input - use effective friction (snow increases it)
        float friction = effectiveFriction * DeltaTime * 10.0f; // Very strong friction to stop sliding
        if (currentSpeed > friction) {
            horizontalVel = horizontalVel.Normalized() * (currentSpeed - friction);
        } else {
            horizontalVel = Math::Vec3(0, 0, 0); // Stop completely
        }
    }

    // Preserve Y velocity (gravity, jumping, etc.)
    float oldY = RB.Velocity.y;
    RB.Velocity.x = horizontalVel.x;
    RB.Velocity.z = horizontalVel.z;
    RB.Velocity.y = oldY; // Restore Y velocity

    // Update RigidBody friction for physics system (snow increases friction)
    if (Movement.EnableSnowMovement && Movement.SnowFrictionMultiplier > 1.0f) {
        RB.Friction = 0.6f * Movement.SnowFrictionMultiplier; // Base friction * snow multiplier
    } else {
        RB.Friction = 0.6f; // Default friction
    }

    // Immediately sync to ReactPhysics3D to ensure velocity is applied
    Physics::ReactPhysics3DBridge& Bridge = Physics::PhysicsSystem::Instance().GetBridge();
    Bridge.SyncToReactPhysics3D();
}

void FPSMovementSystem::ApplyAirMovement(Physics::RigidBody& RB, FPSMovement& Movement,
                                         const Math::Vec3& WishDir, const Math::Vec3& Forward, const Math::Vec3& Right,
                                         float DeltaTime) {
    Math::Vec3 horizontalVel = RB.Velocity;
    horizontalVel.y = 0.0f;
    float currentSpeed = horizontalVel.Magnitude();

    // Use ground acceleration in air if air acceleration is too low (fallback for better control)
    float airAccel = Movement.AirAcceleration > 0.1f ? Movement.AirAcceleration : Movement.GroundAcceleration * 0.5f;

    if (WishDir.Magnitude() > 0.01f && airAccel > 0.0f) {
        // Air strafing: accelerate perpendicular to current velocity
        Math::Vec3 velDir = currentSpeed > 0.1f ? horizontalVel.Normalized() : WishDir;

        // Project wish direction onto plane perpendicular to velocity
        Math::Vec3 perpDir = WishDir - velDir * WishDir.Dot(velDir);
        perpDir = perpDir.Normalized();

        // Accelerate in perpendicular direction (air strafing)
        Math::Vec3 accel = perpDir * airAccel * DeltaTime;
        horizontalVel += accel;

        // Clamp to max air speed
        float speed = horizontalVel.Magnitude();
        if (speed > Movement.MaxAirSpeed) {
            horizontalVel = horizontalVel.Normalized() * Movement.MaxAirSpeed;
        }
    }

    // Apply minimal air friction
    if (Movement.AirFriction > 0.0f) {
        float friction = Movement.AirFriction * DeltaTime;
        float speed = horizontalVel.Magnitude();
        if (speed > friction) {
            horizontalVel = horizontalVel.Normalized() * (speed - friction);
        } else {
            horizontalVel = Math::Vec3(0, 0, 0);
        }
    }

    // Preserve Y velocity (gravity, jumping, etc.)
    float oldY = RB.Velocity.y;
    RB.Velocity.x = horizontalVel.x;
    RB.Velocity.z = horizontalVel.z;
    RB.Velocity.y = oldY; // Restore Y velocity
}

void FPSMovementSystem::ApplyBunnyHop(Physics::RigidBody& RB, FPSMovement& Movement, float DeltaTime) {
    (void)DeltaTime;

    // Bunny hopping is handled in the jump section
    // This function is for additional bhop mechanics if needed
}

Math::Vec3 FPSMovementSystem::GetWishDirection(const Math::Vec3& MoveInput, const Math::Vec3& Forward, const Math::Vec3& Right) {
    // If no input, return zero
    if (MoveInput.Magnitude() < 0.01f) {
        return Math::Vec3(0, 0, 0);
    }

    Math::Vec3 wishDir = Forward * MoveInput.z + Right * MoveInput.x;
    float mag = wishDir.Magnitude();
    if (mag < 0.01f) {
        // Camera vectors might be invalid - use fallback direction based on input
        return Math::Vec3(MoveInput.x, 0, MoveInput.z).Normalized();
    }
    return wishDir.Normalized();
}

} // namespace Solstice::Game
