#pragma once

#include <Solstice.hxx>
#include <Physics/Fluid/Fluid.hxx>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace Solstice::Render {

struct Camera;

/// Uploads fluid scalar fields to a 3D texture and draws raymarched volume, marching-tetra isosurface, or velocity lines.
class SOLSTICE_API FluidVolumeVisualizer {
public:
    enum class FieldMode : uint8_t { Temperature = 0, Schlieren = 1 };
    enum class DrawMode : uint8_t { Raymarch = 0, IsoSurface = 1, VelocityLines = 2 };

    FluidVolumeVisualizer() = default;
    ~FluidVolumeVisualizer();

    FluidVolumeVisualizer(const FluidVolumeVisualizer&) = delete;
    FluidVolumeVisualizer& operator=(const FluidVolumeVisualizer&) = delete;

    void Shutdown();

    /// Packs interior (Nx×Ny×Nz) into R8 3D texture (normalized per upload).
    void UploadFromSimulation(const Physics::FluidSimulation& fluid, FieldMode mode);

    /// Overlay on an existing frame (call after `SoftwareRenderer::RenderScene`, before ImGui). Uses alpha blending.
    void DrawOverlay(const Camera& cam, int viewportW, int viewportH, const Physics::FluidSimulation& fluid, DrawMode drawMode,
                     FieldMode fieldMode, float isoLevel01, int velocityStride, float velocityScale, bgfx::ViewId viewId = 0);

private:
    void EnsureRayResources();
    void EnsureDebugResources();
    void EnsureTexture3D(uint16_t nx, uint16_t ny, uint16_t nz);

    bgfx::ProgramHandle m_RayProgram{bgfx::kInvalidHandle};
    bgfx::VertexLayout m_RayLayout{};
    bgfx::UniformHandle m_uFluidInvViewProj{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidCameraPos{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidAabbMin{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidAabbMax{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidVisParams{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidViewport{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidClipMin{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidClipMax{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_uFluidClipParams{bgfx::kInvalidHandle};
    bgfx::UniformHandle m_sTexVolume{bgfx::kInvalidHandle};

    bgfx::ProgramHandle m_DebugProgram{bgfx::kInvalidHandle};
    bgfx::VertexLayout m_DebugLayout{};

    bgfx::TextureHandle m_VolumeTex{bgfx::kInvalidHandle};
    uint16_t m_TexW = 0;
    uint16_t m_TexH = 0;
    uint16_t m_TexD = 0;

    std::vector<uint8_t> m_UploadScratch;
    std::vector<float> m_SchlierenScratch;
};

} // namespace Solstice::Render
