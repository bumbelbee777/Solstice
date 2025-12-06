#include <Render/SoftwareRenderer.hxx>
#include <Render/Mesh.hxx>
#include <Math/Vector.hxx>
#include <Core/Debug.hxx>
#include <Core/SIMD.hxx>
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

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Solstice::Render {
    namespace Math = Solstice::Math;

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
    SIMPLE_LOG("SoftwareRenderer: Initializing CPU rasterizer...");
    
    // Calculate tile grid dimensions
    m_TilesX = (m_Width + TILE_SIZE - 1) / TILE_SIZE;
    m_TilesY = (m_Height + TILE_SIZE - 1) / TILE_SIZE;
    
    // Allocate framebuffers
    AllocateFramebuffers();
    
    // Initialize BGFX for display only
    InitializeBGFX(Window);
    
    SIMPLE_LOG("SoftwareRenderer: Initialized " + std::to_string(m_TilesX) + "x" + std::to_string(m_TilesY) + " tiles");
}

SoftwareRenderer::~SoftwareRenderer() {
    if (bgfx::isValid(m_DisplayFramebuffer)) {
        bgfx::destroy(m_DisplayFramebuffer);
    }
    if (bgfx::isValid(m_DisplayTexture)) {
        bgfx::destroy(m_DisplayTexture);
    }
    if (bgfx::isValid(m_BlitProgram)) {
        bgfx::destroy(m_BlitProgram);
    }
    
    // Shutdown BGFX if we initialized it
    // Note: This assumes we're the only one using BGFX
    // In a real app, you'd track who initialized it
    bgfx::shutdown();
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
        SIMPLE_LOG("Initializing BGFX...");
        
        // Create custom callback for error logging
        static BgfxCallback callback;
        
        // Get native window handle from SDL
        bgfx::PlatformData pd;
        memset(&pd, 0, sizeof(pd));
        
#if defined(_WIN32)
        // On Windows, get the HWND from SDL
        SIMPLE_LOG("Getting native window handle from SDL...");
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (!pd.nwh) {
            SIMPLE_LOG("ERROR: Failed to get native window handle from SDL");
            throw std::runtime_error("Failed to get native window handle");
        }
        SIMPLE_LOG("Got window handle: " + std::to_string(reinterpret_cast<uintptr_t>(pd.nwh)));
#elif defined(__APPLE__)
        pd.nwh = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif defined(__linux__)
        pd.ndt = SDL_GetPointerProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
        pd.nwh = (void*)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(Window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
#endif
        
        SIMPLE_LOG("Configuring BGFX init parameters...");
        
        // List of renderer types to try in order
        std::vector<bgfx::RendererType::Enum> renderersToTry;
        
#if defined(_WIN32)
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
            init.resolution.reset = BGFX_RESET_VSYNC;
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
        bgfx::reset(m_Width, m_Height, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X16);
    }
    
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(m_Width), static_cast<uint16_t>(m_Height));
    
    // Initialize vertex layout to match QuantizedVertex (16 bytes total)
    m_CubeLayout
        .begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
        
    SIMPLE_LOG("Vertex Layout Stride: " + std::to_string(m_CubeLayout.getStride()));
    SIMPLE_LOG("sizeof(QuantizedVertex): " + std::to_string(sizeof(QuantizedVertex)));
    SIMPLE_LOG(
        std::string("Offsets  ") +
        "pos=" + std::to_string(m_CubeLayout.getOffset(bgfx::Attrib::Position)) +
        " nrm=" + std::to_string(m_CubeLayout.getOffset(bgfx::Attrib::Normal)) +
        " uv0=" + std::to_string(m_CubeLayout.getOffset(bgfx::Attrib::TexCoord0))
    );
    
    if (m_CubeLayout.getStride() != sizeof(QuantizedVertex)) {
        SIMPLE_LOG("CRITICAL ERROR: Vertex layout stride mismatch!" 
            " Layout=" + std::to_string(m_CubeLayout.getStride()) +
            " Struct=" + std::to_string(sizeof(QuantizedVertex)));
        throw std::runtime_error("BGFX vertex layout stride mismatch");
    }
        
    // Load shaders
    SIMPLE_LOG("Loading shaders from source/Shaders/bin/...");
    bgfx::ShaderHandle vsh = LoadShader("vs_cube.bin");
    bgfx::ShaderHandle fsh = LoadShader("fs_cube.bin");
    
    if (!bgfx::isValid(vsh)) {
        SIMPLE_LOG("ERROR: Failed to load vertex shader 'vs_cube.bin'");
        SIMPLE_LOG("  Make sure shaders are compiled to 'source/Shaders/bin/' directory");
        throw std::runtime_error("Failed to load vertex shader - shader binaries missing");
    }
    
    if (!bgfx::isValid(fsh)) {
        SIMPLE_LOG("ERROR: Failed to load fragment shader 'fs_cube.bin'");
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
}

bgfx::ShaderHandle SoftwareRenderer::LoadShader(const std::string& Name) {
    const std::vector<std::string> searchPaths = {
        "source/Shaders/bin/",
        "../source/Shaders/bin/",
        "../../source/Shaders/bin/",
        "../../../source/Shaders/bin/",
        "../../../../source/Shaders/bin/"
    };

    std::string finalPath;
    std::ifstream file;

    for (const auto& prefix : searchPaths) {
        std::string path = prefix + Name;
        file.open(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            finalPath = path;
            SIMPLE_LOG("Found shader at: " + finalPath);
            break;
        }
    }

    if (!file.is_open()) {
        char cwd[1024];
#if defined(_WIN32)
        GetCurrentDirectoryA(sizeof(cwd), cwd);
#else
        getcwd(cwd, sizeof(cwd));
#endif
        SIMPLE_LOG("Failed to open shader file: " + Name);
        SIMPLE_LOG("  Current Working Directory: " + std::string(cwd));
        SIMPLE_LOG("  Tried paths:");
        for (const auto& prefix : searchPaths) {
            SIMPLE_LOG("    " + prefix + Name);
        }
        return BGFX_INVALID_HANDLE;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[mem->size - 1] = '\0';
    
    return bgfx::createShader(mem);
}

void SoftwareRenderer::Clear(const Math::Vec4& Color) {
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
    bgfx::reset(m_Width, m_Height, BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4);
}

void SoftwareRenderer::SetTileSize(int Size) {
    // No-op - tile size is compile-time constant
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
    bgfx::frame();
}

void SoftwareRenderer::RenderScene(Scene& SceneGraph, const Camera& Cam) {
    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Optional: sync physics to scene before building view/culling
    if (m_PhysicsRegistry) {
        SyncPhysicsToScene(*m_PhysicsRegistry, SceneGraph);
    }

    // Reset per-frame allocator
    m_FrameAllocator.Reset();
    
    // Clear any pending jobs from previous frame
    WaitForJobs();
    
    // Set view 0 default viewport
    bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(m_Width), static_cast<uint16_t>(m_Height));
    // Update debug flags per frame (wireframe overlay when enabled)
    uint32_t debugFlags = BGFX_DEBUG_TEXT;
    if (m_WireframeEnabled) debugFlags |= BGFX_DEBUG_WIREFRAME;
    bgfx::setDebug(debugFlags);
    
    // Build view (row-major) and projection; convert projection to D3D depth [0,1]
    Math::Matrix4 View = Cam.GetViewMatrix();
    // Fallback: if View is identity (camera at origin), push the camera back so origin isn't on the eye
    auto isApproxIdentity = [](const Math::Matrix4& M) {
        const float eps = 1e-6f;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                float expected = (i == j) ? 1.0f : 0.0f;
                if (std::fabs(M.M[i][j] - expected) > eps) return false;
            }
        return true;
    };
    if (isApproxIdentity(View)) {
        SIMPLE_LOG("View was identity; applying fallback LookAt eye=(0,0,2)->(0,0,0)");
        View = Math::Matrix4::LookAt(Math::Vec3(0.0f, 0.0f, 2.0f), Math::Vec3(0.0f, 0.0f, 0.0f), Math::Vec3(0.0f, 1.0f, 0.0f));
    }
    Math::Matrix4 Proj = Math::Matrix4::Perspective(
        Cam.GetZoom() * 0.0174533f,
        static_cast<float>(m_Width) / m_Height,
        0.1f, 1000.0f
    );
    static bool s_loggedOnceVP = false;
    if (!s_loggedOnceVP) {
        SIMPLE_LOG(std::string("VP Setup  fovDeg=") + std::to_string(Cam.GetZoom()) +
                   " aspect=" + std::to_string(static_cast<float>(m_Width) / m_Height) +
                   " near=0.1 far=1000");
        SIMPLE_LOG(std::string("View r0=") +
                   std::to_string(View.M[0][0]) + "," + std::to_string(View.M[0][1]) + "," + std::to_string(View.M[0][2]) + "," + std::to_string(View.M[0][3]));
        SIMPLE_LOG(std::string("View r1=") +
                   std::to_string(View.M[1][0]) + "," + std::to_string(View.M[1][1]) + "," + std::to_string(View.M[1][2]) + "," + std::to_string(View.M[1][3]));
        SIMPLE_LOG(std::string("Proj r0=") +
                   std::to_string(Proj.M[0][0]) + "," + std::to_string(Proj.M[0][1]) + "," + std::to_string(Proj.M[0][2]) + "," + std::to_string(Proj.M[0][3]));
        SIMPLE_LOG(std::string("Proj r1=") +
                   std::to_string(Proj.M[1][0]) + "," + std::to_string(Proj.M[1][1]) + "," + std::to_string(Proj.M[1][2]) + "," + std::to_string(Proj.M[1][3]));
        SIMPLE_LOG(std::string("Proj r2=") +
                   std::to_string(Proj.M[2][0]) + "," + std::to_string(Proj.M[2][1]) + "," + std::to_string(Proj.M[2][2]) + "," + std::to_string(Proj.M[2][3]));
        SIMPLE_LOG(std::string("Proj r3=") +
                   std::to_string(Proj.M[3][0]) + "," + std::to_string(Proj.M[3][1]) + "," + std::to_string(Proj.M[3][2]) + "," + std::to_string(Proj.M[3][3]));
    }
    // Transpose to column-major for BGFX
    Math::Matrix4 ViewT = View.Transposed();
    Math::Matrix4 ProjT = Proj.Transposed();
    bgfx::setViewTransform(0, &ViewT.M[0][0], &ProjT.M[0][0]);
    if (!s_loggedOnceVP) {
        // Log a quick VP*origin test in clip space using CPU side
        Math::Matrix4 VP = Proj * View; // Math uses column vectors: clip = (Proj*View)*vec4
        Math::Vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
        Math::Vec4 clip = VP * origin;
        SIMPLE_LOG(std::string("(Proj*View)*origin clip=") + std::to_string(clip.x) + "," + std::to_string(clip.y) + "," + std::to_string(clip.z) + "," + std::to_string(clip.w));
        s_loggedOnceVP = true;
    }
    
    // Get visible objects
    std::vector<SceneObjectID> VisibleObjects;
    SceneGraph.FrustumCull(Cam, VisibleObjects);
    m_Stats.VisibleObjects = static_cast<uint32_t>(VisibleObjects.size());
    
    m_MeshLibrary = SceneGraph.GetMeshLibrary();
    if (!m_MeshLibrary) return;
    
    uint32_t TotalTriangles = 0;
    
    // Process each visible object
    for (SceneObjectID ObjID : VisibleObjects) {
        uint32_t MeshID = SceneGraph.GetMeshID(ObjID);
        Mesh* MeshPtr = m_MeshLibrary->GetMesh(MeshID);
        if (!MeshPtr) {
            SIMPLE_LOG("RenderScene: Mesh not found for object " + std::to_string(ObjID));
            continue;
        }
        if (MeshPtr->Vertices.empty()) {
            SIMPLE_LOG("RenderScene: Mesh " + std::to_string(MeshID) + " has 0 vertices!");
            continue;
        }

        const Math::Matrix4& WorldMat = SceneGraph.GetWorldMatrix(ObjID);
        static bool s_loggedOnceWorld = false;
        if (!s_loggedOnceWorld) {
            SIMPLE_LOG(std::string("World r0=") +
                       std::to_string(WorldMat.M[0][0]) + "," + std::to_string(WorldMat.M[0][1]) + "," + std::to_string(WorldMat.M[0][2]) + "," + std::to_string(WorldMat.M[0][3]));
            SIMPLE_LOG(std::string("World r1=") +
                       std::to_string(WorldMat.M[1][0]) + "," + std::to_string(WorldMat.M[1][1]) + "," + std::to_string(WorldMat.M[1][2]) + "," + std::to_string(WorldMat.M[1][3]));
            SIMPLE_LOG(std::string("World r2=") +
                       std::to_string(WorldMat.M[2][0]) + "," + std::to_string(WorldMat.M[2][1]) + "," + std::to_string(WorldMat.M[2][2]) + "," + std::to_string(WorldMat.M[2][3]));
            SIMPLE_LOG(std::string("World r3=") +
                       std::to_string(WorldMat.M[3][0]) + "," + std::to_string(WorldMat.M[3][1]) + "," + std::to_string(WorldMat.M[3][2]) + "," + std::to_string(WorldMat.M[3][3]));
        }
        float model[16];
        // Convert row-major World to column-major for BGFX u_model
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                model[c*4 + r] = WorldMat.M[r][c];
        bgfx::setTransform(model);
        if (!s_loggedOnceWorld) {
            Math::Vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
            Math::Vec4 worldOrigin = WorldMat * origin;
            SIMPLE_LOG(std::string("WorldOrigin=") + std::to_string(worldOrigin.x) + "," + std::to_string(worldOrigin.y) + "," + std::to_string(worldOrigin.z) + "," + std::to_string(worldOrigin.w));
            s_loggedOnceWorld = true;
        }
        
        // Create transient buffers for this frame
        // Note: In a real engine, we would cache these buffers
        uint32_t numVerts = static_cast<uint32_t>(MeshPtr->Vertices.size());
        uint32_t numIndices = static_cast<uint32_t>(MeshPtr->Indices.size());
        
        // SIMPLE_LOG("Checking transient buffer availability for " + std::to_string(numVerts) + " verts, " + std::to_string(numIndices) + " indices");
        
        if (bgfx::getAvailTransientVertexBuffer(numVerts, m_CubeLayout) >= numVerts &&
            bgfx::getAvailTransientIndexBuffer(numIndices, true) >= numIndices) {
            
            // SIMPLE_LOG("Allocating transient vertex buffer...");
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, numVerts, m_CubeLayout);
            
            if (tvb.data == nullptr) {
                SIMPLE_LOG("CRITICAL: tvb.data is NULL!");
            } else {
                // SIMPLE_LOG("tvb.data allocated at " + std::to_string(reinterpret_cast<uintptr_t>(tvb.data)));
                // SIMPLE_LOG("Copying vertex data (" + std::to_string(numVerts * sizeof(QuantizedVertex)) + " bytes)...");
                std::memcpy(tvb.data, MeshPtr->Vertices.data(), numVerts * sizeof(QuantizedVertex));
                // SIMPLE_LOG("Vertex data copied");
            }
            
            // SIMPLE_LOG("Allocating transient index buffer...");
            bgfx::TransientIndexBuffer tib;
            bgfx::allocTransientIndexBuffer(&tib, numIndices, true); // 32-bit indices
            
            if (tib.data == nullptr) {
                SIMPLE_LOG("CRITICAL: tib.data is NULL!");
            } else {
                // SIMPLE_LOG("Copying index data...");
                std::memcpy(tib.data, MeshPtr->Indices.data(), numIndices * sizeof(uint32_t));
                // SIMPLE_LOG("Index data copied");
            }

            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            
            // Set material albedo color uniform
            // Default to gray if no material lib or material
            Math::Vec4 albedoColor(0.7f, 0.7f, 0.7f, 0.5f);
            MaterialLibrary* matLib = SceneGraph.GetMaterialLibrary();
            if (matLib && !MeshPtr->SubMeshes.empty()) {
                uint32_t matID = MeshPtr->SubMeshes[0].MaterialID;
                const auto& materials = matLib->GetMaterials();
                if (matID < materials.size()) {
                    const Material& mat = materials[matID];
                    Math::Vec3 albedo = mat.GetAlbedoColor();
                    float roughness = mat.GetRoughness();
                    albedoColor = Math::Vec4(albedo.x, albedo.y, albedo.z, roughness);
                }
            }
            
            // Create uniform if needed
            static bgfx::UniformHandle u_albedoColor = bgfx::createUniform("u_albedoColor", bgfx::UniformType::Vec4);
            bgfx::setUniform(u_albedoColor, &albedoColor);
            
            uint64_t state = 0
                | BGFX_STATE_WRITE_R
                | BGFX_STATE_WRITE_G
                | BGFX_STATE_WRITE_B
                | BGFX_STATE_WRITE_A
                | BGFX_STATE_WRITE_Z
                | BGFX_STATE_DEPTH_TEST_LESS
                // | BGFX_STATE_CULL_CCW  // Temporarily disabled to debug cylinder
                | BGFX_STATE_MSAA;
            
            // Wireframe handled via bgfx::setDebug(BGFX_DEBUG_WIREFRAME)

            bgfx::setState(state);
            
            bgfx::submit(0, m_CubeProgram);
            
            TotalTriangles += static_cast<uint32_t>(MeshPtr->Indices.size() / 3);
        }
    }
    
    m_Stats.TrianglesSubmitted = TotalTriangles;
    m_Stats.TrianglesRendered = TotalTriangles;
    
    auto t1 = std::chrono::high_resolution_clock::now();
    m_Stats.TotalTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    
    // Simple UI crosshair overlay submitted on view 1 (UI)
    if (m_ShowCrosshair && bgfx::isValid(m_UIProgram)) {
        bgfx::setViewRect(1, 0, 0, (uint16_t)m_Width, (uint16_t)m_Height);
        float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        bgfx::setViewTransform(1, ident, ident);
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
            bgfx::submit(1, m_UIProgram);
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
    for (auto& future : m_RenderJobs) {
        if (future.valid()) {
            future.wait();
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
    
    // TODO: Cast to actual PhysicsSystem type and render debug info
    // For now, this is a placeholder
    // Future: Render AABBs, velocity vectors, collision shapes, etc.
    SIMPLE_LOG("Physics debug rendering - placeholder");
}

bool SoftwareRenderer::ShouldUseAsync(uint32_t ObjectCount, uint32_t TriangleCount) const {
    if (!m_EnableAsync) return false;
    return (ObjectCount >= ASYNC_THRESHOLD_OBJECTS) || (TriangleCount >= ASYNC_THRESHOLD_TRIANGLES);
}

bool SoftwareRenderer::ShouldUseSIMD(uint32_t VertexCount) const {
    if (!m_UseSIMD) return false;
    return VertexCount >= SIMD_THRESHOLD_VERTICES;
}

} // namespace Solstice::Render
