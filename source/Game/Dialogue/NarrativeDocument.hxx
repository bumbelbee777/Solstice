#pragma once

#include "../../Solstice.hxx"
#include "DialogueTree.hxx"
#include <string>
#include <vector>

namespace Solstice::Game {

// Intermediate narrative document (v1): dual-canonical JSON/YAML map to the same struct before DialogueTree.
struct SOLSTICE_API NarrativeDocumentV1 {
    std::string Format{"solstice.narrative.v1"};
    std::string StartNodeId;
    std::vector<DialogueNode> Nodes;
    // Provenance for round-trip / merge tooling
    std::string ProvenanceSource{"json"};

    void ToDialogueTree(DialogueTree& Out) const;
    void FromDialogueTree(const DialogueTree& Tree);
};

// Load JSON string into document; returns false and appends to Err on failure.
SOLSTICE_API bool NarrativeDocumentFromJSON(const std::string& JSON, NarrativeDocumentV1& Out, std::string& Err);

// Serialize document to JSON (deterministic key order in objects where applicable).
SOLSTICE_API std::string NarrativeDocumentToJSON(const NarrativeDocumentV1& Doc, bool Pretty = true);

// Load from .json file. For .yaml/.yml uses NarrativeDocumentFromYAML.
SOLSTICE_API bool NarrativeDocumentLoadFile(const std::string& Path, NarrativeDocumentV1& Out, std::string& Err);

SOLSTICE_API bool NarrativeDocumentFromYAML(const std::string& YAML, NarrativeDocumentV1& Out, std::string& Err);
SOLSTICE_API std::string NarrativeDocumentToYAML(const NarrativeDocumentV1& Doc);

} // namespace Solstice::Game
