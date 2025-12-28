#pragma once

#include "../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <vector>
#include <unordered_map>
#include <string>

namespace Solstice::Render {

/**
 * UniformBufferManager - Centralized uniform buffer management
 * Packs uniforms into vec4 arrays for better cache locality and reduced state changes
 * Supports batching uniform updates and caching uniform locations
 */
class SOLSTICE_API UniformBufferManager {
public:
    UniformBufferManager();
    ~UniformBufferManager();

    // Initialize with buffer size (in vec4s)
    void Initialize(uint32_t bufferSize = 256); // Default: 256 vec4s = 4KB
    void Shutdown();

    // Packed uniform buffer structure
    // Groups related uniforms together for better cache locality
    struct PackedUniforms {
        // Material uniforms (vec4 aligned)
        Math::Vec4 AlbedoColor;      // RGB + roughness
        Math::Vec4 MaterialParams;    // metallic + unused
        Math::Vec4 TextureBlend;      // blend mode + factor + unused
        
        // Lighting uniforms
        Math::Vec4 LightDir;          // xyz: direction, w: unused
        Math::Vec4 LightColor;        // rgb: color, w: intensity
        Math::Vec4 CameraPos;         // xyz: position, w: unused
        
        // Shadow uniforms
        Math::Matrix4 ShadowMatrix;   // 4x4 matrix (4 vec4s)
        
        // Post-processing uniforms
        Math::Vec4 PostParams;        // exposure + other params
    };

    // Create a uniform buffer handle
    bgfx::UniformHandle CreateUniformBuffer(const std::string& name, uint32_t numVec4s);

    // Get or create a uniform buffer (cached)
    bgfx::UniformHandle GetUniformBuffer(const std::string& name);

    // Update uniform buffer data
    void UpdateUniformBuffer(const std::string& name, const void* data, uint32_t size);

    // Batch uniform updates - collect updates and apply them together
    void BeginBatch();
    void AddUniformUpdate(const std::string& name, const void* data, uint32_t size);
    void EndBatch(); // Applies all batched updates

    // Direct uniform setting (for compatibility)
    void SetUniform(bgfx::UniformHandle handle, const void* value, uint16_t num = 1);

private:
    struct UniformBuffer {
        bgfx::UniformHandle Handle;
        std::vector<uint8_t> Data;
        bool Dirty;
    };

    std::unordered_map<std::string, UniformBuffer> m_Buffers;
    std::vector<std::pair<std::string, std::vector<uint8_t>>> m_BatchUpdates;
    bool m_Batching;
    uint32_t m_BufferSize;
    bool m_Initialized;
};

} // namespace Solstice::Render

