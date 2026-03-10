#include <UI/Sprite.hxx>
#include <UI/ViewportUI.hxx>
#include <Render/Post/PostProcessing.hxx>
#include <Core/Debug.hxx>
#include <fstream>
#include <cmath>

// Simple JSON parsing (we'll use a basic approach for now)
// For production, consider using a proper JSON library
#include <sstream>
#include <algorithm>

namespace Solstice::UI {

// Sprite implementation
Sprite::Sprite(bgfx::TextureHandle Texture, const ImVec2& Size)
    : m_Texture(Texture), m_Size(Size) {
    m_Frame.UV0 = ImVec2(0.0f, 0.0f);
    m_Frame.UV1 = ImVec2(1.0f, 1.0f);
}

void Sprite::SetFrame(const SpriteFrame& Frame) {
    m_Frame = Frame;
}

void Sprite::Render(ImDrawList* DrawList) {
    if (!DrawList || !bgfx::isValid(m_Texture)) {
        return;
    }

    ImVec2 min = m_Position;
    ImVec2 max = ImVec2(m_Position.x + m_Size.x, m_Position.y + m_Size.y);

    ImU32 color = ImGui::ColorConvertFloat4ToU32(m_Color);

    DrawList->AddImage(static_cast<ImTextureID>(static_cast<uintptr_t>(m_Texture.idx)),
                       min, max, m_Frame.UV0, m_Frame.UV1, color);
}

void Sprite::RenderWorldSpace(const Math::Vec3& WorldPos,
                             const Math::Matrix4& ViewMatrix,
                             const Math::Matrix4& ProjectionMatrix,
                             int ScreenWidth, int ScreenHeight,
                             ImDrawList* DrawList,
                             bgfx::ProgramHandle SceneProgram,
                             bgfx::ViewId ViewId) {
    if (!bgfx::isValid(m_Texture)) {
        return;
    }

    // Try to render as 3D billboard if we have a valid program or billboard shader is available
    bool canRenderWorldSpace = bgfx::isValid(SceneProgram) || ViewportUI::IsBillboardShaderAvailable();
    
    if (canRenderWorldSpace) {
        // Convert pixel size to world size (approximate - you may want to adjust this)
        float worldWidth = m_Size.x * 0.01f;  // Adjust scale as needed
        float worldHeight = m_Size.y * 0.01f;

        // Use billboard shader if SceneProgram not provided or invalid
        bgfx::ProgramHandle programToUse = BGFX_INVALID_HANDLE;
        if (bgfx::isValid(SceneProgram)) {
            programToUse = SceneProgram;
        }
        
        // Convert ImVec4 color to Math::Vec4 for color tint
        Math::Vec4 colorTint(m_Color.x, m_Color.y, m_Color.z, m_Color.w);
        
        // Render using billboard helper (will use billboard shader if programToUse is invalid)
        ViewportUI::RenderBillboardQuad(WorldPos, worldWidth, worldHeight, m_Texture,
                                       ViewMatrix, ProjectionMatrix, programToUse, ViewId, true, colorTint);
        return; // World-space rendering attempted, return (don't fall back to screen space)
    }

    // Fallback to ImGui screen-space rendering
    // Project 3D position to screen space
    Math::Vec2 screenPos = ViewportUI::ProjectToScreen(WorldPos, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

    // Check if position is on screen
    if (screenPos.x < -m_Size.x || screenPos.x > ScreenWidth + m_Size.x ||
        screenPos.y < -m_Size.y || screenPos.y > ScreenHeight + m_Size.y) {
        return; // Off screen
    }

    // Use foreground draw list if not provided
    if (!DrawList) {
        DrawList = ImGui::GetForegroundDrawList();
    }

    if (!DrawList) {
        return;
    }

    ImVec2 min = ImVec2(screenPos.x - m_Size.x * 0.5f, screenPos.y - m_Size.y * 0.5f);
    ImVec2 max = ImVec2(screenPos.x + m_Size.x * 0.5f, screenPos.y + m_Size.y * 0.5f);

    ImU32 color = ImGui::ColorConvertFloat4ToU32(m_Color);

    DrawList->AddImage(static_cast<ImTextureID>(static_cast<uintptr_t>(m_Texture.idx)),
                       min, max, m_Frame.UV0, m_Frame.UV1, color);
}

// SpriteSheet implementation
SpriteSheet::SpriteSheet() {
}

bool SpriteSheet::LoadFromFile(const std::string& FilePath, uint32_t FrameWidth, uint32_t FrameHeight) {
    m_FrameWidth = FrameWidth;
    m_FrameHeight = FrameHeight;

    // Load texture
    m_Texture = ImageLoader::GetInstance().LoadImageFromFile(FilePath);
    if (!bgfx::isValid(m_Texture)) {
        SIMPLE_LOG("SpriteSheet: Failed to load texture: " + FilePath);
        return false;
    }

    // Get image info to determine sheet dimensions
    ImageInfo info = ImageLoader::GetInstance().GetImageInfo(FilePath);
    m_SheetWidth = info.Width;
    m_SheetHeight = info.Height;

    // Calculate grid frames
    CalculateGridFrames(m_SheetWidth, m_SheetHeight, FrameWidth, FrameHeight);

    return true;
}

bool SpriteSheet::LoadFromJSON(const std::string& JSONPath, const std::string& ImagePath) {
    // Load texture first
    m_Texture = ImageLoader::GetInstance().LoadImageFromFile(ImagePath);
    if (!bgfx::isValid(m_Texture)) {
        SIMPLE_LOG("SpriteSheet: Failed to load texture: " + ImagePath);
        return false;
    }

    // Get image info
    ImageInfo info = ImageLoader::GetInstance().GetImageInfo(ImagePath);
    m_SheetWidth = info.Width;
    m_SheetHeight = info.Height;

    // Read JSON file
    std::ifstream file(JSONPath);
    if (!file.is_open()) {
        SIMPLE_LOG("SpriteSheet: Failed to open JSON file: " + JSONPath);
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Basic JSON parsing for Aseprite/TexturePacker format
    // This is a simplified parser - for production, use a proper JSON library
    // Format: {"frames": {"name": {"x": 0, "y": 0, "w": 32, "h": 32}, ...}}

    // For now, we'll use a simple approach: assume grid-based if JSON parsing fails
    // In a full implementation, you'd parse the JSON properly

    SIMPLE_LOG("SpriteSheet: JSON parsing not fully implemented, using grid-based fallback");

    // Fallback to grid-based if we can't parse JSON
    // Assume frames are 32x32 for now (this should be parsed from JSON)
    m_FrameWidth = 32;
    m_FrameHeight = 32;
    CalculateGridFrames(m_SheetWidth, m_SheetHeight, m_FrameWidth, m_FrameHeight);

    return true;
}

Sprite SpriteSheet::GetSprite(uint32_t FrameIndex) const {
    if (FrameIndex >= m_Frames.size()) {
        return Sprite(BGFX_INVALID_HANDLE, ImVec2(0.0f, 0.0f));
    }

    Sprite sprite(m_Texture, ImVec2(static_cast<float>(m_FrameWidth), static_cast<float>(m_FrameHeight)));
    sprite.SetFrame(m_Frames[FrameIndex]);
    return sprite;
}

Sprite SpriteSheet::GetSpriteByName(const std::string& Name) const {
    auto it = m_NameToFrameIndex.find(Name);
    if (it == m_NameToFrameIndex.end()) {
        return Sprite(BGFX_INVALID_HANDLE, ImVec2(0.0f, 0.0f));
    }

    return GetSprite(it->second);
}

void SpriteSheet::CalculateGridFrames(uint32_t SheetWidth, uint32_t SheetHeight,
                                      uint32_t FrameWidth, uint32_t FrameHeight) {
    m_Frames.clear();
    m_NameToFrameIndex.clear();

    if (FrameWidth == 0 || FrameHeight == 0) {
        return;
    }

    uint32_t cols = SheetWidth / FrameWidth;
    uint32_t rows = SheetHeight / FrameHeight;

    float uStep = static_cast<float>(FrameWidth) / static_cast<float>(SheetWidth);
    float vStep = static_cast<float>(FrameHeight) / static_cast<float>(SheetHeight);

    for (uint32_t row = 0; row < rows; ++row) {
        for (uint32_t col = 0; col < cols; ++col) {
            SpriteFrame frame;
            frame.UV0 = ImVec2(col * uStep, row * vStep);
            frame.UV1 = ImVec2((col + 1) * uStep, (row + 1) * vStep);
            frame.Duration = 0.1f; // Default frame duration

            m_Frames.push_back(frame);
        }
    }
}

// TextureAtlas implementation
TextureAtlas::TextureAtlas() {
}

bool TextureAtlas::LoadTexture(const std::string& FilePath) {
    m_Texture = ImageLoader::GetInstance().LoadImageFromFile(FilePath);
    if (!bgfx::isValid(m_Texture)) {
        SIMPLE_LOG("TextureAtlas: Failed to load texture: " + FilePath);
        return false;
    }

    ImageInfo info = ImageLoader::GetInstance().GetImageInfo(FilePath);
    m_AtlasWidth = info.Width;
    m_AtlasHeight = info.Height;

    return true;
}

void TextureAtlas::AddSprite(const std::string& Name, const ImVec2& UV0, const ImVec2& UV1) {
    SpriteFrame frame;
    frame.UV0 = UV0;
    frame.UV1 = UV1;
    frame.Duration = 0.0f;

    m_Sprites[Name] = frame;
}

Sprite TextureAtlas::GetSprite(const std::string& Name) const {
    auto it = m_Sprites.find(Name);
    if (it == m_Sprites.end()) {
        return Sprite(BGFX_INVALID_HANDLE, ImVec2(0.0f, 0.0f));
    }

    // Calculate size from UV coordinates
    float width = (it->second.UV1.x - it->second.UV0.x) * static_cast<float>(m_AtlasWidth);
    float height = (it->second.UV1.y - it->second.UV0.y) * static_cast<float>(m_AtlasHeight);

    Sprite sprite(m_Texture, ImVec2(width, height));
    sprite.SetFrame(it->second);
    return sprite;
}

bool TextureAtlas::HasSprite(const std::string& Name) const {
    return m_Sprites.find(Name) != m_Sprites.end();
}

} // namespace Solstice::UI
