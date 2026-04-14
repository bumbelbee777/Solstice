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
        m_Initialized = true;
        SOLSTICE_LOG("Audio Subsystem Initialized");
    }

    void AudioManager::Shutdown() {
        LockGuard Guard(m_Lock);
        if (!m_Initialized) return;

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
        Source.MinDistance = 1.0f;
        Source.MaxDistance = MaxDistance;
        Source.RolloffFactor = 1.0f;
        Source.OcclusionFactor = 0.0f;
        Source.ObstructionFactor = 0.0f;
        Source.PitchVariance = 0.0f;
        Source.WetLevel = 1.0f;
        Source.DryLevel = 1.0f;
        Source.OcclusionAttack = kTargetOcclusionAttackSec;
        Source.OcclusionRelease = kTargetOcclusionReleaseSec;
        Source.WetAttack = kTargetWetAttackSec;
        Source.WetRelease = kTargetWetReleaseSec;
        Source.CurrentOcclusion = 0.0f;
        Source.CurrentWetLevel = 0.0f;
        Source.Volume = 1.0f; // Default full volume
        Source.Priority = 0;
        Source.DistanceMode = DistanceModel::Inverse;
        Source.IsDialogue = false;
        Source.IsCriticalCue = false;
        Source.IsLooping = Loop;
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
        const float targetWet = Clamp01(Source.WetLevel * zoneWet);

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
        const float occludedGain = std::lerp(1.0f, 0.15f, Clamp01(Source.CurrentOcclusion));
        const float wetPenalty = std::lerp(1.0f, 0.85f, Clamp01(Source.CurrentWetLevel));
        const float finalGain = Clamp01(Source.Volume * Source.DryLevel * distanceGain * occludedGain * wetPenalty * DuckingFactor);

        try {
            MIX_SetTrackGain(Source.Track, finalGain);
        } catch (...) {
            Source.Track = nullptr;
        }
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
    }

    void AudioManager::SetReverbPreset(ReverbPresetType Preset) {
        LockGuard Guard(m_Lock);
        m_Listener.TargetReverb = Preset;
    }

    void AudioManager::SetAcousticZones(const std::vector<AcousticZone>& Zones) {
        LockGuard Guard(m_Lock);
        m_AcousticZones = Zones;
    }

    void AudioManager::ClearAcousticZones() {
        LockGuard Guard(m_Lock);
        m_AcousticZones.clear();
    }

    std::optional<AcousticZone> AudioManager::GetActiveListenerZone() const {
        LockGuard Guard(m_Lock);
        return EvaluateZoneAt(m_Listener.Position);
    }

} // namespace Solstice::Core::Audio
