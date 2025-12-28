#pragma once

#include "../Solstice.hxx"
#include "MeshData.hxx"
#include "Generator.hxx"
#include <Render/Mesh.hxx>
#include <memory>
#include <vector>

namespace Solstice::Arzachel {

// Asset building utilities for complete asset pipeline

// Build complete asset from generator (mesh + materials + textures)
// This is a placeholder for future expansion
struct CompleteAsset {
    std::unique_ptr<Render::Mesh> Mesh;
    // Materials and textures would be added here
};

// Convert MeshData to Render::Mesh
SOLSTICE_API std::unique_ptr<Render::Mesh> ConvertToRenderMesh(const MeshData& meshData);

// Validate mesh integrity
SOLSTICE_API bool ValidateMesh(const MeshData& meshData);
SOLSTICE_API bool ValidateMesh(const Render::Mesh& mesh);

// Build complete asset from generator
SOLSTICE_API CompleteAsset BuildCompleteAsset(
    const Generator<MeshData>& meshGenerator,
    const Seed& seed
);

// Export MeshData to Render::Mesh (alias for ConvertToRenderMesh)
inline std::unique_ptr<Render::Mesh> ExportToRenderMesh(const MeshData& meshData) {
    return ConvertToRenderMesh(meshData);
}

} // namespace Solstice::Arzachel
