#pragma once

#include "../../Solstice.hxx"
#include "../../Core/System/Async.hxx"
#include <Render/SoftwareRenderer.hxx>
#include "../../Material/Material.hxx"
#include "../../Arzachel/ProceduralTexture.hxx"
#include "../../Arzachel/MaterialSerializer.hxx"
#include "../../Asset/IO/ResourceHandle.hxx"
#include <atomic>
#include <future>
#include <vector>
#include <string>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <bgfx/bgfx.h>

namespace Solstice::Game {

// Texture generation state
enum class TextureGenState {
    NotStarted,
    InProgress,
    Completed,
    Error
};

// Texture generation configuration
struct TextureConfig {
    std::string Name;
    uint32_t Resolution;
    uint32_t Octaves;
    float Scale;
    uint32_t Seed;

    // Optional: Custom generator function (if nullptr, uses default presets)
    std::function<Arzachel::TextureData()> CustomGenerator;
};

// Material assignment configuration
struct MaterialAssignment {
    uint32_t MaterialID;
    uint16_t AlbedoTexIndex;
    uint16_t AlbedoTexIndex2{0xFFFF}; // Optional detail texture
    uint16_t AlbedoTexIndex3{0xFFFF}; // Optional blend mask
    Math::Vec3 AlbedoColor{1.0f, 1.0f, 1.0f};
    float Roughness{0.5f};
    uint8_t TextureBlendMode{0};
    float TextureBlendFactor{0.5f};
    std::string MaterialPath; // Optional: path to save/load material
};

// Procedural texture manager for async texture generation
class SOLSTICE_API ProceduralTextureManager {
public:
    ProceduralTextureManager();
    ~ProceduralTextureManager();

    // Initialize texture generation with configuration
    // TextureConfigs: List of textures to generate
    // CacheDirectory: Directory for texture cache (default: "cache")
    void Initialize(const std::vector<TextureConfig>& TextureConfigs, const std::string& CacheDirectory = "cache");

    // Process pending textures (call from Render() on main thread)
    // Returns true if a texture was processed this frame
    bool ProcessPendingTextures();

    // Finalize textures and assign to materials
    // Renderer: Renderer to register textures with
    // MaterialLibrary: Material library to update
    // MaterialAssignments: Map of texture index -> material assignment
    void Finalize(Render::SoftwareRenderer* Renderer, Core::MaterialLibrary* MaterialLibrary,
                  const std::vector<MaterialAssignment>& MaterialAssignments);

    // State queries
    TextureGenState GetState() const { return m_TextureGenState.load(std::memory_order_acquire); }
    float GetProgress() const { return m_TextureGenProgress.load(std::memory_order_acquire); }
    bool IsReady() const { return m_TexturesReady.load(std::memory_order_acquire); }
    std::string GetCurrentTextureName() const;

    // Get texture handle by index (BGFX handle)
    bgfx::TextureHandle GetTexture(size_t Index) const;

    // Get texture handle by index (wrapped TextureHandle)
    // Returns Invalid handle if index is out of range or texture not registered
    // Note: Textures must be registered via Finalize() before this returns valid handles
    Core::TextureHandle GetTextureHandle(size_t Index) const;

    // Shutdown and cleanup
    void Shutdown();

private:
    struct TexturePixelData {
        std::vector<uint8_t> Pixels;
        uint32_t Resolution;
        std::string Name;
        size_t Index;
    };

    void GenerateTextureAsync(size_t Index, const TextureConfig& Config, size_t TotalTextures);
    void StartCompletionChecker(size_t TotalTextures);

    // State
    std::atomic<TextureGenState> m_TextureGenState{TextureGenState::NotStarted};
    std::atomic<float> m_TextureGenProgress{0.0f};
    std::atomic<int> m_TexturesCompleted{0};
    std::atomic<int> m_TexturesCreated{0};
    std::atomic<bool> m_TexturesReady{false};

    // Texture data
    std::vector<bgfx::TextureHandle> m_Textures;
    std::vector<TexturePixelData> m_PendingTextureData;
    std::vector<std::future<void>> m_TextureGenFutures;

    // Registered texture indices (config index -> registered index)
    // Populated during Finalize()
    std::unordered_map<size_t, uint16_t> m_RegisteredIndices;

    // Synchronization (mutable for const methods)
    mutable Core::Spinlock m_TextureNameLock;
    mutable Core::Spinlock m_TextureDataLock;
    mutable Core::Spinlock m_TextureArrayLock;
    mutable Core::Spinlock m_TextureGenFuturesLock;

    // Configuration
    std::string m_CacheDirectory;
    std::string m_CurrentTextureName;
    std::vector<TextureConfig> m_TextureConfigs;
};

} // namespace Solstice::Game
