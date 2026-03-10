#pragma once

#include "../Solstice.hxx"
#include "DialogueTree.hxx"
#include "VisualNovelPresets.hxx"
#include "../UI/ViewportUI.hxx"
#include "../UI/Widgets.hxx"
#include <memory>

namespace Solstice::Game {

// Renders dialogue using ViewportUI + Widgets (typewriter, choices).
// Preset-driven layout and text speed.
class SOLSTICE_API DialoguePresenter {
public:
    DialoguePresenter();
    explicit DialoguePresenter(const VisualNovelConfig& Config);
    ~DialoguePresenter() = default;

    void SetConfig(const VisualNovelConfig& Config) { m_Config = Config; }
    const VisualNovelConfig& GetConfig() const { return m_Config; }

    void SetTree(DialogueTree* Tree) { m_Tree = Tree; }
    DialogueTree* GetTree() const { return m_Tree; }

    void Start();
    void Stop();
    bool IsActive() const { return m_IsActive; }

    void Update(float DeltaTime);

    // Draw dialogue box and content (call after UISystem::NewFrame).
    void Render();

    // Call when user confirms (space/enter/click): complete typewriter or advance linear node.
    void AdvanceToNext();

    // True when dialogue is active and waiting for confirm (progress >= 1, no choices).
    bool IsWaitingForConfirm() const;

private:
    void ResetTypewriter();
    void UpdateTypewriter(float DeltaTime);

    VisualNovelConfig m_Config;
    DialogueTree* m_Tree{nullptr};
    std::unique_ptr<UI::ViewportUI::OverlayPanel> m_Panel;

    bool m_IsActive{false};
    float m_TypewriterProgress{0.0f};
    float m_TypewriterElapsed{0.0f};
};

} // namespace Solstice::Game
