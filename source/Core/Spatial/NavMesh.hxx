#pragma once

#include <Solstice.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Solstice::Core {

// Navigation polygon (triangle)
struct NavPolygon {
    Math::Vec3 Vertices[3]{};
    std::vector<size_t> Neighbors; // Geometric neighbors (shared edge)
    size_t ID{0};

    NavPolygon() = default;

    Math::Vec3 GetCenter() const {
        return (Vertices[0] + Vertices[1] + Vertices[2]) * (1.0f / 3.0f);
    }

    bool Contains(const Math::Vec3& Point) const {
        Math::Vec3 v0 = Vertices[2] - Vertices[0];
        Math::Vec3 v1 = Vertices[1] - Vertices[0];
        Math::Vec3 v2 = Point - Vertices[0];

        float dot00 = v0.Dot(v0);
        float dot01 = v0.Dot(v1);
        float dot02 = v0.Dot(v2);
        float dot11 = v1.Dot(v1);
        float dot12 = v1.Dot(v2);

        float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
        float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

        return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
    }
};

enum class PathfindingAlgorithm { AStar, ThetaStar };

struct PathNode {
    size_t PolygonID{0};
    float GCost{0};
    float HCost{0};
    float FCost{0};
    size_t ParentID{SIZE_MAX};

    PathNode() = default;

    bool operator<(const PathNode& Other) const {
        return FCost > Other.FCost;
    }
};

// Navigation mesh: geometric connectivity plus optional synthetic portal / off-mesh links.
class SOLSTICE_API NavMesh {
public:
    NavMesh();
    ~NavMesh() = default;

    void SetAlgorithm(PathfindingAlgorithm Algorithm) { m_Algorithm = Algorithm; }
    PathfindingAlgorithm GetAlgorithm() const { return m_Algorithm; }

    void BuildFromGeometry(const std::vector<Math::Vec3>& Vertices, const std::vector<uint32_t>& Indices);

    /// Remove all portal/off-mesh links (geometric topology unchanged).
    void ClearSyntheticPortalLinks();

    /// Connect polygons near logical portal anchors across non-Euclidean topology.
    /// \p worldToPartner maps world coordinates from entrance side framing to exit-side framing (same as ECS \c Portal).
    /// Builds a bipartite web between stencil A (near \p entranceWorld) and stencil B (near \p exitWorld and topology image of entrance).
    void AttachSymmetricPortalAnchors(const Math::Vec3& entranceWorld, const Math::Vec3& exitWorld,
                                     const Math::Matrix4& worldToPartner, float linkRadius,
                                     int maxPolysPerAnchorSide, float extraTraversalCost);

    bool FindPath(const Math::Vec3& Start, const Math::Vec3& End, std::vector<Math::Vec3>& OutPath);

    size_t FindNearestPolygon(const Math::Vec3& Point) const;

    bool Raycast(const Math::Vec3& Origin, const Math::Vec3& Direction, float MaxDistance,
                 Math::Vec3& OutHit) const;

    const std::vector<NavPolygon>& GetPolygons() const { return m_Polygons; }

private:
    using DirectedKey = uint64_t;

    static DirectedKey MakeDirectedKey(size_t From, size_t To) {
        return (static_cast<DirectedKey>(From) << 32) |
            static_cast<DirectedKey>(static_cast<uint32_t>(To));
    }

    void ResizeSyntheticContainers();
    void RegisterDirectedPortalEdge(size_t From, size_t To, float Cost);

    [[nodiscard]] std::vector<size_t> CollectStencilPolygons(const Math::Vec3& Anchor, float Radius,
                                                             int MaxCount) const;

    [[nodiscard]] std::vector<size_t> GatherNeighborPolygonIds(size_t PolyId) const;
    [[nodiscard]] float PolygonTraversalCost(size_t FromId, size_t ToId) const;

    [[nodiscard]] bool HasUndirectedTraversal(size_t From, size_t To) const;

    [[nodiscard]] bool SolveAStar(size_t StartID, size_t GoalID, std::vector<size_t>& OutPath) const;
    [[nodiscard]] bool SolveThetaStar(size_t StartID, size_t GoalID, std::vector<size_t>& OutPath) const;

    [[nodiscard]] bool RunPathfinding(size_t StartID, size_t GoalID, std::vector<size_t>& PolygonPath) const;

    [[nodiscard]] bool AreNeighbors(const NavPolygon& A, const NavPolygon& B) const;

    [[nodiscard]] bool RayTriangleIntersect(const Math::Vec3& Origin, const Math::Vec3& Direction,
                                            const NavPolygon& Poly, Math::Vec3& OutHit) const;

    std::vector<NavPolygon> m_Polygons{};
    PathfindingAlgorithm m_Algorithm{PathfindingAlgorithm::AStar};

    /// Extra adjacency layered on top of tri mesh (directed pairs with explicit travel cost).
    std::vector<std::vector<size_t>> m_SyntheticNeighbors{};
    std::unordered_map<DirectedKey, float> m_DirectedPortalCosts{};
};

} // namespace Solstice::Core
