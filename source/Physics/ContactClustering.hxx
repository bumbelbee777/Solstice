#pragma once

#include "CollisionResolution.hxx"
#include "../Math/Vector.hxx"
#include <vector>

namespace Solstice::Physics {

/// Contact clustering and reduction algorithms (Source engine style)
namespace ContactClustering {

/// Cluster nearby contact points together
/// Groups contacts within clusteringDistance of each other
std::vector<std::vector<size_t>> ClusterContacts(const std::vector<CollisionResolution::ContactPoint>& contacts,
                                                   float clusteringDistance = 0.05f);

/// Reduce contact points to maximum of maxContacts using Source-style heuristics
/// - Prefers contacts that are furthest apart (distance-based)
/// - Prefers contacts with deeper penetration (penetration-weighted)
/// - Prefers face contacts over edge/vertex contacts
void ReduceContacts(std::vector<CollisionResolution::ContactPoint>& contacts, size_t maxContacts = 4);

/// Apply clustering and reduction to a contact manifold
void ProcessManifold(CollisionResolution::ContactManifold& manifold);

} // namespace ContactClustering

} // namespace Solstice::Physics
