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
    Triangle(const Math::Vec3& V0, const Math::Vec3& V1, const Math::Vec3& V2, uint32_t Index = 0)
        : OriginalIndex(Index) {
        Vertices[0] = V0;
        Vertices[1] = V1;
        Vertices[2] = V2;
    }

    Math::Vec3 GetNormal() const {
        Math::Vec3 Edge1 = Vertices[1] - Vertices[0];
        Math::Vec3 Edge2 = Vertices[2] - Vertices[0];
        return Edge1.Cross(Edge2).Normalized();
    }

    Math::Vec3 GetCentroid() const {
        return (Vertices[0] + Vertices[1] + Vertices[2]) * (1.0f / 3.0f);
    }

    bool IntersectRay(const Math::Vec3& Origin, const Math::Vec3& Direction, float& T, float& U, float& V) const {
        const float Epsilon = 1e-6f;
        Math::Vec3 Edge1 = Vertices[1] - Vertices[0];
        Math::Vec3 Edge2 = Vertices[2] - Vertices[0];
        Math::Vec3 H = Direction.Cross(Edge2);
        float A = Edge1.Dot(H);
        if (A > -Epsilon && A < Epsilon) return false;
        float F = 1.0f / A;
        Math::Vec3 S = Origin - Vertices[0];
        U = F * S.Dot(H);
        if (U < 0.0f || U > 1.0f) return false;
        Math::Vec3 Q = S.Cross(Edge1);
        V = F * Direction.Dot(Q);
        if (V < 0.0f || U + V > 1.0f) return false;
        T = F * Edge2.Dot(Q);
        return T > Epsilon;
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
    void Build(const std::vector<Triangle>& TrianglesParam, int MaxDepth = 20, int MinTriangles = 4) {
        MMaxDepth = MaxDepth;
        MMinTriangles = MinTriangles;
        MRoot = BuildRecursive(TrianglesParam, 0);
    }
    
    // Traverse the tree front-to-back relative to the position
    template<typename Func>
    void Traverse(const Math::Vec3& Position, Func&& Visitor) const {
        TraverseRecursive(MRoot.get(), Position, Visitor);
    }

    // Intersect a ray with the BSP tree
    bool IntersectRay(const Math::Vec3& Origin, const Math::Vec3& Direction, float& OutT, Triangle& OutTriangle) const {
        return IntersectRayRecursive(MRoot.get(), Origin, Direction, OutT, OutTriangle);
    }

    const BSPNode* GetRoot() const { return MRoot.get(); }

private:
    std::unique_ptr<BSPNode> MRoot;
    int MMaxDepth = 20;
    int MMinTriangles = 4;

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
        
        float D0 = ClassifyPoint(Tri.Vertices[0], Plane);
        float D1 = ClassifyPoint(Tri.Vertices[1], Plane);
        float D2 = ClassifyPoint(Tri.Vertices[2], Plane);

        int FrontCount = 0, BackCount = 0, CoplanarCount = 0;
        
        if (D0 > Epsilon) FrontCount++; else if (D0 < -Epsilon) BackCount++; else CoplanarCount++;
        if (D1 > Epsilon) FrontCount++; else if (D1 < -Epsilon) BackCount++; else CoplanarCount++;
        if (D2 > Epsilon) FrontCount++; else if (D2 < -Epsilon) BackCount++; else CoplanarCount++;

        if (CoplanarCount == 3) return Classification::Coplanar;
        if (FrontCount > 0 && BackCount == 0) return Classification::Front;
        if (BackCount > 0 && FrontCount == 0) return Classification::Back;
        return Classification::Spanning;
    }

    // Split a triangle by a plane
    void SplitTriangle(const Triangle& Tri, const Math::Vec4& Plane,
                      std::vector<Triangle>& FrontTris, std::vector<Triangle>& BackTris) const {
        static constexpr float Epsilon = 1e-4f;
        
        float Distances[3];
        for (int I = 0; I < 3; ++I) {
            Distances[I] = ClassifyPoint(Tri.Vertices[I], Plane);
        }

        std::vector<Math::Vec3> FrontVerts, BackVerts;

        for (int I = 0; I < 3; ++I) {
            int Next = (I + 1) % 3;
            
            const Math::Vec3& V0 = Tri.Vertices[I];
            const Math::Vec3& V1 = Tri.Vertices[Next];
            float D0 = Distances[I];
            float D1 = Distances[Next];

            // Add current vertex to appropriate list
            if (D0 > Epsilon) {
                FrontVerts.push_back(V0);
            } else if (D0 < -Epsilon) {
                BackVerts.push_back(V0);
            } else {
                FrontVerts.push_back(V0);
                BackVerts.push_back(V0);
            }

            // Check if edge crosses the plane
            if ((D0 > Epsilon && D1 < -Epsilon) || (D0 < -Epsilon && D1 > Epsilon)) {
                float T = D0 / (D0 - D1);
                Math::Vec3 Intersection = V0 + (V1 - V0) * T;
                FrontVerts.push_back(Intersection);
                BackVerts.push_back(Intersection);
            }
        }

        // Create triangles from the vertex lists
        if (FrontVerts.size() >= 3) {
            FrontTris.push_back(Triangle(FrontVerts[0], FrontVerts[1], FrontVerts[2], Tri.OriginalIndex));
            if (FrontVerts.size() == 4) {
                FrontTris.push_back(Triangle(FrontVerts[0], FrontVerts[2], FrontVerts[3], Tri.OriginalIndex));
            }
        }

        if (BackVerts.size() >= 3) {
            BackTris.push_back(Triangle(BackVerts[0], BackVerts[1], BackVerts[2], Tri.OriginalIndex));
            if (BackVerts.size() == 4) {
                BackTris.push_back(Triangle(BackVerts[0], BackVerts[2], BackVerts[3], Tri.OriginalIndex));
            }
        }
    }

    // Select the best splitting plane using a heuristic
    Math::Vec4 SelectSplittingPlane(const std::vector<Triangle>& TrianglesParam) const {
        if (TrianglesParam.empty()) {
            return Math::Vec4(0, 1, 0, 0); // Default plane
        }

        const Triangle& Tri = TrianglesParam[0];
        Math::Vec3 Normal = Tri.GetNormal();
        float DVal = -Normal.Dot(Tri.Vertices[0]);
        
        return Math::Vec4(Normal.x, Normal.y, Normal.z, DVal);
    }

    // Recursive BSP tree construction
    std::unique_ptr<BSPNode> BuildRecursive(const std::vector<Triangle>& TrianglesParam, int Depth) {
        if (TrianglesParam.empty()) {
            return nullptr;
        }

        auto Node = std::make_unique<BSPNode>();

        // Stop recursion if we've reached max depth or have few triangles
        if (Depth >= MMaxDepth || (int)TrianglesParam.size() <= MMinTriangles) {
            Node->Triangles = TrianglesParam;
            return Node;
        }

        // Select splitting plane
        Node->Plane = SelectSplittingPlane(TrianglesParam);

        std::vector<Triangle> FrontTris, BackTris;

        // Partition triangles
        for (const Triangle& Tri : TrianglesParam) {
            Classification Cls = ClassifyTriangle(Tri, Node->Plane);

            switch (Cls) {
                case Classification::Front:
                    FrontTris.push_back(Tri);
                    break;
                case Classification::Back:
                    BackTris.push_back(Tri);
                    break;
                case Classification::Coplanar:
                    // Keep coplanar triangles in this node
                    Node->Triangles.push_back(Tri);
                    break;
                case Classification::Spanning:
                    // Split the triangle
                    SplitTriangle(Tri, Node->Plane, FrontTris, BackTris);
                    break;
            }
        }

        // Recursively build child nodes
        if (!FrontTris.empty()) {
            Node->Front = BuildRecursive(FrontTris, Depth + 1);
        }
        if (!BackTris.empty()) {
            Node->Back = BuildRecursive(BackTris, Depth + 1);
        }

        return Node;
    }

    template<typename Func>
    void TraverseRecursive(const BSPNode* Node, const Math::Vec3& Position, Func&& Visitor) const {
        if (!Node) return;

        if (Node->IsLeaf()) {
            for (const Triangle& Tri : Node->Triangles) {
                Visitor(Tri);
            }
            return;
        }

        for (const Triangle& Tri : Node->Triangles) {
            Visitor(Tri);
        }

        float Dist = Node->Plane.x * Position.x + 
                     Node->Plane.y * Position.y + 
                     Node->Plane.z * Position.z + 
                     Node->Plane.w;

        if (Dist > 0) {
            TraverseRecursive(Node->Back.get(), Position, Visitor);
            TraverseRecursive(Node->Front.get(), Position, Visitor);
        } else {
            TraverseRecursive(Node->Front.get(), Position, Visitor);
            TraverseRecursive(Node->Back.get(), Position, Visitor);
        }
    }

    bool IntersectRayRecursive(const BSPNode* Node, const Math::Vec3& Origin, const Math::Vec3& Direction, float& T, Triangle& OutTriangle) const {
        if (!Node) return false;

        bool Hit = false;
        float MinT = std::numeric_limits<float>::max();

        // Check triangles in this node
        for (const auto& Tri : Node->Triangles) {
            float CurrentT, U, V;
            if (Tri.IntersectRay(Origin, Direction, CurrentT, U, V)) {
                if (CurrentT < MinT) {
                    MinT = CurrentT;
                    OutTriangle = Tri;
                    Hit = true;
                }
            }
        }

        if (Node->IsLeaf()) {
            T = MinT;
            return Hit;
        }

        float DistOrigin = ClassifyPoint(Origin, Node->Plane);
        float DistDir = Node->Plane.x * Direction.x + Node->Plane.y * Direction.y + Node->Plane.z * Direction.z;

        if (std::abs(DistDir) < 1e-6f) {
            // Ray parallel to plane
            if (DistOrigin > 0) return IntersectRayRecursive(Node->Front.get(), Origin, Direction, T, OutTriangle);
            else return IntersectRayRecursive(Node->Back.get(), Origin, Direction, T, OutTriangle);
        }

        float TPlane = -DistOrigin / DistDir;

        if (DistOrigin > 0) {
            // Origin in front
            if (IntersectRayRecursive(Node->Front.get(), Origin, Direction, T, OutTriangle)) return true;
            if (TPlane > 0 && IntersectRayRecursive(Node->Back.get(), Origin, Direction, T, OutTriangle)) return true;
        } else {
            // Origin in back
            if (IntersectRayRecursive(Node->Back.get(), Origin, Direction, T, OutTriangle)) return true;
            if (TPlane > 0 && IntersectRayRecursive(Node->Front.get(), Origin, Direction, T, OutTriangle)) return true;
        }

        T = MinT;
        return Hit;
    }
};

} // namespace Solstice::Core