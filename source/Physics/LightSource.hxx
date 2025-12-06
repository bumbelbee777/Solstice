#pragma once

#include <Math/Vector.hxx>

namespace Solstice::Physics {
struct LightSource {
	Math::Vec3 Position, Color;
	float Intensity, Hue, Attenuation;

	LightSource() = default;
	LightSource(Math::Vec3 Position, Math::Vec3 Color, float Intensity, float Hue, float Attenuation) : Position(Position), Color(Color)
		, Intensity(Intensity), Hue(Hue), Attenuation(Attenuation) {}

	void Apply();

	// Calculates the lighting contribution for a surface point
    // Normal: Surface normal (normalized)
    // ViewDir: Direction from surface to viewer (normalized)
    // Shininess: Specular exponent
    Math::Vec3 CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& Normal, const Math::Vec3& ViewDir, float Shininess) const;
};
}