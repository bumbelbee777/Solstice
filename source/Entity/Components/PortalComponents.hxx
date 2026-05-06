#pragma once

#include <Entity/EntityId.hxx>
#include <Math/Matrix.hxx>
#include <cstdint>

namespace Solstice::ECS {

/// Portal topological link plus optional authoring for visuals / FX.
/// Framing lives on ECS::Transform (Matrix maps portal-local axes to world).
struct Portal {
    EntityId Partner{0};

    /// When true, downstream systems that honor portals (audio, etc.) use this link.
    bool LinkEnabled{true};

    /// When false, dynamic rigid bodies are not topology-warped through this portal (audio/nav unchanged).
    bool AffectPhysics{true};

    /// False: derive WorldToPartnerWorld as partner.Matrix * Inverse(this.Matrix).
    /// True: use WorldToPartnerWorld as authored.
    bool ManualTopology{false};
    Math::Matrix4 WorldToPartnerWorld{Math::Matrix4::Identity()};

    /// When false the portal emits no editor/debug overlay or particle streaks here.
    bool ShowVisual{true};

    float ColorR{0.35f};
    float ColorG{0.72f};
    float ColorB{1.0f};
    float ColorA{0.92f};

    /// Quad opening in portal local XY plane (+Z faces “through”).
    float HalfWidth{1.25f};
    float HalfHeight{2.25f};

    bool EmitParticles{false};
    float ParticleRate{32.0f};
    float ParticleLifetime{1.5f};
    float ParticleSpeed{0.95f};
    float ParticleSpread{0.22f};   // lateral jitter (fraction of half extents)
    float ParticleTurbulence{0.12f};
    int MaxParticles{96};

    /// Mixed into RNG for stable per-portal streams.
    uint64_t VisualSeed{0};

    /// 0 disables directional blend + ghost point/spot propagation for this doorway; 1.0 typical.
    float LightPropagationScale{1.0f};

    /// Nav synthesis (RebuildNavMeshPortalLinks): gathers triangles near anchors and bipartite-links across topology.
    float NavMeshLinkRadius{3.5f};
    int NavMeshPolyBudgetPerSide{3};
    float NavMeshTraversalMargin{0.12f};
};

} // namespace Solstice::ECS
