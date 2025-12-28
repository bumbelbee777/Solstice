#pragma once

#include "../Solstice.hxx"
#include <UI/ImageLoader.hxx>
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <string>
#include <vector>
#include <unordered_map>

namespace Solstice::UI {

struct SpriteFrame {
    ImVec2 UV0{0.0f, 0.0f};
    ImVec2 UV1{1.0f, 1.0f};
    float Duration{0.0f};  // Frame duration in seconds
};

class SOLSTICE_API Sprite {
public:
    Sprite(bgfx::TextureHandle Texture, const ImVec2& Size);
    ~Sprite() = default;

    // Frame management
    void SetFrame(const SpriteFrame& Frame);
    SpriteFrame GetFrame() const { return m_Frame; }

    // Position and size
    void SetPosition(const ImVec2& Position) { m_Position = Position; }
    ImVec2 GetPosition() const { return m_Position; }

    void SetSize(const ImVec2& Size) { m_Size = Size; }
    ImVec2 GetSize() const { return m_Size; }

    // Color tint
    void SetColor(const ImVec4& Color) { m_Color = Color; }
    ImVec4 GetColor() const { return m_Color; }

    // Texture
    bgfx::TextureHandle GetTexture() const { return m_Texture; }
    void SetTexture(bgfx::TextureHandle Texture) { m_Texture = Texture; }

    // Rendering
    void Render(ImDrawList* DrawList);  // 2D screen-space
    void RenderWorldSpace(const Math::Vec3& WorldPos,
                         const Math::Matrix4& ViewMatrix,
                         const Math::Matrix4& ProjectionMatrix,
                         int ScreenWidth, int ScreenHeight,
                         ImDrawList* DrawList = nullptr);  // 3D world-space

private:
    bgfx::TextureHandle m_Texture{BGFX_INVALID_HANDLE};
    ImVec2 m_Position{0.0f, 0.0f};
    ImVec2 m_Size{0.0f, 0.0f};
    SpriteFrame m_Frame;
    ImVec4 m_Color{1.0f, 1.0f, 1.0f, 1.0f};
};

class SOLSTICE_API SpriteSheet {
public:
    SpriteSheet();
    ~SpriteSheet() = default;

    // Load sprite sheet from file (grid-based)
    bool LoadFromFile(const std::string& FilePath, uint32_t FrameWidth, uint32_t FrameHeight);

    // Load sprite sheet from JSON (Aseprite, TexturePacker, etc.)
    bool LoadFromJSON(const std::string& JSONPath, const std::string& ImagePath);

    // Get sprite by frame index
    Sprite GetSprite(uint32_t FrameIndex) const;

    // Get sprite by name (if loaded from JSON)
    Sprite GetSpriteByName(const std::string& Name) const;

    // Frame information
    uint32_t GetFrameCount() const { return static_cast<uint32_t>(m_Frames.size()); }
    uint32_t GetFrameWidth() const { return m_FrameWidth; }
    uint32_t GetFrameHeight() const { return m_FrameHeight; }

    // Texture
    bgfx::TextureHandle GetTexture() const { return m_Texture; }

private:
    void CalculateGridFrames(uint32_t SheetWidth, uint32_t SheetHeight,
                            uint32_t FrameWidth, uint32_t FrameHeight);

    bgfx::TextureHandle m_Texture{BGFX_INVALID_HANDLE};
    std::vector<SpriteFrame> m_Frames;
    std::unordered_map<std::string, uint32_t> m_NameToFrameIndex;
    uint32_t m_FrameWidth{0};
    uint32_t m_FrameHeight{0};
    uint32_t m_SheetWidth{0};
    uint32_t m_SheetHeight{0};
};

class SOLSTICE_API TextureAtlas {
public:
    TextureAtlas();
    ~TextureAtlas() = default;

    // Load atlas texture
    bool LoadTexture(const std::string& FilePath);

    // Add sprite to atlas
    void AddSprite(const std::string& Name, const ImVec2& UV0, const ImVec2& UV1);

    // Get sprite by name
    Sprite GetSprite(const std::string& Name) const;

    // Check if sprite exists
    bool HasSprite(const std::string& Name) const;

    // Texture
    bgfx::TextureHandle GetTexture() const { return m_Texture; }

private:
    bgfx::TextureHandle m_Texture{BGFX_INVALID_HANDLE};
    std::unordered_map<std::string, SpriteFrame> m_Sprites;
    uint32_t m_AtlasWidth{0};
    uint32_t m_AtlasHeight{0};
};

} // namespace Solstice::UI
