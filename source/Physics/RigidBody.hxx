#pragma once

#include "../Math/Vector.hxx"
#include "../Math/Quaternion.hxx"
#include <vector>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Solstice::Physics {

// Forward declaration
struct ConvexHull;

enum class ColliderType {
    Sphere,
    Box,
    Triangle,
    Capsule,       // Sphere-swept line segment
    ConvexHull,    // General convex polyhedron
    Tetrahedron   // Tetrahedron collider type
};

struct AABB {
    Math::Vec3 Min;
    Math::Vec3 Max;

    // Helper to check intersection
    bool Intersects(const AABB& other) const {
        return (Min.x <= other.Max.x && Max.x >= other.Min.x) &&
               (Min.y <= other.Max.y && Max.y >= other.Min.y) &&
               (Min.z <= other.Max.z && Max.z >= other.Min.z);
    }
    
    Math::Vec3 Center() const { return (Min + Max) * 0.5f; }
    Math::Vec3 Extents() const { return (Max - Min) * 0.5f; }
};

struct RigidBody {
    Math::Vec3 Position{}, Velocity{}, Acceleration{};
    Math::Quaternion Rotation{}; // Default identity
    Math::Vec3 AngularVelocity{};
    Math::Vec3 Torque{};
    
    Math::Vec3 Force{}, Impulse{};
    Math::Vec3 ConstantForce{}; // Continuous forces (thrusters, gravity zones, etc.)
    std::vector<Math::Vec3> PendingForces; // Arbitrary forces queued for this step
    std::vector<Math::Vec3> PendingTorques;

    // Collider properties
    ColliderType Type{ColliderType::Sphere};
    
    // Sphere
    float Radius{0.5f};
    
    // Box
    Math::Vec3 HalfExtents{0.5f, 0.5f, 0.5f};
    
    // Capsule
    float CapsuleHeight{1.0f};      // Height of cylindrical section
    float CapsuleRadius{0.5f};
    
    // Convex Hull
    std::vector<Math::Vec3> HullVertices;  // DEPRECATED: use Hull instead
    std::vector<Math::Vec3> HullNormals;   // DEPRECATED: use Hull instead
    std::shared_ptr<ConvexHull> Hull;       // Computed convex hull (for ConvexHull type)
    
    // Continuous Collision Detection (CCD)
    bool IsGrabbed{false};                  // Tracked by gameplay code for special CCD handling
    bool EnableCCD{false};                  // Enable continuous collision detection
    float CCDMotionThreshold{1.0f};         // Min velocity magnitude to trigger CCD

    // Material properties
    float Friction{0.5f};
    float Restitution{0.5f};

    // Link to renderer (Render::SceneObjectID) without including render headers
    uint32_t RenderObjectID{0xFFFFFFFFu};

    float Mass{1.0f};
    float InverseMass{1.0f};
    
    // Inertia Tensor (simplified as diagonal for now, or just inverse scalar for sphere/box approx)
    // For a box, I = m/12 * (h^2 + d^2) etc. 
    // We'll store InverseInertiaTensor in local space (Vec3 diagonal)
    Math::Vec3 InverseInertiaTensor{1.0f, 1.0f, 1.0f}; 
    
    float Drag{0.0f};
    float AngularDrag{0.05f};
    float LinearDamping{0.0f};       // Exponential damping factor (s^-1)
    float QuadraticDrag{0.0f};       // Quadratic drag coefficient
    float GravityScale{1.0f};        // Scale gravity per-body
    bool IsStatic{false};

    void SetMass(float mass) {
        if (mass <= 0.0f) {
            Mass = 0.0f;
            InverseMass = 0.0f;
            InverseInertiaTensor = Math::Vec3(0,0,0);
            IsStatic = true;
        } else {
            Mass = mass;
            InverseMass = 1.0f / mass;
            IsStatic = false;
            // Recompute inertia based on shape? For now default to sphere-like inertia 
            // I = 2/5 * m * r^2. InvI = 1/I.
            float I = 0.4f * mass * Radius * Radius; 
            if (I > 0.0001f) {
                float invI = 1.0f / I;
                InverseInertiaTensor = Math::Vec3(invI, invI, invI);
            } else {
                 InverseInertiaTensor = Math::Vec3(1,1,1); // Fallback
            }
        }
    }
    
    void SetBoxInertia(float mass, const Math::Vec3& halfExtents) {
        if (mass <= 0.0f) {
             SetMass(0.0f);
             return;
        }
        Mass = mass;
        InverseMass = 1.0f / mass;
        IsStatic = false;
        
        // Box inertia: Ixx = m/12 * (h^2 + d^2) = m/12 * ((2*he.y)^2 + (2*he.z)^2)
        // = m/3 * (he.y^2 + he.z^2)
        float c = mass / 3.0f;
        float ixx = c * (halfExtents.y*halfExtents.y + halfExtents.z*halfExtents.z);
        float iyy = c * (halfExtents.x*halfExtents.x + halfExtents.z*halfExtents.z);
        float izz = c * (halfExtents.x*halfExtents.x + halfExtents.y*halfExtents.y);
        
        InverseInertiaTensor = Math::Vec3(1.0f/ixx, 1.0f/iyy, 1.0f/izz);
    }

    // Helper to add arbitrary force for the next integration step
    inline void ApplyForce(const Math::Vec3& f) { PendingForces.push_back(f); }
    inline void ApplyTorque(const Math::Vec3& t) { PendingTorques.push_back(t); }

    // Helper to add impulse immediately (affects velocity)
    inline void ApplyImpulse(const Math::Vec3& j) {
        if (IsStatic || InverseMass == 0.0f) return;
        Velocity += j * InverseMass;
    }
    
    inline void ApplyAngularImpulse(const Math::Vec3& j) {
        if (IsStatic) return;
        // Transform impulse to local space: j_local = q_inv * j_world * q
        // But wait, vector rotation by quaternion q is v' = q * v * q_inv.
        // So to go World -> Local (inverse rotation), we use q_inv.
        // v_local = q_inv * v_world * q
        
        Math::Quaternion qInv = Rotation.Conjugate();
        
        // Manual quaternion-vector multiplication (or use helper if available, but let's inline for safety)
        // v_rot = q * v * q_inv
        // Here we want qInv *j * qInv_inv (which is q)
        
        Math::Quaternion jQ(0, j.x, j.y, j.z);
        Math::Quaternion jLocalQ = qInv * jQ * Rotation;
        Math::Vec3 jLocal(jLocalQ.x, jLocalQ.y, jLocalQ.z);
        
        // Apply inertia
        Math::Vec3 wLocal;
        wLocal.x = jLocal.x * InverseInertiaTensor.x;
        wLocal.y = jLocal.y * InverseInertiaTensor.y;
        wLocal.z = jLocal.z * InverseInertiaTensor.z;
        
        // Transform back to world: w_world = q * w_local * q_inv
        Math::Quaternion wLocalQ(0, wLocal.x, wLocal.y, wLocal.z);
        Math::Quaternion wWorldQ = Rotation * wLocalQ * qInv;
        
        AngularVelocity.x += wWorldQ.x;
        AngularVelocity.y += wWorldQ.y;
        AngularVelocity.z += wWorldQ.z;
    }
};
}