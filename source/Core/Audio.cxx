#include "Audio.hxx"
#include "Debug.hxx"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <algorithm>
#include <cmath>
#include <random>

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

        MIX_Audio* Audio = MIX_LoadAudio(m_Mixer, Path, false);
        if (!Audio) {
            SOLSTICE_LOG("Failed to load audio: ", Path, " Error: ", SDL_GetError());
            return nullptr;
        }

        m_CachedAudio[PathStr] = Audio;
        return Audio;
    }

    void AudioManager::PlayMusic(const char* Path, int Loops) {
        LockGuard Guard(m_Lock);
        MIX_Audio* Audio = LoadAudio(Path);
        if (Audio) {
            if (m_MusicTrack) {
                MIX_DestroyTrack(m_MusicTrack);
                m_MusicTrack = nullptr;
            }
            m_MusicTrack = MIX_CreateTrack(m_Mixer);
            if (m_MusicTrack) {
                MIX_SetTrackAudio(m_MusicTrack, Audio);
                
                // Set loops
                SDL_PropertiesID props = SDL_CreateProperties();
                SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, Loops);
                MIX_PlayTrack(m_MusicTrack, props);
                SDL_DestroyProperties(props);
            }
        }
    }

    void AudioManager::PauseMusic() {
        if (m_MusicTrack) MIX_PauseTrack(m_MusicTrack);
    }

    void AudioManager::ResumeMusic() {
        if (m_MusicTrack) MIX_ResumeTrack(m_MusicTrack);
    }

    void AudioManager::StopMusic() {
        if (m_MusicTrack) {
            MIX_StopTrack(m_MusicTrack, 0);
            MIX_DestroyTrack(m_MusicTrack);
            m_MusicTrack = nullptr;
        }
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
        if (m_MusicTrack) MIX_SetTrackGain(m_MusicTrack, Volume);
    }

    void AudioManager::PlaySound(const char* Path, int Loops) {
        LockGuard Guard(m_Lock);
        MIX_Audio* Audio = LoadAudio(Path);
        if (Audio) {
            MIX_PlayAudio(m_Mixer, Audio);
        }
    }

    void AudioManager::SetSoundVolume(float Volume) {
        // Global sound volume? 
        // SDL_mixer 3.0 might allow setting mixer master gain
        MIX_SetMasterGain(m_Mixer, Volume);
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
        LockGuard Guard(m_Lock);
        AudioSource Source;
        Source.Position = Position;
        Source.MaxDistance = MaxDistance;
        Source.RolloffFactor = 1.0f;
        Source.OcclusionFactor = 0.0f;
        Source.PitchVariance = 0.0f;
        Source.IsLooping = Loop;
        Source.Track = nullptr;

        MIX_Audio* Audio = LoadAudio(Path);
        if (Audio) {
            MIX_Track* Track = MIX_CreateTrack(m_Mixer);
            if (Track) {
                MIX_SetTrackAudio(Track, Audio);
                Source.Track = Track;
                UpdateAudioSource(Source);
                
                SDL_PropertiesID props = SDL_CreateProperties();
                if (Loop) SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
                MIX_PlayTrack(Track, props);
                SDL_DestroyProperties(props);
            }
        }
        return Source;
    }

    void AudioManager::UpdateAudioSource(AudioSource& Source) {
        if (!Source.Track) return;
        
        // Check if playing
        if (!MIX_TrackPlaying(Source.Track)) {
            return;
        }

        Math::Vec3 ToSource = Source.Position - m_Listener.Position;
        float Distance = ToSource.Magnitude();
        
        if (Distance < 0.1f) Distance = 0.1f;

        // Custom attenuation logic (SDL_mixer has its own, but we want control)
        // We can use MIX_SetTrackGain for volume and MIX_SetTrack3DPosition for spatialization.
        // MIX_SetTrack3DPosition takes a position relative to the listener (0,0,0).
        // We need to transform the world position of the source into the listener's local space.

        // Listener Basis
        Math::Vec3 F = m_Listener.Forward;
        Math::Vec3 U = m_Listener.Up;
        Math::Vec3 R = F.Cross(U).Normalized();
        // Re-orthogonalize Up
        U = R.Cross(F).Normalized();

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

        // Apply 3D position
        MIX_SetTrack3DPosition(Source.Track, &LocalPos);

        // Apply additional volume factors (Occlusion, etc.)
        // SDL_mixer handles distance attenuation if we pass the position.
        // But we might want to override or augment it.
        // For now, let's trust SDL_mixer's attenuation but apply occlusion.
        
        float OccludedGain = std::lerp(1.0f, 0.15f, Source.OcclusionFactor);
        
        // We can modify the gain ON TOP of the distance attenuation?
        // "Each sample from this track is modulated by this gain value."
        // Yes.
        
        MIX_SetTrackGain(Source.Track, OccludedGain);
    }

    void AudioManager::StopAudioSource(AudioSource& Source) {
        if (Source.Track) {
            MIX_StopTrack(Source.Track, 0);
            MIX_DestroyTrack(Source.Track);
            Source.Track = nullptr;
        }
    }

    void AudioManager::SetReverbPreset(ReverbPresetType Preset) {
        m_Listener.TargetReverb = Preset;
    }

} // namespace Solstice::Core::Audio
