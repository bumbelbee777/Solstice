#include <Physics/Collision/Broadphase/CCD.hxx>
#include <Physics/Collision/Broadphase/BVH.hxx>
#include <Physics/Collision/Narrowphase/ConvexHull.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
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

// Helper: Get bounding radius for a rigid body (for conservative CCD)
static float GetBoundingRadius(const RigidBody& rb) {
    switch (rb.Type) {
        case ColliderType::Sphere:
            return rb.Radius;
        case ColliderType::Box: {
            // Max distance from center to corner
            return rb.HalfExtents.Magnitude();
        }
        case ColliderType::ConvexHull:
            // Use stored radius if available, otherwise compute from hull
            if (rb.Radius > 0.0f) {
                return rb.Radius; // Bounding sphere radius
            } else if (rb.Hull && !rb.Hull->Vertices.empty()) {
                // Compute max distance from centroid
                float maxDist = 0.0f;
                for (const auto& v : rb.Hull->Vertices) {
                    float dist = v.Magnitude();
                    if (dist > maxDist) maxDist = dist;
                }
                return maxDist;
            }
            return 0.5f; // Fallback
        case ColliderType::Capsule: {
            // Capsule bounding radius = radius + half height
            float halfHeight = rb.CapsuleHeight * 0.5f;
            return rb.CapsuleRadius + halfHeight;
        }
        case ColliderType::Cylinder: {
            // Cylinder bounding radius = sqrt(radius^2 + (height/2)^2)
            float halfHeight = rb.CylinderHeight * 0.5f;
            return std::sqrt(rb.CylinderRadius * rb.CylinderRadius + halfHeight * halfHeight);
        }
        case ColliderType::Tetrahedron:
            // Tetrahedron uses HullVertices (expected 4 vertices)
            if (!rb.HullVertices.empty()) {
                float maxDist = 0.0f;
                for (const auto& v : rb.HullVertices) {
                    float dist = v.Magnitude();
                    if (dist > maxDist) maxDist = dist;
                }
                return maxDist;
            }
            // Fallback to radius field if set
            return rb.Radius;
        default:
            return rb.Radius; // Default to radius field
    }
}

// Helper: Swept convex hull vs box (conservative approximation)
static float SweptConvexHullVsBox(const RigidBody& hull, const Math::Vec3& motion, const RigidBody& box) {
    // Use a more accurate approach: expand the box by the hull's bounding radius
    // This is more accurate than treating the hull as a sphere

    // Get hull bounding radius
    float hullRadius = GetBoundingRadius(hull);

    // Use a slightly larger radius to account for the fact that convex hulls
    // can have sharp edges that the bounding sphere doesn't fully cover
    // This helps prevent small interpenetrations
    float safetyMargin = 0.05f; // 5cm safety margin
    hullRadius += safetyMargin;

    // Create a temporary sphere at hull position with hull radius
    RigidBody tempSphere;
    tempSphere.Position = hull.Position;
    tempSphere.Radius = hullRadius;
    tempSphere.Type = ColliderType::Sphere;

    // The swept sphere cast will test sphere vs expanded box, which is more accurate
    // than just using a larger sphere radius
    return CCD::SweptSphereCast(tempSphere, motion, box);
}

void CCD::PerformCCD(Solstice::ECS::Registry& registry, float dt) {
    // CCD implementation for fast-moving objects

    // Threshold for CCD (e.g., if moving more than min radius per frame)
    const float ccdSpeedThreshold = 5.0f;

    // Collect fast-moving dynamic bodies
    std::vector<RigidBody*> fastBodies;

    registry.ForEach<RigidBody>([&](ECS::EntityId id, RigidBody& rb) {
        if (rb.IsStatic) return;

        float speed = rb.Velocity.Magnitude();
        // Always check convex hulls and triangles for CCD (they're more prone to tunneling)
        // Also check if explicitly enabled or moving fast
        if (rb.Type == ColliderType::ConvexHull || rb.Type == ColliderType::Tetrahedron ||
            speed >= ccdSpeedThreshold || rb.EnableCCD || speed >= rb.CCDMotionThreshold) {
            fastBodies.push_back(&rb);
        }
    });

    // --- Ground Plane CCD ---
    for (auto* rb : fastBodies) {
        // Check if we are moving downwards
        if (rb->Velocity.y < 0.0f) {
            float bottomY = 0.0f;
            if (rb->Type == ColliderType::Sphere) {
                bottomY = rb->Position.y - rb->Radius;
            } else if (rb->Type == ColliderType::Box) {
                // Approximate bottom for box (using half extent y)
                // Ideally we should check rotated corners, but for simple stacking this is often enough
                // or we can take the lowest corner.
                // Let's take the lowest corner for robustness.
                Math::Vec3 corners[8];
                GetBoxCorners(rb->HalfExtents, corners);
                float minCornerY = 1e9f;
                for(int i=0; i<8; ++i) {
                    Math::Vec3 worldPos = TransformPoint(corners[i], rb->Position, rb->Rotation);
                    if(worldPos.y < minCornerY) minCornerY = worldPos.y;
                }
                bottomY = minCornerY;
            } else {
                // Default fallback for other types (Triangle, ConvexHull)
                // Use bounding radius
                bottomY = rb->Position.y - GetBoundingRadius(*rb);
            }

            // Predict next position
            float nextBottomY = bottomY + rb->Velocity.y * dt;

            // Check for tunneling through ground (y=0)
            if (bottomY >= 0.0f && nextBottomY < 0.0f) {
                // Tunneling detected!
                // Calculate Time of Impact (TOI)
                // bottomY + vel * dt * t = 0
            // t = -bottomY / (vel * dt)
            float t = -bottomY / (rb->Velocity.y * dt);
            t = std::max(0.0f, std::min(1.0f, t));

                // Move body to impact point (plus epsilon)
                Math::Vec3 motion = rb->Velocity * dt * t;
                rb->Position += motion;

                // Stop vertical movement (inelastic collision for stability)
                rb->Velocity.y = 0.0f;

                // Nudge up slightly to prevent sinking
                rb->Position.y += 0.001f;
            }
        }
    }

    // --- Dynamic-Dynamic CCD ---
    // Check fast-moving objects against all other dynamic objects
    if (fastBodies.size() < 2) return; // Need at least 2 bodies for collisions

    // Build BVH for all dynamic bodies to find potential pairs efficiently
    std::vector<RigidBody*> allDynamicBodies;
    registry.ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        if (!rb.IsStatic) {
            allDynamicBodies.push_back(&rb);
        }
    });

    if (allDynamicBodies.size() < 2) return;

    BVH bvh;
    bvh.BuildAppend(allDynamicBodies);

    // Find potential collision pairs
    std::vector<std::pair<int, int>> pairs;
    bvh.FindSelfCollisions(pairs);

    // Check each pair for tunneling
    for (const auto& pair : pairs) {
        RigidBody* A = allDynamicBodies[pair.first];
        RigidBody* B = allDynamicBodies[pair.second];

        // Check if either body is in the fast list (or is a convex hull/triangle)
        bool AIsFast = std::find(fastBodies.begin(), fastBodies.end(), A) != fastBodies.end();
        bool BIsFast = std::find(fastBodies.begin(), fastBodies.end(), B) != fastBodies.end();

        // Always check convex hulls and triangles, even if slow
        bool AIsConvex = A->Type == ColliderType::ConvexHull || A->Type == ColliderType::Tetrahedron;
        bool BIsConvex = B->Type == ColliderType::ConvexHull || B->Type == ColliderType::Tetrahedron;

        if (!AIsFast && !BIsFast && !AIsConvex && !BIsConvex) continue;

        // Compute relative motion
        Math::Vec3 relVel = A->Velocity - B->Velocity;
        Math::Vec3 motionA = A->Velocity * dt;
        Math::Vec3 motionB = B->Velocity * dt;
        Math::Vec3 relMotion = motionA - motionB;

        // Compute distance and check for overlap
        Math::Vec3 toB = B->Position - A->Position;
        float dist = toB.Magnitude();

        // Only do aggressive overlap resolution for convex hulls/triangles
        bool needsAggressiveCCD = AIsConvex || BIsConvex;

        if (needsAggressiveCCD && dist > 1e-6f) {
            float radiusA = GetBoundingRadius(*A);
            float radiusB = GetBoundingRadius(*B);
            float minDist = radiusA + radiusB;

            // If already overlapping, push apart immediately (only for convex shapes)
            if (dist < minDist) {
                Math::Vec3 separation = toB / dist;
                float overlap = minDist - dist;
                float pushAmount = overlap * 0.5f;
                if (!A->IsStatic) {
                    A->Position -= separation * pushAmount;
                }
                if (!B->IsStatic) {
                    B->Position += separation * pushAmount;
                }
                // Also reduce velocities to prevent further penetration
                A->Velocity *= 0.7f;
                B->Velocity *= 0.7f;
                // Also dampen angular velocity
                A->AngularVelocity *= 0.8f;
                B->AngularVelocity *= 0.8f;
                // Stop very small angular velocities
                if (A->AngularVelocity.Magnitude() < 0.1f) {
                    A->AngularVelocity = Math::Vec3(0, 0, 0);
                }
                if (B->AngularVelocity.Magnitude() < 0.1f) {
                    B->AngularVelocity = Math::Vec3(0, 0, 0);
                }
                continue; // Skip swept test, already handled
            }
        }

        if (dist < 1e-6f) continue; // Coincident, let regular collision handle it

        Math::Vec3 dir = toB / dist;
        if (relMotion.Dot(dir) >= 0.0f) continue; // Moving apart

        // Perform swept test based on types
        float toi = 2.0f; // Time of impact (1.0 = end of frame, >1.0 = no collision)

        // Determine which body to sweep
        // For convex hulls, always sweep the convex hull (it's more complex)
        bool sweepA = false;
        if (AIsConvex && !BIsConvex) {
            sweepA = true; // Sweep convex hull A
        } else if (BIsConvex && !AIsConvex) {
            sweepA = false; // Sweep convex hull B
        } else {
            // Both or neither are convex - sweep the faster one
            sweepA = AIsFast && (!BIsFast || motionA.Magnitude() >= motionB.Magnitude());
        }

        RigidBody* moving = sweepA ? A : B;
        RigidBody* target = sweepA ? B : A;
        Math::Vec3 motion = sweepA ? motionA : motionB;

        // Perform appropriate swept test
        if (moving->Type == ColliderType::Sphere) {
            toi = CCD::SweptSphereCast(*moving, motion, *target);
        } else if (moving->Type == ColliderType::ConvexHull && target->Type == ColliderType::Box) {
            toi = SweptConvexHullVsBox(*moving, motion, *target);
        } else if (moving->Type == ColliderType::Box && target->Type == ColliderType::Sphere) {
            // Reverse: swept sphere vs box
            RigidBody tempSphere;
            tempSphere.Position = moving->Position;
            tempSphere.Radius = GetBoundingRadius(*moving);
            toi = CCD::SweptSphereCast(tempSphere, motion, *target);
        } else if (moving->Type == ColliderType::ConvexHull && target->Type == ColliderType::Sphere) {
            // Approximate as swept sphere
            RigidBody tempSphere;
            tempSphere.Position = moving->Position;
            tempSphere.Radius = GetBoundingRadius(*moving);
            toi = CCD::SweptSphereCast(tempSphere, motion, *target);
        } else {
            // Generic: use bounding sphere approximation
            RigidBody tempSphereA, tempSphereB;
            tempSphereA.Position = moving->Position;
            tempSphereA.Radius = GetBoundingRadius(*moving);
            tempSphereB.Position = target->Position;
            tempSphereB.Radius = GetBoundingRadius(*target);
            toi = CCD::SweptSphereCast(tempSphereA, motion, tempSphereB);
        }

        // If collision detected during sweep, move to TOI
        if (toi >= 0.0f && toi <= 1.0f) {
            bool isConvexCollision = moving->Type == ColliderType::ConvexHull ||
                                    target->Type == ColliderType::ConvexHull ||
                                    moving->Type == ColliderType::Tetrahedron ||
                                    target->Type == ColliderType::Tetrahedron;

            // For convex hulls, use more conservative TOI (stop earlier)
            // For boxes/spheres, use the exact TOI
            // Increased safety margin for convex hulls to prevent interpenetration
            float safeTOI = isConvexCollision ? std::max(0.0f, toi - 0.03f) : toi;
            Math::Vec3 impactMotion = motion * safeTOI;
            moving->Position += impactMotion;

            // Compute separation vector and distance
            Math::Vec3 separation = (target->Position - moving->Position);
            float sepDist = separation.Magnitude();

            if (sepDist > 1e-6f) {
                separation = separation / sepDist;

                // Compute overlap using bounding radii
                float radiusA = GetBoundingRadius(*moving);
                float radiusB = GetBoundingRadius(*target);
                float overlap = radiusA + radiusB - sepDist;

                if (overlap > 0.0f) {
                    // More aggressive separation for convex hulls
                    // Use full overlap correction to ensure no interpenetration
                    float pushAmount = isConvexCollision ? overlap * 0.9f : overlap * 0.4f;
                    if (!moving->IsStatic) {
                        moving->Position -= separation * pushAmount;
                    }
                    if (!target->IsStatic) {
                        target->Position += separation * pushAmount;
                    }
                } else {
                    // Even if not overlapping, add a small safety gap for convex hulls
                    // This prevents them from getting too close and interpenetrating
                    if (isConvexCollision && sepDist < (radiusA + radiusB) * 0.95f) {
                        float safetyGap = (radiusA + radiusB) * 0.05f; // 5% safety gap
                        if (!moving->IsStatic) {
                            moving->Position -= separation * safetyGap;
                        }
                        if (!target->IsStatic) {
                            target->Position += separation * safetyGap;
                        }
                    }
                }
            }

            // Velocity damping: more aggressive for convex hulls, gentler for boxes
            float damping = isConvexCollision ? 0.5f : 0.9f;
            moving->Velocity *= damping;

            // Also apply damping to target if it's dynamic
            if (!target->IsStatic) {
                target->Velocity *= damping;
            }

            // CRITICAL: Apply angular velocity damping to prevent perpetual rolling
            // This is especially important for convex hulls which can get stuck rolling
            float angularDamping = isConvexCollision ? 0.5f : 0.95f;
            moving->AngularVelocity *= angularDamping;
            if (!target->IsStatic) {
                target->AngularVelocity *= angularDamping;
            }

            // If velocity is very small, also reduce angular velocity more aggressively
            // This helps objects settle instead of rotating in place
            float linearSpeed = moving->Velocity.Magnitude();
            if (linearSpeed < 0.1f && moving->AngularVelocity.Magnitude() > 0.05f) {
                // If moving slowly but still rotating, dampen rotation more
                moving->AngularVelocity *= 0.7f;
            }

            // If angular velocity is very small, stop it completely to prevent jitter
            const float angularStopThreshold = isConvexCollision ? 0.05f : 0.1f;
            if (moving->AngularVelocity.Magnitude() < angularStopThreshold) {
                moving->AngularVelocity = Math::Vec3(0, 0, 0);
            }
            if (!target->IsStatic && target->AngularVelocity.Magnitude() < angularStopThreshold) {
                target->AngularVelocity = Math::Vec3(0, 0, 0);
            }
        }
    }
}

float CCD::SweptSphereCast(const RigidBody& sphere, const Math::Vec3& motion, const RigidBody& target) {
    // Swept sphere vs static geometry
    // Returns TOI (time of impact) in [0, 1], or >1 if no collision

    // Check sphere vs sphere
    if (target.Type == ColliderType::Sphere) {
        float r = target.Radius + sphere.Radius;
        Math::Vec3 f = sphere.Position - target.Position;

        float a = motion.Dot(motion);
        if (a < 1e-6f) return 2.0f; // No motion

        float b = 2.0f * f.Dot(motion);
        float c = f.Dot(f) - r*r;

        float discriminant = b*b - 4*a*c;
        if (discriminant < 0.0f) return 2.0f;

        float sqrtDisc = std::sqrt(discriminant);
        float t0 = (-b - sqrtDisc) / (2.0f * a);
        // float t1 = (-b + sqrtDisc) / (2.0f * a);

        if (t0 >= 0.0f && t0 <= 1.0f) return t0;

        return 2.0f;
    }
    // Check sphere vs box
    else if (target.Type == ColliderType::Box) {
        // Transform ray to box local space
        Math::Vec3 relPos = sphere.Position - target.Position;
        Math::Quaternion invRot = target.Rotation.Conjugate();
        Math::Matrix4 mInv = invRot.ToMatrix(); // Rotation matrix for Local -> World (if invRot is world->local?)
        // Wait, invRot is Conjugate, so it represents World -> Local.
        // So mInv is the rotation matrix that rotates World Vector to Local Vector.

        // Apply rotation (v * M) pattern from PhysicsSystem.cxx
        Math::Vec3 localStart;
        localStart.x = mInv.M[0][0]*relPos.x + mInv.M[1][0]*relPos.y + mInv.M[2][0]*relPos.z;
        localStart.y = mInv.M[0][1]*relPos.x + mInv.M[1][1]*relPos.y + mInv.M[2][1]*relPos.z;
        localStart.z = mInv.M[0][2]*relPos.x + mInv.M[1][2]*relPos.y + mInv.M[2][2]*relPos.z;

        Math::Vec3 localDir;
        localDir.x = mInv.M[0][0]*motion.x + mInv.M[1][0]*motion.y + mInv.M[2][0]*motion.z;
        localDir.y = mInv.M[0][1]*motion.x + mInv.M[1][1]*motion.y + mInv.M[2][1]*motion.z;
        localDir.z = mInv.M[0][2]*motion.x + mInv.M[1][2]*motion.y + mInv.M[2][2]*motion.z;

        // Ray vs AABB (expanded by sphere radius)
        Math::Vec3 min = target.HalfExtents * -1.0f - Math::Vec3(sphere.Radius, sphere.Radius, sphere.Radius);
        Math::Vec3 max = target.HalfExtents + Math::Vec3(sphere.Radius, sphere.Radius, sphere.Radius);

        float tMin = 0.0f;
        float tMax = 1.0f;

        // Standard slab method
        Math::Vec3 invD(
            std::abs(localDir.x) < 1e-9f ? 1e20f : 1.0f / localDir.x,
            std::abs(localDir.y) < 1e-9f ? 1e20f : 1.0f / localDir.y,
            std::abs(localDir.z) < 1e-9f ? 1e20f : 1.0f / localDir.z
        );

        // X
        float t1 = (min.x - localStart.x) * invD.x;
        float t2 = (max.x - localStart.x) * invD.x;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return 2.0f;

        // Y
        t1 = (min.y - localStart.y) * invD.y;
        t2 = (max.y - localStart.y) * invD.y;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return 2.0f;

        // Z
        t1 = (min.z - localStart.z) * invD.z;
        t2 = (max.z - localStart.z) * invD.z;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return 2.0f;

        return tMin;
    }

    return 2.0f; // No collision
}

} // namespace Solstice::Physics
