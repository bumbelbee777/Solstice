#include "Audio.hxx"
#include "Debug.hxx"
#include "AssetLoader.hxx"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <filesystem>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Solstice::Core::Audio {

    AudioManager::~AudioManager() {
        Shutdown();
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

        m_Initialized = true;
        SOLSTICE_LOG("Audio Subsystem Initialized");
    }

    void AudioManager::Shutdown() {
        LockGuard Guard(m_Lock);
        if (!m_Initialized) return;

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
        (void)Dt;
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
        Source.MaxDistance = MaxDistance;
        Source.RolloffFactor = 1.0f;
        Source.OcclusionFactor = 0.0f;
        Source.PitchVariance = 0.0f;
        Source.Volume = 1.0f; // Default full volume
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

        // Check mixer with lock to avoid race conditions
        {
            LockGuard Guard(m_Lock);
            if (!m_Mixer) {
                // Mixer not initialized or destroyed, mark track as invalid
                Source.Track = nullptr;
                return;
            }
        }

        // Wrap all SDL_mixer calls in try-catch to prevent crashes
        try {
            // Check if playing - this might crash if track is invalid
            if (!MIX_TrackPlaying(Source.Track)) {
                // Track stopped playing, mark as invalid
                Source.Track = nullptr;
                return;
            }
        } catch (...) {
            // Track might be invalid, mark as stopped
            Source.Track = nullptr;
            return;
        }

        // Get listener with lock to avoid race conditions
        Listener listener;
        {
            LockGuard Guard(m_Lock);
            listener = m_Listener;
        }

        // Validate listener vectors are initialized
        float forwardMagSq = listener.Forward.x * listener.Forward.x +
                              listener.Forward.y * listener.Forward.y +
                              listener.Forward.z * listener.Forward.z;
        if (forwardMagSq < 0.001f) {
            // Listener not initialized, use default forward
            listener.Forward = Math::Vec3(0, 0, -1);
        }
        float upMagSq = listener.Up.x * listener.Up.x +
                        listener.Up.y * listener.Up.y +
                        listener.Up.z * listener.Up.z;
        if (upMagSq < 0.001f) {
            // Listener not initialized, use default up
            listener.Up = Math::Vec3(0, 1, 0);
        }

        Math::Vec3 ToSource = Source.Position - listener.Position;
        float Distance = ToSource.Magnitude();

        if (Distance < 0.1f) Distance = 0.1f;

        // Custom attenuation logic (SDL_mixer has its own, but we want control)
        // We can use MIX_SetTrackGain for volume and MIX_SetTrack3DPosition for spatialization.
        // MIX_SetTrack3DPosition takes a position relative to the listener (0,0,0).
        // We need to transform the world position of the source into the listener's local space.

        // Listener Basis
        Math::Vec3 F = listener.Forward.Normalized();
        Math::Vec3 U = listener.Up.Normalized();
        Math::Vec3 R = F.Cross(U);

        // Check if Forward and Up are parallel (cross product is zero)
        float rMagSq = R.x * R.x + R.y * R.y + R.z * R.z;
        if (rMagSq < 0.001f) {
            // Forward and Up are parallel, use default right vector
            R = Math::Vec3(1, 0, 0);
        } else {
            R = R.Normalized();
        }

        // Re-orthogonalize Up
        U = R.Cross(F);
        float uMagSq = U.x * U.x + U.y * U.y + U.z * U.z;
        if (uMagSq < 0.001f) {
            U = Math::Vec3(0, 1, 0);
        } else {
            U = U.Normalized();
        }

        // Transform ToSource to local space
        // Local.x = ToSource . Right
        // Local.y = ToSource . Up
        // Local.z = ToSource . -Forward (Right-handed: Z is back, Forward is -Z)
        // Wait, SDL_mixer says: X right, Y up, Z back (negative forward).
        // So Forward corresponds to -Z.

        float LocalX = ToSource.Dot(R);
        float LocalY = ToSource.Dot(U);
        float LocalZ = ToSource.Dot(F * -1.0f); // Forward is -Z

        MIX_Point3D LocalPos = { LocalX, LocalY, LocalZ };

        // Apply 3D position - wrap in try-catch for safety
        try {
            MIX_SetTrack3DPosition(Source.Track, &LocalPos);
        } catch (...) {
            // Track might be invalid, mark as stopped
            Source.Track = nullptr;
            return;
        }

        // Apply additional volume factors (Occlusion, etc.)
        // SDL_mixer handles distance attenuation if we pass the position.
        // But we might want to override or augment it.
        // For now, let's trust SDL_mixer's attenuation but apply occlusion.

        float OccludedGain = std::lerp(1.0f, 0.15f, Source.OcclusionFactor);

        // Apply volume multiplier
        float FinalGain = OccludedGain * Source.Volume;

        // Clamp gain to valid range
        FinalGain = std::max(0.0f, std::min(1.0f, FinalGain));

        // We can modify the gain ON TOP of the distance attenuation?
        // "Each sample from this track is modulated by this gain value."
        // Yes.

        try {
            MIX_SetTrackGain(Source.Track, FinalGain);
        } catch (...) {
            // Track might be invalid, mark as stopped
            Source.Track = nullptr;
            return;
        }
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

    void AudioManager::SetReverbPreset(ReverbPresetType Preset) {
        m_Listener.TargetReverb = Preset;
    }

} // namespace Solstice::Core::Audio
