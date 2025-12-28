#pragma once

#include "../Solstice.hxx"
#include <Render/Mesh.hxx>
#include <memory>

namespace Solstice {
    namespace Arzachel {
        struct MeshData; // Forward declaration
        
        namespace MeshFactory {
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateCube(float Size);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreatePlane(float Width, float Height);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateSphere(float Radius, int Segments);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateTetrahedron(float Size);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateCylinder(float Radius, float Height, int Segments);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreatePyramid(float BaseWidth, float Height);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateTorus(float MajorRadius, float MinorRadius, int Segments, int Rings);
            SOLSTICE_API std::unique_ptr<Render::Mesh> CreateIcosphere(float Radius, int Subdivisions);

            // Validation utilities
            SOLSTICE_API bool ValidateMesh(const Render::Mesh& Mesh);

            // Convert MeshData to Render::Mesh
            SOLSTICE_API std::unique_ptr<Render::Mesh> ConvertToRenderMesh(const MeshData& MeshDataParam);
        }
    }
}
