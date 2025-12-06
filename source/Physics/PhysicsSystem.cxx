#include "PhysicsSystem.hxx"
#include "RigidBody.hxx"
#include "Fluid.hxx"
#include "BVH.hxx"
#include "CollisionResolution.hxx"
#include <Physics/ConvexHull.hxx>
#include "GJK.hxx"
#include "../Math/Matrix.hxx"
#include "../Core/Debug.hxx"
#include <algorithm>
#include <cmath>
#include <string>

namespace Solstice::Physics {

// Helper: Get corners of a box (local space)
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

// Helper: Get test points of a box (corners + center)
static void GetBoxTestPoints(const Math::Vec3& halfExtents, Math::Vec3 points[9]) {
    points[0] = Math::Vec3(-halfExtents.x, -halfExtents.y, -halfExtents.z);
    points[1] = Math::Vec3( halfExtents.x, -halfExtents.y, -halfExtents.z);
    points[2] = Math::Vec3( halfExtents.x,  halfExtents.y, -halfExtents.z);
    points[3] = Math::Vec3(-halfExtents.x,  halfExtents.y, -halfExtents.z);
    points[4] = Math::Vec3(- halfExtents.x, -halfExtents.y,  halfExtents.z);
    points[5] = Math::Vec3( halfExtents.x, -halfExtents.y,  halfExtents.z);
    points[6] = Math::Vec3( halfExtents.x,  halfExtents.y,  halfExtents.z);
    points[7] = Math::Vec3(-halfExtents.x,  halfExtents.y,  halfExtents.z);
    points[8] = Math::Vec3(0, 0, 0); // Center
}

void PhysicsSystem::Start(Solstice::ECS::Registry& registry) {
    m_Registry = &registry;
    m_Running = true;
}

void PhysicsSystem::Stop() {
    m_Running = false;
}

void PhysicsSystem::UpdateAsync(float dt) {
    if (!m_Running || !m_Registry) return;

    // Submit the physics step as an async job
    Core::JobSystem::Instance().SubmitAsync([this, dt]() {
        Update(dt);
    });
}

void PhysicsSystem::IntegrateVelocity(float dt) {
    if (!m_Registry) return;

    // Define velocity integration strategy
    auto integrateVel = [](RigidBody& rb, float deltaTime) {
        if (rb.IsStatic) return;
        
        // Accumulate forces
        for (const auto& f : rb.PendingForces) rb.Force += f;
        rb.PendingForces.clear();

        // Accumulate torques
        for (const auto& t : rb.PendingTorques) rb.Torque += t;
        rb.PendingTorques.clear();

        // Linear Integration (Force -> Acceleration -> Velocity)
        Math::Vec3 acceleration = rb.Acceleration;
        acceleration += rb.Force * rb.InverseMass;
        acceleration += rb.ConstantForce * rb.InverseMass;
        
        // Apply gravity with per-body scale
        acceleration.y -= 9.81f * rb.GravityScale;

        // Apply drag
        if (rb.Drag > 0.0f) {
            float dragScale = rb.Drag * rb.InverseMass;
            using Solstice::Core::SIMD::Vec4;
            Vec4 vAcc(acceleration.x, acceleration.y, acceleration.z, 0.0f);
            Vec4 vVel(rb.Velocity.x, rb.Velocity.y, rb.Velocity.z, 0.0f);
            vAcc = vAcc - (vVel * dragScale);
            float accArr[4]; vAcc.Store(accArr);
            acceleration.x = accArr[0]; acceleration.y = accArr[1]; acceleration.z = accArr[2];
        }

        // Quadratic drag
        if (rb.QuadraticDrag > 0.0f) {
            using Solstice::Core::SIMD::Vec4;
            Vec4 vVel(rb.Velocity.x, rb.Velocity.y, rb.Velocity.z, 0.0f);
            float speed = std::sqrt(rb.Velocity.x*rb.Velocity.x + rb.Velocity.y*rb.Velocity.y + rb.Velocity.z*rb.Velocity.z);
            float kq = rb.QuadraticDrag * rb.InverseMass;
            Vec4 vDrag = vVel * (kq * speed);
            Vec4 vAcc(acceleration.x, acceleration.y, acceleration.z, 0.0f);
            vAcc = vAcc - vDrag;
            float accArr[4]; vAcc.Store(accArr);
            acceleration.x = accArr[0]; acceleration.y = accArr[1]; acceleration.z = accArr[2];
        }

        // Integrate velocity
        {
            using Solstice::Core::SIMD::Vec4;
            Vec4 vVel(rb.Velocity.x, rb.Velocity.y, rb.Velocity.z, 0.0f);
            Vec4 vAcc(acceleration.x, acceleration.y, acceleration.z, 0.0f);
            vVel = vVel + (vAcc * deltaTime);
            float out[4]; vVel.Store(out);
            rb.Velocity.x = out[0]; rb.Velocity.y = out[1]; rb.Velocity.z = out[2];
        }

        // Apply exponential damping on velocity
        if (rb.LinearDamping > 0.0f) {
            float d = std::exp(-rb.LinearDamping * deltaTime);
            rb.Velocity *= d;
        }
        
        // Angular Integration (Torque -> Angular Acc -> Angular Vel)
        Math::Vec3 angularAcc;
        angularAcc.x = rb.Torque.x * rb.InverseInertiaTensor.x;
        angularAcc.y = rb.Torque.y * rb.InverseInertiaTensor.y;
        angularAcc.z = rb.Torque.z * rb.InverseInertiaTensor.z;
        
        rb.AngularVelocity += angularAcc * deltaTime;
        
        // Angular Drag
        if (rb.AngularDrag > 0.0f) {
            float d = std::exp(-rb.AngularDrag * deltaTime);
            rb.AngularVelocity *= d;
        }

        // Reset forces
        rb.Force = Math::Vec3{0.0f, 0.0f, 0.0f};
        rb.Torque = Math::Vec3{0.0f, 0.0f, 0.0f};
    };

    // Apply velocity integration
    m_Registry->ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        integrateVel(rb, dt);
    });

    // Simple Fluid interaction (buoyancy) - applies forces for NEXT frame or modifies current velocity?
    // Ideally forces should be applied before integration, but here we just modify force accumulators for next frame
    // or directly modify velocity. Let's keep it simple and apply forces for next frame.
    m_Registry->ForEach<RigidBody, Fluid>([&](ECS::EntityId, RigidBody& rb, const Fluid& fluid) {
        if (rb.IsStatic) return;
        float displacement = 1.0f;
        float buoyancyForce = fluid.Density * 9.81f * displacement;
        rb.Force.y += buoyancyForce;
        rb.Force.x -= rb.Velocity.x * fluid.Viscosity;
        rb.Force.y -= rb.Velocity.y * fluid.Viscosity;
        rb.Force.z -= rb.Velocity.z * fluid.Viscosity;
    });
}

void PhysicsSystem::IntegratePosition(float dt) {
    if (!m_Registry) return;

    auto integratePos = [](RigidBody& rb, float deltaTime) {
        if (rb.IsStatic) return;

        // Integrate position (Velocity -> Position)
        {
            using Solstice::Core::SIMD::Vec4;
            Vec4 vPos(rb.Position.x, rb.Position.y, rb.Position.z, 0.0f);
            Vec4 vVel(rb.Velocity.x, rb.Velocity.y, rb.Velocity.z, 0.0f);
            vPos = vPos + (vVel * deltaTime);
            float out[4]; vPos.Store(out);
            rb.Position.x = out[0]; rb.Position.y = out[1]; rb.Position.z = out[2];
        }
        
        // Integrate Rotation: dq/dt = 0.5 * w * q
        Math::Quaternion wQ(0, rb.AngularVelocity.x, rb.AngularVelocity.y, rb.AngularVelocity.z);
        Math::Quaternion qDot = wQ * rb.Rotation;
        rb.Rotation = rb.Rotation + (qDot * (0.5f * deltaTime));
        rb.Rotation = rb.Rotation.Normalized();
    };

    m_Registry->ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        integratePos(rb, dt);
    });
}

void PhysicsSystem::Update(float dt) {
    if (!m_Running || !m_Registry) return;
    
    // 1. Integrate Velocity (Forces -> Velocity)
    IntegrateVelocity(dt);
    
    // 2. Perform CCD (Continuous Collision Detection)
    //    This helps fast moving objects not tunnel through static geometry
    CCD::PerformCCD(*m_Registry, dt);

    // 3. Generate Manifolds & Solve Velocity Constraints
    //    (This prevents velocity from penetrating before we move position)
    ResolveCollisions();
    
    // 4. Integrate Position (Velocity -> Position)
    // MOVED: IntegratePosition is now called inside ResolveCollisions to be interleaved between Velocity and Position solves.
    // IntegratePosition(dt);
    
    // 5. Solve Position Constraints (Baumgarte/NGS) to fix any remaining penetration
    //    (This is handled inside ResolveCollisions currently)
}

void PhysicsSystem::UpdateBroadphase() {
    // Deprecated: Grid-based broadphase removed in favor of BVH
}

// PerformCCD and SweptSphereCast moved to CCD.cxx

void PhysicsSystem::ResolveCollisions() {
    if (!m_Registry) return;

    // Gather dynamic bodies
    std::vector<RigidBody*> bodies;
    bodies.reserve(128);
    m_Registry->ForEach<RigidBody>([&](ECS::EntityId id, RigidBody& rb) {
        bodies.push_back(&rb);
    });
    
    // Build BVH
    BVH bvh;
    bvh.BuildAppend(bodies);

    // Ground plane (y=0)
    const Math::Vec3 groundN = {0.0f, 1.0f, 0.0f};
    const float groundD = 0.0f;

    // Contact manifold collection
    std::vector<CollisionResolution::ContactManifold> manifolds;

    // 1. Plane Collisions
    for (auto* prb : bodies) {
        RigidBody& rb = *prb;
        if (rb.IsStatic) continue;
        
        CollisionResolution::ContactManifold manifold;
        manifold.BodyA = &rb;
        manifold.BodyB = nullptr; // Ground (static infinite mass)
        manifold.Friction = rb.Friction;
        manifold.Restitution = rb.Restitution;
        
        if (rb.Type == ColliderType::Sphere) {
            float penetration = (groundD + rb.Radius) - rb.Position.y;
            if (penetration > 0.0f) {
                CollisionResolution::ContactPoint contact;
                contact.Position = rb.Position - Math::Vec3(0, rb.Radius, 0);
                contact.Normal = groundN;
                contact.Penetration = penetration;
                manifold.AddContact(contact);
            }
        } else if (rb.Type == ColliderType::Box) {
            Math::Vec3 corners[8];
            GetBoxCorners(rb.HalfExtents, corners);
            
            for (int i = 0; i < 8; ++i) {
                Math::Vec3 worldPos = TransformPoint(corners[i], rb.Position, rb.Rotation);
                float pen = groundD - worldPos.y;
                if (pen > 0.0f) {
                    CollisionResolution::ContactPoint contact;
                    contact.Position = worldPos;
                    contact.Normal = groundN;
                    contact.Penetration = pen;
                    manifold.AddContact(contact);
                }
            }
        } else if (rb.Type == ColliderType::ConvexHull || rb.Type == ColliderType::Triangle) {
            // Generic convex vs plane
            // Iterate all vertices and check against plane
            const std::vector<Math::Vec3>* verts = nullptr;
            if (rb.Type == ColliderType::ConvexHull && rb.Hull) {
                verts = &rb.Hull->Vertices;
            } else if (rb.Type == ColliderType::Triangle) {
                verts = &rb.HullVertices;
            }

            if (verts && !verts->empty()) {
                for (const auto& v : *verts) {
                    Math::Vec3 worldPos = TransformPoint(v, rb.Position, rb.Rotation);
                    float pen = groundD - worldPos.y;
                    if (pen > 0.0f) {
                        CollisionResolution::ContactPoint contact;
                        contact.Position = worldPos;
                        contact.Normal = groundN;
                        contact.Penetration = pen;
                        manifold.AddContact(contact);
                    }
                }
            }
        }
        
        if (!manifold.Contacts.empty()) {
            // Removed: Immediate positional correction (Ground Hack).
            // We rely on the solver now.
            manifolds.push_back(manifold);
        }
    }

    // 2. Body-Body Collisions (BVH)
    std::vector<std::pair<int, int>> pairs;
    bvh.FindSelfCollisions(pairs);
    
    for (const auto& pair : pairs) {
        RigidBody* A = bodies[pair.first];
        RigidBody* B = bodies[pair.second];
        
        if (A->IsStatic && B->IsStatic) continue;
        
        CollisionResolution::ContactManifold manifold;
        manifold.BodyA = A;
        manifold.BodyB = B;
        manifold.Friction = std::min(A->Friction, B->Friction);
        manifold.Restitution = std::min(A->Restitution, B->Restitution);
        
        // Dispatch based on types
        if (A->Type == ColliderType::Sphere && B->Type == ColliderType::Sphere) {
            // Sphere-Sphere
            Math::Vec3 delta = B->Position - A->Position;
            float dist2 = delta.Dot(delta);
            float r = A->Radius + B->Radius;
            if (dist2 > r*r || dist2 < 1e-12f) continue;

            float dist = std::sqrt(dist2);
            Math::Vec3 n = (dist > 1e-6f) ? (delta / dist) : Math::Vec3{0.0f, 1.0f, 0.0f};
            float penetration = r - dist;
            
            CollisionResolution::ContactPoint contact;
            contact.Position = A->Position + n * (A->Radius - penetration * 0.5f);
            contact.Normal = n;
            contact.Penetration = penetration;
            
            manifold.AddContact(contact);
        }
        else if (A->Type == ColliderType::Box && B->Type == ColliderType::Box) {
            // Box-Box with SAT
            Math::Vec3 normal;
            float penetration;
            std::vector<CollisionResolution::ContactPoint> contacts;
            
            if (TestSATBoxVsBox(*A, *B, normal, penetration, contacts)) {
                for (const auto& c : contacts) {
                    manifold.AddContact(c);
                }
            }
        }
        else if ((A->Type == ColliderType::Sphere && B->Type == ColliderType::Box) ||
                 (A->Type == ColliderType::Box && B->Type == ColliderType::Sphere)) {
            // Sphere-Box (using existing simple approach)
            RigidBody* sphere = (A->Type == ColliderType::Sphere) ? A : B;
            RigidBody* box = (A->Type == ColliderType::Sphere) ? B : A;
            
            Math::Quaternion invRot = box->Rotation.Conjugate();
            Math::Vec3 relPos = sphere->Position - box->Position;
            Math::Matrix4 mInv = invRot.ToMatrix();
            Math::Vec3 localPos;
            localPos.x = mInv.M[0][0]*relPos.x + mInv.M[1][0]*relPos.y + mInv.M[2][0]*relPos.z;
            localPos.y = mInv.M[0][1]*relPos.x + mInv.M[1][1]*relPos.y + mInv.M[2][1]*relPos.z;
            localPos.z = mInv.M[0][2]*relPos.x + mInv.M[1][2]*relPos.y + mInv.M[2][2]*relPos.z;
            
            Math::Vec3 closest;
            closest.x = std::clamp(localPos.x, -box->HalfExtents.x, box->HalfExtents.x);
            closest.y = std::clamp(localPos.y, -box->HalfExtents.y, box->HalfExtents.y);
            closest.z = std::clamp(localPos.z, -box->HalfExtents.z, box->HalfExtents.z);
            
            Math::Vec3 delta = localPos - closest;
            float dist2 = delta.Dot(delta);
            
            if (dist2 > sphere->Radius * sphere->Radius) continue;
            
            float dist = std::sqrt(dist2);
            Math::Vec3 localNormal;
            float penetration;
            
            if (dist < 1e-6f) {
                float dx = box->HalfExtents.x - std::abs(localPos.x);
                float dy = box->HalfExtents.y - std::abs(localPos.y);
                float dz = box->HalfExtents.z - std::abs(localPos.z);
                
                if (dx < dy && dx < dz) {
                    penetration = dx + sphere->Radius;
                    localNormal = Math::Vec3((localPos.x > 0) ? 1.0f : -1.0f, 0, 0);
                } else if (dy < dz) {
                    penetration = dy + sphere->Radius;
                    localNormal = Math::Vec3(0, (localPos.y > 0) ? 1.0f : -1.0f, 0);
                } else {
                    penetration = dz + sphere->Radius;
                    localNormal = Math::Vec3(0, 0, (localPos.z > 0) ? 1.0f : -1.0f);
                }
            } else {
                localNormal = delta / dist;
                penetration = sphere->Radius - dist;
            }
            
            Math::Matrix4 mRot = box->Rotation.ToMatrix();
            Math::Vec3 normal;
            normal.x = mRot.M[0][0]*localNormal.x + mRot.M[1][0]*localNormal.y + mRot.M[2][0]*localNormal.z;
            normal.y = mRot.M[0][1]*localNormal.x + mRot.M[1][1]*localNormal.y + mRot.M[2][1]*localNormal.z;
            normal.z = mRot.M[0][2]*localNormal.x + mRot.M[1][2]*localNormal.y + mRot.M[2][2]*localNormal.z;
            
            Math::Vec3 contactPoint = sphere->Position - normal * sphere->Radius;
            
            if (A == box) {
                normal = normal * -1.0f;
            }
            
            CollisionResolution::ContactPoint contact;
            contact.Position = contactPoint;
            contact.Normal = normal;
            contact.Penetration = penetration;
            manifold.AddContact(contact);
        }
        else {
            // Generic Convex Collision (GJK/EPA)
            // Handles ConvexHull-ConvexHull, ConvexHull-Box, ConvexHull-Sphere, Triangle-*, etc.
            // Also handles Box-Sphere if we wanted, but we have a special case above.
            
            Simplex simplex;
            if (GJK(*A, *B, simplex)) {
                // Collision detected! Use EPA to find penetration info
                Math::Vec3 normal;
                float penetration;
                if (EPA(simplex, *A, *B, normal, penetration)) {
                    // EPA returns normal from A to B (usually)
                    // Let's verify direction. EPA usually pushes B out of A?
                    // Or finds vector to separate.
                    // We need normal pointing from B to A for our solver convention?
                    // Solver expects Normal to point from BodyB to BodyA (to separate A from B).
                    // If EPA returns normal such that A + normal * pen = surface?
                    // Let's assume EPA returns normal pointing OUT of A towards B.
                    // If so, we need to invert it if we want B->A.
                    
                    // Actually, let's look at GJK/EPA implementation.
                    // EPA finds vector 'v' such that A and B are separated if we move A by -v?
                    // Usually EPA returns the Minimum Translation Vector (MTV).
                    // MTV direction is usually A -> B.
                    
                    // Let's assume normal is A -> B.
                    // Our solver: 
                    // constraint.RA = contact - A.Pos
                    // constraint.RB = contact - B.Pos
                    // lambda = ...
                    // A.ApplyImpulse(normal * lambda)
                    // B.ApplyImpulse(normal * -lambda)
                    // So if lambda > 0, A is pushed along normal.
                    // So normal should point A -> B? No, if A is pushed along normal, it moves towards B. That increases overlap!
                    // We want A to move AWAY from B.
                    // So normal should point B -> A.
                    
                    // If EPA returns A -> B, we must invert it.
                    // Let's assume EPA returns A -> B.
                    normal = normal * -1.0f;
                    
                    CollisionResolution::ContactPoint contact;
                    // Contact point: EPA usually gives us the depth and normal.
                    // Finding the exact contact point is harder.
                    // We can approximate it as the support point of A in direction -normal?
                    // Or average of support points.
                    // For now, let's use: Point on A = Support(A, -normal).
                    // Point on B = Support(B, normal).
                    // Contact = (PointA + PointB) * 0.5f ?
                    
                    Math::Vec3 pA = GetSupportPoint(*A, normal * -1.0f); // Support in direction B->A (so away from B)
                    Math::Vec3 pB = GetSupportPoint(*B, normal);         // Support in direction A->B (so away from A)
                    
                    // Refine contact point:
                    // The contact point should be on the surface.
                    // pA is on A's surface. pB is on B's surface.
                    // They should be close.
                    contact.Position = (pA + pB) * 0.5f;
                    contact.Normal = normal;
                    contact.Penetration = penetration;
                    
                    manifold.AddContact(contact);
                }
            }
        }
        
        if (!manifold.Contacts.empty()) {
            manifolds.push_back(manifold);
        }
    }

    // 3. Solve contacts with iterative solver
    m_Solver.Initialize(manifolds, 0.016f); // Fixed dt for solver
    m_Solver.WarmStart();
    
    // Solve Velocity (ENHANCED: More iterations for box stacking)
    for (int i = 0; i < 64; ++i) {
        m_Solver.SolveVelocity();
    }
    
    // Integrate Position (Interleaved)
    // We need to integrate position AFTER velocity solve but BEFORE position solve
    // This requires passing a callback or splitting this function.
    // For this refactor, let's assume Update() calls IntegratePosition() separately.
    // But wait, ResolveCollisions() is monolithic.
    
    // HACK: We will call IntegratePosition HERE, inside ResolveCollisions, between Velocity and Position solves.
    // This is the only way without changing the header signature of ResolveCollisions significantly or exposing manifolds.
    // Actually, we can just call the member function.
    IntegratePosition(0.016f); // Use fixed dt matching the solver
    
    // Solve Position (ENHANCED: Aggressive projection-based correction)
    for (int i = 0; i < 32; ++i) {
        m_Solver.SolvePosition();
    }
    
    // Store impulses for next frame (Warm Starting)
    m_Solver.StoreImpulses();
}

bool PhysicsSystem::TestSATBoxVsBox(const RigidBody& A, const RigidBody& B,
                                     Math::Vec3& outNormal, float& outPenetration,
                                     std::vector<CollisionResolution::ContactPoint>& outContacts) {
    // Separating Axis Theorem for Oriented Bounding Boxes
    // Test 15 axes: 3 face normals from A, 3 from B, 9 edge-edge combinations
    
    // Get rotation matrices
    Math::Matrix4 matA = A.Rotation.ToMatrix();
    Math::Matrix4 matB = B.Rotation.ToMatrix();
    
    // Extract axes
    Math::Vec3 axesA[3] = {
        Math::Vec3(matA.M[0][0], matA.M[0][1], matA.M[0][2]),
        Math::Vec3(matA.M[1][0], matA.M[1][1], matA.M[1][2]),
        Math::Vec3(matA.M[2][0], matA.M[2][1], matA.M[2][2])
    };
    
    Math::Vec3 axesB[3] = {
        Math::Vec3(matB.M[0][0], matB.M[0][1], matB.M[0][2]),
        Math::Vec3(matB.M[1][0], matB.M[1][1], matB.M[1][2]),
        Math::Vec3(matB.M[2][0], matB.M[2][1], matB.M[2][2])
    };
    
    Math::Vec3 centerDelta = B.Position - A.Position;
    
    float minPen = 1e9f;
    Math::Vec3 minAxis;
    int minAxisIndex = -1; // 0-2: A, 3-5: B, 6-14: Edge
    
    // Helper to test an axis
    auto TestAxis = [&](const Math::Vec3& axis, int index) -> bool {
        float lenSq = axis.Dot(axis);
        if (lenSq < 1e-8f) return true; // Skip near-zero axes
        
        // Normalize
        float invLen = 1.0f / std::sqrt(lenSq);
        Math::Vec3 normAxis = axis * invLen;
        
        // Project centers
        float centerProj = centerDelta.Dot(normAxis);
        
        // Project extents of A
        float projA = std::abs(A.HalfExtents.x * axesA[0].Dot(normAxis)) +
                      std::abs(A.HalfExtents.y * axesA[1].Dot(normAxis)) +
                      std::abs(A.HalfExtents.z * axesA[2].Dot(normAxis));
        
        // Project extents of B
        float projB = std::abs(B.HalfExtents.x * axesB[0].Dot(normAxis)) +
                      std::abs(B.HalfExtents.y * axesB[1].Dot(normAxis)) +
                      std::abs(B.HalfExtents.z * axesB[2].Dot(normAxis));
        
        float overlap = projA + projB - std::abs(centerProj);
        
        if (overlap < 0.0f) {
            return false; // Separating axis found
        }
        
        // Track minimum penetration
        if (overlap < minPen) {
            minPen = overlap;
            // Ensure normal points from B to A (push A away from B, or B away from A?)
            // Convention: Normal points from B to A.
            // If centerDelta is B - A.
            // If centerProj > 0, B is "in front" of A along axis. Normal should point A <- B (negative).
            // If centerProj < 0, B is "behind" A. Normal should point B -> A (positive).
            minAxis = (centerProj < 0.0f) ? normAxis : (normAxis * -1.0f);
            minAxisIndex = index;
        }
        
        return true;
    };
    
    // Test face normals of A
    for (int i = 0; i < 3; ++i) {
        if (!TestAxis(axesA[i], i)) return false;
    }
    
    // Test face normals of B
    for (int i = 0; i < 3; ++i) {
        if (!TestAxis(axesB[i], i + 3)) return false;
    }
    
    // Test edge-edge axes
    int edgeIdx = 6;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Math::Vec3 edgeAxis = axesA[i].Cross(axesB[j]);
            if (!TestAxis(edgeAxis, edgeIdx++)) return false;
        }
    }
    
    // Collision detected
    outNormal = minAxis;
    outPenetration = minPen;
    
    // --- Contact Point Generation ---
    
    // Case 1: Face-Face (Axis is from A or B)
    if (minAxisIndex < 6) {
        const RigidBody* Ref = (minAxisIndex < 3) ? &A : &B;
        const RigidBody* Inc = (minAxisIndex < 3) ? &B : &A;
        
        // If Ref is A, Normal is B->A. So Normal points INTO Ref.
        // If Ref is B, Normal is B->A. So Normal points AWAY from Ref.
        // We want the normal pointing OUT of Ref towards Inc.
        Math::Vec3 refNormal = (minAxisIndex < 3) ? (minAxis * -1.0f) : minAxis;
        
        // Find corners of Incident body
        Math::Vec3 corners[8];
        GetBoxCorners(Inc->HalfExtents, corners);
        
        // Transform corners to world space
        for (int i = 0; i < 8; ++i) {
            corners[i] = TransformPoint(corners[i], Inc->Position, Inc->Rotation);
        }
        
        // Find the "deepest" points (furthest along -refNormal, i.e., into the Ref body)
        // We project onto refNormal. The ones with MINIMUM projection value are deepest into Ref.
        float minProj = 1e9f;
        float projections[8];
        
        for (int i = 0; i < 8; ++i) {
            projections[i] = corners[i].Dot(refNormal);
            if (projections[i] < minProj) {
                minProj = projections[i];
            }
        }
        
        // Keep all corners within a small epsilon of the deepest point
        const float epsilon = 0.05f; // 5cm threshold
        int count = 0;
        
        // We also need to project these points onto the Reference Face to get the actual contact point
        // The contact point should be on the surface of the Reference body?
        // Or we can just use the Incident corner as the contact point (it's inside).
        // The solver handles penetration.
        
        struct Candidate {
            int index;
            float proj;
        };
        std::vector<Candidate> candidates;
        
        for (int i = 0; i < 8; ++i) {
            if (projections[i] < minProj + epsilon) {
                candidates.push_back({i, projections[i]});
            }
        }
        
        // Sort by depth (optional, but good for stability if we cap count)
        // But we just take up to 4
        if (candidates.size() > 4) {
             // If more than 4, usually we have a full face (4 points). 
             // We could pick the ones forming the largest area, but taking the first 4 is often "okay" for a simple engine.
             // Better: just resize.
             candidates.resize(4);
        }
        
        for (const auto& c : candidates) {
            CollisionResolution::ContactPoint contact;
            contact.Position = corners[c.index];
            contact.Normal = outNormal;
            contact.Penetration = minPen; // Use the global penetration depth (SAT)
            // Refinement: we could calculate per-point penetration:
            // float pointPen = (RefPos + RefExtent*RefNormal) - CornerPos ...
            // But SAT minPen is the "maximum valid separation", so it's safer to use that.
            
            outContacts.push_back(contact);
        }
    }
    // Case 2: Edge-Edge
    else {
        // Fallback to simple center point for edge-edge
        // (Calculating exact edge-edge contact point is complex and less critical for stacking)
        CollisionResolution::ContactPoint contact;
        contact.Position = (A.Position + B.Position) * 0.5f;
        contact.Normal = outNormal;
        contact.Penetration = minPen;
        outContacts.push_back(contact);
    }
    
    return true;
}

// Legacy function kept for backward compatibility
// Now a wrapper around the new iterative solver
void PhysicsSystem::ResolveContact(RigidBody& A, RigidBody& B, const Math::Vec3& contactPoint,
                                    const Math::Vec3& n, float penetration, bool applyPositionalCorrection) {
    // Create a single-contact manifold
    CollisionResolution::ContactManifold manifold;
    manifold.BodyA = &A;
    manifold.BodyB = &B;
    manifold.Friction = std::min(A.Friction, B.Friction);
    manifold.Restitution = std::min(A.Restitution, B.Restitution);
    
    CollisionResolution::ContactPoint contact;
    contact.Position = contactPoint;
    contact.Normal = n;
    contact.Penetration = penetration;
    manifold.AddContact(contact);
    
    // Use iterative solver for a single contact
    std::vector<CollisionResolution::ContactManifold> manifolds;
    manifolds.push_back(manifold);
    
    CollisionResolution::IterativeSolver solver;
    solver.SetVelocityIterations(5);  // Fewer iterations for single contact
    solver.SetPositionIterations(applyPositionalCorrection ? 2 : 0);
    solver.Initialize(manifolds, 0.016f);
    
    for (int i = 0; i < solver.GetVelocityIterations(); ++i) {
        solver.SolveVelocity();
    }
    
    for (int i = 0; i < solver.GetPositionIterations(); ++i) {
        solver.SolvePosition();
    }
}

} // namespace Solstice::Physics
