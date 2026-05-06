#pragma once

#include <Solstice.hxx>
#include <Entity/Registry.hxx>

namespace Solstice::Physics {

class ReactPhysics3DBridge;

/// After each physics backend step, teleport dynamic bodies through enabled portal apertures using the same
/// \c ECS::Portal topology as audio/navigation (\c WorldToPartnerWorld maps entrance world coords to partner world).
SOLSTICE_API void ResolvePhysicsPortalCrossings(Solstice::ECS::Registry& Registry, ReactPhysics3DBridge& Bridge);

} // namespace Solstice::Physics
