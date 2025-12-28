#pragma once

#include "../Solstice.hxx"
#include <cstdint>

namespace Solstice::Core {

// Material handle - wraps material ID with validation
// Materials are owned by MaterialLibrary, handles are references
class SOLSTICE_API MaterialHandle {
public:
    MaterialHandle() : m_Value(InvalidValue) {}
    explicit MaterialHandle(uint32_t Value) : m_Value(Value) {}

    // Check if handle is valid
    bool IsValid() const { return m_Value != InvalidValue; }

    // Get raw value (use with caution)
    uint32_t GetValue() const { return m_Value; }

    // Create invalid handle
    static MaterialHandle Invalid() { return MaterialHandle(InvalidValue); }

    // Comparison operators
    bool operator==(const MaterialHandle& Other) const { return m_Value == Other.m_Value; }
    bool operator!=(const MaterialHandle& Other) const { return m_Value != Other.m_Value; }
    bool operator<(const MaterialHandle& Other) const { return m_Value < Other.m_Value; }

private:
    static constexpr uint32_t InvalidValue = 0xFFFFFFFF; // UINT32_MAX
    uint32_t m_Value;
};

// Texture handle - wraps texture index with validation
// Textures are owned by TextureRegistry, handles are references
class SOLSTICE_API TextureHandle {
public:
    TextureHandle() : m_Value(InvalidValue) {}
    explicit TextureHandle(uint16_t Value) : m_Value(Value) {}

    // Check if handle is valid
    bool IsValid() const { return m_Value != InvalidValue; }

    // Get raw value (use with caution)
    uint16_t GetValue() const { return m_Value; }

    // Create invalid handle
    static TextureHandle Invalid() { return TextureHandle(InvalidValue); }

    // Comparison operators
    bool operator==(const TextureHandle& Other) const { return m_Value == Other.m_Value; }
    bool operator!=(const TextureHandle& Other) const { return m_Value != Other.m_Value; }
    bool operator<(const TextureHandle& Other) const { return m_Value < Other.m_Value; }

private:
    static constexpr uint16_t InvalidValue = 0xFFFF;
    uint16_t m_Value;
};

// Mesh handle - wraps mesh ID with validation
// Meshes are owned by MeshLibrary, handles are references
class SOLSTICE_API MeshHandle {
public:
    MeshHandle() : m_Value(InvalidValue) {}
    explicit MeshHandle(uint32_t Value) : m_Value(Value) {}

    // Check if handle is valid
    bool IsValid() const { return m_Value != InvalidValue; }

    // Get raw value (use with caution)
    uint32_t GetValue() const { return m_Value; }

    // Create invalid handle
    static MeshHandle Invalid() { return MeshHandle(InvalidValue); }

    // Comparison operators
    bool operator==(const MeshHandle& Other) const { return m_Value == Other.m_Value; }
    bool operator!=(const MeshHandle& Other) const { return m_Value != Other.m_Value; }
    bool operator<(const MeshHandle& Other) const { return m_Value < Other.m_Value; }

private:
    static constexpr uint32_t InvalidValue = 0xFFFFFFFF; // UINT32_MAX
    uint32_t m_Value;
};

} // namespace Solstice::Core

