#include "VisualNovelGame.hxx"
#include "../../Core/Debug/Debug.hxx"
#include "../../UI/Core/UISystem.hxx"
#include "Dialogue/NarrativeBridge.hxx"
#include "Dialogue/NarrativeDocument.hxx"
#include "Gameplay/SFXManager.hxx"
#include <bgfx/bgfx.h>

namespace Solstice::Game {

VisualNovelGame::VisualNovelGame() {
    m_InputManager = std::make_unique<InputManager>();
    m_DialoguePresenter = std::make_unique<DialoguePresenter>(VisualNovelPresets::GetClassicPreset());
    m_DialogueTree = std::make_unique<DialogueTree>();
    m_Preferences = std::make_unique<GamePreferences>();
}

void VisualNovelGame::Initialize() {
    Solstice::Initialize();

    GameBase::Initialize();

    if (m_Window) {
        m_InputManager->Update(m_Window.get());
        if (!UI::UISystem::Instance().IsInitialized() && bgfx::getRendererType() != bgfx::RendererType::Noop) {
            UI::UISystem::Instance().Initialize(m_Window->NativeWindow());
        }
    }

    m_Preferences->Load();
    SFXManager::Instance().Initialize();
    m_Preferences->ApplyAudioSettings();

    InitializeDialogueTree();

    m_NarrativeRuntime.SetTree(m_DialogueTree.get());
    m_NarrativeRuntime.SetGameplaySettings(&m_Preferences->GetGameplaySettings());
    m_NarrativeRuntime.SetAudioSettings(&m_Preferences->GetAudioSettings());
    NarrativeBridge::SetDialogueTree(m_DialogueTree.get());
    NarrativeBridge::SetNarrativeRuntime(&m_NarrativeRuntime);
    NarrativeBridge::SetCutscenePlayer(&m_CutscenePlayer);

    if (m_DialogueTree && m_DialoguePresenter) {
        m_DialoguePresenter->SetNarrativeRuntime(&m_NarrativeRuntime);
        m_DialoguePresenter->SetTree(m_DialogueTree.get());
        m_DialoguePresenter->Start();
    }

    SIMPLE_LOG("VisualNovelGame: Initialized");
}

void VisualNovelGame::InitializeDialogueTree() {
    if (!m_DialogueTree) {
        return;
    }

    static const char* CandidatePaths[] = {
        "example/VisualNovel/assets/sample_narrative.json",
        "../example/VisualNovel/assets/sample_narrative.json",
        "../../example/VisualNovel/assets/sample_narrative.json",
        "assets/visual_novel/sample_narrative.json",
    };
    for (const char* P : CandidatePaths) {
        NarrativeDocumentV1 Doc;
        std::string Err;
        if (NarrativeDocumentLoadFile(P, Doc, Err)) {
            std::vector<std::string> ValErr;
            Doc.ToDialogueTree(*m_DialogueTree);
            m_DialogueTree->Validate(ValErr);
            if (!ValErr.empty()) {
                SIMPLE_LOG("VisualNovelGame: narrative validation: " + ValErr[0]);
            }
            SIMPLE_LOG(std::string("VisualNovelGame: loaded narrative from ") + P);
            return;
        }
    }

    DialogueNode StartNode;
    StartNode.NodeId = "start";
    StartNode.SpeakerName = "Narrator";
    StartNode.Text = "Welcome to the visual novel preset. Press Space or Enter to advance.";
    StartNode.NextNodeId = "second";
    m_DialogueTree->AddNode(StartNode);

    DialogueNode SecondNode;
    SecondNode.NodeId = "second";
    SecondNode.SpeakerName = "Narrator";
    SecondNode.Text = "This is a second line. You'll see a choice next.";
    SecondNode.NextNodeId = "choice_node";
    m_DialogueTree->AddNode(SecondNode);

    DialogueNode ChoiceNode;
    ChoiceNode.NodeId = "choice_node";
    ChoiceNode.SpeakerName = "System";
    ChoiceNode.Text = "What do you want to do?";
    ChoiceNode.Choices = {
        {"Continue the story", "continue"},
        {"End for now", "end"}
    };
    m_DialogueTree->AddNode(ChoiceNode);

    DialogueNode ContinueNode;
    ContinueNode.NodeId = "continue";
    ContinueNode.SpeakerName = "Narrator";
    ContinueNode.Text = "You chose to continue. The dialogue system is working.";
    ContinueNode.NextNodeId = "end";
    m_DialogueTree->AddNode(ContinueNode);

    DialogueNode EndNode;
    EndNode.NodeId = "end";
    EndNode.SpeakerName = "";
    EndNode.Text = "Thanks for trying the visual novel preset. Restart to see the dialogue again.";
    EndNode.NextNodeId = "";
    m_DialogueTree->AddNode(EndNode);

    m_DialogueTree->SetStartNodeId("start");
}

void VisualNovelGame::Update(float DeltaTime) {
    GameBase::Update(DeltaTime);

    if (m_InputManager && m_Window) {
        m_InputManager->Update(m_Window.get());
    }

    if (m_DialoguePresenter) {
        m_DialoguePresenter->Update(DeltaTime);
    }
    m_CutscenePlayer.Update(DeltaTime);
}

void VisualNovelGame::Render() {
    GameBase::Render();

    if (!UI::UISystem::Instance().IsInitialized()) {
        return;
    }

    UI::UISystem::Instance().NewFrame();

    if (m_DialoguePresenter) {
        m_DialoguePresenter->Render();
    }

    UI::UISystem::Instance().Render();
}

void VisualNovelGame::HandleInput() {
    if (!m_DialoguePresenter || !m_InputManager) {
        return;
    }

    if (m_DialoguePresenter->IsWaitingForConfirm()) {
        if (m_InputManager->IsKeyJustPressed(44) || m_InputManager->IsKeyJustPressed(13)) {
            m_DialoguePresenter->AdvanceToNext();
        }
    }
}

} // namespace Solstice::Game
