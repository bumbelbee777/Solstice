#include <Render/Mesh.hxx>
#include <memory>
#include <Core/Debug.hxx>
#include <string>

namespace Solstice::Render {

namespace MeshFactory {

std::unique_ptr<Mesh> CreateCube(float Size) {
    SIMPLE_LOG("MeshFactory: Creating Cube with size " + std::to_string(Size));
    auto MeshPtr = std::make_unique<Mesh>();
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
    // P indices:
    // 0: - - - (Left Bottom Back) -> Wait, Z- is Front in OpenGL usually? 
    // Let's stick to the previous logic which seemed to be:
    // 0: - - -
    // 1: + - -
    // 2: + + -
    // 3: - + -
    // 4: - - +
    // 5: + - +
    // 6: + + +
    // 7: - + +
    
    // Front (Z-): 0, 1, 2, 3
    // Back (Z+): 5, 4, 7, 6
    // Bottom (Y-): 0, 1, 5, 4
    // Top (Y+): 3, 2, 6, 7 -> Wait, 3(-+-), 2(++-), 6(+++), 7(-++)
    // Left (X-): 4, 0, 3, 7
    // Right (X+): 1, 5, 6, 2
    
    uint32_t FaceIndices[6][4] = {
        {0, 1, 2, 3}, // Front (Z-)
        {5, 4, 7, 6}, // Back (Z+)
        {4, 5, 1, 0}, // Bottom (Y-) - 4(- - +), 5(+ - +), 1(+ - -), 0(- - -) -> CCW from bottom?
                      // Normal is (0, -1, 0).
                      // 0(-,-,-) -> 1(+,-,-) -> 5(+,-,+) -> 4(-,-,+)
                      // If looking from bottom (Y-), 0 is top-left? No.
                      // Let's just use standard winding.
                      // 0, 1, 5, 4 seems correct for Y-
        {3, 2, 6, 7}, // Top (Y+)
        {4, 0, 3, 7}, // Left (X-)
        {1, 5, 6, 2}  // Right (X+)
    };

    for (int f = 0; f < 6; ++f) {
        // Add 4 vertices per face
        for (int i = 0; i < 4; ++i) {
            MeshPtr->AddVertex(P[FaceIndices[f][i]], N[f], UV[i]);
        }
        
        // Add 2 triangles
        uint32_t Base = f * 4;
        MeshPtr->AddTriangle(Base + 0, Base + 1, Base + 2);
        MeshPtr->AddTriangle(Base + 0, Base + 2, Base + 3);
    }
    
    MeshPtr->AddSubMesh(0, 0, 6 * 2 * 3); // 6 faces, 2 tris, 3 indices
    
    SIMPLE_LOG("MeshFactory: Cube created. Vertices: " + std::to_string(MeshPtr->Vertices.size()) + 
               ", Indices: " + std::to_string(MeshPtr->Indices.size()));
    SIMPLE_LOG("MeshFactory: Bounds: Min(" + 
               std::to_string(MeshPtr->BoundsMin.x) + ", " + 
               std::to_string(MeshPtr->BoundsMin.y) + ", " + 
               std::to_string(MeshPtr->BoundsMin.z) + ") Max(" + 
               std::to_string(MeshPtr->BoundsMax.x) + ", " + 
               std::to_string(MeshPtr->BoundsMax.y) + ", " + 
               std::to_string(MeshPtr->BoundsMax.z) + ")");
    
    return MeshPtr;
}

std::unique_ptr<Mesh> CreatePlane(float Width, float Height) {
    auto MeshPtr = std::make_unique<Mesh>();
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


std::unique_ptr<Mesh> CreateSphere(float Radius, int Segments) {
    auto MeshPtr = std::make_unique<Mesh>();
    
    // Bounds
    MeshPtr->BoundsMin = Math::Vec3(-Radius, -Radius, -Radius);
    MeshPtr->BoundsMax = Math::Vec3(Radius, Radius, Radius);
    
    for (int y = 0; y <= Segments; ++y) {
        for (int x = 0; x <= Segments; ++x) {
            float xSegment = (float)x / (float)Segments;
            float ySegment = (float)y / (float)Segments;
            float xPos = std::cos(xSegment * 2.0f * 3.14159f) * std::sin(ySegment * 3.14159f);
            float yPos = std::cos(ySegment * 3.14159f);
            float zPos = std::sin(xSegment * 2.0f * 3.14159f) * std::sin(ySegment * 3.14159f);
            
            Math::Vec3 Pos(xPos * Radius, yPos * Radius, zPos * Radius);
            Math::Vec3 Normal(xPos, yPos, zPos);
            Math::Vec2 UV(xSegment, ySegment);
            
            MeshPtr->AddVertex(Pos, Normal, UV);
        }
    }
    
    for (int y = 0; y < Segments; ++y) {
        for (int x = 0; x < Segments; ++x) {
            uint32_t i0 = (y + 1) * (Segments + 1) + x;
            uint32_t i1 = y * (Segments + 1) + x;
            uint32_t i2 = y * (Segments + 1) + x + 1;
            uint32_t i3 = (y + 1) * (Segments + 1) + x + 1;
            
            MeshPtr->AddTriangle(i0, i1, i2);
            MeshPtr->AddTriangle(i0, i2, i3);
        }
    }
    
    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

std::unique_ptr<Mesh> CreateTetrahedron(float Size) {
    auto MeshPtr = std::make_unique<Mesh>();
    float s = Size;
    
    // Vertices
    Math::Vec3 v0( s,  s,  s);
    Math::Vec3 v1(-s, -s,  s);
    Math::Vec3 v2(-s,  s, -s);
    Math::Vec3 v3( s, -s, -s);
    
    MeshPtr->BoundsMin = Math::Vec3(-s, -s, -s);
    MeshPtr->BoundsMax = Math::Vec3(s, s, s);
    
    // Helper to add face
    auto AddFace = [&](const Math::Vec3& a, const Math::Vec3& b, const Math::Vec3& c) {
        Math::Vec3 n = (b - a).Cross(c - a).Normalized();
        MeshPtr->AddVertex(a, n, Math::Vec2(0, 0));
        MeshPtr->AddVertex(b, n, Math::Vec2(1, 0));
        MeshPtr->AddVertex(c, n, Math::Vec2(0.5f, 1));
        uint32_t base = static_cast<uint32_t>(MeshPtr->Vertices.size()) - 3;
        MeshPtr->AddTriangle(base, base + 1, base + 2);
    };
    
    AddFace(v0, v2, v1);
    AddFace(v0, v1, v3);
    AddFace(v0, v3, v2);
    AddFace(v1, v2, v3);
    
    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

std::unique_ptr<Mesh> CreateCylinder(float Radius, float Height, int Segments) {
    auto MeshPtr = std::make_unique<Mesh>();
    float halfH = Height * 0.5f;
    
    MeshPtr->BoundsMin = Math::Vec3(-Radius, -halfH, -Radius);
    MeshPtr->BoundsMax = Math::Vec3(Radius, halfH, Radius);
    
    // Side
    for (int i = 0; i <= Segments; ++i) {
        float theta = (float)i / Segments * 2.0f * 3.14159f;
        float x = std::cos(theta);
        float z = std::sin(theta);
        
        Math::Vec3 n(x, 0, z);
        MeshPtr->AddVertex(Math::Vec3(x * Radius, -halfH, z * Radius), n, Math::Vec2((float)i/Segments, 0));
        MeshPtr->AddVertex(Math::Vec3(x * Radius, halfH, z * Radius), n, Math::Vec2((float)i/Segments, 1));
    }
    
    for (int i = 0; i < Segments; ++i) {
        uint32_t base = i * 2;
        MeshPtr->AddTriangle(base, base + 1, base + 2);
        MeshPtr->AddTriangle(base + 1, base + 3, base + 2);
    }
    
    // Caps (simplified fan)
    uint32_t centerBottom = static_cast<uint32_t>(MeshPtr->Vertices.size());
    MeshPtr->AddVertex(Math::Vec3(0, -halfH, 0), Math::Vec3(0, -1, 0), Math::Vec2(0.5f, 0.5f));
    for (int i = 0; i <= Segments; ++i) {
        float theta = (float)i / Segments * 2.0f * 3.14159f;
        MeshPtr->AddVertex(Math::Vec3(std::cos(theta) * Radius, -halfH, std::sin(theta) * Radius), Math::Vec3(0, -1, 0), Math::Vec2(0, 0));
    }
    for (int i = 0; i < Segments; ++i) {
        // Bottom cap - try opposite winding
        MeshPtr->AddTriangle(centerBottom, centerBottom + i + 2, centerBottom + i + 1);
    }
    
    uint32_t centerTop = static_cast<uint32_t>(MeshPtr->Vertices.size());
    MeshPtr->AddVertex(Math::Vec3(0, halfH, 0), Math::Vec3(0, 1, 0), Math::Vec2(0.5f, 0.5f));
    for (int i = 0; i <= Segments; ++i) {
        float theta = (float)i / Segments * 2.0f * 3.14159f;
        MeshPtr->AddVertex(Math::Vec3(std::cos(theta) * Radius, halfH, std::sin(theta) * Radius), Math::Vec3(0, 1, 0), Math::Vec2(0, 0));
    }
    for (int i = 0; i < Segments; ++i) {
        // Top cap - try opposite winding
        MeshPtr->AddTriangle(centerTop, centerTop + i + 1, centerTop + i + 2);
    }
    
    MeshPtr->AddSubMesh(0, 0, static_cast<uint32_t>(MeshPtr->Indices.size()));
    return MeshPtr;
}

} // namespace MeshFactory

} // namespace Solstice::Render
