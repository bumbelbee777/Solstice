#include "SolsticeAPI/V1/Narrative.h"
#include "../../Game/Dialogue/NarrativeDocument.hxx"
#include "../../Game/Dialogue/DialogueTree.hxx"
#include <cstring>
#include <vector>
#include <sstream>
#include <string>

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NarrativeValidateJSON(
    const char* Json,
    char* ErrBuffer,
    size_t ErrBufferSize) {
    if (!Json) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Game::NarrativeDocumentV1 Doc;
    std::string Err;
    if (!Solstice::Game::NarrativeDocumentFromJSON(std::string(Json), Doc, Err)) {
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, Err.size());
            std::memcpy(ErrBuffer, Err.c_str(), L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    Solstice::Game::DialogueTree Tree;
    Doc.ToDialogueTree(Tree);
    std::vector<std::string> V;
    Tree.Validate(V);
    if (!V.empty()) {
        std::ostringstream Oss;
        for (size_t i = 0; i < V.size(); ++i) {
            if (i > 0) {
                Oss << "\n";
            }
            Oss << V[i];
        }
        std::string S = Oss.str();
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, S.size());
            std::memcpy(ErrBuffer, S.c_str(), L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    if (ErrBuffer && ErrBufferSize > 0) {
        ErrBuffer[0] = '\0';
    }
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_NarrativeJSONToYAML(
    const char* Json,
    char* OutBuffer,
    size_t OutBufferSize,
    char* ErrBuffer,
    size_t ErrBufferSize) {
    if (!Json || !OutBuffer || OutBufferSize == 0) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Game::NarrativeDocumentV1 Doc;
    std::string Err;
    if (!Solstice::Game::NarrativeDocumentFromJSON(std::string(Json), Doc, Err)) {
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, Err.size());
            std::memcpy(ErrBuffer, Err.c_str(), L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    std::string Yaml = Solstice::Game::NarrativeDocumentToYAML(Doc);
    if (Yaml.size() >= OutBufferSize) {
        const char* msg = "Output buffer too small";
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, strlen(msg));
            std::memcpy(ErrBuffer, msg, L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    std::memcpy(OutBuffer, Yaml.c_str(), Yaml.size() + 1);
    if (ErrBuffer && ErrBufferSize > 0) {
        ErrBuffer[0] = '\0';
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
