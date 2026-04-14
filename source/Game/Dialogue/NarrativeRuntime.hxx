#pragma once

#include "../../Solstice.hxx"
#include "DialogueTree.hxx"
#include "../Gameplay/SFXManager.hxx"
#include <string>
#include <vector>

namespace Solstice::Game {

struct GameplaySettings;
struct AudioSettings;

// Owns dialogue presentation policy: voice playback, subtitle text selection, preferences.
// Optional SFXManager for voicelines; preferences gate subtitles and scale.
class SOLSTICE_API NarrativeRuntime {
public:
    NarrativeRuntime() = default;

    void SetTree(DialogueTree* Tree) { m_Tree = Tree; }
    DialogueTree* GetTree() const { return m_Tree; }

    void SetGameplaySettings(const GameplaySettings* Settings) { m_Gameplay = Settings; }
    void SetAudioSettings(const AudioSettings* Settings) { m_Audio = Settings; }

    // Display line: subtitle override if set, else node text (for localization).
    static std::string GetDisplayText(const DialogueNode& Node);

    bool ShouldShowDialogueText() const;
    float GetSubtitleScale() const;

    // Call when the current dialogue line becomes active (after Start or Advance).
    void OnCurrentLineEntered();

    // Stop any voiceline SFX (e.g. when stopping dialogue).
    void StopVoiceLine();

    std::string GetLastVoicePath() const { return m_LastVoicePath; }

private:
    DialogueTree* m_Tree{nullptr};
    const GameplaySettings* m_Gameplay{nullptr};
    const AudioSettings* m_Audio{nullptr};
    std::string m_LastVoicePath;
};

// Validate graph: missing nodes, dangling next/choice targets.
SOLSTICE_API void ValidateDialogueTree(const DialogueTree& Tree, std::vector<std::string>& OutErrors);

} // namespace Solstice::Game
