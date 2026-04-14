#pragma once

#include <Physics/Collision/Narrowphase/CollisionResolution.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <unordered_map>
#include <vector>

namespace Solstice::Physics {

/// Manages persistent contact manifolds between frames (Source engine style)
class ManifoldManager {
public:
    ManifoldManager();
    ~ManifoldManager();

    /// Update persistent manifolds with new collision data
    /// Merges new contacts into cached manifolds or creates new ones
    void UpdateManifolds(const std::vector<CollisionResolution::ContactManifold>& newManifolds);

    /// Get all active manifolds (including cached ones that are still valid)
    std::vector<CollisionResolution::ContactManifold> GetActiveManifolds() const;

    /// Clear all cached manifolds
    void Clear();

    /// Remove stale manifolds (called each frame)
    void RemoveStaleManifolds();

private:
    struct BodyPair {
        RigidBody* A;
        RigidBody* B;

        bool operator==(const BodyPair& other) const {
            return (A == other.A && B == other.B) || (A == other.B && B == other.A);
        }
    };

    struct BodyPairHash {
        size_t operator()(const BodyPair& pair) const {
            size_t h1 = std::hash<RigidBody*>{}(pair.A);
            size_t h2 = std::hash<RigidBody*>{}(pair.B);
            return h1 ^ (h2 << 1);
        }
    };

    struct CachedManifold {
        CollisionResolution::ContactManifold Manifold;
        int LifeTime; // Frames since last update
    };

    std::unordered_map<BodyPair, CachedManifold, BodyPairHash> m_CachedManifolds;
    static constexpr int MAX_MANIFOLD_LIFETIME = 2; // Keep for 2 frames after separation
    static constexpr float CONTACT_MATCH_DISTANCE = 0.02f; // Distance threshold for matching contacts
};

} // namespace Solstice::Physics
