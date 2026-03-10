#include "VisualNovelGame.hxx"
#include "../../Core/Debug.hxx"
#include "../../UI/UISystem.hxx"
#include <bgfx/bgfx.h>

namespace Solstice::Game {

VisualNovelGame::VisualNovelGame() {
    m_InputManager = std::make_unique<InputManager>();
    m_DialoguePresenter = std::make_unique<DialoguePresenter>(VisualNovelPresets::GetClassicPreset());
    m_DialogueTree = std::make_unique<DialogueTree>();
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

    InitializeDialogueTree();

    if (m_DialogueTree && m_DialoguePresenter) {
        m_DialoguePresenter->SetTree(m_DialogueTree.get());
        m_DialoguePresenter->Start();
    }

    SIMPLE_LOG("VisualNovelGame: Initialized");
}

void VisualNovelGame::InitializeDialogueTree() {
    if (!m_DialogueTree) {
        return;
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
