#include <UI/UISystem.hxx>
#include <Core/Debug.hxx>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

namespace {
    // Utility: build an orthographic projection matrix
    // Similar to bx::mtxOrtho but without the bx dependency
    void mtxOrtho(float* result, float left, float right, float bottom, float top, float zNear, float zFar, float offset, bool homogeneousNdc) {
        const float aa = 2.0f / (right - left);
        const float bb = 2.0f / (top - bottom);
        const float cc = (homogeneousNdc ? 2.0f : 1.0f) / (zFar - zNear);
        const float dd = (left + right) / (left - right);
        const float ee = (top + bottom) / (bottom - top);
        const float ff = homogeneousNdc ? (zNear + zFar) / (zNear - zFar) : zNear / (zNear - zFar);

        std::memset(result, 0, sizeof(float) * 16);
        result[0] = aa;
        result[5] = bb;
        result[10] = cc;
        result[12] = dd + offset;
        result[13] = ee;
        result[14] = ff;
        result[15] = 1.0f;
    }
    
    template<typename T>
    T bxMax(T a, T b) { return a > b ? a : b; }
    
    template<typename T>
    T bxMin(T a, T b) { return a < b ? a : b; }
}

namespace Solstice::UI {

// Helper function to load shader binary
static bgfx::ShaderHandle LoadShader(const std::string& Name) {
    const std::vector<std::string> searchPaths = {
        "source/Shaders/bin/",
        "../source/Shaders/bin/",
        "../../source/Shaders/bin/",
        "../../../source/Shaders/bin/",
        "../../../../source/Shaders/bin/"
    };

    std::ifstream file;
    for (const auto& prefix : searchPaths) {
        std::string path = prefix + Name;
        file.open(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            SIMPLE_LOG("UISystem: Found shader at: " + path);
            break;
        }
    }

    if (!file.is_open()) {
        SIMPLE_LOG("UISystem: Failed to open shader file: " + Name);
        return BGFX_INVALID_HANDLE;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size) + 1);
    file.read(reinterpret_cast<char*>(mem->data), size);
    mem->data[mem->size - 1] = '\0';
    
    return bgfx::createShader(mem);
}

UISystem& UISystem::Instance() {
    static UISystem instance;
    return instance;
}

void UISystem::Initialize(SDL_Window* Window) {
    if (m_Initialized) {
        SIMPLE_LOG("UISystem already initialized!");
        return;
    }
    
    m_Window = Window;
    
    // Get window dimensions
    SDL_GetWindowSize(m_Window, &m_DisplayWidth, &m_DisplayHeight);
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Set display size immediately (important for ImGui 1.92+)
    io.DisplaySize = ImVec2(static_cast<float>(m_DisplayWidth), static_cast<float>(m_DisplayHeight));
    
    // Enable keyboard and gamepad navigation
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Tell ImGui we handle textures (required for ImGui 1.92+ texture system)
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    
    // Setup SDL3 platform backend
    ImGui_ImplSDL3_InitForOther(m_Window);
    
    // Create BGFX rendering resources
    CreateBGFXResources();
    
    m_Initialized = true;
    SIMPLE_LOG("UISystem initialized with BGFX rendering backend");
}

void UISystem::CreateBGFXResources() {
    // Create vertex layout for ImGui (matches ImDrawVert)
    m_Layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
    
    SIMPLE_LOG("UISystem: Vertex layout stride: " + std::to_string(m_Layout.getStride()) + 
               ", sizeof(ImDrawVert): " + std::to_string(sizeof(ImDrawVert)));
    
    // Load shaders
    bgfx::ShaderHandle vsh = LoadShader("vs_imgui.bin");
    bgfx::ShaderHandle fsh = LoadShader("fs_imgui.bin");
    
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        SIMPLE_LOG("UISystem: Failed to load ImGui shaders - UI rendering disabled");
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
        return;
    }
    
    m_Program = bgfx::createProgram(vsh, fsh, true);
    if (!bgfx::isValid(m_Program)) {
        SIMPLE_LOG("UISystem: Failed to create ImGui program");
        return;
    }
    
    // Create texture uniform
    m_TextureUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    
    // Add default font if none added
    ImGuiIO& io = ImGui::GetIO();
    if (io.Fonts->Fonts.empty()) {
        io.Fonts->AddFontDefault();
    }
    
    // Note: Texture creation will happen lazily during render via ProcessTextureUpdates()
    
    SIMPLE_LOG("UISystem: Created BGFX resources successfully");
}

void UISystem::DestroyBGFXResources() {
    if (bgfx::isValid(m_FontTexture)) {
        bgfx::destroy(m_FontTexture);
        m_FontTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_TextureUniform)) {
        bgfx::destroy(m_TextureUniform);
        m_TextureUniform = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_Program)) {
        bgfx::destroy(m_Program);
        m_Program = BGFX_INVALID_HANDLE;
    }
}

void UISystem::Shutdown() {
    if (!m_Initialized) return;
    
    DestroyBGFXResources();
    
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    
    m_Initialized = false;
    m_Window = nullptr;
    SIMPLE_LOG("UISystem shutdown");
}

void UISystem::NewFrame() {
    if (!m_Initialized) return;
    
    // Update display size
    SDL_GetWindowSize(m_Window, &m_DisplayWidth, &m_DisplayHeight);
    
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UISystem::Render() {
    if (!m_Initialized) return;
    
    ImGui::Render();
    RenderDrawData();
}

void UISystem::RenderDrawData() {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !bgfx::isValid(m_Program)) {
        return;
    }
    
    // Process texture requests from ImGui 1.92+
    // This handles lazy font atlas creation
    if (drawData->Textures != nullptr) {
        for (ImTextureData* texData : *drawData->Textures) {
            switch (texData->Status) {
            case ImTextureStatus_WantCreate:
                {
                    int width = texData->Width;
                    int height = texData->Height;
                    int bpp = texData->BytesPerPixel;
                    
                    // Use BGRA8 for 4bpp (DirectX expects this), R8 for single channel
                    bgfx::TextureFormat::Enum format = (bpp == 4) ? bgfx::TextureFormat::BGRA8 : bgfx::TextureFormat::R8;
                    
                    // Create texture without initial data first, then update
                    bgfx::TextureHandle th = bgfx::createTexture2D(
                        static_cast<uint16_t>(width),
                        static_cast<uint16_t>(height),
                        false, 1,
                        format,
                        0 // No flags - writable
                    );
                    bgfx::setName(th, "ImGui Font Atlas");
                    
                    // Update with pixel data
                    bgfx::updateTexture2D(th, 0, 0, 0, 0,
                        static_cast<uint16_t>(width),
                        static_cast<uint16_t>(height),
                        bgfx::copy(texData->GetPixels(), texData->GetSizeInBytes())
                    );
                    
                    // Store in our member if it's the font atlas
                    if (!bgfx::isValid(m_FontTexture)) {
                        m_FontTexture = th;
                    }
                    
                    texData->SetTexID(static_cast<ImTextureID>(th.idx));
                    texData->SetStatus(ImTextureStatus_OK);
                    
                    SIMPLE_LOG("UISystem: Created texture " + std::to_string(width) + "x" + std::to_string(height));
                }
                break;
                
            case ImTextureStatus_WantUpdates:
                {
                    // Handle incremental texture updates
                    bgfx::TextureHandle th;
                    th.idx = static_cast<uint16_t>(texData->GetTexID());
                    
                    if (bgfx::isValid(th)) {
                        for (const ImTextureRect& rect : texData->Updates) {
                            const uint32_t bpp = texData->BytesPerPixel;
                            const bgfx::Memory* pix = bgfx::alloc(rect.h * rect.w * bpp);
                            
                            // Gather pixels from the source (row by row)
                            const uint8_t* src = static_cast<const uint8_t*>(texData->GetPixelsAt(rect.x, rect.y));
                            uint8_t* dst = pix->data;
                            int pitch = texData->GetPitch();
                            for (int row = 0; row < rect.h; ++row) {
                                std::memcpy(dst, src, rect.w * bpp);
                                src += pitch;
                                dst += rect.w * bpp;
                            }
                            
                            bgfx::updateTexture2D(th, 0, 0, rect.x, rect.y, rect.w, rect.h, pix);
                        }
                        texData->SetStatus(ImTextureStatus_OK);
                    }
                }
                break;
                
            case ImTextureStatus_WantDestroy:
                {
                    ImTextureID texId = texData->GetTexID();
                    if (texId != ImTextureID_Invalid) {
                        bgfx::TextureHandle th;
                        th.idx = static_cast<uint16_t>(texId);
                        bgfx::destroy(th);
                    }
                    texData->SetTexID(ImTextureID_Invalid);
                    texData->SetStatus(ImTextureStatus_Destroyed);
                }
                break;
                
            default:
                break;
            }
        }
    }
    
    // Now check if we have a valid font texture
    if (!bgfx::isValid(m_FontTexture)) {
        return;
    }
    
    // Avoid rendering when minimized
    int fbWidth = static_cast<int>(drawData->DisplaySize.x * drawData->FramebufferScale.x);
    int fbHeight = static_cast<int>(drawData->DisplaySize.y * drawData->FramebufferScale.y);
    if (fbWidth <= 0 || fbHeight <= 0) {
        return;
    }
    
    // Setup orthographic projection matrix
    const bgfx::Caps* caps = bgfx::getCaps();
    float ortho[16];
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
    
    mtxOrtho(ortho, L, R, B, T, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);
    
    // Setup view for UI rendering
    bgfx::setViewName(m_ViewId, "ImGui");
    bgfx::setViewMode(m_ViewId, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(m_ViewId, 0, 0, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));
    bgfx::setViewTransform(m_ViewId, nullptr, ortho);
    
    const ImVec2 clipOff = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;
    
    // Render command lists
    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        uint32_t numVertices = static_cast<uint32_t>(cmdList->VtxBuffer.Size);
        uint32_t numIndices = static_cast<uint32_t>(cmdList->IdxBuffer.Size);
        
        // Check if we have enough transient buffer space
        if (bgfx::getAvailTransientVertexBuffer(numVertices, m_Layout) < numVertices ||
            bgfx::getAvailTransientIndexBuffer(numIndices, sizeof(ImDrawIdx) == 4) < numIndices) {
            SIMPLE_LOG("UISystem: Not enough transient buffer space");
            break;
        }
        
        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;
        
        bgfx::allocTransientVertexBuffer(&tvb, numVertices, m_Layout);
        bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);
        
        // Copy vertex and index data
        memcpy(tvb.data, cmdList->VtxBuffer.Data, numVertices * sizeof(ImDrawVert));
        memcpy(tib.data, cmdList->IdxBuffer.Data, numIndices * sizeof(ImDrawIdx));
        
        // Process each draw command
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];
            
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
            } else if (pcmd->ElemCount > 0) {
                // Project scissor/clipping rectangles
                ImVec4 clipRect;
                clipRect.x = (pcmd->ClipRect.x - clipOff.x) * clipScale.x;
                clipRect.y = (pcmd->ClipRect.y - clipOff.y) * clipScale.y;
                clipRect.z = (pcmd->ClipRect.z - clipOff.x) * clipScale.x;
                clipRect.w = (pcmd->ClipRect.w - clipOff.y) * clipScale.y;
                
                if (clipRect.x < fbWidth && clipRect.y < fbHeight && 
                    clipRect.z >= 0.0f && clipRect.w >= 0.0f) {
                    
                    uint16_t xx = static_cast<uint16_t>(bxMax(clipRect.x, 0.0f));
                    uint16_t yy = static_cast<uint16_t>(bxMax(clipRect.y, 0.0f));
                    uint16_t ww = static_cast<uint16_t>(bxMin(clipRect.z, 65535.0f) - xx);
                    uint16_t hh = static_cast<uint16_t>(bxMin(clipRect.w, 65535.0f) - yy);
                    
                    bgfx::setScissor(xx, yy, ww, hh);
                    
                    // Set render state (alpha blending)
                    uint64_t state = 0
                        | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A
                        | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA)
                        | BGFX_STATE_MSAA;
                    
                    bgfx::setState(state);
                    
                    // Set texture
                    bgfx::TextureHandle th = m_FontTexture;
                    ImTextureID texId = pcmd->GetTexID();
                    if (texId != ImTextureID_Invalid) {
                        th.idx = static_cast<uint16_t>(texId);
                    }
                    bgfx::setTexture(0, m_TextureUniform, th);
                    
                    // Set buffers and submit
                    bgfx::setVertexBuffer(0, &tvb, pcmd->VtxOffset, numVertices);
                    bgfx::setIndexBuffer(&tib, pcmd->IdxOffset, pcmd->ElemCount);
                    bgfx::submit(m_ViewId, m_Program);
                }
            }
        }
    }
}

bool UISystem::ProcessEvent(const SDL_Event* Event) {
    if (!m_Initialized) return false;
    
    return ImGui_ImplSDL3_ProcessEvent(Event);
}

bool UISystem::WantCaptureMouse() const {
    if (!m_Initialized) return false;
    
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse;
}

bool UISystem::WantCaptureKeyboard() const {
    if (!m_Initialized) return false;
    
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}

void* UISystem::GetImGuiContext() const {
    return ImGui::GetCurrentContext();
}

} // namespace Solstice::UI
