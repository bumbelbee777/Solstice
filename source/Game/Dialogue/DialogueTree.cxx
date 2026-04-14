#include "Dialogue/DialogueTree.hxx"

namespace Solstice::Game {

void DialogueTree::Validate(std::vector<std::string>& OutErrors) const {
    if (m_StartNodeId.empty()) {
        OutErrors.push_back("startNodeId is empty");
    } else if (GetNode(m_StartNodeId) == nullptr) {
        OutErrors.push_back("start node \"" + m_StartNodeId + "\" not found");
    }

    for (const auto& Pair : m_Nodes) {
        const DialogueNode& N = Pair.second;
        if (!N.NextNodeId.empty() && GetNode(N.NextNodeId) == nullptr) {
            OutErrors.push_back("node \"" + N.NodeId + "\" references missing next \"" + N.NextNodeId + "\"");
        }
        for (const auto& C : N.Choices) {
            if (!C.TargetNodeId.empty() && GetNode(C.TargetNodeId) == nullptr) {
                OutErrors.push_back("node \"" + N.NodeId + "\" choice \"" + C.Label + "\" targets missing \"" + C.TargetNodeId + "\"");
            }
        }
    }
}

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

bool DialogueTree::JumpToNodeId(const std::string& NodeId) {
    if (NodeId.empty() || GetNode(NodeId) == nullptr) {
        m_IsAtEnd = true;
        m_CurrentNodeId.clear();
        return false;
    }
    m_CurrentNodeId = NodeId;
    m_IsAtEnd = false;
    return true;
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

std::vector<DialogueNode> DialogueTree::GetAllNodes() const {
    std::vector<DialogueNode> V;
    V.reserve(m_Nodes.size());
    for (const auto& P : m_Nodes) {
        V.push_back(P.second);
    }
    return V;
}

} // namespace Solstice::Game
