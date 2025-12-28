#include "MeshFactory.hxx"
#include <Render/Mesh.hxx>
#include "../Solstice.hxx"
#include "AssetBuilder.hxx"
#include "MeshData.hxx"
#include <memory>
#include <Core/Debug.hxx>
#include <string>
#include <map>
#include <array>
#include <vector>
#include <cmath>

namespace Solstice::Arzachel {

namespace MeshFactory {

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateCube(float Size) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float Half = Size * 0.5f;

    MeshPtr->BoundsMin = Math::Vec3(-Half, -Half, -Half);
    MeshPtr->BoundsMax = Math::Vec3(Half, Half, Half);

    // 8 corner positions
    Math::Vec3 P[8] = {
        Math::Vec3(-Half, -Half, -Half), Math::Vec3(Half, -Half, -Half),
        Math::Vec3(Half, Half, -Half),   Math::Vec3(-Half, Half, -Half),
        Math::Vec3(-Half, -Half, Half),  Math::Vec3(Half, -Half, Half),
        Math::Vec3(Half, Half, Half),    Math::Vec3(-Half, Half, Half)
    };

    // 6 face normals
    Math::Vec3 N[6] = {
        Math::Vec3(0, 0, -1), Math::Vec3(0, 0, 1),   // Front, Back
        Math::Vec3(0, -1, 0), Math::Vec3(0, 1, 0),   // Bottom, Top
        Math::Vec3(-1, 0, 0), Math::Vec3(1, 0, 0)    // Left, Right
    };

    // UVs
    Math::Vec2 UV[4] = {
        Math::Vec2(0, 0), Math::Vec2(1, 0),
        Math::Vec2(1, 1), Math::Vec2(0, 1)
    };

    // Face indices (CCW)
    uint32_t FaceIndices[6][4] = {
        {0, 1, 2, 3}, // Front (Z-)
        {5, 4, 7, 6}, // Back (Z+)
        {4, 5, 1, 0}, // Bottom (Y-)
        {3, 2, 6, 7}, // Top (Y+)
        {4, 0, 3, 7}, // Left (X-)
        {1, 5, 6, 2}  // Right (X+)
    };

    for (int F = 0; F < 6; ++F) {
        // Add 4 vertices per face
        for (int I = 0; I < 4; ++I) {
            MeshPtr->AddVertex(P[FaceIndices[F][I]], N[F], UV[I]);
        }

        // Add 2 triangles
        uint32_t Base = F * 4;
        MeshPtr->AddTriangle(Base + 0, Base + 1, Base + 2);
        MeshPtr->AddTriangle(Base + 0, Base + 2, Base + 3);
    }

    MeshPtr->AddSubMesh(0, 0, 6 * 2 * 3); // 6 faces, 2 tris, 3 indices

    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreatePlane(float Width, float Height) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float HalfW = Width * 0.5f;
    float HalfH = Height * 0.5f;

    MeshPtr->BoundsMin = Math::Vec3(-HalfW, 0, -HalfH);
    MeshPtr->BoundsMax = Math::Vec3(HalfW, 0, HalfH);

    Math::Vec3 Normal(0, 1, 0);

    MeshPtr->AddVertex(Math::Vec3(-HalfW, 0, -HalfH), Normal, Math::Vec2(0, 0));
    MeshPtr->AddVertex(Math::Vec3(HalfW, 0, -HalfH), Normal, Math::Vec2(Width, 0));
    MeshPtr->AddVertex(Math::Vec3(HalfW, 0, HalfH), Normal, Math::Vec2(Width, Height));
    MeshPtr->AddVertex(Math::Vec3(-HalfW, 0, HalfH), Normal, Math::Vec2(0, Height));

    MeshPtr->AddTriangle(0, 1, 2);
    MeshPtr->AddTriangle(0, 2, 3);

    MeshPtr->AddSubMesh(0, 0, 6);

    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateSphere(float Radius, int Segments) {
    auto MeshPtr = std::make_unique<Render::Mesh>();

    // Bounds
    MeshPtr->BoundsMin = Math::Vec3(-Radius, -Radius, -Radius);
    MeshPtr->BoundsMax = Math::Vec3(Radius, Radius, Radius);

    for (int Y = 0; Y <= Segments; ++Y) {
        for (int X = 0; X <= Segments; ++X) {
            float XSegment = static_cast<float>(X) / static_cast<float>(Segments);
            float YSegment = static_cast<float>(Y) / static_cast<float>(Segments);
            float XPos = std::cos(XSegment * 2.0f * 3.14159f) * std::sin(YSegment * 3.14159f);
            float YPos = std::cos(YSegment * 3.14159f);
            float ZPos = std::sin(XSegment * 2.0f * 3.14159f) * std::sin(YSegment * 3.14159f);

            Math::Vec3 Pos(XPos * Radius, YPos * Radius, ZPos * Radius);
            Math::Vec3 Normal(XPos, YPos, ZPos);
            Math::Vec2 UV(XSegment, YSegment);

            MeshPtr->AddVertex(Pos, Normal, UV);
        }
    }

    for (int Y = 0; Y < Segments; ++Y) {
        for (int X = 0; X < Segments; ++X) {
            uint32_t I0 = (Y + 1) * (Segments + 1) + X;
            uint32_t I1 = Y * (Segments + 1) + X;
            uint32_t I2 = Y * (Segments + 1) + X + 1;
            uint32_t I3 = (Y + 1) * (Segments + 1) + X + 1;

            MeshPtr->AddTriangle(I0, I1, I2);
            MeshPtr->AddTriangle(I0, I2, I3);
        }
    }

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateTetrahedron(float Size) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float S = Size;

    // Vertices
    Math::Vec3 V0( S,  S,  S);
    Math::Vec3 V1(-S, -S,  S);
    Math::Vec3 V2(-S,  S, -S);
    Math::Vec3 V3( S, -S, -S);

    MeshPtr->BoundsMin = Math::Vec3(-S, -S, -S);
    MeshPtr->BoundsMax = Math::Vec3(S, S, S);

    // Helper to add face
    auto AddFace = [&](const Math::Vec3& A, const Math::Vec3& B, const Math::Vec3& C) {
        Math::Vec3 N = (B - A).Cross(C - A).Normalized();
        MeshPtr->AddVertex(A, N, Math::Vec2(0, 0));
        MeshPtr->AddVertex(B, N, Math::Vec2(1, 0));
        MeshPtr->AddVertex(C, N, Math::Vec2(0.5f, 1));
        uint32_t Base = static_cast<uint32_t>(MeshPtr->Vertices.size()) - 3;
        MeshPtr->AddTriangle(Base, Base + 1, Base + 2);
    };

    AddFace(V0, V2, V1);
    AddFace(V0, V1, V3);
    AddFace(V0, V3, V2);
    AddFace(V1, V2, V3);

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateCylinder(float Radius, float Height, int Segments) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float HalfH = Height * 0.5f;

    MeshPtr->BoundsMin = Math::Vec3(-Radius, -HalfH, -Radius);
    MeshPtr->BoundsMax = Math::Vec3(Radius, HalfH, Radius);

    // Side
    for (int I = 0; I <= Segments; ++I) {
        float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
        float X = std::cos(Theta);
        float Z = std::sin(Theta);

        Math::Vec3 N(X, 0, Z);
        MeshPtr->AddVertex(Math::Vec3(X * Radius, -HalfH, Z * Radius), N, Math::Vec2(static_cast<float>(I)/Segments, 0));
        MeshPtr->AddVertex(Math::Vec3(X * Radius, HalfH, Z * Radius), N, Math::Vec2(static_cast<float>(I)/Segments, 1));
    }

    for (int I = 0; I < Segments; ++I) {
        uint32_t Base = I * 2;
        MeshPtr->AddTriangle(Base, Base + 1, Base + 2);
        MeshPtr->AddTriangle(Base + 1, Base + 3, Base + 2);
    }

    // Caps (simplified fan)
    uint32_t CenterBottom = static_cast<uint32_t>(MeshPtr->Vertices.size());
    MeshPtr->AddVertex(Math::Vec3(0, -HalfH, 0), Math::Vec3(0, -1, 0), Math::Vec2(0.5f, 0.5f));
    for (int I = 0; I <= Segments; ++I) {
        float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
        MeshPtr->AddVertex(Math::Vec3(std::cos(Theta) * Radius, -HalfH, std::sin(Theta) * Radius), Math::Vec3(0, -1, 0), Math::Vec2(0, 0));
    }
    for (int I = 0; I < Segments; ++I) {
        // Bottom cap - try opposite winding
        MeshPtr->AddTriangle(CenterBottom, CenterBottom + I + 2, CenterBottom + I + 1);
    }

    uint32_t CenterTop = static_cast<uint32_t>(MeshPtr->Vertices.size());
    MeshPtr->AddVertex(Math::Vec3(0, HalfH, 0), Math::Vec3(0, 1, 0), Math::Vec2(0.5f, 0.5f));
    for (int I = 0; I <= Segments; ++I) {
        float Theta = static_cast<float>(I) / Segments * 2.0f * 3.14159f;
        MeshPtr->AddVertex(Math::Vec3(std::cos(Theta) * Radius, HalfH, std::sin(Theta) * Radius), Math::Vec3(0, 1, 0), Math::Vec2(0, 0));
    }
    for (int I = 0; I < Segments; ++I) {
        // Top cap - try opposite winding
        MeshPtr->AddTriangle(CenterTop, CenterTop + I + 1, CenterTop + I + 2);
    }

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreatePyramid(float BaseWidth, float Height) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float HalfBase = BaseWidth * 0.5f;

    MeshPtr->BoundsMin = Math::Vec3(-HalfBase, 0, -HalfBase);
    MeshPtr->BoundsMax = Math::Vec3(HalfBase, Height, HalfBase);

    // Base vertices (square)
    Math::Vec3 BaseVerts[4] = {
        Math::Vec3(-HalfBase, 0, -HalfBase),  // 0: back-left
        Math::Vec3(HalfBase, 0, -HalfBase),   // 1: back-right
        Math::Vec3(HalfBase, 0, HalfBase),    // 2: front-right
        Math::Vec3(-HalfBase, 0, HalfBase)   // 3: front-left
    };

    // Apex
    Math::Vec3 Apex(0, Height, 0);

    // Base face (bottom, Y-)
    Math::Vec3 BaseNormal(0, -1, 0);
    uint32_t BaseStart = static_cast<uint32_t>(MeshPtr->Vertices.size());
    for (int I = 0; I < 4; ++I) {
        MeshPtr->AddVertex(BaseVerts[I], BaseNormal, Math::Vec2((I % 2) * 1.0f, (I / 2) * 1.0f));
    }
    MeshPtr->AddTriangle(BaseStart + 0, BaseStart + 2, BaseStart + 1);
    MeshPtr->AddTriangle(BaseStart + 0, BaseStart + 3, BaseStart + 2);

    // Side faces (4 triangular faces)
    auto AddSideFace = [&](const Math::Vec3& V0, const Math::Vec3& V1) {
        Math::Vec3 Edge = V1 - V0;
        Math::Vec3 ToApex = Apex - V0;
        Math::Vec3 Normal = Edge.Cross(ToApex).Normalized();

        uint32_t Start = static_cast<uint32_t>(MeshPtr->Vertices.size());
        MeshPtr->AddVertex(V0, Normal, Math::Vec2(0, 0));
        MeshPtr->AddVertex(V1, Normal, Math::Vec2(1, 0));
        MeshPtr->AddVertex(Apex, Normal, Math::Vec2(0.5f, 1));
        MeshPtr->AddTriangle(Start, Start + 1, Start + 2);
    };

    AddSideFace(BaseVerts[0], BaseVerts[1]); // Back face
    AddSideFace(BaseVerts[1], BaseVerts[2]); // Right face
    AddSideFace(BaseVerts[2], BaseVerts[3]); // Front face
    AddSideFace(BaseVerts[3], BaseVerts[0]); // Left face

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateTorus(float MajorRadius, float MinorRadius, int Segments, int Rings) {
    auto MeshPtr = std::make_unique<Render::Mesh>();
    float MaxRadius = MajorRadius + MinorRadius;

    MeshPtr->BoundsMin = Math::Vec3(-MaxRadius, -MinorRadius, -MaxRadius);
    MeshPtr->BoundsMax = Math::Vec3(MaxRadius, MinorRadius, MaxRadius);

    // Generate vertices
    for (int Ring = 0; Ring <= Rings; ++Ring) {
        float RingAngle = static_cast<float>(Ring) / Rings * 2.0f * 3.14159f;
        float RingCos = std::cos(RingAngle);
        float RingSin = std::sin(RingAngle);
        Math::Vec3 RingCenter(RingCos * MajorRadius, 0, RingSin * MajorRadius);

        for (int Seg = 0; Seg <= Segments; ++Seg) {
            float SegAngle = static_cast<float>(Seg) / Segments * 2.0f * 3.14159f;
            float SegCos = std::cos(SegAngle);
            float SegSin = std::sin(SegAngle);

            // Position on the torus
            Math::Vec3 Pos = RingCenter + Math::Vec3(SegCos * MinorRadius * RingCos, SegSin * MinorRadius, SegCos * MinorRadius * RingSin);

            // Normal (from center of ring to position)
            Math::Vec3 Normal = (Pos - RingCenter).Normalized();

            Math::Vec2 UV(static_cast<float>(Ring) / Rings, static_cast<float>(Seg) / Segments);
            MeshPtr->AddVertex(Pos, Normal, UV);
        }
    }

    // Generate triangles
    for (int Ring = 0; Ring < Rings; ++Ring) {
        for (int Seg = 0; Seg < Segments; ++Seg) {
            uint32_t I0 = Ring * (Segments + 1) + Seg;
            uint32_t I1 = Ring * (Segments + 1) + Seg + 1;
            uint32_t I2 = (Ring + 1) * (Segments + 1) + Seg;
            uint32_t I3 = (Ring + 1) * (Segments + 1) + Seg + 1;

            MeshPtr->AddTriangle(I0, I2, I1);
            MeshPtr->AddTriangle(I1, I2, I3);
        }
    }

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

SOLSTICE_API std::unique_ptr<Render::Mesh> CreateIcosphere(float Radius, int Subdivisions) {
    auto MeshPtr = std::make_unique<Render::Mesh>();

    MeshPtr->BoundsMin = Math::Vec3(-Radius, -Radius, -Radius);
    MeshPtr->BoundsMax = Math::Vec3(Radius, Radius, Radius);

    // Icosahedron base vertices (12 vertices)
    const float T = (1.0f + std::sqrt(5.0f)) / 2.0f; // Golden ratio

    std::vector<Math::Vec3> Vertices = {
        Math::Vec3(-1, T, 0).Normalized(),
        Math::Vec3(1, T, 0).Normalized(),
        Math::Vec3(-1, -T, 0).Normalized(),
        Math::Vec3(1, -T, 0).Normalized(),
        Math::Vec3(0, -1, T).Normalized(),
        Math::Vec3(0, 1, T).Normalized(),
        Math::Vec3(0, -1, -T).Normalized(),
        Math::Vec3(0, 1, -T).Normalized(),
        Math::Vec3(T, 0, -1).Normalized(),
        Math::Vec3(T, 0, 1).Normalized(),
        Math::Vec3(-T, 0, -1).Normalized(),
        Math::Vec3(-T, 0, 1).Normalized()
    };

    // Icosahedron base faces (20 triangles)
    std::vector<std::array<uint32_t, 3>> Faces;
    Faces.reserve(20);
    Faces.push_back(std::array<uint32_t, 3>{0, 11, 5});
    Faces.push_back(std::array<uint32_t, 3>{0, 5, 1});
    Faces.push_back(std::array<uint32_t, 3>{0, 1, 7});
    Faces.push_back(std::array<uint32_t, 3>{0, 7, 10});
    Faces.push_back(std::array<uint32_t, 3>{0, 10, 11});
    Faces.push_back(std::array<uint32_t, 3>{1, 5, 9});
    Faces.push_back(std::array<uint32_t, 3>{5, 11, 4});
    Faces.push_back(std::array<uint32_t, 3>{11, 10, 2});
    Faces.push_back(std::array<uint32_t, 3>{10, 7, 6});
    Faces.push_back(std::array<uint32_t, 3>{7, 1, 8});
    Faces.push_back(std::array<uint32_t, 3>{3, 9, 4});
    Faces.push_back(std::array<uint32_t, 3>{3, 4, 2});
    Faces.push_back(std::array<uint32_t, 3>{3, 2, 6});
    Faces.push_back(std::array<uint32_t, 3>{3, 6, 8});
    Faces.push_back(std::array<uint32_t, 3>{3, 8, 9});
    Faces.push_back(std::array<uint32_t, 3>{4, 9, 5});
    Faces.push_back(std::array<uint32_t, 3>{2, 4, 11});
    Faces.push_back(std::array<uint32_t, 3>{6, 2, 10});
    Faces.push_back(std::array<uint32_t, 3>{8, 6, 7});
    Faces.push_back(std::array<uint32_t, 3>{9, 8, 1});

    // Subdivide
    for (int Sub = 0; Sub < Subdivisions; ++Sub) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> EdgeMap;
        std::vector<std::array<uint32_t, 3>> NewFaces;

        auto GetMidpoint = [&](uint32_t V0, uint32_t V1) -> uint32_t {
            if (V0 > V1) std::swap(V0, V1);
            auto Key = std::make_pair(V0, V1);
            if (EdgeMap.find(Key) != EdgeMap.end()) {
                return EdgeMap[Key];
            }

            Math::Vec3 Mid = (Vertices[V0] + Vertices[V1]).Normalized();
            uint32_t Idx = static_cast<uint32_t>(Vertices.size());
            Vertices.push_back(Mid);
            EdgeMap[Key] = Idx;
            return Idx;
        };

        for (const auto& Face : Faces) {
            uint32_t V0 = Face[0];
            uint32_t V1 = Face[1];
            uint32_t V2 = Face[2];

            uint32_t A = GetMidpoint(V0, V1);
            uint32_t B = GetMidpoint(V1, V2);
            uint32_t C = GetMidpoint(V2, V0);

            NewFaces.push_back(std::array<uint32_t, 3>{V0, A, C});
            NewFaces.push_back(std::array<uint32_t, 3>{V1, B, A});
            NewFaces.push_back(std::array<uint32_t, 3>{V2, C, B});
            NewFaces.push_back(std::array<uint32_t, 3>{A, B, C});
        }

        Faces = NewFaces;
    }

    // Add vertices to mesh
    for (const auto& V : Vertices) {
        Math::Vec3 Pos = V * Radius;
        Math::Vec2 UV(0, 0); // Simple UV mapping
        MeshPtr->AddVertex(Pos, V, UV);
    }

    // Add faces
    for (const auto& Face : Faces) {
        MeshPtr->AddTriangle(Face[0], Face[1], Face[2]);
    }

    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

// Validation utilities
bool ValidateMesh(const Render::Mesh& Mesh) {
    return Arzachel::ValidateMesh(Mesh);
}

// Convert MeshData to Render::Mesh (delegates to AssetBuilder)
std::unique_ptr<Render::Mesh> ConvertToRenderMesh(const MeshData& MeshDataParam) {
    return Arzachel::ConvertToRenderMesh(MeshDataParam);
}

} // namespace MeshFactory

} // namespace Solstice::Arzachel
