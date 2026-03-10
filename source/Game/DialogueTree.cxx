#include "DialogueTree.hxx"

namespace Solstice::Game {

void DialogueTree::AddNode(const DialogueNode& Node) {
    if (Node.NodeId.empty()) {
        return;
    }
    m_Nodes[Node.NodeId] = Node;
}

void DialogueTree::Clear() {
    m_Nodes.clear();
    m_StartNodeId.clear();
    m_CurrentNodeId.clear();
    m_IsAtEnd = true;
}

const DialogueNode* DialogueTree::GetNode(const std::string& NodeId) const {
    if (NodeId.empty()) {
        return nullptr;
    }
    auto It = m_Nodes.find(NodeId);
    if (It == m_Nodes.end()) {
        return nullptr;
    }
    return &It->second;
}

std::vector<DialogueChoice> DialogueTree::GetChoices(const std::string& NodeId) const {
    const DialogueNode* Node = GetNode(NodeId);
    if (Node == nullptr || Node->Choices.empty()) {
        return {};
    }
    return Node->Choices;
}

bool DialogueTree::HasChoices(const std::string& NodeId) const {
    const DialogueNode* Node = GetNode(NodeId);
    return Node != nullptr && !Node->Choices.empty();
}

void DialogueTree::Start() {
    m_CurrentNodeId = m_StartNodeId;
    m_IsAtEnd = (m_StartNodeId.empty() || GetNode(m_StartNodeId) == nullptr);
}

void DialogueTree::Advance() {
    if (m_IsAtEnd) {
        return;
    }
    const DialogueNode* Node = GetNode(m_CurrentNodeId);
    if (Node == nullptr) {
        m_IsAtEnd = true;
        return;
    }
    if (!Node->Choices.empty()) {
        // Branching node requires Advance(ChoiceIndex)
        return;
    }
    if (Node->NextNodeId.empty()) {
        m_IsAtEnd = true;
        m_CurrentNodeId.clear();
        return;
    }
    m_CurrentNodeId = Node->NextNodeId;
    if (GetNode(m_CurrentNodeId) == nullptr) {
        m_IsAtEnd = true;
        m_CurrentNodeId.clear();
    }
}

void DialogueTree::Advance(size_t ChoiceIndex) {
    if (m_IsAtEnd) {
        return;
    }
    const DialogueNode* Node = GetNode(m_CurrentNodeId);
    if (Node == nullptr || Node->Choices.empty() || ChoiceIndex >= Node->Choices.size()) {
        m_IsAtEnd = true;
        m_CurrentNodeId.clear();
        return;
    }
    const std::string& Target = Node->Choices[ChoiceIndex].TargetNodeId;
    if (Target.empty() || GetNode(Target) == nullptr) {
        m_IsAtEnd = true;
        m_CurrentNodeId.clear();
        return;
    }
    m_CurrentNodeId = Target;
}

const DialogueNode* DialogueTree::GetCurrentNode() const {
    return GetNode(m_CurrentNodeId);
}

} // namespace Solstice::Game
