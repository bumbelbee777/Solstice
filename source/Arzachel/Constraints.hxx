#pragma once

#include "../Solstice.hxx"
#include "Generator.hxx"
#include "MeshData.hxx"
#include <Math/Vector.hxx>
#include <vector>
#include <cstdint>

namespace Solstice::Arzachel {

// Constraint types for procedural generation
struct PositionConstraint {
    Math::Vec3 PreferredPosition;
    float Tolerance; // Maximum distance from preferred position

    PositionConstraint() : PreferredPosition(0, 0, 0), Tolerance(0.0f) {}
    PositionConstraint(const Math::Vec3& pos, float tol)
        : PreferredPosition(pos), Tolerance(tol) {}
};

struct SizeConstraint {
    float MinSize;
    float MaxSize;

    SizeConstraint() : MinSize(0.0f), MaxSize(1.0f) {}
    SizeConstraint(float min, float max) : MinSize(min), MaxSize(max) {}
};

struct DistanceConstraint {
    Math::Vec3 ReferencePoint;
    float MinDistance;
    float MaxDistance;

    DistanceConstraint() : ReferencePoint(0, 0, 0), MinDistance(0.0f), MaxDistance(1000.0f) {}
    DistanceConstraint(const Math::Vec3& ref, float minDist, float maxDist)
        : ReferencePoint(ref), MinDistance(minDist), MaxDistance(maxDist) {}
};

struct TerrainConstraint {
    const std::vector<float>* Heightmap;
    uint32_t Resolution;
    float TerrainSize;
    float MinHeight;
    float MaxHeight;

    TerrainConstraint()
        : Heightmap(nullptr), Resolution(0), TerrainSize(0.0f), MinHeight(0.0f), MaxHeight(1000.0f) {}

    TerrainConstraint(const std::vector<float>* hm, uint32_t res, float size, float minH, float maxH)
        : Heightmap(hm), Resolution(res), TerrainSize(size), MinHeight(minH), MaxHeight(maxH) {}
};

// Visual objective types
struct VisualObjective {
    enum Type {
        Landmark,  // Place a prominent feature
        Viewpoint, // Create an interesting viewing position
        Path       // Guide along a path
    };

    Type ObjectiveType;
    Math::Vec3 TargetPosition;
    float Importance; // 0.0 to 1.0
    std::vector<PositionConstraint> PositionConstraints;
    std::vector<SizeConstraint> SizeConstraints;
    std::vector<DistanceConstraint> DistanceConstraints;

    VisualObjective()
        : ObjectiveType(Landmark), TargetPosition(0, 0, 0), Importance(1.0f) {}

    VisualObjective(Type type, const Math::Vec3& target, float importance)
        : ObjectiveType(type), TargetPosition(target), Importance(importance) {}
};

// Constraint satisfaction result
struct ConstraintResult {
    bool Satisfied;
    Math::Vec3 FinalPosition;
    float FinalSize;
    float Score; // Quality score (0.0 to 1.0)

    ConstraintResult() : Satisfied(false), FinalPosition(0, 0, 0), FinalSize(0.0f), Score(0.0f) {}
};

// Satisfy a single position constraint
SOLSTICE_API ConstraintResult SatisfyPositionConstraint(
    const PositionConstraint& constraint,
    const Math::Vec3& candidatePosition
);

// Satisfy a single distance constraint
SOLSTICE_API ConstraintResult SatisfyDistanceConstraint(
    const DistanceConstraint& constraint,
    const Math::Vec3& candidatePosition
);

// Satisfy terrain constraint (place on terrain surface)
SOLSTICE_API ConstraintResult SatisfyTerrainConstraint(
    const TerrainConstraint& constraint,
    const Math::Vec3& candidatePosition
);

// Satisfy multiple constraints (returns best position)
SOLSTICE_API ConstraintResult SatisfyConstraints(
    const std::vector<PositionConstraint>& PositionConstraints,
    const std::vector<DistanceConstraint>& DistanceConstraints,
    const TerrainConstraint& TerrainConstraint,
    const Math::Vec3& InitialPosition,
    const Seed& S
);

// Satisfy a visual objective
SOLSTICE_API ConstraintResult SatisfyVisualObjective(
    const VisualObjective& Objective,
    const TerrainConstraint& Terrain,
    const Math::Vec3& InitialPosition,
    const Seed& S
);

} // namespace Solstice::Arzachel
