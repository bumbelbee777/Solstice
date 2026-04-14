#include <Physics/Collision/Narrowphase/ContactClustering.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {
namespace ContactClustering {

std::vector<std::vector<size_t>> ClusterContacts(const std::vector<CollisionResolution::ContactPoint>& contacts,
                                                   float clusteringDistance) {
    std::vector<std::vector<size_t>> clusters;
    std::vector<bool> assigned(contacts.size(), false);

    for (size_t i = 0; i < contacts.size(); ++i) {
        if (assigned[i]) continue;

        std::vector<size_t> cluster;
        cluster.push_back(i);
        assigned[i] = true;

        // Find all contacts within clustering distance
        for (size_t j = i + 1; j < contacts.size(); ++j) {
            if (assigned[j]) continue;

            Math::Vec3 diff = contacts[i].Position - contacts[j].Position;
            float dist = diff.Magnitude();
            if (dist <= clusteringDistance) {
                cluster.push_back(j);
                assigned[j] = true;
            }
        }

        clusters.push_back(cluster);
    }

    return clusters;
}

void ReduceContacts(std::vector<CollisionResolution::ContactPoint>& contacts, size_t maxContacts) {
    if (contacts.size() <= maxContacts) return;

    // Source-style reduction: prefer contacts that are furthest apart and have deeper penetration
    std::vector<CollisionResolution::ContactPoint> reduced;

    // First, sort by penetration (deeper is better)
    std::vector<std::pair<size_t, float>> indexedPenetrations;
    for (size_t i = 0; i < contacts.size(); ++i) {
        indexedPenetrations.push_back({i, contacts[i].Penetration});
    }
    std::sort(indexedPenetrations.begin(), indexedPenetrations.end(),
        [](const std::pair<size_t, float>& a, const std::pair<size_t, float>& b) {
            return a.second > b.second;
        });

    // Greedy selection: pick contacts that are furthest from already selected ones
    std::vector<bool> selected(contacts.size(), false);

    // Always include the deepest contact
    if (!indexedPenetrations.empty()) {
        size_t deepestIdx = indexedPenetrations[0].first;
        reduced.push_back(contacts[deepestIdx]);
        selected[deepestIdx] = true;
    }

    // Select remaining contacts based on distance from selected set
    while (reduced.size() < maxContacts && reduced.size() < contacts.size()) {
        float maxMinDist = -1.0f;
        size_t bestIdx = 0;

        for (size_t i = 0; i < contacts.size(); ++i) {
            if (selected[i]) continue;

            // Find minimum distance to any already selected contact
            float minDist = 1e9f;
            for (const auto& selectedContact : reduced) {
                Math::Vec3 diff = contacts[i].Position - selectedContact.Position;
                float dist = diff.Magnitude();
                if (dist < minDist) {
                    minDist = dist;
                }
            }

            // Prefer contacts that are far from selected ones (better distribution)
            // Weight by penetration depth
            float score = minDist * (1.0f + contacts[i].Penetration * 10.0f);
            if (score > maxMinDist) {
                maxMinDist = score;
                bestIdx = i;
            }
        }

        if (maxMinDist > 0.0f) {
            reduced.push_back(contacts[bestIdx]);
            selected[bestIdx] = true;
        } else {
            break; // No more valid contacts
        }
    }

    contacts = reduced;
}

void ProcessManifold(CollisionResolution::ContactManifold& manifold) {
    if (manifold.Contacts.size() <= 4) return; // Already at or below limit

    // First cluster contacts
    auto clusters = ClusterContacts(manifold.Contacts, 0.05f);

    // For each cluster, pick the best representative (deepest penetration)
    std::vector<CollisionResolution::ContactPoint> clustered;
    for (const auto& cluster : clusters) {
        if (cluster.empty()) continue;

        // Find contact with deepest penetration in cluster
        size_t bestIdx = cluster[0];
        float bestPen = manifold.Contacts[bestIdx].Penetration;
        for (size_t idx : cluster) {
            if (manifold.Contacts[idx].Penetration > bestPen) {
                bestPen = manifold.Contacts[idx].Penetration;
                bestIdx = idx;
            }
        }
        clustered.push_back(manifold.Contacts[bestIdx]);
    }

    // Then reduce to max 4 contacts
    ReduceContacts(clustered, 4);
    manifold.Contacts = clustered;
}

} // namespace ContactClustering
} // namespace Solstice::Physics
