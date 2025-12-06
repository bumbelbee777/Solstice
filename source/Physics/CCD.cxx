#include "CCD.hxx"
#include "../Math/Vector.hxx"
#include "../Core/Debug.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {

// Helper: Get corners of a box (local space) - Duplicated from PhysicsSystem for now, should be in a utility
static void GetBoxCorners(const Math::Vec3& halfExtents, Math::Vec3 corners[8]) {
    corners[0] = Math::Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
    corners[1] = Math::Vec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
    corners[2] = Math::Vec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
    corners[3] = Math::Vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
    corners[4] = Math::Vec3(-halfExtents.x, -halfExtents.y,  halfExtents.z);
    corners[5] = Math::Vec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
    corners[6] = Math::Vec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
    corners[7] = Math::Vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);
}

// Helper: Transform point using quaternion rotation
static Math::Vec3 TransformPoint(const Math::Vec3& p, const Math::Vec3& pos, const Math::Quaternion& rot) {
    float x = rot.x, y = rot.y, z = rot.z, w = rot.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    Math::Vec3 result;
    result.x = p.x * (1.0f - yy - zz) + p.y * (xy - wz) + p.z * (xz + wy) + pos.x;
    result.y = p.x * (xy + wz) + p.y * (1.0f - xx - zz) + p.z * (yz - wx) + pos.y;
    result.z = p.x * (xz - wy) + p.y * (yz + wx) + p.z * (1.0f - xx - yy) + pos.z;
    return result;
}

void CCD::PerformCCD(Solstice::ECS::Registry& registry, float dt) {
    // CCD implementation for fast-moving objects
    
    // Threshold for CCD (e.g., if moving more than min radius per frame)
    const float ccdSpeedThreshold = 5.0f; 

    registry.ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        if (rb.IsStatic) return;

        float speed = rb.Velocity.Magnitude();
        if (speed < ccdSpeedThreshold) return;

        // --- Ground Plane CCD ---
        // Check if we are moving downwards
        if (rb.Velocity.y < 0.0f) {
            float bottomY = 0.0f;
            if (rb.Type == ColliderType::Sphere) {
                bottomY = rb.Position.y - rb.Radius;
            } else if (rb.Type == ColliderType::Box) {
                // Approximate bottom for box (using half extent y)
                // Ideally we should check rotated corners, but for simple stacking this is often enough
                // or we can take the lowest corner.
                // Let's take the lowest corner for robustness.
                Math::Vec3 corners[8];
                GetBoxCorners(rb.HalfExtents, corners);
                float minCornerY = 1e9f;
                for(int i=0; i<8; ++i) {
                    Math::Vec3 worldPos = TransformPoint(corners[i], rb.Position, rb.Rotation);
                    if(worldPos.y < minCornerY) minCornerY = worldPos.y;
                }
                bottomY = minCornerY;
            } else {
                // Default fallback for other types (Triangle, ConvexHull)
                // Just use position for now, or maybe bounding sphere?
                bottomY = rb.Position.y - rb.Radius; // Assuming Radius is bounding radius
            }

            // Predict next position
            float nextBottomY = bottomY + rb.Velocity.y * dt;

            // Check for tunneling through ground (y=0)
            if (bottomY >= 0.0f && nextBottomY < 0.0f) {
                // Tunneling detected!
                // Calculate Time of Impact (TOI)
                // bottomY + vel * dt * t = 0
                // t = -bottomY / (vel * dt)
                float t = -bottomY / (rb.Velocity.y * dt);
                t = std::clamp(t, 0.0f, 1.0f);

                // Move body to impact point (plus epsilon)
                Math::Vec3 motion = rb.Velocity * dt * t;
                rb.Position += motion;
                
                // Stop vertical movement (inelastic collision for stability)
                rb.Velocity.y = 0.0f;
                
                // Nudge up slightly to prevent sinking
                rb.Position.y += 0.001f;
                
                // Update derived data (like corners) if we were caching them
            }
        }
    });
}

float CCD::SweptSphereCast(const RigidBody& sphere, const Math::Vec3& motion, const RigidBody& target) {
    // Swept sphere vs static geometry
    // Returns TOI (time of impact) in [0, 1], or >1 if no collision
    
    // This is a simplified implementation
    // Full CCD would require proper swept shape tests
    return 2.0f; // No collision
}

} // namespace Solstice::Physics
