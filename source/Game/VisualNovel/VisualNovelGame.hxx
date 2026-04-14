#pragma once

#include "../App/GameBase.hxx"
#include "../App/GamePreferences.hxx"
#include "../Cutscene/CutscenePlayer.hxx"
#include "../Dialogue/NarrativeRuntime.hxx"
#include "../UI/DialoguePresenter.hxx"
#include "../Dialogue/DialogueTree.hxx"
#include "../App/InputManager.hxx"
#include "../Dialogue/VisualNovelPresets.hxx"
#include <memory>

namespace Solstice::Game {

// Minimal game loop for visual novel: dialogue UI only, preset-driven.
class SOLSTICE_API VisualNovelGame : public GameBase {
public:
    VisualNovelGame();
    virtual ~VisualNovelGame() = default;

protected:
    void Initialize() override;
    void Update(float DeltaTime) override;
    void Render() override;
    void HandleInput() override;

    // Optional: build a sample tree for testing (override to load from data).
    virtual void InitializeDialogueTree();

private:
    std::unique_ptr<DialoguePresenter> m_DialoguePresenter;
    std::unique_ptr<DialogueTree> m_DialogueTree;
    std::unique_ptr<InputManager> m_InputManager;
    std::unique_ptr<GamePreferences> m_Preferences;
    NarrativeRuntime m_NarrativeRuntime;
    CutscenePlayer m_CutscenePlayer;
};

} // namespace Solstice::Game
