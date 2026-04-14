#include "HDRProcessor.hxx"
#include <Solstice.hxx>
#include <Core/Debug/Debug.hxx>
#include <bgfx/bgfx.h>
#include <cmath>
#include <algorithm>

namespace Solstice::Render {

HDRProcessor::HDRProcessor() {
    InitializeLUTs();
}

HDRProcessor::~HDRProcessor() {
    Shutdown();
}

void HDRProcessor::Initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    // Create processed texture (RGBA16F for HDR)
    uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
    m_processedTexture = bgfx::createTexture2D(
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height),
        false, 1,
        bgfx::TextureFormat::RGBA16F,
        flags,
        nullptr
    );

    m_processedFB = bgfx::createFrameBuffer(1, &m_processedTexture, false);

    if (!bgfx::isValid(m_processedFB)) {
        SIMPLE_LOG("HDRProcessor: Failed to create processed framebuffer");
    }
}

void HDRProcessor::Shutdown() {
    if (bgfx::isValid(m_processedFB)) {
        bgfx::destroy(m_processedFB);
        m_processedFB = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_processedTexture)) {
        bgfx::destroy(m_processedTexture);
        m_processedTexture = BGFX_INVALID_HANDLE;
    }
}

void HDRProcessor::Resize(uint32_t width, uint32_t height) {
    if (m_width == width && m_height == height) return;

    Shutdown();
    Initialize(width, height);
}

void HDRProcessor::InitializeLUTs() {
    if (m_LUTsInitialized) return;

    // Initialize log LUT for range [0.1, 10.0]
    for (size_t i = 0; i < LUT_SIZE; ++i) {
        float x = 0.1f + (10.0f - 0.1f) * (static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1));
        m_logLUT[i] = std::log(x);
    }

    // Initialize exp LUT for range [-2.0, 2.0]
    for (size_t i = 0; i < LUT_SIZE; ++i) {
        float x = -2.0f + 4.0f * (static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1));
        m_expLUT[i] = std::exp(x);
    }

    m_LUTsInitialized = true;
}

float HDRProcessor::FastLog(float x) {
    if (!m_LUTsInitialized) InitializeLUTs();
    // Clamp to valid range
    x = std::max(0.1f, std::min(10.0f, x));

    // Use LUT with linear interpolation
    float t = (x - 0.1f) / (10.0f - 0.1f);
    float index = t * (LUT_SIZE - 1);
    size_t i0 = static_cast<size_t>(index);
    size_t i1 = std::min(i0 + 1, LUT_SIZE - 1);
    float frac = index - static_cast<float>(i0);

    return m_logLUT[i0] * (1.0f - frac) + m_logLUT[i1] * frac;
}

float HDRProcessor::FastExp(float x) {
    if (!m_LUTsInitialized) InitializeLUTs();
    // Clamp to valid range
    x = std::max(-2.0f, std::min(2.0f, x));

    // Use LUT with linear interpolation
    float t = (x - (-2.0f)) / (2.0f - (-2.0f));
    float index = t * (LUT_SIZE - 1);
    size_t i0 = static_cast<size_t>(index);
    size_t i1 = std::min(i0 + 1, LUT_SIZE - 1);
    float frac = index - static_cast<float>(i0);

    return m_expLUT[i0] * (1.0f - frac) + m_expLUT[i1] * frac;
}

void HDRProcessor::CalculateLuminanceSIMD(const Core::SIMD::Vec4* colors, float* luminances, size_t count) {
    // Luminance weights: (0.299, 0.587, 0.114)
    const Core::SIMD::Vec4 weights(0.299f, 0.587f, 0.114f, 0.0f);

    for (size_t i = 0; i < count; ++i) {
        // Dot product for luminance: dot(color.rgb, weights.rgb)
        Core::SIMD::Vec4 weighted = colors[i] * weights;
        luminances[i] = weighted.X() + weighted.Y() + weighted.Z();
    }
}

void HDRProcessor::ApplyExposureSIMD(Core::SIMD::Vec4* colors, float exposure, size_t count) {
    const Core::SIMD::Vec4 expVec(exposure, exposure, exposure, 1.0f);

    for (size_t i = 0; i < count; ++i) {
        colors[i] = colors[i] * expVec;
    }
}

void HDRProcessor::ProcessTile(uint32_t tileX, uint32_t tileY, const float* inputData, float* outputData, uint32_t stride) {
    uint32_t tileStartX = tileX * m_tileSize;
    uint32_t tileStartY = tileY * m_tileSize;
    uint32_t tileEndX = std::min(tileStartX + m_tileSize, m_width);
    uint32_t tileEndY = std::min(tileStartY + m_tileSize, m_height);

    // Process tile in SIMD-friendly chunks (4 pixels at a time)
    constexpr uint32_t SIMD_CHUNK = 4;

    for (uint32_t y = tileStartY; y < tileEndY; ++y) {
        for (uint32_t x = tileStartX; x < tileEndX; x += SIMD_CHUNK) {
            uint32_t pixelsToProcess = std::min(SIMD_CHUNK, tileEndX - x);

            // Load pixels into SIMD vectors
            Core::SIMD::Vec4 pixels[SIMD_CHUNK];
            for (uint32_t p = 0; p < pixelsToProcess; ++p) {
                uint32_t pixelIdx = (y * m_width + (x + p)) * 4;
                pixels[p] = Core::SIMD::Vec4::Load(&inputData[pixelIdx]);
            }

            // Apply exposure using SIMD
            ApplyExposureSIMD(pixels, m_exposure, pixelsToProcess);

            // Store results
            for (uint32_t p = 0; p < pixelsToProcess; ++p) {
                uint32_t pixelIdx = (y * m_width + (x + p)) * 4;
                pixels[p].Store(&outputData[pixelIdx]);
            }
        }
    }
}

void HDRProcessor::Process(bgfx::TextureHandle inputTexture, bgfx::TextureHandle outputTexture, float exposure) {
    if (!bgfx::isValid(inputTexture) || !bgfx::isValid(outputTexture)) {
        return;
    }

    m_exposure = exposure;

    // For now, HDR processing is done in the shader for efficiency
    // This CPU-side processor can be used for more complex operations if needed
    // The main HDR work (exposure, tone mapping) happens in fs_post.sc

    // Future: Could implement CPU-side tile-based processing here if needed
    // For now, we rely on GPU shader for performance
}

} // namespace Solstice::Render
