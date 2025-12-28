#pragma once

#include "Generator.hxx"
#include "RiggedMesh.hxx"
#include "MeshData.hxx"
#include "Seed.hxx"

namespace Solstice::Arzachel {

// Base factories that introduce skeletons (only place where skeletons are created)

// Create a rigged mesh with a single root bone
Generator<RiggedMesh> RigidRoot(const Generator<MeshData>& Mesh, const Seed& S);

// Create a rigged mesh with a linear chain of bones
Generator<RiggedMesh> Chain(const Generator<MeshData>& Mesh, int BoneCount, const Seed& S);

// Create a rigged mesh with a spine (segmented chain)
Generator<RiggedMesh> Spine(const Generator<MeshData>& Mesh, int Segments, const Seed& S);

// Advanced rigs
Generator<RiggedMesh> HumanoidRig(const Seed& SeedParam);
Generator<RiggedMesh> AnimalRig(const Seed& SeedParam);

// Proximity-based auto-weighting
void AutoWeight(RiggedMesh& Mesh);

} // namespace Solstice::Arzachel
