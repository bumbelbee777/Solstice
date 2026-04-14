#include "Gameplay/SFXManager.hxx"
#include "../../Core/Debug/Debug.hxx"

namespace Solstice::Game {

void SFXManager::Initialize() {
    m_AudioManager = &Core::Audio::AudioManager::Instance();

    // Initialize category volumes
    m_CategoryVolumes[SFXCategory::UI] = 1.0f;
    m_CategoryVolumes[SFXCategory::Combat] = 1.0f;
    m_CategoryVolumes[SFXCategory::Ambient] = 0.7f;
    m_CategoryVolumes[SFXCategory::Footsteps] = 0.5f;
    m_CategoryVolumes[SFXCategory::Voice] = 1.0f;
    m_CategoryVolumes[SFXCategory::Music] = 0.7f;

    SIMPLE_LOG("SFXManager: Initialized");
}

void SFXManager::Shutdown() {
    StopAllSounds();
    m_SoundPools.clear();
    m_ActiveSounds.clear();
    SIMPLE_LOG("SFXManager: Shutdown");
}

void SFXManager::Update(float DeltaTime) {
    if (!m_AudioManager) {
        return;
    }
    m_AudioManager->UpdateEmitters(DeltaTime);

    // Refresh active sources from managed emitters.
    for (auto& sound : m_ActiveSounds) {
        if (sound.EmitterHandle != 0) {
            m_AudioManager->GetEmitterSnapshot(sound.EmitterHandle, sound.Source);
        }
    }

    // Clean up finished sounds
    m_ActiveSounds.erase(
        std::remove_if(m_ActiveSounds.begin(), m_ActiveSounds.end(),
            [this](const SoundEffect& sound) {
                if (sound.IsLooping) {
                    return false;
                }
                if (sound.EmitterHandle == 0) {
                    return sound.Source.Track == nullptr;
                }
                return !m_AudioManager->IsEmitterValid(sound.EmitterHandle);
            }),
        m_ActiveSounds.end()
    );
}

void SFXManager::SetCategoryVolume(SFXCategory Category, float Volume) {
    m_CategoryVolumes[Category] = std::max(0.0f, std::min(1.0f, Volume));

    // Update active sounds in this category
    for (auto& sound : m_ActiveSounds) {
        if (sound.Category == Category && sound.Source.Track) {
            sound.Source.Volume = GetEffectiveVolume(Category, sound.Volume);
            if (m_AudioManager && sound.EmitterHandle != 0) {
                m_AudioManager->SetEmitterVolume(sound.EmitterHandle, sound.Source.Volume);
            }
        }
    }
}

float SFXManager::GetCategoryVolume(SFXCategory Category) const {
    auto it = m_CategoryVolumes.find(Category);
    return (it != m_CategoryVolumes.end()) ? it->second : 1.0f;
}

void SFXManager::PlaySound(const std::string& Path, SFXCategory Category, float Volume, bool Loop) {
    if (!m_AudioManager) return;

    float effectiveVolume = GetEffectiveVolume(Category, Volume);
    m_AudioManager->PlaySound(Path.c_str(), Loop ? -1 : 0);
    m_AudioManager->SetSoundVolume(effectiveVolume);
}

Core::Audio::AudioSource SFXManager::PlaySound3D(const std::string& Path, const Math::Vec3& Position,
                                                   SFXCategory Category, float Volume,
                                                   float MaxDistance, bool Loop) {
    if (!m_AudioManager) {
        return Core::Audio::AudioSource();
    }

    float effectiveVolume = GetEffectiveVolume(Category, Volume);
    Core::Audio::AudioEmitterHandle handle = m_AudioManager->CreateEmitter(Path.c_str(), Position, MaxDistance, Loop);
    if (handle != 0) {
        m_AudioManager->SetEmitterVolume(handle, effectiveVolume);
    }
    Core::Audio::AudioSource source{};
    if (handle != 0) {
        m_AudioManager->GetEmitterSnapshot(handle, source);
    }

    // Track active sound
    SoundEffect effect;
    effect.Path = Path;
    effect.Category = Category;
    effect.Volume = Volume;
    effect.IsLooping = Loop;
    effect.MaxDistance = MaxDistance;
    effect.EmitterHandle = handle;
    effect.Source = source;
    m_ActiveSounds.push_back(effect);

    return source;
}

void SFXManager::PlayMusic(const std::string& Path, int Loops) {
    if (m_AudioManager) {
        m_AudioManager->PlayMusic(Path.c_str(), Loops);
        float musicVolume = GetEffectiveVolume(SFXCategory::Music, 1.0f);
        m_AudioManager->SetMusicVolume(musicVolume);
    }
}

void SFXManager::StopMusic() {
    if (m_AudioManager) {
        m_AudioManager->StopMusic();
    }
}

void SFXManager::PauseMusic() {
    if (m_AudioManager) {
        m_AudioManager->PauseMusic();
    }
}

void SFXManager::ResumeMusic() {
    if (m_AudioManager) {
        m_AudioManager->ResumeMusic();
    }
}

void SFXManager::SetMusicVolume(float Volume) {
    if (m_AudioManager) {
        float effectiveVolume = GetEffectiveVolume(SFXCategory::Music, Volume);
        m_AudioManager->SetMusicVolume(effectiveVolume);
    }
}

void SFXManager::RegisterSoundPool(const std::string& Path, SFXCategory Category, int PoolSize) {
    SoundPool pool;
    pool.Path = Path;
    pool.Category = Category;
    pool.Sources.resize(PoolSize);
    pool.CurrentIndex = 0;
    m_SoundPools[Path] = pool;
}

void SFXManager::PlaySoundFromPool(const std::string& Path, SFXCategory Category) {
    SoundPool* pool = GetSoundPool(Path);
    if (!pool) {
        // Create pool on demand
        RegisterSoundPool(Path, Category, 5);
        pool = GetSoundPool(Path);
    }

    if (pool && m_AudioManager) {
        // Use round-robin to cycle through pool
        Core::Audio::AudioEmitterHandle& handle = pool->Sources[pool->CurrentIndex];

        // Stop current sound if playing
        if (handle != 0) {
            m_AudioManager->DestroyEmitter(handle);
            handle = 0;
        }

        // Play new sound
        handle = m_AudioManager->CreateEmitter(Path.c_str(), Math::Vec3(0, 0, 0), 50.0f, false);
        if (handle != 0) {
            m_AudioManager->SetEmitterVolume(handle, GetEffectiveVolume(Category, 1.0f));
        }

        pool->CurrentIndex = (pool->CurrentIndex + 1) % pool->Sources.size();
    }
}

void SFXManager::OnPlayerFootstep(const Math::Vec3& Position) {
    PlaySound3D(m_FootstepSoundPath, Position, SFXCategory::Footsteps, 0.5f, 20.0f, false);
}

void SFXManager::OnWeaponFire(const Math::Vec3& Position) {
    PlaySound3D(m_WeaponFireSoundPath, Position, SFXCategory::Combat, 1.0f, 100.0f, false);
}

void SFXManager::OnWeaponReload() {
    PlaySound(m_WeaponReloadSoundPath, SFXCategory::Combat, 0.8f, false);
}

void SFXManager::OnEnemyDeath(const Math::Vec3& Position) {
    PlaySound3D(m_EnemyDeathSoundPath, Position, SFXCategory::Combat, 0.7f, 50.0f, false);
}

void SFXManager::OnPlayerDamage() {
    PlaySound(m_PlayerDamageSoundPath, SFXCategory::Combat, 0.9f, false);
}

void SFXManager::OnUIButtonClick() {
    PlaySound(m_UIButtonClickSoundPath, SFXCategory::UI, 0.6f, false);
}

void SFXManager::OnUIButtonHover() {
    PlaySound(m_UIButtonHoverSoundPath, SFXCategory::UI, 0.3f, false);
}

void SFXManager::UpdateAudioSource(Core::Audio::AudioSource& Source, const Math::Vec3& Position) {
    Source.Position = Position;
    if (m_AudioManager && Source.Track) {
        m_AudioManager->UpdateAudioSource(Source);
    } else if (m_AudioManager) {
        // Try to route through active managed emitter when possible.
        for (auto& sound : m_ActiveSounds) {
            if (sound.Source.Track == Source.Track && sound.EmitterHandle != 0) {
                m_AudioManager->UpdateEmitterTransform(sound.EmitterHandle, Position);
                m_AudioManager->GetEmitterSnapshot(sound.EmitterHandle, Source);
                break;
            }
        }
    }
}

void SFXManager::StopSound(Core::Audio::AudioSource& Source) {
    if (!m_AudioManager) {
        return;
    }
    for (auto& sound : m_ActiveSounds) {
        if (sound.Source.Track == Source.Track && sound.EmitterHandle != 0) {
            m_AudioManager->DestroyEmitter(sound.EmitterHandle);
            sound.EmitterHandle = 0;
            sound.Source.Track = nullptr;
            Source.Track = nullptr;
            return;
        }
    }
    if (Source.Track) {
        m_AudioManager->StopAudioSource(Source);
    }
}

void SFXManager::StopAllSounds(SFXCategory Category) {
    for (auto& sound : m_ActiveSounds) {
        if (sound.Category == Category && sound.Source.Track) {
            StopSound(sound.Source);
        }
    }
}

void SFXManager::StopAllSounds() {
    for (auto& sound : m_ActiveSounds) {
        if (sound.Source.Track) {
            StopSound(sound.Source);
        }
    }
    m_ActiveSounds.clear();
}

float SFXManager::GetEffectiveVolume(SFXCategory Category, float BaseVolume) const {
    float categoryVolume = GetCategoryVolume(Category);
    return BaseVolume * categoryVolume * m_MasterVolume;
}

SFXManager::SoundPool* SFXManager::GetSoundPool(const std::string& Path) {
    auto it = m_SoundPools.find(Path);
    return (it != m_SoundPools.end()) ? &it->second : nullptr;
}

} // namespace Solstice::Game
