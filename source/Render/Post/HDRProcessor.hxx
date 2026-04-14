#pragma once

#include <Solstice.hxx>
#include <bgfx/bgfx.h>
#include <Core/ML/SIMD.hxx>
#include <Math/Vector.hxx>
#include <cstdint>

namespace Solstice::Render {

// HDR Processor for CPU/iGPU optimization
// Uses tiled processing and SIMD for performance
class SOLSTICE_API HDRProcessor {
public:
    HDRProcessor();
    ~HDRProcessor();

    // Initialize with resolution
    void Initialize(uint32_t width, uint32_t height);

    // Shutdown and cleanup
    void Shutdown();

    // Resize for new resolution
    void Resize(uint32_t width, uint32_t height);

    // Process HDR texture (tile-based with SIMD)
    // Input: RGBA16F texture from scene render
    // Output: Processed HDR texture ready for tone mapping
    void Process(bgfx::TextureHandle inputTexture, bgfx::TextureHandle outputTexture, float exposure);

    // Get processed texture
    bgfx::TextureHandle GetProcessedTexture() const { return m_processedTexture; }

    // Settings
    void SetExposure(float exposure) { m_exposure = exposure; }
    float GetExposure() const { return m_exposure; }

    void SetTileSize(uint32_t size) { m_tileSize = size; }
    uint32_t GetTileSize() const { return m_tileSize; }

private:
    // Tile-based processing
    void ProcessTile(uint32_t tileX, uint32_t tileY, const float* inputData, float* outputData, uint32_t stride);

    // SIMD-optimized luminance calculation (4 pixels at once)
    void CalculateLuminanceSIMD(const Core::SIMD::Vec4* colors, float* luminances, size_t count);

    // SIMD-optimized exposure adjustment (4 pixels at once)
    void ApplyExposureSIMD(Core::SIMD::Vec4* colors, float exposure, size_t count);

    // Fast approximations for HDR operations (non-static to access LUTs)
    float FastLog(float x);
    float FastExp(float x);

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_tileSize = 32; // 32x32 tiles for good cache locality
    float m_exposure = 1.0f;

    bgfx::TextureHandle m_processedTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_processedFB = BGFX_INVALID_HANDLE;

    // Lookup tables for fast approximations
    static constexpr size_t LUT_SIZE = 256;
    float m_logLUT[LUT_SIZE];
    float m_expLUT[LUT_SIZE];
    bool m_LUTsInitialized = false;

    void InitializeLUTs();
};

} // namespace Solstice::Render
