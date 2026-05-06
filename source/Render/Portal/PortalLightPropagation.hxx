#pragma once

#include <Solstice.hxx>
#include <Math/Matrix.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <vector>

namespace Solstice::Render {

namespace PortalLightPropagation {

/// Last active portal topology for forward / raytracing / volumetrics (cheap single-link model).
/// Call from gameplay when the audible portal resolves; disables when portals are inactive.
SOLSTICE_API void ConfigureActivePortal(const Math::Matrix4& worldToPartnerWorld, float strengthScale);
SOLSTICE_API void ClearActivePortal();

/// Applies directional blending + clones point / spot lamps through \c WorldToPartner.
/// Intended for RenderScene/light paths; harmless no-op when no portal is active.
SOLSTICE_API std::vector<Solstice::Physics::LightSource>
PrepareLights(const std::vector<Solstice::Physics::LightSource>& lights);

} // namespace PortalLightPropagation

} // namespace Solstice::Render
