#include <Render/Assets/ShaderUniforms.hxx>
#include <Core/Debug/Debug.hxx>

namespace Solstice::Render {

// Static uniform handles
bgfx::UniformHandle ShaderUniforms::s_shadowMatrix = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_shadowParams = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_shadowSampler = BGFX_INVALID_HANDLE;

bgfx::UniformHandle ShaderUniforms::s_albedoColor = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_materialParams = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_albedoSampler = BGFX_INVALID_HANDLE;

bgfx::UniformHandle ShaderUniforms::s_postParams = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_colorSampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_bloomSampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_shadowRTSampler = BGFX_INVALID_HANDLE;
bgfx::UniformHandle ShaderUniforms::s_aORTSampler = BGFX_INVALID_HANDLE;

bgfx::UniformHandle ShaderUniforms::CreateUniform(const char* name, bgfx::UniformType::Enum type) {
    bgfx::UniformHandle handle = bgfx::createUniform(name, type);
    if (!bgfx::isValid(handle)) {
        SIMPLE_LOG("ShaderUniforms: Failed to create uniform '" + std::string(name) + "'");
    }
    return handle;
}

bgfx::UniformHandle ShaderUniforms::GetShadowMatrix() {
    if (!bgfx::isValid(s_shadowMatrix)) {
        s_shadowMatrix = CreateUniform("u_shadowMtx", bgfx::UniformType::Mat4);
    }
    return s_shadowMatrix;
}

bgfx::UniformHandle ShaderUniforms::GetShadowParams() {
    if (!bgfx::isValid(s_shadowParams)) {
        s_shadowParams = CreateUniform("u_shadowParams", bgfx::UniformType::Vec4);
    }
    return s_shadowParams;
}

bgfx::UniformHandle ShaderUniforms::GetShadowSampler() {
    if (!bgfx::isValid(s_shadowSampler)) {
        s_shadowSampler = CreateUniform("s_texShadow", bgfx::UniformType::Sampler);
    }
    return s_shadowSampler;
}

bgfx::UniformHandle ShaderUniforms::GetAlbedoColor() {
    if (!bgfx::isValid(s_albedoColor)) {
        s_albedoColor = CreateUniform("u_albedoColor", bgfx::UniformType::Vec4);
    }
    return s_albedoColor;
}

bgfx::UniformHandle ShaderUniforms::GetMaterialParams() {
    if (!bgfx::isValid(s_materialParams)) {
        s_materialParams = CreateUniform("u_materialParams", bgfx::UniformType::Vec4);
    }
    return s_materialParams;
}

bgfx::UniformHandle ShaderUniforms::GetAlbedoSampler() {
    if (!bgfx::isValid(s_albedoSampler)) {
        s_albedoSampler = CreateUniform("s_texAlbedo", bgfx::UniformType::Sampler);
    }
    return s_albedoSampler;
}

bgfx::UniformHandle ShaderUniforms::GetPostParams() {
    if (!bgfx::isValid(s_postParams)) {
        s_postParams = CreateUniform("u_postParams", bgfx::UniformType::Vec4);
    }
    return s_postParams;
}

bgfx::UniformHandle ShaderUniforms::GetColorSampler() {
    if (!bgfx::isValid(s_colorSampler)) {
        s_colorSampler = CreateUniform("s_texColor", bgfx::UniformType::Sampler);
    }
    return s_colorSampler;
}

bgfx::UniformHandle ShaderUniforms::GetBloomSampler() {
    if (!bgfx::isValid(s_bloomSampler)) {
        s_bloomSampler = CreateUniform("s_texBloom", bgfx::UniformType::Sampler);
    }
    return s_bloomSampler;
}

bgfx::UniformHandle ShaderUniforms::GetShadowRTSampler() {
    if (!bgfx::isValid(s_shadowRTSampler)) {
        s_shadowRTSampler = CreateUniform("s_texShadowRT", bgfx::UniformType::Sampler);
    }
    return s_shadowRTSampler;
}

bgfx::UniformHandle ShaderUniforms::GetAORTSampler() {
    if (!bgfx::isValid(s_aORTSampler)) {
        s_aORTSampler = CreateUniform("s_texAORT", bgfx::UniformType::Sampler);
    }
    return s_aORTSampler;
}

void ShaderUniforms::Shutdown() {
    if (bgfx::isValid(s_shadowMatrix)) bgfx::destroy(s_shadowMatrix);
    if (bgfx::isValid(s_shadowParams)) bgfx::destroy(s_shadowParams);
    if (bgfx::isValid(s_shadowSampler)) bgfx::destroy(s_shadowSampler);

    if (bgfx::isValid(s_albedoColor)) bgfx::destroy(s_albedoColor);
    if (bgfx::isValid(s_materialParams)) bgfx::destroy(s_materialParams);
    if (bgfx::isValid(s_albedoSampler)) bgfx::destroy(s_albedoSampler);

    if (bgfx::isValid(s_postParams)) bgfx::destroy(s_postParams);
    if (bgfx::isValid(s_colorSampler)) bgfx::destroy(s_colorSampler);
    if (bgfx::isValid(s_bloomSampler)) bgfx::destroy(s_bloomSampler);
    if (bgfx::isValid(s_shadowRTSampler)) bgfx::destroy(s_shadowRTSampler);
    if (bgfx::isValid(s_aORTSampler)) bgfx::destroy(s_aORTSampler);

    s_shadowMatrix = BGFX_INVALID_HANDLE;
    s_shadowParams = BGFX_INVALID_HANDLE;
    s_shadowSampler = BGFX_INVALID_HANDLE;
    s_albedoColor = BGFX_INVALID_HANDLE;
    s_materialParams = BGFX_INVALID_HANDLE;
    s_albedoSampler = BGFX_INVALID_HANDLE;
    s_postParams = BGFX_INVALID_HANDLE;
    s_colorSampler = BGFX_INVALID_HANDLE;
    s_bloomSampler = BGFX_INVALID_HANDLE;
    s_shadowRTSampler = BGFX_INVALID_HANDLE;
    s_aORTSampler = BGFX_INVALID_HANDLE;
}

} // namespace Solstice::Render
