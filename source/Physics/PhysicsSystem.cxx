#include "PhysicsSystem.hxx"
#include "RigidBody.hxx"
#include "Fluid.hxx"
#include "BVH.hxx"
#include "CollisionResolution.hxx"
#include "IterativeSolver.hxx"
#include "ContactGeneration.hxx"
#include "ContactClustering.hxx"
#include "ShapeHelpers.hxx"
#include "../Core/SIMD.hxx"
#include "../Math/SIMDVec3.hxx"
#include "../Core/Debug/Debug.hxx"
#include "../Core/Profiling/ScopeTimer.hxx"
#include "../Core/Profiling/Profiler.hxx"
#include <algorithm>
#include <cmath>
#include <future>
#include <string>

namespace Solstice::Physics {

void PhysicsSystem::Start(Solstice::ECS::Registry& registry) {
    if (m_Running && m_Registry == &registry) {
        return;
    }
    if (m_Running && m_Registry && m_Registry != &registry) {
        Stop();
    }
    m_Registry = &registry;
    m_Running = true;
    m_Bridge.Initialize(registry);
}

void PhysicsSystem::Stop() {
    if (!m_Running && !m_Registry) {
        return;
    }
    m_Running = false;
    m_Bridge.Shutdown();
    m_Registry = nullptr;
}

void PhysicsSystem::SetVelocityIterations(int iterations) {
    // Update ReactPhysics3D solver iterations
    if (m_Bridge.GetPhysicsWorld()) {
        m_Bridge.GetPhysicsWorld()->setNbIterationsVelocitySolver(static_cast<uint16_t>(iterations));
    }
    // Also update legacy solver for backward compatibility
    m_Solver.SetVelocityIterations(iterations);
}

void PhysicsSystem::SetPositionIterations(int iterations) {
    // Update ReactPhysics3D solver iterations
    if (m_Bridge.GetPhysicsWorld()) {
        m_Bridge.GetPhysicsWorld()->setNbIterationsPositionSolver(static_cast<uint32_t>(iterations));
    }
    // Also update legacy solver for backward compatibility
    m_Solver.SetPositionIterations(iterations);
}

void PhysicsSystem::UpdateAsync(float dt) {
    if (!m_Running || !m_Registry) return;

    // Submit the physics step as an async job
    Core::JobSystem::Instance().SubmitAsync([this, dt]() {
        Update(dt);
    });
}

void PhysicsSystem::IntegrateVelocity(float dt) {
    PROFILE_SCOPE("Physics.IntegrateVelocity");
    if (!m_Registry) return;

    // Pre-collect fluid simulations to avoid repeated iteration
    std::vector<FluidSimulation*> activeFluidSims;
    activeFluidSims.reserve(m_FluidSimulations.size());
    for (FluidSimulation* fluidSim : m_FluidSimulations) {
        if (fluidSim) activeFluidSims.push_back(fluidSim);
    }

    // Combined single-pass integration: velocity + fluid interactions
    // This reduces multiple ForEach iterations into one
    uint32_t activeBodies = 0;
    m_Registry->ForEach<RigidBody>([&](ECS::EntityId entityId, RigidBody& rb) {
        if (rb.IsStatic || rb.IsAsleep) return; // Skip sleeping/static bodies
        activeBodies++;

        // Accumulate forces
        for (const auto& f : rb.PendingForces) rb.Force += f;
        rb.PendingForces.clear();

        // Accumulate torques
        for (const auto& t : rb.PendingTorques) rb.Torque += t;
        rb.PendingTorques.clear();

        // Legacy Fluid component (buoyancy / damping) — apply before acceleration
        if (const Fluid* fluid = m_Registry->TryGet<Fluid>(entityId)) {
            float displacement = 1.0f;
            float buoyancyForce = fluid->Density * 9.81f * displacement;
            rb.Force.y += buoyancyForce;
            rb.Force.x -= rb.Velocity.x * fluid->Viscosity;
            rb.Force.y -= rb.Velocity.y * fluid->Viscosity;
            rb.Force.z -= rb.Velocity.z * fluid->Viscosity;
        }

        // FluidSimulation: drag and buoyancy relative to sampled fluid velocity
        for (FluidSimulation* fluidSim : activeFluidSims) {
            const Math::Vec3 fluidVel = fluidSim->SampleVelocity(rb.Position);
            const Math::Vec3 relativeVel = Solstice::Math::SubSIMD(rb.Velocity, fluidVel);
            const float dragCoeff = fluidSim->GetFluidDragCoefficient();
            const float relativeSpeed = Solstice::Math::MagnitudeSIMD(relativeVel);
            if (relativeSpeed > 1e-6f) {
                const Math::Vec3 dragForce = relativeVel * (-dragCoeff * relativeSpeed);
                rb.Force += dragForce;
            }
            float submergedVolume = 1.0f;
            const float fluidDensity = fluidSim->GetReferenceDensity();
            const Math::Vec3 buoyancyForce(0, fluidDensity * 9.81f * submergedVolume, 0);
            rb.Force += buoyancyForce;
        }

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
            vVel = vVel + (vAcc * dt);
            float out[4]; vVel.Store(out);
            rb.Velocity.x = out[0]; rb.Velocity.y = out[1]; rb.Velocity.z = out[2];
        }

        // Apply exponential damping on velocity
        if (rb.LinearDamping > 0.0f) {
            float d = std::exp(-rb.LinearDamping * dt);
            rb.Velocity *= d;
        }

        // Angular Integration (Torque -> Angular Acc -> Angular Vel)
        Math::Vec3 angularAcc;
        angularAcc.x = rb.Torque.x * rb.InverseInertiaTensor.x;
        angularAcc.y = rb.Torque.y * rb.InverseInertiaTensor.y;
        angularAcc.z = rb.Torque.z * rb.InverseInertiaTensor.z;

        rb.AngularVelocity += angularAcc * dt;

        // Angular Drag
        if (rb.AngularDrag > 0.0f) {
            float d = std::exp(-rb.AngularDrag * dt);
            rb.AngularVelocity *= d;
        }

        // Special handling for cylinders: extra damping when nearly at rest to prevent perpetual rolling
        if (rb.Type == ColliderType::Cylinder) {
            float linearSpeed = rb.Velocity.Magnitude();
            float angularSpeed = rb.AngularVelocity.Magnitude();

            // If moving slowly but still rotating, apply extra angular damping
            if (linearSpeed < 0.2f && angularSpeed > 0.05f) {
                rb.AngularVelocity *= 0.9f; // Additional damping
            }

            // If very slow, stop rotation completely to prevent jitter
            if (linearSpeed < 0.05f && angularSpeed < 0.1f) {
                rb.AngularVelocity = Math::Vec3(0, 0, 0);
            }
        }

        // Reset forces
        rb.Force = Math::Vec3{0.0f, 0.0f, 0.0f};
        rb.Torque = Math::Vec3{0.0f, 0.0f, 0.0f};
    });
    Core::Profiler::Instance().SetCounter("Physics.ActiveBodies", activeBodies);
}

void PhysicsSystem::IntegratePosition(float dt) {
    PROFILE_SCOPE("Physics.IntegratePosition");
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
        if (rb.IsStatic || rb.IsAsleep) return; // Skip sleeping/static bodies
        integratePos(rb, dt);
    });
}

void PhysicsSystem::Update(float dt) {
    if (!m_Running || !m_Registry) return;
    if (dt <= 0.0f) return;
    float stepDt = dt;
    if (m_MaxStepDt > 0.0f && stepDt > m_MaxStepDt) {
        stepDt = m_MaxStepDt;
        Core::Profiler::Instance().IncrementCounter("Physics.StepClamped");
    }

    // Start grid fluids as soon as stepDt is known (before Physics.Update scope / SyncTo) so the
    // solver runs in parallel with registry->RP3D sync and any main-thread setup in PROFILE_SCOPE.
    std::future<void> fluidFuture;
    bool fluidAsyncPending = false;
    bool fluidsSteppedSynchronously = false;
    for (FluidSimulation* fp : m_FluidSimulations) {
        if (!fp) {
            continue;
        }
        try {
            fluidFuture = Core::JobSystem::Instance().SubmitAsync([this, stepDt]() {
                UpdateFluidSimulations(stepDt);
            });
            fluidAsyncPending = true;
        } catch (...) {
            UpdateFluidSimulations(stepDt);
            fluidsSteppedSynchronously = true;
        }
        break;
    }

    PROFILE_SCOPE("Physics.Update");

    // ReactPhysics3D Integration:
    // 1. Sync RigidBody components -> ReactPhysics3D bodies (overlaps with fluid job above)
    {
        PROFILE_SCOPE("Physics.SyncTo");
        m_Bridge.SyncToReactPhysics3D();
    }

    if (fluidAsyncPending) {
        fluidFuture.wait();
    } else if (!fluidsSteppedSynchronously) {
        UpdateFluidSimulations(stepDt);
    }

    // 2. Integrate velocity (fluid drag/buoyancy sample the fluid fields stepped above)
    IntegrateVelocity(stepDt);

    // 3. Update ReactPhysics3D physics world (handles collision detection and dynamics)
    {
        PROFILE_SCOPE("Physics.BridgeUpdate");
        m_Bridge.Update(stepDt);
    }

    // 4. Sync ReactPhysics3D bodies -> RigidBody components
    {
        PROFILE_SCOPE("Physics.SyncFrom");
        m_Bridge.SyncFromReactPhysics3D();
    }

    // 5. Update sleep state (for backward compatibility with existing code)
    {
        PROFILE_SCOPE("Physics.SleepState");
        UpdateSleepState();
    }

    // Note: Custom collision resolution (ResolveCollisions) is now handled by ReactPhysics3D
    // The old code is kept for reference but is no longer called in the main update loop
}

void PhysicsSystem::UpdateBroadphase() {
    // Deprecated: Grid-based broadphase removed in favor of BVH
}

void PhysicsSystem::RegisterFluidSimulation(FluidSimulation* fluid) {
    if (!fluid) return;

    // Check if already registered
    auto it = std::find(m_FluidSimulations.begin(), m_FluidSimulations.end(), fluid);
    if (it == m_FluidSimulations.end()) {
        m_FluidSimulations.push_back(fluid);
    }
}

void PhysicsSystem::UnregisterFluidSimulation(FluidSimulation* fluid) {
    if (!fluid) return;

    auto it = std::find(m_FluidSimulations.begin(), m_FluidSimulations.end(), fluid);
    if (it != m_FluidSimulations.end()) {
        m_FluidSimulations.erase(it);
    }
}

void PhysicsSystem::UpdateSleepState() {
    if (!m_Registry) return;

    m_Registry->ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        // Early exit for static bodies - they're always asleep
        if (rb.IsStatic) {
            if (!rb.IsAsleep) {
                rb.IsAsleep = true;
                rb.SleepCounter = 0;
            }
            return;
        }

        // Check if body should sleep
        float vMag = rb.Velocity.Magnitude();
        float wMag = rb.AngularVelocity.Magnitude();

        if (vMag < RigidBody::SLEEP_VELOCITY_THRESHOLD &&
            wMag < RigidBody::SLEEP_ANGULAR_THRESHOLD) {
            rb.SleepCounter++;
            if (rb.SleepCounter >= RigidBody::SLEEP_FRAMES_REQUIRED) {
                if (!rb.IsAsleep) {
                    rb.IsAsleep = true;
                    // Zero out tiny velocities when sleeping
                    rb.Velocity = Math::Vec3(0, 0, 0);
                    rb.AngularVelocity = Math::Vec3(0, 0, 0);
                }
            }
        } else {
            // Wake up if moving
            if (rb.IsAsleep) {
                rb.SleepCounter = 0;
                rb.IsAsleep = false;
            } else {
                rb.SleepCounter = 0;
            }
        }
    });
}

void PhysicsSystem::UpdateFluidSimulations(float dt) {
    PROFILE_SCOPE("Physics.Fluids");
    std::vector<FluidSimulation*> active;
    active.reserve(m_FluidSimulations.size());
    for (FluidSimulation* fluidSim : m_FluidSimulations) {
        if (fluidSim) {
            active.push_back(fluidSim);
        }
    }
    if (active.empty()) {
        return;
    }
    if (active.size() == 1) {
        active[0]->Step(dt);
        return;
    }
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
    for (int si = 0; si < static_cast<int>(active.size()); ++si) {
        active[static_cast<size_t>(si)]->Step(dt);
    }
#else
    for (FluidSimulation* f : active) {
        f->Step(dt);
    }
#endif
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
    std::vector<CollisionResolution::ContactManifold> newManifolds;

    // 1. Plane Collisions
    for (auto* prb : bodies) {
        RigidBody& rb = *prb;
        if (rb.IsStatic) continue;

        CollisionResolution::ContactManifold manifold;
        if (ContactGeneration::GenerateGroundContacts(rb, groundN, groundD, manifold)) {
            // Apply contact clustering/reduction
            ContactClustering::ProcessManifold(manifold);
            newManifolds.push_back(manifold);
        }
    }

    // 2. Body-Body Collisions (BVH)
    std::vector<std::pair<int, int>> pairs;
    bvh.FindSelfCollisions(pairs);

    for (const auto& pair : pairs) {
        RigidBody* A = bodies[pair.first];
        RigidBody* B = bodies[pair.second];

        CollisionResolution::ContactManifold manifold;
        if (ContactGeneration::GenerateBodyContacts(A, B, manifold)) {
            // Apply contact clustering/reduction
            ContactClustering::ProcessManifold(manifold);
            newManifolds.push_back(manifold);
        }
    }

    // 3. Update persistent manifolds (Source engine style)
    m_ManifoldManager.UpdateManifolds(newManifolds);
    m_ManifoldManager.RemoveStaleManifolds();
    std::vector<CollisionResolution::ContactManifold> manifolds = m_ManifoldManager.GetActiveManifolds();

    // 4. Solve contacts with iterative solver
    m_Solver.Initialize(manifolds, 0.016f); // Fixed dt for solver
    m_Solver.WarmStart();

    // Solve Velocity - use configured iterations (default 8)
    for (int i = 0; i < m_Solver.GetVelocityIterations(); ++i) {
        m_Solver.SolveVelocity();
    }

    // Integrate Position (Interleaved)
    IntegratePosition(0.016f); // Use fixed dt matching the solver

    // Source/UE3: More position iterations for better stability, especially for resting contacts
    int positionIterations = m_Solver.GetPositionIterations();

    // Check if we have problematic shapes (tetrahedrons, cylinders) that need extra iterations
    bool hasProblematicShapes = false;
    for (const auto& manifold : manifolds) {
        if (manifold.BodyA) {
            if (manifold.BodyA->Type == ColliderType::Tetrahedron ||
                manifold.BodyA->Type == ColliderType::Cylinder) {
                hasProblematicShapes = true;
                break;
            }
        }
        if (manifold.BodyB) {
            if (manifold.BodyB->Type == ColliderType::Tetrahedron ||
                manifold.BodyB->Type == ColliderType::Cylinder) {
                hasProblematicShapes = true;
                break;
            }
        }
    }

    // UE3: Increase position iterations for problematic shapes
    if (hasProblematicShapes) {
        positionIterations = std::max(positionIterations, 4); // At least 4 iterations
    }

    // Solve Position - use increased iterations for stability
    for (int i = 0; i < positionIterations; ++i) {
        m_Solver.SolvePosition();
    }

    // Source/UE3: Additional stabilization pass for tetrahedrons to force face settling
    // This helps prevent edge balancing and floating - only apply when actually unstable
    m_Registry->ForEach<RigidBody>([&](ECS::EntityId, RigidBody& rb) {
        // Early exit conditions
        if (rb.IsStatic || rb.Type != ColliderType::Tetrahedron) return;

        // Check if tetrahedron is resting on ground
        float velMag = rb.Velocity.Magnitude();
        float angVelMag = rb.AngularVelocity.Magnitude();
        if (velMag > 0.08f || angVelMag > 0.15f) return; // Not resting

        // Early exit: position check
        if (rb.Position.y >= 0.3f || rb.Position.y <= 0.0f) return;

        // Count vertices below ground to detect edge/vertex contact
        const std::vector<Math::Vec3>* verts = nullptr;
        if (!rb.HullVertices.empty()) {
            verts = &rb.HullVertices;
        } else if (rb.Hull && !rb.Hull->Vertices.empty()) {
            verts = &rb.Hull->Vertices;
        }

        if (!verts || verts->empty()) return;

        int belowGroundCount = 0;
        const float groundThreshold = 0.05f;
        for (const auto& v : *verts) {
            Math::Vec3 worldPos = TransformPoint(v, rb.Position, rb.Rotation);
            if (worldPos.y <= groundThreshold) {
                belowGroundCount++;
                // Early exit: if 3+ vertices are below ground, it's stable (on a face)
                if (belowGroundCount >= 3) return;
            }
        }

        // If on edge (2 vertices) or vertex (1 vertex), apply VERY aggressive stabilization
        if (belowGroundCount <= 2) {
            // UE3: Apply strong stabilization to force face contact
            const float stabilizationImpulse = 0.1f; // Much stronger
            rb.Velocity.y -= stabilizationImpulse; // Downward push to tip onto face

            // Also apply angular impulse to rotate onto face
            Math::Vec3 center = rb.Position;
            Math::Vec3 down(0, -1, 0);
            Math::Vec3 torque = down.Cross(rb.Position - center) * 0.5f;
            rb.AngularVelocity += torque * 0.016f; // Apply torque
        }
    });

    // Store impulses for next frame (Warm Starting)
    m_Solver.StoreImpulses();
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

// Check if a body would collide when moved from prevPos to targetPos
bool PhysicsSystem::CheckSweptCollision(RigidBody* Body, const Math::Vec3& PrevPos, const Math::Vec3& TargetPos,
                                        Math::Vec3& OutCollisionPoint, Math::Vec3& OutCollisionNormal) {
    if (!Body || Body->IsStatic) {
        return false;
    }

    Math::Vec3 motion = TargetPos - PrevPos;
    float motionLength = motion.Magnitude();
    if (motionLength < 0.001f) {
        return false; // No movement
    }

    // Use CCD swept sphere cast for collision detection
    // Create a temporary body at previous position
    RigidBody tempBody = *Body;
    tempBody.Position = PrevPos;

    // Check against all other bodies in the registry
    PhysicsSystem& instance = PhysicsSystem::Instance();
    if (!instance.m_Registry) {
        return false;
    }

    float minTOI = 2.0f; // Time of impact (1.0 = end of motion, >1.0 = no collision)
    RigidBody* hitBody = nullptr;

    instance.m_Registry->ForEach<RigidBody>([&](ECS::EntityId id, RigidBody& other) {
        if (&other == Body || other.IsStatic) {
            return; // Skip self and static bodies (we check against static separately)
        }

        // Perform swept sphere cast
        float toi = CCD::SweptSphereCast(tempBody, motion, other);
        if (toi < minTOI && toi >= 0.0f && toi <= 1.0f) {
            minTOI = toi;
            hitBody = &other;
        }
    });

    // Check against ground plane (y=0)
    if (Body->Type == ColliderType::Sphere) {
        float bottomY = PrevPos.y - Body->Radius;
        float nextBottomY = TargetPos.y - Body->Radius;
        if (bottomY >= 0.0f && nextBottomY < 0.0f) {
            float toi = -bottomY / (motion.y);
            if (toi >= 0.0f && toi <= 1.0f && toi < minTOI) {
                minTOI = toi;
                hitBody = nullptr; // Ground collision
                OutCollisionPoint = PrevPos + motion * toi;
                OutCollisionPoint.y = Body->Radius + 0.001f; // Slightly above ground
                OutCollisionNormal = Math::Vec3(0.0f, 1.0f, 0.0f);
                return true;
            }
        }
    }

    if (minTOI < 1.0f && hitBody) {
        // Calculate collision point and normal
        OutCollisionPoint = PrevPos + motion * minTOI;
        // Calculate normal (from hit body to this body)
        Math::Vec3 toBody = (Body->Position - hitBody->Position).Normalized();
        OutCollisionNormal = toBody;
        return true;
    }

    return false;
}

} // namespace Solstice::Physics
