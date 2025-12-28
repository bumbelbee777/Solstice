#pragma once

#include "../Solstice.hxx"
#include "../Core/Audio.hxx"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Solstice::Game {

// SFX categories
enum class SFXCategory {
    UI,
    Combat,
    Ambient,
    Footsteps,
    Voice,
    Music
};

// Sound effect entry
struct SoundEffect {
    std::string Path;
    SFXCategory Category;
    float Volume{1.0f};
    bool IsLooping{false};
    float MaxDistance{50.0f};
    Core::Audio::AudioSource Source;
};

// SFX manager wrapping AudioManager
class SOLSTICE_API SFXManager {
public:
    static SFXManager& Instance() {
        static SFXManager instance;
        return instance;
    }

    // Initialization
    void Initialize();
    void Shutdown();
    void Update(float DeltaTime);

    // Volume controls per category
    void SetCategoryVolume(SFXCategory Category, float Volume);
    float GetCategoryVolume(SFXCategory Category) const;
    void SetMasterVolume(float Volume) { m_MasterVolume = Volume; }
    float GetMasterVolume() const { return m_MasterVolume; }

    // Play sounds
    void PlaySound(const std::string& Path, SFXCategory Category, float Volume = 1.0f, bool Loop = false);
    Core::Audio::AudioSource PlaySound3D(const std::string& Path, const Math::Vec3& Position,
                                         SFXCategory Category, float Volume = 1.0f,
                                         float MaxDistance = 50.0f, bool Loop = false);

    // Music
    void PlayMusic(const std::string& Path, int Loops = -1);
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();
    void SetMusicVolume(float Volume);

    // Sound pools for performance
    void RegisterSoundPool(const std::string& Path, SFXCategory Category, int PoolSize = 5);
    void PlaySoundFromPool(const std::string& Path, SFXCategory Category);

    // Event-based sound triggers
    void OnPlayerFootstep(const Math::Vec3& Position);
    void OnWeaponFire(const Math::Vec3& Position);
    void OnWeaponReload();
    void OnEnemyDeath(const Math::Vec3& Position);
    void OnPlayerDamage();
    void OnUIButtonClick();
    void OnUIButtonHover();

    // Update 3D audio sources
    void UpdateAudioSource(Core::Audio::AudioSource& Source, const Math::Vec3& Position);

    // Stop sounds
    void StopSound(Core::Audio::AudioSource& Source);
    void StopAllSounds(SFXCategory Category);
    void StopAllSounds();

private:
    SFXManager() = default;
    ~SFXManager() = default;
    SFXManager(const SFXManager&) = delete;
    SFXManager& operator=(const SFXManager&) = delete;

    Core::Audio::AudioManager* m_AudioManager{nullptr};
    float m_MasterVolume{1.0f};

    // Category volumes
    std::unordered_map<SFXCategory, float> m_CategoryVolumes;

    // Sound pools
    struct SoundPool {
        std::string Path;
        SFXCategory Category;
        std::vector<Core::Audio::AudioSource> Sources;
        size_t CurrentIndex{0};
    };
    std::unordered_map<std::string, SoundPool> m_SoundPools;

    // Active sounds
    std::vector<SoundEffect> m_ActiveSounds;

    // Event sound paths (configurable)
    std::string m_FootstepSoundPath{"assets/sfx/footstep.wav"};
    std::string m_WeaponFireSoundPath{"assets/sfx/weapon_fire.wav"};
    std::string m_WeaponReloadSoundPath{"assets/sfx/weapon_reload.wav"};
    std::string m_EnemyDeathSoundPath{"assets/sfx/enemy_death.wav"};
    std::string m_PlayerDamageSoundPath{"assets/sfx/player_damage.wav"};
    std::string m_UIButtonClickSoundPath{"assets/sfx/ui_click.wav"};
    std::string m_UIButtonHoverSoundPath{"assets/sfx/ui_hover.wav"};

    // Internal methods
    float GetEffectiveVolume(SFXCategory Category, float BaseVolume) const;
    SoundPool* GetSoundPool(const std::string& Path);
};

} // namespace Solstice::Game
