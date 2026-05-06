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

Core::Audio::AudioSource SFXManager::PlaySound3DProfiled(const std::string& Path, const Math::Vec3& Position,
                                                         SFXCategory Category, SpatialProfile Profile,
                                                         float Volume, float MaxDistance, bool Loop) {
    Core::Audio::AudioSource source = PlaySound3D(Path, Position, Category, Volume, MaxDistance, Loop);
    if (!m_AudioManager || m_ActiveSounds.empty()) {
        return source;
    }
    SoundEffect& effect = m_ActiveSounds.back();
    if (effect.EmitterHandle != 0) {
        ConfigureEmitterSpatial(effect.EmitterHandle, Profile);
        m_AudioManager->GetEmitterSnapshot(effect.EmitterHandle, effect.Source);
        source = effect.Source;
    }
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
    PlaySound3DProfiled(m_FootstepSoundPath, Position, SFXCategory::Footsteps, SpatialProfile::Footstep, 0.5f, 20.0f, false);
}

void SFXManager::OnWeaponFire(const Math::Vec3& Position) {
    PlaySound3DProfiled(m_WeaponFireSoundPath, Position, SFXCategory::Combat, SpatialProfile::Weapon, 1.0f, 100.0f, false);
}

void SFXManager::OnWeaponReload() {
    PlaySound(m_WeaponReloadSoundPath, SFXCategory::Combat, 0.8f, false);
}

void SFXManager::OnEnemyDeath(const Math::Vec3& Position) {
    PlaySound3DProfiled(m_EnemyDeathSoundPath, Position, SFXCategory::Combat, SpatialProfile::Voice, 0.7f, 50.0f, false);
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

bool SFXManager::ConfigureEmitterSpatial(Core::Audio::AudioEmitterHandle Handle, SpatialProfile Profile, const Math::Vec3& Direction) {
    if (!m_AudioManager || Handle == 0) {
        return false;
    }

    bool ok = true;
    switch (Profile) {
        case SpatialProfile::AmbientBed:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 2.0f, 160.0f, 0.4f, Core::Audio::DistanceModel::Linear) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.05f, 0.03f, 0.95f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.75f, 0.65f, 0.45f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.65f, 0.20f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.90f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.12f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 0.15f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 360.0f, 360.0f, 1.0f) && ok;
            break;
        case SpatialProfile::Footstep:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 0.7f, 24.0f, 1.2f, Core::Audio::DistanceModel::Inverse) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.30f, 0.07f, 1.0f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.45f, 0.30f, 0.18f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.35f, 0.10f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.35f) && ok;
            ok = m_AudioManager->SetEmitterPitchVariance(Handle, 0.05f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.45f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 0.7f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 300.0f, 360.0f, 0.9f) && ok;
            break;
        case SpatialProfile::Weapon:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 1.2f, 180.0f, 1.0f, Core::Audio::DistanceModel::Inverse) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.55f, 0.10f, 1.05f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.60f, 0.50f, 0.28f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.55f, 0.16f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.40f) && ok;
            ok = m_AudioManager->SetEmitterPitchVariance(Handle, 0.02f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.28f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 1.0f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 85.0f, 180.0f, 0.45f) && ok;
            ok = m_AudioManager->SetEmitterDirection(Handle, Direction) && ok;
            break;
        case SpatialProfile::Voice:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 0.8f, 42.0f, 1.35f, Core::Audio::DistanceModel::Inverse) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.85f, 0.12f, 1.1f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.70f, 0.42f, 0.35f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.50f, 0.22f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.30f) && ok;
            ok = m_AudioManager->SetEmitterPitchVariance(Handle, 0.01f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.33f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 0.85f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 60.0f, 140.0f, 0.35f) && ok;
            ok = m_AudioManager->SetEmitterDirection(Handle, Direction) && ok;
            ok = m_AudioManager->SetEmitterFlags(Handle, true, false, 2) && ok;
            break;
        case SpatialProfile::Vehicle:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 2.5f, 260.0f, 0.9f, Core::Audio::DistanceModel::Exponential) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.35f, 0.08f, 1.05f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.85f, 0.58f, 0.42f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.70f, 0.18f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.75f) && ok;
            ok = m_AudioManager->SetEmitterPitchVariance(Handle, 0.015f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.22f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 1.2f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 100.0f, 220.0f, 0.5f) && ok;
            ok = m_AudioManager->SetEmitterDirection(Handle, Direction) && ok;
            break;
        case SpatialProfile::Default:
        default:
            ok = m_AudioManager->SetEmitterRolloff(Handle, 1.0f, 50.0f, 1.0f, Core::Audio::DistanceModel::Inverse) && ok;
            ok = m_AudioManager->SetEmitterFocus(Handle, 0.15f, 0.05f, 1.0f) && ok;
            ok = m_AudioManager->SetEmitterImmersion(Handle, 0.35f, 0.40f, 0.25f) && ok;
            ok = m_AudioManager->SetEmitterDiffraction(Handle, 0.35f, 0.12f) && ok;
            ok = m_AudioManager->SetEmitterMotionAdaptation(Handle, 0.20f) && ok;
            ok = m_AudioManager->SetEmitterPitchVariance(Handle, 0.03f) && ok;
            ok = m_AudioManager->SetEmitterAirAbsorption(Handle, 0.25f) && ok;
            ok = m_AudioManager->SetEmitterDoppler(Handle, 1.0f, 343.3f) && ok;
            ok = m_AudioManager->SetEmitterCone(Handle, 360.0f, 360.0f, 1.0f) && ok;
            break;
    }
    return ok;
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
