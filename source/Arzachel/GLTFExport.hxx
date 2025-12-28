#pragma once

#include "Generator.hxx"
#include "MeshData.hxx"
#include "RiggedMesh.hxx"
#include "AnimationClip.hxx"
#include "Skeleton.hxx"
#include "Seed.hxx"
#include <filesystem>

namespace Solstice::Arzachel {

// Export profile configuration
struct ExportProfile {
    bool BinaryFormat = false; // true = .glb, false = .gltf
    bool Compress = false;
    bool EmbedBuffers = true;
    // Add more options as needed
};

// Export mesh to glTF
void ExportMeshToGLTF(const Generator<MeshData>& Mesh, const std::filesystem::path& Path, const Seed& S, const ExportProfile& Profile = ExportProfile{});

// Export rigged mesh to glTF
void ExportRiggedToGLTF(const Generator<RiggedMesh>& RiggedMesh, const std::filesystem::path& Path, const Seed& S, const ExportProfile& Profile = ExportProfile{});

// Export animation to glTF
void ExportAnimationToGLTF(const Generator<AnimationClip>& Clip, const Skeleton& Skeleton, const std::filesystem::path& Path, const Seed& S, const ExportProfile& Profile = ExportProfile{});

} // namespace Solstice::Arzachel
