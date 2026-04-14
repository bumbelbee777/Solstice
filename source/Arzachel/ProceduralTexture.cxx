#include "ProceduralTexture.hxx"
#include <Core/Debug/Debug.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <mutex>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace Solstice::Arzachel {

uint32_t ProceduralTexture::Hash(uint32_t X, uint32_t Y, uint32_t SeedVal) {
    uint32_t H = SeedVal;
    H ^= X * 0x9e3779b9u; H ^= Y * 0x9e3779b9u;
    H = (H << 13) | (H >> 19);
    H = H * 5 + 0xe6546b64u;
    return H;
}

float ProceduralTexture::HashFloat(uint32_t X, uint32_t Y, uint32_t SeedVal) {
    return static_cast<float>(Hash(X, Y, SeedVal) & 0x7FFFFFFFu) / 2147483647.0f;
}

float ProceduralTexture::Fade(float T) { return T * T * T * (T * (T * 6.0f - 15.0f) + 10.0f); }
float ProceduralTexture::Lerp(float A, float B, float T) { return A + T * (B - A); }
float ProceduralTexture::Grad(int HashVal, float X, float Y) {
    int H = HashVal & 3;
    float U = H < 2 ? X : Y;
    float V = H < 1 ? Y : (H == 3 ? -X : X);
    return U + V;
}

static std::unordered_map<uint32_t, std::vector<uint32_t>> SPermCache;
static std::mutex SPermCacheMutex;

float ProceduralTexture::Noise2D(float X, float Y, uint32_t SeedVal) {
    int Xi = static_cast<int>(std::floor(X)) & 255;
    int Yi = static_cast<int>(std::floor(Y)) & 255;
    float Fx = X - std::floor(X); float Fy = Y - std::floor(Y);
    float U = Fade(Fx); float V = Fade(Fy);
    std::vector<uint32_t> P;
    {
        std::lock_guard<std::mutex> Lock(SPermCacheMutex);
        auto It = SPermCache.find(SeedVal);
        if (It != SPermCache.end()) P = It->second;
        else {
            P.resize(512);
            for (int I = 0; I < 256; I++) { P[I] = Hash(I, 0, SeedVal) % 256; P[256 + I] = P[I]; }
            SPermCache[SeedVal] = P;
        }
    }
    int AA = P[P[Xi] + Yi]; int AB = P[P[Xi] + Yi + 1];
    int BA = P[P[Xi + 1] + Yi]; int BB = P[P[Xi + 1] + Yi + 1];
    return Lerp(Lerp(Grad(AA, Fx, Fy), Grad(BA, Fx - 1.0f, Fy), U),
                Lerp(Grad(AB, Fx, Fy - 1.0f), Grad(BB, Fx - 1.0f, Fy - 1.0f), U), V);
}

float ProceduralTexture::PerlinNoise2D(float X, float Y, uint32_t Octaves, float Scale, const Seed& SeedParam) {
    float Value = 0.0f; float Amplitude = 1.0f; float Frequency = Scale; float MaxValue = 0.0f;
    for (uint32_t I = 0; I < Octaves; I++) {
        Value += Noise2D(X * Frequency, Y * Frequency, static_cast<uint32_t>(SeedParam.Value) + I) * Amplitude;
        MaxValue += Amplitude; Amplitude *= 0.5f; Frequency *= 2.0f;
    }
    return Value / MaxValue;
}

TextureData ProceduralTexture::GenerateNoise(uint32_t Res, uint32_t Oct, float Scale, const Seed& S, bool Parallel) {
    TextureData Data(Res, Res);
#ifdef _OPENMP
    if (Parallel) {
        #pragma omp parallel for schedule(static)
        for (int Y = 0; Y < (int)Res; Y++) {
            for (uint32_t X = 0; X < Res; X++) {
                float Noise = (PerlinNoise2D((float)X/Res, (float)Y/Res, Oct, Scale, S) + 1.0f) * 0.5f;
                uint8_t V = (uint8_t)(Noise * 255.0f);
                uint32_t I = (Y * Res + X) * 4;
                Data.Pixels[I] = Data.Pixels[I+1] = Data.Pixels[I+2] = V; Data.Pixels[I+3] = 255;
            }
        }
        return Data;
    }
#endif
    for (uint32_t Y = 0; Y < Res; Y++) {
        for (uint32_t X = 0; X < Res; X++) {
            float Noise = (PerlinNoise2D((float)X/Res, (float)Y/Res, Oct, Scale, S) + 1.0f) * 0.5f;
            uint8_t V = (uint8_t)(Noise * 255.0f);
            uint32_t I = (Y * Res + X) * 4;
            Data.Pixels[I] = Data.Pixels[I+1] = Data.Pixels[I+2] = V; Data.Pixels[I+3] = 255;
        }
    }
    return Data;
}

TextureData ProceduralTexture::GenerateCheckerboard(uint32_t Res, uint32_t Tile, const Math::Vec3& C1, const Math::Vec3& C2) {
    TextureData Data(Res, Res);
    for (uint32_t Y = 0; Y < Res; Y++) {
        for (uint32_t X = 0; X < Res; X++) {
            const Math::Vec3& C = ((X/Tile + Y/Tile) % 2 == 0) ? C1 : C2;
            uint32_t I = (Y * Res + X) * 4;
            Data.Pixels[I] = (uint8_t)(C.x * 255.0f); Data.Pixels[I+1] = (uint8_t)(C.y * 255.0f);
            Data.Pixels[I+2] = (uint8_t)(C.z * 255.0f); Data.Pixels[I+3] = 255;
        }
    }
    return Data;
}

TextureData ProceduralTexture::GenerateStripes(uint32_t Res, uint32_t Width, bool Horiz, const Math::Vec3& C1, const Math::Vec3& C2) {
    TextureData Data(Res, Res);
    for (uint32_t Y = 0; Y < Res; Y++) {
        for (uint32_t X = 0; X < Res; X++) {
            const Math::Vec3& C = (((Horiz ? Y : X) / Width) % 2 == 0) ? C1 : C2;
            uint32_t I = (Y * Res + X) * 4;
            Data.Pixels[I] = (uint8_t)(C.x * 255.0f); Data.Pixels[I+1] = (uint8_t)(C.y * 255.0f);
            Data.Pixels[I+2] = (uint8_t)(C.z * 255.0f); Data.Pixels[I+3] = 255;
        }
    }
    return Data;
}

TextureData ProceduralTexture::GenerateRoughness(uint32_t Res, float MinR, float MaxR, const Seed& S) {
    TextureData Data(Res, Res);
    for (uint32_t Y = 0; Y < Res; Y++) {
        for (uint32_t X = 0; X < Res; X++) {
            float Noise = (PerlinNoise2D((float)X/Res, (float)Y/Res, 4, 4.0f, S) + 1.0f) * 0.5f;
            uint8_t V = (uint8_t)((MinR + Noise * (MaxR - MinR)) * 255.0f);
            uint32_t I = (Y * Res + X) * 4;
            Data.Pixels[I] = Data.Pixels[I+1] = Data.Pixels[I+2] = V; Data.Pixels[I+3] = 255;
        }
    }
    return Data;
}

TextureData ProceduralTexture::GenerateNormalMap(const TextureData& HeightMap, float Strength) {
    uint32_t Res = HeightMap.Width; TextureData Data(Res, Res);
    for (uint32_t Y = 0; Y < Res; Y++) {
        for (uint32_t X = 0; X < Res; X++) {
            uint32_t X0 = (X > 0 ? X - 1 : Res - 1), X1 = (X < Res - 1 ? X + 1 : 0);
            uint32_t Y0 = (Y > 0 ? Y - 1 : Res - 1), Y1 = (Y < Res - 1 ? Y + 1 : 0);
            float HL = HeightMap.Pixels[(Y * Res + X0) * 4] / 255.0f;
            float HR = HeightMap.Pixels[(Y * Res + X1) * 4] / 255.0f;
            float HD = HeightMap.Pixels[(Y0 * Res + X) * 4] / 255.0f;
            float HU = HeightMap.Pixels[(Y1 * Res + X) * 4] / 255.0f;
            Math::Vec3 N = Math::Vec3((HL - HR) * Strength, (HD - HU) * Strength, 1.0f).Normalized();
            uint32_t I = (Y * Res + X) * 4;
            Data.Pixels[I] = (uint8_t)((N.x + 1.0f) * 0.5f * 255.0f);
            Data.Pixels[I+1] = (uint8_t)((N.y + 1.0f) * 0.5f * 255.0f);
            Data.Pixels[I+2] = (uint8_t)((N.z + 1.0f) * 0.5f * 255.0f);
            Data.Pixels[I+3] = 255;
        }
    }
    return Data;
}

bgfx::TextureHandle ProceduralTexture::UploadToGPU(const TextureData& Data) {
    // Validate data before uploading
    if (Data.Width == 0 || Data.Height == 0) {
        return BGFX_INVALID_HANDLE;
    }
    uint32_t expectedSize = Data.Width * Data.Height * 4; // RGBA8
    if (Data.Pixels.size() != expectedSize) {
        return BGFX_INVALID_HANDLE;
    }
    if (Data.Pixels.empty() || Data.Pixels.data() == nullptr) {
        return BGFX_INVALID_HANDLE;
    }

    return bgfx::createTexture2D((uint16_t)Data.Width, (uint16_t)Data.Height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, bgfx::copy(Data.Pixels.data(), (uint32_t)Data.Pixels.size()));
}

ProceduralMaterial MaterialGenerator::Generate(const Seed& S, uint32_t Res) {
    ProceduralMaterial Mat;
    Mat.Albedo = ProceduralTexture::GenerateNoise(Res, 4, 2.0f, S.Derive(0));
    Mat.Roughness = ProceduralTexture::GenerateRoughness(Res, 0.1f, 0.9f, S.Derive(1));
    TextureData Height = ProceduralTexture::GenerateNoise(Res, 6, 4.0f, S.Derive(2));
    Mat.Normal = ProceduralTexture::GenerateNormalMap(Height, 2.0f);
    Mat.Metallic = ProceduralTexture::GenerateNoise(Res, 2, 1.0f, S.Derive(3));
    return Mat;
}

ProceduralMaterial MaterialGenerator::GenerateIndustrial(const Seed& SeedParam, uint32_t Resolution) {
    ProceduralMaterial Mat;
    auto AlbedoNoise = [&](float X, float Y) {
        float N = (ProceduralTexture::PerlinNoise2D(X, Y, 4, 4.0f, SeedParam.Derive(0)) + 1.0f) * 0.5f;
        Math::Vec3 Color;
        if (N > 0.7f) Color = Math::Vec3(0.4f, 0.3f, 0.2f); // Rust
        else if (N > 0.4f) Color = Math::Vec3(0.3f, 0.35f, 0.4f); // Industrial Blue
        else Color = Math::Vec3(0.2f, 0.2f, 0.2f); // Dark metal
        return Color;
    };

    Mat.Albedo = TextureData(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            Math::Vec3 C = AlbedoNoise((float)X/Resolution, (float)Y/Resolution);
            uint32_t I = (Y * Resolution + X) * 4;
            Mat.Albedo.Pixels[I] = (uint8_t)(C.x * 255.0f);
            Mat.Albedo.Pixels[I+1] = (uint8_t)(C.y * 255.0f);
            Mat.Albedo.Pixels[I+2] = (uint8_t)(C.z * 255.0f);
            Mat.Albedo.Pixels[I+3] = 255;
        }
    }

    Mat.Roughness = ProceduralTexture::GenerateRoughness(Resolution, 0.3f, 0.7f, SeedParam.Derive(1));
    Mat.Metallic = ProceduralTexture::GenerateNoise(Resolution, 2, 2.0f, SeedParam.Derive(2));
    TextureData Height = ProceduralTexture::GenerateNoise(Resolution, 6, 8.0f, SeedParam.Derive(3));
    Mat.Normal = ProceduralTexture::GenerateNormalMap(Height, 1.5f);
    return Mat;
}

ProceduralMaterial MaterialGenerator::GenerateNatural(const Seed& SeedParam, uint32_t Resolution) {
    ProceduralMaterial Mat;
    auto AlbedoNoise = [&](float X, float Y) {
        float N = (ProceduralTexture::PerlinNoise2D(X, Y, 6, 2.0f, SeedParam.Derive(0)) + 1.0f) * 0.5f;
        Math::Vec3 Color;
        if (N > 0.8f) Color = Math::Vec3(0.9f, 0.95f, 1.0f); // Ice/Snow
        else if (N > 0.4f) Color = Math::Vec3(0.5f, 0.5f, 0.5f); // Rock
        else Color = Math::Vec3(0.3f, 0.2f, 0.1f); // Dirt/Bark
        return Color;
    };

    Mat.Albedo = TextureData(Resolution, Resolution);
    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            Math::Vec3 C = AlbedoNoise((float)X/Resolution, (float)Y/Resolution);
            uint32_t I = (Y * Resolution + X) * 4;
            Mat.Albedo.Pixels[I] = (uint8_t)(C.x * 255.0f);
            Mat.Albedo.Pixels[I+1] = (uint8_t)(C.y * 255.0f);
            Mat.Albedo.Pixels[I+2] = (uint8_t)(C.z * 255.0f);
            Mat.Albedo.Pixels[I+3] = 255;
        }
    }

    Mat.Roughness = ProceduralTexture::GenerateRoughness(Resolution, 0.6f, 0.9f, SeedParam.Derive(1));
    Mat.Metallic = TextureData(Resolution, Resolution); // Non-metal
    TextureData Height = ProceduralTexture::GenerateNoise(Resolution, 8, 4.0f, SeedParam.Derive(2));
    Mat.Normal = ProceduralTexture::GenerateNormalMap(Height, 2.0f);
    return Mat;
}

ProceduralMaterial MaterialGenerator::GenerateDamage(const ProceduralMaterial& Base, const Seed& SeedParam, float Amount) {
    ProceduralMaterial Mat = Base;
    uint32_t Resolution = Base.Albedo.Width;

    for (uint32_t Y = 0; Y < Resolution; Y++) {
        for (uint32_t X = 0; X < Resolution; X++) {
            float N = (ProceduralTexture::PerlinNoise2D((float)X/Resolution, (float)Y/Resolution, 4, 10.0f, SeedParam) + 1.0f) * 0.5f;
            if (N < Amount) {
                uint32_t I = (Y * Resolution + X) * 4;
                Mat.Albedo.Pixels[I] = (uint8_t)(Mat.Albedo.Pixels[I] * 0.2f);
                Mat.Albedo.Pixels[I+1] = (uint8_t)(Mat.Albedo.Pixels[I+1] * 0.1f);
                Mat.Albedo.Pixels[I+2] = (uint8_t)(Mat.Albedo.Pixels[I+2] * 0.05f);
                Mat.Roughness.Pixels[I] = 255;
            }
        }
    }
    return Mat;
}

} // namespace Solstice::Arzachel
