#include "NavMeshPortalLinks.hxx"
#include <Core/Spatial/NavMesh.hxx>
#include <Entity/Components/PortalComponents.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Game {

void RebuildNavMeshPortalLinks(Core::NavMesh& Mesh, ECS::Registry& Registry) {
    Mesh.ClearSyntheticPortalLinks();
    if (Mesh.GetPolygons().empty()) return;

    Registry.ForEach<ECS::Portal, ECS::Transform>(
        [&](ECS::EntityId, ECS::Portal& portal, ECS::Transform& entranceTr) {
            if (!portal.LinkEnabled || portal.NavMeshPolyBudgetPerSide <= 0) return;

            const float radius =
                std::max(0.05f, std::clamp(portal.NavMeshLinkRadius, 0.1f, 5000.f));
            const int budget = std::clamp(portal.NavMeshPolyBudgetPerSide, 1, 32);
            const float margin = std::max(0.f, portal.NavMeshTraversalMargin);

            if (portal.ManualTopology) {
                if (portal.Partner == 0 || !Registry.Valid(portal.Partner)) return;
                if (!Registry.Has<ECS::Transform>(portal.Partner)) return;

                const ECS::Transform& exitTr = Registry.Get<ECS::Transform>(portal.Partner);
                Mesh.AttachSymmetricPortalAnchors(entranceTr.Position, exitTr.Position,
                                                  portal.WorldToPartnerWorld, radius, budget, margin);
                return;
            }

            if (portal.Partner == 0 || !Registry.Valid(portal.Partner)) return;
            if (!Registry.Has<ECS::Transform>(portal.Partner)) return;

            const ECS::Transform& partnerTr = Registry.Get<ECS::Transform>(portal.Partner);

            if (std::abs(entranceTr.Matrix.Determinant()) < static_cast<float>(1e-8) ||
                std::abs(partnerTr.Matrix.Determinant()) < static_cast<float>(1e-8)) {
                return;
            }

            const Math::Matrix4 worldToPartner = partnerTr.Matrix * entranceTr.Matrix.Inverse();

            Mesh.AttachSymmetricPortalAnchors(entranceTr.Position, partnerTr.Position, worldToPartner, radius,
                                                budget, margin);
        });
}

} // namespace Solstice::Game
