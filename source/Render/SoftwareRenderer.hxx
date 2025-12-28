#pragma once

#include "../Solstice.hxx"
#include <Core/Allocator.hxx>
#include <Core/Async.hxx>
#include <Render/Mesh.hxx>
#include <Core/Material.hxx>
#include <Render/Camera.hxx>
#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <Render/PostProcessing.hxx>
#include <Render/Scene.hxx>
#include <Render/Skybox.hxx>
#include <Render/TextureRegistry.hxx>
#include <Render/Raytracing.hxx>
#include <Render/ShadowRenderer.hxx>
#include <Render/SceneRenderer.hxx>
#include <Physics/LightSource.hxx>
#include <bgfx/bgfx.h>
#include <set>
#include <cstdint>

// Forward declarations
struct SDL_Window;
namespace Solstice { namespace ECS { class Registry; } }

namespace Solstice::Render {

// Custom BGFX callback for error logging
class SOLSTICE_API BgfxCallback : public bgfx::CallbackI {
public:
    virtual ~BgfxCallback() {}

    virtual void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override;
    virtual void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override;
    virtual void profilerBegin(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override {}
    virtual void profilerBeginLiteral(const char* _name, uint32_t _abgr, const char* _filePath, uint16_t _line) override {}
    virtual void profilerEnd() override {}
    virtual uint32_t cacheReadSize(uint64_t _id) override { return 0; }
    virtual bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override { return false; }
    virtual void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override {}
    virtual void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t _size, bool _yflip) override {}
    virtual void captureBegin(uint32_t _width, uint32_t _height, uint32_t _pitch, bgfx::TextureFormat::Enum _format, bool _yflip) override {}
    virtual void captureEnd() override {}
    virtual void captureFrame(const void* _data, uint32_t _size) override {}
};

// Viewport configuration for multi-viewport rendering
struct SOLSTICE_API ViewportConfig {
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    bool Active = true;
};

class SOLSTICE_API SoftwareRenderer {
public:
    static constexpr int TILE_SIZE = 16;
    SoftwareRenderer(int Width, int Height, int TileSize = TILE_SIZE, SDL_Window* Window = nullptr);
    ~SoftwareRenderer();

    // Delete copy constructor and assignment operator (contains non-copyable std::future)
    SoftwareRenderer(const SoftwareRenderer&) = delete;
    SoftwareRenderer& operator=(const SoftwareRenderer&) = delete;

    void RenderScene(Scene& SceneGraph, const Camera& Cam);
    void RenderScene(Scene& SceneGraph, const Camera& Cam, const std::vector<Physics::LightSource>& Lights);

    // VR stereo rendering
    void RenderSceneVR(Scene& SceneGraph, const Camera& Cam, bool LeftEye);

    // Multi-viewport support
    void SetViewportCount(uint32_t Count);
    uint32_t GetViewportCount() const { return static_cast<uint32_t>(m_Viewports.size()); }
    void SetViewport(uint32_t Index, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height);
    void GetViewport(uint32_t Index, uint32_t& X, uint32_t& Y, uint32_t& Width, uint32_t& Height) const;

    // Clear buffers
    void Clear(const Solstice::Math::Vec4& Color);

    // Present to screen
    void Present();

    // VSync Control
    void SetVSync(bool Enable);

    // Optimization Control
    void SetOptimizeStaticBuffers(bool Enable) { m_OptimizeStaticBuffers = Enable; }

    // Resize viewport
    void Resize(int NewWidth, int NewHeight);

    // UI helpers
    void SetShowCrosshair(bool Enable) { m_ShowCrosshair = Enable; }

    // Configuration
    void SetTileSize(int Size); // Kept for API compatibility, no-op
    void EnableDepthTest(bool Enable) { m_DepthTestEnabled = Enable; }
    void SetWireframe(bool Enable);
    void SetShowDebugOverlay(bool Enable);

    // Initialization state
    bool IsInitialized() const { return m_Initialized; }

    // Stats
    struct RenderStats {
        uint32_t VisibleObjects;
        uint32_t TrianglesSubmitted;
        uint32_t TrianglesCulled;
        uint32_t TrianglesRendered;
        float CullTimeMs;
        float TransformTimeMs;
        float RasterTimeMs;
        float TotalTimeMs;
    };

    const RenderStats& GetStats() const { return m_Stats; }

    // Async rendering control
    void SetAsyncRendering(bool Enable) { m_EnableAsync = Enable; }
    bool IsAsyncEnabled() const { return m_EnableAsync; }

    // SIMD control
    void SetUseSIMD(bool Enable) { m_UseSIMD = Enable; }
    bool IsUsingSIMD() const { return m_UseSIMD; }

    // Physics debug visualization
    void SetPhysicsDebugMode(bool Enable) { m_ShowPhysicsDebug = Enable; }
    void RenderPhysicsDebug(const void* PhysicsSystem); // void* to avoid circular dependency

    // Optional physics integration: when set, RenderScene will sync physics transforms to the scene
    void SetPhysicsRegistry(Solstice::ECS::Registry* R) { m_PhysicsRegistry = R; }

    // Selection and hover state
    void SetSelectedObjects(const std::set<SceneObjectID>& objects);
    void SetHoveredObject(SceneObjectID objectID);

    // Skybox
    void SetSkybox(Skybox* skybox);

    // Texture registry
    TextureRegistry& GetTextureRegistry() { return m_TextureRegistry; }
    const TextureRegistry& GetTextureRegistry() const { return m_TextureRegistry; }

    // Post-processing access
    PostProcessing* GetPostProcessing() { return m_PostProcessing.get(); }
    const PostProcessing* GetPostProcessing() const { return m_PostProcessing.get(); }

private:
    // BGFX specific
    void UploadFramebufferToGPU();

private:
    void AllocateFramebuffers();
    void InitializeBGFX(SDL_Window* Window);

    // Shader helpers
    bgfx::ShaderHandle LoadShader(const std::string& Name);
    bgfx::ProgramHandle m_CubeProgram;
    bgfx::VertexLayout m_CubeLayout;
    bgfx::ProgramHandle m_UIProgram{bgfx::kInvalidHandle};
    bgfx::VertexLayout m_UILayout;
    bgfx::ProgramHandle m_SkyboxProgram{bgfx::kInvalidHandle};
    bgfx::ProgramHandle m_DebugProgram{bgfx::kInvalidHandle};
    bgfx::VertexLayout m_DebugLayout;

    // CPU Rasterizer state
    int m_Width;
    int m_Height;
    int m_TilesX;
    int m_TilesY;

    std::vector<std::vector<uint32_t>> m_TileBins;  // Triangle indices per tile
    std::vector<uint32_t> m_ColorBuffer;
    std::vector<float> m_DepthBuffer;

    // BGFX Resources (for display only)
    bgfx::TextureHandle m_DisplayTexture{bgfx::kInvalidHandle};
    bgfx::FrameBufferHandle m_DisplayFramebuffer{bgfx::kInvalidHandle};
    bgfx::ProgramHandle m_BlitProgram{bgfx::kInvalidHandle};

    // Configuration
    bool m_Initialized{false};
    bool m_DepthTestEnabled;
    bool m_WireframeEnabled{false};
    bool m_ShowDebugOverlay{false};
    bool m_ShowCrosshair{false};

    // Stats
    RenderStats m_Stats;

    // Async rendering
    bool m_EnableAsync{true};
    std::vector<std::future<void>> m_RenderJobs;

    // SIMD optimization
    bool m_UseSIMD{true};

    // Optimization flags
    bool m_OptimizeStaticBuffers{true};
    bool m_VSyncEnabled{true};

    // Memory management
    Core::ArenaAllocator m_FrameAllocator;

    // Physics debug
    bool m_ShowPhysicsDebug{false};

    // Post Processing
    std::unique_ptr<PostProcessing> m_PostProcessing;

    // Raytracing for advanced lighting
    std::unique_ptr<Raytracing> m_Raytracing;

    // Rendering helper classes
    std::unique_ptr<ShadowRenderer> m_ShadowRenderer;
    std::unique_ptr<SceneRenderer> m_SceneRenderer;

    // Optional physics integration
    Solstice::ECS::Registry* m_PhysicsRegistry{nullptr};

    // Selection and hover state
    std::set<SceneObjectID> m_SelectedObjects;
    SceneObjectID m_HoveredObject{InvalidObjectID};

    // Skybox
    Skybox* m_Skybox{nullptr};

    // Texture registry
    TextureRegistry m_TextureRegistry;

    // Multi-viewport support
    std::vector<ViewportConfig> m_Viewports;
    uint32_t m_ActiveViewport{0};

    // Complexity thresholds for async/SIMD
    static constexpr uint32_t ASYNC_THRESHOLD_OBJECTS = 10;
    static constexpr uint32_t ASYNC_THRESHOLD_TRIANGLES = 1000;
    static constexpr uint32_t SIMD_THRESHOLD_VERTICES = 100;

    // Resource references (not owned)
    Render::MeshLibrary* m_MeshLibrary;
    Core::MaterialLibrary* m_MaterialLibrary;

    // Transformed vertex cache
    struct TransformedVertex {
        Solstice::Math::Vec3 ScreenPos;  // x, y in pixels, z = depth
        Solstice::Math::Vec3 Normal;
        Solstice::Math::Vec2 UV;
    };
    std::vector<TransformedVertex> m_TransformedVerts;

    // Pre-allocated buffers for common operations (reused across frames)
    mutable std::vector<SceneObjectID> m_PreallocatedVisibleObjects; // Pre-allocated for culling results

    // Helper functions
    void TransformVertices(const std::vector<QuantizedVertex>& Vertices, const Solstice::Math::Matrix4& MVP);
    void BinTriangles(const std::vector<uint32_t>& Indices);
    void RasterizeTiles();
    void RasterizeTriangle(uint32_t TriIndex, const std::vector<uint32_t>& Indices, int TileX, int TileY);
    float EdgeFunction(const Solstice::Math::Vec2& a, const Solstice::Math::Vec2& b, const Solstice::Math::Vec2& c) const;

    // Async job helpers
    void WaitForJobs();
    void SubmitTileRasterJob(int TileX, int TileY, const std::vector<uint32_t>& Indices);

    // Scene complexity analysis
    bool ShouldUseAsync(uint32_t ObjectCount, uint32_t TriangleCount) const;
    bool ShouldUseSIMD(uint32_t VertexCount) const;
};

} // namespace Solstice::Render
