#include "MaterialPresets.hxx"
#include "ProceduralTexture.hxx"
#include <Math/Vector.hxx>
#include <algorithm>
#include <cmath>

namespace Solstice::Arzachel::MaterialPresets {

namespace Math = Solstice::Math;

TextureData SnowData(const Seed& SeedVal, uint32_t Resolution) {
    auto Material = MaterialGenerator::GenerateNatural(SeedVal, Resolution);
    // Overwrite albedo to be more snow-like (very bright white/blue)
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 4, 10.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            uint8_t V = static_cast<uint8_t>(240 + N * 15);
            Material.Albedo.Pixels[I] = V;
            Material.Albedo.Pixels[I+1] = V;
            Material.Albedo.Pixels[I+2] = 255;
            Material.Albedo.Pixels[I+3] = 255;
        }
    }
    return Material.Albedo;
}

bgfx::TextureHandle Snow(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(SnowData(SeedVal, Resolution));
}

TextureData IceData(const Seed& SeedVal, uint32_t Resolution) {
    auto Material = MaterialGenerator::GenerateNatural(SeedVal, Resolution);
    // Ice: Transparent-ish blue, high smoothness (low roughness)
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            uint32_t I = (Y * Resolution + X) * 4;
            Material.Albedo.Pixels[I] = 100;
            Material.Albedo.Pixels[I+1] = 150;
            Material.Albedo.Pixels[I+2] = 255;
            Material.Albedo.Pixels[I+3] = 180;
        }
    }
    return Material.Albedo;
}

bgfx::TextureHandle Ice(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(IceData(SeedVal, Resolution));
}

TextureData RockData(const Seed& SeedVal, uint32_t Resolution) {
    auto Material = MaterialGenerator::GenerateNatural(SeedVal, Resolution);
    return Material.Albedo;
}

bgfx::TextureHandle Rock(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(RockData(SeedVal, Resolution));
}

TextureData DirtData(const Seed& SeedVal, uint32_t Resolution) {
    auto Material = MaterialGenerator::GenerateNatural(SeedVal.Derive(1337), Resolution);
    return Material.Albedo;
}

bgfx::TextureHandle Dirt(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(DirtData(SeedVal, Resolution));
}

TextureData WoodData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            // Wooden grain using stretched noise
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution * 20.0f, (float)Y/Resolution * 2.0f, 4, 1.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            Albedo.Pixels[I] = static_cast<uint8_t>(60 + N * 40);   // R: Brown
            Albedo.Pixels[I+1] = static_cast<uint8_t>(30 + N * 20); // G
            Albedo.Pixels[I+2] = static_cast<uint8_t>(10 + N * 10); // B
            Albedo.Pixels[I+3] = 255;
        }
    }
    return Albedo;
}

bgfx::TextureHandle Wood(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(WoodData(SeedVal, Resolution));
}

TextureData FrostData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            // Frost patterns (crystalline noise)
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 8, 20.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            uint8_t V = static_cast<uint8_t>(200 + N * 55);
            Albedo.Pixels[I] = V;
            Albedo.Pixels[I+1] = V;
            Albedo.Pixels[I+2] = 255;
            Albedo.Pixels[I+3] = static_cast<uint8_t>(N * 255);
        }
    }
    return Albedo;
}

bgfx::TextureHandle Frost(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(FrostData(SeedVal, Resolution));
}

// Early '00s aesthetic materials

TextureData ConcreteData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            float u = (float)X / Resolution;
            float v = (float)Y / Resolution;
            uint32_t I = (Y * Resolution + X) * 4;
            
            // 1. Base generator: Muted industrial gray gradient
            float baseR = 0.35f; // Darker, more desaturated
            float baseG = 0.35f;
            float baseB = 0.38f; // Slightly bluer
            
            // 2. Noise layers: Multiple octaves for microvariation
            float noise1 = (ProceduralTexture::PerlinNoise2D(u, v, 6, 8.0f, SeedVal) + 1.0f) * 0.5f;
            float noise2 = (ProceduralTexture::PerlinNoise2D(u * 2.0f, v * 2.0f, 4, 16.0f, SeedVal.Derive(1)) + 1.0f) * 0.5f;
            float noise3 = (ProceduralTexture::PerlinNoise2D(u * 4.0f, v * 4.0f, 3, 32.0f, SeedVal.Derive(2)) + 1.0f) * 0.5f;
            float combinedNoise = (noise1 * 0.5f + noise2 * 0.3f + noise3 * 0.2f);
            
            // 3. Imperfection masks: Dirt and wear
            float dirt = (ProceduralTexture::PerlinNoise2D(u * 0.3f, v * 0.3f, 2, 1.5f, SeedVal.Derive(10)) + 1.0f) * 0.5f;
            float wear = (ProceduralTexture::PerlinNoise2D(u * 0.1f, v * 0.1f, 1, 0.5f, SeedVal.Derive(20)) + 1.0f) * 0.5f;
            // Wear at edges
            float edgeDist = std::min(std::min(u, 1.0f - u), std::min(v, 1.0f - v));
            float edgeWear = 1.0f - (edgeDist * 4.0f); // More wear near edges
            edgeWear = std::max(0.0f, std::min(1.0f, edgeWear));
            
            // 4. Tiling & offset: Procedural variation
            float tilingOffset = (ProceduralTexture::PerlinNoise2D(u * 0.05f, v * 0.05f, 1, 0.2f, SeedVal.Derive(30)) + 1.0f) * 0.5f;
            
            // 5. Fake AO / baked highlights: Depth perception
            float ao = (ProceduralTexture::PerlinNoise2D(u * 0.2f, v * 0.2f, 3, 2.0f, SeedVal.Derive(40)) + 1.0f) * 0.5f;
            float highlight = (ProceduralTexture::PerlinNoise2D(u * 0.15f, v * 0.15f, 2, 1.8f, SeedVal.Derive(50)) + 1.0f) * 0.5f;
            // Darken corners and edges (fake AO)
            float cornerAO = (1.0f - edgeDist * 2.0f) * 0.3f;
            
            // Combine all layers
            float r = baseR + (combinedNoise - 0.5f) * 0.15f; // Noise variation
            float g = baseG + (combinedNoise - 0.5f) * 0.15f;
            float b = baseB + (combinedNoise - 0.5f) * 0.12f;
            
            // Apply dirt (darken)
            r *= (1.0f - dirt * 0.2f);
            g *= (1.0f - dirt * 0.2f);
            b *= (1.0f - dirt * 0.15f);
            
            // Apply wear (slight darkening)
            r *= (1.0f - edgeWear * 0.15f);
            g *= (1.0f - edgeWear * 0.15f);
            b *= (1.0f - edgeWear * 0.15f);
            
            // Apply fake AO
            r *= (1.0f - ao * 0.1f - cornerAO);
            g *= (1.0f - ao * 0.1f - cornerAO);
            b *= (1.0f - ao * 0.1f - cornerAO);
            
            // Apply highlights (subtle)
            r += highlight * 0.05f;
            g += highlight * 0.05f;
            b += highlight * 0.05f;
            
            // Clamp and convert to uint8
            Albedo.Pixels[I] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r * 255.0f)));
            Albedo.Pixels[I+1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g * 255.0f)));
            Albedo.Pixels[I+2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b * 255.0f)));
            Albedo.Pixels[I+3] = 255;
        }
    }
    return Albedo;
}

bgfx::TextureHandle Concrete(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(ConcreteData(SeedVal, Resolution));
}

TextureData MetalData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            float u = (float)X / Resolution;
            float v = (float)Y / Resolution;
            uint32_t I = (Y * Resolution + X) * 4;
            
            // 1. Base generator: Industrial metal blue-gray
            float baseR = 0.28f; // Darker, more industrial
            float baseG = 0.32f;
            float baseB = 0.36f;
            
            // 2. Noise layers: Multiple octaves
            float noise1 = (ProceduralTexture::PerlinNoise2D(u, v, 5, 6.0f, SeedVal) + 1.0f) * 0.5f;
            float noise2 = (ProceduralTexture::PerlinNoise2D(u * 3.0f, v * 3.0f, 4, 18.0f, SeedVal.Derive(1)) + 1.0f) * 0.5f;
            float noise3 = (ProceduralTexture::PerlinNoise2D(u * 8.0f, v * 8.0f, 3, 40.0f, SeedVal.Derive(2)) + 1.0f) * 0.5f;
            float combinedNoise = (noise1 * 0.5f + noise2 * 0.3f + noise3 * 0.2f);
            
            // 3. Imperfection masks: Scratches, rust, wear
            float scratches = (ProceduralTexture::PerlinNoise2D(u * 8.0f, v * 8.0f, 2, 50.0f, SeedVal.Derive(10)) + 1.0f) * 0.5f;
            float rust = (ProceduralTexture::PerlinNoise2D(u * 0.4f, v * 0.4f, 3, 2.5f, SeedVal.Derive(20)) + 1.0f) * 0.5f;
            // Scratches are dark lines
            float scratchMask = (scratches < 0.3f) ? scratches * 3.33f : 1.0f;
            
            // 4. Tiling & offset
            float tilingOffset = (ProceduralTexture::PerlinNoise2D(u * 0.05f, v * 0.05f, 1, 0.2f, SeedVal.Derive(30)) + 1.0f) * 0.5f;
            
            // 5. Fake AO / baked highlights
            float ao = (ProceduralTexture::PerlinNoise2D(u * 0.2f, v * 0.2f, 3, 2.0f, SeedVal.Derive(40)) + 1.0f) * 0.5f;
            float highlight = (ProceduralTexture::PerlinNoise2D(u * 0.12f, v * 0.12f, 2, 1.5f, SeedVal.Derive(50)) + 1.0f) * 0.5f;
            float edgeDist = std::min(std::min(u, 1.0f - u), std::min(v, 1.0f - v));
            float cornerAO = (1.0f - edgeDist * 2.0f) * 0.25f;
            
            // Combine layers
            float r = baseR + (combinedNoise - 0.5f) * 0.12f;
            float g = baseG + (combinedNoise - 0.5f) * 0.12f;
            float b = baseB + (combinedNoise - 0.5f) * 0.10f;
            
            // Apply scratches (darken)
            r *= scratchMask;
            g *= scratchMask;
            b *= scratchMask;
            
            // Apply rust (orange-brown tint)
            if (rust > 0.65f) {
                float rustAmount = (rust - 0.65f) / 0.35f;
                r = r * (1.0f - rustAmount * 0.3f) + rustAmount * 0.4f; // More red
                g = g * (1.0f - rustAmount * 0.2f) + rustAmount * 0.25f; // More orange
                b = b * (1.0f - rustAmount * 0.4f); // Less blue
            }
            
            // Apply fake AO
            r *= (1.0f - ao * 0.12f - cornerAO);
            g *= (1.0f - ao * 0.12f - cornerAO);
            b *= (1.0f - ao * 0.12f - cornerAO);
            
            // Apply highlights (metallic reflection)
            r += highlight * 0.08f;
            g += highlight * 0.08f;
            b += highlight * 0.10f;
            
            // Clamp and convert
            Albedo.Pixels[I] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, r * 255.0f)));
            Albedo.Pixels[I+1] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, g * 255.0f)));
            Albedo.Pixels[I+2] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, b * 255.0f)));
            Albedo.Pixels[I+3] = 255;
        }
    }
    return Albedo;
}

bgfx::TextureHandle Metal(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(MetalData(SeedVal, Resolution));
}

TextureData PlasticData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            // Early '00s plastic - smooth, slightly glossy, muted colors
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 4, 4.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            
            // Muted beige/gray plastic
            uint8_t Base = 100;
            uint8_t Variation = static_cast<uint8_t>(N * 15);
            Albedo.Pixels[I] = Base + Variation;
            Albedo.Pixels[I+1] = Base + Variation - 5;
            Albedo.Pixels[I+2] = Base + Variation - 10;
            Albedo.Pixels[I+3] = 255;
        }
    }
    return Albedo;
}

bgfx::TextureHandle Plastic(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(PlasticData(SeedVal, Resolution));
}

TextureData GlassData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            // Window glass - very light, slightly tinted
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 3, 2.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            
            // Very light blue-gray tint
            Albedo.Pixels[I] = static_cast<uint8_t>(230 + N * 20);
            Albedo.Pixels[I+1] = static_cast<uint8_t>(235 + N * 20);
            Albedo.Pixels[I+2] = static_cast<uint8_t>(240 + N * 15);
            Albedo.Pixels[I+3] = static_cast<uint8_t>(180 + N * 75); // Semi-transparent
        }
    }
    return Albedo;
}

bgfx::TextureHandle Glass(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(GlassData(SeedVal, Resolution));
}

TextureData RubberData(const Seed& SeedVal, uint32_t Resolution) {
    TextureData Albedo(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            // Rubber/insulation - dark, matte
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 4, 3.0f, SeedVal) + 1.0f) * 0.5f;
            uint32_t I = (Y * Resolution + X) * 4;
            
            // Very dark gray/black
            uint8_t Base = 20;
            uint8_t Variation = static_cast<uint8_t>(N * 10);
            Albedo.Pixels[I] = Base + Variation;
            Albedo.Pixels[I+1] = Base + Variation;
            Albedo.Pixels[I+2] = Base + Variation;
            Albedo.Pixels[I+3] = 255;
        }
    }
    return Albedo;
}

bgfx::TextureHandle Rubber(const Seed& SeedVal, uint32_t Resolution) {
    return ProceduralTexture::UploadToGPU(RubberData(SeedVal, Resolution));
}

} // namespace Solstice::Arzachel::MaterialPresets
