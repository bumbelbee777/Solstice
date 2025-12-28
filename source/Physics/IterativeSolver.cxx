#include "IterativeSolver.hxx"
#include "../Core/Async.hxx"
#include "../Core/SIMD.hxx"
#include "../Core/Debug.hxx"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <memory>
#ifdef _OPENMP
#include <omp.h>
#endif

namespace Solstice::Physics {
namespace CollisionResolution {

// Lightweight per-body lock used during parallel impulse application
using Core::Spinlock;

// Helper: acquire locks for two bodies in a stable order to avoid deadlock
static inline void LockPair(Spinlock* a, Spinlock* b) {
    if (a == b) { if (a) a->Lock(); return; }
    if (a < b) { if (a) a->Lock(); if (b) b->Lock(); }
    else { if (b) b->Lock(); if (a) a->Lock(); }
}

static inline void UnlockPair(Spinlock* a, Spinlock* b) {
    if (a == b) { if (a) a->Unlock(); return; }
    if (a < b) { if (b) b->Unlock(); if (a) a->Unlock(); }
    else { if (a) a->Unlock(); if (b) b->Unlock(); }
}

// Debug helpers
static inline bool is_finite_vec3(const Math::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static inline bool is_finite_f(float f) { return std::isfinite(f); }

IterativeSolver::IterativeSolver() {
    m_Constraints.reserve(256); // Pre-allocate for common case
}

IterativeSolver::~IterativeSolver() {
    Clear();
}

void IterativeSolver::Initialize(const std::vector<ContactManifold>& manifolds, float dt) {
    m_DeltaTime = dt;
    // Honor debug env var to force single-threaded solver
    const char* env = std::getenv("SOLSTICE_SINGLE_THREAD_SOLVER");
    if (env && std::string(env) == "1") m_ForceSingleThreaded = true;
    m_Constraints.clear();

    // Convert manifolds to constraints
    // Limit to max 4 contacts per manifold for performance
    const size_t MAX_CONTACTS_PER_MANIFOLD = 4;

    for (const auto& manifold : manifolds) {
        if (!manifold.BodyA) continue;
        if (manifold.Contacts.empty()) continue;

        // Limit contact points per manifold
        size_t numContacts = std::min(manifold.Contacts.size(), MAX_CONTACTS_PER_MANIFOLD);

        for (size_t i = 0; i < numContacts; ++i) {
            const auto& contact = manifold.Contacts[i];
            ContactConstraint constraint;
            constraint.BodyA = manifold.BodyA;
            constraint.BodyB = manifold.BodyB;
            constraint.ContactPoint = contact.Position;
            constraint.Normal = contact.Normal;
            constraint.Penetration = contact.Penetration;
            constraint.Friction = manifold.Friction;
            constraint.Restitution = manifold.Restitution;

            // Compute relative position vectors
            constraint.RA = contact.Position - manifold.BodyA->Position;
            if (manifold.BodyB) constraint.RB = contact.Position - manifold.BodyB->Position;
            else constraint.RB = Math::Vec3(0,0,0);

            // Compute tangent basis
            ComputeTangentBasis(constraint.Normal, constraint.Tangent1, constraint.Tangent2);

            // Compute effective masses
            if (manifold.BodyB) {
                constraint.NormalMass = ComputeEffectiveMass(*manifold.BodyA, *manifold.BodyB,
                                                              constraint.RA, constraint.RB,
                                                              constraint.Normal);
                constraint.TangentMass1 = ComputeEffectiveMass(*manifold.BodyA, *manifold.BodyB,
                                                              constraint.RA, constraint.RB,
                                                              constraint.Tangent1);
                constraint.TangentMass2 = ComputeEffectiveMass(*manifold.BodyA, *manifold.BodyB,
                                                              constraint.RA, constraint.RB,
                                                              constraint.Tangent2);
            } else {
                // Static/inf mass bodyB
                float invMassA = manifold.BodyA->InverseMass;
                Math::Vec3 rnA = constraint.RA.Cross(constraint.Normal);
                float angA = rnA.Dot(ApplyWorldInertia(*manifold.BodyA, rnA));
                float denomN = invMassA + angA;
                constraint.NormalMass = denomN > 0.0f ? 1.0f / denomN : 0.0f;

                Math::Vec3 rnT1 = constraint.RA.Cross(constraint.Tangent1);
                float angT1 = rnT1.Dot(ApplyWorldInertia(*manifold.BodyA, rnT1));
                float denomT1 = invMassA + angT1;
                constraint.TangentMass1 = denomT1 > 0.0f ? 1.0f / denomT1 : 0.0f;

                Math::Vec3 rnT2 = constraint.RA.Cross(constraint.Tangent2);
                float angT2 = rnT2.Dot(ApplyWorldInertia(*manifold.BodyA, rnT2));
                float denomT2 = invMassA + angT2;
                constraint.TangentMass2 = denomT2 > 0.0f ? 1.0f / denomT2 : 0.0f;
            }

            // Compute velocity bias for restitution and Source-style resting contact bias
            Math::Vec3 vA = manifold.BodyA->Velocity + manifold.BodyA->AngularVelocity.Cross(constraint.RA);
            Math::Vec3 vB(0.0f, 0.0f, 0.0f);
            if (manifold.BodyB) vB = manifold.BodyB->Velocity + manifold.BodyB->AngularVelocity.Cross(constraint.RB);
            Math::Vec3 relVel = vA - vB;
            float vn = relVel.Dot(constraint.Normal);

            // Source engine resting contact detection
            float velMagA = manifold.BodyA->Velocity.Magnitude();
            float velMagB = (manifold.BodyB) ? manifold.BodyB->Velocity.Magnitude() : 0.0f;
            float angVelMagA = manifold.BodyA->AngularVelocity.Magnitude();
            float angVelMagB = (manifold.BodyB) ? manifold.BodyB->AngularVelocity.Magnitude() : 0.0f;

            const float RESTING_VEL_THRESHOLD = 0.1f; // m/s (Source engine value)
            const float RESTING_ANG_THRESHOLD = 0.2f; // rad/s (Source engine value)
            bool isRestingContact = (velMagA < RESTING_VEL_THRESHOLD && velMagB < RESTING_VEL_THRESHOLD &&
                                     angVelMagA < RESTING_ANG_THRESHOLD && angVelMagB < RESTING_ANG_THRESHOLD &&
                                     std::abs(vn) < RESTING_VEL_THRESHOLD);

            // Apply restitution only if objects are separating fast enough
            const float restitutionThreshold = 1.0f; // m/s
            if (vn < -restitutionThreshold) {
                constraint.VelocityBias = -constraint.Restitution * vn;
            } else {
                constraint.VelocityBias = 0.0f;
            }

            // Source-style Baumgarte stabilization bias
            const float baumgarteSlop = 0.01f; // Source engine slop
            const float baumgarteCoef = 0.2f; // Source engine coefficient
            const float maxBaumgarteBias = 10.0f; // Clamp to prevent huge corrections
            float pen = std::max(0.0f, constraint.Penetration - baumgarteSlop);
            float baumgarteBias = baumgarteCoef * pen / dt;

            // Source/UE3 engine style: Add velocity bias for resting contacts to prevent sinking
            if (isRestingContact) {
                // For resting contacts, add a stronger upward bias to prevent sinking
                // This is Source/UE3 engine's approach to handle resting contact stability
                const float restingBiasScale = 0.2f; // Increased bias to prevent sinking
                float restingBias = pen * restingBiasScale / dt;
                baumgarteBias += restingBias;

                // UE3: Additional bias for ground contacts to prevent sinking
                bool isGroundContact = (manifold.BodyB == nullptr);
                if (isGroundContact && constraint.Normal.y > 0.5f) { // Ground normal
                    float antiSinkBias = pen * 0.15f / dt; // Additional push up
                    baumgarteBias += antiSinkBias;
                }

                // Clamp resting contact bias more aggressively
                baumgarteBias = std::min(baumgarteBias, maxBaumgarteBias * 0.15f); // Slightly more allowed
            } else {
                baumgarteBias = std::min(baumgarteBias, maxBaumgarteBias);
            }

            constraint.VelocityBias += baumgarteBias;

            // Compute ID for Warm Starting
            auto rotate = [](const Math::Vec3& v, const Math::Quaternion& q) {
                Math::Vec3 u(q.x, q.y, q.z);
                float s = q.w;
                Math::Vec3 t = u.Cross(v) * 2.0f;
                return v + (t * s) + u.Cross(t);
            };

            Math::Vec3 localPos = rotate(contact.Position - manifold.BodyA->Position, manifold.BodyA->Rotation.Conjugate());
            size_t hx = (size_t)(localPos.x * 100.0f);
            size_t hy = (size_t)(localPos.y * 100.0f);
            size_t hz = (size_t)(localPos.z * 100.0f);
            size_t featureHash = hx ^ (hy << 1) ^ (hz << 2);

            constraint.ID = { manifold.BodyA, manifold.BodyB, featureHash };

            m_Constraints.push_back(constraint);
        }
    }
}

void IterativeSolver::WarmStart() {
    // Apply cached impulses from previous frame
    std::unordered_map<RigidBody*, std::unique_ptr<Spinlock>> lockMap;
    lockMap.reserve(m_Constraints.size()*2);
    for (auto& c : m_Constraints) {
        if (c.BodyA && lockMap.find(c.BodyA) == lockMap.end()) lockMap.emplace(c.BodyA, std::make_unique<Spinlock>());
        if (c.BodyB && lockMap.find(c.BodyB) == lockMap.end()) lockMap.emplace(c.BodyB, std::make_unique<Spinlock>());
    }

    auto applyFn = [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto& c = m_Constraints[i];
            if (!c.BodyA) continue;

            // Look up cached impulse from previous frame
            auto it = m_ImpulseCache.find(c.ID);
            if (it != m_ImpulseCache.end()) {
                c.NormalImpulse = it->second.NormalImpulse;
                c.TangentImpulse1 = it->second.TangentImpulse1;
                c.TangentImpulse2 = it->second.TangentImpulse2;
                it->second.LifeTime = 0;
            }

            // Compute total impulse
            Math::Vec3 impulse = c.Normal * c.NormalImpulse +
                                c.Tangent1 * c.TangentImpulse1 +
                                c.Tangent2 * c.TangentImpulse2;

            // Lock both bodies before applying
            Spinlock* aLock = nullptr;
            Spinlock* bLock = nullptr;
            auto itA = lockMap.find(c.BodyA);
            if (itA != lockMap.end()) aLock = itA->second.get();
            if (c.BodyB) {
                auto itB = lockMap.find(c.BodyB);
                if (itB != lockMap.end()) bLock = itB->second.get();
            }

            LockPair(aLock, bLock);

            if (c.BodyA) {
                c.BodyA->ApplyImpulse(impulse);
                c.BodyA->ApplyAngularImpulse(c.RA.Cross(impulse));
            }

            if (c.BodyB) {
                c.BodyB->ApplyImpulse(impulse * -1.0f);
                c.BodyB->ApplyAngularImpulse(c.RB.Cross(impulse * -1.0f));
            }

            UnlockPair(aLock, bLock);
        }
    };

    const size_t N = m_Constraints.size();
    if (N == 0) return;
    if (m_ForceSingleThreaded) {
        applyFn(0, N);
        return;
    }

#ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)N; ++i) {
        applyFn(i, i+1);
    }
#else
    const size_t numTasks = std::min<size_t>(4, (N + 63) / 64);
    std::vector<std::future<void>> futures;
    size_t stride = (N + numTasks - 1) / numTasks;
    for (size_t t = 0; t < numTasks; ++t) {
        size_t b = t * stride;
        size_t e = std::min(N, b + stride);
        futures.push_back(Core::JobSystem::Instance().SubmitAsync([b,e, &applyFn]() { applyFn(b,e); }));
    }
    for (auto& f : futures) f.get();
#endif
}

void IterativeSolver::SolveVelocity() {
    // Solve normal and tangent impulses
    std::unordered_map<RigidBody*, std::unique_ptr<Spinlock>> lockMap;
    lockMap.reserve(m_Constraints.size()*2);
    for (auto& c : m_Constraints) {
        if (c.BodyA && lockMap.find(c.BodyA) == lockMap.end()) lockMap.emplace(c.BodyA, std::make_unique<Spinlock>());
        if (c.BodyB && lockMap.find(c.BodyB) == lockMap.end()) lockMap.emplace(c.BodyB, std::make_unique<Spinlock>());
    }

    auto solveRange = [&](size_t begin, size_t end) {
        for (size_t i = begin; i < end; ++i) {
            auto& c = m_Constraints[i];
            RigidBody* bodyA = c.BodyA;
            RigidBody* bodyB = c.BodyB;
            if (!bodyA) continue;

            // --- Normal Impulse ---
            Math::Vec3 vA = bodyA->Velocity + bodyA->AngularVelocity.Cross(c.RA);
            Math::Vec3 vB(0.0f, 0.0f, 0.0f);
            if (bodyB) vB = bodyB->Velocity + bodyB->AngularVelocity.Cross(c.RB);
            Math::Vec3 dv = vA - vB;

            using Core::SIMD::Vec4;
            Vec4 a4(dv.x, dv.y, dv.z, 0.0f);
            Vec4 b4(c.Normal.x, c.Normal.y, c.Normal.z, 0.0f);
            float vn = a4.Dot(b4);

            float lambda = c.NormalMass * (-(vn - c.VelocityBias));

            float oldImpulse = c.NormalImpulse;
            float newImpulse = oldImpulse + lambda;
            if (newImpulse < 0.0f) newImpulse = 0.0f;
            c.NormalImpulse = newImpulse;
            lambda = c.NormalImpulse - oldImpulse;

            Math::Vec3 impulse = c.Normal * lambda;

            // Lock and apply
            Spinlock* aLock = nullptr;
            Spinlock* bLock = nullptr;
            auto itA = lockMap.find(bodyA);
            if (itA != lockMap.end()) aLock = itA->second.get();
            if (bodyB) {
                auto itB = lockMap.find(bodyB);
                if (itB != lockMap.end()) bLock = itB->second.get();
            }
            LockPair(aLock, bLock);

            if (bodyA) {
                bodyA->ApplyImpulse(impulse);
                bodyA->ApplyAngularImpulse(c.RA.Cross(impulse));
            }
            if (bodyB) {
                bodyB->ApplyImpulse(impulse * -1.0f);
                bodyB->ApplyAngularImpulse(c.RB.Cross(impulse * -1.0f));
            }

            // --- Friction Impulses with Static Friction ---
            if (c.Friction > 0.0f && c.NormalImpulse > 0.0f) {
                // Recompute relative velocity after normal impulse
                vA = bodyA->Velocity + bodyA->AngularVelocity.Cross(c.RA);
                vB = (bodyB) ? (bodyB->Velocity + bodyB->AngularVelocity.Cross(c.RB)) : Math::Vec3(0.0f,0.0f,0.0f);
                dv = vA - vB;

                float vn_friction = dv.Dot(c.Normal);
                Math::Vec3 vTangent = dv - c.Normal * vn_friction;
                float vtMag = vTangent.Magnitude();

                // Static friction threshold
                const float staticFrictionThreshold = 0.01f; // m/s
                if (vtMag < staticFrictionThreshold) {
                    // Static friction: zero out tangential velocity
                    float staticFriction = c.Friction * 1.2f;
                    float maxStaticFriction = staticFriction * c.NormalImpulse;

                    // Cancel tangential velocity
                    float lambdaT1 = c.TangentMass1 * (-vTangent.Dot(c.Tangent1));
                    float lambdaT2 = c.TangentMass2 * (-vTangent.Dot(c.Tangent2));

                    float oldT1 = c.TangentImpulse1;
                    float newT1 = oldT1 + lambdaT1;
                    if (newT1 > maxStaticFriction) newT1 = maxStaticFriction;
                    if (newT1 < -maxStaticFriction) newT1 = -maxStaticFriction;
                    c.TangentImpulse1 = newT1;
                    lambdaT1 = c.TangentImpulse1 - oldT1;

                    float oldT2 = c.TangentImpulse2;
                    float newT2 = oldT2 + lambdaT2;
                    if (newT2 > maxStaticFriction) newT2 = maxStaticFriction;
                    if (newT2 < -maxStaticFriction) newT2 = -maxStaticFriction;
                    c.TangentImpulse2 = newT2;
                    lambdaT2 = c.TangentImpulse2 - oldT2;

                    Math::Vec3 impT = c.Tangent1 * lambdaT1 + c.Tangent2 * lambdaT2;
                    bodyA->ApplyImpulse(impT);
                    bodyA->ApplyAngularImpulse(c.RA.Cross(impT));
                    if (bodyB) {
                        bodyB->ApplyImpulse(impT * -1.0f);
                        bodyB->ApplyAngularImpulse(c.RB.Cross(impT * -1.0f));
                    }
                } else {
                    // Dynamic friction
                    float vt1 = dv.Dot(c.Tangent1);
                    float lambdaT = c.TangentMass1 * (-vt1);
                    float maxFriction = c.Friction * c.NormalImpulse;
                    float oldT = c.TangentImpulse1;
                    float newT = oldT + lambdaT;
                    if (newT > maxFriction) newT = maxFriction;
                    if (newT < -maxFriction) newT = -maxFriction;
                    c.TangentImpulse1 = newT;
                    lambdaT = c.TangentImpulse1 - oldT;
                    Math::Vec3 impT = c.Tangent1 * lambdaT;
                    bodyA->ApplyImpulse(impT);
                    bodyA->ApplyAngularImpulse(c.RA.Cross(impT));
                    if (bodyB) {
                        bodyB->ApplyImpulse(impT * -1.0f);
                        bodyB->ApplyAngularImpulse(c.RB.Cross(impT * -1.0f));
                    }

                    // Tangent 2
                    vA = bodyA->Velocity + bodyA->AngularVelocity.Cross(c.RA);
                    vB = (bodyB) ? (bodyB->Velocity + bodyB->AngularVelocity.Cross(c.RB)) : Math::Vec3(0.0f,0.0f,0.0f);
                    dv = vA - vB;
                    float vt2 = dv.Dot(c.Tangent2);
                    lambdaT = c.TangentMass2 * (-vt2);
                    oldT = c.TangentImpulse2;
                    newT = oldT + lambdaT;
                    if (newT > maxFriction) newT = maxFriction;
                    if (newT < -maxFriction) newT = -maxFriction;
                    c.TangentImpulse2 = newT;
                    lambdaT = c.TangentImpulse2 - oldT;
                    impT = c.Tangent2 * lambdaT;
                    bodyA->ApplyImpulse(impT);
                    bodyA->ApplyAngularImpulse(c.RA.Cross(impT));
                    if (bodyB) {
                        bodyB->ApplyImpulse(impT * -1.0f);
                        bodyB->ApplyAngularImpulse(c.RB.Cross(impT * -1.0f));
                    }
                }
            }

            UnlockPair(aLock, bLock);
        }
    };

    // Single iteration per call (caller handles multiple iterations)
    const size_t N = m_Constraints.size();
    if (N == 0) return;
    if (m_ForceSingleThreaded) {
        solveRange(0, N);
        return;
    }

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < (int)N; ++i) {
        solveRange(i, i+1);
    }
#else
    const size_t numTasks = std::min<size_t>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 2, (N + 63) / 64);
    std::vector<std::future<void>> futures;
    size_t stride = (N + numTasks - 1) / numTasks;
    for (size_t t = 0; t < numTasks; ++t) {
        size_t b = t * stride;
        size_t e = std::min(N, b + stride);
        futures.push_back(Core::JobSystem::Instance().SubmitAsync([b,e, &solveRange]() { solveRange(b,e); }));
    }
    for (auto& f : futures) f.get();
#endif
}

void IterativeSolver::SolvePosition() {
    // Source/UE3 engine-style Non-linear Gauss-Seidel position correction
    // Enhanced with more aggressive correction for problematic shapes
    const float SOURCE_SLOP = 0.01f; // Source engine slop value
    const float SOURCE_CORRECTION_FACTOR = 0.3f; // Increased for more aggressive correction
    const float MAX_CORRECTION = 0.3f; // Increased maximum correction
    const float RESTING_MAX_CORRECTION = 0.08f; // Increased for better resting contact handling
    const float RESTING_SLOP = 0.005f; // More conservative slop for resting contacts

    for (int iter = 0; iter < m_PositionIterations; ++iter) {
        for (auto& c : m_Constraints) {
            RigidBody* bodyA = c.BodyA;
            RigidBody* bodyB = c.BodyB;

            if (!bodyA) continue;
            if (bodyA->IsStatic && (bodyB == nullptr || bodyB->IsStatic)) continue;

            // Recompute penetration (update RA/RB relative to current positions)
            Math::Vec3 worldPointA = bodyA->Position + c.RA;
            Math::Vec3 worldPointB = (bodyB) ? (bodyB->Position + c.RB) : Math::Vec3(0.0f, 0.0f, 0.0f);
            Math::Vec3 delta = worldPointA - worldPointB;
            float penetration = -delta.Dot(c.Normal);

            // Don't apply correction if separated
            if (penetration <= 0.0f) continue;

            // Detect resting contacts (Source engine style)
            float velMagA = bodyA->Velocity.Magnitude();
            float velMagB = (bodyB) ? bodyB->Velocity.Magnitude() : 0.0f;
            float angVelMagA = bodyA->AngularVelocity.Magnitude();
            float angVelMagB = (bodyB) ? bodyB->AngularVelocity.Magnitude() : 0.0f;

            const float RESTING_VEL_THRESHOLD = 0.08f; // Tighter threshold
            const float RESTING_ANG_THRESHOLD = 0.15f; // Tighter threshold
            bool isResting = (velMagA < RESTING_VEL_THRESHOLD && velMagB < RESTING_VEL_THRESHOLD &&
                             angVelMagA < RESTING_ANG_THRESHOLD && angVelMagB < RESTING_ANG_THRESHOLD);

            // Source/UE3: Use more conservative slop for resting contacts to prevent sinking
            float slop = isResting ? RESTING_SLOP : SOURCE_SLOP;
            float corrected = std::max(0.0f, penetration - slop);
            if (corrected <= 0.0f) continue;

            float invMassSum = bodyA->InverseMass + (bodyB ? bodyB->InverseMass : 0.0f);
            if (invMassSum <= 0.0f) continue;

            // Source/UE3-style correction with shape-specific enhancements
            float correctionFactor = SOURCE_CORRECTION_FACTOR;

            // More aggressive correction for problematic shapes
            bool isTetrahedron = (bodyA->Type == ColliderType::Tetrahedron) ||
                                (bodyB && bodyB->Type == ColliderType::Tetrahedron);
            bool isCylinder = (bodyA->Type == ColliderType::Cylinder) ||
                             (bodyB && bodyB->Type == ColliderType::Cylinder);
            bool isBox = (bodyA->Type == ColliderType::Box) ||
                        (bodyB && bodyB->Type == ColliderType::Box);
            bool isGroundContact = (bodyB == nullptr);

            // UE3/Source: Apply stronger correction for tetrahedrons and cylinders on ground
            if (isResting && isGroundContact) {
                if (isTetrahedron || isCylinder) {
                    correctionFactor = 0.4f; // More aggressive for these shapes
                }
            }

            // Prevent cube sinking: use less aggressive correction for box-ground contacts
            if (isResting && isGroundContact && isBox && !isTetrahedron && !isCylinder) {
                correctionFactor = 0.15f; // Less aggressive to prevent sinking
            }

            float correctionMag = corrected * correctionFactor;
            correctionMag = std::min(correctionMag, MAX_CORRECTION);

            // Source/UE3: More aggressive correction for resting contacts (but clamped)
            if (isResting) {
                // Allow slightly more correction for resting to prevent sinking
                correctionMag = std::min(correctionMag, RESTING_MAX_CORRECTION);

                // For ground contacts, apply additional "push up" bias to prevent sinking
                if (isGroundContact && c.Normal.y > 0.5f) { // Ground normal is up
                    float sinkBias = std::max(0.0f, penetration - slop) * 0.15f; // Additional push
                    correctionMag = std::max(correctionMag, sinkBias);
                    correctionMag = std::min(correctionMag, RESTING_MAX_CORRECTION * 1.5f); // Allow more for anti-sink
                }
            }

            // Clamp penetration depth to prevent excessive corrections (UE3 style)
            const float MAX_PENETRATION = 0.5f; // Maximum allowed penetration
            if (penetration > MAX_PENETRATION) {
                correctionMag = std::min(correctionMag, MAX_PENETRATION * correctionFactor);
            }

            // Apply correction with proper mass distribution (Source engine style)
            Math::Vec3 correctionVec = c.Normal * correctionMag;

            // Apply correction proportionally to inverse mass
            if (!bodyA->IsStatic) {
                float correctionA = correctionVec.Magnitude() * bodyA->InverseMass / invMassSum;
                bodyA->Position += c.Normal * correctionA;
            }
            if (bodyB && !bodyB->IsStatic) {
                float correctionB = correctionVec.Magnitude() * bodyB->InverseMass / invMassSum;
                bodyB->Position -= c.Normal * correctionB;
            }
        }
    }
}

void IterativeSolver::Clear() {
    m_Constraints.clear();
}

float IterativeSolver::ComputeEffectiveMass(const RigidBody& bodyA, const RigidBody& bodyB,
                                            const Math::Vec3& rA, const Math::Vec3& rB,
                                            const Math::Vec3& direction) {
    float invMassSum = bodyA.InverseMass;
    float angularA = 0.0f;
    float angularB = 0.0f;

    Math::Vec3 rnA = rA.Cross(direction);
    Math::Vec3 i_rnA = ApplyWorldInertia(bodyA, rnA);
    angularA = rnA.Dot(i_rnA);

    invMassSum += bodyB.InverseMass;
    if (bodyB.InverseMass > 0.0f) {
        Math::Vec3 rnB = rB.Cross(direction);
        Math::Vec3 i_rnB = ApplyWorldInertia(bodyB, rnB);
        angularB = rnB.Dot(i_rnB);
    }

    float denom = invMassSum + angularA + angularB;
    return (denom > 0.0f) ? (1.0f / denom) : 0.0f;
}

Math::Vec3 IterativeSolver::ApplyWorldInertia(const RigidBody& rb, const Math::Vec3& v) {
    Math::Quaternion qInv = rb.Rotation.Conjugate();
    Math::Quaternion vQ(0, v.x, v.y, v.z);
    Math::Quaternion vLocalQ = qInv * vQ * rb.Rotation;
    Math::Vec3 vLocal(vLocalQ.x, vLocalQ.y, vLocalQ.z);

    Math::Vec3 wLocal;
    wLocal.x = vLocal.x * rb.InverseInertiaTensor.x;
    wLocal.y = vLocal.y * rb.InverseInertiaTensor.y;
    wLocal.z = vLocal.z * rb.InverseInertiaTensor.z;

    Math::Quaternion wLocalQ(0, wLocal.x, wLocal.y, wLocal.z);
    Math::Quaternion wWorldQ = rb.Rotation * wLocalQ * qInv;

    return Math::Vec3(wWorldQ.x, wWorldQ.y, wWorldQ.z);
}

void IterativeSolver::ComputeTangentBasis(const Math::Vec3& normal, Math::Vec3& tangent1, Math::Vec3& tangent2) {
    Math::Vec3 ref(1, 0, 0);
    if (std::abs(normal.x) > 0.9f) {
        ref = Math::Vec3(0, 1, 0);
    }

    tangent1 = ref - (normal * ref.Dot(normal));
    tangent1 = tangent1.Normalized();

    tangent2 = normal.Cross(tangent1);
}

void IterativeSolver::StoreImpulses() {
    for (const auto& c : m_Constraints) {
        if (c.NormalImpulse > 0.0f || std::abs(c.TangentImpulse1) > 0.0f || std::abs(c.TangentImpulse2) > 0.0f) {
            CachedImpulse& cached = m_ImpulseCache[c.ID];
            cached.NormalImpulse = c.NormalImpulse;
            cached.TangentImpulse1 = c.TangentImpulse1;
            cached.TangentImpulse2 = c.TangentImpulse2;
            cached.LifeTime = 0;
        }
    }
}

} // namespace CollisionResolution
} // namespace Solstice::Physics

