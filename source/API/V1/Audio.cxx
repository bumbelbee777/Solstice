#include "SolsticeAPI/V1/Audio.h"
#include "Solstice.hxx"
#include "Core/Audio.hxx"

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioPlayMusic(const char* Path, int Loops) {
    if (!Path || !Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::AudioManager::Instance().PlayMusic(Path, Loops);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioStopMusic(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::AudioManager::Instance().StopMusic();
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioPlaySound(const char* Path, int Loops) {
    if (!Path || !Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::AudioManager::Instance().PlaySound(Path, Loops);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetMasterVolume(float Volume) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::AudioManager::Instance().SetMasterVolume(Volume);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioUpdate(float Dt) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::AudioManager::Instance().Update(Dt);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

} // extern "C"
