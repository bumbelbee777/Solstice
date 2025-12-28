#pragma once

#include "../Solstice.hxx"
#include "../Core/Material.hxx"
#include "../Core/ResourceHandle.hxx"
#include "../Render/TextureRegistry.hxx"
#include "../Math/Vector.hxx"
#include <string>

namespace Solstice::Game {

// Material configuration for preset creation
struct MaterialConfig {
    Math::Vec3 AlbedoColor{1.0f, 1.0f, 1.0f};
    float Roughness{0.5f};
    float Metallic{0.0f};
    Math::Vec3 EmissionColor{0.0f, 0.0f, 0.0f};
    float EmissionStrength{0.0f};
    
    // Texture handles (use TextureHandle::Invalid() for no texture)
    Core::TextureHandle AlbedoTexture{Core::TextureHandle::Invalid()};
    Core::TextureHandle DetailTexture{Core::TextureHandle::Invalid()};
    Core::TextureHandle BlendMaskTexture{Core::TextureHandle::Invalid()};
    
    // Texture blending
    uint8_t TextureBlendMode{0}; // Core::TextureBlendMode enum
    float TextureBlendFactor{0.5f};
    
    // Material properties
    uint16_t Flags{0}; // Core::MaterialFlags
    uint8_t AlphaMode{0}; // Core::AlphaMode
    uint8_t ShadingModel{0}; // Core::ShadingModel
    
    // Optional: path to load/save material
    std::string MaterialPath;
};

// Material preset manager - handles atomic material creation and texture assignment
// Materials are owned by MaterialLibrary, textures by TextureRegistry
// Handles are references to these resources
class SOLSTICE_API MaterialPresetManager {
public:
    MaterialPresetManager();
    ~MaterialPresetManager() = default;

    // Set the material library (must be called before creating materials)
    void SetMaterialLibrary(Core::MaterialLibrary* MaterialLibrary) { m_MaterialLibrary = MaterialLibrary; }

    // Set the texture registry (must be called before creating materials with textures)
    void SetTextureRegistry(Render::TextureRegistry* TextureRegistry) { m_TextureRegistry = TextureRegistry; }

    // Create a material with textures atomically
    // Returns MaterialHandle (Invalid if creation failed)
    // Materials are owned by MaterialLibrary, handles are references
    Core::MaterialHandle CreateMaterialWithTextures(const MaterialConfig& Config);

    // Create a simple material without textures
    // Returns MaterialHandle (Invalid if creation failed)
    Core::MaterialHandle CreateMaterial(const MaterialConfig& Config);

private:
    Core::MaterialLibrary* m_MaterialLibrary{nullptr};
    Render::TextureRegistry* m_TextureRegistry{nullptr};
};

} // namespace Solstice::Game

