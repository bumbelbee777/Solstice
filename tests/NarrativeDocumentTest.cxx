#include "Game/Dialogue/NarrativeDocument.hxx"
#include "Game/Dialogue/DialogueTree.hxx"
#include <cassert>
#include <cstring>
#include <string>

int main() {
    const char* Json = R"({
        "format": "solstice.narrative.v1",
        "startNodeId": "a",
        "nodes": [
            { "nodeId": "a", "text": "x", "nextNodeId": "b", "choices": [] },
            { "nodeId": "b", "text": "y", "nextNodeId": "", "choices": [] }
        ],
        "provenance": { "source": "json" }
    })";

    Solstice::Game::NarrativeDocumentV1 Doc;
    std::string Err;
    assert(Solstice::Game::NarrativeDocumentFromJSON(Json, Doc, Err));

    Solstice::Game::DialogueTree Tree;
    Doc.ToDialogueTree(Tree);
    std::vector<std::string> VE;
    Tree.Validate(VE);
    assert(VE.empty());

    std::string Yaml = Solstice::Game::NarrativeDocumentToYAML(Doc);
    (void)Yaml;

    std::string JsonRound = Solstice::Game::NarrativeDocumentToJSON(Doc, false);
    Solstice::Game::NarrativeDocumentV1 Doc2;
    assert(Solstice::Game::NarrativeDocumentFromJSON(JsonRound, Doc2, Err));
    assert(Doc2.StartNodeId == "a");

    return 0;
}
