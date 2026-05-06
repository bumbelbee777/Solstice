#include "NavMesh.hxx"

#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_set>

namespace Solstice::Core {

namespace {
float HeuristicCenters(const NavPolygon& A, const NavPolygon& B) {
    return A.GetCenter().Distance(B.GetCenter());
}
} // namespace

NavMesh::NavMesh() = default;

void NavMesh::BuildFromGeometry(const std::vector<Math::Vec3>& Vertices, const std::vector<uint32_t>& Indices) {
    ClearSyntheticPortalLinks();
    m_Polygons.clear();

    for (size_t i = 0; i + 2 < Indices.size(); i += 3) {
        NavPolygon poly;
        poly.Vertices[0] = Vertices[Indices[i]];
        poly.Vertices[1] = Vertices[Indices[i + 1]];
        poly.Vertices[2] = Vertices[Indices[i + 2]];
        poly.ID = m_Polygons.size();
        m_Polygons.push_back(poly);
    }

    for (size_t a = 0; a < m_Polygons.size(); ++a) {
        for (size_t b = a + 1; b < m_Polygons.size(); ++b) {
            if (AreNeighbors(m_Polygons[a], m_Polygons[b])) {
                m_Polygons[a].Neighbors.push_back(b);
                m_Polygons[b].Neighbors.push_back(a);
            }
        }
    }

    ResizeSyntheticContainers();
}

void NavMesh::ClearSyntheticPortalLinks() {
    m_DirectedPortalCosts.clear();
    for (auto& adj : m_SyntheticNeighbors) {
        adj.clear();
    }
}

void NavMesh::ResizeSyntheticContainers() {
    m_SyntheticNeighbors.assign(m_Polygons.size(), {});
}

void NavMesh::RegisterDirectedPortalEdge(size_t From, size_t To, float Cost) {
    if (From >= m_Polygons.size() || To >= m_Polygons.size() || Cost < 0.f || From == To) {
        return;
    }

    const DirectedKey Key = MakeDirectedKey(From, To);
    auto it = m_DirectedPortalCosts.find(Key);
    if (it == m_DirectedPortalCosts.end() || Cost < it->second) {
        m_DirectedPortalCosts[Key] = Cost;
        auto& list = m_SyntheticNeighbors[From];
        if (std::find(list.begin(), list.end(), To) == list.end()) {
            list.push_back(To);
        }
    }
}

std::vector<size_t> NavMesh::CollectStencilPolygons(const Math::Vec3& Anchor, float Radius,
                                                    int MaxCount) const {
    if (m_Polygons.empty() || MaxCount <= 0) {
        return {};
    }

    const float r = std::max(Radius, 0.05f);

    std::vector<std::pair<float, size_t>> scored;
    scored.reserve(m_Polygons.size());
    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        const NavPolygon& p = m_Polygons[i];
        float d = p.GetCenter().Distance(Anchor);
        if (p.Contains(Anchor)) {
            d *= 0.18f;
        }
        if (d <= r * 10.0f || p.Contains(Anchor)) {
            scored.push_back({ d, i });
        }
    }

    if (scored.empty()) {
        size_t nid = FindNearestPolygon(Anchor);
        if (nid == SIZE_MAX) {
            return {};
        }
        return { nid };
    }

    const size_t k = std::min(static_cast<size_t>(MaxCount), scored.size());
    std::partial_sort(scored.begin(), scored.begin() + static_cast<std::ptrdiff_t>(k), scored.end(),
                      [](const auto& A, const auto& B) { return A.first < B.first; });

    std::vector<size_t> out;
    out.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        out.push_back(scored[i].second);
    }
    return out;
}

std::vector<size_t> NavMesh::GatherNeighborPolygonIds(size_t PolyId) const {
    std::unordered_set<size_t> seen;
    std::vector<size_t> merged;

    const auto Push = [&](size_t id) {
        if (seen.insert(id).second) {
            merged.push_back(id);
        }
    };

    if (PolyId >= m_Polygons.size()) {
        return merged;
    }

    for (size_t geo : m_Polygons[PolyId].Neighbors) {
        Push(geo);
    }
    if (PolyId < m_SyntheticNeighbors.size()) {
        for (size_t syn : m_SyntheticNeighbors[PolyId]) {
            Push(syn);
        }
    }
    return merged;
}

float NavMesh::PolygonTraversalCost(size_t FromId, size_t ToId) const {
    if (FromId >= m_Polygons.size() || ToId >= m_Polygons.size()) {
        return std::numeric_limits<float>::infinity();
    }

    const DirectedKey Key = MakeDirectedKey(FromId, ToId);
    auto syn = m_DirectedPortalCosts.find(Key);
    if (syn != m_DirectedPortalCosts.end()) {
        return syn->second;
    }

    for (size_t geo : m_Polygons[FromId].Neighbors) {
        if (geo == ToId) {
            return m_Polygons[FromId].GetCenter().Distance(m_Polygons[ToId].GetCenter());
        }
    }
    return std::numeric_limits<float>::infinity();
}

bool NavMesh::HasUndirectedTraversal(size_t From, size_t To) const {
    if (PolygonTraversalCost(From, To) < std::numeric_limits<float>::infinity()) {
        return true;
    }
    return PolygonTraversalCost(To, From) < std::numeric_limits<float>::infinity();
}

bool NavMesh::SolveAStar(size_t StartID, size_t GoalID, std::vector<size_t>& OutPath) const {
    OutPath.clear();
    if (StartID >= m_Polygons.size() || GoalID >= m_Polygons.size()) {
        return false;
    }

    if (StartID == GoalID) {
        OutPath.push_back(StartID);
        return true;
    }

    const float kInf = std::numeric_limits<float>::infinity();

    std::priority_queue<PathNode> openSet;
    std::unordered_set<size_t> closedSet;
    std::vector<float> gScores(m_Polygons.size(), kInf);
    std::vector<size_t> parents(m_Polygons.size(), SIZE_MAX);

    PathNode startNode{};
    startNode.PolygonID = StartID;
    startNode.GCost = 0.0f;
    startNode.HCost = HeuristicCenters(m_Polygons[StartID], m_Polygons[GoalID]);
    startNode.FCost = startNode.GCost + startNode.HCost;
    startNode.ParentID = SIZE_MAX;
    gScores[StartID] = 0.f;
    openSet.push(startNode);

    while (!openSet.empty()) {
        PathNode current = openSet.top();
        openSet.pop();

        if (closedSet.find(current.PolygonID) != closedSet.end()) {
            continue;
        }

        closedSet.insert(current.PolygonID);

        if (current.PolygonID == GoalID) {
            size_t nodeID = GoalID;
            while (nodeID != SIZE_MAX) {
                OutPath.push_back(nodeID);
                nodeID = parents[nodeID];
            }
            std::reverse(OutPath.begin(), OutPath.end());
            return true;
        }

        for (size_t neighborID : GatherNeighborPolygonIds(current.PolygonID)) {
            if (closedSet.find(neighborID) != closedSet.end()) {
                continue;
            }

            const float step = PolygonTraversalCost(current.PolygonID, neighborID);
            if (!(step < kInf)) {
                continue;
            }

            const float tentativeG = current.GCost + step;

            if (tentativeG < gScores[neighborID]) {
                gScores[neighborID] = tentativeG;
                parents[neighborID] = current.PolygonID;
                PathNode neighborNode{};
                neighborNode.PolygonID = neighborID;
                neighborNode.GCost = tentativeG;
                neighborNode.HCost = HeuristicCenters(m_Polygons[neighborID], m_Polygons[GoalID]);
                neighborNode.FCost = tentativeG + neighborNode.HCost;
                neighborNode.ParentID = current.PolygonID;
                openSet.push(neighborNode);
            }
        }
    }

    return false;
}

bool NavMesh::SolveThetaStar(size_t StartID, size_t GoalID, std::vector<size_t>& OutPath) const {
    OutPath.clear();
    if (StartID >= m_Polygons.size() || GoalID >= m_Polygons.size()) {
        return false;
    }

    if (StartID == GoalID) {
        OutPath.push_back(StartID);
        return true;
    }

    const float kInf = std::numeric_limits<float>::infinity();

    std::priority_queue<PathNode> openSet;
    std::unordered_set<size_t> closedSet;
    std::vector<float> gScores(m_Polygons.size(), kInf);
    std::vector<size_t> parents(m_Polygons.size(), SIZE_MAX);

    PathNode startNode{};
    startNode.PolygonID = StartID;
    startNode.GCost = 0.0f;
    startNode.HCost = HeuristicCenters(m_Polygons[StartID], m_Polygons[GoalID]);
    startNode.FCost = startNode.GCost + startNode.HCost;
    startNode.ParentID = SIZE_MAX;
    gScores[StartID] = 0.f;
    openSet.push(startNode);

    while (!openSet.empty()) {
        PathNode current = openSet.top();
        openSet.pop();

        if (closedSet.find(current.PolygonID) != closedSet.end()) {
            continue;
        }

        closedSet.insert(current.PolygonID);

        if (current.PolygonID == GoalID) {
            size_t nodeID = GoalID;
            while (nodeID != SIZE_MAX) {
                OutPath.push_back(nodeID);
                nodeID = parents[nodeID];
            }
            std::reverse(OutPath.begin(), OutPath.end());
            return true;
        }

        const size_t parentID = parents[current.PolygonID];

        for (size_t neighborID : GatherNeighborPolygonIds(current.PolygonID)) {
            if (closedSet.find(neighborID) != closedSet.end()) {
                continue;
            }

            const float stepFromCurrent = PolygonTraversalCost(current.PolygonID, neighborID);
            if (!(stepFromCurrent < kInf)) {
                continue;
            }

            float tentativeG = gScores[current.PolygonID] + stepFromCurrent;
            size_t chosenParent = current.PolygonID;

            if (parentID != SIZE_MAX && HasUndirectedTraversal(parentID, neighborID)) {
                const float stepFromGrand = PolygonTraversalCost(parentID, neighborID);
                if (stepFromGrand < kInf) {
                    const float viaParentG = gScores[parentID] + stepFromGrand;
                    if (viaParentG < tentativeG) {
                        tentativeG = viaParentG;
                        chosenParent = parentID;
                    }
                }
            }

            if (tentativeG < gScores[neighborID]) {
                gScores[neighborID] = tentativeG;
                parents[neighborID] = chosenParent;
                PathNode neighborNode{};
                neighborNode.PolygonID = neighborID;
                neighborNode.GCost = tentativeG;
                neighborNode.HCost = HeuristicCenters(m_Polygons[neighborID], m_Polygons[GoalID]);
                neighborNode.FCost = tentativeG + neighborNode.HCost;
                neighborNode.ParentID = chosenParent;
                openSet.push(neighborNode);
            }
        }
    }

    return false;
}

bool NavMesh::RunPathfinding(size_t StartID, size_t GoalID, std::vector<size_t>& PolygonPath) const {
    if (m_Algorithm == PathfindingAlgorithm::ThetaStar) {
        return SolveThetaStar(StartID, GoalID, PolygonPath);
    }
    return SolveAStar(StartID, GoalID, PolygonPath);
}

void NavMesh::AttachSymmetricPortalAnchors(const Math::Vec3& entranceWorld, const Math::Vec3& exitWorld,
                                          const Math::Matrix4& worldToPartner, float linkRadius,
                                          int maxPolysPerAnchorSide, float extraTraversalCost) {
    if (m_Polygons.empty()) {
        return;
    }

    ResizeSyntheticContainers();

    const Math::Vec4 mappedEntHp =
        worldToPartner * Math::Vec4(entranceWorld.x, entranceWorld.y, entranceWorld.z, 1.0f);
    const Math::Vec3 mappedEntrancePartner(mappedEntHp.x, mappedEntHp.y, mappedEntHp.z);

    auto mergePoints = [&](const Math::Vec3& A, const Math::Vec3& B) -> std::vector<size_t> {
        std::vector<size_t> sa = CollectStencilPolygons(A, linkRadius, maxPolysPerAnchorSide);
        std::vector<size_t> sb = CollectStencilPolygons(B, linkRadius, maxPolysPerAnchorSide);

        std::unordered_set<size_t> uniq;
        std::vector<size_t> merged;
        for (size_t x : sa) {
            if (uniq.insert(x).second) merged.push_back(x);
        }
        for (size_t x : sb) {
            if (uniq.insert(x).second) merged.push_back(x);
        }

        while (merged.size() > static_cast<size_t>(std::max(1, maxPolysPerAnchorSide * 2))) {
            merged.pop_back();
        }
        return merged.empty() ? CollectStencilPolygons(A, linkRadius * 4.f, 1) : merged;
    };

    std::vector<size_t> stencilA =
        CollectStencilPolygons(entranceWorld, linkRadius, std::max(1, maxPolysPerAnchorSide));
    std::vector<size_t> stencilB = mergePoints(exitWorld, mappedEntrancePartner);

    if (stencilA.empty() || stencilB.empty()) return;

    const float extra = std::max(extraTraversalCost, 0.f);

    for (size_t ia : stencilA) {
        const Math::Vec3 centerA = m_Polygons[ia].GetCenter();
        for (size_t ib : stencilB) {
            const Math::Vec3 centerB = m_Polygons[ib].GetCenter();

            const float costAB =
                centerA.Distance(entranceWorld) + extra + exitWorld.Distance(centerB);
            const float costBA =
                centerB.Distance(exitWorld) + extra + entranceWorld.Distance(centerA);

            RegisterDirectedPortalEdge(ia, ib, costAB);
            RegisterDirectedPortalEdge(ib, ia, costBA);
        }
    }
}

bool NavMesh::FindPath(const Math::Vec3& Start, const Math::Vec3& End, std::vector<Math::Vec3>& OutPath) {
    size_t startID = FindNearestPolygon(Start);
    size_t endID = FindNearestPolygon(End);

    if (startID == SIZE_MAX || endID == SIZE_MAX) {
        return false;
    }

    std::vector<size_t> polygonPath;
    if (!RunPathfinding(startID, endID, polygonPath)) {
        return false;
    }

    OutPath.clear();
    for (size_t polyID : polygonPath) {
        OutPath.push_back(m_Polygons[polyID].GetCenter());
    }

    return true;
}

size_t NavMesh::FindNearestPolygon(const Math::Vec3& Point) const {
    size_t nearestID = SIZE_MAX;
    float minDist = std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_Polygons.size(); ++i) {
        if (m_Polygons[i].Contains(Point)) {
            return i;
        }

        float dist = Point.Distance(m_Polygons[i].GetCenter());
        if (dist < minDist) {
            minDist = dist;
            nearestID = i;
        }
    }

    return nearestID;
}

bool NavMesh::Raycast(const Math::Vec3& Origin, const Math::Vec3& Direction, float MaxDistance,
                      Math::Vec3& OutHit) const {
    float minT = MaxDistance;
    bool hit = false;

    for (const auto& poly : m_Polygons) {
        Math::Vec3 hitPoint;
        if (RayTriangleIntersect(Origin, Direction, poly, hitPoint)) {
            float t = Origin.Distance(hitPoint);
            if (t < minT) {
                minT = t;
                OutHit = hitPoint;
                hit = true;
            }
        }
    }

    return hit;
}

bool NavMesh::AreNeighbors(const NavPolygon& A, const NavPolygon& B) const {
    int sharedVertices = 0;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (A.Vertices[i].Distance(B.Vertices[j]) < 0.001f) {
                sharedVertices++;
            }
        }
    }
    return sharedVertices >= 2;
}

bool NavMesh::RayTriangleIntersect(const Math::Vec3& Origin, const Math::Vec3& Direction,
                                   const NavPolygon& Poly, Math::Vec3& OutHit) const {
    const float EPSILON = 0.0000001f;
    Math::Vec3 edge1 = Poly.Vertices[1] - Poly.Vertices[0];
    Math::Vec3 edge2 = Poly.Vertices[2] - Poly.Vertices[0];
    Math::Vec3 h = Direction.Cross(edge2);
    float a = edge1.Dot(h);

    if (a > -EPSILON && a < EPSILON) {
        return false;
    }

    float f = 1.0f / a;
    Math::Vec3 s = Origin - Poly.Vertices[0];
    float u = f * s.Dot(h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    Math::Vec3 q = s.Cross(edge1);
    float v = f * Direction.Dot(q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * edge2.Dot(q);
    if (t > EPSILON) {
        OutHit = Origin + Direction * t;
        return true;
    }

    return false;
}

} // namespace Solstice::Core
