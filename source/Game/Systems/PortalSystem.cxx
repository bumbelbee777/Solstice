#include "PortalSystem.hxx"
#include <Arzachel/Seed.hxx>
#include <Entity/Components/PortalComponents.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Render/Portal/PortalDebugDraw.hxx>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace Solstice::Game {

namespace {

struct LiveParticle {
    Math::Vec3 Position{};
    Math::Vec3 Velocity{};
    float Age{0};
    float MaxAge{1};
};

struct PortalVfxLane {
    std::vector<LiveParticle> Particles{};
    float SpawnDebt{0};
    Arzachel::Seed Rng{914737};
    bool RngPrimed{false};
};

std::unordered_map<ECS::EntityId, PortalVfxLane>& VfxLanes() {
    static std::unordered_map<ECS::EntityId, PortalVfxLane> lanes;
    return lanes;
}

void PruneStaleLanes(const ECS::Registry& Registry) {
    auto& lanes = VfxLanes();
    for (auto it = lanes.begin(); it != lanes.end();) {
        if (!Registry.Valid(it->first)) it = lanes.erase(it);
        else ++it;
    }
}

Math::Vec3 QuadCorner(const ECS::Transform& tr, float hx, float hy, float lx, float ly, float lz) {
    const Math::Vec3 local(lx * hx, ly * hy, lz);
    return tr.Matrix.TransformPoint(local);
}

} // namespace

void PortalSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    Render::PortalDebugDrawBuffer::Instance().BeginFrame();
    PruneStaleLanes(Registry);

    Registry.ForEach<ECS::Portal, ECS::Transform>([&](ECS::EntityId entity, ECS::Portal& portal, ECS::Transform& tr) {
        if (!portal.ShowVisual && !portal.EmitParticles) return;

        using Render::PackPortalLineColor;

        const float hw = std::max(0.05f, portal.HalfWidth);
        const float hh = std::max(0.05f, portal.HalfHeight);

        auto& dbg = Render::PortalDebugDrawBuffer::Instance();

        if (portal.ShowVisual) {
            const uint32_t lineRgb =
                PackPortalLineColor(portal.ColorR * portal.ColorA + (1.f - portal.ColorA) * 0.05f,
                    portal.ColorG * portal.ColorA + (1.f - portal.ColorA) * 0.05f,
                    portal.ColorB * portal.ColorA + (1.f - portal.ColorA) * 0.05f);

            const Math::Vec3 a = QuadCorner(tr, hw, hh, -1, -1, 0);
            const Math::Vec3 b = QuadCorner(tr, hw, hh, 1, -1, 0);
            const Math::Vec3 c = QuadCorner(tr, hw, hh, 1, 1, 0);
            const Math::Vec3 d = QuadCorner(tr, hw, hh, -1, 1, 0);

            dbg.AddLine(a, b, lineRgb);
            dbg.AddLine(b, c, lineRgb);
            dbg.AddLine(c, d, lineRgb);
            dbg.AddLine(d, a, lineRgb);
        }

        if (!portal.EmitParticles || portal.MaxParticles <= 0) return;

        auto& lane = VfxLanes()[entity];
        if (!lane.RngPrimed) {
            const uint64_t salt = portal.VisualSeed != 0 ? portal.VisualSeed : 0x9E3779B97F4A7C15ULL;
            lane.Rng = Arzachel::Seed(salt ^ ((uint64_t(entity) << 32) | entity));
            lane.RngPrimed = true;
        }

        constexpr float kEpsilon = 1e-6f;

        lane.SpawnDebt += std::max(0.f, portal.ParticleRate) * DeltaTime;

        LiveParticle newborn{};
        Math::Vec3 forward(tr.Matrix.M[0][2], tr.Matrix.M[1][2], tr.Matrix.M[2][2]);
        if (forward.Dot(forward) < kEpsilon * kEpsilon) forward = Math::Vec3(0.0f, 0.0f, 1.0f);
        forward = forward.Normalized();
        Math::Vec3 basisX(tr.Matrix.M[0][0], tr.Matrix.M[1][0], tr.Matrix.M[2][0]);
        Math::Vec3 basisY(tr.Matrix.M[0][1], tr.Matrix.M[1][1], tr.Matrix.M[2][1]);
        basisX = basisX.Normalized();
        basisY = basisY.Normalized();

        while (lane.SpawnDebt >= 1.f &&
               static_cast<int>(lane.Particles.size()) < std::max(1, portal.MaxParticles)) {
            lane.SpawnDebt -= 1.f;
            const float u = Arzachel::NextFloat(lane.Rng) * 2.f - 1.f;
            lane.Rng = Arzachel::NextSeed(lane.Rng);
            const float v = Arzachel::NextFloat(lane.Rng) * 2.f - 1.f;
            lane.Rng = Arzachel::NextSeed(lane.Rng);

            Math::Vec3 local(u * hw * portal.ParticleSpread, v * hh * portal.ParticleSpread, 0.0f);
            newborn.Position = tr.Matrix.TransformPoint(local);

            Math::Vec3 vel = forward * std::max(0.05f, portal.ParticleSpeed);
            const float jx = Arzachel::NextFloat(lane.Rng.Derive(0x511u)) - 0.5f + kEpsilon;
            lane.Rng = Arzachel::NextSeed(lane.Rng);
            const float jy = Arzachel::NextFloat(lane.Rng.Derive(0x717u)) - 0.5f + kEpsilon;
            lane.Rng = Arzachel::NextSeed(lane.Rng);
            vel += basisX * jx * portal.ParticleTurbulence;
            vel += basisY * jy * portal.ParticleTurbulence;
            newborn.Velocity = vel;
            newborn.Age = 0;
            newborn.MaxAge =
                std::max(0.08f, portal.ParticleLifetime) * (0.82f + 0.36f * Arzachel::NextFloat(lane.Rng));
            lane.Rng = Arzachel::NextSeed(lane.Rng);
            lane.Particles.push_back(newborn);
        }

        while (static_cast<int>(lane.Particles.size()) > portal.MaxParticles) {
            lane.Particles.erase(lane.Particles.begin());
        }

        const uint32_t pCol = PackPortalLineColor(portal.ColorR * 1.08f, portal.ColorG * 1.08f, portal.ColorB * 1.08f);

        std::vector<LiveParticle> next;
        next.reserve(lane.Particles.size());
        uint32_t turbTick = 0;
        for (LiveParticle p : lane.Particles) {
            p.Age += DeltaTime;
            if (p.Age >= p.MaxAge) continue;

            const float tx =
                Arzachel::NextFloat(lane.Rng.Derive(0x913u ^ (uint64_t(turbTick++) * 997u))) - 0.5f;
            lane.Rng = Arzachel::NextSeed(lane.Rng);
            const float ty =
                Arzachel::NextFloat(lane.Rng.Derive(0xABCu ^ (uint64_t(turbTick++) * 1009u))) - 0.5f;
            lane.Rng = Arzachel::NextSeed(lane.Rng);

            p.Velocity += basisX.Normalized() * tx * portal.ParticleTurbulence * DeltaTime;
            p.Velocity += basisY.Normalized() * ty * portal.ParticleTurbulence * DeltaTime;
            p.Position += p.Velocity * DeltaTime;

            Math::Vec3 dir = p.Velocity;
            if (dir.Dot(dir) < kEpsilon * kEpsilon) dir = forward;
            const float streak =
                std::clamp(0.02f + 0.35f * (1.f - p.Age / p.MaxAge), 0.015f, 0.55f);
            const Math::Vec3 tip = p.Position + dir.Normalized() * streak;

            dbg.AddLine(p.Position, tip, pCol);

            float s =
                std::clamp(0.035f + 0.045f * (1.f - p.Age / p.MaxAge), 0.015f, 0.18f);
            const Math::Vec3 bx = basisX * s;
            const Math::Vec3 by = basisY * (s * 0.92f);
            dbg.AddLine(p.Position - bx, p.Position + bx, pCol);
            dbg.AddLine(p.Position - by, p.Position + by, pCol);

            next.push_back(std::move(p));
        }
        lane.Particles.swap(next);
    });
}

} // namespace Solstice::Game
