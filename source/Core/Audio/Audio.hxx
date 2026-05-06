#pragma once

#include "Solstice.hxx"
#include <Core/Audio/Ambisonics/AmbisonicCodec.hxx>
#include <Core/Audio/FluidCoupling/FluidAudioCoupling.hxx>
#include <Core/Audio/HRTF/HRTFRenderer.hxx>
#include <Core/Audio/MLSpatialization/MLSpatialization.hxx>
#include <Core/Audio/PortalAudio/PortalAudio.hxx>
#include <Core/Audio/WaveTracing/WaveTracer.hxx>
#include <Core/System/Async.hxx>
#include <Math/Matrix.hxx>
#include <Math/Vector.hxx>
#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <cstdint>

// Forward declarations for SDL_mixer types
typedef struct MIX_Mixer MIX_Mixer;
typedef struct MIX_Audio MIX_Audio;
typedef struct MIX_Track MIX_Track;

namespace Solstice::Core::Audio {

    struct ReverbParams {
        float Decay; // 0.0 - 10.0 seconds
        float Wet;   // 0.0 - 1.0 volume
        float Dry;   // 0.0 - 1.0 volume
    };

    enum class ReverbPresetType {
        None,
        Room,
        Cave,
        Hallway,
        Sewer,
        Industrial,
        COUNT
    };

    enum class DistanceModel : uint8_t {
        Linear,
        Inverse,
        Exponential
    };

    struct Listener {
        Math::Vec3 Position;
        Math::Vec3 Forward;
        Math::Vec3 Up;
        ReverbParams CurrentReverb;
        ReverbPresetType TargetReverb;
    };

    struct AcousticZone {
        std::string Name;
        Math::Vec3 Center;
        Math::Vec3 Extents{5.0f, 5.0f, 5.0f}; // If spherical, x is radius.
        ReverbPresetType Preset{ReverbPresetType::Room};
        float Wetness{0.35f};
        float ObstructionMultiplier{1.0f};
        int Priority{0};
        bool IsSpherical{false};
        bool Enabled{true};
        /// Relative asset path (e.g. under path table / assets). Empty = do not drive music from this zone.
        std::string MusicPath;
        /// Optional looping 3D emitter at `Center` for ambient bed. Empty = none.
        std::string AmbiencePath;
    };

    struct AudioSource {
        Math::Vec3 Position;
        Math::Vec3 PreviousPosition;
        Math::Vec3 Direction;
        Math::Vec3 Velocity;
        float MinDistance;     // Distance at which attenuation begins.
        float MaxDistance;     // Distance at which sound is inaudible
        float RolloffFactor;   // How quickly volume decreases with distance (1.0 = inverse square)
        float OcclusionFactor; // 0.0 = unoccluded, 1.0 = fully occluded
        float ObstructionFactor; // Coarse obstruction separate from occlusion ray checks.
        float PitchVariance;   // Random pitch deviation (e.g., 0.02 for +/- 2%)
        float BasePitchRatio;  // Source-local base pitch ratio after variance jitter.
        float DopplerFactor;   // OpenAL-inspired strength multiplier.
        float SpeedOfSound;    // Units/second for doppler calculations.
        float AirAbsorptionFactor; // Additional high-frequency style falloff over distance.
        float ConeInnerAngleDeg;   // Fully audible inner cone angle.
        float ConeOuterAngleDeg;   // Outer cone angle where outer gain is reached.
        float ConeOuterGain;       // Gain when listener is outside outer cone.
        float FocusFactor;         // Listener-forward intelligibility shaping (0.0 - 1.0).
        float NearFieldGain;       // Extra gain when inside min distance.
        float MaxGain;             // Safety cap to avoid runaway gain stacking.
        float DopplerSmoothing;    // Seconds for doppler/frequency smoothing.
        float ImmersionFactor;     // Enables rear/side spatial depth shaping.
        float DistanceReverbFactor;// Adds distance-driven wetness bloom.
        float RearReverbFactor;    // Adds rear hemisphere wetness bloom.
        float DiffractionFactor;   // Edge bleed when occluded (0.0 - 1.0).
        float OcclusionPitchDamping; // Pitch/frequency damping proxy for muffling.
        float AdaptiveMotionFactor;  // Listener-motion adaptive ambience/spread response.
        float WetLevel;        // 0.0 - 1.0 reverb send target.
        float DryLevel;        // 0.0 - 1.0 dry signal level.
        float OcclusionAttack; // Seconds to react when occlusion rises.
        float OcclusionRelease;// Seconds to recover when occlusion falls.
        float WetAttack;       // Seconds to react when wetness rises.
        float WetRelease;      // Seconds to recover when wetness falls.
        float CurrentOcclusion;// Runtime-smoothed occlusion.
        float CurrentWetLevel; // Runtime-smoothed wet level.
        float CurrentFrequencyRatio; // Runtime-smoothed doppler+pitch ratio.
        float Volume;          // Volume multiplier (0.0 - 1.0, default 1.0)
        int Priority;          // Higher priority gets clarity preference.
        DistanceModel DistanceMode;
        bool IsDialogue;
        bool IsCriticalCue;
        bool IsLooping;
        bool HasManualVelocity;
        bool HasPreviousPosition;
        MIX_Track* Track;      // SDL_mixer track assigned (nullptr if not playing)
    };

    using AudioEmitterHandle = uint64_t;

    class SOLSTICE_API AudioManager {
    public:
        static AudioManager& Instance() {
            static AudioManager instance;
            return instance;
        }

        // Core
        void Initialize(int Frequency = 44100, int Channels = 2, int ChunkSize = 2048);
        void Shutdown();
        void Update(float Dt);
        bool IsInitialized() const { return m_Initialized; }

        // Music (Streaming)
        void PlayMusic(const char* Path, int Loops = -1);
        void PauseMusic();
        void ResumeMusic();
        void StopMusic();
        void FadeInMusic(const char* Path, int Ms, int Loops = -1);
        void FadeOutMusic(int Ms);
        void SetMusicVolume(float Volume); // 0.0 - 1.0
        void SetMasterVolume(float Volume); // 0.0 - 1.0

        // SFX (Chunked)
        void PlaySound(const char* Path, int Loops = 0);
        void SetSoundVolume(float Volume); // 0.0 - 1.0

        // Spatial Audio
        void SetListener(const Listener& ListenerData);
        const Listener& GetListener() const;

        // Plays a sound at a 3D position and returns a handle to the source
        AudioSource PlaySound3D(const char* Path, const Math::Vec3& Position,
                                float MaxDistance = 50.0f, bool Loop = false);

        // Updates the spatial properties of an active audio source
        void UpdateAudioSource(AudioSource& Source);

        // Stops an audio source
        void StopAudioSource(AudioSource& Source);

        // Managed 3D emitters for scripting/C API and gameplay systems.
        AudioEmitterHandle CreateEmitter(const char* Path, const Math::Vec3& Position,
                                         float MaxDistance = 50.0f, bool Loop = false);
        bool UpdateEmitterTransform(AudioEmitterHandle Handle, const Math::Vec3& Position);
        bool SetEmitterVolume(AudioEmitterHandle Handle, float Volume);
        bool SetEmitterOcclusion(AudioEmitterHandle Handle, float Occlusion);
        bool SetEmitterRolloff(AudioEmitterHandle Handle, float MinDistance, float MaxDistance, float RolloffFactor,
                               DistanceModel Model = DistanceModel::Inverse);
        bool SetEmitterDirection(AudioEmitterHandle Handle, const Math::Vec3& Direction);
        bool SetEmitterCone(AudioEmitterHandle Handle, float InnerAngleDeg, float OuterAngleDeg, float OuterGain);
        bool SetEmitterDoppler(AudioEmitterHandle Handle, float DopplerFactor, float SpeedOfSound);
        bool SetEmitterVelocity(AudioEmitterHandle Handle, const Math::Vec3& Velocity, bool Manual = true);
        bool ClearEmitterVelocity(AudioEmitterHandle Handle);
        bool SetEmitterFocus(AudioEmitterHandle Handle, float FocusFactor, float NearFieldGain = 0.0f, float MaxGain = 1.0f);
        bool SetEmitterImmersion(AudioEmitterHandle Handle, float ImmersionFactor, float DistanceReverbFactor, float RearReverbFactor);
        bool SetEmitterDiffraction(AudioEmitterHandle Handle, float DiffractionFactor, float OcclusionPitchDamping);
        bool SetEmitterMotionAdaptation(AudioEmitterHandle Handle, float AdaptiveMotionFactor);
        bool SetEmitterAirAbsorption(AudioEmitterHandle Handle, float AirAbsorptionFactor);
        bool SetEmitterPitchVariance(AudioEmitterHandle Handle, float PitchVariance);
        bool SetEmitterFlags(AudioEmitterHandle Handle, bool IsDialogue, bool IsCriticalCue, int Priority = 0);
        bool ApplySpatialProfile(AudioEmitterHandle Handle, int Profile);
        bool GetEmitterSnapshot(AudioEmitterHandle Handle, AudioSource& OutSource) const;
        bool DestroyEmitter(AudioEmitterHandle Handle);
        bool IsEmitterValid(AudioEmitterHandle Handle) const;
        void UpdateEmitters(float Dt);

        // Supercharged feature toggles/config
        void SetHRTFEnabled(bool Enabled);
        bool IsHRTFEnabled() const { return m_HRTFEnabled; }
        bool LoadHRTFDatabase(const char* Path);
        void SetHeadRadius(float Radius);
        void SetWaveTracingEnabled(bool Enabled);
        bool IsWaveTracingEnabled() const { return m_WaveTracingEnabled; }
        void SetWaveTracingMaxRays(int PerSource);
        void SetWaveTracingMaxBounces(int Bounces);
        void SetAmbisonicOrder(int Order);
        int GetAmbisonicOrder() const;
        void SetFluidCouplingEnabled(bool Enabled);
        bool IsFluidCouplingEnabled() const { return m_FluidCouplingEnabled; }
        void SetPortalTransform(const Math::Matrix4& Transform, bool Enabled);
        void ClearPortalTransform();
        void SetMLSpatializationEnabled(bool Enabled);
        bool IsMLSpatializationEnabled() const { return m_MLSpatializationEnabled; }
        void TrainMLModel();
        bool SaveMLWeights(const char* Path);
        bool LoadMLWeights(const char* Path);

        // Reverb
        void SetReverbPreset(ReverbPresetType Preset);
        void SetAcousticZones(const std::vector<AcousticZone>& Zones);
        void ClearAcousticZones();
        std::optional<AcousticZone> GetActiveListenerZone() const;

    private:
        AudioManager() = default;
        ~AudioManager();

        MIX_Audio* LoadAudio(const char* Path);

        // Internal state
        bool m_Initialized{ false };
        Listener m_Listener;
        MIX_Mixer* m_Mixer{ nullptr };
        MIX_Track* m_MusicTrack{ nullptr };

        // Resources
        std::unordered_map<std::string, MIX_Audio*> m_CachedAudio;
        std::unordered_map<AudioEmitterHandle, AudioSource> m_Emitters;
        AudioEmitterHandle m_NextEmitterHandle{1};
        std::vector<AcousticZone> m_AcousticZones;
        float m_ListenerZoneWetness{0.0f};
        float m_DialogueDuckingStrength{0.25f};
        float m_CriticalCueDuckingStrength{0.15f};
        Math::Vec3 m_PreviousListenerPosition{0.0f, 0.0f, 0.0f};
        bool m_HasPreviousListenerPosition{false};

        /// Per-map zone BGM/ambience (not mixed with ad-hoc `PlayMusic` / emitters for other features).
        std::string m_ZoneDrivenMusicPath;
        std::string m_ZoneDrivenAmbiencePath;
        AudioEmitterHandle m_ZoneAmbienceEmitter{0};

        // Supercharged subsystems
        HRTFRenderer m_HRTFRenderer;
        WaveTracer m_WaveTracer;
        AmbisonicCodec m_Ambisonics;
        FluidAudioCoupler m_FluidCoupler;
        PortalAudio m_PortalAudio;
        MLSpatializer m_MLSpatializer;
        bool m_HRTFEnabled{false};
        bool m_WaveTracingEnabled{false};
        bool m_FluidCouplingEnabled{false};
        bool m_MLSpatializationEnabled{false};

        // Thread safety
        mutable Spinlock m_Lock;

        // Internal helpers
        void ClearZoneDrivenMedia();
        void UpdateZoneDrivenMedia();
        static ReverbParams GetPresetParams(ReverbPresetType Preset);
        static float Clamp01(float V);
        static float ComputeDistanceAttenuation(const AudioSource& Source, float Distance);
        static float ComputeConeAttenuation(const AudioSource& Source, const Math::Vec3& ToListenerDir);
        static float ComputeDopplerRatio(const AudioSource& Source, const Math::Vec3& ListenerVelocity,
                                         const Math::Vec3& SourceVelocity, const Math::Vec3& ToSourceDir);
        static float ComputeFocusGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir);
        static float ComputeImmersionGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir);
        static float ComputeDiffractionGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir, float Occlusion01);
        static float ComputePriorityGain(const AudioSource& Source);
        static float SmoothTowards(float Current, float Target, float Attack, float Release, float Dt);
        static bool ContainsZonePoint(const AcousticZone& Zone, const Math::Vec3& Point);
        std::optional<AcousticZone> EvaluateZoneAt(const Math::Vec3& Point) const;
        void ApplySpatialization(AudioSource& Source, const Listener& ListenerData, float Dt, float DuckingFactor);
    };

} // namespace Solstice::Core::Audio
