#include "CollisionResolution.hxx"
#include "../Core/Async.hxx"
#include "../Core/SIMD.hxx"
#include "../Core/Debug.hxx"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include <random>
#include <functional>
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
    for (const auto& manifold : manifolds) {
        if (!manifold.BodyA) continue;
        if (manifold.Contacts.empty()) continue;
        
        for (const auto& contact : manifold.Contacts) {
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
            // Compute effective masses; allow BodyB == nullptr (infinite mass/static)
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
                // Static/inf mass bodyB: compute effective mass with only bodyA contributing
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
            
            // Compute velocity bias for restitution
            // Relative velocity at contact point
            Math::Vec3 vA = manifold.BodyA->Velocity + manifold.BodyA->AngularVelocity.Cross(constraint.RA);
            Math::Vec3 vB(0.0f, 0.0f, 0.0f);
            if (manifold.BodyB) vB = manifold.BodyB->Velocity + manifold.BodyB->AngularVelocity.Cross(constraint.RB);
            Math::Vec3 relVel = vA - vB;
            float vn = relVel.Dot(constraint.Normal);
            
            // Apply restitution only if objects are separating fast enough
            const float restitutionThreshold = 1.0f; // m/s
            if (vn < -restitutionThreshold) {
                constraint.VelocityBias = -constraint.Restitution * vn;
            } else {
                constraint.VelocityBias = 0.0f;
            }
            
            // Add Baumgarte stabilization bias
            // Hybrid approach: Use a small amount of velocity-bias Baumgarte to help with stacking stability,
            // while relying on Post-Stabilization for the bulk of the correction.
            const float baumgarteSlop = 0.005f; // Tighter slop for better stacking
            const float baumgarteCoef = 0.05f;  // Reduced coefficient to prevent floating
            float pen = std::max(0.0f, constraint.Penetration - baumgarteSlop);
            const float baumgarteBias = baumgarteCoef * pen / dt;
            constraint.VelocityBias += baumgarteBias;
            
            // Compute ID for Warm Starting
            // Transform contact point to BodyA's local space for stable hashing
            auto rotate = [](const Math::Vec3& v, const Math::Quaternion& q) {
                Math::Vec3 u(q.x, q.y, q.z);
                float s = q.w;
                Math::Vec3 t = u.Cross(v) * 2.0f;
                return v + (t * s) + u.Cross(t);
            };
            
            Math::Vec3 localPos = rotate(contact.Position - manifold.BodyA->Position, manifold.BodyA->Rotation.Conjugate());
            
            // Simple spatial hash (1cm buckets)
            // Use raw bits to avoid float precision issues in hash
            // But for now, simple casting is fine for a prototype
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
    // Build a per-body spinlock table
    std::unordered_map<RigidBody*, std::unique_ptr<Spinlock>> lockMap;
    lockMap.reserve(m_Constraints.size()*2);
    for (auto& c : m_Constraints) {
        if (c.BodyA && lockMap.find(c.BodyA) == lockMap.end()) lockMap.emplace(c.BodyA, std::make_unique<Spinlock>());
        if (c.BodyB && lockMap.find(c.BodyB) == lockMap.end()) lockMap.emplace(c.BodyB, std::make_unique<Spinlock>());
    }

    // Parallel apply using JobSystem; fallback to simple loop if JobSystem not initialized
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
                it->second.LifeTime = 0; // Reset lifetime
            }

            // Compute total impulse
            Math::Vec3 impulse = c.Normal * c.NormalImpulse +
                                c.Tangent1 * c.TangentImpulse1 +
                                c.Tangent2 * c.TangentImpulse2;

            // Lock both bodies before applying (lookup safely)
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
    // If forced single-threaded, run sequentially for easier debugging
    if (m_ForceSingleThreaded) {
        applyFn(0, N);
        return;
    }

    // Use OpenMP if available for simple parallel loop
#ifdef _OPENMP
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)N; ++i) {
        applyFn(i, i+1);
    }
#else
    // Try JobSystem parallelization in coarse grains
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
    // Prepare per-body locks (same as WarmStart)
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

            // Use SIMD Vec4 for dot (x,y,z,0)
            using Core::SIMD::Vec4;
            Vec4 a4(dv.x, dv.y, dv.z, 0.0f);
            Vec4 b4(c.Normal.x, c.Normal.y, c.Normal.z, 0.0f);
            float vn = a4.Dot(b4);

            float lambda = c.NormalMass * (-(vn - c.VelocityBias));

            float oldImpulse = c.NormalImpulse;
            // clamp to >= 0
            float newImpulse = oldImpulse + lambda;
            if (newImpulse < 0.0f) newImpulse = 0.0f;
            c.NormalImpulse = newImpulse;
            lambda = c.NormalImpulse - oldImpulse;

            Math::Vec3 impulse = c.Normal * lambda;

            // Debug: sanity checks
            if (!is_finite_vec3(impulse) || !is_finite_f(lambda)) {
                SIMPLE_LOG("[Solver] NaN detected in normal impulse");
            }
            if (std::abs(lambda) > 1e6f) {
                SIMPLE_LOG("[Solver] Huge lambda in normal: " + std::to_string(lambda));
            }

            // Lock and apply (lookup safely; bodyB may be nullptr)
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

            // --- Friction Impulses ---
            if (c.Friction > 0.0f) {
                // Tangent 1
                vA = bodyA->Velocity + bodyA->AngularVelocity.Cross(c.RA);
                vB = (bodyB) ? (bodyB->Velocity + bodyB->AngularVelocity.Cross(c.RB)) : Math::Vec3(0.0f,0.0f,0.0f);
                dv = vA - vB;
                float vt = dv.Dot(c.Tangent1);
                float lambdaT = c.TangentMass1 * (-vt);
                if (!is_finite_f(lambdaT) || !is_finite_f(vt)) SIMPLE_LOG("[Solver] NaN in tangent1 lambda/vt");
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
                vt = dv.Dot(c.Tangent2);
                lambdaT = c.TangentMass2 * (-vt);
                if (!is_finite_f(lambdaT) || !is_finite_f(vt)) SIMPLE_LOG("[Solver] NaN in tangent2 lambda/vt");
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

            UnlockPair(aLock, bLock);
        }
    };

    const size_t N = m_Constraints.size();
    if (N == 0) return;
    // If forced single-threaded, run sequentially
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
    // Position-based correction (non-linear Gauss-Seidel) with random perturbation
    // Thread-local RNG for perturbation (symmetry breaking + FP error compensation)
    thread_local std::mt19937 rng(std::random_device{}());
    thread_local std::uniform_real_distribution<float> dist(-0.0001f, 0.0001f);
    
    int correctionCount = 0;
    float totalCorrection = 0.0f;
    
    for (auto& c : m_Constraints) {
    RigidBody* bodyA = c.BodyA;
    RigidBody* bodyB = c.BodyB;

    if (!bodyA) continue;
    if (bodyA->IsStatic && (bodyB == nullptr || bodyB->IsStatic)) continue;

    // Recompute penetration (positions may have changed)
    Math::Vec3 worldPointA = bodyA->Position + c.RA;
    Math::Vec3 worldPointB = (bodyB) ? (bodyB->Position + c.RB) : Math::Vec3(0.0f, 0.0f, 0.0f);
    Math::Vec3 delta = worldPointA - worldPointB;
    // penetration: distance along normal where positive means overlap (points are penetrating)
    float penetration = -delta.Dot(c.Normal);

    // ENHANCED: Aggressive projection-based correction
    const float slop = 0.005f; // Tighter tolerance (was 0.01f)
    const float maxCorrection = 0.5f; // Allow larger steps (was 0.2f)
    float corrected = std::max(0.0f, penetration - slop);
    if (corrected <= 0.0f) continue;

    float invMassSum = bodyA->InverseMass + (bodyB ? bodyB->InverseMass : 0.0f);
    if (invMassSum <= 0.0f) continue;

    // ENHANCED: Stronger Baumgarte coefficient (projection-based approach)
    float baumgarte = 0.50f; // Much stronger (was 0.35f)
    float correctionMag = std::min(maxCorrection, corrected * baumgarte);

    if (!is_finite_f(correctionMag) || !is_finite_f(penetration)) {
        SIMPLE_LOG("[Solver] NaN detected in positional correction");
    }

    if (std::abs(correctionMag) > 1e4f) {
        SIMPLE_LOG("[Solver] Large positional correction: " + std::to_string(correctionMag));
    }

    Math::Vec3 correctionVec = c.Normal * (correctionMag / invMassSum);
    
    // NEW: Random perturbation to break symmetry and compensate for FP drift
    if (correctionMag > 0.001f) {
        Math::Vec3 perturbation(dist(rng), dist(rng), dist(rng));
        correctionVec += perturbation;
    }
    
    // DEBUG: Log significant corrections
    //if (penetration > 0.01f) {
    //    SIMPLE_LOG("[Solver] Penetration: " + std::to_string(penetration) + 
    //                              " Correction: " + std::to_string(correctionMag) +
    //                              " Normal: (" + std::to_string(c.Normal.x) + "," + std::to_string(c.Normal.y) + "," + std::to_string(c.Normal.z) + ")");
    //    correctionCount++;
    //    totalCorrection += correctionMag;
    //}

    if (!bodyA->IsStatic) bodyA->Position += correctionVec * bodyA->InverseMass;
    if (bodyB && !bodyB->IsStatic) bodyB->Position -= correctionVec * bodyB->InverseMass;

    if (!is_finite_vec3(bodyA->Position) || (bodyB && !is_finite_vec3(bodyB->Position))) {
        SIMPLE_LOG("[Solver] Invalid body position after correction");
    }
    }
}

void IterativeSolver::Clear() {
    m_Constraints.clear();
}

float IterativeSolver::ComputeEffectiveMass(const RigidBody& bodyA, const RigidBody& bodyB,
                                            const Math::Vec3& rA, const Math::Vec3& rB,
                                            const Math::Vec3& direction) {
    // Effective mass formula: 1 / (1/mA + 1/mB + (rA x n) * IA^-1 * (rA x n) + (rB x n) * IB^-1 * (rB x n))
    
    float invMassSum = bodyA.InverseMass;
    float angularA = 0.0f;
    float angularB = 0.0f;

    // Angular contribution from A
    Math::Vec3 rnA = rA.Cross(direction);
    Math::Vec3 i_rnA = ApplyWorldInertia(bodyA, rnA);
    angularA = rnA.Dot(i_rnA);

    // If bodyB is a sentinel (zero inverse mass) then treat as static
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
    // Transform to local space
    Math::Quaternion qInv = rb.Rotation.Conjugate();
    Math::Quaternion vQ(0, v.x, v.y, v.z);
    Math::Quaternion vLocalQ = qInv * vQ * rb.Rotation;
    Math::Vec3 vLocal(vLocalQ.x, vLocalQ.y, vLocalQ.z);
    
    // Apply local inertia tensor
    Math::Vec3 wLocal;
    wLocal.x = vLocal.x * rb.InverseInertiaTensor.x;
    wLocal.y = vLocal.y * rb.InverseInertiaTensor.y;
    wLocal.z = vLocal.z * rb.InverseInertiaTensor.z;
    
    // Transform back to world space
    Math::Quaternion wLocalQ(0, wLocal.x, wLocal.y, wLocal.z);
    Math::Quaternion wWorldQ = rb.Rotation * wLocalQ * qInv;
    
    return Math::Vec3(wWorldQ.x, wWorldQ.y, wWorldQ.z);
}

void IterativeSolver::ComputeTangentBasis(const Math::Vec3& normal, Math::Vec3& tangent1, Math::Vec3& tangent2) {
    // Create orthonormal basis from normal using Gram-Schmidt
    // Choose a vector not parallel to normal
    Math::Vec3 ref(1, 0, 0);
    if (std::abs(normal.x) > 0.9f) {
        ref = Math::Vec3(0, 1, 0);
    }
    
    // First tangent
    tangent1 = ref - (normal * ref.Dot(normal));
    tangent1 = tangent1.Normalized();
    
    // Second tangent (cross product)
    tangent2 = normal.Cross(tangent1);
}

void IterativeSolver::StoreImpulses() {
    for (const auto& c : m_Constraints) {
        // Only cache if there is significant impulse
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
