#pragma once

#include "../../Solstice.hxx"
#include <string>
#include <vector>
#include <unordered_map>

namespace Solstice::Game {

// Sentinel for invalid or missing node id
constexpr const char* const INVALID_NODE_ID = "";

// Choice: display label and target node id
struct DialogueChoice {
    std::string Label;
    std::string TargetNodeId;
};

// Single node: id, optional speaker, text, and either linear next or choices
struct DialogueNode {
    std::string NodeId;
    std::string SpeakerName;
    std::string Text;
    std::string NextNodeId;  // Linear: next node; empty = end
    std::vector<DialogueChoice> Choices;  // If non-empty, branching (ignore NextNodeId for advance)
    // Optional: path to voiceline asset (2D UI dialogue; uses SFX Voice category).
    std::string VoiceAssetPath;
    // Stable id for string tables / localization tooling (optional).
    std::string SubtitleId;
    // If non-empty, shown instead of Text when subtitles are enabled (localized line).
    std::string SubtitleText;
    // Optional tags for presenter styles (e.g. "whisper", "shout").
    std::string SpeakerStyleTag;
    // Optional event name emitted when line is shown (game/script bridge).
    std::string OnEnterEvent;
};

// Dialogue tree: nodes and runtime state
class SOLSTICE_API DialogueTree {
public:
    DialogueTree() = default;
    ~DialogueTree() = default;

    // Build
    void SetStartNodeId(const std::string& StartNodeId) { m_StartNodeId = StartNodeId; }
    const std::string& GetStartNodeId() const { return m_StartNodeId; }
    void AddNode(const DialogueNode& Node);
    void Clear();

    // Static validation: broken links, empty start (errors appended).
    void Validate(std::vector<std::string>& OutErrors) const;

    // Lookup
    const DialogueNode* GetNode(const std::string& NodeId) const;
    std::vector<DialogueChoice> GetChoices(const std::string& NodeId) const;
    bool HasChoices(const std::string& NodeId) const;

    // Runtime
    void Start();
    void Advance();  // Linear: go to next node
    void Advance(size_t ChoiceIndex);  // Branching: go to chosen target
    // Script/cutscene: jump to a node if it exists.
    bool JumpToNodeId(const std::string& NodeId);
    bool IsAtEnd() const { return m_IsAtEnd; }
    const DialogueNode* GetCurrentNode() const;
    std::string GetCurrentNodeId() const { return m_CurrentNodeId; }

    // Snapshot of all nodes (unordered; for export/round-trip tooling).
    std::vector<DialogueNode> GetAllNodes() const;

private:
    std::unordered_map<std::string, DialogueNode> m_Nodes;
    std::string m_StartNodeId;
    std::string m_CurrentNodeId;
    bool m_IsAtEnd{true};
};

} // namespace Solstice::Game
