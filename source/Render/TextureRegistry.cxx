#include "TextureRegistry.hxx"
#include <Core/Debug.hxx>

namespace Solstice::Render {

uint16_t TextureRegistry::Register(bgfx::TextureHandle handle) {
    if (!bgfx::isValid(handle)) {
        SIMPLE_LOG("TextureRegistry: Attempted to register invalid texture handle");
        return 0xFFFF; // Invalid index
    }

    // Find next available index
    while (m_Textures.find(m_NextIndex) != m_Textures.end()) {
        m_NextIndex++;
        if (m_NextIndex == 0xFFFF) {
            m_NextIndex = 0; // Wrap around (shouldn't happen in practice)
            SIMPLE_LOG("TextureRegistry: WARNING - Index wrapped around, registry may be full");
            break;
        }
    }

    uint16_t index = m_NextIndex;
    m_Textures[index] = handle;
    m_NextIndex++;

    return index;
}

bool TextureRegistry::Register(uint16_t index, bgfx::TextureHandle handle) {
    if (!bgfx::isValid(handle)) {
        SIMPLE_LOG("TextureRegistry: Attempted to register invalid texture handle at index " + std::to_string(index));
        return false;
    }

    if (m_Textures.find(index) != m_Textures.end()) {
        SIMPLE_LOG("TextureRegistry: Index " + std::to_string(index) + " already in use");
        return false;
    }

    m_Textures[index] = handle;
    if (index >= m_NextIndex) {
        m_NextIndex = index + 1;
    }

    return true;
}

bool TextureRegistry::Has(uint16_t index) const {
    return m_Textures.find(index) != m_Textures.end();
}

bgfx::TextureHandle TextureRegistry::Get(uint16_t index) const {
    auto it = m_Textures.find(index);
    if (it != m_Textures.end()) {
        return it->second;
    }
    return BGFX_INVALID_HANDLE;
}

void TextureRegistry::Remove(uint16_t index) {
    m_Textures.erase(index);
}

void TextureRegistry::Clear() {
    m_Textures.clear();
    m_NextIndex = 0;
}

} // namespace Solstice::Render
