#pragma once

#include "../Solstice.hxx"
#include <Entity/System.hxx>
#include <bgfx/bgfx.h>

// Forward declarations
struct SDL_Window;
union SDL_Event;
struct ImFont;

namespace Solstice::UI {

/**
 * Render settings for ImGui rendering
 * Allows different UI layers to specify their own rendering parameters
 */
struct SOLSTICE_API UIRenderSettings {
    bgfx::ViewId ViewId{10};
    bool UsePointFiltering{false};
    bool UseBlending{true};
    bool UseDepthTest{false};

    // Default constructor provides regular UI settings
    UIRenderSettings() = default;

    // Constructor for custom settings
    UIRenderSettings(bgfx::ViewId viewId, bool pointFiltering, bool blending, bool depthTest)
        : ViewId(viewId), UsePointFiltering(pointFiltering), UseBlending(blending), UseDepthTest(depthTest) {}

    // Static factory method for HUD settings (with anti-aliasing for smooth rendering)
    static UIRenderSettings HUD() {
        return UIRenderSettings(11, false, true, false);
    }

    // Static factory method for regular UI settings
    static UIRenderSettings Default() {
        return UIRenderSettings(10, false, true, false);
    }
};

/**
 * UISystem manages Dear ImGui integration and UI rendering
 * Follows singleton pattern like PhysicsSystem
 */
class SOLSTICE_API UISystem {
public:
    // Singleton access
    static UISystem& Instance();

    // Lifecycle - Initialize MUST be called AFTER BGFX is initialized
    void Initialize(SDL_Window* Window);
    void Shutdown();

    // Frame management
    void NewFrame();
    void Render();  // Renders ImGui draw data to BGFX with default settings
    void RenderWithSettings(const UIRenderSettings& settings);  // Renders ImGui draw data with custom settings

    // Event handling
    bool ProcessEvent(const SDL_Event* Event);

    // State queries
    bool IsInitialized() const { return m_Initialized; }
    bool WantCaptureMouse() const;
    bool WantCaptureKeyboard() const;

    // Configuration
    void SetViewId(bgfx::ViewId ViewId) { m_ViewId = ViewId; }
    bgfx::ViewId GetViewId() const { return m_ViewId; }

    // DLL/EXE context sharing - call GetImGuiContext() from DLL and SetCurrentContext() in EXE
    void* GetImGuiContext() const;

    // Font access
    ImFont* GetFontRegular() const { return m_FontRegular; }
    ImFont* GetFontBold() const { return m_FontBold; }
    ImFont* GetFontItalic() const { return m_FontItalic; }
    ImFont* GetFontBoldItalic() const { return m_FontBoldItalic; }

private:
    UISystem() = default;
    ~UISystem() = default;

    // Prevent copying
    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;

    // BGFX resources
    void CreateBGFXResources();
    void DestroyBGFXResources();
    void RenderDrawData(const UIRenderSettings& settings);  // Render with specified settings

    // Style setup
    void SetupAeroStyle();

    bool m_Initialized{false};
    SDL_Window* m_Window{nullptr};

    // BGFX rendering resources
    bgfx::ViewId m_ViewId{10};  // Default view ID for UI overlay
    bgfx::ProgramHandle m_Program{BGFX_INVALID_HANDLE};
    bgfx::UniformHandle m_TextureUniform{BGFX_INVALID_HANDLE};
    bgfx::TextureHandle m_FontTexture{BGFX_INVALID_HANDLE};
    bgfx::VertexLayout m_Layout;

    // Display dimensions
    int m_DisplayWidth{0};
    int m_DisplayHeight{0};

    // Font handles
    ImFont* m_FontRegular{nullptr};
    ImFont* m_FontBold{nullptr};
    ImFont* m_FontItalic{nullptr};
    ImFont* m_FontBoldItalic{nullptr};
};

} // namespace Solstice::UI
