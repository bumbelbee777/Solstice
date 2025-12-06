#include "LightSource.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {

Math::Vec3 LightSource::CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& Normal, const Math::Vec3& ViewDir, float Shininess) const {
    // 1. Calculate Light Direction and Distance
    Math::Vec3 LightDir = Position - SurfacePos;
    float Distance = LightDir.Magnitude();
    
    // Avoid division by zero
    if (Distance < 0.0001f) return Math::Vec3(0, 0, 0);

    LightDir = LightDir / Distance; // Normalize

    // 2. Attenuation (Inverse Square Law with modification for control)
    // Using a simple model: 1 / (1 + K * d^2)
    // We use Attenuation member as the coefficient K
    float AttenFactor = 1.0f / (1.0f + Attenuation * Distance * Distance);

    // 3. Diffuse (Lambert)
    // Dot product of Normal and Light Direction
    float NdotL = std::max(Normal.Dot(LightDir), 0.0f);
    
    // 4. Specular (Blinn-Phong)
    float SpecularFactor = 0.0f;
    if (NdotL > 0.0f) {
        Math::Vec3 HalfwayDir = (LightDir + ViewDir).Normalized();
        float NdotH = std::max(Normal.Dot(HalfwayDir), 0.0f);
        SpecularFactor = std::pow(NdotH, Shininess);
    }

    // 5. Combine
    // Final = LightColor * Intensity * Attenuation * (Diffuse + Specular)
    // Note: This assumes white specular for simplicity, or we can use LightColor for both
    Math::Vec3 FinalColor = Color * Intensity * AttenFactor * (NdotL + SpecularFactor);

    return FinalColor;
}

}
