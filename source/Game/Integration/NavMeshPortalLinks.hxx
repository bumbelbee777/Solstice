#pragma once

#include <Solstice.hxx>

namespace Solstice::Core {
class NavMesh;
}
namespace Solstice::ECS {
class Registry;
}

namespace Solstice::Game {

/// Clears stale portal/off-mesh nav links then registers directed edges for every ECS \c Portal.
/// Call after \c NavMesh::BuildFromGeometry and whenever doorway topology changes materially.
SOLSTICE_API void RebuildNavMeshPortalLinks(Core::NavMesh& Mesh, ECS::Registry& Registry);

} // namespace Solstice::Game
