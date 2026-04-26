#include "SolsticeAPI/V1/Smf.h"
#include <Arzachel/MapSerializer.hxx>
#include <Smf/SmfBinary.hxx>
#include <Smf/SmfMap.hxx>
#include <Smf/SmfUtil.hxx>

#include <algorithm>
#include <cstring>
#include <span>

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmfValidateBinary(
    const void* Bytes,
    size_t ByteCount,
    char* ErrBuffer,
    size_t ErrBufferSize) {
    if (!Bytes || ByteCount == 0) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Smf::SmfMap Map;
    Solstice::Smf::SmfError Err = Solstice::Smf::SmfError::None;
    std::span<const std::byte> Span(static_cast<const std::byte*>(Bytes), ByteCount);
    if (!Solstice::Smf::LoadSmfFromBytes(Map, Span, nullptr, &Err) || Err != Solstice::Smf::SmfError::None) {
        const char* Msg = Solstice::Smf::SmfErrorMessage(Err);
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, std::strlen(Msg));
            std::memcpy(ErrBuffer, Msg, L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    if (ErrBuffer && ErrBufferSize > 0) {
        ErrBuffer[0] = '\0';
    }
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmfApplyGameplay(
    const void* Bytes,
    size_t ByteCount,
    char* ErrBuffer,
    size_t ErrBufferSize) {
    if (!Bytes || ByteCount == 0) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Smf::SmfMap Map;
    Solstice::Smf::SmfError Err = Solstice::Smf::SmfError::None;
    std::span<const std::byte> Span(static_cast<const std::byte*>(Bytes), ByteCount);
    if (!Solstice::Smf::LoadSmfFromBytes(Map, Span, nullptr, &Err) || Err != Solstice::Smf::SmfError::None) {
        const char* Msg = Solstice::Smf::SmfErrorMessage(Err);
        if (ErrBuffer && ErrBufferSize > 0) {
            size_t L = std::min(ErrBufferSize - 1, std::strlen(Msg));
            std::memcpy(ErrBuffer, Msg, L);
            ErrBuffer[L] = '\0';
        }
        return SolsticeV1_ResultFailure;
    }
    Solstice::Arzachel::MapSerializer::ApplyGameplayFromSmfMap(Map);
    if (ErrBuffer && ErrBufferSize > 0) {
        ErrBuffer[0] = '\0';
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
