#include "StrategyCamera.hxx"
#include "../../Core/Debug.hxx"
#include <cmath>

namespace Solstice::Game {

StrategyCamera::StrategyCamera() {
    m_Camera.WorldUp = Math::Vec3(0, 1, 0);
    m_Camera.Zoom = 60.0f;
    UpdateCameraTransform();
}

void StrategyCamera::SetPosition(const Math::Vec3& Position) {
    m_Target = Position;
    UpdateCameraTransform();
}

void StrategyCamera::SetTarget(const Math::Vec3& Target) {
    m_Target = Target;
    UpdateCameraTransform();
}

void StrategyCamera::Pan(const Math::Vec2& Delta) {
    Math::Vec3 right = m_Camera.Right;
    Math::Vec3 forward = m_Camera.Front;
    forward.y = 0.0f;
    forward = forward.Normalized();

    m_Target += right * Delta.x + forward * Delta.y;
    UpdateCameraTransform();
}

void StrategyCamera::Zoom(float Delta) {
    m_Zoom = std::max(0.1f, std::min(10.0f, m_Zoom + Delta));
    UpdateCameraTransform();
}

void StrategyCamera::Rotate(float Delta) {
    m_Rotation += Delta;
    UpdateCameraTransform();
}

void StrategyCamera::Update(float DeltaTime) {
    UpdateCameraTransform();
    // Apply interpolation for smooth camera movement
    m_Camera.Update(DeltaTime);
}

void StrategyCamera::UpdateCameraTransform() {
    float angleRad = m_Angle * (3.14159f / 180.0f);
    float rotationRad = m_Rotation * (3.14159f / 180.0f);

    // Calculate camera position
    float distance = m_Height / std::sin(angleRad);
    float x = std::cos(rotationRad) * distance * std::cos(angleRad);
    float z = std::sin(rotationRad) * distance * std::cos(angleRad);
    float y = m_Height;

    Math::Vec3 desiredPos = m_Target + Math::Vec3(x, y, z) * m_Zoom;
    m_Camera.SetTargetPosition(desiredPos);
    // Note: Update() should be called from Update() method, but for immediate updates we can set position directly
    // For smooth interpolation, call Update() in the Update() method
    m_Camera.Position = desiredPos; // Strategy camera may want immediate updates, but we set target for consistency
    m_Camera.Front = (m_Target - m_Camera.Position).Normalized();
    m_Camera.Up = Math::Vec3(0, 1, 0);
}

} // namespace Solstice::Game
