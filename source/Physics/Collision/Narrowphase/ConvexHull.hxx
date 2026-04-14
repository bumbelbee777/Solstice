#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <cstdint>

namespace Solstice::Physics {

/// Face of a convex hull (triangle or polygon)
struct HullFace {
    std::vector<uint32_t> VertexIndices;  // Indices into ConvexHull::Vertices
    Math::Vec3 Normal;                     // Outward-facing normal
    float PlaneDistance;                   // Distance from origin along normal
    
    HullFace() : Normal(0, 1, 0), PlaneDistance(0.0f) {}
};

/// Edge of a convex hull
struct HullEdge {
    uint32_t VertexA;
    uint32_t VertexB;
    uint32_t FaceA;  // Adjacent face index
    uint32_t FaceB;  // Adjacent face index (-1 if boundary)
    
    HullEdge() : VertexA(0), VertexB(0), FaceA(0), FaceB(static_cast<uint32_t>(-1)) {}
};

/// Convex hull representation for collision detection
/// Contains vertices, faces, and edge adjacency information
struct ConvexHull {
    std::vector<Math::Vec3> Vertices;   // Local-space vertices
    std::vector<HullFace> Faces;        // Faces with normals
    std::vector<HullEdge> Edges;        // Edge adjacency
    
    Math::Vec3 Centroid;                // Center of mass (local space)
    
    ConvexHull() : Centroid(0, 0, 0) {}
    
    /// Get support point in local space (furthest point along direction)
    Math::Vec3 GetSupportPoint(const Math::Vec3& direction) const {
        if (Vertices.empty()) return Math::Vec3(0, 0, 0);
        
        float maxDot = direction.Dot(Vertices[0]);
        Math::Vec3 maxPoint = Vertices[0];
        
        for (size_t i = 1; i < Vertices.size(); ++i) {
            float dot = direction.Dot(Vertices[i]);
            if (dot > maxDot) {
                maxDot = dot;
                maxPoint = Vertices[i];
            }
        }
        
        return maxPoint;
    }
    
    /// Get number of vertices
    size_t GetVertexCount() const { return Vertices.size(); }
    
    /// Get number of faces
    size_t GetFaceCount() const { return Faces.size(); }
    
    /// Get number of edges
    size_t GetEdgeCount() const { return Edges.size(); }
};

} // namespace Solstice::Physics
