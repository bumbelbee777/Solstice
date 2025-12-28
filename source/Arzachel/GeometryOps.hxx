#pragma once

#include "../Solstice.hxx"
#include "Generator.hxx"
#include "MeshData.hxx"
#include "Seed.hxx"
#include "Constraints.hxx"
#include <Math/Matrix.hxx>
#include <vector>

namespace Solstice::Arzachel {

// Core geometry operations
SOLSTICE_API Generator<MeshData> Merge(const Generator<MeshData>& A, const Generator<MeshData>& B);
SOLSTICE_API Generator<MeshData> Transform(const Generator<MeshData>& Mesh, const Math::Matrix4& TransformMatrix);
SOLSTICE_API Generator<MeshData> Instance(const Generator<MeshData>& Mesh, const std::vector<Math::Matrix4>& Transforms);

// CSG operations
SOLSTICE_API Generator<MeshData> Union(const Generator<MeshData>& A, const Generator<MeshData>& B);
SOLSTICE_API Generator<MeshData> Difference(const Generator<MeshData>& A, const Generator<MeshData>& B);
SOLSTICE_API Generator<MeshData> Intersection(const Generator<MeshData>& A, const Generator<MeshData>& B);

// Constraint satisfaction
SOLSTICE_API Generator<MeshData> PlaceOnTerrain(const Generator<MeshData>& Mesh, const TerrainConstraint& Terrain);
SOLSTICE_API Generator<MeshData> WithConstraints(
    const Generator<MeshData>& Mesh,
    const std::vector<PositionConstraint>& PositionConstraints,
    const std::vector<DistanceConstraint>& DistanceConstraints,
    const TerrainConstraint& TerrainConstraint,
    const Seed& SeedParam);

SOLSTICE_API Generator<MeshData> SatisfyVisualObjective(
    const Generator<MeshData>& Mesh,
    const VisualObjective& Objective,
    const TerrainConstraint& Terrain,
    const Seed& SeedParam);

// Blender-like modeling and sculpting operations
SOLSTICE_API Generator<MeshData> Extrude(const Generator<MeshData>& Mesh, float Distance);
SOLSTICE_API Generator<MeshData> Inset(const Generator<MeshData>& Mesh, float Distance);
SOLSTICE_API Generator<MeshData> Bevel(const Generator<MeshData>& Mesh, float Distance);
SOLSTICE_API Generator<MeshData> Smooth(const Generator<MeshData>& Mesh, int Iterations);
SOLSTICE_API Generator<MeshData> Rotate(const Generator<MeshData>& Mesh, float Angle, const Math::Vec3& Axis);
SOLSTICE_API Generator<MeshData> Scale(const Generator<MeshData>& Mesh, const Math::Vec3& ScaleFactors);
SOLSTICE_API Generator<MeshData> Simplify(const Generator<MeshData>& Mesh, float Threshold);
SOLSTICE_API Generator<MeshData> Clone(const Generator<MeshData>& Mesh, const Math::Vec3& Offset = Math::Vec3(0, 0, 0));
SOLSTICE_API Generator<MeshData> Subdivide(const Generator<MeshData>& Mesh);
SOLSTICE_API Generator<MeshData> Mirror(const Generator<MeshData>& Mesh, const Math::Vec4& Plane);

// Organic sculpting tools
SOLSTICE_API Generator<MeshData> Inflate(const Generator<MeshData>& Mesh, float Distance);
SOLSTICE_API Generator<MeshData> Pinch(const Generator<MeshData>& Mesh, float Factor, const Math::Vec3& Point);
SOLSTICE_API Generator<MeshData> Twist(const Generator<MeshData>& Mesh, float Angle, const Math::Vec3& Axis);

// Specialized industrial/scenic generators
SOLSTICE_API Generator<MeshData> Pipes(const std::vector<Math::Vec3>& Path, float Radius, int Segments = 8);
SOLSTICE_API Generator<MeshData> Panels(const Generator<MeshData>& Mesh, float Thickness, float InsetDistance);
SOLSTICE_API Generator<MeshData> LSystemTree(const Seed& SeedParam, int Iterations = 4);
SOLSTICE_API Generator<MeshData> HeightmapTerrain(const Seed& SeedParam, float Size, float MaxHeight);

// LOD support
SOLSTICE_API Generator<MeshData> GenerateLOD(const Generator<MeshData>& Mesh, int Level);

// Fluent API wrapper for MeshData generators
class SOLSTICE_API MeshGenerator {
public:
    MeshGenerator(Generator<MeshData> Gen) : MGen(std::move(Gen)) {}

    MeshData Build(const Seed& Seed) const { return MGen(Seed); }

    MeshGenerator MergeWith(const MeshGenerator& Other) const {
        return MeshGenerator(Merge(MGen, Other.MGen));
    }

    MeshGenerator Transform(const Math::Matrix4& Matrix) const {
        return MeshGenerator(Arzachel::Transform(MGen, Matrix));
    }

    MeshGenerator Extrude(float Distance) const {
        return MeshGenerator(Arzachel::Extrude(MGen, Distance));
    }

    MeshGenerator Inset(float Distance) const {
        return MeshGenerator(Arzachel::Inset(MGen, Distance));
    }

    MeshGenerator Bevel(float Distance) const {
        return MeshGenerator(Arzachel::Bevel(MGen, Distance));
    }

    MeshGenerator Smooth(int Iterations) const {
        return MeshGenerator(Arzachel::Smooth(MGen, Iterations));
    }

    MeshGenerator Rotate(float Angle, const Math::Vec3& Axis) const {
        return MeshGenerator(Arzachel::Rotate(MGen, Angle, Axis));
    }

    MeshGenerator Scale(const Math::Vec3& Factors) const {
        return MeshGenerator(Arzachel::Scale(MGen, Factors));
    }

    MeshGenerator Simplify(float Threshold) const {
        return MeshGenerator(Arzachel::Simplify(MGen, Threshold));
    }

    MeshGenerator Clone(const Math::Vec3& Offset = Math::Vec3(0, 0, 0)) const {
        return MeshGenerator(Arzachel::Clone(MGen, Offset));
    }

    MeshGenerator Subdivide() const {
        return MeshGenerator(Arzachel::Subdivide(MGen));
    }

    MeshGenerator Mirror(const Math::Vec4& Plane) const {
        return MeshGenerator(Arzachel::Mirror(MGen, Plane));
    }

    MeshGenerator Panels(float Thickness, float InsetDistance) const {
        return MeshGenerator(Arzachel::Panels(MGen, Thickness, InsetDistance));
    }

    MeshGenerator Inflate(float Distance) const {
        return MeshGenerator(Arzachel::Inflate(MGen, Distance));
    }

    MeshGenerator Pinch(float Factor, const Math::Vec3& Point) const {
        return MeshGenerator(Arzachel::Pinch(MGen, Factor, Point));
    }

    MeshGenerator Twist(float Angle, const Math::Vec3& Axis) const {
        return MeshGenerator(Arzachel::Twist(MGen, Angle, Axis));
    }

    MeshGenerator GenerateLOD(int Level) const {
        return MeshGenerator(Arzachel::GenerateLOD(MGen, Level));
    }

    const Generator<MeshData>& GetGenerator() const { return MGen; }

private:
    Generator<MeshData> MGen;
};

} // namespace Solstice::Arzachel
