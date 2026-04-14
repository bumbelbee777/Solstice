#pragma once

#include "../../Solstice.hxx"
#include <bgfx/bgfx.h>
#include <string>
#include <unordered_map>
#include <memory>

// Hash specialization for bgfx::TextureHandle (must be before unordered_map usage)
namespace std {
    template<>
    struct hash<bgfx::TextureHandle> {
        size_t operator()(const bgfx::TextureHandle& handle) const noexcept {
            return std::hash<uint16_t>{}(handle.idx);
        }
    };
    
    template<>
    struct equal_to<bgfx::TextureHandle> {
        bool operator()(const bgfx::TextureHandle& lhs, const bgfx::TextureHandle& rhs) const noexcept {
            return lhs.idx == rhs.idx;
        }
    };
}

// Forward declarations
namespace bx {
    struct AllocatorI;
}

namespace Solstice::UI {

struct ImageInfo {
    uint32_t Width{0};
    uint32_t Height{0};
    uint32_t Channels{0};
    bool HasAlpha{false};
};

class SOLSTICE_API ImageLoader {
public:
    static ImageLoader& GetInstance();

    // Load image from file path
    bgfx::TextureHandle LoadImageFromFile(const std::string& FilePath);

    // Load image from memory buffer
    bgfx::TextureHandle LoadImageFromMemory(const uint8_t* Data, size_t Size);

    // Get image info without loading full image
    ImageInfo GetImageInfo(const std::string& FilePath);

    // Create bgfx texture from loaded image data
    bgfx::TextureHandle CreateTextureFromImage(const uint8_t* Data, uint32_t Width, uint32_t Height, uint32_t Channels);

    // Destroy texture and remove from cache
    void DestroyTexture(bgfx::TextureHandle Handle);

    // Clear all cached textures
    void ClearCache();

    // Check if texture is cached
    bool IsCached(const std::string& FilePath) const;

private:
    ImageLoader() = default;
    ~ImageLoader() = default;

    // Prevent copying
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    // Internal helper functions
    bgfx::TextureHandle LoadImageSTB(const std::string& FilePath);
    bgfx::TextureHandle LoadImageBIMG(const std::string& FilePath);
    bgfx::TextureHandle LoadImageSTBFromMemory(const uint8_t* Data, size_t Size);
    bgfx::TextureHandle LoadImageBIMGFromMemory(const uint8_t* Data, size_t Size);

    bgfx::TextureFormat::Enum GetBGFXFormat(uint32_t Channels) const;
    void ConvertToRGBA8(const uint8_t* Source, uint8_t* Dest, uint32_t Width, uint32_t Height, uint32_t SourceChannels);

    // Texture cache
    std::unordered_map<std::string, bgfx::TextureHandle> m_TextureCache;
    std::unordered_map<bgfx::TextureHandle, std::string> m_HandleToPath;

    // Allocator for bimg
    bx::AllocatorI* m_Allocator{nullptr};
};

} // namespace Solstice::UI
