#include "VolumetricLighting.hxx"
#include <Render/Scene/Scene.hxx>
#include <Core/Debug.hxx>
#include <Core/Async.hxx>
#include <bx/uint32_t.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <unordered_map>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef SOLSTICE_SIMD_SSE
#include <immintrin.h>
#endif

namespace Solstice::Render {

// Fast inverse square root (Quake III algorithm)
inline float FastInvSqrt(float x) {
    union { float f; uint32_t i; } conv = { .f = x };
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f *= 1.5f - (x * 0.5f * conv.f * conv.f);
    return conv.f;
}

// Fast distance using inverse sqrt (avoids sqrt)
inline float FastDistance(const Math::Vec3& a, const Math::Vec3& b) {
    Math::Vec3 diff = a - b;
    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    return distSq * FastInvSqrt(distSq);
}

// ============================================================================
// LightOcclusionGrid Implementation
// ============================================================================

void LightOcclusionGrid::Initialize(const Math::Vec3& InWorldMin, const Math::Vec3& InWorldMax,
                                    uint32_t ResX, uint32_t ResY, uint32_t ResZ) {
    ResolutionX = ResX;
    ResolutionY = ResY;
    ResolutionZ = ResZ;
    WorldMin = InWorldMin;
    WorldMax = InWorldMax;

    VoxelSize = (WorldMax - WorldMin);
    VoxelSize.x /= static_cast<float>(ResolutionX);
    VoxelSize.y /= static_cast<float>(ResolutionY);
    VoxelSize.z /= static_cast<float>(ResolutionZ);

    // Precompute reciprocals (avoid division in hot paths)
    VoxelSizeInvX = 1.0f / VoxelSize.x;
    VoxelSizeInvY = 1.0f / VoxelSize.y;
    VoxelSizeInvZ = 1.0f / VoxelSize.z;

    // Calculate rows per Y slice (each uint64_t holds 64 bits)
    RowsPerY = (ResolutionX + 63) / 64;

    // Allocate flattened bitmask storage
    size_t TotalSize = static_cast<size_t>(ResolutionZ) * ResolutionY * RowsPerY;
    Bitmasks.resize(TotalSize, 0);
}

void LightOcclusionGrid::WorldToVoxel(const Math::Vec3& WorldPos,
                                      int32_t& OutVX, int32_t& OutVY, int32_t& OutVZ) const {
    Math::Vec3 LocalPos = WorldPos - WorldMin;
    // Use precomputed reciprocals (faster than division)
    OutVX = static_cast<int32_t>(LocalPos.x * VoxelSizeInvX);
    OutVY = static_cast<int32_t>(LocalPos.y * VoxelSizeInvY);
    OutVZ = static_cast<int32_t>(LocalPos.z * VoxelSizeInvZ);

    // Clamp to grid bounds (use bitwise AND if power-of-2, but min/max for safety)
    OutVX = std::max(0, std::min(static_cast<int32_t>(ResolutionX - 1), OutVX));
    OutVY = std::max(0, std::min(static_cast<int32_t>(ResolutionY - 1), OutVY));
    OutVZ = std::max(0, std::min(static_cast<int32_t>(ResolutionZ - 1), OutVZ));
}

void LightOcclusionGrid::MarkOccluder(int32_t VX, int32_t VY, int32_t VZ) {
    if (VX < 0 || VX >= static_cast<int32_t>(ResolutionX) ||
        VY < 0 || VY >= static_cast<int32_t>(ResolutionY) ||
        VZ < 0 || VZ >= static_cast<int32_t>(ResolutionZ)) {
        return;
    }

    uint32_t RowIndex = VX / 64;
    uint64_t Bit = 1ULL << (VX % 64);

    size_t Idx = static_cast<size_t>(VZ) * ResolutionY * RowsPerY +
                 static_cast<size_t>(VY) * RowsPerY +
                 RowIndex;

    if (Idx < Bitmasks.size()) {
        Bitmasks[Idx] |= Bit;
    }
}

bool LightOcclusionGrid::IsOccluded(int32_t VX, int32_t VY, int32_t VZ) const {
    if (VX < 0 || VX >= static_cast<int32_t>(ResolutionX) ||
        VY < 0 || VY >= static_cast<int32_t>(ResolutionY) ||
        VZ < 0 || VZ >= static_cast<int32_t>(ResolutionZ)) {
        return false;
    }

    uint32_t RowIndex = VX / 64;
    uint64_t Bit = 1ULL << (VX % 64);

    size_t Idx = static_cast<size_t>(VZ) * ResolutionY * RowsPerY +
                 static_cast<size_t>(VY) * RowsPerY +
                 RowIndex;

    if (Idx < Bitmasks.size()) {
        return (Bitmasks[Idx] & Bit) != 0;
    }
    return false;
}

void LightOcclusionGrid::Clear() {
    std::fill(Bitmasks.begin(), Bitmasks.end(), 0);
}

// ============================================================================
// LightPacket Implementation
// ============================================================================

void LightPacket::AddLight(const Physics::LightSource& Light) {
    if (LightCount >= 4) return;

    Positions[LightCount] = Core::SIMD::Vec4(
        Light.Position.x, Light.Position.y, Light.Position.z, Light.Range);
    Colors[LightCount] = Core::SIMD::Vec4(
        Light.Color.x, Light.Color.y, Light.Color.z, Light.Intensity);

    // Store attenuation in the shared Vec4
    float AttenuationValues[4];
    Attenuations.Store(AttenuationValues);
    AttenuationValues[LightCount] = Light.Attenuation;
    Attenuations = Core::SIMD::Vec4::Load(AttenuationValues);

    LightCount++;
}

void LightPacket::CalculateContribution4(
    const Math::Vec3& SurfacePos,
    const Math::Vec3& Normal,
    float OutContributions[4]) const {

    // Initialize outputs to zero
    for (int I = 0; I < 4; I++) {
        OutContributions[I] = 0.0f;
    }

    // Process each light in the packet
    for (int I = 0; I < LightCount; I++) {
        Math::Vec3 LightPos(Positions[I].X(), Positions[I].Y(), Positions[I].Z());
        float Range = Positions[I].W();
        float Intensity = Colors[I].W();

        float AttenuationValues[4];
        Attenuations.Store(AttenuationValues);
        float Atten = AttenuationValues[I];

        Math::Vec3 ToLight = LightPos - SurfacePos;
        float Distance = ToLight.Magnitude();

        if (Distance > Range && Range > 0.0f) {
            OutContributions[I] = 0.0f;
            continue;
        }

        if (Distance < 0.0001f) {
            OutContributions[I] = Intensity;
            continue;
        }

        Math::Vec3 LightDir = ToLight / Distance;
        float NdotL = std::max(0.0f, Normal.Dot(LightDir));

        // Inverse square attenuation with range falloff
        float AttenFactor = 1.0f / (1.0f + Atten * Distance * Distance);
        if (Range > 0.0f) {
            float RangeFactor = 1.0f - (Distance / Range);
            RangeFactor = std::max(0.0f, RangeFactor);
            AttenFactor *= RangeFactor * RangeFactor;
        }

        OutContributions[I] = NdotL * Intensity * AttenFactor;
    }
}

void LightPacket::CalculateVolumetricContribution4(
    const Math::Vec3& SamplePos,
    float OutContributions[4]) const {

    // Initialize outputs to zero
    for (int I = 0; I < 4; I++) {
        OutContributions[I] = 0.0f;
    }

    if (LightCount == 0) return;

    // Simplified: simple distance-based falloff (early 2000s style)
    // contribution = intensity / (1 + attenuation * distance²)
    for (int I = 0; I < LightCount; I++) {
        Math::Vec3 LightPos(Positions[I].X(), Positions[I].Y(), Positions[I].Z());
        float Range = Positions[I].W();
        float Intensity = Colors[I].W();

        float AttenuationValues[4];
        Attenuations.Store(AttenuationValues);
        float Atten = AttenuationValues[I];

        Math::Vec3 ToLight = LightPos - SamplePos;
        float DistanceSq = ToLight.x * ToLight.x + ToLight.y * ToLight.y + ToLight.z * ToLight.z;
        float Distance = DistanceSq * FastInvSqrt(DistanceSq); // Fast distance

        if (Distance > Range && Range > 0.0f) {
            OutContributions[I] = 0.0f;
            continue;
        }

        if (Distance < 0.0001f) {
            OutContributions[I] = Intensity;
            continue;
        }

        // Simple inverse square attenuation: 1 / (1 + k * d²)
        float AttenFactor = 1.0f / (1.0f + Atten * DistanceSq);
        if (Range > 0.0f) {
            float RangeFactor = 1.0f - (Distance / Range);
            RangeFactor = std::max(0.0f, RangeFactor);
            AttenFactor *= RangeFactor * RangeFactor;
        }

        OutContributions[I] = Intensity * AttenFactor;
    }
}

// ============================================================================
// VolumetricLighting Implementation
// ============================================================================

VolumetricLighting::VolumetricLighting() = default;

VolumetricLighting::~VolumetricLighting() {
    Shutdown();
}

void VolumetricLighting::Initialize(uint32_t Width, uint32_t Height,
                                    const Math::Vec3& WorldMin, const Math::Vec3& WorldMax) {
    // Avoid zero-sized internal buffers (e.g. tiny framebuffer before WM resize on some WMs);
    // zero width breaks indexing and stochastic mask generation.
    m_Width = std::max(1u, Width / 4);    // 1/4 resolution for visible godrays (balanced performance)
    m_Height = std::max(1u, Height / 4);

    // Initialize occlusion grid (simplified - single level)
    m_OcclusionGrid.Initialize(WorldMin, WorldMax, 32, 32, 32);

    // Initialize single mipmap level (16³ only)
    m_OcclusionMipmaps.resize(1);
    m_OcclusionMipmaps[0].Initialize(WorldMin, WorldMax, 16, 16, 16);

    // Allocate volumetric buffers (RGB separate for SIMD)
    size_t BufferSize = m_Width * m_Height;
    m_VolumetricBufferR.resize(BufferSize, 0.0f);
    m_VolumetricBufferG.resize(BufferSize, 0.0f);
    m_VolumetricBufferB.resize(BufferSize, 0.0f);
    m_PrevBufferR.resize(BufferSize, 0.0f);
    m_PrevBufferG.resize(BufferSize, 0.0f);
    m_PrevBufferB.resize(BufferSize, 0.0f);

    // Initialize async buffers
    m_AsyncBuffers[0].Resize(BufferSize);
    m_AsyncBuffers[1].Resize(BufferSize);

    // Initialize stochastic sampling resources
    GenerateBlueNoise();
    m_StochasticMask.resize(BufferSize, false);

    // Optimized defaults (balanced for quality and performance)
    m_NumRaysPerLight = 6;         // Reduced for speed while keeping visibility
    m_RaySpreadAngle = 1.57f;      // 90 degrees
    m_SimpleFalloff = 0.1f;        // Simple distance falloff
    m_StochasticRatio = 0.6f;      // Reduced sampling ratio for speed
    m_TemporalFrames = 2;          // Reduced from 4
    m_RayMarchSteps = 6;           // Reduced for speed

    // Precompute all LUTs and caches asynchronously to avoid blocking init
    m_PrecomputeReady.store(false, std::memory_order_release);
    if (m_PrecomputeFuture.valid()) {
        m_PrecomputeFuture.wait();
    }
    m_PrecomputeFuture = std::async(std::launch::async, [this]() {
        PrecomputeAll();
        m_PrecomputeReady.store(true, std::memory_order_release);
    });

    // Create BGFX texture (must be readable for shader sampling, not write-only)
    m_VolumetricTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(m_Width),
        static_cast<uint16_t>(m_Height),
        false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
}

void VolumetricLighting::Shutdown() {
    if (m_PrecomputeFuture.valid()) {
        m_PrecomputeFuture.wait();
    }

    if (bgfx::isValid(m_VolumetricTexture)) {
        bgfx::destroy(m_VolumetricTexture);
        m_VolumetricTexture = BGFX_INVALID_HANDLE;
    }

    m_VolumetricBufferR.clear();
    m_VolumetricBufferG.clear();
    m_VolumetricBufferB.clear();
    m_OcclusionGrid.Clear();
}

void VolumetricLighting::GenerateBlueNoise() {
    m_BlueNoiseTexture.resize(m_BlueNoiseSize * m_BlueNoiseSize);

    // Simple blue noise approximation using scrambled low-discrepancy sequence
    std::mt19937 Rng(12345);
    std::uniform_real_distribution<float> Dist(0.0f, 1.0f);

    for (size_t I = 0; I < m_BlueNoiseTexture.size(); I++) {
        // Van der Corput sequence with scrambling
        uint32_t N = static_cast<uint32_t>(I);
        float Value = 0.0f;
        float Denom = 1.0f;

        while (N > 0) {
            Denom *= 2.0f;
            Value += (N & 1) / Denom;
            N >>= 1;
        }

        // Add scrambling
        Value = std::fmod(Value + Dist(Rng) * 0.1f, 1.0f);
        m_BlueNoiseTexture[I] = Value;
    }
}

void VolumetricLighting::GenerateStochasticMask(uint32_t Width, uint32_t Height, int Frame) {
    size_t TotalPixels = static_cast<size_t>(Width) * static_cast<size_t>(Height);

    // Must match pixel loop extent; otherwise operator[] on vector<bool> is out of bounds
    // (GCC libstdc++ checks this; MSVC release often does not).
    m_StochasticMask.resize(TotalPixels);
    std::fill(m_StochasticMask.begin(), m_StochasticMask.end(), false);

    // Use blue noise + frame offset for temporal distribution
    uint32_t FrameOffset = (Frame % 3) * 7; // Cycle through 3 phases

#ifdef SOLSTICE_SIMD_SSE
    // SIMD-optimized: process 4 pixels at once
    __m128 thresholdVec = _mm_set1_ps(m_StochasticRatio);
    size_t simdCount = (TotalPixels / 4) * 4;

    for (size_t Idx = 0; Idx < simdCount; Idx += 4) {
        // Calculate X, Y for 4 pixels
        float noiseVals[4];
        for (int I = 0; I < 4; I++) {
            uint32_t X = (Idx + I) % Width;
            uint32_t Y = (Idx + I) / Width;
            uint32_t NoiseX = (X + FrameOffset) % m_BlueNoiseSize;
            uint32_t NoiseY = (Y + FrameOffset * 3) % m_BlueNoiseSize;
            noiseVals[I] = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
        }
        __m128 noise = _mm_loadu_ps(noiseVals);

        // Compare: noise < threshold
        __m128 mask = _mm_cmplt_ps(noise, thresholdVec);

        // Store mask bits
        uint32_t maskBits = _mm_movemask_ps(mask);
        m_StochasticMask[Idx + 0] = (maskBits & 1) != 0;
        m_StochasticMask[Idx + 1] = (maskBits & 2) != 0;
        m_StochasticMask[Idx + 2] = (maskBits & 4) != 0;
        m_StochasticMask[Idx + 3] = (maskBits & 8) != 0;
    }

    // Process remaining pixels (scalar)
    for (size_t Idx = simdCount; Idx < TotalPixels; Idx++) {
        uint32_t X = Idx % Width;
        uint32_t Y = Idx / Width;
        uint32_t NoiseX = (X + FrameOffset) % m_BlueNoiseSize;
        uint32_t NoiseY = (Y + FrameOffset * 3) % m_BlueNoiseSize;
        float NoiseValue = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
        if (NoiseValue < m_StochasticRatio) {
            m_StochasticMask[Idx] = true;
        }
    }
#else
    // Scalar fallback
    for (uint32_t Y = 0; Y < Height; Y++) {
        for (uint32_t X = 0; X < Width; X++) {
            size_t Idx = Y * Width + X;
            uint32_t NoiseX = (X + FrameOffset) % m_BlueNoiseSize;
            uint32_t NoiseY = (Y + FrameOffset * 3) % m_BlueNoiseSize;
            float NoiseValue = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
            if (NoiseValue < m_StochasticRatio) {
                m_StochasticMask[Idx] = true;
            }
        }
    }
#endif
}

// ============================================================================
// Validation Helpers
// ============================================================================

std::vector<Physics::LightSource> VolumetricLighting::ValidateLights(
    const std::vector<Physics::LightSource>& Lights) const {
    std::vector<Physics::LightSource> ValidLights;
    ValidLights.reserve(Lights.size());

    size_t FilteredCount = 0;
    size_t LowIntensityCount = 0;
    float MaxIntensity = 0.0f;

    for (const auto& Light : Lights) {
        bool Filtered = false;

        // Check for NaN positions
        if (std::isnan(Light.Position.x) || std::isnan(Light.Position.y) || std::isnan(Light.Position.z)) {
            Filtered = true;
        }

        // Check for invalid intensity
        if (!Filtered && (Light.Intensity <= 0.0f || std::isnan(Light.Intensity) || !std::isfinite(Light.Intensity))) {
            Filtered = true;
        }

        // Check for invalid colors
        if (!Filtered && (std::isnan(Light.Color.x) || std::isnan(Light.Color.y) || std::isnan(Light.Color.z) ||
            !std::isfinite(Light.Color.x) || !std::isfinite(Light.Color.y) || !std::isfinite(Light.Color.z))) {
            Filtered = true;
        }

        // Check for negative color components
        if (!Filtered && (Light.Color.x < 0.0f || Light.Color.y < 0.0f || Light.Color.z < 0.0f)) {
            Filtered = true;
        }

        if (Filtered) {
            FilteredCount++;
            continue;
        }

        // Check for very low intensity (warn but don't filter)
        if (Light.Intensity < 0.1f) {
            LowIntensityCount++;
        }
        MaxIntensity = std::max(MaxIntensity, Light.Intensity);

        ValidLights.push_back(Light);
    }

    // Log warnings about filtered lights (every 5 seconds)
    static int LightWarningCount = 0;
    if (LightWarningCount++ % 300 == 0) {
        if (FilteredCount > 0) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting filtered out ") + std::to_string(FilteredCount) +
                      std::string(" invalid lights out of ") + std::to_string(Lights.size()) + std::string(" total."));
        }
        if (LowIntensityCount > 0 && ValidLights.size() > 0) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting has ") + std::to_string(LowIntensityCount) +
                      std::string(" lights with intensity < 0.1. Max intensity: ") + std::to_string(MaxIntensity) +
                      std::string(". Godrays may be faint!"));
        }
        if (ValidLights.size() > 0 && MaxIntensity < 1.0f) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting max light intensity is low (") +
                      std::to_string(MaxIntensity) + std::string("). Consider increasing light intensities for visible godrays."));
        }
    }

    return ValidLights;
}

bool VolumetricLighting::ValidateLightingData() const {
    if (m_VolumetricBufferR.empty() || m_VolumetricBufferG.empty() || m_VolumetricBufferB.empty()) {
        return false;
    }

    size_t BufferSize = m_VolumetricBufferR.size();
    if (m_VolumetricBufferG.size() != BufferSize || m_VolumetricBufferB.size() != BufferSize) {
        return false;
    }

    // Check for NaN/Inf values and count non-zero pixels
    bool HasNonZero = false;
    size_t NanCount = 0;
    size_t InfCount = 0;

    for (size_t Idx = 0; Idx < BufferSize; Idx++) {
        float R = m_VolumetricBufferR[Idx];
        float G = m_VolumetricBufferG[Idx];
        float B = m_VolumetricBufferB[Idx];

        // Check for NaN
        if (std::isnan(R) || std::isnan(G) || std::isnan(B)) {
            NanCount++;
            continue;
        }

        // Check for Inf
        if (!std::isfinite(R) || !std::isfinite(G) || !std::isfinite(B)) {
            InfCount++;
            continue;
        }

        // Check for non-zero values
        if (R > 0.0001f || G > 0.0001f || B > 0.0001f) {
            HasNonZero = true;
        }
    }

    // Data is valid if:
    // 1. No NaN values (allow some tolerance for edge cases - up to 1% NaN is acceptable)
    // 2. No Inf values
    // Note: We don't require non-zero values - sparse volumetric data is valid
    size_t TotalPixels = BufferSize;
    float NanRatio = static_cast<float>(NanCount) / static_cast<float>(TotalPixels);
    bool Valid = (NanRatio < 0.01f) && (InfCount == 0);

    // Calculate statistics for warning
    float MaxValue = 0.0f;
    size_t NonZeroCount = 0;
    float TotalValue = 0.0f;
    for (size_t Idx = 0; Idx < BufferSize; Idx++) {
        float R = m_VolumetricBufferR[Idx];
        float G = m_VolumetricBufferG[Idx];
        float B = m_VolumetricBufferB[Idx];
        if (std::isfinite(R) && std::isfinite(G) && std::isfinite(B)) {
            float Magnitude = std::sqrt(R * R + G * G + B * B);
            MaxValue = std::max(MaxValue, Magnitude);
            if (Magnitude > 0.0001f) {
                NonZeroCount++;
                TotalValue += Magnitude;
            }
        }
    }
    float AvgValue = (NonZeroCount > 0) ? (TotalValue / static_cast<float>(NonZeroCount)) : 0.0f;
    float NonZeroRatio = static_cast<float>(NonZeroCount) / static_cast<float>(TotalPixels);

    // Store statistics for warning checks (using static to avoid frequent logging)
    static float LastMaxValue = 0.0f;
    static float LastAvgValue = 0.0f;
    static float LastNonZeroRatio = 0.0f;
    static int FaintWarningCount = 0;

    // Warn if data is too faint (every 5 seconds)
    if (FaintWarningCount++ % 300 == 0) {
        if (Valid && NonZeroCount > 0) {
            if (MaxValue < 0.01f) {
                SIMPLE_LOG(std::string("WARNING: VolumetricLighting data is very faint! Max value: ") +
                          std::to_string(MaxValue) + std::string(", Avg: ") + std::to_string(AvgValue) +
                          std::string(", Non-zero pixels: ") + std::to_string(NonZeroRatio * 100.0f) + std::string("%"));
            } else if (NonZeroRatio < 0.01f) {
                SIMPLE_LOG(std::string("WARNING: VolumetricLighting data is very sparse! Only ") +
                          std::to_string(NonZeroRatio * 100.0f) + std::string("% of pixels have non-zero values."));
            } else if (AvgValue < 0.001f && MaxValue < 0.1f) {
                SIMPLE_LOG(std::string("WARNING: VolumetricLighting data is faint! Max: ") +
                          std::to_string(MaxValue) + std::string(", Avg: ") + std::to_string(AvgValue) +
                          std::string(". Consider increasing density or light intensity."));
            }
        } else if (Valid && NonZeroCount == 0) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting data is all zeros! No godrays will be visible. ") +
                      std::string("Check light positions, intensities, and occlusion grid."));
        }
    }

    LastMaxValue = MaxValue;
    LastAvgValue = AvgValue;
    LastNonZeroRatio = NonZeroRatio;

    return Valid;
}

void VolumetricLighting::PackLightsIntoPackets(const std::vector<Physics::LightSource>& Lights,
                                               std::vector<LightPacket>& OutPackets) {
    OutPackets.clear();

    if (Lights.empty()) return;

    size_t NumPackets = (Lights.size() + 3) / 4;
    OutPackets.resize(NumPackets);

    size_t ValidLights = 0;
    for (size_t I = 0; I < Lights.size(); I++) {
        // Sanity check: ensure light is valid before packing
        if (std::isnan(Lights[I].Position.x) || std::isnan(Lights[I].Position.y) || std::isnan(Lights[I].Position.z)) continue;
        if (Lights[I].Intensity <= 0.0f || std::isnan(Lights[I].Intensity)) continue;

        size_t PacketIdx = I / 4;
        OutPackets[PacketIdx].AddLight(Lights[I]);
        ValidLights++;
    }

    // No logging for performance
}

float VolumetricLighting::CalculateSimpleContribution(const Math::Vec3& SamplePos,
                                                       const Physics::LightSource& Light) const {
    Math::Vec3 ToLight = Light.Position - SamplePos;
    float DistanceSq = ToLight.x * ToLight.x + ToLight.y * ToLight.y + ToLight.z * ToLight.z;
    float Distance = DistanceSq * FastInvSqrt(DistanceSq);

    if (Light.Range > 0.0f && Distance > Light.Range) {
        return 0.0f;
    }

    if (Distance < 0.0001f) {
        return Light.Intensity;
    }

    // Simple falloff: intensity / (1 + falloff * distance²)
    float FalloffFactor = 1.0f / (1.0f + m_SimpleFalloff * DistanceSq);
    if (Light.Range > 0.0f) {
        float RangeFactor = 1.0f - (Distance / Light.Range);
        RangeFactor = std::max(0.0f, RangeFactor);
        FalloffFactor *= RangeFactor * RangeFactor;
    }

    return Light.Intensity * FalloffFactor;
}

void VolumetricLighting::BuildOcclusionGrid(const Scene& SceneGraph) {
    m_OcclusionGrid.Clear();

    // Get mesh library
    MeshLibrary* MeshLib = const_cast<Scene&>(SceneGraph).GetMeshLibrary();
    if (!MeshLib) return;

    const_cast<Scene&>(SceneGraph).UpdateTransforms();

    size_t ObjectCount = SceneGraph.GetObjectCount();

    // Optimized: Use bounding boxes instead of all vertices
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int ObjIdx = 0; ObjIdx < static_cast<int>(ObjectCount); ObjIdx++) {
        SceneObjectID ObjID = static_cast<SceneObjectID>(ObjIdx);
        uint32_t MeshID = SceneGraph.GetMeshID(ObjID);
        const Mesh* MeshPtr = MeshLib->GetMesh(MeshID);

        if (!MeshPtr || MeshPtr->Vertices.empty()) continue;

        const Math::Matrix4& WorldMat = SceneGraph.GetWorldMatrix(ObjID);

        // Use bounding box instead of all vertices (much faster)
        const Math::Vec3& Min = MeshPtr->BoundsMin;
        const Math::Vec3& Max = MeshPtr->BoundsMax;

        // Transform bounding box corners
        Math::Vec3 Corners[8] = {
            {Min.x, Min.y, Min.z}, {Max.x, Min.y, Min.z},
            {Min.x, Max.y, Min.z}, {Max.x, Max.y, Min.z},
            {Min.x, Min.y, Max.z}, {Max.x, Min.y, Max.z},
            {Min.x, Max.y, Max.z}, {Max.x, Max.y, Max.z}
        };

        // Mark voxels in bounding box (sparse sampling)
        int32_t MinVX, MinVY, MinVZ, MaxVX, MaxVY, MaxVZ;
        Math::Vec3 WorldMin(1e6f, 1e6f, 1e6f), WorldMax(-1e6f, -1e6f, -1e6f);

        for (int I = 0; I < 8; I++) {
            Math::Vec4 WorldPos4 = WorldMat * Math::Vec4(Corners[I].x, Corners[I].y, Corners[I].z, 1.0f);
            Math::Vec3 WorldPos(WorldPos4.x, WorldPos4.y, WorldPos4.z);
            WorldMin.x = std::min(WorldMin.x, WorldPos.x);
            WorldMin.y = std::min(WorldMin.y, WorldPos.y);
            WorldMin.z = std::min(WorldMin.z, WorldPos.z);
            WorldMax.x = std::max(WorldMax.x, WorldPos.x);
            WorldMax.y = std::max(WorldMax.y, WorldPos.y);
            WorldMax.z = std::max(WorldMax.z, WorldPos.z);
        }

        m_OcclusionGrid.WorldToVoxel(WorldMin, MinVX, MinVY, MinVZ);
        m_OcclusionGrid.WorldToVoxel(WorldMax, MaxVX, MaxVY, MaxVZ);

        // Mark voxels in bounding box (sparse - every 2nd voxel for performance)
        for (int32_t VZ = MinVZ; VZ <= MaxVZ; VZ += 2) {
            for (int32_t VY = MinVY; VY <= MaxVY; VY += 2) {
                for (int32_t VX = MinVX; VX <= MaxVX; VX += 2) {
#ifdef _OPENMP
                    #pragma omp critical
#endif
                    {
                        m_OcclusionGrid.MarkOccluder(VX, VY, VZ);
                    }
                }
            }
        }
    }

    BuildOcclusionMipmaps();
}

void VolumetricLighting::BuildOcclusionMipmaps() {
    if (m_OcclusionMipmaps.empty()) return;

    // Clear mipmap level
    m_OcclusionMipmaps[0].Clear();

    // Downsample 32³ → 16³ (single level only)
    LightOcclusionGrid& Mip16 = m_OcclusionMipmaps[0];
    for (uint32_t Z = 0; Z < Mip16.ResolutionZ; Z++) {
        for (uint32_t Y = 0; Y < Mip16.ResolutionY; Y++) {
            for (uint32_t X = 0; X < Mip16.ResolutionX; X++) {
                bool Occupied = false;
                for (int DZ = 0; DZ < 2 && !Occupied; DZ++) {
                    for (int DY = 0; DY < 2 && !Occupied; DY++) {
                        for (int DX = 0; DX < 2 && !Occupied; DX++) {
                            if (m_OcclusionGrid.IsOccluded(
                                static_cast<int32_t>(X * 2 + DX),
                                static_cast<int32_t>(Y * 2 + DY),
                                static_cast<int32_t>(Z * 2 + DZ))) {
                                Occupied = true;
                            }
                        }
                    }
                }
                if (Occupied) {
                    Mip16.MarkOccluder(static_cast<int32_t>(X),
                                       static_cast<int32_t>(Y),
                                       static_cast<int32_t>(Z));
                }
            }
        }
    }
}

bool VolumetricLighting::GenerateRadialRays(const Physics::LightSource& Light,
                                            const Math::Matrix4& ViewProj,
                                            Math::Vec2& OutLightScreenPos,
                                            std::vector<Math::Vec2>& OutRayDirs2D) {
    OutRayDirs2D.clear();
    OutRayDirs2D.reserve(m_NumRaysPerLight);

    // Project light position to screen space
    Math::Vec4 LightPos4(Light.Position.x, Light.Position.y, Light.Position.z, 1.0f);
    Math::Vec4 LightProj = ViewProj * LightPos4;

    if (std::abs(LightProj.w) < 0.0001f) return false; // Behind camera or invalid

    LightProj = LightProj / LightProj.w;
    OutLightScreenPos = Math::Vec2(LightProj.x * 0.5f + 0.5f, 1.0f - (LightProj.y * 0.5f + 0.5f));

    // Generate hardcoded radial rays in screen space
    float AngleStep = m_RaySpreadAngle / static_cast<float>(m_NumRaysPerLight);
    float BaseAngle = 0.0f; // Start angle (can be randomized per frame for variation)

    for (int I = 0; I < m_NumRaysPerLight; I++) {
        float Angle = BaseAngle + I * AngleStep;
        Math::Vec2 RayDir(std::cos(Angle), std::sin(Angle));
        OutRayDirs2D.push_back(RayDir);
    }
    return !OutRayDirs2D.empty();
}

Math::Vec3 VolumetricLighting::MarchScreenSpaceRay(int PixelX, int PixelY,
                                                    const Math::Vec2& RayDir2D,
                                                    const Math::Vec2& LightScreenPos,
                                                    const Physics::LightSource& Light,
                                                    const Math::Matrix4& InvViewProj) {
    Math::Vec3 AccumulatedColor(0.0f, 0.0f, 0.0f);

    // Convert pixel to screen space (0-1)
    float U = (static_cast<float>(PixelX) + 0.5f) / static_cast<float>(m_Width);
    float V = (static_cast<float>(PixelY) + 0.5f) / static_cast<float>(m_Height);

    // Check if pixel is in ray path (simple 2D check)
    Math::Vec2 PixelPos(U, V);
    Math::Vec2 ToPixel = PixelPos - LightScreenPos;
    float ToPixelDist = ToPixel.Magnitude();

    if (ToPixelDist < 0.001f) return AccumulatedColor; // At light position

    // Check if pixel aligns with ray direction (dot product check)
    Math::Vec2 ToPixelNorm = ToPixel / ToPixelDist;
    float Alignment = ToPixelNorm.Dot(RayDir2D);

    // Only process if pixel is roughly aligned with ray (within spread)
    if (Alignment < 0.2f) return AccumulatedColor; // Wider catch, beam shaping below

    float Beam = std::max(0.0f, Alignment);
    float Beam2 = Beam * Beam;
    float Beam4 = Beam2 * Beam2;
    float Beam6 = Beam4 * Beam2; // Stronger forward-scattering streak
    float DistanceFade = 1.0f - std::min(1.0f, ToPixelDist * 0.7f);
    DistanceFade = DistanceFade * DistanceFade;
    float BeamBoost = 0.35f + Beam6 * 2.5f;

    // Simple screen-space ray marching along the hardcoded ray direction
    int NumSteps = m_RayMarchSteps; // Use configurable steps
    float MaxDist = ToPixelDist * 1.5f; // March slightly beyond pixel
    float StepSize = MaxDist / static_cast<float>(NumSteps);
    float Transmittance = 1.0f;
    const float Decay = 0.96f; // Slower decay for longer streaks

    for (int Step = 0; Step < NumSteps; Step++) {
        float T = static_cast<float>(Step) * StepSize;
        Math::Vec2 SamplePos2D = LightScreenPos + RayDir2D * T;

        // Check bounds
        if (SamplePos2D.x < 0.0f || SamplePos2D.x > 1.0f ||
            SamplePos2D.y < 0.0f || SamplePos2D.y > 1.0f) {
            break;
        }

        // Convert sample position back to world space for distance calculation
        float SampleU = SamplePos2D.x;
        float SampleV = SamplePos2D.y;
        float NdcX = SampleU * 2.0f - 1.0f;
        float NdcY = 1.0f - SampleV * 2.0f;

        // Use depth approximation (0.5 = mid-range)
        Math::Vec4 WorldPoint = InvViewProj * Math::Vec4(NdcX, NdcY, 0.5f, 1.0f);
        WorldPoint = WorldPoint / WorldPoint.w;
        Math::Vec3 SamplePos(WorldPoint.x, WorldPoint.y, WorldPoint.z);

        // Simple occlusion check using occlusion grid
        if (!m_OcclusionMipmaps.empty()) {
            int32_t VX, VY, VZ;
            m_OcclusionMipmaps[0].WorldToVoxel(SamplePos, VX, VY, VZ);
            if (m_OcclusionMipmaps[0].IsOccluded(VX, VY, VZ)) {
                Transmittance *= Decay * Decay; // Faster decay when occluded
                if (Transmittance < 0.01f) break;
                continue;
            }
        }

        // Calculate simple contribution
        float Contribution = CalculateSimpleContribution(SamplePos, Light);
        Contribution *= BeamBoost * DistanceFade;
        Contribution *= Transmittance;

        if (Contribution > 0.0001f) {
            AccumulatedColor = AccumulatedColor + Light.Color * Contribution;
        }

        Transmittance *= Decay;
    }

    // Scale down for half-precision (density will be applied in ProcessTile)
    return AccumulatedColor * 0.7f; // Boosted for more visible godrays
}

void VolumetricLighting::ProcessTile(int TileX, int TileY,
                                     const std::vector<Physics::LightSource>& Lights,
                                     const Math::Vec3& CameraPos,
                                     const Math::Matrix4& InvViewProj) {
    int StartX = TileX * TILE_SIZE;
    int StartY = TileY * TILE_SIZE;
    int EndX = std::min(StartX + TILE_SIZE, static_cast<int>(m_Width));
    int EndY = std::min(StartY + TILE_SIZE, static_cast<int>(m_Height));

    // Compute view-projection for ray generation
    Math::Matrix4 ViewProj = InvViewProj.Inverse();

    // Generate hardcoded radial rays for each light
    std::vector<std::vector<Math::Vec2>> LightRayDirs;
    std::vector<Math::Vec2> LightScreenPositions;
    LightRayDirs.reserve(Lights.size());
    LightScreenPositions.reserve(Lights.size());

    for (const auto& Light : Lights) {
        std::vector<Math::Vec2> RayDirs;
        Math::Vec2 LightScreenPos(0.0f, 0.0f);
        bool Valid = GenerateRadialRays(Light, ViewProj, LightScreenPos, RayDirs);
        LightRayDirs.push_back(std::move(RayDirs));
        LightScreenPositions.push_back(Valid ? LightScreenPos : Math::Vec2(-1.0f, -1.0f));
    }

    for (int Y = StartY; Y < EndY; Y++) {
        for (int X = StartX; X < EndX; X++) {
            size_t Idx = Y * m_Width + X;

            // Check stochastic mask
            if (!m_StochasticMask[Idx]) {
                // Use previous frame value (handled in temporal accumulation)
                continue;
            }

            Math::Vec3 AccumulatedColor(0.0f, 0.0f, 0.0f);

            // Process each light with its hardcoded radial rays
            for (size_t LightIdx = 0; LightIdx < Lights.size() && LightIdx < LightRayDirs.size(); LightIdx++) {
                const auto& Light = Lights[LightIdx];
                const auto& RayDirs = LightRayDirs[LightIdx];
                if (RayDirs.empty()) continue;
                const Math::Vec2& LightScreenPos = LightScreenPositions[LightIdx];

                // Sample a subset of rays (stochastic selection for quality)
                int RaysToSample = std::min(8, static_cast<int>(RayDirs.size())); // Increased from 4
                for (int RayIdx = 0; RayIdx < RaysToSample; RayIdx++) {
                    // Use blue noise to select which ray to sample
                    uint32_t NoiseX = (X + m_FrameCounter * 7) % m_BlueNoiseSize;
                    uint32_t NoiseY = (Y + m_FrameCounter * 11) % m_BlueNoiseSize;
                    float Noise = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
                    int SelectedRay = static_cast<int>(Noise * RayDirs.size()) % static_cast<int>(RayDirs.size());

                    if (SelectedRay >= static_cast<int>(RayDirs.size())) continue;

                    // March screen-space ray
                    Math::Vec3 RayColor = MarchScreenSpaceRay(X, Y, RayDirs[SelectedRay], LightScreenPos, Light, InvViewProj);
                    AccumulatedColor = AccumulatedColor + RayColor;
                }
            }

            // Validate accumulated color: check for NaN, Inf, and clamp to reasonable range
            if (std::isnan(AccumulatedColor.x) || std::isnan(AccumulatedColor.y) || std::isnan(AccumulatedColor.z) ||
                !std::isfinite(AccumulatedColor.x) || !std::isfinite(AccumulatedColor.y) || !std::isfinite(AccumulatedColor.z)) {
                AccumulatedColor = Math::Vec3(0.0f, 0.0f, 0.0f);
            }

            // Apply density multiplier
            AccumulatedColor = AccumulatedColor * m_Density;

            // Hardcoded beam shaping for early-00s style: boost bright streaks, suppress haze
            float Luma = (AccumulatedColor.x + AccumulatedColor.y + AccumulatedColor.z) * 0.3333f;
            if (Luma < 0.002f) {
                AccumulatedColor = Math::Vec3(0.0f, 0.0f, 0.0f);
            } else {
                float BeamBoost = 1.2f + std::sqrt(Luma) * 1.5f;
                AccumulatedColor = AccumulatedColor * BeamBoost;
            }

            // Clamp to reasonable range (0 to 1000) to prevent overflow
            AccumulatedColor.x = std::max(0.0f, std::min(1000.0f, AccumulatedColor.x));
            AccumulatedColor.y = std::max(0.0f, std::min(1000.0f, AccumulatedColor.y));
            AccumulatedColor.z = std::max(0.0f, std::min(1000.0f, AccumulatedColor.z));

            // Store result
            m_VolumetricBufferR[Idx] = AccumulatedColor.x;
            m_VolumetricBufferG[Idx] = AccumulatedColor.y;
            m_VolumetricBufferB[Idx] = AccumulatedColor.z;
        }
    }
}

void VolumetricLighting::AccumulateTemporal() {
    float BlendFactor = 1.0f / static_cast<float>(m_TemporalFrames);
    float PrevFactor = 1.0f - BlendFactor;

    size_t BufferSize = m_Width * m_Height;

#ifdef SOLSTICE_SIMD_SSE
    // SIMD-optimized: process 4 pixels at once
    __m128 blendVec = _mm_set1_ps(BlendFactor);
    __m128 prevVec = _mm_set1_ps(PrevFactor);

    size_t simdCount = (BufferSize / 4) * 4;

#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int Idx = 0; Idx < static_cast<int>(simdCount); Idx += 4) {
        // Load current RGB values for 4 pixels
        __m128 rCur = _mm_loadu_ps(&m_VolumetricBufferR[Idx]);
        __m128 gCur = _mm_loadu_ps(&m_VolumetricBufferG[Idx]);
        __m128 bCur = _mm_loadu_ps(&m_VolumetricBufferB[Idx]);
        __m128 rPrev = _mm_loadu_ps(&m_PrevBufferR[Idx]);
        __m128 gPrev = _mm_loadu_ps(&m_PrevBufferG[Idx]);
        __m128 bPrev = _mm_loadu_ps(&m_PrevBufferB[Idx]);

        // Load mask for 4 pixels (convert bool to float mask)
        __m128i maskInt = _mm_set_epi32(
            m_StochasticMask[Idx + 3] ? 0xFFFFFFFF : 0,
            m_StochasticMask[Idx + 2] ? 0xFFFFFFFF : 0,
            m_StochasticMask[Idx + 1] ? 0xFFFFFFFF : 0,
            m_StochasticMask[Idx + 0] ? 0xFFFFFFFF : 0
        );
        __m128 mask = _mm_castsi128_ps(maskInt);

        // Blend: new = current * blendFactor + prev * prevFactor
        __m128 rBlended = _mm_add_ps(
            _mm_mul_ps(rCur, blendVec),
            _mm_mul_ps(rPrev, prevVec)
        );
        __m128 gBlended = _mm_add_ps(
            _mm_mul_ps(gCur, blendVec),
            _mm_mul_ps(gPrev, prevVec)
        );
        __m128 bBlended = _mm_add_ps(
            _mm_mul_ps(bCur, blendVec),
            _mm_mul_ps(bPrev, prevVec)
        );

        // Select: if mask (traced), use blended; else use prev (not traced)
        __m128 rResult = _mm_or_ps(
            _mm_and_ps(mask, rBlended),
            _mm_andnot_ps(mask, rPrev)
        );
        __m128 gResult = _mm_or_ps(
            _mm_and_ps(mask, gBlended),
            _mm_andnot_ps(mask, gPrev)
        );
        __m128 bResult = _mm_or_ps(
            _mm_and_ps(mask, bBlended),
            _mm_andnot_ps(mask, bPrev)
        );

        // Store results
        _mm_storeu_ps(&m_VolumetricBufferR[Idx], rResult);
        _mm_storeu_ps(&m_VolumetricBufferG[Idx], gResult);
        _mm_storeu_ps(&m_VolumetricBufferB[Idx], bResult);
    }

    // Process remaining pixels (scalar)
    for (size_t Idx = simdCount; Idx < BufferSize; Idx++) {
        if (!m_StochasticMask[Idx]) {
            m_VolumetricBufferR[Idx] = m_PrevBufferR[Idx];
            m_VolumetricBufferG[Idx] = m_PrevBufferG[Idx];
            m_VolumetricBufferB[Idx] = m_PrevBufferB[Idx];
        } else {
            m_VolumetricBufferR[Idx] = m_VolumetricBufferR[Idx] * BlendFactor + m_PrevBufferR[Idx] * PrevFactor;
            m_VolumetricBufferG[Idx] = m_VolumetricBufferG[Idx] * BlendFactor + m_PrevBufferG[Idx] * PrevFactor;
            m_VolumetricBufferB[Idx] = m_VolumetricBufferB[Idx] * BlendFactor + m_PrevBufferB[Idx] * PrevFactor;
        }
    }
#else
    // Scalar fallback
#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int Idx = 0; Idx < static_cast<int>(BufferSize); Idx++) {
        if (!m_StochasticMask[Idx]) {
            m_VolumetricBufferR[Idx] = m_PrevBufferR[Idx];
            m_VolumetricBufferG[Idx] = m_PrevBufferG[Idx];
            m_VolumetricBufferB[Idx] = m_PrevBufferB[Idx];
        } else {
            m_VolumetricBufferR[Idx] = m_VolumetricBufferR[Idx] * BlendFactor + m_PrevBufferR[Idx] * PrevFactor;
            m_VolumetricBufferG[Idx] = m_VolumetricBufferG[Idx] * BlendFactor + m_PrevBufferG[Idx] * PrevFactor;
            m_VolumetricBufferB[Idx] = m_VolumetricBufferB[Idx] * BlendFactor + m_PrevBufferB[Idx] * PrevFactor;
        }
    }
#endif
}

void VolumetricLighting::ApplyBlueNoiseDither() {
    size_t BufferSize = m_Width * m_Height;
    float DitherStrength = 0.01f;

#ifdef SOLSTICE_SIMD_SSE
    // SIMD-optimized: process 4 pixels at once
    __m128 ditherStrengthVec = _mm_set1_ps(DitherStrength);
    __m128 halfVec = _mm_set1_ps(0.5f);
    __m128 zeroVec = _mm_setzero_ps();

    size_t simdCount = (BufferSize / 4) * 4;

    for (size_t Idx = 0; Idx < simdCount; Idx += 4) {
        // Load noise values for 4 pixels
        float noiseVals[4];
        for (int I = 0; I < 4; I++) {
            uint32_t X = (Idx + I) % m_Width;
            uint32_t Y = (Idx + I) / m_Width;
            noiseVals[I] = m_BlueNoiseTexture[(Y % m_BlueNoiseSize) * m_BlueNoiseSize + (X % m_BlueNoiseSize)];
        }
        __m128 noise = _mm_loadu_ps(noiseVals);

        // Noise = (Noise - 0.5) * DitherStrength
        noise = _mm_mul_ps(_mm_sub_ps(noise, halfVec), ditherStrengthVec);

        // Load RGB values
        __m128 r = _mm_loadu_ps(&m_VolumetricBufferR[Idx]);
        __m128 g = _mm_loadu_ps(&m_VolumetricBufferG[Idx]);
        __m128 b = _mm_loadu_ps(&m_VolumetricBufferB[Idx]);

        // Add noise and clamp to 0
        r = _mm_max_ps(zeroVec, _mm_add_ps(r, noise));
        g = _mm_max_ps(zeroVec, _mm_add_ps(g, noise));
        b = _mm_max_ps(zeroVec, _mm_add_ps(b, noise));

        // Store results
        _mm_storeu_ps(&m_VolumetricBufferR[Idx], r);
        _mm_storeu_ps(&m_VolumetricBufferG[Idx], g);
        _mm_storeu_ps(&m_VolumetricBufferB[Idx], b);
    }

    // Process remaining pixels (scalar)
    for (size_t Idx = simdCount; Idx < BufferSize; Idx++) {
        uint32_t X = Idx % m_Width;
        uint32_t Y = Idx / m_Width;
        float Noise = m_BlueNoiseTexture[(Y % m_BlueNoiseSize) * m_BlueNoiseSize + (X % m_BlueNoiseSize)];
        Noise = (Noise - 0.5f) * DitherStrength;
        m_VolumetricBufferR[Idx] = std::max(0.0f, m_VolumetricBufferR[Idx] + Noise);
        m_VolumetricBufferG[Idx] = std::max(0.0f, m_VolumetricBufferG[Idx] + Noise);
        m_VolumetricBufferB[Idx] = std::max(0.0f, m_VolumetricBufferB[Idx] + Noise);
    }
#else
    // Scalar fallback
    for (size_t Idx = 0; Idx < BufferSize; Idx++) {
        uint32_t X = Idx % m_Width;
        uint32_t Y = Idx / m_Width;
        float Noise = m_BlueNoiseTexture[(Y % m_BlueNoiseSize) * m_BlueNoiseSize + (X % m_BlueNoiseSize)];
        Noise = (Noise - 0.5f) * DitherStrength;
        m_VolumetricBufferR[Idx] = std::max(0.0f, m_VolumetricBufferR[Idx] + Noise);
        m_VolumetricBufferG[Idx] = std::max(0.0f, m_VolumetricBufferG[Idx] + Noise);
        m_VolumetricBufferB[Idx] = std::max(0.0f, m_VolumetricBufferB[Idx] + Noise);
    }
#endif
}

// ============================================================================
// Precomputation Methods
// ============================================================================

void VolumetricLighting::PrecomputeAll() {
    PrecomputeFalloffLUT();
    PrecomputeRayDirections();
    PrecomputeStochasticMasks();
    PrecomputeHalfConversion();
}

void VolumetricLighting::PrecomputeFalloffLUT() {
    for (int I = 0; I < FALLOFF_LUT_SIZE; I++) {
        float Distance = (static_cast<float>(I) / static_cast<float>(FALLOFF_LUT_SIZE - 1)) * FALLOFF_LUT_MAX_DIST;
        float DistanceSq = Distance * Distance;
        m_FalloffLUT[I] = 1.0f / (1.0f + m_SimpleFalloff * DistanceSq);

        // Range falloff (for lights with range)
        float RangeFactor = 1.0f - (Distance / FALLOFF_LUT_MAX_DIST);
        RangeFactor = std::max(0.0f, RangeFactor);
        m_RangeFalloffLUT[I] = RangeFactor * RangeFactor;
    }
}

void VolumetricLighting::RebuildFalloffLUT() {
    PrecomputeFalloffLUT();
}

void VolumetricLighting::PrecomputeRayDirections() {
    float AngleStep = m_RaySpreadAngle / static_cast<float>(m_NumRaysPerLight);
    for (int I = 0; I < m_NumRaysPerLight && I < MAX_PRECOMPUTED_RAYS; I++) {
        float Angle = I * AngleStep;
        m_PrecomputedRayDirs[I] = Math::Vec2(std::cos(Angle), std::sin(Angle));
    }
}

void VolumetricLighting::PrecomputeStochasticMasks() {
    size_t BufferSize = m_Width * m_Height;
    for (int MaskIdx = 0; MaskIdx < NUM_STOCHASTIC_MASKS; MaskIdx++) {
        m_PrecomputedMasks[MaskIdx].resize(BufferSize, false);

        uint32_t FrameOffset = (MaskIdx % 3) * 7;
        size_t TargetPixels = static_cast<size_t>(BufferSize * m_StochasticRatio);

        for (size_t Idx = 0; Idx < BufferSize; Idx++) {
            uint32_t X = Idx % m_Width;
            uint32_t Y = Idx / m_Width;
            uint32_t NoiseX = (X + FrameOffset) % m_BlueNoiseSize;
            uint32_t NoiseY = (Y + FrameOffset * 3) % m_BlueNoiseSize;
            float NoiseValue = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
            if (NoiseValue < m_StochasticRatio) {
                m_PrecomputedMasks[MaskIdx][Idx] = true;
            }
        }
    }
}

void VolumetricLighting::PrecomputeHalfConversion() {
    // Precompute half-float conversion for common values (0-255 range)
    for (int I = 0; I < 256; I++) {
        float Value = static_cast<float>(I);
        m_FloatToHalfLUT[I] = bx::halfFromFloat(Value);
    }
}

void VolumetricLighting::UpdateScreenPosCache(const Math::Matrix4& InvViewProj) {
    // Only update if matrix changed
    if (m_CachedInvViewProj == InvViewProj) {
        return;
    }
    m_CachedInvViewProj = InvViewProj;

    size_t BufferSize = m_Width * m_Height;
    m_ScreenPosCache.resize(BufferSize);

    for (uint32_t Y = 0; Y < m_Height; Y++) {
        for (uint32_t X = 0; X < m_Width; X++) {
            size_t Idx = Y * m_Width + X;

            // Convert pixel to screen space (0-1)
            float U = (static_cast<float>(X) + 0.5f) / static_cast<float>(m_Width);
            float V = (static_cast<float>(Y) + 0.5f) / static_cast<float>(m_Height);

            // Convert to NDC
            float NdcX = U * 2.0f - 1.0f;
            float NdcY = 1.0f - V * 2.0f;

            // Unproject to world space (using mid-depth 0.5)
            Math::Vec4 WorldPoint = InvViewProj * Math::Vec4(NdcX, NdcY, 0.5f, 1.0f);
            WorldPoint = WorldPoint / WorldPoint.w;
            m_ScreenPosCache[Idx] = Math::Vec3(WorldPoint.x, WorldPoint.y, WorldPoint.z);
        }
    }
}

const Math::Vec3& VolumetricLighting::GetCachedWorldPos(int X, int Y) const {
    size_t Idx = Y * m_Width + X;
    if (Idx < m_ScreenPosCache.size()) {
        return m_ScreenPosCache[Idx];
    }
    static const Math::Vec3 Default(0.0f, 0.0f, 0.0f);
    return Default;
}

float VolumetricLighting::LookupFalloff(float Distance) const {
    float Normalized = Distance / FALLOFF_LUT_MAX_DIST;
    int Index = static_cast<int>(Normalized * (FALLOFF_LUT_SIZE - 1));
    Index = std::max(0, std::min(Index, FALLOFF_LUT_SIZE - 1));
    return m_FalloffLUT[Index];
}

uint16_t VolumetricLighting::FastFloatToHalf(float Value) const {
    // Clamp to reasonable range
    Value = std::max(0.0f, std::min(Value, 65504.0f));

    // Use LUT for common values, fallback to bx::halfFromFloat
    if (Value >= 0.0f && Value <= 255.0f) {
        int Index = static_cast<int>(Value);
        if (Index < 256) {
            return m_FloatToHalfLUT[Index];
        }
    }

    return bx::halfFromFloat(Value);
}

// ============================================================================
// Tiered Processing
// ============================================================================

ProcessingTier VolumetricLighting::DetermineProcessingTier(size_t LightCount) const {
    if (LightCount <= 8) {
        return ProcessingTier::Fast;
    } else if (LightCount <= 32) {
        return ProcessingTier::Batched;
    } else {
        return ProcessingTier::Clustered;
    }
}

void VolumetricLighting::ClusterLights(const std::vector<Physics::LightSource>& Lights,
                                       std::vector<LightCluster>& OutClusters,
                                       int MaxClusters) {
    OutClusters.clear();
    if (Lights.empty()) return;

    // Simple grid-based clustering
    const float CellSize = 10.0f; // 10 unit cells
    std::unordered_map<int64_t, std::vector<size_t>> GridCells;

    // Assign lights to grid cells
    for (size_t I = 0; I < Lights.size(); I++) {
        const auto& Light = Lights[I];
        int64_t CellHash = HashGridCell(Light.Position, CellSize);
        GridCells[CellHash].push_back(I);
    }

    // Create clusters from grid cells
    OutClusters.reserve(std::min(static_cast<size_t>(MaxClusters), GridCells.size()));

    for (const auto& [CellHash, LightIndices] : GridCells) {
        if (OutClusters.size() >= static_cast<size_t>(MaxClusters)) break;

        LightCluster Cluster;
        Cluster.LightCount = static_cast<int>(LightIndices.size());

        Math::Vec3 TotalPos(0.0f, 0.0f, 0.0f);
        Math::Vec3 TotalColor(0.0f, 0.0f, 0.0f);
        float TotalIntensity = 0.0f;
        float MaxRange = 0.0f;

        for (size_t LightIdx : LightIndices) {
            const auto& Light = Lights[LightIdx];
            TotalPos = TotalPos + Light.Position;
            TotalColor = TotalColor + Light.Color * Light.Intensity;
            TotalIntensity += Light.Intensity;
            MaxRange = std::max(MaxRange, Light.Range);
        }

        Cluster.CentroidPosition = TotalPos / static_cast<float>(Cluster.LightCount);
        Cluster.AverageColor = TotalColor / TotalIntensity;
        Cluster.TotalIntensity = TotalIntensity;
        Cluster.EffectiveRange = MaxRange;

        OutClusters.push_back(Cluster);
    }
}

int64_t VolumetricLighting::HashGridCell(const Math::Vec3& Pos, float CellSize) {
    int32_t X = static_cast<int32_t>(std::floor(Pos.x / CellSize));
    int32_t Y = static_cast<int32_t>(std::floor(Pos.y / CellSize));
    int32_t Z = static_cast<int32_t>(std::floor(Pos.z / CellSize));

    // Simple hash combining X, Y, Z
    int64_t Hash = static_cast<int64_t>(X);
    Hash = (Hash << 20) ^ static_cast<int64_t>(Y);
    Hash = (Hash << 20) ^ static_cast<int64_t>(Z);
    return Hash;
}

// ============================================================================
// Async Processing
// ============================================================================

void VolumetricLighting::ProcessTile_Async(int TileX, int TileY, int WriteBuffer,
                                           const std::vector<Physics::LightSource>& Lights,
                                           const std::vector<LightCluster>& Clusters,
                                           ProcessingTier Tier,
                                           const Math::Vec3& CameraPos,
                                           const Math::Matrix4& InvViewProj) {
    auto& Buffer = m_AsyncBuffers[WriteBuffer];

    int StartX = TileX * TILE_SIZE;
    int StartY = TileY * TILE_SIZE;
    int EndX = std::min(StartX + TILE_SIZE, static_cast<int>(m_Width));
    int EndY = std::min(StartY + TILE_SIZE, static_cast<int>(m_Height));

    // Compute view-projection for ray generation
    Math::Matrix4 ViewProj = InvViewProj.Inverse();

    // Generate hardcoded radial rays for each light (same as sync path)
    std::vector<std::vector<Math::Vec2>> LightRayDirs;
    std::vector<Math::Vec2> LightScreenPositions;
    LightRayDirs.reserve(Lights.size());
    LightScreenPositions.reserve(Lights.size());

    for (const auto& Light : Lights) {
        std::vector<Math::Vec2> RayDirs;
        Math::Vec2 LightScreenPos(0.0f, 0.0f);
        bool Valid = GenerateRadialRays(Light, ViewProj, LightScreenPos, RayDirs);
        LightRayDirs.push_back(std::move(RayDirs));
        LightScreenPositions.push_back(Valid ? LightScreenPos : Math::Vec2(-1.0f, -1.0f));
    }

    // Get stochastic mask for this frame
    int MaskIdx = m_FrameCounter % NUM_STOCHASTIC_MASKS;
    const auto& StochasticMask = m_PrecomputedMasks[MaskIdx];

    for (int Y = StartY; Y < EndY; Y++) {
        for (int X = StartX; X < EndX; X++) {
            size_t Idx = Y * m_Width + X;

            // Check stochastic mask
            if (!StochasticMask[Idx]) {
                continue;
            }

            Math::Vec3 AccumulatedColor(0.0f, 0.0f, 0.0f);

            // Process each light with its hardcoded radial rays (same as sync path)
            for (size_t LightIdx = 0; LightIdx < Lights.size() && LightIdx < LightRayDirs.size(); LightIdx++) {
                const auto& Light = Lights[LightIdx];
                const auto& RayDirs = LightRayDirs[LightIdx];
                if (RayDirs.empty()) continue;
                const Math::Vec2& LightScreenPos = LightScreenPositions[LightIdx];

                // Sample a subset of rays (stochastic selection for quality)
                int RaysToSample = std::min(8, static_cast<int>(RayDirs.size())); // Increased from 4
                for (int RayIdx = 0; RayIdx < RaysToSample; RayIdx++) {
                    // Use blue noise to select which ray to sample
                    uint32_t NoiseX = (X + m_FrameCounter * 7) % m_BlueNoiseSize;
                    uint32_t NoiseY = (Y + m_FrameCounter * 11) % m_BlueNoiseSize;
                    float Noise = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];
                    int SelectedRay = static_cast<int>(Noise * RayDirs.size()) % static_cast<int>(RayDirs.size());

                    if (SelectedRay >= static_cast<int>(RayDirs.size())) continue;

                    // March screen-space ray (proper ray marching for godrays)
                    Math::Vec3 RayColor = MarchScreenSpaceRay(X, Y, RayDirs[SelectedRay], LightScreenPos, Light, InvViewProj);
                    AccumulatedColor = AccumulatedColor + RayColor;
                }
            }

            // Validate accumulated color: check for NaN, Inf, and clamp to reasonable range
            if (std::isnan(AccumulatedColor.x) || std::isnan(AccumulatedColor.y) || std::isnan(AccumulatedColor.z) ||
                !std::isfinite(AccumulatedColor.x) || !std::isfinite(AccumulatedColor.y) || !std::isfinite(AccumulatedColor.z)) {
                AccumulatedColor = Math::Vec3(0.0f, 0.0f, 0.0f);
            }

            // Apply density multiplier
            AccumulatedColor = AccumulatedColor * m_Density;

            // Hardcoded beam shaping for early-00s style: boost bright streaks, suppress haze
            float Luma = (AccumulatedColor.x + AccumulatedColor.y + AccumulatedColor.z) * 0.3333f;
            if (Luma < 0.002f) {
                AccumulatedColor = Math::Vec3(0.0f, 0.0f, 0.0f);
            } else {
                float BeamBoost = 1.2f + std::sqrt(Luma) * 1.5f;
                AccumulatedColor = AccumulatedColor * BeamBoost;
            }

            // Clamp to reasonable range (0 to 1000) to prevent overflow
            AccumulatedColor.x = std::max(0.0f, std::min(1000.0f, AccumulatedColor.x));
            AccumulatedColor.y = std::max(0.0f, std::min(1000.0f, AccumulatedColor.y));
            AccumulatedColor.z = std::max(0.0f, std::min(1000.0f, AccumulatedColor.z));

            // Store result
            Buffer.R[Idx] = AccumulatedColor.x;
            Buffer.G[Idx] = AccumulatedColor.y;
            Buffer.B[Idx] = AccumulatedColor.z;
        }
    }
}

void VolumetricLighting::AccumulateTemporal_Async(int WriteBuffer) {
    auto& Buffer = m_AsyncBuffers[WriteBuffer];
    float BlendFactor = 1.0f / static_cast<float>(m_TemporalFrames);
    float PrevFactor = 1.0f - BlendFactor;

    size_t BufferSize = m_Width * m_Height;

    // Get stochastic mask
    int MaskIdx = m_FrameCounter % NUM_STOCHASTIC_MASKS;
    const auto& StochasticMask = m_PrecomputedMasks[MaskIdx];

    for (size_t Idx = 0; Idx < BufferSize; Idx++) {
        if (!StochasticMask[Idx]) {
            // Use previous frame value
            Buffer.R[Idx] = m_PrevBufferR[Idx];
            Buffer.G[Idx] = m_PrevBufferG[Idx];
            Buffer.B[Idx] = m_PrevBufferB[Idx];
        } else {
            // Blend with previous
            Buffer.R[Idx] = Buffer.R[Idx] * BlendFactor + m_PrevBufferR[Idx] * PrevFactor;
            Buffer.G[Idx] = Buffer.G[Idx] * BlendFactor + m_PrevBufferG[Idx] * PrevFactor;
            Buffer.B[Idx] = Buffer.B[Idx] * BlendFactor + m_PrevBufferB[Idx] * PrevFactor;
        }
    }
}

void VolumetricLighting::TraceVolumetricRays(
    const std::vector<Physics::LightSource>& Lights,
    const Math::Vec3& CameraPos,
    const Math::Matrix4& ViewProj) {

    // Sanity checks
    if (m_VolumetricBufferR.empty()) return;
    if (!m_PrecomputeReady.load(std::memory_order_acquire)) {
        UploadTexture();
        return;
    }

    // Validate and filter lights
    std::vector<Physics::LightSource> ValidLights = ValidateLights(Lights);
    if (ValidLights.empty()) {
        // No valid lights - skip processing but still upload previous frame's texture
        static int WarningCount = 0;
        if (WarningCount++ % 60 == 0) { // Log once per second at 60fps
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting - No valid lights found! Input had ") +
                      std::to_string(Lights.size()) + std::string(" lights, all were filtered out."));
        }
        UploadTexture();
        return;
    }

    // Warn if density or exposure are very low
    static int LowParamWarningCount = 0;
    if (LowParamWarningCount++ % 300 == 0) { // Log every 5 seconds
        if (m_Density < 0.1f) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting density is very low (") +
                      std::to_string(m_Density) + std::string("). Godrays may be too faint!"));
        }
        if (m_Exposure < 0.1f) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting exposure is very low (") +
                      std::to_string(m_Exposure) + std::string("). Godrays may be too faint!"));
        }
    }

    // Force synchronous processing for:
    // 1. Warmup: First 3 frames to ensure valid data
    // 2. Minimal lighting: ≤4 lights to eliminate async overhead
    bool ForceSync = (m_FrameCounter < 3) || (ValidLights.size() <= 4);

    // Try to start async processing (skip if forcing sync)
    if (!ForceSync && !m_TraceGuard.TryExecute(100)) {
        // Previous async trace still running, use previous frame's results
        UploadTexture();
        return;
    }

    // Swap buffers
    int WriteBuffer = 1 - m_CurrentReadBuffer;
    auto& Buffer = m_AsyncBuffers[WriteBuffer];
    Buffer.Ready.store(false, std::memory_order_release);
    Buffer.Clear();

    // Copy previous frame for temporal stability
    m_PrevBufferR = m_VolumetricBufferR;
    m_PrevBufferG = m_VolumetricBufferG;
    m_PrevBufferB = m_VolumetricBufferB;

    // Precompute frame data
    Math::Matrix4 InvViewProj = ViewProj.Inverse();
    UpdateScreenPosCache(InvViewProj);

    // Determine processing tier and cluster lights if needed (use validated lights)
    ProcessingTier Tier = DetermineProcessingTier(ValidLights.size());
    std::vector<LightCluster> Clusters;
    if (Tier == ProcessingTier::Clustered) {
        ClusterLights(ValidLights, Clusters, MAX_CLUSTERS);
    }

    // Submit tile batches to JobSystem
    int TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
    int TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;
    int TotalTiles = TilesX * TilesY;

    m_PendingTileFutures.clear();
    m_PendingTileFutures.reserve((TotalTiles + TILES_PER_JOB - 1) / TILES_PER_JOB);

    // Use synchronous path for warmup or minimal lighting
    if (ForceSync) {
        // Synchronous path - process all tiles immediately
        std::fill(m_VolumetricBufferR.begin(), m_VolumetricBufferR.end(), 0.0f);
        std::fill(m_VolumetricBufferG.begin(), m_VolumetricBufferG.end(), 0.0f);
        std::fill(m_VolumetricBufferB.begin(), m_VolumetricBufferB.end(), 0.0f);

        GenerateStochasticMask(m_Width, m_Height, m_FrameCounter);

        // Process all tiles synchronously using validated lights
        for (int TileY = 0; TileY < TilesY; TileY++) {
            for (int TileX = 0; TileX < TilesX; TileX++) {
                ProcessTile(TileX, TileY, ValidLights, CameraPos, InvViewProj);
            }
        }

        AccumulateTemporal();

        // Validate data after processing completes
        bool DataValid = ValidateLightingData();
        if (!DataValid) {
            static int InvalidDataWarningCount = 0;
            if (InvalidDataWarningCount++ % 60 == 0) {
                SIMPLE_LOG(std::string("WARNING: VolumetricLighting synchronous path produced invalid data (NaN/Inf). ") +
                          std::string("Frame: ") + std::to_string(m_FrameCounter));
            }
        }

        // Upload immediately after validation
        UploadTexture();
        m_FrameCounter++;
        return;
    }

    try {
        // IMPORTANT: Capture ValidLights, Clusters, CameraPos, and InvViewProj BY VALUE
        // These are local/parameter variables that go out of scope before async jobs complete
        for (int JobIdx = 0; JobIdx < (TotalTiles + TILES_PER_JOB - 1) / TILES_PER_JOB; JobIdx++) {
            auto Future = Core::JobSystem::Instance().SubmitAsync(
                [this, JobIdx, WriteBuffer, ValidLights, Clusters, Tier, CameraPos, InvViewProj, TilesX, TilesY, TotalTiles]() {
                    int StartTile = JobIdx * TILES_PER_JOB;
                    int EndTile = std::min(StartTile + TILES_PER_JOB, TotalTiles);

                    for (int TileIdx = StartTile; TileIdx < EndTile; TileIdx++) {
                        int TileX = TileIdx % TilesX;
                        int TileY = TileIdx / TilesX;

                        ProcessTile_Async(TileX, TileY, WriteBuffer,
                                          ValidLights, Clusters, Tier,
                                          CameraPos, InvViewProj);
                    }
                });
            m_PendingTileFutures.push_back(std::move(Future));
        }

        // Submit completion job that swaps buffers
        Core::JobSystem::Instance().Submit([this, WriteBuffer]() {
            // Wait for all tile jobs
            for (auto& F : m_PendingTileFutures) {
                if (F.valid()) F.wait();
            }

            // Apply temporal accumulation
            AccumulateTemporal_Async(WriteBuffer);

            // Copy results to main buffers for validation
            const auto& Buffer = m_AsyncBuffers[WriteBuffer];
            m_VolumetricBufferR = Buffer.R;
            m_VolumetricBufferG = Buffer.G;
            m_VolumetricBufferB = Buffer.B;

            // Validate data before marking as ready
            // If invalid, clear buffers to prevent uploading corrupted data
            bool DataValid = ValidateLightingData();
            if (!DataValid) {
                static int AsyncInvalidWarningCount = 0;
                if (AsyncInvalidWarningCount++ % 60 == 0) {
                    SIMPLE_LOG(std::string("WARNING: VolumetricLighting async path produced invalid data (NaN/Inf). ") +
                              std::string("Clearing buffers."));
                }
                // Clear invalid data - will use previous frame's texture
                std::fill(m_VolumetricBufferR.begin(), m_VolumetricBufferR.end(), 0.0f);
                std::fill(m_VolumetricBufferG.begin(), m_VolumetricBufferG.end(), 0.0f);
                std::fill(m_VolumetricBufferB.begin(), m_VolumetricBufferB.end(), 0.0f);
            }

            // Mark buffer ready (even if invalid - UploadTexture will handle it)
            m_AsyncBuffers[WriteBuffer].Ready.store(true, std::memory_order_release);

            // Release execution guard
            m_TraceGuard.Release();
        });
    } catch (...) {
        // JobSystem not initialized, fall back to synchronous
        m_TraceGuard.Release();

        // Fallback to old synchronous path (use validated lights)
        std::fill(m_VolumetricBufferR.begin(), m_VolumetricBufferR.end(), 0.0f);
        std::fill(m_VolumetricBufferG.begin(), m_VolumetricBufferG.end(), 0.0f);
        std::fill(m_VolumetricBufferB.begin(), m_VolumetricBufferB.end(), 0.0f);

        GenerateStochasticMask(m_Width, m_Height, m_FrameCounter);

        int TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
        int TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;

        for (int TileY = 0; TileY < TilesY; TileY++) {
            for (int TileX = 0; TileX < TilesX; TileX++) {
                ProcessTile(TileX, TileY, ValidLights, CameraPos, InvViewProj);
            }
        }

        AccumulateTemporal();

        // Validate data after fallback processing
        // Always upload (UploadTexture will validate), but increment counter to prevent getting stuck
        UploadTexture();
    }

    // Upload texture (uses previous frame's results if async not ready)
    UploadTexture();

    m_FrameCounter++;
}

void VolumetricLighting::UploadTexture() {
    if (!bgfx::isValid(m_VolumetricTexture)) return;

    // Check if async buffer is ready, otherwise use current buffer
    int ReadBuffer = m_CurrentReadBuffer;
    if (m_AsyncBuffers[ReadBuffer].Ready.load(std::memory_order_acquire)) {
        // Use async buffer
        const auto& Buffer = m_AsyncBuffers[ReadBuffer];
        m_VolumetricBufferR = Buffer.R;
        m_VolumetricBufferG = Buffer.G;
        m_VolumetricBufferB = Buffer.B;

        // Swap to other buffer for next read
        if (m_AsyncBuffers[1 - ReadBuffer].Ready.load(std::memory_order_acquire)) {
            m_CurrentReadBuffer = 1 - ReadBuffer;
        }
    }

    // Pack RGB into RGBA16F
    // Note: We always upload (even if validation fails) because packing handles NaN/Inf by converting to 0
    std::vector<uint16_t> PackedData(m_Width * m_Height * 4);

    // Track statistics for warning
    size_t NonZeroPackedCount = 0;
    float MaxPackedValue = 0.0f;
    size_t ClampedCount = 0;

    for (size_t Idx = 0; Idx < m_Width * m_Height; Idx++) {
        // Additional safety: clamp values to reasonable range and check for NaN/Inf
        float R = m_VolumetricBufferR[Idx];
        float G = m_VolumetricBufferG[Idx];
        float B = m_VolumetricBufferB[Idx];

        bool WasClamped = false;
        // Clamp to reasonable range (0 to 1000) and ensure finite
        // If data was invalid, ensure we at least upload zeros (not NaN/Inf)
        float OriginalR = R;
        float OriginalG = G;
        float OriginalB = B;
        R = std::isfinite(R) ? std::max(0.0f, std::min(1000.0f, R)) : 0.0f;
        G = std::isfinite(G) ? std::max(0.0f, std::min(1000.0f, G)) : 0.0f;
        B = std::isfinite(B) ? std::max(0.0f, std::min(1000.0f, B)) : 0.0f;

        if (R != OriginalR || G != OriginalG || B != OriginalB) {
            WasClamped = true;
            ClampedCount++;
        }

        float Magnitude = std::sqrt(R * R + G * G + B * B);
        if (Magnitude > 0.0001f) {
            NonZeroPackedCount++;
            MaxPackedValue = std::max(MaxPackedValue, Magnitude);
        }

        PackedData[Idx * 4 + 0] = FastFloatToHalf(R);
        PackedData[Idx * 4 + 1] = FastFloatToHalf(G);
        PackedData[Idx * 4 + 2] = FastFloatToHalf(B);
        PackedData[Idx * 4 + 3] = FastFloatToHalf(1.0f);
    }

    // Warn if packed data is too faint (every 5 seconds)
    static int PackedWarningCount = 0;
    if (PackedWarningCount++ % 300 == 0) {
        float NonZeroRatio = static_cast<float>(NonZeroPackedCount) / static_cast<float>(m_Width * m_Height);
        if (NonZeroPackedCount == 0) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting texture is all zeros! No godrays will be visible."));
        } else if (MaxPackedValue < 0.01f) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting texture values are very faint! Max: ") +
                      std::to_string(MaxPackedValue) + std::string(", Non-zero: ") +
                      std::to_string(NonZeroRatio * 100.0f) + std::string("%"));
        } else if (NonZeroRatio < 0.05f && MaxPackedValue < 0.1f) {
            SIMPLE_LOG(std::string("WARNING: VolumetricLighting texture is sparse and faint! Max: ") +
                      std::to_string(MaxPackedValue) + std::string(", Coverage: ") +
                      std::to_string(NonZeroRatio * 100.0f) + std::string("%"));
        }
        if (ClampedCount > 0) {
            float ClampRatio = static_cast<float>(ClampedCount) / static_cast<float>(m_Width * m_Height);
            if (ClampRatio > 0.01f) {
                SIMPLE_LOG(std::string("WARNING: VolumetricLighting clamped ") + std::to_string(ClampRatio * 100.0f) +
                          std::string("% of pixels (values out of range or NaN/Inf)."));
            }
        }
    }

    const bgfx::Memory* Mem = bgfx::copy(PackedData.data(),
                                          static_cast<uint32_t>(PackedData.size() * sizeof(uint16_t)));
    bgfx::updateTexture2D(m_VolumetricTexture, 0, 0, 0, 0,
                          static_cast<uint16_t>(m_Width),
                          static_cast<uint16_t>(m_Height),
                          Mem);
}

} // namespace Solstice::Render
