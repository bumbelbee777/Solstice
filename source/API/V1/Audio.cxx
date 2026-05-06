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

SOLSTICE_V1_API void SolsticeV1_AudioSetHRTFEnabled(int Enabled) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetHRTFEnabled(Enabled != 0);
}

SOLSTICE_V1_API int SolsticeV1_AudioIsHRTFEnabled(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return 0;
    return Solstice::Core::Audio::AudioManager::Instance().IsHRTFEnabled() ? 1 : 0;
}

SOLSTICE_V1_API void SolsticeV1_AudioLoadHRTFDatabase(const char* Path) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().LoadHRTFDatabase(Path);
}

SOLSTICE_V1_API void SolsticeV1_AudioSetHeadRadius(float Radius) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetHeadRadius(Radius);
}

SOLSTICE_V1_API void SolsticeV1_AudioSetWaveTracingEnabled(int Enabled) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetWaveTracingEnabled(Enabled != 0);
}

SOLSTICE_V1_API int SolsticeV1_AudioIsWaveTracingEnabled(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return 0;
    return Solstice::Core::Audio::AudioManager::Instance().IsWaveTracingEnabled() ? 1 : 0;
}

SOLSTICE_V1_API void SolsticeV1_AudioSetMaxRays(int PerSource) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetWaveTracingMaxRays(PerSource);
}

SOLSTICE_V1_API void SolsticeV1_AudioSetMaxBounces(int Bounces) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetWaveTracingMaxBounces(Bounces);
}

SOLSTICE_V1_API void SolsticeV1_AudioSetAmbisonicOrder(int Order) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetAmbisonicOrder(Order);
}

SOLSTICE_V1_API int SolsticeV1_AudioGetAmbisonicOrder(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return 1;
    return Solstice::Core::Audio::AudioManager::Instance().GetAmbisonicOrder();
}

SOLSTICE_V1_API void SolsticeV1_AudioSetFluidCouplingEnabled(int Enabled) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetFluidCouplingEnabled(Enabled != 0);
}

SOLSTICE_V1_API int SolsticeV1_AudioIsFluidCouplingEnabled(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return 0;
    return Solstice::Core::Audio::AudioManager::Instance().IsFluidCouplingEnabled() ? 1 : 0;
}

SOLSTICE_V1_API void SolsticeV1_AudioSetPortalTransform(const float* Matrix, int Enabled) {
    if (!Solstice::Initialized.load(std::memory_order_acquire) || !Matrix) return;
    Solstice::Math::Matrix4 m{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            m.M[row][col] = Matrix[row * 4 + col];
        }
    }
    Solstice::Core::Audio::AudioManager::Instance().SetPortalTransform(m, Enabled != 0);
}

SOLSTICE_V1_API void SolsticeV1_AudioClearPortalTransform(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().ClearPortalTransform();
}

SOLSTICE_V1_API void SolsticeV1_AudioSetMLSpatializationEnabled(int Enabled) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().SetMLSpatializationEnabled(Enabled != 0);
}

SOLSTICE_V1_API int SolsticeV1_AudioIsMLSpatializationEnabled(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return 0;
    return Solstice::Core::Audio::AudioManager::Instance().IsMLSpatializationEnabled() ? 1 : 0;
}

SOLSTICE_V1_API void SolsticeV1_AudioTrainMLModel(void) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) return;
    Solstice::Core::Audio::AudioManager::Instance().TrainMLModel();
}

SOLSTICE_V1_API void SolsticeV1_AudioSaveMLWeights(const char* Path) {
    if (!Solstice::Initialized.load(std::memory_order_acquire) || !Path) return;
    Solstice::Core::Audio::AudioManager::Instance().SaveMLWeights(Path);
}

SOLSTICE_V1_API void SolsticeV1_AudioLoadMLWeights(const char* Path) {
    if (!Solstice::Initialized.load(std::memory_order_acquire) || !Path) return;
    Solstice::Core::Audio::AudioManager::Instance().LoadMLWeights(Path);
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

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterRolloff(
    SolsticeV1_AudioEmitterHandle Handle,
    float MinDistance,
    float MaxDistance,
    float RolloffFactor,
    int DistanceModel
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        using Model = Solstice::Core::Audio::DistanceModel;
        Model model = Model::Inverse;
        if (DistanceModel >= static_cast<int>(Model::Linear) && DistanceModel <= static_cast<int>(Model::Exponential)) {
            model = static_cast<Model>(DistanceModel);
        }
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterRolloff(
            static_cast<EmitterHandle>(Handle),
            MinDistance,
            MaxDistance,
            RolloffFactor,
            model
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDirection(
    SolsticeV1_AudioEmitterHandle Handle,
    float Dx, float Dy, float Dz
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterDirection(
            static_cast<EmitterHandle>(Handle),
            Solstice::Math::Vec3(Dx, Dy, Dz)
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterCone(
    SolsticeV1_AudioEmitterHandle Handle,
    float InnerAngleDeg,
    float OuterAngleDeg,
    float OuterGain
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterCone(
            static_cast<EmitterHandle>(Handle),
            InnerAngleDeg,
            OuterAngleDeg,
            OuterGain
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDoppler(
    SolsticeV1_AudioEmitterHandle Handle,
    float DopplerFactor,
    float SpeedOfSound
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterDoppler(
            static_cast<EmitterHandle>(Handle),
            DopplerFactor,
            SpeedOfSound
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterVelocity(
    SolsticeV1_AudioEmitterHandle Handle,
    float Vx, float Vy, float Vz,
    SolsticeV1_Bool Manual
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterVelocity(
            static_cast<EmitterHandle>(Handle),
            Solstice::Math::Vec3(Vx, Vy, Vz),
            Manual == SolsticeV1_True
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioClearEmitterVelocity(
    SolsticeV1_AudioEmitterHandle Handle
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().ClearEmitterVelocity(
            static_cast<EmitterHandle>(Handle)
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterFocus(
    SolsticeV1_AudioEmitterHandle Handle,
    float FocusFactor,
    float NearFieldGain,
    float MaxGain
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterFocus(
            static_cast<EmitterHandle>(Handle),
            FocusFactor,
            NearFieldGain,
            MaxGain
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterImmersion(
    SolsticeV1_AudioEmitterHandle Handle,
    float ImmersionFactor,
    float DistanceReverbFactor,
    float RearReverbFactor
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterImmersion(
            static_cast<EmitterHandle>(Handle),
            ImmersionFactor,
            DistanceReverbFactor,
            RearReverbFactor
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterDiffraction(
    SolsticeV1_AudioEmitterHandle Handle,
    float DiffractionFactor,
    float OcclusionPitchDamping
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterDiffraction(
            static_cast<EmitterHandle>(Handle),
            DiffractionFactor,
            OcclusionPitchDamping
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterMotionAdaptation(
    SolsticeV1_AudioEmitterHandle Handle,
    float AdaptiveMotionFactor
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterMotionAdaptation(
            static_cast<EmitterHandle>(Handle),
            AdaptiveMotionFactor
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterAirAbsorption(
    SolsticeV1_AudioEmitterHandle Handle,
    float AirAbsorptionFactor
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterAirAbsorption(
            static_cast<EmitterHandle>(Handle),
            AirAbsorptionFactor
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterPitchVariance(
    SolsticeV1_AudioEmitterHandle Handle,
    float PitchVariance
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterPitchVariance(
            static_cast<EmitterHandle>(Handle),
            PitchVariance
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioSetEmitterFlags(
    SolsticeV1_AudioEmitterHandle Handle,
    SolsticeV1_Bool IsDialogue,
    SolsticeV1_Bool IsCriticalCue,
    int Priority
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        bool ok = Solstice::Core::Audio::AudioManager::Instance().SetEmitterFlags(
            static_cast<EmitterHandle>(Handle),
            IsDialogue == SolsticeV1_True,
            IsCriticalCue == SolsticeV1_True,
            Priority
        );
        return ok ? SolsticeV1_ResultSuccess : SolsticeV1_ResultFailure;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_AudioApplySpatialProfile(
    SolsticeV1_AudioEmitterHandle Handle,
    int Profile
) {
    if (!Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        auto& audio = Solstice::Core::Audio::AudioManager::Instance();
        const EmitterHandle handle = static_cast<EmitterHandle>(Handle);
        bool ok = true;
        switch (Profile) {
            case 1: // AmbientBed
                ok = audio.SetEmitterFocus(handle, 0.05f, 0.03f, 0.95f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.75f, 0.65f, 0.45f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.65f, 0.20f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.90f) && ok;
                ok = audio.SetEmitterRolloff(handle, 2.0f, 160.0f, 0.4f, Solstice::Core::Audio::DistanceModel::Linear) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.12f) && ok;
                ok = audio.SetEmitterDoppler(handle, 0.15f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 360.0f, 360.0f, 1.0f) && ok;
                break;
            case 2: // Footstep
                ok = audio.SetEmitterFocus(handle, 0.30f, 0.07f, 1.0f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.45f, 0.30f, 0.18f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.35f, 0.10f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.35f) && ok;
                ok = audio.SetEmitterRolloff(handle, 0.7f, 24.0f, 1.2f, Solstice::Core::Audio::DistanceModel::Inverse) && ok;
                ok = audio.SetEmitterPitchVariance(handle, 0.05f) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.45f) && ok;
                ok = audio.SetEmitterDoppler(handle, 0.7f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 300.0f, 360.0f, 0.9f) && ok;
                break;
            case 3: // Weapon
                ok = audio.SetEmitterFocus(handle, 0.55f, 0.10f, 1.05f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.60f, 0.50f, 0.28f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.55f, 0.16f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.40f) && ok;
                ok = audio.SetEmitterRolloff(handle, 1.2f, 180.0f, 1.0f, Solstice::Core::Audio::DistanceModel::Inverse) && ok;
                ok = audio.SetEmitterPitchVariance(handle, 0.02f) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.28f) && ok;
                ok = audio.SetEmitterDoppler(handle, 1.0f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 85.0f, 180.0f, 0.45f) && ok;
                break;
            case 4: // Voice
                ok = audio.SetEmitterFocus(handle, 0.85f, 0.12f, 1.1f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.70f, 0.42f, 0.35f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.50f, 0.22f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.30f) && ok;
                ok = audio.SetEmitterRolloff(handle, 0.8f, 42.0f, 1.35f, Solstice::Core::Audio::DistanceModel::Inverse) && ok;
                ok = audio.SetEmitterPitchVariance(handle, 0.01f) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.33f) && ok;
                ok = audio.SetEmitterDoppler(handle, 0.85f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 60.0f, 140.0f, 0.35f) && ok;
                ok = audio.SetEmitterFlags(handle, true, false, 2) && ok;
                break;
            case 5: // Vehicle
                ok = audio.SetEmitterFocus(handle, 0.35f, 0.08f, 1.05f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.85f, 0.58f, 0.42f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.70f, 0.18f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.75f) && ok;
                ok = audio.SetEmitterRolloff(handle, 2.5f, 260.0f, 0.9f, Solstice::Core::Audio::DistanceModel::Exponential) && ok;
                ok = audio.SetEmitterPitchVariance(handle, 0.015f) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.22f) && ok;
                ok = audio.SetEmitterDoppler(handle, 1.2f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 100.0f, 220.0f, 0.5f) && ok;
                break;
            case 0: // Default
            default:
                ok = audio.SetEmitterFocus(handle, 0.15f, 0.05f, 1.0f) && ok;
                ok = audio.SetEmitterImmersion(handle, 0.35f, 0.40f, 0.25f) && ok;
                ok = audio.SetEmitterDiffraction(handle, 0.35f, 0.12f) && ok;
                ok = audio.SetEmitterMotionAdaptation(handle, 0.20f) && ok;
                ok = audio.SetEmitterRolloff(handle, 1.0f, 50.0f, 1.0f, Solstice::Core::Audio::DistanceModel::Inverse) && ok;
                ok = audio.SetEmitterPitchVariance(handle, 0.03f) && ok;
                ok = audio.SetEmitterAirAbsorption(handle, 0.25f) && ok;
                ok = audio.SetEmitterDoppler(handle, 1.0f, 343.3f) && ok;
                ok = audio.SetEmitterCone(handle, 360.0f, 360.0f, 1.0f) && ok;
                break;
        }
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
