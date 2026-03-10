#include <UI/ViewportUI.hxx>
#include <UI/UISystem.hxx>
#include <Core/Debug.hxx>
#include <imgui.h>
#include <imgui_internal.h>
#include <Render/Post/PostProcessing.hxx>
#include <Render/Assets/ShaderLoader.hxx>
#include <bgfx/bgfx.h>
#include <cmath>

namespace Solstice::UI::ViewportUI {

    // Billboard shader program (loaded once, reused)
    static bgfx::ProgramHandle s_BillboardProgram = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle s_BillboardTextureUniform = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle s_BillboardColorTintUniform = BGFX_INVALID_HANDLE;

    // Initialize billboard shader (call once during initialization)
    static void InitializeBillboardShader() {
        if (bgfx::isValid(s_BillboardProgram)) {
            return; // Already initialized
        }

        bgfx::ShaderHandle vsh = Render::ShaderLoader::LoadShader("vs_billboard.bin");
        bgfx::ShaderHandle fsh = Render::ShaderLoader::LoadShader("fs_billboard.bin");

        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            s_BillboardProgram = bgfx::createProgram(vsh, fsh, true);
            if (bgfx::isValid(s_BillboardProgram)) {
                s_BillboardTextureUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
                s_BillboardColorTintUniform = bgfx::createUniform("u_ColorTint", bgfx::UniformType::Vec4);
            }
        }
    }

    // Check if billboard shader is available
    bool IsBillboardShaderAvailable() {
        InitializeBillboardShader();
        return bgfx::isValid(s_BillboardProgram);
    }

    // Render a billboard quad in world space
    void RenderBillboardQuad(
        const Math::Vec3& WorldPos,
        float Width, float Height,
        bgfx::TextureHandle Texture,
        const Math::Matrix4& ViewMatrix,
        const Math::Matrix4& ProjectionMatrix,
        bgfx::ProgramHandle Program,
        bgfx::ViewId ViewId,
        bool DepthTest,
        const Math::Vec4& ColorTint) {

        // Initialize billboard shader if needed
        InitializeBillboardShader();

        // Use billboard shader if no program provided, otherwise use provided program
        bgfx::ProgramHandle programToUse = Program;
        bool usingBillboardShader = false;
        if (!bgfx::isValid(programToUse)) {
            programToUse = s_BillboardProgram;
            usingBillboardShader = true;
        }

        if (!bgfx::isValid(Texture) || !bgfx::isValid(programToUse)) {
            return;
        }

        // Get camera forward direction from view matrix
        // View matrix transforms world to view space, so the negative Z axis in view space is the camera forward
        // We need the camera's right and up vectors in world space
        Math::Vec3 CameraRight(ViewMatrix.M[0][0], ViewMatrix.M[1][0], ViewMatrix.M[2][0]);
        Math::Vec3 CameraUp(ViewMatrix.M[0][1], ViewMatrix.M[1][1], ViewMatrix.M[2][1]);

        // Create quad vertices in world space (billboard faces camera)
        Math::Vec3 HalfWidth = CameraRight * (Width * 0.5f);
        Math::Vec3 HalfHeight = CameraUp * (Height * 0.5f);

        struct BillboardVertex {
            float x, y, z;
            float u, v;
        };

        BillboardVertex verts[4] = {
            { WorldPos.x - HalfWidth.x - HalfHeight.x, WorldPos.y - HalfWidth.y - HalfHeight.y, WorldPos.z - HalfWidth.z - HalfHeight.z, 0.0f, 0.0f }, // TL
            { WorldPos.x + HalfWidth.x - HalfHeight.x, WorldPos.y + HalfWidth.y - HalfHeight.y, WorldPos.z + HalfWidth.z - HalfHeight.z, 1.0f, 0.0f }, // TR
            { WorldPos.x - HalfWidth.x + HalfHeight.x, WorldPos.y - HalfWidth.y + HalfHeight.y, WorldPos.z - HalfWidth.z + HalfHeight.z, 0.0f, 1.0f }, // BL
            { WorldPos.x + HalfWidth.x + HalfHeight.x, WorldPos.y + HalfWidth.y + HalfHeight.y, WorldPos.z + HalfWidth.z + HalfHeight.z, 1.0f, 1.0f }  // BR
        };

        uint16_t indices[6] = { 0, 1, 2, 2, 1, 3 };

        // Create vertex layout for billboard
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();

        bgfx::TransientVertexBuffer tvb;
        bgfx::TransientIndexBuffer tib;

        if (bgfx::getAvailTransientVertexBuffer(4, layout) >= 4) {
            bgfx::allocTransientVertexBuffer(&tvb, 4, layout);
            std::memcpy(tvb.data, verts, sizeof(verts));
            bgfx::allocTransientIndexBuffer(&tib, 6);
            std::memcpy(tib.data, indices, sizeof(indices));

            // Set view transform
            Math::Matrix4 ViewT = ViewMatrix.Transposed();
            Math::Matrix4 ProjT = ProjectionMatrix.Transposed();
            bgfx::setViewTransform(ViewId, &ViewT.M[0][0], &ProjT.M[0][0]);

            // Set identity transform (vertices are already in world space)
            float identity[16] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            };
            bgfx::setTransform(identity);

            // Bind texture and color tint
            if (usingBillboardShader && bgfx::isValid(s_BillboardTextureUniform)) {
                // Use billboard shader uniforms
                bgfx::setTexture(0, s_BillboardTextureUniform, Texture);
                if (bgfx::isValid(s_BillboardColorTintUniform)) {
                    float colorTintData[4] = { ColorTint.x, ColorTint.y, ColorTint.z, ColorTint.w };
                    bgfx::setUniform(s_BillboardColorTintUniform, colorTintData);
                }
            } else {
                // Fallback: try to use s_texColor uniform (for compatibility with other shaders)
                static bgfx::UniformHandle s_TexFallback = BGFX_INVALID_HANDLE;
                if (!bgfx::isValid(s_TexFallback)) {
                    s_TexFallback = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
                }
                if (bgfx::isValid(s_TexFallback)) {
                    bgfx::setTexture(0, s_TexFallback, Texture);
                }
            }

            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);

            // Set render state
            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
            if (DepthTest) {
                state |= BGFX_STATE_DEPTH_TEST_LESS;
            } else {
                state |= BGFX_STATE_DEPTH_TEST_ALWAYS;
            }
            bgfx::setState(state);

            bgfx::submit(ViewId, programToUse);
        }
    }

    // Utility function to project 3D point to screen space
    Math::Vec2 ProjectToScreen(const Math::Vec3& WorldPos, const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight) {
        // Transform to clip space
        Math::Vec4 clipPos = ProjectionMatrix * (ViewMatrix * Math::Vec4(WorldPos.x, WorldPos.y, WorldPos.z, 1.0f));

        // specific check for points behind camera
        if (clipPos.w <= 0.0f) {
           return Math::Vec2(-10000.0f, -10000.0f);
        }

        // Perspective divide
        if (std::abs(clipPos.w) > 0.0001f) {
            clipPos.x /= clipPos.w;
            clipPos.y /= clipPos.w;
            clipPos.z /= clipPos.w;
        }

        // NDC to screen space
        float screenX = (clipPos.x + 1.0f) * 0.5f * (float)ScreenWidth;
        float screenY = (1.0f - clipPos.y) * 0.5f * (float)ScreenHeight;

        return Math::Vec2(screenX, screenY);
    }

    // Utility function to calculate anchor position
    ::ImVec2 CalculateAnchorPosition(Anchor AnchorPos, float Width, float Height, int ScreenWidth, int ScreenHeight, float OffsetX, float OffsetY) {
        float x = 0.0f;
        float y = 0.0f;

        switch (AnchorPos) {
            case Anchor::TopLeft:
                x = OffsetX;
                y = OffsetY;
                break;
            case Anchor::TopCenter:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = OffsetY;
                break;
            case Anchor::TopRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = OffsetY;
                break;
            case Anchor::CenterLeft:
                x = OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::Center:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::CenterRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = ((float)ScreenHeight - Height) * 0.5f + OffsetY;
                break;
            case Anchor::BottomLeft:
                x = OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
            case Anchor::BottomCenter:
                x = ((float)ScreenWidth - Width) * 0.5f + OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
            case Anchor::BottomRight:
                x = (float)ScreenWidth - Width - OffsetX;
                y = (float)ScreenHeight - Height - OffsetY;
                break;
        }

        return ::ImVec2(x, y);
    }

    // WorldSpaceDialog implementation
    WorldSpaceDialog::WorldSpaceDialog(const Math::Vec3& Position, float Width, float Height)
        : m_Position(Position), m_Width(Width), m_Height(Height) {
    }

    WorldSpaceDialog::~WorldSpaceDialog() {
        if (bgfx::isValid(m_CachedTexture)) {
            bgfx::destroy(m_CachedTexture);
        }
    }

    void WorldSpaceDialog::SetSize(float Width, float Height) {
        if (m_Width != Width || m_Height != Height) {
            m_Width = Width;
            m_Height = Height;
            m_TextureValid = false; // Invalidate texture when size changes
        }
    }

    void WorldSpaceDialog::SetContentCallback(std::function<void()> Callback) {
        // Store callback - invalidate texture to ensure fresh content
        // Note: This is called every frame in TerminalHub, which causes texture recreation
        // This is inefficient but ensures content is always up-to-date
        m_ContentCallback = Callback;
        m_TextureValid = false; // Invalidate texture when content callback changes
    }

    void WorldSpaceDialog::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight,
                                  bgfx::ProgramHandle SceneProgram, bgfx::ViewId ViewId, bgfx::FrameBufferHandle SceneFramebuffer) {
        if (!m_Visible) return;

        // Try to render as 3D billboard if we have a valid program or billboard shader
        bool canRenderWorldSpace = bgfx::isValid(SceneProgram) || IsBillboardShaderAvailable();
        
        if (canRenderWorldSpace) {
            // Setup view for world-space rendering
            bgfx::setViewRect(ViewId, 0, 0, static_cast<uint16_t>(ScreenWidth), static_cast<uint16_t>(ScreenHeight));
            if (bgfx::isValid(SceneFramebuffer)) {
                bgfx::setViewFrameBuffer(ViewId, SceneFramebuffer);
            }
            bgfx::setViewClear(ViewId, BGFX_CLEAR_NONE); // Don't clear - add to existing scene

            // Get or create ImGui texture
            bgfx::TextureHandle textureToUse = BGFX_INVALID_HANDLE;
            if (m_ContentCallback && (m_Width > 0 && m_Height > 0)) {
                // Regenerate texture if invalid or size changed
                if (!m_TextureValid || !bgfx::isValid(m_CachedTexture)) {
                    // Destroy old texture if exists
                    if (bgfx::isValid(m_CachedTexture)) {
                        bgfx::destroy(m_CachedTexture);
                    }
                    // Render ImGui content to texture
                    // Wrap content callback to ensure it creates a window
                    m_CachedTexture = RenderImGuiToTexture(
                        static_cast<int>(m_Width),
                        static_cast<int>(m_Height),
                        [this]() {
                            // Ensure window is created for the content callback
                            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                            ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);
                            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;
                            if (ImGui::Begin("##WorldSpaceDialog", nullptr, flags)) {
                                if (m_ContentCallback) {
                                    m_ContentCallback();
                                }
                            }
                            ImGui::End();
                        },
                        ViewId + 100 // Use a different view ID to avoid conflicts
                    );
                    m_TextureValid = bgfx::isValid(m_CachedTexture);
                    if (!m_TextureValid) {
                        // Log error for debugging
                        SIMPLE_LOG("WorldSpaceDialog: Failed to create ImGui texture (width=" + 
                                  std::to_string(static_cast<int>(m_Width)) + 
                                  ", height=" + std::to_string(static_cast<int>(m_Height)) + ")");
                    }
                }
                textureToUse = m_CachedTexture;
            }

            // Fallback to white texture if ImGui texture not available
            if (!bgfx::isValid(textureToUse)) {
                static bgfx::TextureHandle whiteTexture = BGFX_INVALID_HANDLE;
                if (!bgfx::isValid(whiteTexture)) {
                    uint8_t whitePixel[4] = {255, 255, 255, 255};
                    const bgfx::Memory* mem = bgfx::copy(whitePixel, 4);
                    whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
                }
                textureToUse = whiteTexture;
            }

            // Convert pixel size to world size (approximate - you may want to adjust this)
            // Scale is based on typical screen resolution - 400px = 4 units in world space
            float worldWidth = m_Width * 0.01f;  // 400px = 4 units
            float worldHeight = m_Height * 0.01f;  // 350px = 3.5 units

            // Use billboard shader if SceneProgram not provided or invalid
            bgfx::ProgramHandle programToUse = BGFX_INVALID_HANDLE;
            if (bgfx::isValid(SceneProgram)) {
                programToUse = SceneProgram;
            }
            
            // Debug: Check if we have a valid texture
            if (!bgfx::isValid(textureToUse)) {
                SIMPLE_LOG("WorldSpaceDialog: Warning - using fallback white texture");
            } else {
                // Log successful rendering attempt
                static int renderCount = 0;
                if (++renderCount % 60 == 0) { // Log every 60 frames to avoid spam
                    SIMPLE_LOG("WorldSpaceDialog: Rendering billboard with texture idx=" + 
                              std::to_string(textureToUse.idx) + 
                              " at position (" + std::to_string(m_Position.x) + ", " + 
                              std::to_string(m_Position.y) + ", " + std::to_string(m_Position.z) + ")");
                }
            }
            
            RenderBillboardQuad(m_Position, worldWidth, worldHeight, textureToUse,
                               ViewMatrix, ProjectionMatrix, programToUse, ViewId, m_DepthTest);
            return;
        }

        // Fallback to old ImGui screen-space rendering
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        if (screenPos.x < -m_Width || screenPos.x > ScreenWidth + m_Width ||
            screenPos.y < -m_Height || screenPos.y > ScreenHeight + m_Height) {
            return; // Off screen
        }

        // Set next window position
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        // Create window flags for billboard-like behavior
        // Removed ImGuiWindowFlags_NoBackground to make the dialog visible
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

        if (ImGui::Begin("##WorldSpaceDialog", nullptr, flags)) {
            if (m_ContentCallback) {
                m_ContentCallback();
            }
        }
        ImGui::End();
    }

    // WorldSpaceLabel implementation
    WorldSpaceLabel::WorldSpaceLabel(const Math::Vec3& Position, const std::string& Text)
        : m_Position(Position), m_Text(Text) {
    }

    WorldSpaceLabel::~WorldSpaceLabel() {
    }

    void WorldSpaceLabel::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight,
                                bgfx::ProgramHandle SceneProgram, bgfx::ViewId ViewId) {
        if (!m_Visible || m_Text.empty()) return;

        // If bgfx resources are provided, render as 3D billboard
        // For text labels, we'd need to render text to a texture first
        // For now, fall back to screen-space rendering
        // TODO: Implement text-to-texture rendering for 3D billboards

        // Project 3D position to screen space
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        ImVec2 textSize = ImGui::CalcTextSize(m_Text.c_str());
        if (screenPos.x < -textSize.x || screenPos.x > ScreenWidth + textSize.x ||
            screenPos.y < -textSize.y || screenPos.y > ScreenHeight + textSize.y) {
            return; // Off screen
        }

        // Draw text at screen position using foreground draw list
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        if (drawList) {
            drawList->AddText(ImVec2(screenPos.x, screenPos.y), ImGui::GetColorU32(ImGuiCol_Text), m_Text.c_str());
        }
    }

    // WorldSpaceButton implementation
    WorldSpaceButton::WorldSpaceButton(const Math::Vec3& Position, float Width, float Height, const std::string& Label)
        : m_Position(Position), m_Width(Width), m_Height(Height), m_Label(Label) {
    }

    WorldSpaceButton::~WorldSpaceButton() {
        if (bgfx::isValid(m_CachedTexture)) {
            bgfx::destroy(m_CachedTexture);
        }
    }

    void WorldSpaceButton::SetSize(float Width, float Height) {
        if (m_Width != Width || m_Height != Height) {
            m_Width = Width;
            m_Height = Height;
            m_TextureValid = false; // Invalidate texture when size changes
        }
    }

    void WorldSpaceButton::SetLabel(const std::string& Label) {
        if (m_Label != Label) {
            m_Label = Label;
            m_TextureValid = false; // Invalidate texture when label changes
        }
    }

    bool WorldSpaceButton::Render(const Math::Matrix4& ViewMatrix, const Math::Matrix4& ProjectionMatrix, int ScreenWidth, int ScreenHeight,
                                 bgfx::ProgramHandle SceneProgram, bgfx::ViewId ViewId) {
        if (!m_Visible) return false;

        // Try to render as 3D billboard if we have a valid program or billboard shader
        bool canRenderWorldSpace = bgfx::isValid(SceneProgram) || IsBillboardShaderAvailable();
        
        if (canRenderWorldSpace) {
            // Get or create ImGui texture for button
            bgfx::TextureHandle textureToUse = BGFX_INVALID_HANDLE;
            if (m_Width > 0 && m_Height > 0) {
                // Regenerate texture if invalid or content changed
                if (!m_TextureValid || !bgfx::isValid(m_CachedTexture)) {
                    // Destroy old texture if exists
                    if (bgfx::isValid(m_CachedTexture)) {
                        bgfx::destroy(m_CachedTexture);
                    }
                    // Render button ImGui content to texture
                    m_CachedTexture = RenderImGuiToTexture(
                        static_cast<int>(m_Width),
                        static_cast<int>(m_Height),
                        [this]() {
                            // Draw button in ImGui
                            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                            ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);
                            ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                                     ImGuiWindowFlags_NoBackground;
                            if (ImGui::Begin("##WorldSpaceButton", nullptr, flags)) {
                                if (ImGui::Button(m_Label.c_str(), ImVec2(m_Width - 20.0f, m_Height - 20.0f))) {
                                    if (m_OnClick) {
                                        m_OnClick();
                                    }
                                }
                            }
                            ImGui::End();
                        },
                        ViewId + 200 // Use a different view ID to avoid conflicts
                    );
                    m_TextureValid = bgfx::isValid(m_CachedTexture);
                }
                textureToUse = m_CachedTexture;
            }

            // Fallback to white texture if ImGui texture not available
            if (!bgfx::isValid(textureToUse)) {
                static bgfx::TextureHandle whiteTexture = BGFX_INVALID_HANDLE;
                if (!bgfx::isValid(whiteTexture)) {
                    uint8_t whitePixel[4] = {255, 255, 255, 255};
                    const bgfx::Memory* mem = bgfx::copy(whitePixel, 4);
                    whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
                }
                textureToUse = whiteTexture;
            }

            // Convert pixel size to world size (approximate - you may want to adjust this)
            float worldWidth = m_Width * 0.01f;  // Adjust scale as needed
            float worldHeight = m_Height * 0.01f;

            // Use billboard shader if SceneProgram not provided or invalid
            bgfx::ProgramHandle programToUse = BGFX_INVALID_HANDLE;
            if (bgfx::isValid(SceneProgram)) {
                programToUse = SceneProgram;
            }
            RenderBillboardQuad(m_Position, worldWidth, worldHeight, textureToUse,
                               ViewMatrix, ProjectionMatrix, programToUse, ViewId, true);

            // Note: Button click detection would need raycasting in 3D mode
            // For now, return false (not clicked in 3D mode)
            return false;
        }

        // Fallback to old ImGui screen-space rendering
        Math::Vec2 screenPos = ProjectToScreen(m_Position, ViewMatrix, ProjectionMatrix, ScreenWidth, ScreenHeight);

        // Check if position is on screen
        if (screenPos.x < -m_Width || screenPos.x > ScreenWidth + m_Width ||
            screenPos.y < -m_Height || screenPos.y > ScreenHeight + m_Height) {
            return false; // Off screen
        }

        // Set next window position
        ImGui::SetNextWindowPos(ImVec2(screenPos.x, screenPos.y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        // Create window flags
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoBackground;

        bool clicked = false;
        if (ImGui::Begin("##WorldSpaceButton", nullptr, flags)) {
            if (ImGui::Button(m_Label.c_str(), ImVec2(m_Width - 20.0f, m_Height - 20.0f))) {
                clicked = true;
                if (m_OnClick) {
                    m_OnClick();
                }
            }
        }
        ImGui::End();

        return clicked;
    }

    // OverlayDialog implementation
    OverlayDialog::OverlayDialog(const std::string& Title, Anchor AnchorPos, float Width, float Height)
        : m_Title(Title), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayDialog::~OverlayDialog() {
    }

    void OverlayDialog::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    bool OverlayDialog::Begin() {
        if (!m_Visible) return false;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        bool opened = ImGui::Begin(m_Title.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (opened && m_ContentCallback) {
            m_ContentCallback();
        }

        return opened;
    }

    void OverlayDialog::End() {
        ImGui::End();
    }

    // OverlayList implementation
    OverlayList::OverlayList(const std::string& Id, Anchor AnchorPos, float Width, float Height)
        : m_Id(Id), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayList::~OverlayList() {
    }

    void OverlayList::SetItems(const std::vector<std::string>& Items) {
        m_Items = Items;
        if (m_SelectedIndex >= (int)m_Items.size()) {
            m_SelectedIndex = -1;
        }
    }

    void OverlayList::AddItem(const std::string& Item) {
        m_Items.push_back(Item);
    }

    void OverlayList::ClearItems() {
        m_Items.clear();
        m_SelectedIndex = -1;
    }

    std::string OverlayList::GetSelectedItem() const {
        if (m_SelectedIndex >= 0 && m_SelectedIndex < (int)m_Items.size()) {
            return m_Items[m_SelectedIndex];
        }
        return "";
    }

    void OverlayList::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    void OverlayList::Render() {
        if (!m_Visible) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        if (ImGui::Begin(m_Id.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar)) {
            for (size_t i = 0; i < m_Items.size(); ++i) {
                bool selected = (m_SelectedIndex == (int)i);
                if (ImGui::Selectable(m_Items[i].c_str(), selected)) {
                    int oldIndex = m_SelectedIndex;
                    m_SelectedIndex = (int)i;
                    if (m_OnSelectionChanged && oldIndex != m_SelectedIndex) {
                        m_OnSelectionChanged(m_SelectedIndex, m_Items[i]);
                    }
                }
            }
        }
        ImGui::End();
    }

    // OverlaySlider implementation
    OverlaySlider::OverlaySlider(const std::string& Label, float& Value, float Min, float Max, Anchor AnchorPos)
        : m_Label(Label), m_Value(&Value), m_Min(Min), m_Max(Max), m_Anchor(AnchorPos) {
    }

    OverlaySlider::~OverlaySlider() {
    }

    void OverlaySlider::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    bool OverlaySlider::Render() {
        if (!m_Visible || !m_Value) return false;

        ImGuiIO& io = ImGui::GetIO();
        float width = 200.0f; // Default slider width
        float height = 50.0f; // Default height for label + slider

        ImVec2 textSize = ImGui::CalcTextSize(m_Label.c_str());
        width = (std::max)(width, textSize.x + 20.0f);

        ImVec2 pos = CalculateAnchorPosition(m_Anchor, width, height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

        bool changed = false;
        if (ImGui::Begin(("##OverlaySlider_" + m_Label).c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground)) {
            ImGui::Text("%s", m_Label.c_str());

            ImGuiSliderFlags flags = 0;
            if (m_Step > 0.0f) {
                flags |= ImGuiSliderFlags_AlwaysClamp;
            }

            if (m_Step > 0.0f) {
                changed = ImGui::SliderFloat("##slider", m_Value, m_Min, m_Max, "%.2f", flags);
                // Apply step
                *m_Value = std::round(*m_Value / m_Step) * m_Step;
                *m_Value = std::max(m_Min, std::min(m_Max, *m_Value));
            } else {
                changed = ImGui::SliderFloat("##slider", m_Value, m_Min, m_Max, "%.2f", flags);
            }

            if (m_ShowValue) {
                ImGui::SameLine();
                ImGui::Text("%.2f", *m_Value);
            }
        }
        ImGui::End();

        return changed;
    }

    // OverlayPanel implementation
    OverlayPanel::OverlayPanel(const std::string& Id, Anchor AnchorPos, float Width, float Height)
        : m_Id(Id), m_Anchor(AnchorPos), m_Width(Width), m_Height(Height) {
    }

    OverlayPanel::~OverlayPanel() {
    }

    void OverlayPanel::SetPosition(Anchor AnchorPos, float OffsetX, float OffsetY) {
        m_Anchor = AnchorPos;
        m_OffsetX = OffsetX;
        m_OffsetY = OffsetY;
    }

    void OverlayPanel::Begin() {
        if (!m_Visible) return;

        ImGuiIO& io = ImGui::GetIO();
        ImVec2 pos = CalculateAnchorPosition(m_Anchor, m_Width, m_Height, (int)io.DisplaySize.x, (int)io.DisplaySize.y, m_OffsetX, m_OffsetY);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(m_Width, m_Height), ImGuiCond_Always);

        ImGui::Begin(m_Id.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (m_ContentCallback) {
            m_ContentCallback();
        }
    }

    void OverlayPanel::End() {
        ImGui::End();
    }

    // Helper function to create orthographic matrix (similar to UISystem's mtxOrtho)
    static void CreateOrthoMatrix(float* result, float left, float right, float bottom, float top, float zNear, float zFar, bool homogeneousNdc) {
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
        result[12] = dd;
        result[13] = ee;
        result[14] = ff;
        result[15] = 1.0f;
    }

    // Render ImGui content to a texture
    bgfx::TextureHandle RenderImGuiToTexture(int Width, int Height, std::function<void()> ContentCallback, bgfx::ViewId ViewId) {
        if (Width <= 0 || Height <= 0 || !ContentCallback) {
            return BGFX_INVALID_HANDLE;
        }

        // Get UISystem to access its rendering resources
        UI::UISystem& uiSystem = UI::UISystem::Instance();
        if (!uiSystem.IsInitialized()) {
            return BGFX_INVALID_HANDLE;
        }

        // Ensure ImGui context is set
        void* imguiContext = uiSystem.GetImGuiContext();
        if (imguiContext && ImGui::GetCurrentContext() != imguiContext) {
            ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiContext));
        }

        // Save current ImGui state
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 savedDisplaySize = io.DisplaySize;
        ImVec2 savedFramebufferScale = io.DisplayFramebufferScale;

        // Set up ImGui to render to our texture size
        // This must be done before calling the content callback
        io.DisplaySize = ImVec2(static_cast<float>(Width), static_cast<float>(Height));
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
        
        // Note: ImGui::NewFrame() should have been called by UISystem before this function is called
        // The content callback will draw ImGui content, which will be captured when we call Render()

        // Create texture for rendering (readable render target with sampler flags for use as regular texture)
        bgfx::TextureHandle texture = bgfx::createTexture2D(
            static_cast<uint16_t>(Width),
            static_cast<uint16_t>(Height),
            false, 1,
            bgfx::TextureFormat::BGRA8,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
        );

        if (!bgfx::isValid(texture)) {
            // Restore ImGui state
            io.DisplaySize = savedDisplaySize;
            io.DisplayFramebufferScale = savedFramebufferScale;
            return BGFX_INVALID_HANDLE;
        }

        // Create framebuffer
        bgfx::FrameBufferHandle framebuffer = bgfx::createFrameBuffer(1, &texture, true);
        if (!bgfx::isValid(framebuffer)) {
            bgfx::destroy(texture);
            // Restore ImGui state
            io.DisplaySize = savedDisplaySize;
            io.DisplayFramebufferScale = savedFramebufferScale;
            return BGFX_INVALID_HANDLE;
        }

        // Set framebuffer BEFORE calling content callback
        // This ensures the framebuffer is set up before any rendering happens
        bgfx::setViewFrameBuffer(ViewId, framebuffer);
        bgfx::setViewRect(ViewId, 0, 0, static_cast<uint16_t>(Width), static_cast<uint16_t>(Height));
        bgfx::setViewClear(ViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
        bgfx::touch(ViewId); // Submit clear command first

        // Call content callback to generate ImGui draw data
        // This must happen after setting DisplaySize but before Render()
        // The content callback should use ImGui::Begin/End, ImGui::Text, etc.
        // Note: The content callback should create a window that fills the DisplaySize
        ContentCallback();

        // Call RenderWithSettings - it will call ImGui::Render() and RenderDrawData
        // RenderDrawData will calculate viewRect from DisplaySize (which we set to Width/Height)
        // and will set the viewTransform and viewClear (to BGFX_CLEAR_NONE), but will keep our framebuffer
        // The clear from touch() above will execute before RenderDrawData's draws
        // IMPORTANT: Do NOT restore ImGui state until AFTER RenderWithSettings completes
        UI::UIRenderSettings settings(ViewId, false, true, false);
        uiSystem.RenderWithSettings(settings);

        // Restore ImGui state AFTER rendering
        io.DisplaySize = savedDisplaySize;
        io.DisplayFramebufferScale = savedFramebufferScale;

        // Detach framebuffer (texture remains valid)
        bgfx::setViewFrameBuffer(ViewId, BGFX_INVALID_HANDLE);
        bgfx::destroy(framebuffer);

        return texture;
    }

} // namespace Solstice::UI::ViewportUI
