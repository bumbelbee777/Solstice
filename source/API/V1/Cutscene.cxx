#include "SolsticeAPI/V1/Cutscene.h"
#include "../../Game/Cutscene/CutscenePlayer.hxx"

#include <algorithm>
#include <cstring>
#include <string>

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_CutsceneValidateJSON(
    const char* Json,
    char* ErrBuffer,
    size_t ErrBufferSize) {
    if (!Json) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Game::CutscenePlayer Player;
    std::string Err;
    if (!Player.LoadFromJSONString(std::string(Json), Err)) {
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, Err.size());
            std::memcpy(ErrBuffer, Err.c_str(), L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    if (ErrBuffer && ErrBufferSize > 0) {
        ErrBuffer[0] = '\0';
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
