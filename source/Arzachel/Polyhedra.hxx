#pragma once

#include "../Solstice.hxx"
#include "Generator.hxx"
#include "MeshData.hxx"
#include "Seed.hxx"
#include <cstdint>

namespace Solstice::Arzachel {

// Polyhedra factories - all return Generator<MeshData>
// These are the base geometry generators

SOLSTICE_API Generator<MeshData> Cube(float Size, const Seed& S);
SOLSTICE_API Generator<MeshData> Sphere(float Radius, int Segments, const Seed& S);
SOLSTICE_API Generator<MeshData> Cylinder(float Radius, float Height, int Segments, const Seed& S);
SOLSTICE_API Generator<MeshData> Torus(float MajorRadius, float MinorRadius, int Segments, int Rings, const Seed& S);
SOLSTICE_API Generator<MeshData> Icosphere(float Radius, int Subdivisions, const Seed& S);
SOLSTICE_API Generator<MeshData> Plane(float Width, float Height, const Seed& S);

} // namespace Solstice::Arzachel
