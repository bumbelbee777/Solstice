#include "Dialogue/NarrativeRuntime.hxx"
#include "../App/GamePreferences.hxx"

namespace Solstice::Game {

std::string NarrativeRuntime::GetDisplayText(const DialogueNode& Node) {
    if (!Node.SubtitleText.empty()) {
        return Node.SubtitleText;
    }
    return Node.Text;
}

bool NarrativeRuntime::ShouldShowDialogueText() const {
    if (m_Gameplay == nullptr) {
        return true;
    }
    return m_Gameplay->SubtitlesEnabled;
}

float NarrativeRuntime::GetSubtitleScale() const {
    if (m_Gameplay == nullptr) {
        return 1.0f;
    }
    float S = m_Gameplay->SubtitlesSize;
    return (S > 0.0f) ? S : 1.0f;
}

void NarrativeRuntime::OnCurrentLineEntered() {
    StopVoiceLine();
    if (m_Tree == nullptr) {
        return;
    }
    const DialogueNode* Node = m_Tree->GetCurrentNode();
    if (Node == nullptr) {
        return;
    }
    if (Node->VoiceAssetPath.empty()) {
        return;
    }
    m_LastVoicePath = Node->VoiceAssetPath;
    float VoiceVol = 1.0f;
    if (m_Audio != nullptr) {
        VoiceVol = m_Audio->VoiceVolume;
    }
    SFXManager::Instance().PlaySound(Node->VoiceAssetPath, SFXCategory::Voice, VoiceVol, false);
}

void NarrativeRuntime::StopVoiceLine() {
    if (!m_LastVoicePath.empty()) {
        SFXManager::Instance().StopAllSounds(SFXCategory::Voice);
    }
    m_LastVoicePath.clear();
}

void ValidateDialogueTree(const DialogueTree& Tree, std::vector<std::string>& OutErrors) {
    OutErrors.clear();
    Tree.Validate(OutErrors);
}

} // namespace Solstice::Game
