#include <Render/Context.hxx>
#include <stdexcept>

namespace Solstice::Render {

RenderContext::RenderContext() {
    // BGFX state management is handled differently
    // State is set per-draw call or via bgfx::setState()
    m_DepthTestEnabled = true;
    m_BlendEnabled = true;
    m_FaceCullingEnabled = true;
}

size_t RenderContext::AddViewport(const Viewport& Viewport) {
    m_Viewports.push_back(Viewport);
    return m_Viewports.size() - 1;
}

void RenderContext::RemoveViewport(size_t Index) {
    if (Index >= m_Viewports.size()) {
        throw std::out_of_range("Viewport index out of range");
    }
    m_Viewports.erase(m_Viewports.begin() + Index);
    if (m_ActiveViewport >= m_Viewports.size() && !m_Viewports.empty()) {
        m_ActiveViewport = m_Viewports.size() - 1;
    }
}

Viewport& RenderContext::GetViewport(size_t Index) {
    if (Index >= m_Viewports.size()) {
        throw std::out_of_range("Viewport index out of range");
    }
    return m_Viewports[Index];
}

const Viewport& RenderContext::GetViewport(size_t Index) const {
    if (Index >= m_Viewports.size()) {
        throw std::out_of_range("Viewport index out of range");
    }
    return m_Viewports[Index];
}

void RenderContext::SetActiveViewport(size_t Index) {
    if (Index >= m_Viewports.size()) {
        throw std::out_of_range("Viewport index out of range");
    }
    m_ActiveViewport = Index;
}

Viewport& RenderContext::GetActiveViewport() {
    if (m_Viewports.empty()) {
        throw std::runtime_error("No viewports available");
    }
    return m_Viewports[m_ActiveViewport];
}

const Viewport& RenderContext::GetActiveViewport() const {
    if (m_Viewports.empty()) {
        throw std::runtime_error("No viewports available");
    }
    return m_Viewports[m_ActiveViewport];
}

void RenderContext::BeginFrame() {
    if (m_InFrame) {
        throw std::runtime_error("Already in a frame");
    }
    m_InFrame = true;
    
    // BGFX frame clearing is handled per-view
    // We'll set the clear color when we begin each viewport
}

void RenderContext::BeginViewport(size_t Index) {
    if (!m_InFrame) {
        throw std::runtime_error("Must call BeginFrame() before BeginViewport()");
    }
    
    const auto& viewport = GetViewport(Index);
    if (!viewport.Active) return;
    
    // BGFX viewport is set via bgfx::setViewRect
    // This would typically be done when setting up the view for rendering
    // For now, we'll just store the viewport state
    
    // Note: BGFX uses bgfx::setViewRect() and bgfx::setViewScissor()
    // These are typically called before submitting draw calls
}

void RenderContext::EndViewport() {
    if (!m_InFrame) {
        throw std::runtime_error("Not in a frame");
    }
    // BGFX viewport management is handled differently
}

void RenderContext::EndFrame() {
    if (!m_InFrame) {
        throw std::runtime_error("Not in a frame");
    }
    m_InFrame = false;
}

void RenderContext::EnableDepthTest(bool Enable) {
    m_DepthTestEnabled = Enable;
    // BGFX: Depth test is enabled via bgfx::setState() per draw call
}

void RenderContext::EnableBlend(bool Enable) {
    m_BlendEnabled = Enable;
    // BGFX: Blending is enabled via bgfx::setState() per draw call
}

void RenderContext::EnableFaceCulling(bool Enable) {
    m_FaceCullingEnabled = Enable;
    // BGFX: Face culling is enabled via bgfx::setState() per draw call
}

Math::Matrix4 RenderContext::CalculateProjectionMatrix(const Viewport& Viewport, float FOV, float Near, float Far) {
    if (Viewport.Width <= 0 || Viewport.Height <= 0) {
        return Math::Matrix4::Identity();
    }
    
    float aspect = static_cast<float>(Viewport.Width) / static_cast<float>(Viewport.Height);
    float fovRadians = FOV * 3.14159265359f / 180.0f; // Convert to radians
    return Math::Matrix4::Perspective(fovRadians, aspect, Near, Far);
}

// DefaultRenderTarget implementation
void DefaultRenderTarget::Bind() const {
    // BGFX: Default framebuffer binding is handled by bgfx::setViewFrameBuffer with BGFX_INVALID_HANDLE
}

void DefaultRenderTarget::Unbind() const {
    // BGFX: No explicit unbind needed
}

Math::Vec2 DefaultRenderTarget::GetSize() const {
    // Return a default size or query from window
    // This should ideally be set from the window dimensions
    return Math::Vec2(1280.0f, 720.0f);
}

void DefaultRenderTarget::Resize(Math::Vec2 NewSize) {
    // Default render target (screen) resize is handled by window events
}

} // namespace Solstice::Render
