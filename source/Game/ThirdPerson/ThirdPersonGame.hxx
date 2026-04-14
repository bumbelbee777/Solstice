#pragma once

#include "../App/GameBase.hxx"
#include "ThirdPersonController.hxx"
#include "../../Entity/Registry.hxx"
#include "../../Entity/Scheduler.hxx"
#include <Render/Scene/Camera.hxx>
#include <memory>

namespace Solstice::Game {

// Third-person game base class
class SOLSTICE_API ThirdPersonGame : public GameBase {
public:
    ThirdPersonGame();
    virtual ~ThirdPersonGame() = default;

protected:
    void Initialize() override;
    void Update(float DeltaTime) override;
    void Render() override;

    // Third-person specific
    virtual void InitializeCamera();
    virtual void InitializeCharacter();
    virtual void ConfigureECSPhases();

    // Camera follow
    void UpdateCameraFollow(float DeltaTime);
    void SetCameraTarget(ECS::EntityId Entity) { m_CameraTarget = Entity; }

    // Lock-on targeting
    void SetLockOnTarget(ECS::EntityId Entity) { m_LockOnTarget = Entity; }
    ECS::EntityId GetLockOnTarget() const { return m_LockOnTarget; }

    ECS::Registry m_Registry;
    ECS::PhaseScheduler m_ECSScheduler;
    ECS::EntityId m_PlayerEntity{0};
    ECS::EntityId m_CameraTarget{0};
    ECS::EntityId m_LockOnTarget{0};
    Render::Camera m_Camera;

    float m_CameraDistance{5.0f};
    float m_CameraHeight{2.0f};
    float m_CameraFollowSpeed{5.0f};
};

} // namespace Solstice::Game
