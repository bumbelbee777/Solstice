#pragma once

#include "../Solstice.hxx"
#include "../Math/Vector.hxx"
#include <vector>
#include <unordered_set>
#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Solstice::Core {

// Navigation polygon (triangle)
struct NavPolygon {
    Math::Vec3 Vertices[3];
    std::vector<size_t> Neighbors; // Indices of neighboring polygons
    size_t ID;

    NavPolygon() : ID(0) {}

    Math::Vec3 GetCenter() const {
        return (Vertices[0] + Vertices[1] + Vertices[2]) * (1.0f / 3.0f);
    }

    bool Contains(const Math::Vec3& Point) const {
        // Barycentric coordinates check
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

// Pathfinding algorithm type
enum class PathfindingAlgorithm {
    AStar,
    ThetaStar
};

// Pathfinding node for A*
struct PathNode {
    size_t PolygonID;
    float GCost; // Cost from start
    float HCost; // Heuristic cost to goal
    float FCost; // Total cost
    size_t ParentID;

    PathNode() : PolygonID(0), GCost(0.0f), HCost(0.0f), FCost(0.0f), ParentID(0) {}

    bool operator<(const PathNode& Other) const {
        return FCost > Other.FCost; // For priority queue (min-heap)
    }
};

// Base pathfinder interface
class Pathfinder {
public:
    virtual ~Pathfinder() = default;
    virtual bool FindPath(const std::vector<NavPolygon>& Polygons, size_t StartID,
                         size_t GoalID, std::vector<size_t>& OutPath) = 0;
};

// A* pathfinding implementation
class AStarPathfinder : public Pathfinder {
public:
    bool FindPath(const std::vector<NavPolygon>& Polygons, size_t StartID,
                 size_t GoalID, std::vector<size_t>& OutPath) override {
        if (StartID >= Polygons.size() || GoalID >= Polygons.size()) {
            return false;
        }

        if (StartID == GoalID) {
            OutPath = {StartID};
            return true;
        }

        std::priority_queue<PathNode> openSet;
        std::unordered_set<size_t> closedSet;
        std::vector<PathNode> nodes(Polygons.size());
        std::vector<size_t> parents(Polygons.size(), SIZE_MAX);

        // Initialize start node
        PathNode startNode;
        startNode.PolygonID = StartID;
        startNode.GCost = 0.0f;
        startNode.HCost = Heuristic(Polygons[StartID], Polygons[GoalID]);
        startNode.FCost = startNode.GCost + startNode.HCost;
        startNode.ParentID = SIZE_MAX;
        nodes[StartID] = startNode;
        openSet.push(startNode);

        while (!openSet.empty()) {
            PathNode current = openSet.top();
            openSet.pop();

            if (closedSet.find(current.PolygonID) != closedSet.end()) {
                continue;
            }

            closedSet.insert(current.PolygonID);

            if (current.PolygonID == GoalID) {
                // Reconstruct path
                size_t nodeID = GoalID;
                while (nodeID != SIZE_MAX) {
                    OutPath.push_back(nodeID);
                    nodeID = parents[nodeID];
                }
                std::reverse(OutPath.begin(), OutPath.end());
                return true;
            }

            const NavPolygon& currentPoly = Polygons[current.PolygonID];
            for (size_t neighborID : currentPoly.Neighbors) {
                if (closedSet.find(neighborID) != closedSet.end()) {
                    continue;
                }

                float tentativeGCost = current.GCost +
                    Distance(currentPoly.GetCenter(), Polygons[neighborID].GetCenter());

                if (tentativeGCost < nodes[neighborID].GCost ||
                    nodes[neighborID].PolygonID == 0) {
                    PathNode neighborNode;
                    neighborNode.PolygonID = neighborID;
                    neighborNode.GCost = tentativeGCost;
                    neighborNode.HCost = Heuristic(Polygons[neighborID], Polygons[GoalID]);
                    neighborNode.FCost = neighborNode.GCost + neighborNode.HCost;
                    neighborNode.ParentID = current.PolygonID;
                    nodes[neighborID] = neighborNode;
                    parents[neighborID] = current.PolygonID;
                    openSet.push(neighborNode);
                }
            }
        }

        return false; // No path found
    }

private:
    float Heuristic(const NavPolygon& A, const NavPolygon& B) const {
        return Distance(A.GetCenter(), B.GetCenter());
    }

    float Distance(const Math::Vec3& A, const Math::Vec3& B) const {
        return A.Distance(B);
    }
};

// Theta* any-angle pathfinding
class ThetaStarPathfinder : public Pathfinder {
public:
    bool FindPath(const std::vector<NavPolygon>& Polygons, size_t StartID,
                 size_t GoalID, std::vector<size_t>& OutPath) override {
        if (StartID >= Polygons.size() || GoalID >= Polygons.size()) {
            return false;
        }

        if (StartID == GoalID) {
            OutPath = {StartID};
            return true;
        }

        std::priority_queue<PathNode> openSet;
        std::unordered_set<size_t> closedSet;
        std::vector<PathNode> nodes(Polygons.size());
        std::vector<size_t> parents(Polygons.size(), SIZE_MAX);

        PathNode startNode;
        startNode.PolygonID = StartID;
        startNode.GCost = 0.0f;
        startNode.HCost = Heuristic(Polygons[StartID], Polygons[GoalID]);
        startNode.FCost = startNode.GCost + startNode.HCost;
        startNode.ParentID = SIZE_MAX;
        nodes[StartID] = startNode;
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

            const NavPolygon& currentPoly = Polygons[current.PolygonID];

            // Check line-of-sight to parent
            size_t parentID = current.ParentID;
            if (parentID != SIZE_MAX && HasLineOfSight(Polygons, parentID, current.PolygonID)) {
                float directCost = nodes[parentID].GCost +
                    Distance(Polygons[parentID].GetCenter(), currentPoly.GetCenter());
                if (directCost < current.GCost) {
                    current.GCost = directCost;
                    current.FCost = current.GCost + current.HCost;
                    parents[current.PolygonID] = parentID;
                    nodes[current.PolygonID] = current;
                }
            }

            for (size_t neighborID : currentPoly.Neighbors) {
                if (closedSet.find(neighborID) != closedSet.end()) {
                    continue;
                }

                float tentativeGCost = current.GCost +
                    Distance(currentPoly.GetCenter(), Polygons[neighborID].GetCenter());

                if (tentativeGCost < nodes[neighborID].GCost ||
                    nodes[neighborID].PolygonID == 0) {
                    PathNode neighborNode;
                    neighborNode.PolygonID = neighborID;
                    neighborNode.GCost = tentativeGCost;
                    neighborNode.HCost = Heuristic(Polygons[neighborID], Polygons[GoalID]);
                    neighborNode.FCost = neighborNode.GCost + neighborNode.HCost;
                    neighborNode.ParentID = current.PolygonID;
                    nodes[neighborID] = neighborNode;
                    parents[neighborID] = current.PolygonID;
                    openSet.push(neighborNode);
                }
            }
        }

        return false;
    }

private:
    bool HasLineOfSight(const std::vector<NavPolygon>& Polygons, size_t FromID, size_t ToID) const {
        // Simplified line-of-sight check
        // In a full implementation, this would check if the line crosses polygon boundaries
        const NavPolygon& from = Polygons[FromID];
        const NavPolygon& to = Polygons[ToID];

        // Check if polygons are neighbors (direct connection)
        for (size_t neighborID : from.Neighbors) {
            if (neighborID == ToID) {
                return true;
            }
        }

        // For now, assume line-of-sight if they share a neighbor
        for (size_t neighborID : from.Neighbors) {
            for (size_t toNeighborID : to.Neighbors) {
                if (neighborID == toNeighborID) {
                    return true;
                }
            }
        }

        return false;
    }

    float Heuristic(const NavPolygon& A, const NavPolygon& B) const {
        return Distance(A.GetCenter(), B.GetCenter());
    }

    float Distance(const Math::Vec3& A, const Math::Vec3& B) const {
        return A.Distance(B);
    }
};

// Navigation mesh container
class SOLSTICE_API NavMesh {
public:
    NavMesh() : m_Algorithm(PathfindingAlgorithm::AStar) {
        m_Pathfinder = std::make_unique<AStarPathfinder>();
    }

    void SetAlgorithm(PathfindingAlgorithm Algorithm) {
        m_Algorithm = Algorithm;
        if (Algorithm == PathfindingAlgorithm::AStar) {
            m_Pathfinder = std::make_unique<AStarPathfinder>();
        } else {
            m_Pathfinder = std::make_unique<ThetaStarPathfinder>();
        }
    }

    void BuildFromGeometry(const std::vector<Math::Vec3>& Vertices,
                          const std::vector<uint32_t>& Indices) {
        m_Polygons.clear();

        // Create polygons from triangles
        for (size_t i = 0; i < Indices.size(); i += 3) {
            NavPolygon poly;
            poly.Vertices[0] = Vertices[Indices[i]];
            poly.Vertices[1] = Vertices[Indices[i + 1]];
            poly.Vertices[2] = Vertices[Indices[i + 2]];
            poly.ID = m_Polygons.size();
            m_Polygons.push_back(poly);
        }

        // Build neighbor connections
        for (size_t i = 0; i < m_Polygons.size(); ++i) {
            for (size_t j = i + 1; j < m_Polygons.size(); ++j) {
                if (AreNeighbors(m_Polygons[i], m_Polygons[j])) {
                    m_Polygons[i].Neighbors.push_back(j);
                    m_Polygons[j].Neighbors.push_back(i);
                }
            }
        }
    }

    bool FindPath(const Math::Vec3& Start, const Math::Vec3& End,
                 std::vector<Math::Vec3>& OutPath) {
        size_t startID = FindNearestPolygon(Start);
        size_t endID = FindNearestPolygon(End);

        if (startID == SIZE_MAX || endID == SIZE_MAX) {
            return false;
        }

        std::vector<size_t> polygonPath;
        if (!m_Pathfinder->FindPath(m_Polygons, startID, endID, polygonPath)) {
            return false;
        }

        OutPath.clear();
        for (size_t polyID : polygonPath) {
            OutPath.push_back(m_Polygons[polyID].GetCenter());
        }

        return true;
    }

    size_t FindNearestPolygon(const Math::Vec3& Point) const {
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

    bool Raycast(const Math::Vec3& Origin, const Math::Vec3& Direction,
                float MaxDistance, Math::Vec3& OutHit) const {
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

private:
    bool AreNeighbors(const NavPolygon& A, const NavPolygon& B) const {
        // Check if polygons share an edge
        int sharedVertices = 0;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (A.Vertices[i].Distance(B.Vertices[j]) < 0.001f) {
                    sharedVertices++;
                }
            }
        }
        return sharedVertices >= 2; // Share an edge
    }

    bool RayTriangleIntersect(const Math::Vec3& Origin, const Math::Vec3& Direction,
                             const NavPolygon& Poly, Math::Vec3& OutHit) const {
        // Möller-Trumbore intersection algorithm
        const float EPSILON = 0.0000001f;
        Math::Vec3 edge1 = Poly.Vertices[1] - Poly.Vertices[0];
        Math::Vec3 edge2 = Poly.Vertices[2] - Poly.Vertices[0];
        Math::Vec3 h = Direction.Cross(edge2);
        float a = edge1.Dot(h);

        if (a > -EPSILON && a < EPSILON) {
            return false; // Ray is parallel to triangle
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

    std::vector<NavPolygon> m_Polygons;
    PathfindingAlgorithm m_Algorithm;
    std::unique_ptr<Pathfinder> m_Pathfinder;
};

} // namespace Solstice::Core

