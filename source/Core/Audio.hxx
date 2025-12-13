#pragma once

#include "../Solstice.hxx"
#include <Core/Async.hxx>
#include <Math/Vector.hxx>
#include <vector>
#include <string>
#include <unordered_map>

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

    struct Listener {
        Math::Vec3 Position;
        Math::Vec3 Forward;
        Math::Vec3 Up;
        ReverbParams CurrentReverb;
        ReverbPresetType TargetReverb;
    };

    struct AudioSource {
        Math::Vec3 Position;
        float MaxDistance;     // Distance at which sound is inaudible
        float RolloffFactor;   // How quickly volume decreases with distance (1.0 = inverse square)
        float OcclusionFactor; // 0.0 = unoccluded, 1.0 = fully occluded
        float PitchVariance;   // Random pitch deviation (e.g., 0.02 for +/- 2%)
        bool IsLooping;
        MIX_Track* Track;      // SDL_mixer track assigned (nullptr if not playing)
    };

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

        // Music (Streaming)
        void PlayMusic(const char* Path, int Loops = -1);
        void PauseMusic();
        void ResumeMusic();
        void StopMusic();
        void FadeInMusic(const char* Path, int Ms, int Loops = -1);
        void FadeOutMusic(int Ms);
        void SetMusicVolume(float Volume); // 0.0 - 1.0

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

        // Reverb
        void SetReverbPreset(ReverbPresetType Preset);

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
        
        // Thread safety
        Spinlock m_Lock;
    };

} // namespace Solstice::Core::Audio