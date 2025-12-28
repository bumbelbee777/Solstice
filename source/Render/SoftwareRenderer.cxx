#include <atomic>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Mesh.hxx>
#include <Render/ShaderLoader.hxx>
#include <Math/Vector.hxx>
#include <Core/Debug.hxx>
#include <Core/SIMD.hxx>
#include <Solstice.hxx>
#include <UI/UISystem.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <cstdarg>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <fstream>
#include <Render/PhysicsBridge.hxx>
#include <Physics/PhysicsSystem.hxx>
#include <reactphysics3d/engine/PhysicsWorld.h>
#include <reactphysics3d/utils/DebugRenderer.h>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Solstice::Render {
namespace Math = Solstice::Math;

// Static instance counter for BGFX management
static std::atomic<int> s_RendererInstanceCount{0};

// Implement BGFX callback methods
void BgfxCallback::fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) {
    SIMPLE_LOG("BGFX FATAL ERROR:");
    SIMPLE_LOG("  File: " + std::string(_filePath ? _filePath : "unknown"));
    SIMPLE_LOG("  Line: " + std::to_string(_line));
    SIMPLE_LOG("  Code: " + std::to_string(static_cast<int>(_code)));
    SIMPLE_LOG("  Message: " + std::string(_str ? _str : "no message"));
}

void BgfxCallback::traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) {
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), _format, _argList);
    SIMPLE_LOG("BGFX TRACE: " + std::string(buffer));
}


SoftwareRenderer::SoftwareRenderer(int Width, int Height, int TileSize, SDL_Window* Window)
    : m_Width(Width)
    , m_Height(Height)
    , m_DepthTestEnabled(true)
    , m_MeshLibrary(nullptr)
    , m_MaterialLibrary(nullptr)
    , m_FrameAllocator(16 * 1024 * 1024) // 16MB per-frame allocator
{
    s_RendererInstanceCount++;
    SIMPLE_LOG("SoftwareRenderer: Initializing CPU rasterizer (Instance #" + std::to_string(s_RendererInstanceCount.load()) + ")...");

    // Calculate tile grid dimensions
    m_TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
    m_TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;

    // Allocate framebuffers
    AllocateFramebuffers();

    // Initialize BGFX for display only
    InitializeBGFX(Window);

    SIMPLE_LOG("SoftwareRenderer: Initialized " + std::to_string(m_TilesX) + "x" + std::to_string(m_TilesY) + " tiles");

    // Initialize Post Processing
    m_PostProcessing = std::make_unique<PostProcessing>();
    m_PostProcessing->Initialize(Width, Height);

    // Initialize helper renderers (after PostProcessing and shaders are loaded)
    // Shaders are loaded in InitializeBGFX, which was called above
    if (m_PostProcessing) {
        m_ShadowRenderer = std::make_unique<ShadowRenderer>();
        bgfx::ProgramHandle shadowProg = m_PostProcessing->GetShadowProgram();
        if (bgfx::isValid(shadowProg)) {
            m_ShadowRenderer->Initialize(shadowProg, m_CubeLayout, m_PostProcessing.get(), 1024);
            SIMPLE_LOG("SoftwareRenderer: Initialized ShadowRenderer");
        } else {
            SIMPLE_LOG("WARNING: Shadow program invalid, ShadowRenderer not initialized");
        }

        m_SceneRenderer = std::make_unique<SceneRenderer>();
        if (bgfx::isValid(m_CubeProgram)) {
            m_SceneRenderer->Initialize(m_CubeProgram, m_CubeLayout, m_PostProcessing.get(),
                                        &m_TextureRegistry, m_Skybox, m_SkyboxProgram, m_Width, m_Height);
            SIMPLE_LOG("SoftwareRenderer: Initialized SceneRenderer");
        } else {
            SIMPLE_LOG("WARNING: Cube program invalid, SceneRenderer not initialized");
        }
    } else {
        SIMPLE_LOG("ERROR: PostProcessing not initialized, cannot initialize helper renderers");
    }

    // Initialize Raytracing for advanced lighting
    m_Raytracing = std::make_unique<Raytracing>();
    Math::Vec3 worldMin(-100.0f, -100.0f, -100.0f);
    Math::Vec3 worldMax(100.0f, 100.0f, 100.0f);
    m_Raytracing->Initialize(Width, Height, worldMin, worldMax);
    SIMPLE_LOG("SoftwareRenderer: Initialized raytracing system");
}

SoftwareRenderer::~SoftwareRenderer() {
    // Shutdown raytracing
    if (m_Raytracing) {
        m_Raytracing->Shutdown();
    }

    // Clean up shader programs
    if (bgfx::isValid(m_SkyboxProgram)) {
        bgfx::destroy(m_SkyboxProgram);
    }
    if (bgfx::isValid(m_UIProgram)) {
        bgfx::destroy(m_UIProgram);
    }
    if (bgfx::isValid(m_CubeProgram)) {
        bgfx::destroy(m_CubeProgram);
    }

    if (bgfx::isValid(m_DisplayFramebuffer)) {
        bgfx::destroy(m_DisplayFramebuffer);
    }
    if (bgfx::isValid(m_DisplayTexture)) {
        bgfx::destroy(m_DisplayTexture);
    }
    if (bgfx::isValid(m_BlitProgram)) {
        bgfx::destroy(m_BlitProgram);
    }

    m_Initialized = false;
    int remaining = --s_RendererInstanceCount;

    // Shutdown BGFX if we were the last one using it
    if (remaining == 0) {
        SIMPLE_LOG("SoftwareRenderer: Last instance destroyed, shutting down BGFX");
        bgfx::shutdown();
    } else {
        SIMPLE_LOG("SoftwareRenderer: Instance destroyed (" + std::to_string(remaining) + " remaining)");
    }
}

void SoftwareRenderer::AllocateFramebuffers() {
    size_t pixelCount = m_Width * m_Height;

    // Allocate color buffer (RGBA8)
    m_ColorBuffer.resize(pixelCount, 0xFF000000);  // Black with full alpha

    // Allocate depth buffer
    m_DepthBuffer.resize(pixelCount, 1.0f);  // Far plane

    // Allocate tile bins
    size_t tileCount = m_TilesX * m_TilesY;
    m_TileBins.resize(tileCount);
}

void SoftwareRenderer::InitializeBGFX(SDL_Window* Window) {
    // Check if BGFX is already initialized
    if (bgfx::getRendererType() == bgfx::RendererType::Noop) {

        // Create custom callback for error logging
        static BgfxCallback callback;

        // Get native window handle from SDL
        bgfx::PlatformData pd;
        memset(&pd, 0, sizeof(pd));

#if defined(_WIN32)
        // On Windows, get the HWND from SDL
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (!pd.nwh) {
            SIMPLE_LOG("ERROR: Failed to get native window handle from SDL");
            throw std::runtime_error("Failed to get native window handle");
        }
#elif defined(__APPLE__)
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(__ANDROID__)
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#elif defined(__EMSCRIPTEN__)
        pd.nwh = (void*)"#canvas";
#elif defined(__linux__)
        pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = (void*)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        if (!pd.nwh) {
            // Try Wayland if X11 fails
            pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        }
#endif

        SIMPLE_LOG("Configuring BGFX init parameters...");

        // List of renderer types to try in order
        std::vector<bgfx::RendererType::Enum> renderersToTry;

#if defined(__EMSCRIPTEN__)
        renderersToTry.push_back(bgfx::RendererType::WebGPU);
        renderersToTry.push_back(bgfx::RendererType::OpenGLES);
#elif defined(_WIN32)
        renderersToTry.push_back(bgfx::RendererType::Direct3D11);
        renderersToTry.push_back(bgfx::RendererType::Direct3D12);
        renderersToTry.push_back(bgfx::RendererType::Vulkan);
        renderersToTry.push_back(bgfx::RendererType::OpenGL);
#elif defined(__APPLE__)
        renderersToTry.push_back(bgfx::RendererType::Metal);
        renderersToTry.push_back(bgfx::RendererType::OpenGL);
#else
        renderersToTry.push_back(bgfx::RendererType::Vulkan);
        renderersToTry.push_back(bgfx::RendererType::OpenGL);
        renderersToTry.push_back(bgfx::RendererType::OpenGLES);
#endif

        bool initSuccess = false;
        bgfx::RendererType::Enum successfulRenderer = bgfx::RendererType::Noop;

        for (auto rendererType : renderersToTry) {
            const char* rendererName = bgfx::getRendererName(rendererType);
            SIMPLE_LOG("Trying renderer: " + std::string(rendererName));

            bgfx::Init init;
            init.type = rendererType;
            init.vendorId = BGFX_PCI_ID_NONE;
            init.resolution.width = m_Width;
            init.resolution.height = m_Height;
            init.resolution.reset = 0; // No VSync by default for uncapped FPS
            init.callback = &callback;
            init.allocator = nullptr;

            // CRITICAL: Set platform data directly in init structure, not via setPlatformData!
            // The legacy setPlatformData() gets overridden by init.platformData
            init.platformData = pd;

            if (bgfx::init(init)) {
                SIMPLE_LOG("SUCCESS: " + std::string(rendererName) + " initialized!");
                initSuccess = true;
                successfulRenderer = rendererType;
                break;
            } else {
                SIMPLE_LOG("FAILED: " + std::string(rendererName) + " initialization failed");
            }
        }

        if (!initSuccess) {
            SIMPLE_LOG("ERROR: All renderer types failed to initialize");
            SIMPLE_LOG("  Width: " + std::to_string(m_Width));
            SIMPLE_LOG("  Height: " + std::to_string(m_Height));
            SIMPLE_LOG("  Window handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.nwh)));
            throw std::runtime_error("Failed to initialize BGFX - no renderer available");
        }

        SIMPLE_LOG("BGFX initialized successfully with SDL window");
        SIMPLE_LOG("  Renderer: " + std::string(bgfx::getRendererName(successfulRenderer)));
    } else {
        // BGFX already initialized, just reset the resolution
        SIMPLE_LOG("BGFX already initialized, resetting resolution...");
        bgfx::reset(m_Width, m_Height, BGFX_RESET_MSAA_X16); // No VSync by default
    }

    bgfx::setDebug(m_ShowDebugOverlay ? BGFX_DEBUG_TEXT : 0);
    bgfx::dbgTextClear(true);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(m_Width), static_cast<uint16_t>(m_Height));

    // Initialize vertex layout to match QuantizedVertex (32 bytes total)
    // Note: Tangents are computed in shader from normals for normal mapping
    // TODO: Extend QuantizedVertex to include tangents for better quality normal mapping
    m_CubeLayout
        .begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();

    if (m_CubeLayout.getStride() != sizeof(QuantizedVertex)) {
        SIMPLE_LOG("CRITICAL ERROR: Vertex layout stride mismatch!"
            " Layout=" + std::to_string(m_CubeLayout.getStride()) +
            " Struct=" + std::to_string(sizeof(QuantizedVertex)));
        throw std::runtime_error("BGFX vertex layout stride mismatch");
    }

    // Load shaders
    SIMPLE_LOG("Loading shaders from shaders/...");
    bgfx::ShaderHandle vsh = LoadShader("vs_standard.bin");
    bgfx::ShaderHandle fsh = LoadShader("fs_standard.bin");

    if (!bgfx::isValid(vsh)) {
        SIMPLE_LOG("ERROR: Failed to load vertex shader 'vs_standard.bin'");
        SIMPLE_LOG("  Make sure shaders are compiled to 'source/Shaders/bin/' directory");
        throw std::runtime_error("Failed to load vertex shader - shader binaries missing");
    }

    if (!bgfx::isValid(fsh)) {
        SIMPLE_LOG("ERROR: Failed to load fragment shader 'fs_standard.bin'");
        SIMPLE_LOG("  Make sure shaders are compiled to 'source/Shaders/bin/' directory");
        bgfx::destroy(vsh);
        throw std::runtime_error("Failed to load fragment shader - shader binaries missing");
    }

    m_CubeProgram = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(m_CubeProgram)) {
        SIMPLE_LOG("ERROR: Failed to create BGFX program");
        throw std::runtime_error("Failed to create BGFX program");
    }

    SIMPLE_LOG("Successfully created BGFX program");

    // UI program (optional)
    bgfx::ShaderHandle vshUi = LoadShader("vs_ui.bin");
    bgfx::ShaderHandle fshUi = LoadShader("fs_ui.bin");
    if (bgfx::isValid(vshUi) && bgfx::isValid(fshUi)) {
        m_UIProgram = bgfx::createProgram(vshUi, fshUi, true);
        m_UILayout
            .begin(bgfx::getRendererType())
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .end();
        SIMPLE_LOG("UI program loaded");
    } else {
        SIMPLE_LOG("UI program not found; crosshair will be disabled");
        m_ShowCrosshair = false;
    }

    // Skybox program (optional)
    bgfx::ShaderHandle vshSkybox = LoadShader("vs_skybox.bin");
    bgfx::ShaderHandle fshSkybox = LoadShader("fs_skybox.bin");
    if (bgfx::isValid(vshSkybox) && bgfx::isValid(fshSkybox)) {
        m_SkyboxProgram = bgfx::createProgram(vshSkybox, fshSkybox, true);
        SIMPLE_LOG("Skybox program loaded");
    } else {
        SIMPLE_LOG("Skybox program not found; skybox rendering will be disabled");
    }

    // Helper renderers will be initialized after PostProcessing is ready
    // (PostProcessing is initialized in constructor after InitializeBGFX)
    m_Initialized = true;
}

bgfx::ShaderHandle SoftwareRenderer::LoadShader(const std::string& Name) {
    return ShaderLoader::LoadShader(Name);
}

void SoftwareRenderer::Clear(const Math::Vec4& Color) {
    if (!m_Initialized) return;

    // Convert color to RGBA8
    uint8_t r = static_cast<uint8_t>(std::clamp(Color.x, 0.0f, 1.0f) * 255.0f);
    uint8_t g = static_cast<uint8_t>(std::clamp(Color.y, 0.0f, 1.0f) * 255.0f);
    uint8_t b = static_cast<uint8_t>(std::clamp(Color.z, 0.0f, 1.0f) * 255.0f);
    uint8_t a = static_cast<uint8_t>(std::clamp(Color.w, 0.0f, 1.0f) * 255.0f);
    uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, rgba, 1.0f, 0);
    bgfx::touch(0);
}


void SoftwareRenderer::Resize(int NewWidth, int NewHeight) {
    if (!m_Initialized) return;
    if (NewWidth <= 0 || NewHeight <= 0) return; // Ignore spurious events
    if (NewWidth == m_Width && NewHeight == m_Height) return; // No-op if unchanged
    m_Width = NewWidth;
    m_Height = NewHeight;

    // Recalculate tile grid
    m_TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
    m_TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;

    // Reallocate framebuffers
    AllocateFramebuffers();

    // Update BGFX (preserve MSAA)
    uint32_t resetFlags = BGFX_RESET_MSAA_X4;
    if (m_VSyncEnabled) {
        resetFlags |= BGFX_RESET_VSYNC;
    }
    bgfx::reset(m_Width, m_Height, resetFlags);

    // Resize PostProcessing buffers
    if (m_PostProcessing) {
        m_PostProcessing->Resize(m_Width, m_Height);
    }

    // Update SceneRenderer with new dimensions
    if (m_SceneRenderer) {
        m_SceneRenderer->Initialize(m_CubeProgram, m_CubeLayout, m_PostProcessing.get(),
                                    &m_TextureRegistry, m_Skybox, m_SkyboxProgram, m_Width, m_Height);
    }

    // Resize raytracing system
    if (m_Raytracing) {
        // Raytracing doesn't have a Resize method, so we need to reinitialize
        // Get the current world bounds (or use defaults)
        Math::Vec3 worldMin(-100.0f, -100.0f, -100.0f);
        Math::Vec3 worldMax(100.0f, 100.0f, 100.0f);
        m_Raytracing->Shutdown();
        m_Raytracing->Initialize(m_Width, m_Height, worldMin, worldMax);
    }
}

void SoftwareRenderer::SetTileSize(int Size) {
    // No-op - tile size is compile-time constant
}

void SoftwareRenderer::SetVSync(bool Enable) {
    if (m_VSyncEnabled == Enable) return;
    m_VSyncEnabled = Enable;

    uint32_t flags = BGFX_RESET_MSAA_X4; // Default MSAA
    if (m_VSyncEnabled) {
        flags |= BGFX_RESET_VSYNC;
    }

    bgfx::reset(m_Width, m_Height, flags);
    SIMPLE_LOG("VSync " + std::string(Enable ? "Enabled" : "Disabled"));
}

void SoftwareRenderer::UploadFramebufferToGPU() {
    // Create texture on first call
    if (!bgfx::isValid(m_DisplayTexture)) {
        m_DisplayTexture = bgfx::createTexture2D(
            static_cast<uint16_t>(m_Width),
            static_cast<uint16_t>(m_Height),
            false, 1,
            bgfx::TextureFormat::RGBA8,
            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK,
            nullptr
        );

        // Create framebuffer from texture for blitting
        m_DisplayFramebuffer = bgfx::createFrameBuffer(1, &m_DisplayTexture, false);
    }

    // Update texture with framebuffer data
    const bgfx::Memory* mem = bgfx::copy(m_ColorBuffer.data(), m_Width * m_Height * 4);
    bgfx::updateTexture2D(
        m_DisplayTexture,
        0, 0,  // layer, mip
        0, 0,  // x, y
        static_cast<uint16_t>(m_Width),
        static_cast<uint16_t>(m_Height),
        mem
    );
}

void SoftwareRenderer::Present() {
    if (!m_Initialized) return;

#ifdef SOLSTICE_DEVELOPMENT_BUILD
        bgfx::dbgTextClear();
        // BGFX debug text uses character grid coordinates (not pixels)
        // Get the text grid dimensions from stats to calculate bottom row
        const bgfx::Stats* stats = bgfx::getStats();
        uint16_t bottomRow = static_cast<uint16_t>(stats->textHeight > 0 ? stats->textHeight - 1 : 29);
        // Format: "Solstice v<version> (Build <major>.<minor>.<patch>-<git_commit>)"
        std::string versionStr = "Development Build (Solstice v" + std::string(SOLSTICE_VERSION) +
                                 " Build " + std::string(SOLSTICE_BUILD_NUM_MAJOR) + "." +
                                 std::string(SOLSTICE_BUILD_NUM_MINOR) + "." +
                                 std::string(SOLSTICE_BUILD_NUM_PATCH) + "-" +
                                 std::string(SOLSTICE_BUILD_GIT_COMMIT) + ")";
        bgfx::dbgTextPrintf(0, bottomRow, 0x0f, versionStr.c_str());
#endif

    bgfx::frame();
}

void SoftwareRenderer::RenderScene(Scene& SceneGraph, const Camera& Cam) {
    if (!m_Initialized) return;
    auto t0 = std::chrono::high_resolution_clock::now();

    // Optional: sync physics to scene before building view/culling
    if (m_PhysicsRegistry) {
        SyncPhysicsToScene(*m_PhysicsRegistry, SceneGraph);
    }

    // Reset per-frame allocator (optimized: reuse buffer, just reset offset)
    m_FrameAllocator.Reset();

    // Clear any pending jobs from previous frame
    WaitForJobs();

    // --- SHADOW PASS ---
    m_MeshLibrary = SceneGraph.GetMeshLibrary();
    if (!m_MeshLibrary) return;

    // Ensure PostProcessing has light direction set (use default if not set)
    if (m_PostProcessing) {
        // If light direction wasn't set via RenderScene(..., Lights), use default
        // This handles the case where RenderScene(SceneGraph, Cam) is called directly
        Math::Vec3 defaultLightDir = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized();
        m_PostProcessing->SetLightDirection(defaultLightDir);
    }

    // Render shadow map using ShadowRenderer
    if (m_ShadowRenderer) {
        m_ShadowRenderer->RenderShadowMap(SceneGraph, Cam, m_MeshLibrary,
                                         m_OptimizeStaticBuffers, m_Stats.VisibleObjects);
    }

    // --- SCENE PASS ---
    // Render scene using SceneRenderer
    // If lights weren't set via RenderScene(..., Lights), SceneRenderer will use fallback
    uint32_t TotalTriangles = 0;
    if (m_SceneRenderer) {
        m_SceneRenderer->SetWireframe(m_WireframeEnabled);
    m_SceneRenderer->SetSelectedObjects(m_SelectedObjects);
    m_SceneRenderer->SetHoveredObject(m_HoveredObject);

    // Multi-viewport support: render to active viewport
    if (!m_Viewports.empty() && m_ActiveViewport < m_Viewports.size()) {
        const auto& vp = m_Viewports[m_ActiveViewport];
        if (vp.Active) {
            bgfx::setViewRect(PostProcessing::VIEW_SCENE,
                            static_cast<uint16_t>(vp.X),
                            static_cast<uint16_t>(vp.Y),
                            static_cast<uint16_t>(vp.Width),
                            static_cast<uint16_t>(vp.Height));
        }
    }

    m_SceneRenderer->RenderScene(SceneGraph, Cam, m_MeshLibrary,
                                SceneGraph.GetMaterialLibrary(), TotalTriangles);
    }

    m_Stats.TrianglesSubmitted = TotalTriangles;
    m_Stats.TrianglesRendered = TotalTriangles;


    // --- POST PROCESS PASS ---
    m_PostProcessing->EndScenePass();
    m_PostProcessing->Apply(0); // Blit to backbuffer (View 0)
    auto t1 = std::chrono::high_resolution_clock::now();
    m_Stats.TotalTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    // UI needs to be drawn AFTER PostProcess (View 0).
    // Need to set view order.
    // Default is 0, 1, 2...
    // We want 1->2->0->3->255 (ImGui)
    bgfx::ViewId viewOrder[] = {
        PostProcessing::VIEW_SHADOW,
        PostProcessing::VIEW_SCENE,
        0,
        3,
        Solstice::UI::UISystem::Instance().GetViewId()
    };
    bgfx::setViewOrder(0, 5, viewOrder);

    // Simple UI crosshair overlay submitted on view 3 (UI)
    // DISABLED: HUD has its own crosshair
    if (false && m_ShowCrosshair && bgfx::isValid(m_UIProgram)) {
        bgfx::ViewId UI_VIEW = 3;
        bgfx::setViewRect(UI_VIEW, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
        float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        bgfx::setViewTransform(UI_VIEW, ident, ident);

        // Don't clear, draw on top
        bgfx::setViewClear(UI_VIEW, BGFX_CLEAR_NONE);

        float px = 8.0f; // crosshair half-size in pixels
        float sx = (px) * 2.0f / (float)m_Width;   // convert to NDC width
        float sy = (px) * 2.0f / (float)m_Height;  // convert to NDC height
        struct V { float x,y,z; };
        V verts[4] = {
            {-sx, -sy, 0.0f},
            { sx, -sy, 0.0f},
            {-sx,  sy, 0.0f},
            { sx,  sy, 0.0f},
        };
        uint16_t indices[6] = {0,1,2,2,1,3};
        if (bgfx::getAvailTransientVertexBuffer(4, m_UILayout) == 4 && bgfx::getAvailTransientIndexBuffer(6) == 6) {
            bgfx::TransientVertexBuffer tvb; bgfx::allocTransientVertexBuffer(&tvb, 4, m_UILayout);
            std::memcpy(tvb.data, verts, sizeof(verts));
            bgfx::TransientIndexBuffer tib; bgfx::allocTransientIndexBuffer(&tib, 6);
            std::memcpy(tib.data, indices, sizeof(indices));
            // Position already in NDC around (0,0); set identity transform
            bgfx::setTransform(ident);
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA | BGFX_STATE_BLEND_ALPHA;
            bgfx::setState(state);
            bgfx::submit(UI_VIEW, m_UIProgram);
        }
    }
}

// Transform vertices from model space to screen space
void SoftwareRenderer::TransformVertices(const std::vector<QuantizedVertex>& Vertices, const Math::Matrix4& MVP) {
    m_TransformedVerts.resize(Vertices.size());

    for (size_t i = 0; i < Vertices.size(); ++i) {
        const auto& v = Vertices[i];

        // Dequantize position (now just direct float copy)
        Math::Vec4 pos(
            v.PosX,
            v.PosY,
            v.PosZ,
            1.0f
        );

        // Transform to clip space
        Math::Vec4 clip = MVP * pos;

        // Perspective divide
        if (std::abs(clip.w) > 0.0001f) {
            clip.x /= clip.w;
            clip.y /= clip.w;
            clip.z /= clip.w;
        }

        // NDC to screen space
        m_TransformedVerts[i].ScreenPos = Math::Vec3(
            (clip.x + 1.0f) * 0.5f * (float)m_Width,
            (1.0f - clip.y) * 0.5f * (float)m_Height,
            clip.z
        );

        // Transform normals and UVs
        m_TransformedVerts[i].Normal = Math::Vec3(v.NormalX, v.NormalY, v.NormalZ);
        m_TransformedVerts[i].UV = Math::Vec2(v.U, v.V);
    }
}

// Bin triangles into tiles
void SoftwareRenderer::BinTriangles(const std::vector<uint32_t>& Indices) {
    for (size_t i = 0; i < Indices.size(); i += 3) {
        uint32_t i0 = Indices[i];
        uint32_t i1 = Indices[i + 1];
        uint32_t i2 = Indices[i + 2];

        if (i0 >= m_TransformedVerts.size() ||
            i1 >= m_TransformedVerts.size() ||
            i2 >= m_TransformedVerts.size()) {
            continue;
        }

        const Math::Vec3& v0 = m_TransformedVerts[i0].ScreenPos;
        const Math::Vec3& v1 = m_TransformedVerts[i1].ScreenPos;
        const Math::Vec3& v2 = m_TransformedVerts[i2].ScreenPos;

        // Calculate bounding box
        float minX = std::min({v0.x, v1.x, v2.x});
        float maxX = std::max({v0.x, v1.x, v2.x});
        float minY = std::min({v0.y, v1.y, v2.y});
        float maxY = std::max({v0.y, v1.y, v2.y});

        // Clip to screen bounds
        minX = std::max(0.0f, minX);
        maxX = std::min(static_cast<float>(m_Width - 1), maxX);
        minY = std::max(0.0f, minY);
        maxY = std::min(static_cast<float>(m_Height - 1), maxY);

        // Calculate tile range
        int minTileX = static_cast<int>(minX) / TILE_SIZE;
        int maxTileX = static_cast<int>(maxX) / TILE_SIZE;
        int minTileY = static_cast<int>(minY) / TILE_SIZE;
        int maxTileY = static_cast<int>(maxY) / TILE_SIZE;

        // Add triangle to overlapping tiles
        uint32_t triIndex = static_cast<uint32_t>(i);
        for (int ty = minTileY; ty <= maxTileY; ++ty) {
            for (int tx = minTileX; tx <= maxTileX; ++tx) {
                int tileIdx = ty * m_TilesX + tx;
                if (tileIdx < static_cast<int>(m_TileBins.size())) {
                    m_TileBins[tileIdx].push_back(triIndex);
                }
            }
        }
    }
}

// Edge function for triangle rasterization
float SoftwareRenderer::EdgeFunction(const Math::Vec2& a, const Math::Vec2& b, const Math::Vec2& c) const {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// Rasterize all tiles
void SoftwareRenderer::RasterizeTiles() {
    // Determine if we should use async based on workload
    uint32_t totalTriangles = 0;
    for (const auto& bin : m_TileBins) {
        totalTriangles += static_cast<uint32_t>(bin.size());
    }

    bool useAsync = m_EnableAsync && (totalTriangles > ASYNC_THRESHOLD_TRIANGLES);

    if (useAsync) {
        // Async + OpenMP parallelization
        #pragma omp parallel for schedule(dynamic) if(totalTriangles > ASYNC_THRESHOLD_TRIANGLES)
        for (int ty = 0; ty < m_TilesY; ++ty) {
            for (int tx = 0; tx < m_TilesX; ++tx) {
                int tileIdx = ty * m_TilesX + tx;
                const auto& triangles = m_TileBins[tileIdx];

                for (uint32_t triIdx : triangles) {
                    RasterizeTriangle(triIdx, {}, tx, ty);
                }
            }
        }
    } else {
        // Synchronous fallback for simple scenes
        for (int ty = 0; ty < m_TilesY; ++ty) {
            for (int tx = 0; tx < m_TilesX; ++tx) {
                int tileIdx = ty * m_TilesX + tx;
                const auto& triangles = m_TileBins[tileIdx];

                for (uint32_t triIdx : triangles) {
                    RasterizeTriangle(triIdx, {}, tx, ty);
                }
            }
        }
    }
}

// Rasterize a single triangle within a tile
void SoftwareRenderer::RasterizeTriangle(uint32_t TriIndex, const std::vector<uint32_t>&, int TileX, int TileY) {
    // TriIndex is actually the base index into m_TransformedVerts
    uint32_t i0 = TriIndex;
    uint32_t i1 = TriIndex + 1;
    uint32_t i2 = TriIndex + 2;

    if (i2 >= m_TransformedVerts.size()) return;

    const Math::Vec3& v0 = m_TransformedVerts[i0].ScreenPos;
    const Math::Vec3& v1 = m_TransformedVerts[i1].ScreenPos;
    const Math::Vec3& v2 = m_TransformedVerts[i2].ScreenPos;

    // Calculate tile bounds
    int tileMinX = TileX * TILE_SIZE;
    int tileMinY = TileY * TILE_SIZE;
    int tileMaxX = std::min(tileMinX + TILE_SIZE, m_Width);
    int tileMaxY = std::min(tileMinY + TILE_SIZE, m_Height);

    // Rasterize pixels in tile
    for (int y = tileMinY; y < tileMaxY; ++y) {
        for (int x = tileMinX; x < tileMaxX; ++x) {
            Math::Vec2 p(x + 0.5f, y + 0.5f);  // Pixel center

            // Edge function test
            float w0 = EdgeFunction(Math::Vec2(v1.x, v1.y), Math::Vec2(v2.x, v2.y), p);
            float w1 = EdgeFunction(Math::Vec2(v2.x, v2.y), Math::Vec2(v0.x, v0.y), p);
            float w2 = EdgeFunction(Math::Vec2(v0.x, v0.y), Math::Vec2(v1.x, v1.y), p);

            // Inside triangle if all edge functions have same sign
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                // Barycentric coordinates
                float area = w0 + w1 + w2;
                if (area > 0.0001f) {
                    float lambda0 = w0 / area;
                    float lambda1 = w1 / area;
                    float lambda2 = w2 / area;

                    // Interpolate depth
                    float depth = lambda0 * v0.z + lambda1 * v1.z + lambda2 * v2.z;

                    // Depth test
                    int pixelIdx = y * m_Width + x;
                    if (depth < m_DepthBuffer[pixelIdx]) {
                        m_DepthBuffer[pixelIdx] = depth;

                        // Simple flat shading (red color for now)
                        m_ColorBuffer[pixelIdx] = 0xFF2020FF;  // Red
                    }
                }
            }
        }
    }
}

void SoftwareRenderer::WaitForJobs() {
    // Optimized: check ready status first, only wait for jobs that aren't ready
    for (auto& future : m_RenderJobs) {
        if (future.valid()) {
            // Use wait_for with zero timeout to check if ready without blocking
            auto status = future.wait_for(std::chrono::seconds(0));
            if (status != std::future_status::ready) {
                // Only wait if not ready
                future.wait();
            }
        }
    }
    m_RenderJobs.clear();
}

void SoftwareRenderer::SubmitTileRasterJob(int TileX, int TileY, const std::vector<uint32_t>& Indices) {
    if (!m_EnableAsync) {
        // Synchronous fallback
        int tileIdx = TileY * m_TilesX + TileX;
        for (uint32_t triIdx : m_TileBins[tileIdx]) {
            RasterizeTriangle(triIdx, Indices, TileX, TileY);
        }
        return;
    }

    // Submit async job
    auto future = Core::JobSystem::Instance().SubmitAsync([this, TileX, TileY, &Indices]() {
        int tileIdx = TileY * m_TilesX + TileX;
        const auto& triangles = m_TileBins[tileIdx];
        for (uint32_t triIdx : triangles) {
            RasterizeTriangle(triIdx, Indices, TileX, TileY);
        }
    });

    m_RenderJobs.push_back(std::move(future));
}

void SoftwareRenderer::RenderPhysicsDebug(const void* PhysicsSystemPtr) {
    if (!m_ShowPhysicsDebug || !PhysicsSystemPtr) return;

    // Cast to PhysicsSystem
    const Physics::PhysicsSystem* physicsSystem = static_cast<const Physics::PhysicsSystem*>(PhysicsSystemPtr);
    if (!physicsSystem) return;

    // Get ReactPhysics3D bridge and physics world
    Physics::ReactPhysics3DBridge& bridge = const_cast<Physics::PhysicsSystem*>(physicsSystem)->GetBridge();
    reactphysics3d::PhysicsWorld* physicsWorld = bridge.GetPhysicsWorld();
    if (!physicsWorld) return;

    // Enable debug rendering
    physicsWorld->setIsDebugRenderingEnabled(true);

    // Get debug renderer
    reactphysics3d::DebugRenderer& debugRenderer = physicsWorld->getDebugRenderer();

    // Configure debug items
    debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
    debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
    debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_POINT, true);
    debugRenderer.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_NORMAL, true);

    // Compute debug rendering primitives
    debugRenderer.reset();
    debugRenderer.computeDebugRenderingPrimitives(*physicsWorld);

    // Initialize debug shader if not already done
    if (!bgfx::isValid(m_DebugProgram)) {
        bgfx::ShaderHandle vsh = LoadShader("vs_debug.bin");
        bgfx::ShaderHandle fsh = LoadShader("fs_debug.bin");
        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            m_DebugProgram = bgfx::createProgram(vsh, fsh, true);
            m_DebugLayout
                .begin(bgfx::getRendererType())
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
                .end();
        } else {
            SIMPLE_LOG("WARNING: Failed to load debug shaders");
            return;
        }
    }

    if (!bgfx::isValid(m_DebugProgram)) return;

    // Render lines
    const uint32_t nbLines = debugRenderer.getNbLines();
    if (nbLines > 0) {
        const reactphysics3d::DebugRenderer::DebugLine* lines = debugRenderer.getLinesArray();

        struct DebugVertex {
            float x, y, z;
            uint8_t r, g, b, a;
        };

        std::vector<DebugVertex> vertices;
        vertices.reserve(nbLines * 2);

        for (uint32_t i = 0; i < nbLines; ++i) {
            const auto& line = lines[i];
            vertices.push_back({
                line.point1.x, line.point1.y, line.point1.z,
                static_cast<uint8_t>((line.color1 >> 16) & 0xFF),
                static_cast<uint8_t>((line.color1 >> 8) & 0xFF),
                static_cast<uint8_t>(line.color1 & 0xFF),
                255
            });
            vertices.push_back({
                line.point2.x, line.point2.y, line.point2.z,
                static_cast<uint8_t>((line.color2 >> 16) & 0xFF),
                static_cast<uint8_t>((line.color2 >> 8) & 0xFF),
                static_cast<uint8_t>(line.color2 & 0xFF),
                255
            });
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, static_cast<uint32_t>(vertices.size()), m_DebugLayout);
        if (tvb.data != nullptr) {
            std::memcpy(tvb.data, vertices.data(), vertices.size() * sizeof(DebugVertex));
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_PT_LINES);
            bgfx::submit(0, m_DebugProgram);
        }
    }

    // Render triangles
    const uint32_t nbTriangles = debugRenderer.getNbTriangles();
    if (nbTriangles > 0) {
        const reactphysics3d::DebugRenderer::DebugTriangle* triangles = debugRenderer.getTrianglesArray();

        struct DebugVertex {
            float x, y, z;
            uint8_t r, g, b, a;
        };

        std::vector<DebugVertex> vertices;
        vertices.reserve(nbTriangles * 3);

        for (uint32_t i = 0; i < nbTriangles; ++i) {
            const auto& tri = triangles[i];
            vertices.push_back({
                tri.point1.x, tri.point1.y, tri.point1.z,
                static_cast<uint8_t>((tri.color1 >> 16) & 0xFF),
                static_cast<uint8_t>((tri.color1 >> 8) & 0xFF),
                static_cast<uint8_t>(tri.color1 & 0xFF),
                255
            });
            vertices.push_back({
                tri.point2.x, tri.point2.y, tri.point2.z,
                static_cast<uint8_t>((tri.color2 >> 16) & 0xFF),
                static_cast<uint8_t>((tri.color2 >> 8) & 0xFF),
                static_cast<uint8_t>(tri.color2 & 0xFF),
                255
            });
            vertices.push_back({
                tri.point3.x, tri.point3.y, tri.point3.z,
                static_cast<uint8_t>((tri.color3 >> 16) & 0xFF),
                static_cast<uint8_t>((tri.color3 >> 8) & 0xFF),
                static_cast<uint8_t>(tri.color3 & 0xFF),
                255
            });
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, static_cast<uint32_t>(vertices.size()), m_DebugLayout);
        if (tvb.data != nullptr) {
            std::memcpy(tvb.data, vertices.data(), vertices.size() * sizeof(DebugVertex));
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_CULL_CW);
            bgfx::submit(0, m_DebugProgram);
        }
    }
}

bool SoftwareRenderer::ShouldUseAsync(uint32_t ObjectCount, uint32_t TriangleCount) const {
    if (!m_EnableAsync) return false;
    return (ObjectCount >= ASYNC_THRESHOLD_OBJECTS) || (TriangleCount >= ASYNC_THRESHOLD_TRIANGLES);
}

bool SoftwareRenderer::ShouldUseSIMD(uint32_t VertexCount) const {
    if (!m_UseSIMD) return false;
    return VertexCount >= SIMD_THRESHOLD_VERTICES;
}

void SoftwareRenderer::SetSelectedObjects(const std::set<SceneObjectID>& objects) {
    m_SelectedObjects = objects;
    // Outline rendering is handled by SceneRenderer
}

void SoftwareRenderer::SetHoveredObject(SceneObjectID objectID) {
    m_HoveredObject = objectID;
    // Outline rendering is handled by SceneRenderer
}

void SoftwareRenderer::SetSkybox(Skybox* skybox) {
    m_Skybox = skybox;
    // Update SceneRenderer with the new skybox
    if (m_SceneRenderer && m_PostProcessing && bgfx::isValid(m_CubeProgram)) {
        m_SceneRenderer->Initialize(m_CubeProgram, m_CubeLayout, m_PostProcessing.get(),
                                    &m_TextureRegistry, m_Skybox, m_SkyboxProgram, m_Width, m_Height);
    }
}

void SoftwareRenderer::SetWireframe(bool Enable) {
    m_WireframeEnabled = Enable;
    if (m_SceneRenderer) {
        m_SceneRenderer->SetWireframe(Enable);
    }
}

void SoftwareRenderer::SetShowDebugOverlay(bool Enable) {
    m_ShowDebugOverlay = Enable;
    bgfx::setDebug(Enable ? BGFX_DEBUG_TEXT : 0);
    if (m_SceneRenderer) {
        m_SceneRenderer->SetShowDebugOverlay(Enable);
    }
}

void SoftwareRenderer::RenderScene(Scene& SceneGraph, const Camera& Cam, const std::vector<Physics::LightSource>& Lights) {
    if (!m_Initialized) return;

    // Extract primary light direction and pass to PostProcessing and SceneRenderer
    if (!Lights.empty()) {
        const auto& primaryLight = Lights[0];
        Math::Vec3 lightDir = primaryLight.Position.Normalized();
        if (m_PostProcessing) {
            m_PostProcessing->SetLightDirection(lightDir);
        }
        if (m_SceneRenderer) {
            m_SceneRenderer->SetLightSources(Lights);
        }
    } else {
        // Fallback to default sun light
        Math::Vec3 defaultLightDir = Math::Vec3(0.5f, 1.0f, -0.5f).Normalized();
        if (m_PostProcessing) {
            m_PostProcessing->SetLightDirection(defaultLightDir);
        }
        if (m_SceneRenderer) {
            m_SceneRenderer->SetLightSources(std::vector<Physics::LightSource>());
        }
    }

    // Update raytracing asynchronously for better lighting
    if (m_Raytracing) {
        // Build/update voxel grid periodically (every 60 frames)
        static int frameCount = 0;
        if (frameCount % 60 == 0) {
            m_Raytracing->BuildVoxelGrid(SceneGraph);
        }
        frameCount++;

        // Use provided lights, or fallback to default sun light
        std::vector<Physics::LightSource> lightsToUse = Lights;
        if (lightsToUse.empty()) {
            // Fallback to default sun light
            Physics::LightSource sunLight(
                Math::Vec3(0.5f, 1.0f, -0.5f).Normalized(), // Direction
                Math::Vec3(1.0f, 0.95f, 0.9f), // Color
                1.5f, // Intensity
                0.0f, // Hue
                0.001f // Attenuation (very low for sun)
            );
            lightsToUse.push_back(sunLight);
        }

        // Update raytracing asynchronously (non-blocking)
        m_Raytracing->UpdateAsync(lightsToUse, SceneGraph);
        m_Raytracing->UpdateAsync(); // Clean up completed jobs
        m_Raytracing->UpdateUniforms();

        // Pass raytracing textures to post-processing
        if (bgfx::isValid(m_Raytracing->GetShadowTexture())) {
            m_PostProcessing->SetRaytracingTextures(
                m_Raytracing->GetShadowTexture(),
                m_Raytracing->GetAOTexture()
            );
        }
    }

    // Render the scene normally
    RenderScene(SceneGraph, Cam);
}

// VR stereo rendering
void SoftwareRenderer::RenderSceneVR(Scene& SceneGraph, const Camera& Cam, bool LeftEye) {
    if (!m_Initialized || !Cam.IsVR()) return;

    // Use VR-specific view and projection matrices
    Math::Matrix4 view = Cam.GetViewMatrixVR(LeftEye);
    Math::Matrix4 proj = Cam.GetProjectionMatrixVR(LeftEye, static_cast<float>(m_Width) / static_cast<float>(m_Height));

    // Render with VR camera setup
    // This is a simplified version - full implementation would handle stereo rendering
    RenderScene(SceneGraph, Cam);
}

// Multi-viewport support
void SoftwareRenderer::SetViewportCount(uint32_t Count) {
    m_Viewports.resize(Count);
    // Initialize viewports to cover full screen
    for (uint32_t i = 0; i < Count; ++i) {
        m_Viewports[i].X = 0;
        m_Viewports[i].Y = 0;
        m_Viewports[i].Width = static_cast<uint32_t>(m_Width);
        m_Viewports[i].Height = static_cast<uint32_t>(m_Height);
        m_Viewports[i].Active = true;
    }
    if (m_ActiveViewport >= Count) {
        m_ActiveViewport = 0;
    }
}

void SoftwareRenderer::SetViewport(uint32_t Index, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height) {
    if (Index >= m_Viewports.size()) {
        SetViewportCount(Index + 1);
    }
    m_Viewports[Index].X = X;
    m_Viewports[Index].Y = Y;
    m_Viewports[Index].Width = Width;
    m_Viewports[Index].Height = Height;
    m_Viewports[Index].Active = true;
}

void SoftwareRenderer::GetViewport(uint32_t Index, uint32_t& X, uint32_t& Y, uint32_t& Width, uint32_t& Height) const {
    if (Index < m_Viewports.size()) {
        const auto& vp = m_Viewports[Index];
        X = vp.X;
        Y = vp.Y;
        Width = vp.Width;
        Height = vp.Height;
    } else {
        X = Y = Width = Height = 0;
    }
}

} // namespace Solstice::Render
