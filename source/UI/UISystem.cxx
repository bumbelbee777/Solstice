#include <UI/UISystem.hxx>
#include <Render/Assets/ShaderLoader.hxx>
#include <Core/Debug.hxx>
#include <Core/ScopeTimer.hxx>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>

#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <cstring>

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

    ImTextureID EncodeImTextureId(bgfx::TextureHandle handle) {
        if (!bgfx::isValid(handle)) {
            return ImTextureID_Invalid;
        }
        return static_cast<ImTextureID>(static_cast<uint64_t>(handle.idx) + 1u);
    }

    bgfx::TextureHandle DecodeImTextureId(ImTextureID id) {
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        if (id == ImTextureID_Invalid) {
            return handle;
        }
        const uint64_t raw = static_cast<uint64_t>(id);
        if (raw == 0 || raw > (static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()) + 1u)) {
            return handle;
        }
        handle.idx = static_cast<uint16_t>(raw - 1u);
        return handle;
    }
}

namespace Solstice::UI {

UISystem& UISystem::Instance() {
    static UISystem instance;
    return instance;
}

void UISystem::Initialize(SDL_Window* Window) {
    if (m_Initialized) {
        SIMPLE_LOG("UISystem already initialized!");
        return;
    }

    if (!Window) {
        throw std::runtime_error("UISystem: Initialize called with null window");
    }
    if (bgfx::getRendererType() == bgfx::RendererType::Noop) {
        throw std::runtime_error("UISystem: BGFX not initialized before UI");
    }

    m_Window = Window;

    // Get window dimensions
    SDL_GetWindowSize(m_Window, &m_DisplayWidth, &m_DisplayHeight);
    if (m_DisplayWidth <= 0 || m_DisplayHeight <= 0) {
        throw std::runtime_error("UISystem: Invalid window size during initialization");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    m_Context = ImGui::GetCurrentContext();
    if (!m_Context) {
        throw std::runtime_error("UISystem: Failed to create ImGui context");
    }
    ImGuiIO& io = ImGui::GetIO();

    // Set display size immediately (important for ImGui 1.92+)
    io.DisplaySize = ImVec2(static_cast<float>(m_DisplayWidth), static_cast<float>(m_DisplayHeight));

    // Enable keyboard and gamepad navigation
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Tell ImGui we handle textures (required for ImGui 1.92+ texture system)
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    // Setup Dear ImGui style - Windows Aero-inspired
    SetupAeroStyle();

    // Setup SDL3 platform backend
    if (!ImGui_ImplSDL3_InitForOther(m_Window)) {
        ImGui::DestroyContext();
        throw std::runtime_error("UISystem: ImGui SDL3 backend initialization failed");
    }

    // Create BGFX rendering resources
    CreateBGFXResources();
    if (!bgfx::isValid(m_Program) || !bgfx::isValid(m_TextureUniform)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("UISystem: BGFX UI resources failed to initialize");
    }
    if (ImGui::GetCurrentContext() != m_Context) {
        ImGui::SetCurrentContext(m_Context);
    }
    if (ImGui::GetCurrentContext() != m_Context) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("UISystem: ImGui context lost during initialization");
    }

    m_Initialized = true;
    SIMPLE_LOG("UISystem: Initialized successfully");
}

void UISystem::CreateBGFXResources() {
    // Create vertex layout for ImGui (matches ImDrawVert)
    m_Layout
        .begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();

    // Load shaders
    bgfx::ShaderHandle vsh = Render::ShaderLoader::LoadShader("vs_imgui.bin");
    bgfx::ShaderHandle fsh = Render::ShaderLoader::LoadShader("fs_imgui.bin");

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
    if (!bgfx::isValid(m_TextureUniform)) {
        SIMPLE_LOG("UISystem: Failed to create texture uniform");
        return;
    }

    // Load high-quality anti-aliased fonts with bold/italic support
    ImGuiIO& io = ImGui::GetIO();

    // Base font size
    const float fontSize = 17.0f;

    // Load regular font
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 3;
    fontConfig.PixelSnapH = false;

    // Try to load Roboto-Medium.ttf from assets/fonts directory
    // Path relative to executable or absolute path
    const char* fontPath = "assets/fonts/Roboto-Medium.ttf";
    m_FontRegular = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &fontConfig);

    if (!m_FontRegular) {
        // Fallback to default font if Roboto not found
        SIMPLE_LOG("UISystem: Could not load Roboto-Medium.ttf, using default font");
        m_FontRegular = io.Fonts->AddFontDefault();
    }

    // Load bold font (using roboto-bold.ttf if available, otherwise use regular with bold flag)
    const char* boldFontPath = "assets/fonts/roboto-bold.ttf";
    fontConfig.FontLoaderFlags = 0; // Reset flags
    m_FontBold = io.Fonts->AddFontFromFileTTF(boldFontPath, fontSize, &fontConfig);

    if (!m_FontBold) {
        // If bold font file not found, try to use regular font with bold styling
        // Note: This requires FreeType for proper bold rendering
        fontConfig.FontLoaderFlags = 0; // Will need ImGuiFreeTypeLoaderFlags_Bold if FreeType enabled
        m_FontBold = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &fontConfig);
        if (m_FontBold) {
            SIMPLE_LOG("UISystem: Using regular font for bold (consider enabling FreeType for better quality)");
        } else {
            m_FontBold = m_FontRegular; // Fallback to regular
        }
    }

    // Load italic font (using regular font with italic styling)
    fontConfig.FontLoaderFlags = 0; // Will need ImGuiFreeTypeLoaderFlags_Oblique if FreeType enabled
    m_FontItalic = io.Fonts->AddFontFromFileTTF(fontPath, fontSize, &fontConfig);
    if (!m_FontItalic) {
        m_FontItalic = m_FontRegular; // Fallback to regular
    }

    // Load bold-italic font
    fontConfig.FontLoaderFlags = 0; // Will need both Bold and Oblique flags if FreeType enabled
    m_FontBoldItalic = io.Fonts->AddFontFromFileTTF(boldFontPath ? boldFontPath : fontPath, fontSize, &fontConfig);
    if (!m_FontBoldItalic) {
        m_FontBoldItalic = m_FontRegular; // Fallback to regular
    }

    // Set default font
    if (m_FontRegular) {
        io.FontDefault = m_FontRegular;
    } else {
        io.Fonts->AddFontDefault();
    }

    // Note: Texture creation will happen lazily during render via ProcessTextureUpdates()
}

void UISystem::SetupAeroStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Base colors - Windows Aero inspired (glassy, premium feel)
    ImVec4* colors = style.Colors;

    // Window colors - deep blue-gray with transparency
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.18f, 0.88f); // Glassy background
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.12f, 0.16f, 0.85f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.14f, 0.18f, 0.95f);

    // Frame colors - buttons, inputs
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.28f, 0.90f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.30f, 0.36f, 0.95f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.37f, 0.43f, 1.0f);

    // Button colors
    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.24f, 0.30f, 0.90f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.32f, 0.34f, 0.40f, 0.95f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.42f, 0.48f, 1.0f);

    // Header colors (collapsible sections)
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.27f, 0.33f, 0.90f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.37f, 0.43f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.45f, 0.47f, 0.53f, 1.0f);

    // Title bar
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.17f, 0.21f, 0.90f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.20f, 0.22f, 0.28f, 0.95f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.12f, 0.14f, 0.18f, 0.85f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.16f, 0.20f, 0.90f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.12f, 0.16f, 0.80f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.32f, 0.38f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.42f, 0.48f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50f, 0.52f, 0.58f, 1.0f);

    // Checkbox, radio, slider
    colors[ImGuiCol_CheckMark] = ImVec4(0.60f, 0.65f, 0.80f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.50f, 0.55f, 0.70f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.60f, 0.65f, 0.80f, 1.0f);

    // Text colors
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.98f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);

    // Border
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.27f, 0.35f, 0.60f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.30f);

    // Separator
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.32f, 0.40f, 0.70f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.40f, 0.42f, 0.50f, 0.80f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.52f, 0.60f, 1.0f);

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.32f, 0.38f, 0.60f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.42f, 0.48f, 0.80f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.52f, 0.58f, 1.0f);

    // Tab colors
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.28f, 0.85f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.32f, 0.38f, 0.95f);
    colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.37f, 0.43f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.17f, 0.23f, 0.80f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.22f, 0.24f, 0.30f, 0.90f);

    // Accent color for selected items (subtle orange/crimson)
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.85f, 0.45f, 0.25f, 1.0f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.85f, 0.45f, 0.25f, 1.0f);

    // Table colors
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18f, 0.20f, 0.26f, 0.90f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30f, 0.32f, 0.40f, 0.80f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.25f, 0.27f, 0.35f, 0.60f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.15f, 0.17f, 0.21f, 0.50f);

    // Drag and drop
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.85f, 0.45f, 0.25f, 0.80f);

    // Navigation highlight
    colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.65f, 0.80f, 1.0f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.60f, 0.65f, 0.80f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.60f);

    // Modal overlay
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.70f);

    // Style properties - rounded corners for glassy feel
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.PopupRounding = 8.0f;

    // Padding and spacing
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 21.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f; // No border on frames for cleaner look
    style.TabBorderSize = 1.0f;

    // Alpha
    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.60f;

    // Window settings
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    // Anti-aliasing
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
    style.AntiAliasedFill = true;

    // Curve tessellation
    style.CurveTessellationTol = 1.25f;
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
    m_Context = nullptr;
}

void UISystem::NewFrame() {
    if (!m_Initialized) return;

    if (!m_Context) {
        throw std::runtime_error("UISystem: ImGui context missing in NewFrame");
    }
    if (ImGui::GetCurrentContext() != m_Context) {
        ImGui::SetCurrentContext(m_Context);
    }

    // Update display size
    SDL_GetWindowSize(m_Window, &m_DisplayWidth, &m_DisplayHeight);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(m_DisplayWidth), static_cast<float>(m_DisplayHeight));

    int fbWidth = 0;
    int fbHeight = 0;
    SDL_GetWindowSizeInPixels(m_Window, &fbWidth, &fbHeight);
    if (m_DisplayWidth > 0 && m_DisplayHeight > 0) {
        io.DisplayFramebufferScale = ImVec2(
            static_cast<float>(fbWidth) / static_cast<float>(m_DisplayWidth),
            static_cast<float>(fbHeight) / static_cast<float>(m_DisplayHeight));
    } else {
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    }

    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void UISystem::Render() {
    PROFILE_SCOPE("UISystem::Render");
    if (!m_Initialized) return;

    if (!m_Context) {
        throw std::runtime_error("UISystem: ImGui context missing in Render");
    }
    if (ImGui::GetCurrentContext() != m_Context) {
        ImGui::SetCurrentContext(m_Context);
    }

    ImGui::Render();
    RenderDrawData(UIRenderSettings::Default());
}

void UISystem::RenderWithSettings(const UIRenderSettings& settings) {
    PROFILE_SCOPE("UISystem::RenderWithSettings");
    if (!m_Initialized) return;

    if (!m_Context) {
        throw std::runtime_error("UISystem: ImGui context missing in RenderWithSettings");
    }
    if (ImGui::GetCurrentContext() != m_Context) {
        ImGui::SetCurrentContext(m_Context);
    }

    ImGui::Render();
    RenderDrawData(settings);
}

void UISystem::RenderDrawData(const UIRenderSettings& settings) {
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !bgfx::isValid(m_Program)) {
        return;
    }
    if (!bgfx::isValid(m_TextureUniform)) {
        SIMPLE_LOG("UISystem: Texture uniform invalid, skipping UI render");
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
                    if (!bgfx::isValid(th)) {
                        SIMPLE_LOG("UISystem: Failed to create ImGui texture");
                        texData->SetTexID(ImTextureID_Invalid);
                        texData->SetStatus(ImTextureStatus_Destroyed);
                        break;
                    }
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

                    texData->SetTexID(EncodeImTextureId(th));
                    texData->SetStatus(ImTextureStatus_OK);
                }
                break;

            case ImTextureStatus_WantUpdates:
                {
                    // Handle incremental texture updates
                    bgfx::TextureHandle th = DecodeImTextureId(texData->GetTexID());

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
                        bgfx::TextureHandle th = DecodeImTextureId(texId);
                        if (bgfx::isValid(th)) {
                            bgfx::destroy(th);
                        }
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

    static bool s_LoggedFirstFrame = false;
    if (!s_LoggedFirstFrame) {
        s_LoggedFirstFrame = true;
        SIMPLE_LOG("UISystem: DrawData lists=" + std::to_string(drawData->CmdListsCount) +
                   " vtx=" + std::to_string(drawData->TotalVtxCount) +
                   " idx=" + std::to_string(drawData->TotalIdxCount));
        SIMPLE_LOG("UISystem: Font texture idx=" + std::to_string(m_FontTexture.idx));
        if (drawData->Textures != nullptr && drawData->Textures->Size > 0 && (*drawData->Textures)[0]) {
            ImTextureID sampleId = (*drawData->Textures)[0]->GetTexID();
            bgfx::TextureHandle decoded = DecodeImTextureId(sampleId);
            SIMPLE_LOG("UISystem: Sample texId raw=" + std::to_string(static_cast<uint64_t>(sampleId)) +
                       " decoded=" + std::to_string(decoded.idx));
        }
    }

    // Setup orthographic projection matrix
    const bgfx::Caps* caps = bgfx::getCaps();
    if (!caps) {
        SIMPLE_LOG("UISystem: BGFX caps unavailable, skipping UI render");
        return;
    }
    float ortho[16];
    float L = drawData->DisplayPos.x;
    float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T = drawData->DisplayPos.y;
    float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

    mtxOrtho(ortho, L, R, B, T, 0.0f, 1000.0f, 0.0f, caps->homogeneousDepth);

    // Use view ID from settings
    bgfx::ViewId viewId = settings.ViewId;

    // Setup view for UI rendering
    bgfx::setViewName(viewId, "ImGui");
    bgfx::setViewMode(viewId, bgfx::ViewMode::Sequential);
    bgfx::setViewRect(viewId, 0, 0, static_cast<uint16_t>(fbWidth), static_cast<uint16_t>(fbHeight));
    bgfx::setViewTransform(viewId, nullptr, ortho);
    bgfx::setViewClear(viewId, BGFX_CLEAR_NONE);

    const ImVec2 clipOff = drawData->DisplayPos;
    const ImVec2 clipScale = drawData->FramebufferScale;

    if (drawData->CmdListsCount < 0) {
        SIMPLE_LOG("UISystem: Invalid draw data (negative CmdListsCount)");
        return;
    }
    if (drawData->CmdListsCount > 1024) {
        SIMPLE_LOG("UISystem: Excessive draw lists, skipping UI frame");
        return;
    }

    const bool useIndex32 = sizeof(ImDrawIdx) == 4;
    const uint32_t totalVertices = static_cast<uint32_t>(drawData->TotalVtxCount);
    const uint32_t totalIndices = static_cast<uint32_t>(drawData->TotalIdxCount);
    if (totalVertices == 0 || totalIndices == 0) {
        return;
    }
    if (totalVertices > 2'000'000 || totalIndices > 3'000'000) {
        SIMPLE_LOG("UISystem: Draw data too large, skipping UI frame");
        return;
    }
    if (bgfx::getAvailTransientVertexBuffer(totalVertices, m_Layout) < totalVertices ||
        bgfx::getAvailTransientIndexBuffer(totalIndices, useIndex32) < totalIndices) {
        static auto lastLogTime = std::chrono::steady_clock::time_point{};
        auto now = std::chrono::steady_clock::now();
        if (now - lastLogTime > std::chrono::seconds(1)) {
            SIMPLE_LOG("UISystem: Not enough transient buffer space (frame skipped)");
            lastLogTime = now;
        }
        return;
    }

    // Render command lists
    for (int n = 0; n < drawData->CmdListsCount; n++) {
        const ImDrawList* cmdList = drawData->CmdLists[n];
        if (!cmdList) {
            SIMPLE_LOG("UISystem: Null ImDrawList encountered");
            continue;
        }
        uint32_t numVertices = static_cast<uint32_t>(cmdList->VtxBuffer.Size);
        uint32_t numIndices = static_cast<uint32_t>(cmdList->IdxBuffer.Size);
        if (numVertices == 0 || numIndices == 0) {
            continue;
        }
        if (numVertices > (std::numeric_limits<uint32_t>::max() / sizeof(ImDrawVert)) ||
            numIndices > (std::numeric_limits<uint32_t>::max() / sizeof(ImDrawIdx))) {
            SIMPLE_LOG("UISystem: ImGui draw data size overflow, skipping list");
            continue;
        }

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;

        bgfx::allocTransientVertexBuffer(&tvb, numVertices, m_Layout);
        bgfx::allocTransientIndexBuffer(&tib, numIndices, useIndex32);

        // Copy vertex and index data
        if (!tvb.data || !tib.data) {
            SIMPLE_LOG("UISystem: Failed to allocate transient buffers");
            return;
        }
        memcpy(tvb.data, cmdList->VtxBuffer.Data, numVertices * sizeof(ImDrawVert));
        memcpy(tib.data, cmdList->IdxBuffer.Data, numIndices * sizeof(ImDrawIdx));

        // Process each draw command
        for (int cmdIdx = 0; cmdIdx < cmdList->CmdBuffer.Size; cmdIdx++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[cmdIdx];

            if (!pcmd) {
                SIMPLE_LOG("UISystem: Null draw command encountered");
                continue;
            }
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmdList, pcmd);
            } else if (pcmd->ElemCount > 0) {
                if (pcmd->VtxOffset > numVertices || pcmd->IdxOffset > numIndices) {
                    SIMPLE_LOG("UISystem: Draw command offset out of range");
                    continue;
                }
                if (pcmd->IdxOffset + pcmd->ElemCount > numIndices) {
                    SIMPLE_LOG("UISystem: Draw command index range out of bounds");
                    continue;
                }
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
                    if (ww == 0 || hh == 0) {
                        continue;
                    }

                    bgfx::setScissor(xx, yy, ww, hh);

                    // Set render state based on settings
                    uint64_t state = 0
                        | BGFX_STATE_WRITE_RGB
                        | BGFX_STATE_WRITE_A;

                    // Configure blending based on settings
                    if (settings.UseBlending) {
                        state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
                    }

                    // Configure depth testing based on settings
                    if (settings.UseDepthTest) {
                        state |= BGFX_STATE_DEPTH_TEST_ALWAYS;
                    }

                    bgfx::setState(state);

                    // Set texture
                    bgfx::TextureHandle th = m_FontTexture;
                    ImTextureID texId = pcmd->GetTexID();
                    if (texId != ImTextureID_Invalid) {
                        bgfx::TextureHandle decoded = DecodeImTextureId(texId);
                        if (bgfx::isValid(decoded)) {
                            th = decoded;
                        }
                    }

                    // Configure texture filtering based on settings
                    uint32_t samplerFlags = 0;
                    if (settings.UsePointFiltering) {
                        samplerFlags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT;
                    }
                    bgfx::setTexture(0, m_TextureUniform, th, samplerFlags);

                    // Set buffers and submit
                    uint32_t drawVertices = numVertices - pcmd->VtxOffset;
                    if (drawVertices == 0) {
                        continue;
                    }
                    bgfx::setVertexBuffer(0, &tvb, pcmd->VtxOffset, drawVertices);
                    bgfx::setIndexBuffer(&tib, pcmd->IdxOffset, pcmd->ElemCount);
                    bgfx::submit(viewId, m_Program);
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
    return m_Context;
}

} // namespace Solstice::UI
