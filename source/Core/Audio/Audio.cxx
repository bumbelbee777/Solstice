#include "Audio.hxx"
#include <Core/Debug/Debug.hxx>
#include <Asset/Loading/AssetLoader.hxx>
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <filesystem>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Solstice::Core::Audio {

    namespace {
        constexpr float kDefaultDt = 1.0f / 60.0f;
        constexpr float kMaxDialogueDuckingDb = 4.0f;
        constexpr float kMaxCriticalCueDuckingDb = 2.5f;
        constexpr float kMinDialogueToSfxRatio = 1.35f;
        constexpr float kTargetOcclusionAttackSec = 0.08f;
        constexpr float kTargetOcclusionReleaseSec = 0.25f;
        constexpr float kTargetWetAttackSec = 0.18f;
        constexpr float kTargetWetReleaseSec = 0.35f;

        float DbToLinear(float db) {
            return std::pow(10.0f, db / 20.0f);
        }

        float RandRange(float minValue, float maxValue) {
            static thread_local std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> dist(minValue, maxValue);
            return dist(rng);
        }
    }

    AudioManager::~AudioManager() {
        Shutdown();
    }

    float AudioManager::Clamp01(float V) {
        return std::clamp(V, 0.0f, 1.0f);
    }

    ReverbParams AudioManager::GetPresetParams(ReverbPresetType Preset) {
        switch (Preset) {
            case ReverbPresetType::Room: return {1.1f, 0.30f, 0.90f};
            case ReverbPresetType::Cave: return {3.0f, 0.65f, 0.75f};
            case ReverbPresetType::Hallway: return {1.8f, 0.45f, 0.85f};
            case ReverbPresetType::Sewer: return {2.4f, 0.58f, 0.78f};
            case ReverbPresetType::Industrial: return {2.0f, 0.52f, 0.82f};
            case ReverbPresetType::None:
            default: return {0.0f, 0.0f, 1.0f};
        }
    }

    float AudioManager::SmoothTowards(float Current, float Target, float Attack, float Release, float Dt) {
        const float riseRate = (Attack > 0.0001f) ? (Dt / Attack) : 1.0f;
        const float fallRate = (Release > 0.0001f) ? (Dt / Release) : 1.0f;
        const float rate = (Target > Current) ? riseRate : fallRate;
        return Current + (Target - Current) * std::clamp(rate, 0.0f, 1.0f);
    }

    float AudioManager::ComputeDistanceAttenuation(const AudioSource& Source, float Distance) {
        const float minDist = std::max(0.01f, Source.MinDistance);
        const float maxDist = std::max(minDist + 0.01f, Source.MaxDistance);
        const float d = std::clamp(Distance, minDist, maxDist);
        const float rolloff = std::max(0.0001f, Source.RolloffFactor);
        switch (Source.DistanceMode) {
            case DistanceModel::Linear: {
                const float t = (d - minDist) / (maxDist - minDist);
                return 1.0f - t;
            }
            case DistanceModel::Exponential: {
                return std::pow(d / minDist, -rolloff);
            }
            case DistanceModel::Inverse:
            default: {
                return minDist / (minDist + rolloff * (d - minDist));
            }
        }
    }

    float AudioManager::ComputeConeAttenuation(const AudioSource& Source, const Math::Vec3& ToListenerDir) {
        const float innerDeg = std::clamp(Source.ConeInnerAngleDeg, 0.0f, 360.0f);
        const float outerDeg = std::clamp(std::max(innerDeg, Source.ConeOuterAngleDeg), 0.0f, 360.0f);
        if (outerDeg >= 359.9f) {
            return 1.0f;
        }

        Math::Vec3 direction = Source.Direction;
        if (direction.Dot(direction) < 0.0001f) {
            direction = Math::Vec3(0.0f, 0.0f, -1.0f);
        } else {
            direction = direction.Normalized();
        }

        const Math::Vec3 toListener = (ToListenerDir.Dot(ToListenerDir) < 0.0001f)
            ? Math::Vec3(0.0f, 0.0f, -1.0f)
            : ToListenerDir.Normalized();
        const float cosTheta = std::clamp(direction.Dot(toListener), -1.0f, 1.0f);
        const float thetaDeg = std::acos(cosTheta) * (180.0f / static_cast<float>(M_PI));
        const float halfInner = innerDeg * 0.5f;
        const float halfOuter = outerDeg * 0.5f;
        const float outerGain = Clamp01(Source.ConeOuterGain);

        if (thetaDeg <= halfInner) {
            return 1.0f;
        }
        if (thetaDeg >= halfOuter) {
            return outerGain;
        }
        const float t = (thetaDeg - halfInner) / std::max(0.001f, halfOuter - halfInner);
        return std::lerp(1.0f, outerGain, t);
    }

    float AudioManager::ComputeDopplerRatio(const AudioSource& Source, const Math::Vec3& ListenerVelocity,
                                            const Math::Vec3& SourceVelocity, const Math::Vec3& ToSourceDir) {
        const float speedOfSound = std::max(0.1f, Source.SpeedOfSound);
        const float dopplerFactor = std::max(0.0f, Source.DopplerFactor);
        if (dopplerFactor <= 0.0001f) {
            return 1.0f;
        }

        const Math::Vec3 dir = (ToSourceDir.Dot(ToSourceDir) < 0.0001f)
            ? Math::Vec3(0.0f, 0.0f, -1.0f)
            : ToSourceDir.Normalized();
        const float listenerVel = ListenerVelocity.Dot(dir);
        const float sourceVel = SourceVelocity.Dot(dir);

        const float maxRelative = speedOfSound * 0.95f;
        const float clampedListener = std::clamp(listenerVel * dopplerFactor, -maxRelative, maxRelative);
        const float clampedSource = std::clamp(sourceVel * dopplerFactor, -maxRelative, maxRelative);
        const float numerator = speedOfSound - clampedListener;
        const float denominator = std::max(0.05f, speedOfSound - clampedSource);
        return std::clamp(numerator / denominator, 0.5f, 2.0f);
    }

    float AudioManager::ComputeFocusGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir) {
        const float focus = Clamp01(Source.FocusFactor);
        if (focus <= 0.0001f) {
            return 1.0f;
        }

        Math::Vec3 listenerForward = ListenerData.Forward;
        if (listenerForward.Dot(listenerForward) < 0.0001f) {
            listenerForward = Math::Vec3(0.0f, 0.0f, -1.0f);
        } else {
            listenerForward = listenerForward.Normalized();
        }
        const Math::Vec3 sourceDir = (ToSourceDir.Dot(ToSourceDir) < 0.0001f)
            ? Math::Vec3(0.0f, 0.0f, -1.0f)
            : ToSourceDir.Normalized();
        const float facing = std::clamp(listenerForward.Dot(sourceDir), -1.0f, 1.0f);
        const float frontWeight = std::clamp((facing + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float clarityGain = std::lerp(0.85f, 1.15f, frontWeight);
        return std::lerp(1.0f, clarityGain, focus);
    }

    float AudioManager::ComputePriorityGain(const AudioSource& Source) {
        const float priorityNorm = std::clamp(static_cast<float>(Source.Priority) / 6.0f, -1.0f, 1.0f);
        float gain = 1.0f + 0.18f * priorityNorm;
        if (Source.IsDialogue) {
            gain *= 1.08f;
        }
        if (Source.IsCriticalCue) {
            gain *= 1.06f;
        }
        return std::clamp(gain, 0.75f, 1.35f);
    }

    float AudioManager::ComputeImmersionGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir) {
        const float immersion = Clamp01(Source.ImmersionFactor);
        if (immersion <= 0.0001f) {
            return 1.0f;
        }

        Math::Vec3 forward = ListenerData.Forward;
        Math::Vec3 up = ListenerData.Up;
        if (forward.Dot(forward) < 0.0001f) forward = Math::Vec3(0.0f, 0.0f, -1.0f);
        if (up.Dot(up) < 0.0001f) up = Math::Vec3(0.0f, 1.0f, 0.0f);
        forward = forward.Normalized();
        up = up.Normalized();
        Math::Vec3 right = forward.Cross(up);
        if (right.Dot(right) < 0.0001f) {
            right = Math::Vec3(1.0f, 0.0f, 0.0f);
        } else {
            right = right.Normalized();
        }

        const Math::Vec3 dir = (ToSourceDir.Dot(ToSourceDir) < 0.0001f)
            ? Math::Vec3(0.0f, 0.0f, -1.0f)
            : ToSourceDir.Normalized();
        const float front = std::clamp(forward.Dot(dir), -1.0f, 1.0f);
        const float side = std::abs(std::clamp(right.Dot(dir), -1.0f, 1.0f));
        const float rearWeight = std::clamp((-front + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float sideWeight = side;
        const float gain = 1.0f - 0.10f * rearWeight + 0.06f * sideWeight;
        return std::lerp(1.0f, gain, immersion);
    }

    float AudioManager::ComputeDiffractionGain(const AudioSource& Source, const Listener& ListenerData, const Math::Vec3& ToSourceDir, float Occlusion01) {
        const float diffraction = Clamp01(Source.DiffractionFactor);
        const float occlusion = Clamp01(Occlusion01);
        if (diffraction <= 0.0001f || occlusion <= 0.0001f) {
            return 1.0f;
        }

        Math::Vec3 forward = ListenerData.Forward;
        if (forward.Dot(forward) < 0.0001f) {
            forward = Math::Vec3(0.0f, 0.0f, -1.0f);
        } else {
            forward = forward.Normalized();
        }
        const Math::Vec3 dir = (ToSourceDir.Dot(ToSourceDir) < 0.0001f)
            ? Math::Vec3(0.0f, 0.0f, -1.0f)
            : ToSourceDir.Normalized();
        const float facing = std::clamp(forward.Dot(dir), -1.0f, 1.0f);
        const float edgeWeight = 1.0f - std::abs(facing); // strongest at listener side.
        const float bleed = occlusion * diffraction * edgeWeight;
        return 1.0f + 0.45f * bleed;
    }

    bool AudioManager::ContainsZonePoint(const AcousticZone& Zone, const Math::Vec3& Point) {
        if (!Zone.Enabled) {
            return false;
        }
        const Math::Vec3 delta = Point - Zone.Center;
        if (Zone.IsSpherical) {
            const float radius = std::max(0.01f, Zone.Extents.x);
            return delta.Dot(delta) <= radius * radius;
        }
        return std::abs(delta.x) <= Zone.Extents.x
            && std::abs(delta.y) <= Zone.Extents.y
            && std::abs(delta.z) <= Zone.Extents.z;
    }

    std::optional<AcousticZone> AudioManager::EvaluateZoneAt(const Math::Vec3& Point) const {
        std::optional<AcousticZone> bestZone;
        int bestPriority = std::numeric_limits<int>::min();
        for (const AcousticZone& zone : m_AcousticZones) {
            if (!ContainsZonePoint(zone, Point)) {
                continue;
            }
            if (zone.Priority >= bestPriority) {
                bestPriority = zone.Priority;
                bestZone = zone;
            }
        }
        return bestZone;
    }

    void AudioManager::Initialize(int Frequency, int Channels, int ChunkSize) {
        LockGuard Guard(m_Lock);
        if (m_Initialized) return;

        if (!SDL_Init(SDL_INIT_AUDIO)) {
            SOLSTICE_LOG("Failed to initialize SDL Audio: ", SDL_GetError());
            return;
        }

        if (!MIX_Init()) {
            SOLSTICE_LOG("Failed to initialize SDL_mixer: ", SDL_GetError());
            return;
        }

        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.format = SDL_AUDIO_S16;
        spec.channels = Channels;
        spec.freq = Frequency;

        m_Mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (!m_Mixer) {
            SOLSTICE_LOG("Failed to create mixer: ", SDL_GetError());
            return;
        }

        // Gameplay-clarity tuning targets:
        // - dialogue ducking ceiling ~= -4 dB
        // - critical cue ducking ceiling ~= -2.5 dB
        m_DialogueDuckingStrength = 1.0f - DbToLinear(-kMaxDialogueDuckingDb);
        m_CriticalCueDuckingStrength = 1.0f - DbToLinear(-kMaxCriticalCueDuckingDb);
        m_HRTFRenderer.Initialize(Frequency, 128);
        m_Ambisonics.SetOrder(AmbisonicOrder::First);
        m_WaveTracer.Initialize(nullptr);
        m_WaveTracer.SetPortalAudio(&m_PortalAudio);
        m_WaveTracer.SetFluidCoupler(&m_FluidCoupler);
        m_MLSpatializer.Initialize();
        m_Initialized = true;
        SOLSTICE_LOG("Audio Subsystem Initialized");
    }

    void AudioManager::ClearZoneDrivenMedia() {
        if (!m_ZoneDrivenMusicPath.empty()) {
            StopMusic();
            m_ZoneDrivenMusicPath.clear();
        }
        if (m_ZoneAmbienceEmitter != 0) {
            const AudioEmitterHandle h = m_ZoneAmbienceEmitter;
            m_ZoneAmbienceEmitter = 0;
            m_ZoneDrivenAmbiencePath.clear();
            DestroyEmitter(h);
        } else {
            m_ZoneDrivenAmbiencePath.clear();
        }
    }

    void AudioManager::UpdateZoneDrivenMedia() {
        std::optional<AcousticZone> z;
        {
            LockGuard Guard(m_Lock);
            z = EvaluateZoneAt(m_Listener.Position);
        }
        if (!z) {
            if (!m_ZoneDrivenMusicPath.empty()) {
                StopMusic();
                m_ZoneDrivenMusicPath.clear();
            }
            if (m_ZoneAmbienceEmitter != 0) {
                const AudioEmitterHandle h = m_ZoneAmbienceEmitter;
                m_ZoneAmbienceEmitter = 0;
                m_ZoneDrivenAmbiencePath.clear();
                DestroyEmitter(h);
            }
            return;
        }
        if (z->MusicPath.empty()) {
            if (!m_ZoneDrivenMusicPath.empty()) {
                StopMusic();
                m_ZoneDrivenMusicPath.clear();
            }
        } else if (z->MusicPath != m_ZoneDrivenMusicPath) {
            PlayMusic(z->MusicPath.c_str(), -1);
            m_ZoneDrivenMusicPath = z->MusicPath;
        }
        if (z->AmbiencePath.empty()) {
            if (m_ZoneAmbienceEmitter != 0) {
                const AudioEmitterHandle h = m_ZoneAmbienceEmitter;
                m_ZoneAmbienceEmitter = 0;
                m_ZoneDrivenAmbiencePath.clear();
                DestroyEmitter(h);
            }
        } else if (z->AmbiencePath != m_ZoneDrivenAmbiencePath || m_ZoneAmbienceEmitter == 0) {
            if (m_ZoneAmbienceEmitter != 0) {
                const AudioEmitterHandle h = m_ZoneAmbienceEmitter;
                m_ZoneAmbienceEmitter = 0;
                DestroyEmitter(h);
            }
            m_ZoneDrivenAmbiencePath = z->AmbiencePath;
            m_ZoneAmbienceEmitter = CreateEmitter(z->AmbiencePath.c_str(), z->Center, 1.0e6f, true);
            if (m_ZoneAmbienceEmitter == 0) {
                m_ZoneDrivenAmbiencePath.clear();
            }
        } else {
            UpdateEmitterTransform(m_ZoneAmbienceEmitter, z->Center);
        }
    }

    void AudioManager::Shutdown() {
        LockGuard Guard(m_Lock);
        if (!m_Initialized) return;

        m_ZoneAmbienceEmitter = 0;
        m_ZoneDrivenMusicPath.clear();
        m_ZoneDrivenAmbiencePath.clear();

        // Destroy active emitter tracks before mixer teardown.
        for (auto& [_, source] : m_Emitters) {
            if (!source.Track) {
                continue;
            }
            try {
                MIX_StopTrack(source.Track, 0);
                MIX_DestroyTrack(source.Track);
            } catch (...) {
                // Best-effort cleanup on shutdown.
            }
            source.Track = nullptr;
        }
        m_Emitters.clear();

        if (m_MusicTrack) {
            try {
                MIX_StopTrack(m_MusicTrack, 0);
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Best-effort cleanup on shutdown.
            }
            m_MusicTrack = nullptr;
        }

        // Free audio
        for (auto& Pair : m_CachedAudio) {
            MIX_DestroyAudio(Pair.second);
        }
        m_CachedAudio.clear();

        if (m_Mixer) {
            MIX_DestroyMixer(m_Mixer);
            m_Mixer = nullptr;
        }

        m_HRTFRenderer.Shutdown();
        m_WaveTracer.Shutdown();
        m_MLSpatializer.Shutdown();

        MIX_Quit();
        m_Initialized = false;
    }

    void AudioManager::Update(float Dt) {
        if (Dt <= 0.0f) {
            Dt = kDefaultDt;
        }
        UpdateEmitters(Dt);

        // Smoothly converge listener reverb parameters.
        Listener listenerCopy;
        {
            LockGuard Guard(m_Lock);
            listenerCopy = m_Listener;
        }
        const ReverbParams target = GetPresetParams(listenerCopy.TargetReverb);
        listenerCopy.CurrentReverb.Decay = SmoothTowards(listenerCopy.CurrentReverb.Decay, target.Decay, 0.20f, 0.35f, Dt);
        listenerCopy.CurrentReverb.Wet = SmoothTowards(listenerCopy.CurrentReverb.Wet, target.Wet, 0.22f, 0.45f, Dt);
        listenerCopy.CurrentReverb.Dry = SmoothTowards(listenerCopy.CurrentReverb.Dry, target.Dry, 0.20f, 0.40f, Dt);
        {
            LockGuard Guard(m_Lock);
            m_Listener = listenerCopy;
        }

        UpdateZoneDrivenMedia();
    }

    MIX_Audio* AudioManager::LoadAudio(const char* Path) {
        std::string PathStr = Path;
        if (m_CachedAudio.find(PathStr) != m_CachedAudio.end()) {
            return m_CachedAudio[PathStr];
        }

        if (!m_Mixer) {
            SOLSTICE_LOG("WARNING: Cannot load audio - mixer not initialized: ", Path);
            return nullptr;
        }

        // Resolve path - use AssetLoader's resolution strategy for consistency
        std::filesystem::path pathObj(Path);
        std::filesystem::path resolvedPath;

        if (pathObj.is_absolute()) {
            resolvedPath = pathObj;
        } else {
            // Check if path already starts with "assets/" or "assets\"
            std::string pathStr = PathStr;
            std::string pathStrLower = pathStr;
            std::transform(pathStrLower.begin(), pathStrLower.end(), pathStrLower.begin(), ::tolower);
            bool hasAssetsPrefix = (pathStrLower.find("assets/") == 0 || pathStrLower.find("assets\\") == 0);

            if (hasAssetsPrefix) {
                // Strip "assets/" prefix and then prepend asset path (matches AssetLoader behavior)
                // This prevents path doubling when CWD is already in assets folder
                std::filesystem::path relativePath;
                if (pathStrLower.find("assets/") == 0) {
                    // Extract path after "assets/"
                    relativePath = pathStr.substr(7); // "assets/" is 7 characters
                } else {
                    // Extract path after "assets\"
                    relativePath = pathStr.substr(8); // "assets\" is 8 characters (backslash)
                }

                // Prepend asset path from AssetLoader
                std::filesystem::path assetPath = Core::AssetLoader::GetAssetPath();
                resolvedPath = assetPath / relativePath;
                SOLSTICE_LOG("Audio: Stripped 'assets/' prefix and resolved to: ", resolvedPath.string());
            } else {
                // Use AssetLoader's asset path
                std::filesystem::path assetPath = Core::AssetLoader::GetAssetPath();
                resolvedPath = assetPath / pathObj;
                SOLSTICE_LOG("Audio: Prepending asset path '", assetPath.string(), "' to '", PathStr, "'");
            }
        }

        std::string resolvedPathStr = resolvedPath.string();

        // Log path resolution for debugging
        if (resolvedPathStr != PathStr) {
            SOLSTICE_LOG("Audio: Resolved path '", Path, "' to '", resolvedPathStr, "'");
        }

        // Check if file exists
        if (!std::filesystem::exists(resolvedPath)) {
            SOLSTICE_LOG("WARNING: Audio file does not exist: ", resolvedPathStr);
            SOLSTICE_LOG("  Original path: ", Path);
            // Try original path as fallback
            if (std::filesystem::exists(PathStr)) {
                resolvedPathStr = PathStr;
                SOLSTICE_LOG("  Using original path instead: ", resolvedPathStr);
            }
        }

        MIX_Audio* Audio = nullptr;
        try {
            Audio = MIX_LoadAudio(m_Mixer, resolvedPathStr.c_str(), false);
        } catch (const std::exception& e) {
            SOLSTICE_LOG("WARNING: Exception in MIX_LoadAudio for: ", resolvedPathStr);
            SOLSTICE_LOG("  Exception: ", e.what());
            SOLSTICE_LOG("  Original path: ", Path);
            SOLSTICE_LOG("This is non-fatal - the game will continue without this audio file");
            return nullptr;
        } catch (...) {
            SOLSTICE_LOG("WARNING: Unknown exception in MIX_LoadAudio for: ", resolvedPathStr);
            SOLSTICE_LOG("  Original path: ", Path);
            SOLSTICE_LOG("This is non-fatal - the game will continue without this audio file");
            return nullptr;
        }

        if (!Audio) {
            const char* error = SDL_GetError();
            std::string errorStr = error ? error : "(no error message)";
            SOLSTICE_LOG("WARNING: Failed to load audio file: ", resolvedPathStr);
            SOLSTICE_LOG("  Original path: ", Path);
            SOLSTICE_LOG("  SDL Error: ", errorStr);

            // Log current working directory for debugging
            try {
                std::filesystem::path cwd = std::filesystem::current_path();
                SOLSTICE_LOG("  Current working directory: ", cwd.string());
            } catch (...) {
                SOLSTICE_LOG("  Could not determine working directory");
            }

            SOLSTICE_LOG("This is non-fatal - the game will continue without this audio file");
            return nullptr;
        }

        // Verify the file actually exists (MIX_LoadAudio might succeed even if file is invalid)
        if (!std::filesystem::exists(resolvedPath)) {
            SOLSTICE_LOG("WARNING: Audio file does not exist at resolved path: ", resolvedPathStr);
            SOLSTICE_LOG("  Original path: ", Path);
            SOLSTICE_LOG("  MIX_LoadAudio returned non-null, but file doesn't exist - this may cause issues");
        } else {
            SOLSTICE_LOG("Audio file loaded successfully: ", resolvedPathStr);
        }

        // Cache using original path string for consistency
        m_CachedAudio[PathStr] = Audio;
        return Audio;
    }

    void AudioManager::PlayMusic(const char* Path, int Loops) {
        LockGuard Guard(m_Lock);
        if (!m_Mixer) {
            SOLSTICE_LOG("WARNING: AudioManager mixer not initialized, cannot play music: ", Path);
            return;
        }

        MIX_Audio* Audio = LoadAudio(Path);
        if (!Audio) {
            SOLSTICE_LOG("WARNING: Failed to load music file: ", Path, " - music will not play");
            return;
        }

        // Log that we're attempting to set audio on track
        SOLSTICE_LOG("Setting audio on music track for: ", Path);

        // Clean up existing music track
        if (m_MusicTrack) {
            try {
                MIX_StopTrack(m_MusicTrack, 0);
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Track might already be invalid, continue anyway
            }
            m_MusicTrack = nullptr;
        }

        // Create new track with exception handling
        try {
            m_MusicTrack = MIX_CreateTrack(m_Mixer);
        } catch (const std::exception& e) {
            SOLSTICE_LOG("WARNING: Exception in MIX_CreateTrack for music: ", Path);
            SOLSTICE_LOG("  Exception: ", e.what());
            return;
        } catch (...) {
            SOLSTICE_LOG("WARNING: Unknown exception in MIX_CreateTrack for music: ", Path);
            return;
        }

        if (!m_MusicTrack) {
            const char* error = SDL_GetError();
            std::string errorStr = error ? error : "(no error message)";
            SOLSTICE_LOG("WARNING: Failed to create music track for: ", Path);
            SOLSTICE_LOG("  SDL Error: ", errorStr);
            return;
        }

        // Set audio on track with error checking and exception handling
        // Capture error immediately after call to prevent it from being cleared
        int setAudioResult = 0;
        try {
            setAudioResult = MIX_SetTrackAudio(m_MusicTrack, Audio);
        } catch (const std::exception& e) {
            SOLSTICE_LOG("WARNING: Exception in MIX_SetTrackAudio for music: ", Path);
            SOLSTICE_LOG("  Exception: ", e.what());
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
            return;
        } catch (...) {
            SOLSTICE_LOG("WARNING: Unknown exception in MIX_SetTrackAudio for music: ", Path);
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
            return;
        }

        // MIX_SetTrackAudio returns true (1) on success, false (0) on error
        if (setAudioResult == 0) {
            const char* error = SDL_GetError();
            std::string errorStr = error ? error : "(no error message)";
            SOLSTICE_LOG("WARNING: Failed to set track audio for music: ", Path);
            SOLSTICE_LOG("  MIX_SetTrackAudio returned: false (error)");
            SOLSTICE_LOG("  SDL Error: ", errorStr);
            if (errorStr == "(no error message)" || errorStr.empty()) {
                SOLSTICE_LOG("  Note: SDL error may have been cleared or not set");
            }
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
            return;
        }

        // Set loops and play with exception handling
        SDL_PropertiesID props = 0;
        try {
            props = SDL_CreateProperties();
        } catch (const std::exception& e) {
            SOLSTICE_LOG("WARNING: Exception in SDL_CreateProperties for music: ", Path);
            SOLSTICE_LOG("  Exception: ", e.what());
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
            return;
        } catch (...) {
            SOLSTICE_LOG("WARNING: Unknown exception in SDL_CreateProperties for music: ", Path);
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
            return;
        }

        if (props) {
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, Loops);
            int playResult = 0;
            try {
                playResult = MIX_PlayTrack(m_MusicTrack, props);
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in MIX_PlayTrack for music: ", Path);
                SOLSTICE_LOG("  Exception: ", e.what());
                try {
                    MIX_DestroyTrack(m_MusicTrack);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                m_MusicTrack = nullptr;
                SDL_DestroyProperties(props);
                return;
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in MIX_PlayTrack for music: ", Path);
                try {
                    MIX_DestroyTrack(m_MusicTrack);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                m_MusicTrack = nullptr;
                SDL_DestroyProperties(props);
                return;
            }

            // MIX_PlayTrack returns true (1) on success, false (0) on error
            if (playResult == 0) {
                const char* error = SDL_GetError();
                std::string errorStr = error ? error : "(no error message)";
                SOLSTICE_LOG("WARNING: Failed to play music track: ", Path);
                SOLSTICE_LOG("  SDL Error: ", errorStr);
                try {
                    MIX_DestroyTrack(m_MusicTrack);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                m_MusicTrack = nullptr;
            }
            SDL_DestroyProperties(props);
        } else {
            SOLSTICE_LOG("WARNING: Failed to create properties for music track: ", Path);
            try {
                MIX_DestroyTrack(m_MusicTrack);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
            m_MusicTrack = nullptr;
        }
    }

    void AudioManager::PauseMusic() {
        if (!m_MusicTrack) return;
        try {
            MIX_PauseTrack(m_MusicTrack);
        } catch (...) {
            // Track might be invalid, mark as null
            m_MusicTrack = nullptr;
        }
    }

    void AudioManager::ResumeMusic() {
        if (!m_MusicTrack) return;
        try {
            MIX_ResumeTrack(m_MusicTrack);
        } catch (...) {
            // Track might be invalid, mark as null
            m_MusicTrack = nullptr;
        }
    }

    void AudioManager::StopMusic() {
        if (!m_MusicTrack) return;
        try {
            MIX_StopTrack(m_MusicTrack, 0);
            MIX_DestroyTrack(m_MusicTrack);
        } catch (...) {
            // Track might already be invalid, continue anyway
        }
        m_MusicTrack = nullptr;
    }

    void AudioManager::FadeInMusic(const char* Path, int Ms, int Loops) {
        LockGuard Guard(m_Lock);
        PlayMusic(Path, Loops);
        // TODO: Apply fade in using properties if supported or manual volume ramp
    }

    void AudioManager::FadeOutMusic(int Ms) {
        if (m_MusicTrack) MIX_StopTrack(m_MusicTrack, MIX_MSToFrames(44100, Ms)); // Approx sample rate
    }

    void AudioManager::SetMusicVolume(float Volume) {
        if (m_MusicTrack) {
            try {
                MIX_SetTrackGain(m_MusicTrack, Volume);
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in MIX_SetTrackGain: ", e.what());
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in MIX_SetTrackGain");
            }
        }
    }

    void AudioManager::SetMasterVolume(float Volume) {
        try {
            MIX_SetMixerGain(m_Mixer, Volume);
        } catch (const std::exception& e) {
            SOLSTICE_LOG("WARNING: Exception in MIX_SetMixerGain: ", e.what());
        } catch (...) {
            SOLSTICE_LOG("WARNING: Unknown exception in MIX_SetMixerGain");
        }
    }

    void AudioManager::PlaySound(const char* Path, int Loops) {
        LockGuard Guard(m_Lock);
        MIX_Audio* Audio = LoadAudio(Path);
        if (Audio) {
            try {
                MIX_PlayAudio(m_Mixer, Audio);
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in MIX_PlayAudio for: ", Path);
                SOLSTICE_LOG("  Exception: ", e.what());
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in MIX_PlayAudio for: ", Path);
            }
        }
    }

    void AudioManager::SetSoundVolume(float Volume) {
        // Global sound volume via mixer gain (SDL_mixer 3)
        MIX_SetMixerGain(m_Mixer, Volume);
    }

    void AudioManager::SetListener(const Listener& ListenerData) {
        LockGuard Guard(m_Lock);
        m_PreviousListenerPosition = m_Listener.Position;
        m_HasPreviousListenerPosition = true;
        m_Listener = ListenerData;
        if (m_Listener.Forward.Dot(m_Listener.Forward) > 0.001f) m_Listener.Forward = m_Listener.Forward.Normalized();
        if (m_Listener.Up.Dot(m_Listener.Up) > 0.001f) m_Listener.Up = m_Listener.Up.Normalized();
    }

    const Listener& AudioManager::GetListener() const {
        return m_Listener;
    }

    AudioSource AudioManager::PlaySound3D(const char* Path, const Math::Vec3& Position, float MaxDistance, bool Loop) {
        AudioSource Source;
        Source.Position = Position;
        Source.PreviousPosition = Position;
        Source.Direction = Math::Vec3(0.0f, 0.0f, -1.0f);
        Source.Velocity = Math::Vec3(0.0f, 0.0f, 0.0f);
        Source.MinDistance = 1.0f;
        Source.MaxDistance = MaxDistance;
        Source.RolloffFactor = 1.0f;
        Source.OcclusionFactor = 0.0f;
        Source.ObstructionFactor = 0.0f;
        Source.PitchVariance = 0.03f;
        Source.BasePitchRatio = 1.0f + RandRange(-Source.PitchVariance, Source.PitchVariance);
        Source.DopplerFactor = 1.0f;
        Source.SpeedOfSound = 343.3f;
        Source.AirAbsorptionFactor = 0.25f;
        Source.ConeInnerAngleDeg = 360.0f;
        Source.ConeOuterAngleDeg = 360.0f;
        Source.ConeOuterGain = 1.0f;
        Source.FocusFactor = 0.15f;
        Source.NearFieldGain = 0.05f;
        Source.MaxGain = 1.0f;
        Source.DopplerSmoothing = 0.06f;
        Source.ImmersionFactor = 0.35f;
        Source.DistanceReverbFactor = 0.40f;
        Source.RearReverbFactor = 0.25f;
        Source.DiffractionFactor = 0.35f;
        Source.OcclusionPitchDamping = 0.12f;
        Source.AdaptiveMotionFactor = 0.20f;
        Source.WetLevel = 1.0f;
        Source.DryLevel = 1.0f;
        Source.OcclusionAttack = kTargetOcclusionAttackSec;
        Source.OcclusionRelease = kTargetOcclusionReleaseSec;
        Source.WetAttack = kTargetWetAttackSec;
        Source.WetRelease = kTargetWetReleaseSec;
        Source.CurrentOcclusion = 0.0f;
        Source.CurrentWetLevel = 0.0f;
        Source.CurrentFrequencyRatio = Source.BasePitchRatio;
        Source.Volume = 1.0f; // Default full volume
        Source.Priority = 0;
        Source.DistanceMode = DistanceModel::Inverse;
        Source.IsDialogue = false;
        Source.IsCriticalCue = false;
        Source.IsLooping = Loop;
        Source.HasManualVelocity = false;
        Source.HasPreviousPosition = false;
        Source.Track = nullptr;

        {
            LockGuard Guard(m_Lock);
            if (!m_Mixer) {
                SOLSTICE_LOG("WARNING: AudioManager mixer not initialized, cannot play sound: ", Path);
                return Source;
            }

            MIX_Audio* Audio = LoadAudio(Path);
            if (!Audio) {
                SOLSTICE_LOG("WARNING: Failed to load audio file: ", Path, " - sound will not play");
                return Source;
            }

            // Log that we're attempting to set audio on track
            SOLSTICE_LOG("Setting audio on 3D sound track for: ", Path);

            // Create track with exception handling
            MIX_Track* Track = nullptr;
            try {
                Track = MIX_CreateTrack(m_Mixer);
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in MIX_CreateTrack for: ", Path);
                SOLSTICE_LOG("  Exception: ", e.what());
                return Source;
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in MIX_CreateTrack for: ", Path);
                return Source;
            }

            if (!Track) {
                const char* error = SDL_GetError();
                std::string errorStr = error ? error : "(no error message)";
                SOLSTICE_LOG("WARNING: Failed to create audio track for: ", Path);
                SOLSTICE_LOG("  SDL Error: ", errorStr);
                return Source;
            }

            // Set audio on track with error checking and exception handling
            // Capture error immediately after call to prevent it from being cleared
            int setAudioResult = 0;
            try {
                setAudioResult = MIX_SetTrackAudio(Track, Audio);
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in MIX_SetTrackAudio for: ", Path);
                SOLSTICE_LOG("  Exception: ", e.what());
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                return Source;
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in MIX_SetTrackAudio for: ", Path);
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                return Source;
            }

            // MIX_SetTrackAudio returns true (1) on success, false (0) on error
            if (setAudioResult == 0) {
                const char* error = SDL_GetError();
                std::string errorStr = error ? error : "(no error message)";
                SOLSTICE_LOG("WARNING: Failed to set track audio for: ", Path);
                SOLSTICE_LOG("  MIX_SetTrackAudio returned: false (error)");
                SOLSTICE_LOG("  SDL Error: ", errorStr);
                if (errorStr == "(no error message)" || errorStr.empty()) {
                    SOLSTICE_LOG("  Note: SDL error may have been cleared or not set");
                }
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                return Source;
            }

            Source.Track = Track;

            // Create properties and play track with exception handling
            SDL_PropertiesID props = 0;
            try {
                props = SDL_CreateProperties();
            } catch (const std::exception& e) {
                SOLSTICE_LOG("WARNING: Exception in SDL_CreateProperties for: ", Path);
                SOLSTICE_LOG("  Exception: ", e.what());
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                Source.Track = nullptr;
                return Source;
            } catch (...) {
                SOLSTICE_LOG("WARNING: Unknown exception in SDL_CreateProperties for: ", Path);
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                Source.Track = nullptr;
                return Source;
            }

            if (props) {
                if (Loop) SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);

                int playResult = 0;
                try {
                    playResult = MIX_PlayTrack(Track, props);
                } catch (const std::exception& e) {
                    SOLSTICE_LOG("WARNING: Exception in MIX_PlayTrack for: ", Path);
                    SOLSTICE_LOG("  Exception: ", e.what());
                    try {
                        MIX_DestroyTrack(Track);
                    } catch (...) {
                        // Ignore exceptions during cleanup
                    }
                    Source.Track = nullptr;
                    SDL_DestroyProperties(props);
                    return Source;
                } catch (...) {
                    SOLSTICE_LOG("WARNING: Unknown exception in MIX_PlayTrack for: ", Path);
                    try {
                        MIX_DestroyTrack(Track);
                    } catch (...) {
                        // Ignore exceptions during cleanup
                    }
                    Source.Track = nullptr;
                    SDL_DestroyProperties(props);
                    return Source;
                }

                // MIX_PlayTrack returns true (1) on success, false (0) on error
                if (playResult == 0) {
                    const char* error = SDL_GetError();
                    std::string errorStr = error ? error : "(no error message)";
                    SOLSTICE_LOG("WARNING: Failed to play track for: ", Path);
                    SOLSTICE_LOG("  SDL Error: ", errorStr);
                    try {
                        MIX_DestroyTrack(Track);
                    } catch (...) {
                        // Ignore exceptions during cleanup
                    }
                    Source.Track = nullptr;
                }

                SDL_DestroyProperties(props);
            } else {
                SOLSTICE_LOG("WARNING: Failed to create properties for track: ", Path);
                try {
                    MIX_DestroyTrack(Track);
                } catch (...) {
                    // Ignore exceptions during cleanup
                }
                Source.Track = nullptr;
            }
        }

        // Update audio source after releasing lock (UpdateAudioSource will lock internally)
        if (Source.Track) {
            UpdateAudioSource(Source);
        }

        return Source;
    }

    void AudioManager::UpdateAudioSource(AudioSource& Source) {
        if (!Source.Track) return;
        Listener listenerCopy;
        float duckingFactor = 1.0f;
        {
            LockGuard Guard(m_Lock);
            if (!m_Mixer) {
                Source.Track = nullptr;
                return;
            }
            listenerCopy = m_Listener;
            if (!Source.IsDialogue && !Source.IsCriticalCue) {
                bool dialogueActive = false;
                for (const auto& [_, emitter] : m_Emitters) {
                    if (!emitter.Track) {
                        continue;
                    }
                    if (emitter.IsDialogue) {
                        dialogueActive = true;
                        duckingFactor *= (1.0f - m_DialogueDuckingStrength);
                    } else if (emitter.IsCriticalCue) {
                        duckingFactor *= (1.0f - m_CriticalCueDuckingStrength);
                    }
                }
                if (dialogueActive) {
                    duckingFactor = std::min(duckingFactor, 1.0f / kMinDialogueToSfxRatio);
                }
            }
        }
        ApplySpatialization(Source, listenerCopy, kDefaultDt, duckingFactor);
    }

    void AudioManager::StopAudioSource(AudioSource& Source) {
        if (!Source.Track) return;
        try {
            MIX_StopTrack(Source.Track, 0);
            MIX_DestroyTrack(Source.Track);
        } catch (...) {
            // Track might already be invalid, continue anyway
        }
        Source.Track = nullptr;
    }

    void AudioManager::ApplySpatialization(AudioSource& Source, const Listener& ListenerData, float Dt, float DuckingFactor) {
        if (!Source.Track) {
            return;
        }
        try {
            if (!MIX_TrackPlaying(Source.Track)) {
                Source.Track = nullptr;
                return;
            }
        } catch (...) {
            Source.Track = nullptr;
            return;
        }

        Listener listener = ListenerData;
        if (listener.Forward.Dot(listener.Forward) < 0.001f) {
            listener.Forward = Math::Vec3(0, 0, -1);
        }
        if (listener.Up.Dot(listener.Up) < 0.001f) {
            listener.Up = Math::Vec3(0, 1, 0);
        }

        const Math::Vec3 toSource = Source.Position - listener.Position;
        const float distance = std::max(0.1f, toSource.Magnitude());
        const Math::Vec3 toSourceDir = (distance > 0.0001f) ? (toSource / distance) : Math::Vec3(0.0f, 0.0f, -1.0f);
        const Math::Vec3 toListenerDir = toSourceDir * -1.0f;

        Math::Vec3 listenerVelocity(0.0f, 0.0f, 0.0f);
        {
            LockGuard Guard(m_Lock);
            if (m_HasPreviousListenerPosition && Dt > 0.0001f) {
                listenerVelocity = (listener.Position - m_PreviousListenerPosition) / Dt;
            }
        }
        Math::Vec3 sourceVelocity(0.0f, 0.0f, 0.0f);
        if (Source.HasManualVelocity) {
            sourceVelocity = Source.Velocity;
        } else if (Source.HasPreviousPosition && Dt > 0.0001f) {
            sourceVelocity = (Source.Position - Source.PreviousPosition) / Dt;
        }

        Math::Vec3 forward = listener.Forward.Normalized();
        Math::Vec3 up = listener.Up.Normalized();
        Math::Vec3 right = forward.Cross(up);
        if (right.Dot(right) < 0.001f) {
            right = Math::Vec3(1, 0, 0);
        } else {
            right = right.Normalized();
        }
        up = right.Cross(forward);
        if (up.Dot(up) < 0.001f) {
            up = Math::Vec3(0, 1, 0);
        } else {
            up = up.Normalized();
        }

        const float localX = toSource.Dot(right);
        const float localY = toSource.Dot(up);
        const float localZ = toSource.Dot(forward * -1.0f);
        MIX_Point3D localPos = { localX, localY, localZ };
        try {
            MIX_SetTrack3DPosition(Source.Track, &localPos);
        } catch (...) {
            Source.Track = nullptr;
            return;
        }

        const auto listenerZone = EvaluateZoneAt(listener.Position);
        const auto sourceZone = EvaluateZoneAt(Source.Position);
        const float zoneWet = std::max(
            listenerZone ? listenerZone->Wetness : 0.0f,
            sourceZone ? sourceZone->Wetness : 0.0f
        );
        const float zoneObstructionMul = sourceZone ? sourceZone->ObstructionMultiplier : 1.0f;
        const float targetOcclusion = Clamp01(Source.OcclusionFactor + Source.ObstructionFactor * zoneObstructionMul);
        const float normalizedDistance = std::clamp((distance - Source.MinDistance) / std::max(0.01f, Source.MaxDistance - Source.MinDistance), 0.0f, 1.0f);
        const float rearWeight = std::clamp((-listener.Forward.Normalized().Dot(toSourceDir) + 1.0f) * 0.5f, 0.0f, 1.0f);
        const float listenerSpeed = listenerVelocity.Magnitude();
        const float motionWeight = std::clamp(listenerSpeed / std::max(1.0f, Source.SpeedOfSound * 0.08f), 0.0f, 1.0f);
        float focusFactor = Source.FocusFactor;
        float immersionFactor = Source.ImmersionFactor;
        float diffractionFactor = Source.DiffractionFactor;
        float adaptiveMotionFactor = Source.AdaptiveMotionFactor;
        float airAbsorptionFactor = Source.AirAbsorptionFactor;
        if (m_MLSpatializationEnabled) {
            const auto ml = m_MLSpatializer.Predict(Source.Position, listener.Position, sourceVelocity, listenerVelocity,
                                                    targetOcclusion, distance, std::atan2(toSourceDir.x, -toSourceDir.z));
            focusFactor = std::lerp(focusFactor, ml.Focus, 0.35f);
            immersionFactor = std::lerp(immersionFactor, ml.Immersion, 0.35f);
            diffractionFactor = std::lerp(diffractionFactor, ml.Diffraction, 0.35f);
            adaptiveMotionFactor = std::lerp(adaptiveMotionFactor, ml.MotionAdaptation, 0.35f);
            airAbsorptionFactor = std::lerp(airAbsorptionFactor, ml.AirAbsorption, 0.35f);
        }
        Source.FocusFactor = Clamp01(focusFactor);
        Source.ImmersionFactor = Clamp01(immersionFactor);
        Source.DiffractionFactor = Clamp01(diffractionFactor);
        Source.AdaptiveMotionFactor = Clamp01(adaptiveMotionFactor);
        Source.AirAbsorptionFactor = Clamp01(airAbsorptionFactor);
        const float adaptiveMotion = Source.AdaptiveMotionFactor * motionWeight;
        const float distanceWetBloom = Clamp01(Source.DistanceReverbFactor) * normalizedDistance * (1.0f + 0.5f * adaptiveMotion);
        const float rearWetBloom = Clamp01(Source.RearReverbFactor) * rearWeight;
        const float targetWet = Clamp01(Source.WetLevel * zoneWet + distanceWetBloom + rearWetBloom);

        Source.CurrentOcclusion = SmoothTowards(
            Source.CurrentOcclusion,
            targetOcclusion,
            Source.OcclusionAttack,
            Source.OcclusionRelease,
            Dt
        );
        Source.CurrentWetLevel = SmoothTowards(
            Source.CurrentWetLevel,
            targetWet,
            Source.WetAttack,
            Source.WetRelease,
            Dt
        );

        const float distanceGain = Clamp01(ComputeDistanceAttenuation(Source, distance));
        const float coneGain = Clamp01(ComputeConeAttenuation(Source, toListenerDir));
        const float focusGain = ComputeFocusGain(Source, listener, toSourceDir);
        const float immersionGain = ComputeImmersionGain(Source, listener, toSourceDir);
        const float diffractionGain = ComputeDiffractionGain(Source, listener, toSourceDir, Source.CurrentOcclusion);
        const float priorityGain = ComputePriorityGain(Source);
        const float airAbsorptionGain = std::exp(-std::max(0.0f, Source.AirAbsorptionFactor) * normalizedDistance);
        const float nearFieldBoost = (distance < Source.MinDistance)
            ? std::lerp(1.0f + std::max(0.0f, Source.NearFieldGain), 1.0f, std::clamp(distance / std::max(0.01f, Source.MinDistance), 0.0f, 1.0f))
            : 1.0f;
        const float occludedGain = std::lerp(1.0f, 0.15f, Clamp01(Source.CurrentOcclusion));
        const float wetPenalty = std::lerp(1.0f, 0.85f, Clamp01(Source.CurrentWetLevel));
        const float rawGain = Source.Volume * Source.DryLevel * distanceGain * coneGain * focusGain * immersionGain * diffractionGain * priorityGain * nearFieldBoost * airAbsorptionGain * occludedGain * wetPenalty * DuckingFactor;
        const float finalGain = std::clamp(rawGain, 0.0f, std::max(0.05f, Source.MaxGain));

        try {
            MIX_SetTrackGain(Source.Track, finalGain);
            const float dopplerRatio = ComputeDopplerRatio(Source, listenerVelocity, sourceVelocity, toSourceDir);
            const float occlusionPitch = std::lerp(1.0f, 1.0f - Clamp01(Source.OcclusionPitchDamping), Clamp01(Source.CurrentOcclusion));
            const float motionSpreadPitch = 1.0f + adaptiveMotion * 0.03f * (1.0f - std::abs(toSourceDir.Dot(listener.Forward.Normalized())));
            const float targetFreqRatio = std::clamp(Source.BasePitchRatio * dopplerRatio * occlusionPitch * motionSpreadPitch, 0.01f, 100.0f);
            const float smoothTime = std::max(0.001f, Source.DopplerSmoothing);
            Source.CurrentFrequencyRatio = SmoothTowards(Source.CurrentFrequencyRatio, targetFreqRatio, smoothTime, smoothTime, Dt);
            Source.CurrentFrequencyRatio = std::clamp(Source.CurrentFrequencyRatio, 0.01f, 100.0f);
            MIX_SetTrackFrequencyRatio(Source.Track, Source.CurrentFrequencyRatio);
        } catch (...) {
            Source.Track = nullptr;
        }
        Source.PreviousPosition = Source.Position;
        Source.HasPreviousPosition = true;
    }

    AudioEmitterHandle AudioManager::CreateEmitter(const char* Path, const Math::Vec3& Position, float MaxDistance, bool Loop) {
        AudioSource source = PlaySound3D(Path, Position, MaxDistance, Loop);
        if (!source.Track) {
            return 0;
        }
        LockGuard Guard(m_Lock);
        const AudioEmitterHandle handle = m_NextEmitterHandle++;
        m_Emitters[handle] = source;
        return handle;
    }

    bool AudioManager::UpdateEmitterTransform(AudioEmitterHandle Handle, const Math::Vec3& Position) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.Position = Position;
        return true;
    }

    bool AudioManager::SetEmitterVolume(AudioEmitterHandle Handle, float Volume) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.Volume = Clamp01(Volume);
        return true;
    }

    bool AudioManager::SetEmitterOcclusion(AudioEmitterHandle Handle, float Occlusion) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.OcclusionFactor = Clamp01(Occlusion);
        return true;
    }

    bool AudioManager::SetEmitterRolloff(AudioEmitterHandle Handle, float MinDistance, float MaxDistance, float RolloffFactor, DistanceModel Model) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.MinDistance = std::max(0.01f, MinDistance);
        it->second.MaxDistance = std::max(it->second.MinDistance + 0.01f, MaxDistance);
        it->second.RolloffFactor = std::max(0.001f, RolloffFactor);
        it->second.DistanceMode = Model;
        return true;
    }

    bool AudioManager::SetEmitterDirection(AudioEmitterHandle Handle, const Math::Vec3& Direction) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        if (Direction.Dot(Direction) > 0.0001f) {
            it->second.Direction = Direction.Normalized();
        }
        return true;
    }

    bool AudioManager::SetEmitterCone(AudioEmitterHandle Handle, float InnerAngleDeg, float OuterAngleDeg, float OuterGain) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.ConeInnerAngleDeg = std::clamp(InnerAngleDeg, 0.0f, 360.0f);
        it->second.ConeOuterAngleDeg = std::clamp(std::max(it->second.ConeInnerAngleDeg, OuterAngleDeg), 0.0f, 360.0f);
        it->second.ConeOuterGain = Clamp01(OuterGain);
        return true;
    }

    bool AudioManager::SetEmitterDoppler(AudioEmitterHandle Handle, float DopplerFactor, float SpeedOfSound) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.DopplerFactor = std::max(0.0f, DopplerFactor);
        it->second.SpeedOfSound = std::max(0.1f, SpeedOfSound);
        return true;
    }

    bool AudioManager::SetEmitterVelocity(AudioEmitterHandle Handle, const Math::Vec3& Velocity, bool Manual) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.Velocity = Velocity;
        it->second.HasManualVelocity = Manual;
        return true;
    }

    bool AudioManager::ClearEmitterVelocity(AudioEmitterHandle Handle) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.Velocity = Math::Vec3(0.0f, 0.0f, 0.0f);
        it->second.HasManualVelocity = false;
        return true;
    }

    bool AudioManager::SetEmitterFocus(AudioEmitterHandle Handle, float FocusFactor, float NearFieldGain, float MaxGain) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.FocusFactor = Clamp01(FocusFactor);
        it->second.NearFieldGain = std::max(0.0f, NearFieldGain);
        it->second.MaxGain = std::max(0.05f, MaxGain);
        return true;
    }

    bool AudioManager::SetEmitterImmersion(AudioEmitterHandle Handle, float ImmersionFactor, float DistanceReverbFactor, float RearReverbFactor) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.ImmersionFactor = Clamp01(ImmersionFactor);
        it->second.DistanceReverbFactor = Clamp01(DistanceReverbFactor);
        it->second.RearReverbFactor = Clamp01(RearReverbFactor);
        return true;
    }

    bool AudioManager::SetEmitterDiffraction(AudioEmitterHandle Handle, float DiffractionFactor, float OcclusionPitchDamping) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.DiffractionFactor = Clamp01(DiffractionFactor);
        it->second.OcclusionPitchDamping = Clamp01(OcclusionPitchDamping);
        return true;
    }

    bool AudioManager::SetEmitterMotionAdaptation(AudioEmitterHandle Handle, float AdaptiveMotionFactor) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.AdaptiveMotionFactor = Clamp01(AdaptiveMotionFactor);
        return true;
    }

    bool AudioManager::SetEmitterAirAbsorption(AudioEmitterHandle Handle, float AirAbsorptionFactor) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.AirAbsorptionFactor = std::max(0.0f, AirAbsorptionFactor);
        return true;
    }

    bool AudioManager::SetEmitterPitchVariance(AudioEmitterHandle Handle, float PitchVariance) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.PitchVariance = std::max(0.0f, PitchVariance);
        it->second.BasePitchRatio = std::clamp(1.0f + RandRange(-it->second.PitchVariance, it->second.PitchVariance), 0.01f, 100.0f);
        return true;
    }

    bool AudioManager::SetEmitterFlags(AudioEmitterHandle Handle, bool IsDialogue, bool IsCriticalCue, int Priority) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        it->second.IsDialogue = IsDialogue;
        it->second.IsCriticalCue = IsCriticalCue;
        it->second.Priority = Priority;
        return true;
    }

    bool AudioManager::GetEmitterSnapshot(AudioEmitterHandle Handle, AudioSource& OutSource) const {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        OutSource = it->second;
        return true;
    }

    bool AudioManager::DestroyEmitter(AudioEmitterHandle Handle) {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        if (it == m_Emitters.end()) {
            return false;
        }
        StopAudioSource(it->second);
        m_Emitters.erase(it);
        return true;
    }

    bool AudioManager::IsEmitterValid(AudioEmitterHandle Handle) const {
        LockGuard Guard(m_Lock);
        auto it = m_Emitters.find(Handle);
        return it != m_Emitters.end() && it->second.Track != nullptr;
    }

    void AudioManager::UpdateEmitters(float Dt) {
        Listener listenerCopy;
        {
            LockGuard Guard(m_Lock);
            listenerCopy = m_Listener;
            auto listenerZone = EvaluateZoneAt(listenerCopy.Position);
            if (listenerZone) {
                m_ListenerZoneWetness = listenerZone->Wetness;
                m_Listener.TargetReverb = listenerZone->Preset;
            } else {
                m_ListenerZoneWetness = 0.0f;
            }
        }

        std::vector<AudioEmitterHandle> toRemove;
        std::vector<AudioEmitterHandle> handles;
        {
            LockGuard Guard(m_Lock);
            handles.reserve(m_Emitters.size());
            for (const auto& [handle, _] : m_Emitters) {
                handles.push_back(handle);
            }
        }

        std::vector<AudioSource> waveSources;
        waveSources.reserve(handles.size());
        for (AudioEmitterHandle handle : handles) {
            AudioSource sourceCopy;
            bool exists = false;
            float duckingFactor = 1.0f;
            {
                LockGuard Guard(m_Lock);
                auto it = m_Emitters.find(handle);
                if (it != m_Emitters.end()) {
                    exists = true;
                    sourceCopy = it->second;
                    if (!sourceCopy.IsDialogue && !sourceCopy.IsCriticalCue) {
                        bool dialogueActive = false;
                        for (const auto& [otherHandle, other] : m_Emitters) {
                            if (otherHandle == handle || !other.Track) {
                                continue;
                            }
                            if (other.IsDialogue) {
                                dialogueActive = true;
                                duckingFactor *= (1.0f - m_DialogueDuckingStrength);
                            } else if (other.IsCriticalCue) {
                                duckingFactor *= (1.0f - m_CriticalCueDuckingStrength);
                            }
                        }
                        if (dialogueActive) {
                            duckingFactor = std::min(duckingFactor, 1.0f / kMinDialogueToSfxRatio);
                        }
                    }
                }
            }
            if (!exists) {
                continue;
            }

            waveSources.push_back(sourceCopy);

            ApplySpatialization(sourceCopy, listenerCopy, Dt, duckingFactor);
            {
                LockGuard Guard(m_Lock);
                auto it = m_Emitters.find(handle);
                if (it == m_Emitters.end()) {
                    continue;
                }
                it->second = sourceCopy;
                if (!it->second.Track) {
                    toRemove.push_back(handle);
                }
            }
        }

        if (!toRemove.empty()) {
            LockGuard Guard(m_Lock);
            for (AudioEmitterHandle handle : toRemove) {
                m_Emitters.erase(handle);
            }
        }

        if (m_WaveTracingEnabled) {
            std::vector<SoundRayResult> results;
            m_WaveTracer.Trace(waveSources, listenerCopy.Position, results);
            m_WaveTracer.ApplyResults(results, *this);
        }

        {
            LockGuard Guard(m_Lock);
            m_PreviousListenerPosition = m_Listener.Position;
            m_HasPreviousListenerPosition = true;
        }
    }

    bool AudioManager::ApplySpatialProfile(AudioEmitterHandle Handle, int Profile) {
        switch (Profile) {
            case 1: // AmbientBed
                return SetEmitterFocus(Handle, 0.05f, 0.03f, 0.95f)
                    && SetEmitterImmersion(Handle, 0.75f, 0.65f, 0.45f)
                    && SetEmitterDiffraction(Handle, 0.65f, 0.20f)
                    && SetEmitterMotionAdaptation(Handle, 0.90f);
            case 2: // Footstep
                return SetEmitterFocus(Handle, 0.30f, 0.07f, 1.0f)
                    && SetEmitterImmersion(Handle, 0.45f, 0.30f, 0.18f)
                    && SetEmitterDiffraction(Handle, 0.35f, 0.10f)
                    && SetEmitterMotionAdaptation(Handle, 0.35f);
            case 3: // Weapon
                return SetEmitterFocus(Handle, 0.55f, 0.10f, 1.05f)
                    && SetEmitterImmersion(Handle, 0.60f, 0.50f, 0.28f)
                    && SetEmitterDiffraction(Handle, 0.55f, 0.16f)
                    && SetEmitterMotionAdaptation(Handle, 0.40f);
            case 4: // Voice
                return SetEmitterFocus(Handle, 0.85f, 0.12f, 1.1f)
                    && SetEmitterImmersion(Handle, 0.70f, 0.42f, 0.35f)
                    && SetEmitterDiffraction(Handle, 0.50f, 0.22f)
                    && SetEmitterMotionAdaptation(Handle, 0.30f)
                    && SetEmitterFlags(Handle, true, false, 2);
            case 5: // Vehicle
                return SetEmitterFocus(Handle, 0.35f, 0.08f, 1.05f)
                    && SetEmitterImmersion(Handle, 0.85f, 0.58f, 0.42f)
                    && SetEmitterDiffraction(Handle, 0.70f, 0.18f)
                    && SetEmitterMotionAdaptation(Handle, 0.75f);
            case 0:
            default:
                return SetEmitterFocus(Handle, 0.15f, 0.05f, 1.0f)
                    && SetEmitterImmersion(Handle, 0.35f, 0.40f, 0.25f)
                    && SetEmitterDiffraction(Handle, 0.35f, 0.12f)
                    && SetEmitterMotionAdaptation(Handle, 0.20f);
        }
    }

    void AudioManager::SetHRTFEnabled(bool Enabled) {
        m_HRTFEnabled = Enabled;
        m_HRTFRenderer.SetEnabled(Enabled);
    }

    bool AudioManager::LoadHRTFDatabase(const char* Path) {
        if (!Path || !Path[0]) {
            return m_HRTFRenderer.LoadDefaultDatabase();
        }
        return m_HRTFRenderer.LoadSOFA(Path);
    }

    void AudioManager::SetHeadRadius(float Radius) {
        m_HRTFRenderer.SetHeadRadius(Radius);
    }

    void AudioManager::SetWaveTracingEnabled(bool Enabled) {
        m_WaveTracingEnabled = Enabled;
    }

    void AudioManager::SetWaveTracingMaxRays(int PerSource) {
        m_WaveTracer.SetMaxRays(PerSource);
    }

    void AudioManager::SetWaveTracingMaxBounces(int Bounces) {
        m_WaveTracer.SetMaxBounces(Bounces);
    }

    void AudioManager::SetAmbisonicOrder(int Order) {
        if (Order <= 1) {
            m_Ambisonics.SetOrder(AmbisonicOrder::First);
        } else if (Order == 2) {
            m_Ambisonics.SetOrder(AmbisonicOrder::Second);
        } else {
            m_Ambisonics.SetOrder(AmbisonicOrder::Third);
        }
    }

    int AudioManager::GetAmbisonicOrder() const {
        switch (m_Ambisonics.GetOrder()) {
            case AmbisonicOrder::First: return 1;
            case AmbisonicOrder::Second: return 2;
            case AmbisonicOrder::Third: return 3;
            default: return 1;
        }
    }

    void AudioManager::SetFluidCouplingEnabled(bool Enabled) {
        m_FluidCouplingEnabled = Enabled;
    }

    void AudioManager::SetPortalTransform(const Math::Matrix4& Transform, bool Enabled) {
        m_PortalAudio.SetPortalTransform(Transform, Enabled);
    }

    void AudioManager::ClearPortalTransform() {
        m_PortalAudio.ClearPortalTransform();
    }

    void AudioManager::SetMLSpatializationEnabled(bool Enabled) {
        m_MLSpatializationEnabled = Enabled;
        m_MLSpatializer.SetEnabled(Enabled);
    }

    void AudioManager::TrainMLModel() {
        m_MLSpatializer.Train();
    }

    bool AudioManager::SaveMLWeights(const char* Path) {
        if (!Path || !Path[0]) return false;
        return m_MLSpatializer.SaveWeights(Path);
    }

    bool AudioManager::LoadMLWeights(const char* Path) {
        if (!Path || !Path[0]) return false;
        return m_MLSpatializer.LoadWeights(Path);
    }

    void AudioManager::SetReverbPreset(ReverbPresetType Preset) {
        LockGuard Guard(m_Lock);
        m_Listener.TargetReverb = Preset;
    }

    void AudioManager::SetAcousticZones(const std::vector<AcousticZone>& Zones) {
        ClearZoneDrivenMedia();
        LockGuard Guard(m_Lock);
        m_AcousticZones = Zones;
    }

    void AudioManager::ClearAcousticZones() {
        ClearZoneDrivenMedia();
        LockGuard Guard(m_Lock);
        m_AcousticZones.clear();
    }

    std::optional<AcousticZone> AudioManager::GetActiveListenerZone() const {
        LockGuard Guard(m_Lock);
        return EvaluateZoneAt(m_Listener.Position);
    }

} // namespace Solstice::Core::Audio
