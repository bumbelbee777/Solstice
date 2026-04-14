#pragma once

#include <Math/Vector.hxx>
#include <cstdint>
#include <vector>

namespace Solstice::Core {
    class JSONValue;

// Compact material representation - exactly 48 bytes (on 64-bit)
struct MaterialExtras {
    float Buoyancy = 0.0f;
    float Flammability = 0.0f;
    float Conductiveness = 0.0f;
    float Density = 1.0f;
};

struct Material {
    // Albedo color (RGB) + roughness (A)
    uint32_t AlbedoRGBA;        // 4 bytes - packed 8-bit RGBA

    // Normal map index + metallic + specular power
    uint16_t NormalMapIndex;    // 2 bytes - texture index (65k textures max)
    uint8_t Metallic;           // 1 byte - 0-255 metallic factor
    uint8_t SpecularPower;      // 1 byte - Blinn-Phong exponent (scaled)

    // Emission color (RGB) + emission strength
    uint32_t EmissionRGBA;      // 4 bytes - packed 8-bit RGBA

    // Texture indices
    uint16_t AlbedoTexIndex;    // 2 bytes
    uint16_t RoughnessTexIndex; // 2 bytes

    // Material flags and properties
    uint16_t Flags;             // 2 bytes - see MaterialFlags
    uint8_t AlphaMode;          // 1 byte - see AlphaMode enum
    uint8_t ShadingModel;       // 1 byte - see ShadingModel enum

    // Lightmap UV scale/offset (packed)
    uint16_t LightmapScaleX;    // 2 bytes - fixed-point 8.8
    uint16_t LightmapScaleY;    // 2 bytes
    uint16_t LightmapOffsetX;   // 2 bytes
    uint16_t LightmapOffsetY;   // 2 bytes

    // UV tiling/offset for albedo (packed)
    uint16_t UVScaleX;          // 2 bytes - fixed-point 8.8
    uint16_t UVScaleY;          // 2 bytes

    // Texture stacking support (additional texture layers)
    uint16_t AlbedoTexIndex2;   // 2 bytes - detail texture layer
    uint16_t AlbedoTexIndex3;   // 2 bytes - blend mask texture
    uint8_t TextureBlendMode;   // 1 byte - blend mode enum
    uint8_t TextureBlendFactor; // 1 byte - blend strength (0-255, represents 0.0-1.0)
    uint8_t Opacity;            // 1 byte - Opacity (0 = transparent, 255 = opaque)
    uint8_t _padding[1];        // 1 byte padding for alignment

    // Optional physical properties (nullptr by default)
    MaterialExtras* Extras;     // 8 bytes (on 64-bit)

    Material()
        : AlbedoRGBA(0xFFFFFFFF)
        , NormalMapIndex(0xFFFF)
        , Metallic(0)
        , SpecularPower(32)
        , EmissionRGBA(0)
        , AlbedoTexIndex(0xFFFF)
        , RoughnessTexIndex(0xFFFF)
        , Flags(0)
        , AlphaMode(0)
        , ShadingModel(0)
        , LightmapScaleX(256)
        , LightmapScaleY(256)
        , LightmapOffsetX(0)
        , LightmapOffsetY(0)
        , UVScaleX(256)
        , UVScaleY(256)
        , AlbedoTexIndex2(0xFFFF)
        , AlbedoTexIndex3(0xFFFF)
        , TextureBlendMode(0)
        , TextureBlendFactor(128) // Default 50% blend
        , Opacity(255)
        , Extras(nullptr)
    {}

    // Helper functions for unpacking
    Math::Vec3 GetAlbedoColor() const {
        return Math::Vec3(
            ((AlbedoRGBA >> 0) & 0xFF) / 255.0f,
            ((AlbedoRGBA >> 8) & 0xFF) / 255.0f,
            ((AlbedoRGBA >> 16) & 0xFF) / 255.0f
        );
    }

    float GetRoughness() const {
        return ((AlbedoRGBA >> 24) & 0xFF) / 255.0f;
    }

    Math::Vec3 GetEmissionColor() const {
        return Math::Vec3(
            ((EmissionRGBA >> 0) & 0xFF) / 255.0f,
            ((EmissionRGBA >> 8) & 0xFF) / 255.0f,
            ((EmissionRGBA >> 16) & 0xFF) / 255.0f
        );
    }

    float GetEmissionStrength() const {
        return ((EmissionRGBA >> 24) & 0xFF) / 255.0f;
    }

    float GetMetallicFactor() const {
        return Metallic / 255.0f;
    }

    float GetSpecularExponent() const {
        // Map 0-255 to reasonable specular range (1-256)
        return 1.0f + SpecularPower;
    }

    Math::Vec2 GetUVScale() const {
        return Math::Vec2(UVScaleX / 256.0f, UVScaleY / 256.0f);
    }

    Math::Vec2 GetLightmapScale() const {
        return Math::Vec2(LightmapScaleX / 256.0f, LightmapScaleY / 256.0f);
    }

    Math::Vec2 GetLightmapOffset() const {
        return Math::Vec2(LightmapOffsetX / 256.0f, LightmapOffsetY / 256.0f);
    }

    // Helper setters
    void SetAlbedoColor(const Math::Vec3& Color, float Roughness = 0.5f) {
        uint8_t r = static_cast<uint8_t>(Color.x * 255.0f);
        uint8_t g = static_cast<uint8_t>(Color.y * 255.0f);
        uint8_t b = static_cast<uint8_t>(Color.z * 255.0f);
        uint8_t a = static_cast<uint8_t>(Roughness * 255.0f);
        AlbedoRGBA = r | (g << 8) | (b << 16) | (a << 24);
    }

    void SetEmission(const Math::Vec3& Color, float Strength = 1.0f) {
        uint8_t r = static_cast<uint8_t>(Color.x * 255.0f);
        uint8_t g = static_cast<uint8_t>(Color.y * 255.0f);
        uint8_t b = static_cast<uint8_t>(Color.z * 255.0f);
        uint8_t a = static_cast<uint8_t>(Strength * 255.0f);
        EmissionRGBA = r | (g << 8) | (b << 16) | (a << 24);
    }

    void SetUVScale(float X, float Y) {
        UVScaleX = static_cast<uint16_t>(X * 256.0f);
        UVScaleY = static_cast<uint16_t>(Y * 256.0f);
    }

    float GetTextureBlendFactor() const {
        return TextureBlendFactor / 255.0f;
    }

    void SetTextureBlendFactor(float Factor) {
        TextureBlendFactor = static_cast<uint8_t>(Factor * 255.0f);
    }

    // Serialization methods
    static JSONValue Serialize(const Material& Material);
    static Material Deserialize(const JSONValue& JSON);

    JSONValue ToJSON() const;
    bool FromJSON(const JSONValue& JSON);
};

// Texture blend modes
enum class TextureBlendMode : uint8_t {
    None = 0,        // No blending
    Multiply = 1,    // Multiply blend
    Overlay = 2,     // Overlay blend
    Add = 3,         // Additive blend
    Mix = 4,         // Linear mix
};

// Material flags (bitfield)
enum MaterialFlags : uint16_t {
    MaterialFlag_None           = 0,
    MaterialFlag_DoubleSided    = 1 << 0,
    MaterialFlag_CastsShadows   = 1 << 1,
    MaterialFlag_ReceivesShadows = 1 << 2,
    MaterialFlag_HasNormalMap   = 1 << 3,
    MaterialFlag_HasEmission    = 1 << 4,
    MaterialFlag_Unlit          = 1 << 5,
    MaterialFlag_Transparent    = 1 << 6,
    MaterialFlag_AlphaTested    = 1 << 7,
    MaterialFlag_Reflective     = 1 << 8,
    MaterialFlag_UseLightmap    = 1 << 9,
};

// Alpha blending modes
enum class AlphaMode : uint8_t {
    Opaque = 0,
    Masked = 1,      // Alpha test
    Blend = 2,       // Alpha blending
    Additive = 3,    // Additive blending
};

// Shading models
enum class ShadingModel : uint8_t {
    Unlit = 0,
    Lambert = 1,         // Simple diffuse
    BlinnPhong = 2,      // Diffuse + specular
    PhysicallyBased = 3, // Simplified PBR (metallic/roughness)
};

// Structure of Arrays layout for batch processing materials
struct MaterialBatch {
    std::vector<uint32_t> AlbedoRGBA;
    std::vector<uint32_t> EmissionRGBA;
    std::vector<uint16_t> NormalMapIndices;
    std::vector<uint16_t> AlbedoTexIndices;
    std::vector<uint16_t> RoughnessTexIndices;
    std::vector<uint8_t> Metallic;
    std::vector<uint8_t> SpecularPower;
    std::vector<uint16_t> Flags;
    std::vector<MaterialExtras*> Extras;

    void Reserve(size_t Count) {
        AlbedoRGBA.reserve(Count);
        EmissionRGBA.reserve(Count);
        NormalMapIndices.reserve(Count);
        AlbedoTexIndices.reserve(Count);
        RoughnessTexIndices.reserve(Count);
        Metallic.reserve(Count);
        SpecularPower.reserve(Count);
        Flags.reserve(Count);
        Extras.reserve(Count);
    }

    void AddMaterial(const Material& Mat) {
        AlbedoRGBA.push_back(Mat.AlbedoRGBA);
        EmissionRGBA.push_back(Mat.EmissionRGBA);
        NormalMapIndices.push_back(Mat.NormalMapIndex);
        AlbedoTexIndices.push_back(Mat.AlbedoTexIndex);
        RoughnessTexIndices.push_back(Mat.RoughnessTexIndex);
        Metallic.push_back(Mat.Metallic);
        SpecularPower.push_back(Mat.SpecularPower);
        Flags.push_back(Mat.Flags);
        Extras.push_back(Mat.Extras);
    }

    size_t Count() const { return AlbedoRGBA.size(); }
};

// Material library/manager
class MaterialLibrary {
public:
    MaterialLibrary() = default;

    uint32_t AddMaterial(const Material& Mat) {
        uint32_t ID = static_cast<uint32_t>(m_Materials.size());
        m_Materials.push_back(Mat);
        return ID;
    }

    Material* GetMaterial(uint32_t ID) {
        if (ID >= m_Materials.size()) return nullptr;
        return &m_Materials[ID];
    }

    const Material* GetMaterial(uint32_t ID) const {
        if (ID >= m_Materials.size()) return nullptr;
        return &m_Materials[ID];
    }

    size_t GetMaterialCount() const { return m_Materials.size(); }

    // Get direct access for batch processing
    const std::vector<Material>& GetMaterials() const { return m_Materials; }
    std::vector<Material>& GetMaterials() { return m_Materials; }

    // Convert to SoA for SIMD processing
    MaterialBatch ToSoA() const {
        MaterialBatch Batch;
        Batch.Reserve(m_Materials.size());
        for (const Material& Mat : m_Materials) {
            Batch.AddMaterial(Mat);
        }
        return Batch;
    }

private:
    std::vector<Material> m_Materials;
};

// Predefined materials
namespace Materials {
    inline Material CreateDefault() {
        Material Mat;
        Mat.SetAlbedoColor(Math::Vec3(0.8f, 0.8f, 0.8f), 0.5f);
        Mat.Flags = MaterialFlag_CastsShadows | MaterialFlag_ReceivesShadows;
        Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::BlinnPhong);
        return Mat;
    }

    inline Material CreateUnlit(const Math::Vec3& Color) {
        Material Mat;
        Mat.SetAlbedoColor(Color, 0.0f);
        Mat.Flags = MaterialFlag_Unlit;
        Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::Unlit);
        return Mat;
    }

    inline Material CreateEmissive(const Math::Vec3& Color, float Strength = 1.0f) {
        Material Mat;
        Mat.SetAlbedoColor(Math::Vec3(0, 0, 0), 0.0f);
        Mat.SetEmission(Color, Strength);
        Mat.Flags = MaterialFlag_HasEmission | MaterialFlag_Unlit;
        Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::Unlit);
        return Mat;
    }

    inline Material CreateMetal(const Math::Vec3& Color, float Roughness = 0.2f) {
        Material Mat;
        Mat.SetAlbedoColor(Color, Roughness);
        Mat.Metallic = 255;
        Mat.SpecularPower = 128;
        Mat.Flags = MaterialFlag_CastsShadows | MaterialFlag_ReceivesShadows | MaterialFlag_Reflective;
        Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::PhysicallyBased);
        return Mat;
    }

    inline Material CreateGlass(const Math::Vec3& Color, float Roughness = 0.05f, float Transparency = 0.7f, float IOR = 1.5f) {
        Material Mat;
        // Glass should have black albedo - it's transparent and color comes from reflections
        // The Color parameter is kept for potential future tinting of reflections
        Mat.SetAlbedoColor(Math::Vec3(0.0f, 0.0f, 0.0f), Roughness);
        Mat.Opacity = static_cast<uint8_t>(Transparency * 255.0f);
        Mat.Metallic = 10; // Slight metallic for better reflections
        Mat.SpecularPower = 255;
        Mat.AlphaMode = static_cast<uint8_t>(AlphaMode::Blend);
        Mat.Flags = MaterialFlag_CastsShadows | MaterialFlag_ReceivesShadows | MaterialFlag_Transparent | MaterialFlag_Reflective;
        Mat.ShadingModel = static_cast<uint8_t>(ShadingModel::PhysicallyBased);
        // Store IOR in MaterialExtras Density field (will be extracted and passed via uniform)
        if (!Mat.Extras) {
            Mat.Extras = new MaterialExtras();
        }
        Mat.Extras->Density = IOR; // Store IOR in Density field temporarily
        return Mat;
    }

    // Helper to get IOR from material (default 1.0 for non-glass, stored in Extras->Density for glass)
    inline float GetIOR(const Material& Mat) {
        // Glass materials have low opacity and Extras with IOR stored in Density
        if (Mat.Opacity < 242 && Mat.Extras && Mat.Extras->Density > 1.0f) {
            return Mat.Extras->Density;
        }
        // Default IOR for non-glass materials (air)
        return 1.0f;
    }
}

} // namespace Solstice::Core
