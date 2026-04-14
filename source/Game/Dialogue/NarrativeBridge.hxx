#pragma once

#include "../../Solstice.hxx"
#include <string>

namespace Solstice::Game {

class DialogueTree;
class NarrativeRuntime;
class CutscenePlayer;

// Global bridge for Moonwalk natives and tools (set by game bootstrap).
class SOLSTICE_API NarrativeBridge {
public:
    static void SetDialogueTree(DialogueTree* Tree) { s_DialogueTree = Tree; }
    static DialogueTree* GetDialogueTree() { return s_DialogueTree; }

    static void SetNarrativeRuntime(NarrativeRuntime* R) { s_NarrativeRuntime = R; }
    static NarrativeRuntime* GetNarrativeRuntime() { return s_NarrativeRuntime; }

    static void SetCutscenePlayer(CutscenePlayer* P) { s_CutscenePlayer = P; }
    static CutscenePlayer* GetCutscenePlayer() { return s_CutscenePlayer; }

private:
    static inline DialogueTree* s_DialogueTree{nullptr};
    static inline NarrativeRuntime* s_NarrativeRuntime{nullptr};
    static inline CutscenePlayer* s_CutscenePlayer{nullptr};
};

} // namespace Solstice::Game
