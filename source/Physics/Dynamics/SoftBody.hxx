#pragma once

#include <Physics/Integration/PhysicsBackend.hxx>
#include <Math/Vector.hxx>
#include <vector>
#include <cstdint>

namespace Solstice::Physics {

struct SoftBody {
    std::vector<SoftBodyNode> Nodes{};
    std::vector<SoftBodyConstraint> Constraints{};
    SoftBodyConfig Config{};
    Math::Vec3 Gravity{0.0f, -9.81f, 0.0f};
    Math::Vec3 WindForce{0.0f, 0.0f, 0.0f};
    bool Enabled{true};

    void BuildRectCloth(const Math::Vec3& origin, const SoftBodyConfig& cfg) {
        Config = cfg;
        Nodes.clear();
        Constraints.clear();

        const uint32_t w = (cfg.GridWidth < 2u) ? 2u : cfg.GridWidth;
        const uint32_t h = (cfg.GridHeight < 2u) ? 2u : cfg.GridHeight;
        const float spacing = (cfg.NodeSpacing <= 0.0f) ? 0.1f : cfg.NodeSpacing;
        const float nodeInvMass = (cfg.NodeMass > 0.0f) ? (1.0f / cfg.NodeMass) : 0.0f;

        Nodes.reserve(static_cast<size_t>(w) * static_cast<size_t>(h));
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                SoftBodyNode node{};
                node.Position = origin + Math::Vec3(static_cast<float>(x) * spacing, -static_cast<float>(y) * spacing, 0.0f);
                node.PredictedPosition = node.Position;
                node.Velocity = Math::Vec3(0.0f, 0.0f, 0.0f);
                node.InverseMass = nodeInvMass;
                if (cfg.AnchorTopRow && y == 0u) {
                    node.InverseMass = 0.0f;
                }
                Nodes.push_back(node);
            }
        }

        auto idx = [w](uint32_t x, uint32_t y) -> uint32_t {
            return y * w + x;
        };
        auto addConstraint = [this](uint32_t a, uint32_t b) {
            SoftBodyConstraint c{};
            c.NodeA = a;
            c.NodeB = b;
            c.RestLength = (Nodes[a].Position - Nodes[b].Position).Magnitude();
            Constraints.push_back(c);
        };

        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                if (x + 1u < w) addConstraint(idx(x, y), idx(x + 1u, y));
                if (y + 1u < h) addConstraint(idx(x, y), idx(x, y + 1u));
                if (x + 1u < w && y + 1u < h) addConstraint(idx(x, y), idx(x + 1u, y + 1u));
                if (x > 0u && y + 1u < h) addConstraint(idx(x, y), idx(x - 1u, y + 1u));
                if (x + 2u < w) addConstraint(idx(x, y), idx(x + 2u, y));
                if (y + 2u < h) addConstraint(idx(x, y), idx(x, y + 2u));
            }
        }
    }
};

} // namespace Solstice::Physics
