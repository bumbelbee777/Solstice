#pragma once

#include <Render/Camera.hxx>
#include <string>
#include <functional>
#include "ObjectSpawner.hxx"

namespace Solstice::PhysicsPlayground {
    enum class ObjectType;
    class ObjectSpawner;
}

namespace Solstice::PhysicsPlayground {

class UIManager {
public:
    using SpawnCallback = std::function<void(ObjectType)>;

    UIManager(ObjectSpawner& objectSpawner, Render::Camera& camera);

    void Render();

    // Set callback for object spawning
    void SetSpawnCallback(SpawnCallback callback) { m_SpawnCallback = callback; }

    // Dialog state
    void ShowAddObjectDialog() { m_ShowAddObjectDialog = true; }
    void HideAddObjectDialog() { m_ShowAddObjectDialog = false; }
    bool IsAddObjectDialogOpen() const { return m_ShowAddObjectDialog; }

    // Debug info
    void SetFPS(float fps) { m_FPS = fps; }
    void SetFrameTime(float frameTime) { m_FrameTime = frameTime; }
    void SetObjectCount(size_t count) { m_ObjectCount = count; }
    void SetGrabbedObject(bool grabbed) { m_ObjectGrabbed = grabbed; }

private:
    void RenderMainMenu();
    void RenderAddObjectDialog();
    void RenderDebugInfo();

    ObjectSpawner& m_ObjectSpawner;
    Render::Camera& m_Camera;

    bool m_ShowAddObjectDialog{false};

    // Debug info
    float m_FPS{0.0f};
    float m_FrameTime{0.0f};
    size_t m_ObjectCount{0};
    bool m_ObjectGrabbed{false};

    // Spawn callback
    SpawnCallback m_SpawnCallback;
};

} // namespace Solstice::PhysicsPlayground
