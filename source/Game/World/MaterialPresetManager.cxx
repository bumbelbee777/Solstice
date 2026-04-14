#include "World/MaterialPresetManager.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../Arzachel/MaterialSerializer.hxx"
#include <filesystem>

namespace Solstice::Game {

MaterialPresetManager::MaterialPresetManager() {
}

Core::MaterialHandle MaterialPresetManager::CreateMaterialWithTextures(const MaterialConfig& Config) {
    if (!m_MaterialLibrary) {
        SIMPLE_LOG("ERROR: MaterialPresetManager::CreateMaterialWithTextures - MaterialLibrary not set");
        return Core::MaterialHandle::Invalid();
    }

    Core::Material Mat;

    // Try to load from disk if path specified
    bool LoadedFromDisk = false;
    if (!Config.MaterialPath.empty() && std::filesystem::exists(Config.MaterialPath)) {
        try {
            Core::Material LoadedMat;
            if (Arzachel::MaterialSerializer::LoadFromFile(Config.MaterialPath, LoadedMat)) {
                Mat = LoadedMat;
                LoadedFromDisk = true;
                SIMPLE_LOG("Loaded material from disk: " + Config.MaterialPath);
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception loading material: " + std::string(e.what()));
        }
    }

    // Set material properties
    if (!LoadedFromDisk) {
        Mat.SetAlbedoColor(Config.AlbedoColor, Config.Roughness);
        Mat.Metallic = static_cast<uint8_t>(Config.Metallic * 255.0f);
        Mat.SetEmission(Config.EmissionColor, Config.EmissionStrength);
        Mat.Flags = Config.Flags;
        Mat.AlphaMode = Config.AlphaMode;
        Mat.ShadingModel = Config.ShadingModel;
    }

    // Assign textures (even if loaded from disk, we may want to override textures)
    if (m_TextureRegistry) {
        if (Config.AlbedoTexture.IsValid()) {
            Mat.AlbedoTexIndex = Config.AlbedoTexture.GetValue();
        } else {
            Mat.AlbedoTexIndex = 0xFFFF; // Invalid texture index
        }

        if (Config.DetailTexture.IsValid()) {
            Mat.AlbedoTexIndex2 = Config.DetailTexture.GetValue();
        } else {
            Mat.AlbedoTexIndex2 = 0xFFFF;
        }

        if (Config.BlendMaskTexture.IsValid()) {
            Mat.AlbedoTexIndex3 = Config.BlendMaskTexture.GetValue();
        } else {
            Mat.AlbedoTexIndex3 = 0xFFFF;
        }
    }

    // Set texture blending
    Mat.TextureBlendMode = Config.TextureBlendMode;
    Mat.SetTextureBlendFactor(Config.TextureBlendFactor);

    // Add material to library
    uint32_t MaterialID = m_MaterialLibrary->AddMaterial(Mat);
    return Core::MaterialHandle(MaterialID);
}

Core::MaterialHandle MaterialPresetManager::CreateMaterial(const MaterialConfig& Config) {
    if (!m_MaterialLibrary) {
        SIMPLE_LOG("ERROR: MaterialPresetManager::CreateMaterial - MaterialLibrary not set");
        return Core::MaterialHandle::Invalid();
    }

    Core::Material Mat;

    // Try to load from disk if path specified
    bool LoadedFromDisk = false;
    if (!Config.MaterialPath.empty() && std::filesystem::exists(Config.MaterialPath)) {
        try {
            Core::Material LoadedMat;
            if (Arzachel::MaterialSerializer::LoadFromFile(Config.MaterialPath, LoadedMat)) {
                Mat = LoadedMat;
                LoadedFromDisk = true;
                SIMPLE_LOG("Loaded material from disk: " + Config.MaterialPath);
            }
        } catch (const std::exception& e) {
            SIMPLE_LOG("ERROR: Exception loading material: " + std::string(e.what()));
        }
    }

    // Set material properties
    if (!LoadedFromDisk) {
        Mat.SetAlbedoColor(Config.AlbedoColor, Config.Roughness);
        Mat.Metallic = static_cast<uint8_t>(Config.Metallic * 255.0f);
        Mat.SetEmission(Config.EmissionColor, Config.EmissionStrength);
        Mat.Flags = Config.Flags;
        Mat.AlphaMode = Config.AlphaMode;
        Mat.ShadingModel = Config.ShadingModel;
    }

    // Add material to library
    uint32_t MaterialID = m_MaterialLibrary->AddMaterial(Mat);
    return Core::MaterialHandle(MaterialID);
}

} // namespace Solstice::Game

