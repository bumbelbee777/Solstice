#pragma once

#include "../SolsticeExport.hxx"
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <Render/Material.hxx>
#include <vector>
#include <cstdint>
#include <memory>
#include <algorithm>

namespace Solstice::Render {

// "Quantized" Vertex - actually using 32-bit floats now as requested
// We keep the name to minimize refactoring churn, but it's no longer quantized.
// Layout: Pos(12) + Normal(12) + UV(8) = 32 bytes.
struct alignas(16) QuantizedVertex {
    float PosX, PosY, PosZ;
    float NormalX, NormalY, NormalZ;
    float U, V;

    // Helper to create from float data
    static QuantizedVertex FromFloat(const Math::Vec3& Pos, const Math::Vec3& Normal, 
                                     const Math::Vec2& UV, const Math::Vec3&, 
                                     const Math::Vec3&) {
        QuantizedVertex Vert;
        Vert.PosX = Pos.x;
        Vert.PosY = Pos.y;
        Vert.PosZ = Pos.z;
        Vert.NormalX = Normal.x;
        Vert.NormalY = Normal.y;
        Vert.NormalZ = Normal.z;
        Vert.U = UV.x;
        Vert.V = UV.y;
        return Vert;
    }
    
    // Decode to float
    Math::Vec3 GetPosition(const Math::Vec3&, const Math::Vec3&) const {
        return Math::Vec3(PosX, PosY, PosZ);
    }
    
    Math::Vec3 GetNormal() const {
        return Math::Vec3(NormalX, NormalY, NormalZ);
    }
    
    Math::Vec2 GetUV() const {
        return Math::Vec2(U, V);
    }
};

// Structure of Arrays vertex layout for SIMD processing
struct VertexDataSoA {
    std::vector<float> PosX, PosY, PosZ;
    std::vector<float> NormalX, NormalY, NormalZ;
    std::vector<float> U, V;
    
    void Reserve(size_t Count) {
        PosX.reserve(Count);
        PosY.reserve(Count);
        PosZ.reserve(Count);
        NormalX.reserve(Count);
        NormalY.reserve(Count);
        NormalZ.reserve(Count);
        U.reserve(Count);
        V.reserve(Count);
    }
    
    void AddVertex(const QuantizedVertex& Vert) {
        PosX.push_back(Vert.PosX);
        PosY.push_back(Vert.PosY);
        PosZ.push_back(Vert.PosZ);
        NormalX.push_back(Vert.NormalX);
        NormalY.push_back(Vert.NormalY);
        NormalZ.push_back(Vert.NormalZ);
        U.push_back(Vert.U);
        V.push_back(Vert.V);
    }
    
    size_t Count() const { return PosX.size(); }
};

// Submesh - a group of triangles with the same material
struct SubMesh {
    uint32_t MaterialID;
    uint32_t IndexStart;
    uint32_t IndexCount;
    
    SubMesh() : MaterialID(0), IndexStart(0), IndexCount(0) {}
    SubMesh(uint32_t MatID, uint32_t Start, uint32_t Count)
        : MaterialID(MatID), IndexStart(Start), IndexCount(Count) {}
};

// Mesh - contains geometry and material assignments
class Mesh {
public:
    Mesh() = default;
    
    // Bounds (kept for API compatibility, though less critical for floats)
    Math::Vec3 BoundsMin;
    Math::Vec3 BoundsMax;
    
    // Vertex data (AoS - tightly packed)
    std::vector<QuantizedVertex> Vertices;
    
    // Index buffer
    std::vector<uint32_t> Indices;
    
    // Submeshes (material groups)
    std::vector<SubMesh> SubMeshes;
    
    // LOD levels
    struct LODLevel {
        uint32_t IndexStart;
        uint32_t IndexCount;
        float ScreenSizeThreshold;
    };
    std::vector<LODLevel> LODs;
    
    // Add a vertex
    void AddVertex(const Math::Vec3& Pos, const Math::Vec3& Normal, const Math::Vec2& UV) {
        Vertices.push_back(QuantizedVertex::FromFloat(Pos, Normal, UV, BoundsMin, BoundsMax));
    }
    
    // Add a triangle
    void AddTriangle(uint32_t I0, uint32_t I1, uint32_t I2) {
        Indices.push_back(I0);
        Indices.push_back(I1);
        Indices.push_back(I2);
    }
    
    // Add a submesh
    void AddSubMesh(uint32_t MaterialID, uint32_t IndexStart, uint32_t IndexCount) {
        SubMeshes.emplace_back(MaterialID, IndexStart, IndexCount);
    }
    
    // Calculate bounds from vertices
    void CalculateBounds() {
        if (Vertices.empty()) {
            BoundsMin = Math::Vec3(0, 0, 0);
            BoundsMax = Math::Vec3(0, 0, 0);
            return;
        }
        // No-op for floats really, but good to have
    }
    
    // Convert to SoA for SIMD processing
    VertexDataSoA ToSoA() const {
        VertexDataSoA SoA;
        SoA.Reserve(Vertices.size());
        for (const QuantizedVertex& Vert : Vertices) {
            SoA.AddVertex(Vert);
        }
        return SoA;
    }
    
    size_t GetVertexCount() const { return Vertices.size(); }
    size_t GetTriangleCount() const { return Indices.size() / 3; }
    size_t GetSubMeshCount() const { return SubMeshes.size(); }
};

// Mesh instance - references a mesh with transform and material overrides
struct MeshInstance {
    uint32_t MeshID;
    Math::Matrix4 Transform;
    
    // Optional material overrides per submesh
    std::vector<uint32_t> MaterialOverrides;
    
    // LOD selection
    uint8_t CurrentLOD;
    
    // Flags
    uint16_t Flags;
    
    MeshInstance() : MeshID(0), CurrentLOD(0), Flags(0) {
        Transform = Math::Matrix4::Identity();
    }
};

// Mesh library/manager
class MeshLibrary {
public:
    MeshLibrary() = default;
    
    uint32_t AddMesh(std::unique_ptr<Mesh> MeshPtr) {
        uint32_t ID = static_cast<uint32_t>(m_Meshes.size());
        m_Meshes.push_back(std::move(MeshPtr));
        return ID;
    }
    
    Mesh* GetMesh(uint32_t ID) {
        if (ID >= m_Meshes.size()) return nullptr;
        return m_Meshes[ID].get();
    }
    
    const Mesh* GetMesh(uint32_t ID) const {
        if (ID >= m_Meshes.size()) return nullptr;
        return m_Meshes[ID].get();
    }
    
    size_t GetMeshCount() const { return m_Meshes.size(); }

private:
    std::vector<std::unique_ptr<Mesh>> m_Meshes;
};

// Helper functions for creating common meshes
namespace MeshFactory {
SOLSTICE_API std::unique_ptr<Mesh> CreateCube(float Size = 1.0f);
SOLSTICE_API std::unique_ptr<Mesh> CreatePlane(float Width = 1.0f, float Height = 1.0f);
SOLSTICE_API std::unique_ptr<Mesh> CreateSphere(float Radius = 1.0f, int Segments = 16);
SOLSTICE_API std::unique_ptr<Mesh> CreateTetrahedron(float Size = 1.0f);
SOLSTICE_API std::unique_ptr<Mesh> CreateCylinder(float Radius = 0.5f, float Height = 1.0f, int Segments = 16);
} // namespace MeshFactory

} // namespace Solstice::Render

static_assert(sizeof(Solstice::Render::QuantizedVertex) == 32, "QuantizedVertex must be 32 bytes");
static_assert(alignof(Solstice::Render::QuantizedVertex) == 16, "QuantizedVertex must stay 16-byte aligned");