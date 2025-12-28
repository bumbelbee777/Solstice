#include "Constraints.hxx"
#include "TerrainGenerator.hxx"
#include <Core/Debug.hxx>
#include <cmath>
#include <algorithm>

namespace Solstice::Arzachel {

ConstraintResult SatisfyPositionConstraint(
    const PositionConstraint& Constraint,
    const Math::Vec3& CandidatePosition) {

    ConstraintResult Result;
    Result.FinalPosition = CandidatePosition;

    float DistanceVal = (CandidatePosition - Constraint.PreferredPosition).Magnitude();

    if (DistanceVal <= Constraint.Tolerance) {
        Result.Satisfied = true;
        // Score is better when closer to preferred position
        Result.Score = 1.0f - (DistanceVal / Constraint.Tolerance);
    } else {
        Result.Satisfied = false;
        Result.Score = 0.0f;
    }

    return Result;
}

ConstraintResult SatisfyDistanceConstraint(
    const DistanceConstraint& Constraint,
    const Math::Vec3& CandidatePosition) {

    ConstraintResult Result;
    Result.FinalPosition = CandidatePosition;

    float DistanceVal = (CandidatePosition - Constraint.ReferencePoint).Magnitude();

    if (DistanceVal >= Constraint.MinDistance && DistanceVal <= Constraint.MaxDistance) {
        Result.Satisfied = true;
        // Score is better when in the middle of the range
        float RangeVal = Constraint.MaxDistance - Constraint.MinDistance;
        if (RangeVal > 0.0f) {
            float MidPointVal = Constraint.MinDistance + RangeVal * 0.5f;
            float DistFromMidVal = std::abs(DistanceVal - MidPointVal);
            Result.Score = 1.0f - (DistFromMidVal / (RangeVal * 0.5f));
            Result.Score = std::max(0.0f, Result.Score);
        } else {
            Result.Score = 1.0f;
        }
    } else {
        Result.Satisfied = false;
        Result.Score = 0.0f;
    }

    return Result;
}

ConstraintResult SatisfyTerrainConstraint(
    const TerrainConstraint& Constraint,
    const Math::Vec3& CandidatePosition) {

    ConstraintResult Result;
    Result.FinalPosition = CandidatePosition;

    if (!Constraint.Heightmap || Constraint.Heightmap->empty() || Constraint.Resolution == 0) {
        // No terrain data, constraint is trivially satisfied
        Result.Satisfied = true;
        Result.Score = 1.0f;
        return Result;
    }

    float TerrainHeightVal = SampleTerrainHeight(
        *Constraint.Heightmap,
        Constraint.Resolution,
        Constraint.TerrainSize,
        CandidatePosition
    );

    // Place on terrain surface
    Result.FinalPosition.y = TerrainHeightVal;

    // Check height constraints
    if (TerrainHeightVal >= Constraint.MinHeight && TerrainHeightVal <= Constraint.MaxHeight) {
        Result.Satisfied = true;
        Result.Score = 1.0f;
    } else {
        Result.Satisfied = false;
        Result.Score = 0.0f;
    }

    return Result;
}

ConstraintResult SatisfyConstraints(
    const std::vector<PositionConstraint>& PositionConstraints,
    const std::vector<DistanceConstraint>& DistanceConstraints,
    const TerrainConstraint& TerrainConstraintObj,
    const Math::Vec3& InitialPosition,
    const Seed& SeedParam) {

    ConstraintResult Result;
    Result.FinalPosition = InitialPosition;
    Result.Satisfied = false;
    Result.Score = 0.0f;

    // Try to satisfy terrain constraint first (place on terrain)
    ConstraintResult TerrainResult = SatisfyTerrainConstraint(TerrainConstraintObj, InitialPosition);
    if (!TerrainResult.Satisfied) {
        return Result; // Can't satisfy terrain, fail early
    }

    Math::Vec3 CurrentPosition = TerrainResult.FinalPosition;
    float TotalScore = TerrainResult.Score;
    uint32_t SatisfiedCount = 1;

    // Check position constraints
    for (const auto& PosConstraint : PositionConstraints) {
        ConstraintResult PosResult = SatisfyPositionConstraint(PosConstraint, CurrentPosition);
        if (PosResult.Satisfied) {
            SatisfiedCount++;
            TotalScore += PosResult.Score;
            // Move towards preferred position if within tolerance
            float DistVal = (CurrentPosition - PosConstraint.PreferredPosition).Magnitude();
            if (DistVal > 0.01f && DistVal <= PosConstraint.Tolerance) {
                // Blend towards preferred position
                float BlendVal = 0.3f; // 30% towards preferred
                CurrentPosition = CurrentPosition * (1.0f - BlendVal) + PosConstraint.PreferredPosition * BlendVal;
            }
        }
    }

    // Check distance constraints
    for (const auto& DistConstraint : DistanceConstraints) {
        ConstraintResult DistResult = SatisfyDistanceConstraint(DistConstraint, CurrentPosition);
        if (DistResult.Satisfied) {
            SatisfiedCount++;
            TotalScore += DistResult.Score;
        } else {
            // Try to adjust position to satisfy constraint
            float DistVal = (CurrentPosition - DistConstraint.ReferencePoint).Magnitude();
            if (DistVal < DistConstraint.MinDistance) {
                // Move away from reference point
                Math::Vec3 Direction = (CurrentPosition - DistConstraint.ReferencePoint).Normalized();
                if (Direction.Magnitude() < 0.01f) {
                    // Random direction if too close
                    Direction = Math::Vec3(1.0f, 0.0f, 0.0f);
                }
                CurrentPosition = DistConstraint.ReferencePoint + Direction * DistConstraint.MinDistance;
            } else if (DistVal > DistConstraint.MaxDistance) {
                // Move towards reference point
                Math::Vec3 Direction = (DistConstraint.ReferencePoint - CurrentPosition).Normalized();
                CurrentPosition = DistConstraint.ReferencePoint - Direction * DistConstraint.MaxDistance;
            }
        }
    }

    // Re-apply terrain constraint after adjustments
    TerrainResult = SatisfyTerrainConstraint(TerrainConstraintObj, CurrentPosition);
    if (TerrainResult.Satisfied) {
        CurrentPosition = TerrainResult.FinalPosition;
    }

    Result.FinalPosition = CurrentPosition;
    uint32_t TotalConstraints = 1 + static_cast<uint32_t>(PositionConstraints.size()) +
                                 static_cast<uint32_t>(DistanceConstraints.size());
    Result.Satisfied = (SatisfiedCount == TotalConstraints);
    Result.Score = TotalScore / static_cast<float>(TotalConstraints);

    return Result;
}

ConstraintResult SatisfyVisualObjective(
    const VisualObjective& Objective,
    const TerrainConstraint& Terrain,
    const Math::Vec3& InitialPosition,
    const Seed& SeedParam) {

    // Combine all constraints from the objective
    ConstraintResult Result = SatisfyConstraints(
        Objective.PositionConstraints,
        Objective.DistanceConstraints,
        Terrain,
        InitialPosition,
        SeedParam
    );

    // Apply importance weighting
    Result.Score *= Objective.Importance;

    return Result;
}

} // namespace Solstice::Arzachel
