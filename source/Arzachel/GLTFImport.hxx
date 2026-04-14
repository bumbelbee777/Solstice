#pragma once

#include "Generator.hxx"
#include "MeshData.hxx"
#include "RiggedMesh.hxx"
#include "AnimationClip.hxx"
#include <Asset/Loading/AssetLoader.hxx>
#include <filesystem>
#include <cstdint>
#include <functional>
#include <fstream>
#include <sstream>

namespace Solstice::Arzachel {

// Import mesh from glTF file
// Determinism: hash file contents to create seed
Generator<MeshData> ImportMeshFromGLTF(const std::filesystem::path& Path, uint32_t MeshIndex);

// Import rigged mesh from glTF file
Generator<RiggedMesh> ImportRiggedFromGLTF(const std::filesystem::path& Path, uint32_t MeshIndex);

// Import animation from glTF file
Generator<AnimationClip> ImportAnimationFromGLTF(const std::filesystem::path& Path, uint32_t AnimIndex);

// Helper: hash file contents for deterministic seed generation
uint64_t HashFileContents(const std::filesystem::path& Path);

} // namespace Solstice::Arzachel
