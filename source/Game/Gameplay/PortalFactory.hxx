#pragma once

#include "../../Solstice.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/EntityId.hxx"
#include "../../Entity/Transform.hxx"
#include "../../Entity/Name.hxx"
#include "../../Entity/Kind.hxx"
#include "../../Entity/Components/PortalComponents.hxx"
#include "../../Math/Vector.hxx"
#include "../../Math/Matrix.hxx"
#include <cmath>
#include <string>
#include <utility>

namespace Solstice::Game {

/// Shared invisible defaults for ECS portals (still affect physics/audio when enabled).
SOLSTICE_API void MakeInvisiblePortal(ECS::Portal& out);

/// Convenience: invisible portal sized for shallow floor/ceiling apertures (XZ span in portal local XY).
SOLSTICE_API void MakeInvisiblePortalFloorOpening(ECS::Portal& out, float halfX, float halfZ);

SOLSTICE_API void LinkBidirectionalPortals(ECS::Registry& registry, ECS::EntityId portalA,
                                           ECS::EntityId portalB);

/// Spawns portal entity at \p worldPosition with rigid orientation matrix (orthogonal + translation).
SOLSTICE_API ECS::EntityId CreatePortalEntityWithMatrix(ECS::Registry& registry,
                                                        const Math::Vec3& worldPosition,
                                                        const Math::Matrix4& worldRigid,
                                                        ECS::Portal spec,
                                                        const std::string& name = "Portal");

/// Horizontal aperture portals on two parallel slabs at \p floorCenterWorld.y and \p ceilingYWorld.
/// Uses identical orientation (local XY spans world X/Z; local +Z = world −Y downward) so
/// `partner * inverse(this)` resolves to roughly a vertical translation of `ceilingYWorld - floorCenterWorld.y`.
/// Returns \{ floorPortalId, ceilingPortalId \}.
SOLSTICE_API std::pair<ECS::EntityId, ECS::EntityId>
CreateVerticalLoopPortalPair(ECS::Registry& registry, const Math::Vec3& floorCenterWorld,
                             float ceilingYWorld, float halfWidthXz, float halfDepthXz,
                             const std::string& debugNameStem = "VertLoopPortal");

#if defined(DEBUG) || !defined(NDEBUG)
/// Debug-only sanity checks for portal topology (orthogonal matrix branch, determinant).
SOLSTICE_API void AssertOrthogonalRigidMatrix(const Math::Matrix4& m, float tol = 1e-4f);
#else
inline void AssertOrthogonalRigidMatrix(const Math::Matrix4&, float = 1e-4f) {}
#endif

} // namespace Solstice::Game
