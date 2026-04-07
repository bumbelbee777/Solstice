#include "ManifoldManager.hxx"
#include "../Math/Vector.hxx"
#include <algorithm>

namespace Solstice::Physics {

ManifoldManager::ManifoldManager() = default;
ManifoldManager::~ManifoldManager() = default;

void ManifoldManager::UpdateManifolds(const std::vector<CollisionResolution::ContactManifold>& newManifolds) {
    // First, mark all existing manifolds as potentially stale
    for (auto& [pair, cached] : m_CachedManifolds) {
        cached.LifeTime++;
    }

    // Update or create manifolds from new data
    for (const auto& newManifold : newManifolds) {
        if (!newManifold.BodyA) continue;

        BodyPair pair{newManifold.BodyA, newManifold.BodyB};
        auto it = m_CachedManifolds.find(pair);

        if (it != m_CachedManifolds.end()) {
            // Update existing manifold with contact matching and merging
            auto& cached = it->second;
            cached.LifeTime = 0; // Reset lifetime

            // Merge new contacts with existing ones using distance-based matching
            std::vector<bool> newContactMatched(newManifold.Contacts.size(), false);
            std::vector<bool> oldContactMatched(cached.Manifold.Contacts.size(), false);

            // Match contacts by distance (Source engine style)
            for (size_t i = 0; i < newManifold.Contacts.size(); ++i) {
                const auto& newContact = newManifold.Contacts[i];
                float minDist = CONTACT_MATCH_DISTANCE;
                size_t bestMatch = SIZE_MAX;

                for (size_t j = 0; j < cached.Manifold.Contacts.size(); ++j) {
                    if (oldContactMatched[j]) continue;

                    Math::Vec3 diff = newContact.Position - cached.Manifold.Contacts[j].Position;
                    float dist = diff.Magnitude();
                    if (dist < minDist) {
                        minDist = dist;
                        bestMatch = j;
                    }
                }

                if (bestMatch != SIZE_MAX) {
                    // Update existing contact (blend penetration and position for stability)
                    auto& oldContact = cached.Manifold.Contacts[bestMatch];
                    oldContact.Position = (oldContact.Position + newContact.Position) * 0.5f; // Average position
                    oldContact.Penetration = std::max(oldContact.Penetration, newContact.Penetration); // Keep max penetration
                    oldContact.Normal = newContact.Normal; // Use new normal
                    newContactMatched[i] = true;
                    oldContactMatched[bestMatch] = true;
                }
            }

            // Add unmatched new contacts
            for (size_t i = 0; i < newManifold.Contacts.size(); ++i) {
                if (!newContactMatched[i] && cached.Manifold.Contacts.size() < 4) {
                    cached.Manifold.Contacts.push_back(newManifold.Contacts[i]);
                    // Keep parallel with Contacts; without this, oldContactMatched[idx] goes out of bounds
                    // (libstdc++ debug builds abort on vector<bool>::operator[]).
                    oldContactMatched.push_back(false);
                }
            }

            // Remove unmatched old contacts that are no longer valid
            // (Keep them for one frame for stability, but mark for removal)
            auto contactsIt = cached.Manifold.Contacts.begin();
            size_t idx = 0;
            while (contactsIt != cached.Manifold.Contacts.end()) {
                if (idx >= oldContactMatched.size()) {
                    break;
                }
                if (!oldContactMatched[idx] && cached.Manifold.Contacts.size() > newManifold.Contacts.size()) {
                    // Remove stale contact if we have too many
                    contactsIt = cached.Manifold.Contacts.erase(contactsIt);
                    oldContactMatched.erase(oldContactMatched.begin() + idx);
                } else {
                    ++contactsIt;
                    ++idx;
                }
            }

            // Update material properties
            cached.Manifold.Friction = newManifold.Friction;
            cached.Manifold.Restitution = newManifold.Restitution;
        } else {
            // Create new cached manifold
            CachedManifold cached;
            cached.Manifold = newManifold;
            cached.LifeTime = 0;
            m_CachedManifolds[pair] = cached;
        }
    }
}

std::vector<CollisionResolution::ContactManifold> ManifoldManager::GetActiveManifolds() const {
    std::vector<CollisionResolution::ContactManifold> result;
    result.reserve(m_CachedManifolds.size());

    for (const auto& [pair, cached] : m_CachedManifolds) {
        if (cached.LifeTime < MAX_MANIFOLD_LIFETIME && !cached.Manifold.Contacts.empty()) {
            result.push_back(cached.Manifold);
        }
    }

    return result;
}

void ManifoldManager::Clear() {
    m_CachedManifolds.clear();
}

void ManifoldManager::RemoveStaleManifolds() {
    auto it = m_CachedManifolds.begin();
    while (it != m_CachedManifolds.end()) {
        if (it->second.LifeTime >= MAX_MANIFOLD_LIFETIME) {
            it = m_CachedManifolds.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Solstice::Physics
