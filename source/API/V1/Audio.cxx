#include "SolsticeAPI/V1/Audio.h"
#include "Solstice.hxx"
#include "Core/Audio/Audio.hxx"

extern "C" {

namespace {
using EmitterHandle = Solstice::Core::Audio::AudioEmitterHandle;
}

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

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioCreateEmitter(
    const char* Path,
    float X, float Y, float Z,
    float MaxDistance,
    SolsticeV1_Bool Loop,
    SolsticeV1_AudioEmitterHandle* OutHandle
) {
    if (!Path || !OutHandle || !Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        EmitterHandle handle = Solstice::Core::Audio::AudioManager::Instance().CreateEmitter(
            Path,
            Solstice::Math::Vec3(X, Y, Z),
            MaxDistance,
            Loop == SolsticeV1_True
        );
        *OutHandle = static_cast<SolsticeV1_AudioEmitterHandle>(handle);
        return handle != 0 ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioUpdateEmitterTransform(
    SolsticeV1_AudioEmitterHandle Handle,
    float X, float Y, float Z
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().UpdateEmitterTransform(
            static_cast<EmitterHandle>(Handle),
            Solstice::Math::Vec3(X, Y, Z)
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterVolume(
    SolsticeV1_AudioEmitterHandle Handle,
    float Volume
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterVolume(
            static_cast<EmitterHandle>(Handle),
            Volume
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterOcclusion(
    SolsticeV1_AudioEmitterHandle Handle,
    float Occlusion
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterOcclusion(
            static_cast<EmitterHandle>(Handle),
            Occlusion
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioDestroyEmitter(SolsticeV1_AudioEmitterHandle Handle) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().DestroyEmitter(
            static_cast<EmitterHandle>(Handle)
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetListener(
    float Px, float Py, float Pz,
    float Fx, float Fy, float Fz,
    float Ux, float Uy, float Uz
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        Solstice::Core::Audio::Listener listener;
        listener.Position = Solstice::Math::Vec3(Px, Py, Pz);
        listener.Forward = Solstice::Math::Vec3(Fx, Fy, Fz);
        listener.Up = Solstice::Math::Vec3(Ux, Uy, Uz);
        listener.CurrentReverb = {0.0f, 0.0f, 1.0f};
        listener.TargetReverb = Solstice::Core::Audio::ReverbPresetType::Room;
        Solstice::Core::Audio::AudioManager::Instance().SetListener(listener);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetReverbPreset(int Preset) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        using PresetType = Solstice::Core::Audio::ReverbPresetType;
        PresetType preset = PresetType::Room;
        if (Preset >= static_cast<int>(PresetType::None) && Preset < static_cast<int>(PresetType::COUNT)) {
            preset = static_cast<PresetType>(Preset);
        }
        Solstice::Core::Audio::AudioManager::Instance().SetReverbPreset(preset);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

} // extern "C"
