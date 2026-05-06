#include "Gameplay/PortalFactory.hxx"
#include "Core/Debug/Debug.hxx"

#include <algorithm>
#include <cmath>

namespace Solstice::Game {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

void MakeInvisiblePortal(ECS::Portal& out) {
    out.ShowVisual = false;
    out.EmitParticles = false;
    out.AffectPhysics = true;
    out.LinkEnabled = true;
}

void MakeInvisiblePortalFloorOpening(ECS::Portal& out, float halfX, float halfZ) {
    MakeInvisiblePortal(out);
    out.HalfWidth = std::max(0.05f, halfX);
    out.HalfHeight = std::max(0.05f, halfZ);
}

void LinkBidirectionalPortals(ECS::Registry& registry, ECS::EntityId portalA,
                              ECS::EntityId portalB) {
    if (portalA == 0 || portalB == 0 || portalA == portalB) return;
    if (!registry.Valid(portalA) || !registry.Valid(portalB)) return;
    if (!(registry.Has<ECS::Portal>(portalA) && registry.Has<ECS::Transform>(portalA) &&
            registry.Has<ECS::Portal>(portalB) && registry.Has<ECS::Transform>(portalB))) {
#if defined(DEBUG) || !defined(NDEBUG)
        SIMPLE_LOG("LinkBidirectionalPortals: Missing Portal or Transform components");
#endif
        return;
    }
    ECS::Portal& a = registry.Get<ECS::Portal>(portalA);
    ECS::Portal& b = registry.Get<ECS::Portal>(portalB);
    a.Partner = portalB;
    b.Partner = portalA;
    a.ManualTopology = false;
    b.ManualTopology = false;
}

ECS::EntityId CreatePortalEntityWithMatrix(ECS::Registry& registry, const Math::Vec3& worldPosition,
                                             const Math::Matrix4& worldRigid, ECS::Portal spec,
                                             const std::string& name) {
    ECS::EntityId entity = registry.Create();
    ECS::Transform tr{};
    tr.Position = worldPosition;
    tr.Scale = Math::Vec3(1.f, 1.f, 1.f);
    tr.Matrix = worldRigid;
#if defined(DEBUG) || !defined(NDEBUG)
    AssertOrthogonalRigidMatrix(worldRigid);
#endif

    registry.Add<ECS::Transform>(entity, tr);
    registry.Add<ECS::Portal>(entity, spec);
    registry.Add<ECS::Name>(entity, ECS::Name{name});
    registry.Add<ECS::Kind>(entity, ECS::Kind{ECS::EntityKind::Environment});
    return entity;
}

std::pair<ECS::EntityId, ECS::EntityId> CreateVerticalLoopPortalPair(ECS::Registry& registry,
                                                                     const Math::Vec3& floorCenterWorld,
                                                                     float ceilingYWorld,
                                                                     float halfWidthXz,
                                                                     float halfDepthXz,
                                                                     const std::string& debugNameStem) {
    // Local XY is horizontal (world XZ); local +Z points down (−world Y) through the slab.
    const Math::Matrix4 rot = Math::Matrix4::RotationX(kPi * 0.5f);
    const Math::Matrix4 floorM = Math::Matrix4::Translation(floorCenterWorld) * rot;
    const Math::Vec3 ceilingPos(floorCenterWorld.x, ceilingYWorld, floorCenterWorld.z);
    const Math::Matrix4 ceilingM = Math::Matrix4::Translation(ceilingPos) * rot;

    ECS::Portal floorSpec{};
    MakeInvisiblePortalFloorOpening(floorSpec, halfWidthXz, halfDepthXz);
    ECS::Portal ceilingSpec = floorSpec;

    ECS::EntityId floorId = CreatePortalEntityWithMatrix(registry, floorCenterWorld, floorM, floorSpec,
                                                         debugNameStem + "_Floor");
    ECS::EntityId ceilingId = CreatePortalEntityWithMatrix(registry, ceilingPos, ceilingM, ceilingSpec,
                                                           debugNameStem + "_Ceiling");

    LinkBidirectionalPortals(registry, floorId, ceilingId);

#if defined(DEBUG) || !defined(NDEBUG)
    // Auto topology: W = Mc * inv(Mf) should match vertical translation by ~room height.
    const Math::Matrix4 W = ceilingM * floorM.Inverse();
    const float expectedDy = ceilingYWorld - floorCenterWorld.y;
    const float actualDy = W.M[1][3];
    if (std::fabs(actualDy - expectedDy) > 0.05f) {
        SIMPLE_LOG("CreateVerticalLoopPortalPair: vertical translation mismatch expected " + std::to_string(
                       expectedDy) + " got " + std::to_string(actualDy));
    }
#endif

    return {floorId, ceilingId};
}

#if defined(DEBUG) || !defined(NDEBUG)
void AssertOrthogonalRigidMatrix(const Math::Matrix4& m, float tol) {
    // Top-left 3x3 should be orthogonal: R * R^T ≈ I
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            float s = 0.f;
            for (int k = 0; k < 3; ++k) {
                s += m.M[i][k] * m.M[j][k];
            }
            const float expect = (i == j) ? 1.f : 0.f;
            if (std::fabs(s - expect) > tol) {
                SIMPLE_LOG("AssertOrthogonalRigidMatrix: non-orthogonal column basis");
                break;
            }
        }
    }
    const float det = m.M[0][0] * (m.M[1][1] * m.M[2][2] - m.M[1][2] * m.M[2][1])
                      - m.M[0][1] * (m.M[1][0] * m.M[2][2] - m.M[1][2] * m.M[2][0])
                      + m.M[0][2] * (m.M[1][0] * m.M[2][1] - m.M[1][1] * m.M[2][0]);
    if (std::fabs(det) < 1e-5f) {
        SIMPLE_LOG("AssertOrthogonalRigidMatrix: near-singular rotation block");
    }
}
#endif

} // namespace Solstice::Game
