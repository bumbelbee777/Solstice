#include "ThirdPersonGame.hxx"
#include "../../Core/Debug.hxx"
#include "../../Entity/Transform.hxx"

namespace Solstice::Game {

ThirdPersonGame::ThirdPersonGame() {
}

void ThirdPersonGame::Initialize() {
    GameBase::Initialize();
    InitializeCamera();
    InitializeCharacter();
}

void ThirdPersonGame::InitializeCamera() {
    m_Camera.Position = Math::Vec3(0, m_CameraHeight, m_CameraDistance);
    m_Camera.WorldUp = Math::Vec3(0, 1, 0);
    m_Camera.Up = Math::Vec3(0, 1, 0);
    m_Camera.Zoom = 60.0f;
    SIMPLE_LOG("ThirdPersonGame: Camera initialized");
}

void ThirdPersonGame::InitializeCharacter() {
    m_PlayerEntity = m_Registry.Create();
    m_CameraTarget = m_PlayerEntity;
    SIMPLE_LOG("ThirdPersonGame: Character initialized");
}

void ThirdPersonGame::Update(float DeltaTime) {
    GameBase::Update(DeltaTime);
    UpdateCameraFollow(DeltaTime);
}

void ThirdPersonGame::Render() {
    GameBase::Render();
}

void ThirdPersonGame::UpdateCameraFollow(float DeltaTime) {
    if (m_CameraTarget == 0) return;
    if (!m_Registry.Has<ECS::Transform>(m_CameraTarget)) return;

    auto& transform = m_Registry.Get<ECS::Transform>(m_CameraTarget);
    Math::Vec3 targetPos = transform.Position;
    targetPos.y += m_CameraHeight;

    // Smooth camera follow with interpolation
    Math::Vec3 desiredPos = targetPos - m_Camera.Front * m_CameraDistance;
    m_Camera.SetTargetPosition(desiredPos);
    m_Camera.SetInterpolationSpeed(m_CameraFollowSpeed * 10.0f); // Scale to match old behavior
    m_Camera.Update(DeltaTime);

    // Look at target
    Math::Vec3 direction = (targetPos - m_Camera.Position).Normalized();
    m_Camera.Front = direction;
}

} // namespace Solstice::Game
