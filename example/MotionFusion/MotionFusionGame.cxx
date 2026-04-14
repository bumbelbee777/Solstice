#include "MotionFusionGame.hxx"
#include <Solstice.hxx>
#include <UI/Core/UISystem.hxx>
#include <Physics/Lighting/LightSource.hxx>
#include <Physics/Integration/PhysicsSystem.hxx>
#include <Arzachel/MeshFactory.hxx>
#include <Core/Profiling/Profiler.hxx>
#include <bgfx/bgfx.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>

using namespace Solstice;
using namespace Solstice::Math;
using namespace Solstice::Render;
using namespace Solstice::UI;

namespace Solstice::MotionFusion {

MotionFusionGame::MotionFusionGame() = default;

MotionFusionGame::~MotionFusionGame() {
    Shutdown();
}

void MotionFusionGame::Initialize() {
    m_ShutdownDone = false;
    Solstice::Initialize();
    InitializeWindow();

    if (!GetWindow()) {
        return;
    }

    auto fbSize = GetWindow()->GetFramebufferSize();
    m_Renderer = std::make_unique<SoftwareRenderer>(fbSize.first, fbSize.second, 16, GetWindow()->NativeWindow());

    UISystem::Instance().Initialize(GetWindow()->NativeWindow());
    if (void* ctx = UISystem::Instance().GetImGuiContext()) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx));
    }

    m_MeshLibrary = std::make_unique<MeshLibrary>();
    m_MaterialLibrary = std::make_unique<Core::MaterialLibrary>();
    m_Scene.SetMeshLibrary(m_MeshLibrary.get());
    m_Scene.SetMaterialLibrary(m_MaterialLibrary.get());
    Physics::PhysicsSystem::Instance().Start(m_Registry);

    InitializeScene();
    InitializeUISpritePhysics();
    m_Camera = Camera(Vec3(0.0f, 2.6f, 7.2f), Vec3(0.0f, 1.2f, 0.0f), -90.0f, -12.0f);
}

void MotionFusionGame::Shutdown() {
    if (m_ShutdownDone) {
        return;
    }
    m_ShutdownDone = true;
    m_PhysSprites.clear();
    m_PhysBodyIds.clear();
    m_UISpritePhys.Clear();
    if (bgfx::isValid(m_UITexture)) {
        bgfx::destroy(m_UITexture);
        m_UITexture = BGFX_INVALID_HANDLE;
    }
    if (UISystem::Instance().IsInitialized()) {
        UISystem::Instance().Shutdown();
    }
    m_Lights.clear();
    m_OrbitalCubes.clear();
    m_MaterialLibrary.reset();
    m_MeshLibrary.reset();
    m_Renderer.reset();
    Solstice::Shutdown();
}

void MotionFusionGame::InitializeWindow() {
    auto window = std::make_unique<Window>(1280, 720, "Solstice - Motion Fusion Showcase");
    window->SetResizeCallback([this](int w, int h) { HandleWindowResize(w, h); });
    window->SetKeyCallback([this](int key, int sc, int action, int mods) {
        HandleKeyInput(key, sc, action, mods);
    });
    window->SetMouseButtonCallback([this](int btn, int action, int mods) {
        HandleMouseButton(btn, action, mods);
    });
    window->SetCursorPosCallback([this](double dx, double dy) {
        HandleCursorPos(dx, dy);
    });
    SetWindow(std::move(window));
}

void MotionFusionGame::InitializeScene() {
    auto floor = Arzachel::MeshFactory::CreatePlane(18.0f, 18.0f);
    uint32_t floorMesh = m_MeshLibrary->AddMesh(std::move(floor));
    m_Scene.AddObject(floorMesh, Vec3(0.0f, 0.0f, 0.0f), Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Static);

    auto cube = Arzachel::MeshFactory::CreateCube(1.2f);
    uint32_t cubeMesh = m_MeshLibrary->AddMesh(std::move(cube));
    m_CenterCube = m_Scene.AddObject(cubeMesh, Vec3(0.0f, 1.15f, 0.0f), Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Dynamic);

    auto sideCube = Arzachel::MeshFactory::CreateCube(0.9f);
    uint32_t sideMesh = m_MeshLibrary->AddMesh(std::move(sideCube));
    m_LeftCube = m_Scene.AddObject(sideMesh, Vec3(-2.2f, 0.8f, -1.4f), Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Static);
    m_RightCube = m_Scene.AddObject(sideMesh, Vec3(2.2f, 0.8f, 1.4f), Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Static);

    auto orb = Arzachel::MeshFactory::CreateCube(0.35f);
    uint32_t orbMesh = m_MeshLibrary->AddMesh(std::move(orb));
    constexpr int kOrbiters = 6;
    m_OrbitalCubes.reserve(kOrbiters);
    for (int i = 0; i < kOrbiters; ++i) {
        const float ang = static_cast<float>(i) * (6.2831853f / static_cast<float>(kOrbiters));
        const Vec3 p(std::cos(ang) * 2.8f, 1.1f + 0.15f * std::sin(ang * 2.0f), std::sin(ang) * 2.8f);
        m_OrbitalCubes.push_back(
            m_Scene.AddObject(orbMesh, p, Quaternion(), Vec3(1.0f, 1.0f, 1.0f), ObjectType_Dynamic)
        );
    }

    Physics::LightSource key;
    key.Position = Vec3(5.0f, 9.0f, 4.0f);
    key.Color = Vec3(1.0f, 0.95f, 0.8f);
    key.Intensity = 2.4f;
    m_Lights.push_back(key);

    Physics::LightSource fill;
    fill.Position = Vec3(-4.0f, 6.0f, -3.0f);
    fill.Color = Vec3(0.4f, 0.7f, 1.0f);
    fill.Intensity = 1.6f;
    m_Lights.push_back(fill);
}

void MotionFusionGame::InitializeUISpritePhysics() {
    if (!GetWindow()) {
        return;
    }
    m_PhysSprites.clear();
    m_PhysBodyIds.clear();
    m_UISpritePhys.Clear();
    const uint32_t rgba = 0xffffffffu;
    const bgfx::Memory* mem = bgfx::copy(&rgba, sizeof(rgba));
    m_UITexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);
    if (!bgfx::isValid(m_UITexture)) {
        return;
    }
    auto fb = GetWindow()->GetFramebufferSize();
    m_UISpritePhys.SetGravity(ImVec2(0.0f, 520.0f));
    m_UISpritePhys.SetRestitution(0.18f);
    m_UISpritePhys.SetBounds(ImVec2(0.0f, 0.0f),
                             ImVec2(static_cast<float>(fb.first), static_cast<float>(fb.second)));
    const ImVec2 sz(52.0f, 52.0f);
    const float palette[4][4] = {
        {1.0f, 0.55f, 0.58f, 1.0f},
        {0.55f, 0.92f, 1.0f, 1.0f},
        {0.72f, 1.0f, 0.62f, 1.0f},
        {1.0f, 0.82f, 0.48f, 1.0f},
    };
    for (int i = 0; i < 4; ++i) {
        const float x = 240.0f + static_cast<float>(i) * 92.0f;
        const float y = 150.0f + static_cast<float>((i % 3) * 36);
        Solstice::UI::Sprite spr(m_UITexture, sz);
        spr.SetColor(ImVec4(palette[i][0], palette[i][1], palette[i][2], palette[i][3]));
        const uint32_t id = m_UISpritePhys.AddAabbBody(ImVec2(x, y), ImVec2(sz.x * 0.5f, sz.y * 0.5f), 1.0f, true);
        spr.SetPosition(ImVec2(x - sz.x * 0.5f, y - sz.y * 0.5f));
        m_PhysSprites.push_back(std::move(spr));
        m_PhysBodyIds.push_back(id);
    }
}

float MotionFusionGame::EaseInOutCubic(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
}

float MotionFusionGame::EaseOutBack(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float tm1 = t - 1.0f;
    return 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
}

void MotionFusionGame::Update(float DeltaTime) {
    m_Time += std::min(DeltaTime, 0.05f);
    const float intensity = std::max(0.2f, m_MotionIntensity);
    const float yaw = std::sin(m_Time * 0.9f * intensity) * 0.65f;
    const float pitch = std::cos(m_Time * 0.55f * intensity) * 0.2f;
    m_Scene.SetRotation(m_CenterCube, Quaternion::FromEuler(pitch, yaw, 0.0f));

    const float bounce = 0.14f * std::sin(m_Time * 2.2f * intensity);
    m_Scene.SetPosition(m_CenterCube, Vec3(0.0f, 1.15f + bounce, 0.0f));
    m_Scene.SetRotation(m_LeftCube, Quaternion::FromEuler(0.0f, -yaw * 0.8f, 0.0f));
    m_Scene.SetRotation(m_RightCube, Quaternion::FromEuler(0.0f, yaw * 0.8f, 0.0f));

    for (size_t i = 0; i < m_OrbitalCubes.size(); ++i) {
        const float fi = static_cast<float>(i);
        const float a = (m_Time * (0.55f + fi * 0.07f) * intensity) + fi * 1.0471976f;
        const float radius = 2.2f + 0.42f * std::sin(m_Time * 0.9f + fi);
        const Vec3 p(std::cos(a) * radius, 1.35f + 0.35f * std::sin(a * 1.8f), std::sin(a) * radius);
        m_Scene.SetPosition(m_OrbitalCubes[i], p);
        m_Scene.SetRotation(m_OrbitalCubes[i], Quaternion::FromEuler(a * 0.2f, a * 0.7f, 0.0f));
    }

    const float frameMs = GetDeltaTime() * 1000.0f;
    Core::Profiler::Instance().SetCounter("MotionFusion.OrbitCount", static_cast<int64_t>(m_OrbitalCubes.size()));
    Core::Profiler::Instance().SetCounter("MotionFusion.FrameMs", static_cast<int64_t>(frameMs * 1000.0f));

    const float physDt = std::min(DeltaTime, 0.05f);
    if (!m_PhysSprites.empty()) {
        m_UISpritePhys.Step(physDt);
        for (size_t i = 0; i < m_PhysSprites.size() && i < m_PhysBodyIds.size(); ++i) {
            m_UISpritePhys.SyncSpriteTopLeft(m_PhysBodyIds[i], m_PhysSprites[i]);
        }
    }
}

void MotionFusionGame::RenderOverlay() {
    if (!m_Renderer || !m_ShowAdvancedOverlay) {
        return;
    }
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    const int width = m_Renderer->GetFramebufferWidth();
    const int height = m_Renderer->GetFramebufferHeight();

    const float t = std::fmod(m_Time * 0.35f, 1.0f);
    const float eased = EaseInOutCubic(t);
    const float pulse = 0.5f + 0.5f * std::sin(m_Time * 2.7f * m_MotionIntensity);
    const float kick = EaseOutBack(std::fmod(m_Time * 0.5f, 1.0f));

    const ImVec2 titleMin(32.0f, 24.0f);
    const ImVec2 titleMax(470.0f, 124.0f);
    const ImU32 c0 = IM_COL32(static_cast<int>(48 + 96 * pulse), 42, 140, 210);
    const ImU32 c1 = IM_COL32(20, static_cast<int>(90 + 120 * pulse), 180, 220);
    Primitives::DrawLinearGradientRect(fg, titleMin, titleMax, c0, c1, 0.0f);
    fg->AddText(ImVec2(52.0f, 48.0f), IM_COL32(255, 255, 255, 245), "MOTION FUSION");
    fg->AddText(ImVec2(52.0f, 78.0f), IM_COL32(220, 235, 255, 220), "2D motion graphics blended with 3D scene anchors");

    if (!m_PhysSprites.empty()) {
        for (auto& spr : m_PhysSprites) {
            spr.Render(fg);
        }
    }
    Primitives::DrawEchoDiscTrail(fg, ImVec2(static_cast<float>(width) - 140.0f, 118.0f), 40.0f, 5,
                                  IM_COL32(130, 210, 255, 220));

    const float ringRadius = 48.0f + 24.0f * eased;
    Primitives::DrawArc(
        fg, ImVec2(1080.0f, 88.0f), ringRadius, 0.0f, 6.28318f * std::max(0.1f, eased), IM_COL32(130, 255, 220, 255), 4.0f,
        64
    );
    Primitives::DrawArc(
        fg,
        ImVec2(1080.0f, 88.0f),
        ringRadius + 14.0f,
        1.047f * kick,
        1.047f * kick + 3.9f,
        IM_COL32(90, 170, 255, 200),
        2.0f,
        56
    );

    const Vec3 cubePos = m_Scene.GetPosition(m_CenterCube);
    const Matrix4 view = m_Camera.GetViewMatrix();
    const float aspect = static_cast<float>(width) / std::max(1.0f, static_cast<float>(height));
    const Matrix4 proj = Matrix4::Perspective(m_Camera.GetZoom() * 0.0174533f, aspect, 0.1f, 300.0f);
    const Vec2 screen = ViewportUI::ProjectToScreen(cubePos + Vec3(0.0f, 1.3f, 0.0f), view, proj, width, height);
    const ImVec2 anchor(screen.x, screen.y);
    Primitives::DrawStarFilled(fg, anchor, 24.0f + pulse * 6.0f, 11.0f, 5, IM_COL32(255, 240, 110, 220));
    fg->AddText(ImVec2(anchor.x + 26.0f, anchor.y - 8.0f), IM_COL32(255, 255, 255, 245), "3D-anchored motion callout");

    if (m_ShowWorldAnchors) {
        const Vec3 left = m_Scene.GetPosition(m_LeftCube) + Vec3(0.0f, 0.75f, 0.0f);
        const Vec3 right = m_Scene.GetPosition(m_RightCube) + Vec3(0.0f, 0.75f, 0.0f);
        const Vec2 l2 = ViewportUI::ProjectToScreen(left, view, proj, width, height);
        const Vec2 r2 = ViewportUI::ProjectToScreen(right, view, proj, width, height);
        const ImVec2 lp(l2.x, l2.y);
        const ImVec2 rp(r2.x, r2.y);
        fg->AddCircleFilled(lp, 8.0f + 4.0f * pulse, IM_COL32(80, 220, 255, 215));
        fg->AddCircleFilled(rp, 8.0f + 4.0f * (1.0f - pulse), IM_COL32(255, 110, 180, 215));
        fg->AddBezierCubic(
            ImVec2(lp.x, lp.y),
            ImVec2(lp.x + 90.0f, lp.y - 50.0f - 20.0f * kick),
            ImVec2(rp.x - 90.0f, rp.y - 60.0f + 20.0f * kick),
            ImVec2(rp.x, rp.y),
            IM_COL32(170, 210, 255, 190),
            2.5f
        );
    }

    // Bottom timeline: layered easing channels for motion-graphics showcase.
    const float panelY = static_cast<float>(height - 110);
    fg->AddRectFilledMultiColor(
        ImVec2(34.0f, panelY),
        ImVec2(static_cast<float>(width - 34), static_cast<float>(height - 26)),
        IM_COL32(20, 24, 38, 210),
        IM_COL32(28, 36, 52, 220),
        IM_COL32(14, 18, 30, 210),
        IM_COL32(18, 22, 35, 220)
    );
    fg->AddText(ImVec2(52.0f, panelY + 10.0f), IM_COL32(216, 226, 255, 245), "Layered Easing Channels");
    const float barW = static_cast<float>(width - 180);
    const float x0 = 140.0f;
    for (int i = 0; i < 3; ++i) {
        const float phase = std::fmod(m_Time * (0.28f + 0.1f * static_cast<float>(i)), 1.0f);
        const float e = (i == 0) ? EaseInOutCubic(phase) : (i == 1 ? EaseOutBack(phase) : std::pow(phase, 0.65f));
        const float y = panelY + 34.0f + static_cast<float>(i) * 20.0f;
        fg->AddLine(ImVec2(x0, y), ImVec2(x0 + barW, y), IM_COL32(80, 95, 120, 190), 1.0f);
        fg->AddCircleFilled(ImVec2(x0 + barW * e, y), 5.0f + 2.0f * pulse, IM_COL32(120 + 40 * i, 170 + 25 * i, 255, 245));
    }
}

void MotionFusionGame::Render() {
    if (!m_Renderer) {
        return;
    }
    UISystem::Instance().NewFrame();
    ImGui::SetNextWindowPos(ImVec2(24.0f, 140.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 180.0f), ImGuiCond_Once);
    ImGui::Begin("MotionFusion Controls");
    ImGui::Checkbox("Advanced overlay", &m_ShowAdvancedOverlay);
    ImGui::Checkbox("3D anchor links", &m_ShowWorldAnchors);
    ImGui::SliderFloat("Motion intensity", &m_MotionIntensity, 0.2f, 2.5f);
    ImGui::Text("W/S/A/D: move camera");
    ImGui::Text("Space/Shift: vertical move");
    ImGui::Text("RMB: toggle mouse-look lock");
    ImGui::Text("Tab: overlay, M: mouse-look, F: fullscreen");
    ImGui::Text("Q/E: intensity down/up");
    ImGui::Text("Mouse-look: %s (%s)", m_MouseLookEnabled ? "enabled" : "disabled", m_MouseLocked ? "locked" : "free");
    ImGui::Text("Orbits: %d", static_cast<int>(m_OrbitalCubes.size()));
    ImGui::Text("Frame: %.2f ms", GetDeltaTime() * 1000.0f);
    ImGui::End();
    m_Renderer->Clear(Vec4(0.03f, 0.035f, 0.06f, 1.0f));
    m_Renderer->RenderScene(m_Scene, m_Camera, m_Lights);
    RenderOverlay();
    UISystem::Instance().Render();
    m_Renderer->Present();
}

void MotionFusionGame::HandleInput() {
    if (!GetWindow()) {
        return;
    }
    if (m_PendingResize) {
        auto now = std::chrono::high_resolution_clock::now();
        float elapsed = std::chrono::duration<float>(now - m_LastResizeEvent).count();
        if (elapsed > 0.12f) {
            m_PendingResize = false;
            if (m_Renderer) {
                m_Renderer->Resize(m_PendingWidth, m_PendingHeight);
            }
            m_UISpritePhys.SetBounds(ImVec2(0.0f, 0.0f),
                                     ImVec2(static_cast<float>(m_PendingWidth), static_cast<float>(m_PendingHeight)));
        }
    }

    float dt = GetDeltaTime();
    const float moveSpeed = 3.2f;
    if (GetWindow()->IsKeyScanPressed(26) || GetWindow()->IsKeyScanPressed(82)) {
        m_Camera.Position += m_Camera.Front * moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(22) || GetWindow()->IsKeyScanPressed(81)) {
        m_Camera.Position -= m_Camera.Front * moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(4) || GetWindow()->IsKeyScanPressed(80)) {
        m_Camera.Position -= m_Camera.Right * moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(7) || GetWindow()->IsKeyScanPressed(79)) {
        m_Camera.Position += m_Camera.Right * moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(44)) { // space
        m_Camera.Position.y += moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(225) || GetWindow()->IsKeyScanPressed(224)) { // shift
        m_Camera.Position.y -= moveSpeed * dt;
    }
    if (GetWindow()->IsKeyScanPressed(20)) { // q
        m_MotionIntensity = std::max(0.2f, m_MotionIntensity - dt);
    }
    if (GetWindow()->IsKeyScanPressed(8)) { // e
        m_MotionIntensity = std::min(2.5f, m_MotionIntensity + dt);
    }
    if (GetWindow()->IsKeyScanPressed(45)) { // -
        m_ShowWorldAnchors = false;
    }
    if (GetWindow()->IsKeyScanPressed(46)) { // =
        m_ShowWorldAnchors = true;
    }
}

void MotionFusionGame::HandleWindowResize(int W, int H) {
    m_PendingResize = true;
    m_PendingWidth = W;
    m_PendingHeight = H;
    m_LastResizeEvent = std::chrono::high_resolution_clock::now();
}

void MotionFusionGame::HandleKeyInput(int /*Key*/, int Scancode, int Action, int /*Mods*/) {
    if (Action != 1) { // press only
        return;
    }
    if (!GetWindow()) {
        return;
    }

    if (Scancode == 43) { // tab: toggle advanced overlay
        m_ShowAdvancedOverlay = !m_ShowAdvancedOverlay;
    }
    if (Scancode == 16) { // m: toggle mouse-look enable
        m_MouseLookEnabled = !m_MouseLookEnabled;
        if (!m_MouseLookEnabled && m_MouseLocked) {
            m_MouseLocked = false;
            GetWindow()->SetRelativeMouse(false);
            GetWindow()->SetCursorGrab(false);
            GetWindow()->ShowCursor(true);
        }
    }
    if (Scancode == 9) { // f: toggle fullscreen
        GetWindow()->SetFullscreen(!GetWindow()->IsFullscreen());
        m_PendingResize = true;
        m_LastResizeEvent = std::chrono::high_resolution_clock::now();
    }
}

void MotionFusionGame::HandleMouseButton(int Button, int Action, int /*Mods*/) {
    if (Button == 1 && Action == 1 && GetWindow()) { // right click
        if (!m_MouseLookEnabled) {
            return;
        }
        m_MouseLocked = !m_MouseLocked;
        GetWindow()->SetRelativeMouse(m_MouseLocked);
        GetWindow()->SetCursorGrab(m_MouseLocked);
        GetWindow()->ShowCursor(!m_MouseLocked);
        m_HadMouseSample = false;
    }
}

void MotionFusionGame::HandleCursorPos(double Dx, double Dy) {
    if (!m_MouseLocked || !m_MouseLookEnabled) {
        return;
    }
    if (!m_HadMouseSample) {
        m_LastMouseX = Dx;
        m_LastMouseY = Dy;
        m_HadMouseSample = true;
        return;
    }

    const float xOff = static_cast<float>(Dx - m_LastMouseX) * m_MouseSensitivity;
    const float yOff = static_cast<float>(m_LastMouseY - Dy) * m_MouseSensitivity;
    m_LastMouseX = Dx;
    m_LastMouseY = Dy;
    m_Camera.ProcessMouseMovement(xOff, yOff);
}

} // namespace Solstice::MotionFusion
