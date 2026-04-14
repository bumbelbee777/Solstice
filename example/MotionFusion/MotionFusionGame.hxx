#pragma once

#include <Game/App/GameBase.hxx>
#include <UI/Core/Window.hxx>
#include <UI/Viewport/ViewportUI.hxx>
#include <UI/Motion/Primitives.hxx>
#include <UI/Motion/Sprite.hxx>
#include <UI/Motion/SpritePhysics2D.hxx>
#include <Render/SoftwareRenderer.hxx>
#include <Render/Scene/Scene.hxx>
#include <Render/Scene/Camera.hxx>
#include <Render/Assets/Mesh.hxx>
#include <Material/Material.hxx>
#include <Math/Vector.hxx>
#include <Math/Quaternion.hxx>
#include <Entity/Registry.hxx>
#include <bgfx/bgfx.h>
#include <memory>
#include <vector>
#include <chrono>

namespace Solstice::MotionFusion {

class MotionFusionGame : public Game::GameBase {
public:
    MotionFusionGame();
    ~MotionFusionGame() override;

protected:
    void Initialize() override;
    void Shutdown() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

private:
    void InitializeWindow();
    void InitializeScene();
    void InitializeUISpritePhysics();
    void RenderOverlay();
    void HandleWindowResize(int W, int H);
    void HandleKeyInput(int Key, int Scancode, int Action, int Mods);
    void HandleMouseButton(int Button, int Action, int Mods);
    void HandleCursorPos(double Dx, double Dy);
    float EaseInOutCubic(float t) const;
    float EaseOutBack(float t) const;

    std::unique_ptr<Render::SoftwareRenderer> m_Renderer;
    Render::Scene m_Scene;
    std::unique_ptr<Render::MeshLibrary> m_MeshLibrary;
    std::unique_ptr<Core::MaterialLibrary> m_MaterialLibrary;
    ECS::Registry m_Registry;
    Render::Camera m_Camera;
    std::vector<Physics::LightSource> m_Lights;

    Render::SceneObjectID m_CenterCube{Render::InvalidObjectID};
    Render::SceneObjectID m_LeftCube{Render::InvalidObjectID};
    Render::SceneObjectID m_RightCube{Render::InvalidObjectID};
    std::vector<Render::SceneObjectID> m_OrbitalCubes;
    float m_Time{0.0f};
    bool m_PendingResize{false};
    int m_PendingWidth{1280};
    int m_PendingHeight{720};
    std::chrono::high_resolution_clock::time_point m_LastResizeEvent;
    bool m_MouseLocked{false};
    bool m_MouseLookEnabled{true};
    float m_MouseSensitivity{0.12f};
    bool m_HadMouseSample{false};
    double m_LastMouseX{0.0};
    double m_LastMouseY{0.0};
    bool m_ShowAdvancedOverlay{true};
    bool m_ShowWorldAnchors{true};
    float m_MotionIntensity{1.0f};
    bool m_ShutdownDone{false};

    Solstice::UI::SpritePhysicsWorld m_UISpritePhys;
    bgfx::TextureHandle m_UITexture{BGFX_INVALID_HANDLE};
    std::vector<Solstice::UI::Sprite> m_PhysSprites;
    std::vector<uint32_t> m_PhysBodyIds;
};

} // namespace Solstice::MotionFusion
