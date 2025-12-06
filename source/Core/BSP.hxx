#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <memory>
#include <algorithm>
#include <limits>

namespace Solstice::Core {

struct Triangle {
    Math::Vec3 Vertices[3];
    uint32_t OriginalIndex;

    Triangle() : OriginalIndex(0) {}
    Triangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, uint32_t idx = 0)
        : OriginalIndex(idx) {
        Vertices[0] = v0;
        Vertices[1] = v1;
        Vertices[2] = v2;
    }

    Math::Vec3 GetNormal() const {
        Math::Vec3 edge1 = Vertices[1] - Vertices[0];
        Math::Vec3 edge2 = Vertices[2] - Vertices[0];
        return edge1.Cross(edge2).Normalized();
    }

    Math::Vec3 GetCentroid() const {
        return (Vertices[0] + Vertices[1] + Vertices[2]) * (1.0f / 3.0f);
    }
};

struct BSPNode {
    // Plane equation: Ax + By + Cz + D = 0
    // Stored as Vec4 (x, y, z, w) where w is D
    Math::Vec4 Plane;
    
    std::unique_ptr<BSPNode> Front;
    std::unique_ptr<BSPNode> Back;
    
    // Leaf data - triangles in this node
    std::vector<Triangle> Triangles;
    
    bool IsLeaf() const { return !Front && !Back; }
};

class BSP {
public:
    BSP() = default;
    
    // Build the BSP tree from a set of triangles
    void Build(const std::vector<Triangle>& Triangles, int MaxDepth = 20, int MinTriangles = 4) {
        m_MaxDepth = MaxDepth;
        m_MinTriangles = MinTriangles;
        m_Root = BuildRecursive(Triangles, 0);
    }
    
    // Traverse the tree front-to-back relative to the camera position
    template<typename Func>
    void Traverse(const Math::Vec3& CameraPos, Func&& Visitor) const {
        TraverseRecursive(m_Root.get(), CameraPos, Visitor);
    }

    const BSPNode* GetRoot() const { return m_Root.get(); }

private:
    std::unique_ptr<BSPNode> m_Root;
    int m_MaxDepth = 20;
    int m_MinTriangles = 4;

    enum class Classification {
        Front,
        Back,
        Coplanar,
        Spanning
    };

    // Classify a point relative to a plane
    float ClassifyPoint(const Math::Vec3& Point, const Math::Vec4& Plane) const {
        return Plane.x * Point.x + Plane.y * Point.y + Plane.z * Point.z + Plane.w;
    }

    // Classify a triangle relative to a plane
    Classification ClassifyTriangle(const Triangle& Tri, const Math::Vec4& Plane) const {
        static constexpr float Epsilon = 1e-4f;
        
        float d0 = ClassifyPoint(Tri.Vertices[0], Plane);
        float d1 = ClassifyPoint(Tri.Vertices[1], Plane);
        float d2 = ClassifyPoint(Tri.Vertices[2], Plane);

        int front = 0, back = 0, coplanar = 0;
        
        if (d0 > Epsilon) front++; else if (d0 < -Epsilon) back++; else coplanar++;
        if (d1 > Epsilon) front++; else if (d1 < -Epsilon) back++; else coplanar++;
        if (d2 > Epsilon) front++; else if (d2 < -Epsilon) back++; else coplanar++;

        if (coplanar == 3) return Classification::Coplanar;
        if (front > 0 && back == 0) return Classification::Front;
        if (back > 0 && front == 0) return Classification::Back;
        return Classification::Spanning;
    }

    // Split a triangle by a plane
    void SplitTriangle(const Triangle& Tri, const Math::Vec4& Plane,
                      std::vector<Triangle>& FrontTris, std::vector<Triangle>& BackTris) const {
        static constexpr float Epsilon = 1e-4f;
        
        float distances[3];
        for (int i = 0; i < 3; ++i) {
            distances[i] = ClassifyPoint(Tri.Vertices[i], Plane);
        }

        std::vector<Math::Vec3> frontVerts, backVerts;

        for (int i = 0; i < 3; ++i) {
            int next = (i + 1) % 3;
            
            const Math::Vec3& v0 = Tri.Vertices[i];
            const Math::Vec3& v1 = Tri.Vertices[next];
            float d0 = distances[i];
            float d1 = distances[next];

            // Add current vertex to appropriate list
            if (d0 > Epsilon) {
                frontVerts.push_back(v0);
            } else if (d0 < -Epsilon) {
                backVerts.push_back(v0);
            } else {
                frontVerts.push_back(v0);
                backVerts.push_back(v0);
            }

            // Check if edge crosses the plane
            if ((d0 > Epsilon && d1 < -Epsilon) || (d0 < -Epsilon && d1 > Epsilon)) {
                float t = d0 / (d0 - d1);
                Math::Vec3 intersection = v0 + (v1 - v0) * t;
                frontVerts.push_back(intersection);
                backVerts.push_back(intersection);
            }
        }

        // Create triangles from the vertex lists
        if (frontVerts.size() >= 3) {
            FrontTris.push_back(Triangle(frontVerts[0], frontVerts[1], frontVerts[2], Tri.OriginalIndex));
            if (frontVerts.size() == 4) {
                FrontTris.push_back(Triangle(frontVerts[0], frontVerts[2], frontVerts[3], Tri.OriginalIndex));
            }
        }

        if (backVerts.size() >= 3) {
            BackTris.push_back(Triangle(backVerts[0], backVerts[1], backVerts[2], Tri.OriginalIndex));
            if (backVerts.size() == 4) {
                BackTris.push_back(Triangle(backVerts[0], backVerts[2], backVerts[3], Tri.OriginalIndex));
            }
        }
    }

    // Select the best splitting plane using a heuristic
    Math::Vec4 SelectSplittingPlane(const std::vector<Triangle>& Triangles) const {
        if (Triangles.empty()) {
            return Math::Vec4(0, 1, 0, 0); // Default plane
        }

        // Simple heuristic: use the first triangle's plane
        // More sophisticated: evaluate multiple candidates and pick the one with best balance
        const Triangle& tri = Triangles[0];
        Math::Vec3 normal = tri.GetNormal();
        float d = -normal.Dot(tri.Vertices[0]);
        
        return Math::Vec4(normal.x, normal.y, normal.z, d);
    }

    // Recursive BSP tree construction
    std::unique_ptr<BSPNode> BuildRecursive(const std::vector<Triangle>& Triangles, int Depth) {
        if (Triangles.empty()) {
            return nullptr;
        }

        auto node = std::make_unique<BSPNode>();

        // Stop recursion if we've reached max depth or have few triangles
        if (Depth >= m_MaxDepth || (int)Triangles.size() <= m_MinTriangles) {
            node->Triangles = Triangles;
            return node;
        }

        // Select splitting plane
        node->Plane = SelectSplittingPlane(Triangles);

        std::vector<Triangle> frontTris, backTris;

        // Partition triangles
        for (const Triangle& tri : Triangles) {
            Classification cls = ClassifyTriangle(tri, node->Plane);

            switch (cls) {
                case Classification::Front:
                    frontTris.push_back(tri);
                    break;
                case Classification::Back:
                    backTris.push_back(tri);
                    break;
                case Classification::Coplanar:
                    // Keep coplanar triangles in this node
                    node->Triangles.push_back(tri);
                    break;
                case Classification::Spanning:
                    // Split the triangle
                    SplitTriangle(tri, node->Plane, frontTris, backTris);
                    break;
            }
        }

        // Recursively build child nodes
        if (!frontTris.empty()) {
            node->Front = BuildRecursive(frontTris, Depth + 1);
        }
        if (!backTris.empty()) {
            node->Back = BuildRecursive(backTris, Depth + 1);
        }

        return node;
    }

    template<typename Func>
    void TraverseRecursive(const BSPNode* Node, const Math::Vec3& CameraPos, Func&& Visitor) const {
        if (!Node) return;

        if (Node->IsLeaf()) {
            for (const Triangle& tri : Node->Triangles) {
                Visitor(tri);
            }
            return;
        }

        // Process coplanar triangles at this node
        for (const Triangle& tri : Node->Triangles) {
            Visitor(tri);
        }

        // Calculate distance to plane
        float dist = Node->Plane.x * CameraPos.x + 
                     Node->Plane.y * CameraPos.y + 
                     Node->Plane.z * CameraPos.z + 
                     Node->Plane.w;

        if (dist > 0) {
            // Camera is in front of the plane - render back to front
            TraverseRecursive(Node->Back.get(), CameraPos, Visitor);
            TraverseRecursive(Node->Front.get(), CameraPos, Visitor);
        } else {
            // Camera is behind the plane - render back to front
            TraverseRecursive(Node->Front.get(), CameraPos, Visitor);
            TraverseRecursive(Node->Back.get(), CameraPos, Visitor);
        }
    }
};

} // namespace Solstice::Core