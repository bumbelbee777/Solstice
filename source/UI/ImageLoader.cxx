#include <UI/ImageLoader.hxx>
#include <Core/Debug.hxx>
#include <fstream>
#include <vector>
#include <algorithm>

// Include stb_image header (implementation is already defined in AssetLoader.cxx)
#include "../../3rdparty/tinygltf/stb_image.h"

// Include bimg headers
#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/file.h>

namespace Solstice::UI {

ImageLoader& ImageLoader::GetInstance() {
    static ImageLoader instance;
    return instance;
}

bgfx::TextureHandle ImageLoader::LoadImageFromFile(const std::string& FilePath) {
    // Check cache first
    auto it = m_TextureCache.find(FilePath);
    if (it != m_TextureCache.end()) {
        if (bgfx::isValid(it->second)) {
            return it->second;
        } else {
            // Texture was destroyed, remove from cache
            m_TextureCache.erase(it);
        }
    }

    // Try loading with stb_image first (simpler, faster for common formats)
    bgfx::TextureHandle handle = LoadImageSTB(FilePath);

    // If stb_image failed, try bimg (supports more formats)
    if (!bgfx::isValid(handle)) {
        handle = LoadImageBIMG(FilePath);
    }

    // Cache the result
    if (bgfx::isValid(handle)) {
        m_TextureCache[FilePath] = handle;
        m_HandleToPath[handle] = FilePath;
    }

    return handle;
}

bgfx::TextureHandle ImageLoader::LoadImageFromMemory(const uint8_t* Data, size_t Size) {
    if (!Data || Size == 0) {
        return BGFX_INVALID_HANDLE;
    }

    // Try stb_image first
    bgfx::TextureHandle handle = LoadImageSTBFromMemory(Data, Size);

    // If stb_image failed, try bimg
    if (!bgfx::isValid(handle)) {
        handle = LoadImageBIMGFromMemory(Data, Size);
    }

    return handle;
}

ImageInfo ImageLoader::GetImageInfo(const std::string& FilePath) {
    ImageInfo info;

    // Use stb_image to get info without loading full image
    int width, height, channels;
    if (stbi_info(FilePath.c_str(), &width, &height, &channels)) {
        info.Width = static_cast<uint32_t>(width);
        info.Height = static_cast<uint32_t>(height);
        info.Channels = static_cast<uint32_t>(channels);
        info.HasAlpha = (channels == 4 || channels == 2);
    }

    return info;
}

bgfx::TextureHandle ImageLoader::CreateTextureFromImage(const uint8_t* Data, uint32_t Width, uint32_t Height, uint32_t Channels) {
    if (!Data || Width == 0 || Height == 0) {
        return BGFX_INVALID_HANDLE;
    }

    // Convert to RGBA8 if needed
    std::vector<uint8_t> rgbaData;
    const uint8_t* finalData = Data;

    if (Channels != 4) {
        rgbaData.resize(Width * Height * 4);
        ConvertToRGBA8(Data, rgbaData.data(), Width, Height, Channels);
        finalData = rgbaData.data();
    }

    // Create bgfx texture
    bgfx::TextureFormat::Enum format = GetBGFXFormat(4); // Always RGBA8 for UI
    const bgfx::Memory* mem = bgfx::copy(finalData, Width * Height * 4);

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(Width),
        static_cast<uint16_t>(Height),
        false, 1,
        format,
        BGFX_TEXTURE_NONE,
        mem
    );

    if (bgfx::isValid(handle)) {
        bgfx::setName(handle, "UI Image");
    }

    return handle;
}

void ImageLoader::DestroyTexture(bgfx::TextureHandle Handle) {
    if (!bgfx::isValid(Handle)) {
        return;
    }

    // Remove from cache
    auto it = m_HandleToPath.find(Handle);
    if (it != m_HandleToPath.end()) {
        m_TextureCache.erase(it->second);
        m_HandleToPath.erase(it);
    }

    // Destroy texture
    bgfx::destroy(Handle);
}

void ImageLoader::ClearCache() {
    // Destroy all cached textures
    for (auto& pair : m_TextureCache) {
        if (bgfx::isValid(pair.second)) {
            bgfx::destroy(pair.second);
        }
    }

    m_TextureCache.clear();
    m_HandleToPath.clear();
}

bool ImageLoader::IsCached(const std::string& FilePath) const {
    auto it = m_TextureCache.find(FilePath);
    return it != m_TextureCache.end() && bgfx::isValid(it->second);
}

bgfx::TextureHandle ImageLoader::LoadImageSTB(const std::string& FilePath) {
    int width, height, channels;
    uint8_t* data = stbi_load(FilePath.c_str(), &width, &height, &channels, 4); // Force RGBA

    if (!data) {
        return BGFX_INVALID_HANDLE;
    }

    bgfx::TextureHandle handle = CreateTextureFromImage(data, static_cast<uint32_t>(width),
                                                         static_cast<uint32_t>(height), 4);

    stbi_image_free(data);
    return handle;
}

bgfx::TextureHandle ImageLoader::LoadImageBIMG(const std::string& FilePath) {
    // Initialize allocator if needed
    if (!m_Allocator) {
        static bx::DefaultAllocator defaultAllocator;
        m_Allocator = &defaultAllocator;
    }

    // Read file
    bx::FileReader reader;
    bx::Error err;
    if (!reader.open(bx::FilePath(FilePath.c_str()), &err)) {
        return BGFX_INVALID_HANDLE;
    }

    uint32_t size = static_cast<uint32_t>(reader.seek(0, bx::Whence::End));
    reader.seek(0, bx::Whence::Begin);

    std::vector<uint8_t> fileData(size);
    reader.read(fileData.data(), static_cast<int32_t>(size), &err);
    reader.close();

    // Parse image
    bx::Error parseErr;
    bimg::ImageContainer* container = bimg::imageParse(m_Allocator, fileData.data(), size,
                                                        bimg::TextureFormat::RGBA8, &parseErr);

    if (!container || !parseErr.isOk()) {
        return BGFX_INVALID_HANDLE;
    }

    // Create texture
    const bgfx::Memory* mem = bgfx::makeRef(container->m_data, container->m_size,
                                            [](void*, void* userData) {
                                                bimg::imageFree(static_cast<bimg::ImageContainer*>(userData));
                                            }, container);

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(container->m_width),
        static_cast<uint16_t>(container->m_height),
        container->m_numMips > 1,
        static_cast<uint16_t>(container->m_numLayers),
        bgfx::TextureFormat::Enum(container->m_format),
        BGFX_TEXTURE_NONE,
        mem
    );

    if (bgfx::isValid(handle)) {
        bgfx::setName(handle, "UI Image (bimg)");
    }

    return handle;
}

bgfx::TextureHandle ImageLoader::LoadImageSTBFromMemory(const uint8_t* Data, size_t Size) {
    int width, height, channels;
    uint8_t* imageData = stbi_load_from_memory(Data, static_cast<int>(Size), &width, &height, &channels, 4);

    if (!imageData) {
        return BGFX_INVALID_HANDLE;
    }

    bgfx::TextureHandle handle = CreateTextureFromImage(imageData, static_cast<uint32_t>(width),
                                                         static_cast<uint32_t>(height), 4);

    stbi_image_free(imageData);
    return handle;
}

bgfx::TextureHandle ImageLoader::LoadImageBIMGFromMemory(const uint8_t* Data, size_t Size) {
    // Initialize allocator if needed
    if (!m_Allocator) {
        static bx::DefaultAllocator defaultAllocator;
        m_Allocator = &defaultAllocator;
    }

    // Parse image
    bx::Error parseErr2;
    bimg::ImageContainer* container = bimg::imageParse(m_Allocator, Data, static_cast<uint32_t>(Size),
                                                       bimg::TextureFormat::RGBA8, &parseErr2);

    if (!container || !parseErr2.isOk()) {
        return BGFX_INVALID_HANDLE;
    }

    // Create texture
    const bgfx::Memory* mem = bgfx::makeRef(container->m_data, container->m_size,
                                            [](void*, void* userData) {
                                                bimg::imageFree(static_cast<bimg::ImageContainer*>(userData));
                                            }, container);

    bgfx::TextureHandle handle = bgfx::createTexture2D(
        static_cast<uint16_t>(container->m_width),
        static_cast<uint16_t>(container->m_height),
        container->m_numMips > 1,
        static_cast<uint16_t>(container->m_numLayers),
        bgfx::TextureFormat::Enum(container->m_format),
        BGFX_TEXTURE_NONE,
        mem
    );

    if (bgfx::isValid(handle)) {
        bgfx::setName(handle, "UI Image (bimg)");
    }

    return handle;
}

bgfx::TextureFormat::Enum ImageLoader::GetBGFXFormat(uint32_t Channels) const {
    switch (Channels) {
        case 1: return bgfx::TextureFormat::R8;
        case 2: return bgfx::TextureFormat::RG8;
        case 3: return bgfx::TextureFormat::RGB8;
        case 4: return bgfx::TextureFormat::RGBA8;
        default: return bgfx::TextureFormat::RGBA8;
    }
}

void ImageLoader::ConvertToRGBA8(const uint8_t* Source, uint8_t* Dest, uint32_t Width, uint32_t Height, uint32_t SourceChannels) {
    const uint32_t pixelCount = Width * Height;

    for (uint32_t i = 0; i < pixelCount; ++i) {
        const uint32_t srcIdx = i * SourceChannels;
        const uint32_t dstIdx = i * 4;

        switch (SourceChannels) {
            case 1: {
                // Grayscale -> RGBA
                Dest[dstIdx + 0] = Source[srcIdx];
                Dest[dstIdx + 1] = Source[srcIdx];
                Dest[dstIdx + 2] = Source[srcIdx];
                Dest[dstIdx + 3] = 255;
                break;
            }
            case 2: {
                // Grayscale + Alpha -> RGBA
                Dest[dstIdx + 0] = Source[srcIdx];
                Dest[dstIdx + 1] = Source[srcIdx];
                Dest[dstIdx + 2] = Source[srcIdx];
                Dest[dstIdx + 3] = Source[srcIdx + 1];
                break;
            }
            case 3: {
                // RGB -> RGBA
                Dest[dstIdx + 0] = Source[srcIdx + 0];
                Dest[dstIdx + 1] = Source[srcIdx + 1];
                Dest[dstIdx + 2] = Source[srcIdx + 2];
                Dest[dstIdx + 3] = 255;
                break;
            }
            case 4: {
                // RGBA -> RGBA (copy)
                Dest[dstIdx + 0] = Source[srcIdx + 0];
                Dest[dstIdx + 1] = Source[srcIdx + 1];
                Dest[dstIdx + 2] = Source[srcIdx + 2];
                Dest[dstIdx + 3] = Source[srcIdx + 3];
                break;
            }
        }
    }
}

} // namespace Solstice::UI
