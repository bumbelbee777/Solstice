#pragma once

#include <Math/Vector.hxx>
#include <vector>
#include <cstddef>

namespace Solstice::Physics {
struct LightSource {
	Math::Vec3 Position, Color;
	float Intensity, Hue, Attenuation;
	float Range = 0.0f; // 0 = infinite range
	enum class LightType {
		Point,
		Spot,
		Directional
	} Type = LightType::Point;

	LightSource() = default;
	LightSource(Math::Vec3 Position, Math::Vec3 Color, float Intensity, float Hue, float Attenuation) 
		: Position(Position), Color(Color), Intensity(Intensity), Hue(Hue), Attenuation(Attenuation) {}

	void Apply();

	// Calculates the lighting contribution for a surface point
    // Normal: Surface normal (normalized) - can be from normal map
    // ViewDir: Direction from surface to viewer (normalized)
    // Shininess: Specular exponent
    Math::Vec3 CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& Normal, const Math::Vec3& ViewDir, float Shininess) const;

	// Calculate contribution with normal-mapped surface
	// TangentNormal: Normal from normal map in tangent space (normalized)
	// TBN: Tangent-Bitangent-Normal matrix (3x3) to transform from tangent to world space
	Math::Vec3 CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& TangentNormal, 
		const Math::Vec3& ViewDir, float Shininess, const Math::Vec3& Tangent, const Math::Vec3& Bitangent, const Math::Vec3& Normal) const;

	// Batch process multiple surface points (SIMD-optimized)
	// Processes 4 points at a time when possible
	void CalculateContributionBatch(
		const Math::Vec3* SurfacePositions,
		const Math::Vec3* Normals,
		const Math::Vec3& ViewDir,
		float Shininess,
		Math::Vec3* OutContributions,
		size_t Count
	) const;

	// Static method to accumulate multiple lights efficiently
	static Math::Vec3 AccumulateLights(
		const std::vector<LightSource>& Lights,
		const Math::Vec3& SurfacePos,
		const Math::Vec3& Normal,
		const Math::Vec3& ViewDir,
		float Shininess,
		float MaxDistance = 100.0f  // Culling distance
	);
};
}