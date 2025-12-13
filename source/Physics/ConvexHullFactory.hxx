#pragma once

#include "ConvexHull.hxx"
#include "../Render/Mesh.hxx"
#include "../Solstice.hxx"
#include <memory>

namespace Solstice::Physics {

/// Generate a convex hull from a render mesh
/// Extracts unique vertices and computes convex hull properties
SOLSTICE_API std::shared_ptr<ConvexHull> GenerateConvexHull(const Render::Mesh& mesh);

/// Simplify a convex hull by reducing vertex count
/// Uses greedy vertex removal while preserving convexity
SOLSTICE_API void SimplifyHull(ConvexHull& hull, int maxVertices);

/// Compute hull properties (normals, edges, centroid)
SOLSTICE_API void ComputeHullProperties(ConvexHull& hull);

} // namespace Solstice::Physics
