#pragma once

#include <Solstice.hxx>
#include <bgfx/bgfx.h>
#include <unordered_map>
#include <cstdint>

namespace Solstice::Render {

// Texture registry for managing texture handles by index
// Follows similar pattern to Entity/Registry.hxx but simplified for texture management
class SOLSTICE_API TextureRegistry {
public:
    // Register a texture and return its assigned index
    // If handle is invalid, returns 0xFFFF (invalid index)
    uint16_t Register(bgfx::TextureHandle handle);

    // Register a texture with a specific index
    // Returns true if successful, false if index already in use
    bool Register(uint16_t index, bgfx::TextureHandle handle);

    // Check if a texture exists at the given index
    bool Has(uint16_t index) const;

    // Get texture handle by index
    // Returns BGFX_INVALID_HANDLE if not found
    bgfx::TextureHandle Get(uint16_t index) const;

    // Remove texture from registry
    void Remove(uint16_t index);

    // Clear all textures
    void Clear();

    // Get next available index
    uint16_t GetNextIndex() const { return m_NextIndex; }

private:
    std::unordered_map<uint16_t, bgfx::TextureHandle> m_Textures;
    uint16_t m_NextIndex{0};
};

} // namespace Solstice::Render
