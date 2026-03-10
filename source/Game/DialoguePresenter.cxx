#include "DialoguePresenter.hxx"
#include "../UI/UISystem.hxx"
#include <imgui.h>
#include <algorithm>

namespace Solstice::Game {

DialoguePresenter::DialoguePresenter()
    : m_Config(VisualNovelPresets::GetClassicPreset()) {
    m_Panel = std::make_unique<UI::ViewportUI::OverlayPanel>(
        "##DialogueBox",
        UI::ViewportUI::Anchor::BottomCenter,
        m_Config.BoxWidth,
        m_Config.BoxHeight);
}

DialoguePresenter::DialoguePresenter(const VisualNovelConfig& Config)
    : m_Config(Config) {
    m_Panel = std::make_unique<UI::ViewportUI::OverlayPanel>(
        "##DialogueBox",
        m_Config.BoxAnchor,
        m_Config.BoxWidth,
        m_Config.BoxHeight);
}

void DialoguePresenter::Start() {
    if (m_Tree == nullptr) {
        return;
    }
    m_Tree->Start();
    if (m_Tree->IsAtEnd()) {
        return;
    }
    ResetTypewriter();
    m_IsActive = true;
}

void DialoguePresenter::Stop() {
    m_IsActive = false;
}

void DialoguePresenter::ResetTypewriter() {
    m_TypewriterProgress = 0.0f;
    m_TypewriterElapsed = 0.0f;
}

void DialoguePresenter::UpdateTypewriter(float DeltaTime) {
    const DialogueNode* Node = (m_Tree != nullptr) ? m_Tree->GetCurrentNode() : nullptr;
    if (Node == nullptr || Node->Text.empty()) {
        m_TypewriterProgress = 1.0f;
        return;
    }
    if (m_Config.SkipMode) {
        m_TypewriterProgress = 1.0f;
        return;
    }
    float CharsPerSecond = m_Config.TypewriterCharsPerSecond > 0.0f
        ? m_Config.TypewriterCharsPerSecond
        : DEFAULT_TYPEWRITER_CHARS_PER_SECOND;
    m_TypewriterElapsed += DeltaTime;
    float FullTime = static_cast<float>(Node->Text.size()) / CharsPerSecond;
    m_TypewriterProgress = (FullTime <= 0.0f) ? 1.0f : std::min(1.0f, m_TypewriterElapsed / FullTime);
}

void DialoguePresenter::Update(float DeltaTime) {
    if (!m_IsActive || m_Tree == nullptr || m_Tree->IsAtEnd()) {
        return;
    }
    UpdateTypewriter(DeltaTime);
}

void DialoguePresenter::Render() {
    if (!m_IsActive || m_Tree == nullptr || m_Tree->IsAtEnd() || m_Panel == nullptr) {
        return;
    }

    const DialogueNode* Node = m_Tree->GetCurrentNode();
    if (Node == nullptr) {
        return;
    }

    m_Panel->SetPosition(m_Config.BoxAnchor, m_Config.BoxOffsetX, m_Config.BoxOffsetY);
    m_Panel->SetSize(m_Config.BoxWidth, m_Config.BoxHeight);
    m_Panel->SetVisible(true);

    DialogueTree* Tree = m_Tree;
    const float Progress = m_TypewriterProgress;
    const VisualNovelConfig& Config = m_Config;
    const std::string& SpeakerName = Node->SpeakerName;
    const std::string& Text = Node->Text;
    std::vector<DialogueChoice> Choices = m_Tree->GetChoices(Node->NodeId);

    m_Panel->SetContentCallback([this, Tree, Progress, &Config, SpeakerName, Text, Choices]() {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(
            Config.BackgroundColor.x,
            Config.BackgroundColor.y,
            Config.BackgroundColor.z,
            Config.BackgroundAlpha));
        ImGui::PushStyleColor(ImGuiCol_Text, Config.TextColor);

        if (Config.NameBoxHeight > 0.0f && !SpeakerName.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, Config.NameColor);
            UI::Widgets::TextBold(SpeakerName);
            ImGui::PopStyleColor();
            UI::Widgets::Spacing();
        }

        UI::Widgets::TypewriterText(Text, Progress);

        if (!Choices.empty()) {
            UI::Widgets::Spacing();
            UI::Widgets::Separator();
            for (size_t i = 0; i < Choices.size(); ++i) {
                if (UI::Widgets::Button(Choices[i].Label, [this, Tree, i]() {
                    Tree->Advance(i);
                    ResetTypewriter();
                })) {
                }
            }
        }

        ImGui::PopStyleColor(2);
    });

    m_Panel->Begin();
    m_Panel->End();
}

void DialoguePresenter::AdvanceToNext() {
    if (!m_IsActive || m_Tree == nullptr) {
        return;
    }
    if (m_Tree->HasChoices(m_Tree->GetCurrentNodeId())) {
        return;
    }
    const DialogueNode* Node = m_Tree->GetCurrentNode();
    if (Node == nullptr) {
        return;
    }
    if (m_TypewriterProgress < 1.0f) {
        float CharsPerSecond = m_Config.TypewriterCharsPerSecond > 0.0f
            ? m_Config.TypewriterCharsPerSecond
            : DEFAULT_TYPEWRITER_CHARS_PER_SECOND;
        m_TypewriterElapsed = static_cast<float>(Node->Text.size()) / CharsPerSecond;
        m_TypewriterProgress = 1.0f;
        return;
    }
    m_Tree->Advance();
    ResetTypewriter();
    if (m_Tree->IsAtEnd()) {
        Stop();
    }
}

bool DialoguePresenter::IsWaitingForConfirm() const {
    if (!m_IsActive || m_Tree == nullptr || m_Tree->IsAtEnd()) {
        return false;
    }
    if (m_Tree->HasChoices(m_Tree->GetCurrentNodeId())) {
        return false;
    }
    return m_TypewriterProgress >= 1.0f;
}

} // namespace Solstice::Game
