#pragma once

#include <Render/Scene/Camera.hxx>
#include <Render/Base/IRenderer.hxx>
#include <Math/Vector.hxx>
#include <Math/Matrix.hxx>
#include <memory>
#include <vector>
#include <string>
#include <bgfx/bgfx.h>

namespace Solstice::Render {

struct RenderConfig {
    int Width{1280};
    int Height{720};
    bool VSync{true};
    bool Fullscreen{false};
};

struct RenderStats {
    uint32_t DrawCalls{0};
    uint32_t TrianglesDrawn{0};
    float FrameTime{0.0f};
};

// Viewport structure for rendering
struct SOLSTICE_API Viewport {
    uint32_t X{0};
    uint32_t Y{0};
    uint32_t Width{0};
    uint32_t Height{0};
    Math::Vec4 ClearColor{0.1f, 0.1f, 0.1f, 1.0f};
    bool Active{true};
};

// Base class for render targets
class SOLSTICE_API RenderTarget {
public:
    virtual ~RenderTarget() = default;
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual Math::Vec2 GetSize() const = 0;
    virtual void Resize(Math::Vec2 NewSize) = 0;
    virtual uint16_t GetFramebufferHandle() const { return UINT16_MAX; }
    virtual uint16_t GetColorTextureHandle() const { return UINT16_MAX; }
    virtual uint16_t GetDepthTextureHandle() const { return UINT16_MAX; }
};

class DefaultRenderTarget : public RenderTarget {
public:
    void Bind() const override;
    void Unbind() const override;
    Math::Vec2 GetSize() const override;
    void Resize(Math::Vec2 NewSize) override;
};

class FramebufferRenderTarget : public RenderTarget {
public:
    FramebufferRenderTarget(Math::Vec2 Size, bool HasDepth = true);
    ~FramebufferRenderTarget();
    
    void Bind() const override;
    void Unbind() const override;
    Math::Vec2 GetSize() const override { return m_Size; }
    void Resize(Math::Vec2 NewSize) override;
    
    // Access to BGFX handle
    bgfx::FrameBufferHandle GetHandle() const { return m_Handle; }
    uint16_t GetFramebufferHandle() const override { return m_Handle.idx; }
    uint16_t GetColorTextureHandle() const override { return UINT16_MAX; }
    uint16_t GetDepthTextureHandle() const override { return UINT16_MAX; }
    
private:
    bgfx::FrameBufferHandle m_Handle{bgfx::kInvalidHandle};
    Math::Vec2 m_Size{0.0f, 0.0f};
    bool m_HasDepth{true};
    
    void CreateFramebuffer();
    void DeleteFramebuffer();
};

class RenderContext {
public:
    RenderContext();
    ~RenderContext() = default;

    // Viewport management
    size_t AddViewport(const Viewport& Viewport);
    void RemoveViewport(size_t Index);
    Viewport& GetViewport(size_t Index);
    const Viewport& GetViewport(size_t Index) const;
    size_t GetViewportCount() const { return m_Viewports.size(); }
    
    // Active viewport control
    void SetActiveViewport(size_t Index);
    size_t GetActiveViewportIndex() const { return m_ActiveViewport; }
    Viewport& GetActiveViewport();
    const Viewport& GetActiveViewport() const;
    
    // Viewport rendering
    void BeginFrame();
    void BeginViewport(size_t Index);
    void EndViewport();
    void EndFrame();
    
    // State management
    void EnableDepthTest(bool Enable = true);
    void EnableBlend(bool Enable = true);
    void EnableFaceCulling(bool Enable = true);
    
    // Global settings
    void SetClearColor(const Math::Vec4& Color) { m_ClearColor = Color; }
    const Math::Vec4& GetClearColor() const { return m_ClearColor; }
    
    // Projection parameters
    void SetNearPlane(float Near) { m_NearPlane = Near; }
    void SetFarPlane(float Far) { m_FarPlane = Far; }
    void SetFieldOfView(float FOV) { m_FieldOfView = FOV; }
    
    float GetNearPlane() const { return m_NearPlane; }
    float GetFarPlane() const { return m_FarPlane; }
    float GetFieldOfView() const { return m_FieldOfView; }
    
    // Helper functions
    static Math::Matrix4 CalculateProjectionMatrix(const Viewport& Viewport, float FOV, float Near, float Far);
    
private:
    std::vector<Viewport> m_Viewports;
    size_t m_ActiveViewport{0};
    
    // Global clear color (used when viewport doesn't specify one)
    Math::Vec4 m_ClearColor{0.1f, 0.1f, 0.1f, 1.0f};
    
    // Rendering state
    bool m_DepthTestEnabled{true};
    bool m_BlendEnabled{true};
    bool m_FaceCullingEnabled{true};
    
    // Projection parameters
    float m_NearPlane{0.1f};
    float m_FarPlane{1000.0f};
    float m_FieldOfView{45.0f};
    
    // Internal state
    bool m_InFrame{false};
};

} // namespace Solstice::Render