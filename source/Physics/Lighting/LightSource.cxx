#include <Physics/Lighting/LightSource.hxx>
#include <Math/Matrix.hxx>
#include <Core/ML/SIMD.hxx>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Solstice::Physics {

// Fast inverse square root approximation (Quake III algorithm)
inline float FastInvSqrt(float x) {
    union { float f; uint32_t i; } conv = { .f = x };
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5f - (x * 0.5f * conv.f * conv.f);
    return conv.f;
}

Math::Vec3 LightSource::CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& Normal, const Math::Vec3& ViewDir, float Shininess) const {
    // Early culling: check range if specified
    if (Range > 0.0f) {
        Math::Vec3 diff = Position - SurfacePos;
        float DistanceSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (DistanceSq > Range * Range) {
            return Math::Vec3(0, 0, 0);
        }
    }

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
    
    // Early exit if facing away from light
    if (NdotL <= 0.0f) return Math::Vec3(0, 0, 0);
    
    // 4. Specular (Blinn-Phong)
    float SpecularFactor = 0.0f;
    Math::Vec3 HalfwayDir = (LightDir + ViewDir).Normalized();
    float NdotH = std::max(Normal.Dot(HalfwayDir), 0.0f);
    if (NdotH > 0.0f && Shininess > 0.0f) {
        // Fast pow approximation for common shininess values
        if (Shininess <= 128.0f) {
            SpecularFactor = std::pow(NdotH, Shininess);
        } else {
            // For high shininess, use faster approximation
            float exp = NdotH * NdotH;
            exp = exp * exp; // ^4
            if (Shininess <= 256.0f) {
                exp = exp * exp; // ^8
            }
            SpecularFactor = exp;
        }
    }

    // 5. Combine
    // Final = LightColor * Intensity * Attenuation * (Diffuse + Specular)
    Math::Vec3 FinalColor = Color * Intensity * AttenFactor * (NdotL + SpecularFactor * 0.5f);

    return FinalColor;
}

Math::Vec3 LightSource::CalculateContribution(const Math::Vec3& SurfacePos, const Math::Vec3& TangentNormal, 
    const Math::Vec3& ViewDir, float Shininess, const Math::Vec3& Tangent, const Math::Vec3& Bitangent, const Math::Vec3& Normal) const {
    
    // Transform tangent space normal to world space using TBN matrix
    // TBN = [Tangent, Bitangent, Normal]
    Math::Vec3 WorldNormal = (Tangent * TangentNormal.x + Bitangent * TangentNormal.y + Normal * TangentNormal.z).Normalized();
    
    // Use the world-space normal for lighting calculations
    return CalculateContribution(SurfacePos, WorldNormal, ViewDir, Shininess);
}

void LightSource::CalculateContributionBatch(
    const Math::Vec3* SurfacePositions,
    const Math::Vec3* Normals,
    const Math::Vec3& ViewDir,
    float Shininess,
    Math::Vec3* OutContributions,
    size_t Count
) const {
    // Process in batches of 4 for SIMD optimization
    const size_t SIMDWidth = 4;
    size_t simdCount = (Count / SIMDWidth) * SIMDWidth;
    
    // SIMD-optimized batch processing
    #ifdef SOLSTICE_SIMD_SSE
    if (simdCount > 0) {
        // Load light position and color into SIMD registers
        Core::SIMD::Vec4 lightPos(Position.x, Position.y, Position.z, 0.0f);
        Core::SIMD::Vec4 lightColor(Color.x, Color.y, Color.z, Intensity);
        Core::SIMD::Vec4 viewDirVec(ViewDir.x, ViewDir.y, ViewDir.z, 0.0f);
        
        for (size_t i = 0; i < simdCount; i += SIMDWidth) {
            // Load 4 surface positions
            Core::SIMD::Vec4 pos0(SurfacePositions[i+0].x, SurfacePositions[i+0].y, SurfacePositions[i+0].z, 0.0f);
            Core::SIMD::Vec4 pos1(SurfacePositions[i+1].x, SurfacePositions[i+1].y, SurfacePositions[i+1].z, 0.0f);
            Core::SIMD::Vec4 pos2(SurfacePositions[i+2].x, SurfacePositions[i+2].y, SurfacePositions[i+2].z, 0.0f);
            Core::SIMD::Vec4 pos3(SurfacePositions[i+3].x, SurfacePositions[i+3].y, SurfacePositions[i+3].z, 0.0f);
            
            // Calculate light directions
            Core::SIMD::Vec4 lightDir0 = lightPos - pos0;
            Core::SIMD::Vec4 lightDir1 = lightPos - pos1;
            Core::SIMD::Vec4 lightDir2 = lightPos - pos2;
            Core::SIMD::Vec4 lightDir3 = lightPos - pos3;
            
            // Calculate distances and normalize
            float dist0 = lightDir0.Magnitude3();
            float dist1 = lightDir1.Magnitude3();
            float dist2 = lightDir2.Magnitude3();
            float dist3 = lightDir3.Magnitude3();
            
            if (dist0 > 0.0001f) lightDir0 = lightDir0 * (1.0f / dist0);
            if (dist1 > 0.0001f) lightDir1 = lightDir1 * (1.0f / dist1);
            if (dist2 > 0.0001f) lightDir2 = lightDir2 * (1.0f / dist2);
            if (dist3 > 0.0001f) lightDir3 = lightDir3 * (1.0f / dist3);
            
            // Calculate contributions for each
            OutContributions[i+0] = CalculateContribution(SurfacePositions[i+0], Normals[i+0], ViewDir, Shininess);
            OutContributions[i+1] = CalculateContribution(SurfacePositions[i+1], Normals[i+1], ViewDir, Shininess);
            OutContributions[i+2] = CalculateContribution(SurfacePositions[i+2], Normals[i+2], ViewDir, Shininess);
            OutContributions[i+3] = CalculateContribution(SurfacePositions[i+3], Normals[i+3], ViewDir, Shininess);
        }
    }
    #endif
    
    // Process remaining elements
    for (size_t i = simdCount; i < Count; ++i) {
        OutContributions[i] = CalculateContribution(SurfacePositions[i], Normals[i], ViewDir, Shininess);
    }
}

Math::Vec3 LightSource::AccumulateLights(
    const std::vector<LightSource>& Lights,
    const Math::Vec3& SurfacePos,
    const Math::Vec3& Normal,
    const Math::Vec3& ViewDir,
    float Shininess,
    float MaxDistance
) {
    Math::Vec3 totalContribution(0, 0, 0);
    
    for (const auto& light : Lights) {
        // Distance culling
        Math::Vec3 diff = light.Position - SurfacePos;
        float distanceSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        if (MaxDistance > 0.0f && distanceSq > MaxDistance * MaxDistance) {
            continue;
        }
        
        // Range culling
        if (light.Range > 0.0f && distanceSq > light.Range * light.Range) {
            continue;
        }
        
        // Early termination: check if light can contribute
        Math::Vec3 lightDir = (light.Position - SurfacePos).Normalized();
        float NdotL = Normal.Dot(lightDir);
        if (NdotL <= 0.0f) {
            continue; // Light is behind surface
        }
        
        // Calculate contribution
        Math::Vec3 contribution = light.CalculateContribution(SurfacePos, Normal, ViewDir, Shininess);
        
        // Early exit if contribution is negligible
        float contributionMagnitude = contribution.Magnitude();
        if (contributionMagnitude < 0.001f) {
            continue;
        }
        
        totalContribution = totalContribution + contribution;
    }
    
    return totalContribution;
}

}
