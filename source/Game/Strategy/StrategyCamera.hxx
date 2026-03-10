#pragma once

#include "../../Solstice.hxx"
#include <Render/Scene/Camera.hxx>
#include "../../Math/Vector.hxx"

namespace Solstice::Game {

// Strategy camera for top-down/isometric view
class SOLSTICE_API StrategyCamera {
public:
    StrategyCamera();
    ~StrategyCamera() = default;

    // Camera control
    void SetPosition(const Math::Vec3& Position);
    void SetTarget(const Math::Vec3& Target);
    void SetHeight(float Height) { m_Height = Height; }
    void SetAngle(float Angle) { m_Angle = Angle; } // Angle from vertical (0 = top-down, 45 = isometric)

    // Movement
    void Pan(const Math::Vec2& Delta);
    void Zoom(float Delta);
    void Rotate(float Delta);

    // Get camera
    Render::Camera& GetCamera() { return m_Camera; }
    const Render::Camera& GetCamera() const { return m_Camera; }

    // Update (call each frame)
    void Update(float DeltaTime);

private:
    Render::Camera m_Camera;
    Math::Vec3 m_Target{0, 0, 0};
    float m_Height{20.0f};
    float m_Angle{45.0f}; // Degrees from vertical
    float m_Zoom{1.0f};
    float m_Rotation{0.0f}; // Rotation around target

    void UpdateCameraTransform();
};

} // namespace Solstice::Game
