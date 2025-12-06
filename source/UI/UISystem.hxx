#pragma once

#include "../SolsticeExport.hxx"
#include <Entity/System.hxx>
#include <bgfx/bgfx.h>

// Forward declarations
struct SDL_Window;
union SDL_Event;

namespace Solstice::UI {

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
    void Render();  // Renders ImGui draw data to BGFX
    
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

private:
    UISystem() = default;
    ~UISystem() = default;
    
    // Prevent copying
    UISystem(const UISystem&) = delete;
    UISystem& operator=(const UISystem&) = delete;
    
    // BGFX resources
    void CreateBGFXResources();
    void DestroyBGFXResources();
    void RenderDrawData();
    
    bool m_Initialized{false};
    SDL_Window* m_Window{nullptr};
    
    // BGFX rendering resources
    bgfx::ViewId m_ViewId{255};  // High view ID for UI overlay
    bgfx::ProgramHandle m_Program{BGFX_INVALID_HANDLE};
    bgfx::UniformHandle m_TextureUniform{BGFX_INVALID_HANDLE};
    bgfx::TextureHandle m_FontTexture{BGFX_INVALID_HANDLE};
    bgfx::VertexLayout m_Layout;
    
    // Display dimensions
    int m_DisplayWidth{0};
    int m_DisplayHeight{0};
};

} // namespace Solstice::UI