#include <Physics/Content/ConvexHullFactory.hxx>
#include <Core/Debug/Debug.hxx>
#include <unordered_set>
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {

// Helper: Hash function for Vec3 to deduplicate vertices
struct Vec3Hash {
    size_t operator()(const Math::Vec3& v) const {
        // Simple hash combining x, y, z
        size_t hx = std::hash<float>{}(v.x);
        size_t hy = std::hash<float>{}(v.y);
        size_t hz = std::hash<float>{}(v.z);
        return hx ^ (hy << 1) ^ (hz << 2);
    }
};

struct Vec3Equal {
    bool operator()(const Math::Vec3& a, const Math::Vec3& b) const {
        const float epsilon = 1e-6f;
        return std::abs(a.x - b.x) < epsilon &&
               std::abs(a.y - b.y) < epsilon &&
               std::abs(a.z - b.z) < epsilon;
    }
};

std::shared_ptr<ConvexHull> GenerateConvexHull(const Render::Mesh& mesh) {
    auto hull = std::make_shared<ConvexHull>();

    if (mesh.Vertices.empty()) {
        SIMPLE_LOG("Warning: GenerateConvexHull called on empty mesh");
        return hull;
    }

    // Step 1: Extract unique vertices
    std::unordered_set<Math::Vec3, Vec3Hash, Vec3Equal> uniqueVertices;

    for (const auto& vertex : mesh.Vertices) {
        Math::Vec3 pos = vertex.GetPosition(mesh.BoundsMin, mesh.BoundsMax);
        uniqueVertices.insert(pos);
    }

    // Copy unique vertices to hull
    hull->Vertices.reserve(uniqueVertices.size());
    for (const auto& v : uniqueVertices) {
        hull->Vertices.push_back(v);
    }

    SIMPLE_LOG("ConvexHull: Extracted " + std::to_string(hull->Vertices.size()) + " unique vertices");

    // Step 2: For simple meshes (tetrahedron, etc.), just use the mesh faces directly
    // Full convex hull algorithm (QuickHull, Gift Wrapping) is complex
    // For now, we'll use a simplified approach: assume input mesh IS convex

    // Extract faces from mesh indices
    for (size_t i = 0; i < mesh.Indices.size(); i += 3) {
        uint32_t i0 = mesh.Indices[i];
        uint32_t i1 = mesh.Indices[i + 1];
        uint32_t i2 = mesh.Indices[i + 2];

        if (i0 >= mesh.Vertices.size() || i1 >= mesh.Vertices.size() || i2 >= mesh.Vertices.size()) {
            continue;  // Invalid index
        }

        Math::Vec3 v0 = mesh.Vertices[i0].GetPosition(mesh.BoundsMin, mesh.BoundsMax);
        Math::Vec3 v1 = mesh.Vertices[i1].GetPosition(mesh.BoundsMin, mesh.BoundsMax);
        Math::Vec3 v2 = mesh.Vertices[i2].GetPosition(mesh.BoundsMin, mesh.BoundsMax);

        // Find indices in hull vertices
        auto findIndex = [&](const Math::Vec3& target) -> uint32_t {
            for (uint32_t j = 0; j < hull->Vertices.size(); ++j) {
                if (Vec3Equal{}(hull->Vertices[j], target)) {
                    return j;
                }
            }
            return 0;  // Shouldn't happen
        };

        HullFace face;
        face.VertexIndices.push_back(findIndex(v0));
        face.VertexIndices.push_back(findIndex(v1));
        face.VertexIndices.push_back(findIndex(v2));

        // Compute face normal
        Math::Vec3 edge1 = v1 - v0;
        Math::Vec3 edge2 = v2 - v0;
        face.Normal = edge1.Cross(edge2).Normalized();
        face.PlaneDistance = face.Normal.Dot(v0);

        hull->Faces.push_back(face);
    }

    SIMPLE_LOG("ConvexHull: Generated " + std::to_string(hull->Faces.size()) + " faces");

    // Step 3: Compute properties
    ComputeHullProperties(*hull);

    return hull;
}

void SimplifyHull(ConvexHull& hull, int maxVertices) {
    // Simple greedy vertex removal
    // For production, use more sophisticated algorithms
    // For now, just keep the first N vertices
    if ((int)hull.Vertices.size() > maxVertices) {
        hull.Vertices.resize(maxVertices);
        // Note: This invalidates faces! Would need to rebuild faces
        SIMPLE_LOG("Warning: SimplifyHull not fully implemented");
    }
}

void ComputeHullProperties(ConvexHull& hull) {
    // Compute centroid
    if (!hull.Vertices.empty()) {
        Math::Vec3 sum(0, 0, 0);
        for (const auto& v : hull.Vertices) {
            sum += v;
        }
        hull.Centroid = sum * (1.0f / hull.Vertices.size());
    }

    // Build edge list from faces
    hull.Edges.clear();

    struct EdgeKey {
        uint32_t a, b;  // a < b
        EdgeKey(uint32_t x, uint32_t y) {
            if (x < y) { a = x; b = y; }
            else { a = y; b = x; }
        }
        bool operator==(const EdgeKey& other) const {
            return a == other.a && b == other.b;
        }
    };

    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const {
            return std::hash<uint32_t>{}(k.a) ^ (std::hash<uint32_t>{}(k.b) << 1);
        }
    };

    std::unordered_map<EdgeKey, std::vector<uint32_t>, EdgeKeyHash> edgeToFaces;

    for (uint32_t faceIdx = 0; faceIdx < hull.Faces.size(); ++faceIdx) {
        const auto& face = hull.Faces[faceIdx];
        for (size_t i = 0; i < face.VertexIndices.size(); ++i) {
            uint32_t v0 = face.VertexIndices[i];
            uint32_t v1 = face.VertexIndices[(i + 1) % face.VertexIndices.size()];

            EdgeKey key(v0, v1);
            edgeToFaces[key].push_back(faceIdx);
        }
    }

    // Create edges
    for (const auto& pair : edgeToFaces) {
        HullEdge edge;
        edge.VertexA = pair.first.a;
        edge.VertexB = pair.first.b;

        if (pair.second.size() >= 1) edge.FaceA = pair.second[0];
        if (pair.second.size() >= 2) edge.FaceB = pair.second[1];

        hull.Edges.push_back(edge);
    }

    SIMPLE_LOG("ConvexHull: Computed " + std::to_string(hull.Edges.size()) + " edges");
}

} // namespace Solstice::Physics
