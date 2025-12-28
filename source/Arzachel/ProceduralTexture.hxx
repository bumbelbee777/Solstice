#pragma once

#include "../Solstice.hxx"
#include "Seed.hxx"
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <vector>
#include <cstdint>
#include <functional>

namespace Solstice::Arzachel {

// Raw texture data for decoupling from bgfx
struct TextureData {
    uint32_t Width;
    uint32_t Height;
    std::vector<uint8_t> Pixels; // RGBA8

    TextureData() : Width(0), Height(0) {}
    TextureData(uint32_t W, uint32_t H) : Width(W), Height(H), Pixels(W * H * 4) {}
};

// Procedural texture generation utilities
// All textures are deterministic (same seed = same result)
class SOLSTICE_API ProceduralTexture {
public:
    // Generate a Perlin noise texture
    static TextureData GenerateNoise(
        uint32_t Resolution = 512,
        uint32_t Octaves = 4,
        float Scale = 1.0f,
        const Seed& SeedParam = Seed(0),
        bool UseParallel = false
    );

    // Generate a checkerboard pattern
    static TextureData GenerateCheckerboard(
        uint32_t Resolution = 512,
        uint32_t TileSize = 64,
        const Math::Vec3& Color1 = Math::Vec3(1.0f, 1.0f, 1.0f),
        const Math::Vec3& Color2 = Math::Vec3(0.0f, 0.0f, 0.0f)
    );

    // Generate a stripes pattern
    static TextureData GenerateStripes(
        uint32_t Resolution = 512,
        uint32_t StripeWidth = 32,
        bool Horizontal = true,
        const Math::Vec3& Color1 = Math::Vec3(1.0f, 1.0f, 1.0f),
        const Math::Vec3& Color2 = Math::Vec3(0.0f, 0.0f, 0.0f)
    );

    // Generate a procedural roughness map
    static TextureData GenerateRoughness(
        uint32_t Resolution = 512,
        float MinRoughness = 0.1f,
        float MaxRoughness = 0.9f,
        const Seed& SeedParam = Seed(0)
    );

    // Generate a normal map from a height map (red/green channels of TextureData)
    static TextureData GenerateNormalMap(
        const TextureData& HeightMap,
        float Strength = 1.0f
    );

    // Helper to upload TextureData to bgfx
    static bgfx::TextureHandle UploadToGPU(const TextureData& Data);

    // Perlin noise with octaves (public for async generation)
    static float PerlinNoise2D(float X, float Y, uint32_t Octaves, float Scale, const Seed& SeedParam);

private:
    static uint32_t Hash(uint32_t X, uint32_t Y, uint32_t SeedVal);
    static float HashFloat(uint32_t X, uint32_t Y, uint32_t SeedVal);
    static float Noise2D(float X, float Y, uint32_t SeedVal);
    static float Fade(float T);
    static float Lerp(float A, float B, float T);
    static float Grad(int HashVal, float X, float Y);
};

// Material generation coordinated by a single seed
struct ProceduralMaterial {
    TextureData Albedo;
    TextureData Normal;
    TextureData Roughness;
    TextureData Metallic;
};

class SOLSTICE_API MaterialGenerator {
public:
    static ProceduralMaterial Generate(const Seed& SeedParam, uint32_t Resolution = 512);
    static ProceduralMaterial GenerateIndustrial(const Seed& SeedParam, uint32_t Resolution = 512);
    static ProceduralMaterial GenerateNatural(const Seed& SeedParam, uint32_t Resolution = 512);
    static ProceduralMaterial GenerateDamage(const ProceduralMaterial& Base, const Seed& SeedParam, float Amount);
};

} // namespace Solstice::Arzachel
