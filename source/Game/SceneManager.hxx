#pragma once

#include "../Solstice.hxx"
#include <Render/Scene/Scene.hxx>
#include <Render/Assets/Mesh.hxx>
#include "../Core/Material.hxx"
#include <string>
#include <memory>
#include <unordered_map>

namespace Solstice::Game {

class SOLSTICE_API SceneManager {
public:
    SceneManager();
    ~SceneManager() = default;

    // Scene management
    void SetCurrentScene(const std::string& SceneName);
    std::string GetCurrentScene() const { return m_CurrentSceneName; }

    // Scene loading/saving
    bool LoadScene(const std::string& SceneName, Render::Scene& Scene,
                   Render::MeshLibrary& MeshLibrary, Core::MaterialLibrary& MaterialLibrary);
    bool SaveScene(const std::string& SceneName, const Render::Scene& Scene);

    // Scene transitions
    void TransitionToScene(const std::string& SceneName, float TransitionTime = 0.5f);
    void UpdateTransition(float DeltaTime);
    bool IsTransitioning() const { return m_IsTransitioning; }
    float GetTransitionProgress() const { return m_TransitionProgress; }

    // Scene state
    void RegisterScene(const std::string& SceneName);
    void UnregisterScene(const std::string& SceneName);
    bool IsSceneRegistered(const std::string& SceneName) const;

private:
    std::string m_CurrentSceneName;
    std::string m_TargetSceneName;
    bool m_IsTransitioning{false};
    float m_TransitionTime{0.5f};
    float m_TransitionProgress{0.0f};
    std::unordered_map<std::string, bool> m_RegisteredScenes;
};

} // namespace Solstice::Game
