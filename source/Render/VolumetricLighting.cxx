#include "VolumetricLighting.hxx"
#include "Scene.hxx"
#include <Core/Debug.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace Solstice::Render {

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

    // Calculate rows per Y slice (each uint64_t holds 64 bits)
    RowsPerY = (ResolutionX + 63) / 64;

    // Allocate flattened bitmask storage
    size_t TotalSize = static_cast<size_t>(ResolutionZ) * ResolutionY * RowsPerY;
    Bitmasks.resize(TotalSize, 0);
}

void LightOcclusionGrid::WorldToVoxel(const Math::Vec3& WorldPos,
                                      int32_t& OutVX, int32_t& OutVY, int32_t& OutVZ) const {
    Math::Vec3 LocalPos = WorldPos - WorldMin;
    OutVX = static_cast<int32_t>(LocalPos.x / VoxelSize.x);
    OutVY = static_cast<int32_t>(LocalPos.y / VoxelSize.y);
    OutVZ = static_cast<int32_t>(LocalPos.z / VoxelSize.z);

    // Clamp to grid bounds
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

// ============================================================================
// VolumetricLighting Implementation
// ============================================================================

VolumetricLighting::VolumetricLighting() = default;

VolumetricLighting::~VolumetricLighting() {
    Shutdown();
}

void VolumetricLighting::Initialize(uint32_t Width, uint32_t Height,
                                    const Math::Vec3& WorldMin, const Math::Vec3& WorldMax) {
    m_Width = Width / 4;   // Quarter resolution for better performance
    m_Height = Height / 4;

    // Initialize occlusion grid
    m_OcclusionGrid.Initialize(WorldMin, WorldMax, 64, 64, 64);

    // Initialize mipmap levels (32³ and 16³)
    m_OcclusionMipmaps.resize(2);
    m_OcclusionMipmaps[0].Initialize(WorldMin, WorldMax, 32, 32, 32);
    m_OcclusionMipmaps[1].Initialize(WorldMin, WorldMax, 16, 16, 16);

    // Allocate volumetric buffers (RGB separate for SIMD)
    size_t BufferSize = m_Width * m_Height;
    m_VolumetricBufferR.resize(BufferSize, 0.0f);
    m_VolumetricBufferG.resize(BufferSize, 0.0f);
    m_VolumetricBufferB.resize(BufferSize, 0.0f);
    m_PrevBufferR.resize(BufferSize, 0.0f);
    m_PrevBufferG.resize(BufferSize, 0.0f);
    m_PrevBufferB.resize(BufferSize, 0.0f);

    // Initialize stochastic sampling resources
    GenerateBlueNoise();
    m_StochasticMask.resize(BufferSize, false);

    // Optimize defaults for performance
    m_NumSamples = 16;  // Reduced from 32
    m_StochasticRatio = 0.25f;  // Reduced from 0.33f (25% coverage)
    m_TemporalFrames = 2;  // Reduced from 3

    // Create BGFX texture
    m_VolumetricTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(m_Width),
        static_cast<uint16_t>(m_Height),
        false, 1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY
    );
}

void VolumetricLighting::Shutdown() {
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
    size_t TotalPixels = Width * Height;
    size_t TargetPixels = static_cast<size_t>(TotalPixels * m_StochasticRatio);

    // Reset mask
    std::fill(m_StochasticMask.begin(), m_StochasticMask.end(), false);

    // Use blue noise + frame offset for temporal distribution
    uint32_t FrameOffset = (Frame % 3) * 7; // Cycle through 3 phases

    for (uint32_t Y = 0; Y < Height; Y++) {
        for (uint32_t X = 0; X < Width; X++) {
            size_t Idx = Y * Width + X;

            // Sample blue noise with frame-based offset
            uint32_t NoiseX = (X + FrameOffset) % m_BlueNoiseSize;
            uint32_t NoiseY = (Y + FrameOffset * 3) % m_BlueNoiseSize;
            float NoiseValue = m_BlueNoiseTexture[NoiseY * m_BlueNoiseSize + NoiseX];

            // Thresholding with ratio
            if (NoiseValue < m_StochasticRatio) {
                m_StochasticMask[Idx] = true;
            }
        }
    }
}

void VolumetricLighting::PackLightsIntoPackets(const std::vector<Physics::LightSource>& Lights,
                                               std::vector<LightPacket>& OutPackets) {
    OutPackets.clear();

    size_t NumPackets = (Lights.size() + 3) / 4;
    OutPackets.resize(NumPackets);

    for (size_t I = 0; I < Lights.size(); I++) {
        size_t PacketIdx = I / 4;
        OutPackets[PacketIdx].AddLight(Lights[I]);
    }
}

float VolumetricLighting::FastAttenuation(float Distance, float Range) const {
    if (Range <= 0.0f) return 1.0f;
    if (Distance >= Range) return 0.0f;

    float NormDist = Distance / Range;
    // Smooth falloff: (1 - d²)²
    float Factor = 1.0f - NormDist * NormDist;
    return Factor * Factor;
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

    // Clear mipmap levels
    for (auto& Mip : m_OcclusionMipmaps) {
        Mip.Clear();
    }

    // Downsample 64³ → 32³
    LightOcclusionGrid& Mip32 = m_OcclusionMipmaps[0];
    for (uint32_t Z = 0; Z < Mip32.ResolutionZ; Z++) {
        for (uint32_t Y = 0; Y < Mip32.ResolutionY; Y++) {
            for (uint32_t X = 0; X < Mip32.ResolutionX; X++) {
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
                    Mip32.MarkOccluder(static_cast<int32_t>(X),
                                       static_cast<int32_t>(Y),
                                       static_cast<int32_t>(Z));
                }
            }
        }
    }

    // Downsample 32³ → 16³
    LightOcclusionGrid& Mip16 = m_OcclusionMipmaps[1];
    for (uint32_t Z = 0; Z < Mip16.ResolutionZ; Z++) {
        for (uint32_t Y = 0; Y < Mip16.ResolutionY; Y++) {
            for (uint32_t X = 0; X < Mip16.ResolutionX; X++) {
                bool Occupied = false;
                for (int DZ = 0; DZ < 2 && !Occupied; DZ++) {
                    for (int DY = 0; DY < 2 && !Occupied; DY++) {
                        for (int DX = 0; DX < 2 && !Occupied; DX++) {
                            if (Mip32.IsOccluded(
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

VolumetricSample VolumetricLighting::MarchRayHierarchical(
    const Math::Vec3& Origin,
    const Math::Vec3& Direction,
    float MaxDistance,
    const std::vector<LightPacket>& NearestLights) {

    VolumetricSample Result;
    Result.Density = 0.0f;
    Result.Color = Math::Vec3(0.0f, 0.0f, 0.0f);

    if (NearestLights.empty()) return Result;

    Math::Vec3 DirNorm = Direction.Normalized();
    float StepSize = MaxDistance / static_cast<float>(m_NumSamples);
    float AccumulatedDensity = 0.0f;
    float Transmittance = 1.0f;
    const float MinTransmittance = 0.005f;  // Early exit threshold (optimized)

    // Adaptive step size - larger steps when transmittance is low
    for (int Step = 0; Step < m_NumSamples && Transmittance > MinTransmittance; Step++) {
        float T = (static_cast<float>(Step) + 0.5f) * StepSize;
        Math::Vec3 SamplePos = Origin + DirNorm * T;

        // Check occlusion at coarse level first (early exit)
        int32_t VX, VY, VZ;
        if (!m_OcclusionMipmaps.empty()) {
            m_OcclusionMipmaps[1].WorldToVoxel(SamplePos, VX, VY, VZ);
            if (m_OcclusionMipmaps[1].IsOccluded(VX, VY, VZ)) {
                // Occluded - no light contribution, decay faster
                Transmittance *= m_Decay * m_Decay;  // Faster decay for occluded regions
                if (Transmittance < MinTransmittance) break;  // Early exit
                continue;
            }
        }

        // Calculate light contribution at sample point
        Math::Vec3 Normal(0.0f, 1.0f, 0.0f); // Approximate normal for volumetric

        for (const auto& Packet : NearestLights) {
            float Contributions[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            Packet.CalculateContribution4(SamplePos, Normal, Contributions);

            for (int I = 0; I < Packet.LightCount; I++) {
                float Contrib = Contributions[I] * m_Density * Transmittance;

                // Early exit if contribution is negligible
                if (Contrib < 0.001f) continue;

                // Get light color
                Math::Vec3 LightColor(Packet.Colors[I].X(),
                                     Packet.Colors[I].Y(),
                                     Packet.Colors[I].Z());

                Result.Color = Result.Color + LightColor * Contrib;
                AccumulatedDensity += Contrib;
            }
        }

        Transmittance *= m_Decay;
    }

    Result.Density = std::min(1.0f, AccumulatedDensity);
    Result.Color = Result.Color * m_Exposure;

    return Result;
}

void VolumetricLighting::ProcessTile(int TileX, int TileY,
                                     const std::vector<LightPacket>& LightPackets,
                                     const Math::Vec3& CameraPos,
                                     const Math::Matrix4& InvViewProj) {
    int StartX = TileX * TILE_SIZE;
    int StartY = TileY * TILE_SIZE;
    int EndX = std::min(StartX + TILE_SIZE, static_cast<int>(m_Width));
    int EndY = std::min(StartY + TILE_SIZE, static_cast<int>(m_Height));

    for (int Y = StartY; Y < EndY; Y++) {
        for (int X = StartX; X < EndX; X++) {
            size_t Idx = Y * m_Width + X;

            // Check stochastic mask
            if (!m_StochasticMask[Idx]) {
                // Use previous frame value
                continue;
            }

            // Convert screen position to world ray
            float U = (static_cast<float>(X) + 0.5f) / static_cast<float>(m_Width);
            float V = (static_cast<float>(Y) + 0.5f) / static_cast<float>(m_Height);

            // NDC coordinates
            float NdcX = U * 2.0f - 1.0f;
            float NdcY = 1.0f - V * 2.0f; // Flip Y

            // Unproject to world space
            Math::Vec4 NearPoint = InvViewProj * Math::Vec4(NdcX, NdcY, -1.0f, 1.0f);
            Math::Vec4 FarPoint = InvViewProj * Math::Vec4(NdcX, NdcY, 1.0f, 1.0f);

            NearPoint = NearPoint / NearPoint.w;
            FarPoint = FarPoint / FarPoint.w;

            Math::Vec3 RayOrigin(NearPoint.x, NearPoint.y, NearPoint.z);
            Math::Vec3 RayEnd(FarPoint.x, FarPoint.y, FarPoint.z);
            Math::Vec3 RayDir = (RayEnd - RayOrigin).Normalized();

            float MaxDist = (RayEnd - RayOrigin).Magnitude();
            MaxDist = std::min(MaxDist, 100.0f); // Cap maximum distance

            // March the ray
            VolumetricSample Sample = MarchRayHierarchical(RayOrigin, RayDir, MaxDist, LightPackets);

            // Store result
            m_VolumetricBufferR[Idx] = Sample.Color.x;
            m_VolumetricBufferG[Idx] = Sample.Color.y;
            m_VolumetricBufferB[Idx] = Sample.Color.z;
        }
    }
}

void VolumetricLighting::TraceVolumetricRays(
    const std::vector<Physics::LightSource>& Lights,
    const Math::Vec3& CameraPos,
    const Math::Matrix4& ViewProj) {

    if (m_VolumetricBufferR.empty() || Lights.empty()) return;

    // Generate stochastic mask for this frame
    GenerateStochasticMask(m_Width, m_Height, m_FrameCounter);

    // Copy previous frame for temporal stability
    m_PrevBufferR = m_VolumetricBufferR;
    m_PrevBufferG = m_VolumetricBufferG;
    m_PrevBufferB = m_VolumetricBufferB;

    // Pack lights into SIMD packets
    std::vector<LightPacket> LightPackets;
    PackLightsIntoPackets(Lights, LightPackets);

    // Compute inverse view-projection for unprojection
    Math::Matrix4 InvViewProj = ViewProj.Inverse();

    // Process tiles in parallel
    int TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
    int TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;

#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic) collapse(2)
#endif
    for (int TileY = 0; TileY < TilesY; TileY++) {
        for (int TileX = 0; TileX < TilesX; TileX++) {
            ProcessTile(TileX, TileY, LightPackets, CameraPos, InvViewProj);
        }
    }

    // Temporal accumulation
    AccumulateTemporal();

    // Apply dithering
    ApplyBlueNoiseDither();

    // Upload to GPU
    UploadTexture();

    m_FrameCounter++;
}

void VolumetricLighting::AccumulateTemporal() {
    float BlendFactor = 1.0f / static_cast<float>(m_TemporalFrames);
    float PrevFactor = 1.0f - BlendFactor;

    size_t BufferSize = m_Width * m_Height;

#ifdef _OPENMP
    #pragma omp parallel for
#endif
    for (int Idx = 0; Idx < static_cast<int>(BufferSize); Idx++) {
        if (!m_StochasticMask[Idx]) {
            // Use previous frame value for non-traced pixels
            m_VolumetricBufferR[Idx] = m_PrevBufferR[Idx];
            m_VolumetricBufferG[Idx] = m_PrevBufferG[Idx];
            m_VolumetricBufferB[Idx] = m_PrevBufferB[Idx];
        } else {
            // Blend with previous frame
            m_VolumetricBufferR[Idx] = m_VolumetricBufferR[Idx] * BlendFactor + m_PrevBufferR[Idx] * PrevFactor;
            m_VolumetricBufferG[Idx] = m_VolumetricBufferG[Idx] * BlendFactor + m_PrevBufferG[Idx] * PrevFactor;
            m_VolumetricBufferB[Idx] = m_VolumetricBufferB[Idx] * BlendFactor + m_PrevBufferB[Idx] * PrevFactor;
        }
    }
}

void VolumetricLighting::ApplyBlueNoiseDither() {
    size_t BufferSize = m_Width * m_Height;
    float DitherStrength = 0.01f;

    for (size_t Idx = 0; Idx < BufferSize; Idx++) {
        uint32_t X = Idx % m_Width;
        uint32_t Y = Idx / m_Width;

        float Noise = m_BlueNoiseTexture[(Y % m_BlueNoiseSize) * m_BlueNoiseSize + (X % m_BlueNoiseSize)];
        Noise = (Noise - 0.5f) * DitherStrength;

        m_VolumetricBufferR[Idx] = std::max(0.0f, m_VolumetricBufferR[Idx] + Noise);
        m_VolumetricBufferG[Idx] = std::max(0.0f, m_VolumetricBufferG[Idx] + Noise);
        m_VolumetricBufferB[Idx] = std::max(0.0f, m_VolumetricBufferB[Idx] + Noise);
    }
}

void VolumetricLighting::UploadTexture() {
    if (!bgfx::isValid(m_VolumetricTexture)) return;

    // Pack RGB into RGBA16F
    std::vector<uint16_t> PackedData(m_Width * m_Height * 4);

    for (size_t Idx = 0; Idx < m_Width * m_Height; Idx++) {
        // Convert float to half-precision (simplified)
        auto FloatToHalf = [](float Value) -> uint16_t {
            // Simplified conversion - just scale and clamp
            Value = std::max(0.0f, std::min(Value, 65000.0f));
            return static_cast<uint16_t>(Value * 1000.0f);
        };

        PackedData[Idx * 4 + 0] = FloatToHalf(m_VolumetricBufferR[Idx]);
        PackedData[Idx * 4 + 1] = FloatToHalf(m_VolumetricBufferG[Idx]);
        PackedData[Idx * 4 + 2] = FloatToHalf(m_VolumetricBufferB[Idx]);
        PackedData[Idx * 4 + 3] = FloatToHalf(1.0f);
    }

    const bgfx::Memory* Mem = bgfx::copy(PackedData.data(),
                                          static_cast<uint32_t>(PackedData.size() * sizeof(uint16_t)));
    bgfx::updateTexture2D(m_VolumetricTexture, 0, 0, 0, 0,
                          static_cast<uint16_t>(m_Width),
                          static_cast<uint16_t>(m_Height),
                          Mem);
}

} // namespace Solstice::Render
