#include <Physics/Integration/PortalPhysics.hxx>
#include <Physics/Integration/ReactPhysics3DBridge.hxx>
#include <Physics/Dynamics/RigidBody.hxx>
#include <Entity/Components/PortalComponents.hxx>
#include <Entity/Transform.hxx>
#include <cmath>
#include <limits>

namespace Solstice::Physics {

namespace {

constexpr float kPlaneEps = 8e-4f;
constexpr float kApertureSlackXY = 0.15f;
constexpr std::uint8_t kTraversalCooldownTicks = 4;
constexpr float kExitForwardNudge = 0.04f;

[[nodiscard]] bool ComputePortalWorldToPartner(Solstice::ECS::Registry& Registry,
                                               Solstice::ECS::Portal& portal,
                                               Solstice::ECS::Transform& entranceTr,
                                               Math::Matrix4& outWorldToPartner) {
    if (portal.ManualTopology) {
        outWorldToPartner = portal.WorldToPartnerWorld;
        return std::fabs(outWorldToPartner.Determinant()) > 1e-8f;
    }
    if (portal.Partner == 0 || !Registry.Valid(portal.Partner)) {
        return false;
    }
    if (!Registry.Has<Solstice::ECS::Transform>(portal.Partner)) {
        return false;
    }
    const Solstice::ECS::Transform& partnerTr = Registry.Get<Solstice::ECS::Transform>(portal.Partner);
    const float da = std::fabs(entranceTr.Matrix.Determinant());
    const float db = std::fabs(partnerTr.Matrix.Determinant());
    if (da < static_cast<float>(1e-7) || db < static_cast<float>(1e-7)) {
        return false;
    }
    outWorldToPartner = partnerTr.Matrix * entranceTr.Matrix.Inverse();
    return true;
}

[[nodiscard]] Math::Quaternion QuatFromLinear(const Math::Matrix4& M) {
    const float m00 = M.M[0][0], m01 = M.M[0][1], m02 = M.M[0][2];
    const float m10 = M.M[1][0], m11 = M.M[1][1], m12 = M.M[1][2];
    const float m20 = M.M[2][0], m21 = M.M[2][1], m22 = M.M[2][2];

    const float trace = m00 + m11 + m22;
    float qw, qx, qy, qz;
    if (trace > 0.0f) {
        const float S = std::sqrt(trace + 1.0f) * 2.0f;
        qw = 0.25f * S;
        qx = (m21 - m12) / S;
        qy = (m02 - m20) / S;
        qz = (m10 - m01) / S;
    } else if (m00 > m11 && m00 > m22) {
        const float S = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        qw = (m21 - m12) / S;
        qx = 0.25f * S;
        qy = (m01 + m10) / S;
        qz = (m02 + m20) / S;
    } else if (m11 > m22) {
        const float S = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        qw = (m02 - m20) / S;
        qx = (m01 + m10) / S;
        qy = 0.25f * S;
        qz = (m12 + m21) / S;
    } else {
        const float S = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        qw = (m10 - m01) / S;
        qx = (m02 + m20) / S;
        qy = (m12 + m21) / S;
        qz = 0.25f * S;
    }
    return Math::Quaternion(qw, qx, qy, qz).Normalized();
}

[[nodiscard]] bool LocalHitInsideAperture(const Solstice::ECS::Portal& portal, const Math::Vec3& localHit) {
    const float hw = std::max(portal.HalfWidth, 1e-3f);
    const float hh = std::max(portal.HalfHeight, 1e-3f);
    return std::fabs(localHit.x) <= hw + kApertureSlackXY && std::fabs(localHit.y) <= hh + kApertureSlackXY;
}

void WarpRigidBodyPoseAndRates(RigidBody& rb, const Math::Matrix4& worldToPartner) {
    const Math::Quaternion qLin = QuatFromLinear(worldToPartner);
    rb.Position = worldToPartner.TransformPoint(rb.Position);
    rb.Velocity = worldToPartner.TransformVector(rb.Velocity);
    rb.AngularVelocity = worldToPartner.TransformVector(rb.AngularVelocity);
    rb.Rotation = (qLin * rb.Rotation).Normalized();
}

[[nodiscard]] Math::Vec3 UnitPortalForwardWorld(const Solstice::ECS::Transform& tr) {
    Math::Vec3 f = tr.Matrix.TransformVector(Math::Vec3(0.f, 0.f, 1.f));
    const float mag = f.Magnitude();
    return (mag < 1e-6f) ? Math::Vec3(0.f, 0.f, 1.f) : (f * (1.0f / mag));
}

} // namespace

void ResolvePhysicsPortalCrossings(Solstice::ECS::Registry& Registry, ReactPhysics3DBridge& Bridge) {

    Registry.ForEach<RigidBody>(
        [&](Solstice::ECS::EntityId, RigidBody& rb) {
            if (rb.PortalTraversalCooldownFrames > 0) {
                --rb.PortalTraversalCooldownFrames;
            }
        });

    Registry.ForEach<RigidBody>(
        [&](Solstice::ECS::EntityId entityId, RigidBody& rb) {

            if (rb.IsStatic || rb.IsAsleep || !rb.HasRenderInterpolationSnapshot) {
                return;
            }

            const Solstice::ECS::EntityId coolA = rb.PortalCooldownPortal;
            const Solstice::ECS::EntityId coolB = rb.PortalCooldownPartner;
            const bool cooling = rb.PortalTraversalCooldownFrames > 0;

            const Math::Vec3 prev = rb.RenderInterpFromPos;
            const Math::Vec3 curr = rb.Position;

            float bestT = std::numeric_limits<float>::infinity();
            Solstice::ECS::EntityId bestPortalEntity{};
            Solstice::ECS::EntityId bestPartnerComponent{};
            Math::Matrix4 bestWarp{};
            Math::Vec3 bestNudge{};
            bool haveBest = false;

            Registry.ForEach<Solstice::ECS::Portal, Solstice::ECS::Transform>(
                [&](Solstice::ECS::EntityId portalEntityId, Solstice::ECS::Portal& portal,
                    Solstice::ECS::Transform& entranceTr) {

                    if (!portal.LinkEnabled || !portal.AffectPhysics) return;
                    if (portalEntityId == entityId) return;
                    if (cooling
                        && (portalEntityId == coolA || portalEntityId == coolB || portal.Partner == coolA
                            || portal.Partner == coolB)) {
                        return;
                    }

                    Math::Matrix4 worldToPartner{};
                    if (!ComputePortalWorldToPartner(Registry, portal, entranceTr, worldToPartner)) {
                        return;
                    }

                    const Math::Matrix4 invGate = entranceTr.Matrix.Inverse();
                    Math::Vec4 ph = invGate * Math::Vec4(prev.x, prev.y, prev.z, 1.0f);
                    Math::Vec4 ch = invGate * Math::Vec4(curr.x, curr.y, curr.z, 1.0f);
                    const Math::Vec3 localPrev(ph.x, ph.y, ph.z);
                    const Math::Vec3 localCurr(ch.x, ch.y, ch.z);

                    const float zp = localPrev.z;
                    const float zc = localCurr.z;

                    Math::Matrix4 warp{};
                    Math::Vec3 nudge{};
                    if (zp <= -kPlaneEps && zc >= kPlaneEps) {
                        warp = worldToPartner;
                        Solstice::ECS::Transform* partnerTrPtr =
                            (portal.Partner != 0 && Registry.Valid(portal.Partner))
                                ? Registry.TryGet<Solstice::ECS::Transform>(portal.Partner)
                                : nullptr;
                        const Math::Vec3 fwd =
                            partnerTrPtr ? UnitPortalForwardWorld(*partnerTrPtr) : UnitPortalForwardWorld(entranceTr);
                        nudge = fwd * kExitForwardNudge;
                    } else if (zp >= kPlaneEps && zc <= -kPlaneEps) {
                        warp = worldToPartner.Inverse();
                        if (!(std::fabs(warp.Determinant()) > 1e-8f)) return;
                        nudge = UnitPortalForwardWorld(entranceTr) * (-kExitForwardNudge);
                    } else return;

                    const float denom = zp - zc;
                    if (std::fabs(denom) < 1e-8f) return;

                    const float tHit = zp / denom;
                    if (tHit < -1e-3f || tHit > 1.0f + 1e-3f || !(tHit < bestT)) return;

                    const Math::Vec3 hitWorld(prev.x + (curr.x - prev.x) * tHit,
                                              prev.y + (curr.y - prev.y) * tHit,
                                              prev.z + (curr.z - prev.z) * tHit);

                    Math::Vec4 lh = invGate * Math::Vec4(hitWorld.x, hitWorld.y, hitWorld.z, 1.0f);
                    if (!LocalHitInsideAperture(portal, Math::Vec3(lh.x, lh.y, lh.z))) return;

                    bestT = tHit;
                    bestPortalEntity = portalEntityId;
                    bestPartnerComponent = portal.Partner;
                    bestWarp = warp;
                    bestNudge = nudge;
                    haveBest = true;
                });


            if (haveBest) {
                WarpRigidBodyPoseAndRates(rb, bestWarp);
                rb.Position += bestNudge;
                rb.PortalTraversalCooldownFrames = kTraversalCooldownTicks;
                rb.PortalCooldownPortal = bestPortalEntity;
                rb.PortalCooldownPartner = bestPartnerComponent;
                rb.RenderInterpFromPos = rb.Position;
                rb.RenderInterpFromRot = rb.Rotation;
                Bridge.ForcePushRigidBodyToBackend(entityId);
                if (Solstice::ECS::Transform* ecsTr =
                        Registry.TryGet<Solstice::ECS::Transform>(entityId)) {
                    ecsTr->Position = rb.Position;
                    ecsTr->Matrix = Math::Matrix4::Translation(rb.Position) * rb.Rotation.ToMatrix();
                }
            }


        });


}

} // namespace Solstice::Physics

