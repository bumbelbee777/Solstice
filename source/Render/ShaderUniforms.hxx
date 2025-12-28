#pragma once

#include "../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <string>

namespace Solstice::Render {

// Centralized shader uniform definitions
// All uniforms are created lazily on first use and cached
class SOLSTICE_API ShaderUniforms {
public:
    // Shadow uniforms
    static bgfx::UniformHandle GetShadowMatrix();
    static bgfx::UniformHandle GetShadowParams();
    static bgfx::UniformHandle GetShadowSampler();

    // Material uniforms
    static bgfx::UniformHandle GetAlbedoColor();
    static bgfx::UniformHandle GetMaterialParams();
    static bgfx::UniformHandle GetAlbedoSampler();

    // Post-processing uniforms
    static bgfx::UniformHandle GetPostParams();
    static bgfx::UniformHandle GetColorSampler();
    static bgfx::UniformHandle GetBloomSampler();
    static bgfx::UniformHandle GetShadowRTSampler();
    static bgfx::UniformHandle GetAORTSampler();

    // Cleanup (call on shutdown)
    static void Shutdown();

private:
    static bgfx::UniformHandle CreateUniform(const char* name, bgfx::UniformType::Enum type);

    // Shadow uniforms
    static bgfx::UniformHandle s_shadowMatrix;
    static bgfx::UniformHandle s_shadowParams;
    static bgfx::UniformHandle s_shadowSampler;

    // Material uniforms
    static bgfx::UniformHandle s_albedoColor;
    static bgfx::UniformHandle s_materialParams;
    static bgfx::UniformHandle s_albedoSampler;

    // Post-processing uniforms
    static bgfx::UniformHandle s_postParams;
    static bgfx::UniformHandle s_colorSampler;
    static bgfx::UniformHandle s_bloomSampler;
    static bgfx::UniformHandle s_shadowRTSampler;
    static bgfx::UniformHandle s_aORTSampler;
};

} // namespace Solstice::Render
