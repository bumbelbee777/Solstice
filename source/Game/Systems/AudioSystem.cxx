#include "Systems/AudioSystem.hxx"
#include <Entity/Components/AudioComponents.hxx>
#include <Entity/Components/PortalComponents.hxx>
#include <Entity/Registry.hxx>
#include <Entity/Transform.hxx>
#include <Render/Portal/PortalLightPropagation.hxx>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Solstice::Game {

void AudioSystem::Initialize(ECS::Registry&) {
    m_Audio = &Core::Audio::AudioManager::Instance();
    m_Initialized = true;
}

void AudioSystem::Shutdown() {
    m_Initialized = false;
}

void AudioSystem::SetHRTFEnabled(bool Enabled) {
    if (m_Audio) m_Audio->SetHRTFEnabled(Enabled);
}

void AudioSystem::SetWaveTracingEnabled(bool Enabled) {
    if (m_Audio) m_Audio->SetWaveTracingEnabled(Enabled);
}

void AudioSystem::SetAmbisonicOrder(int Order) {
    if (m_Audio) m_Audio->SetAmbisonicOrder(Order);
}

void AudioSystem::SetMLSpatializationEnabled(bool Enabled) {
    if (m_Audio) m_Audio->SetMLSpatializationEnabled(Enabled);
}

void AudioSystem::SyncListener(ECS::Registry& Registry) {
    if (!m_Audio) return;
    bool listenerFound = false;
    Registry.ForEach<ECS::AudioListener, ECS::Transform>([&](ECS::EntityId, ECS::AudioListener&, ECS::Transform& tr) {
        if (listenerFound) return;
        Core::Audio::Listener l{};
        l.Position = tr.Position;
        l.Forward = Math::Vec3(0.0f, 0.0f, -1.0f);
        l.Up = Math::Vec3(0.0f, 1.0f, 0.0f);
        l.CurrentReverb = {0.0f, 0.0f, 1.0f};
        l.TargetReverb = Core::Audio::ReverbPresetType::Room;
        m_Audio->SetListener(l);
        listenerFound = true;
    });
}

void AudioSystem::SyncAudioSources(ECS::Registry& Registry) {
    if (!m_Audio) return;
    Registry.ForEach<ECS::AudioSource, ECS::Transform>([&](ECS::EntityId, ECS::AudioSource& s, ECS::Transform& tr) {
        if (!s.IsValid && s.AutoPlay && !s.SoundPath.empty()) {
            s.Handle = m_Audio->CreateEmitter(s.SoundPath.c_str(), tr.Position, s.MaxDistance, s.Loop);
            s.IsValid = (s.Handle != 0);
            if (s.IsValid) {
                m_Audio->SetEmitterVolume(s.Handle, s.Volume);
                m_Audio->SetEmitterRolloff(s.Handle, s.MinDistance, s.MaxDistance, 1.0f, Core::Audio::DistanceModel::Inverse);
                m_Audio->SetEmitterFlags(s.Handle, s.IsDialogue, s.IsCriticalCue, s.Priority);
                m_Audio->ApplySpatialProfile(s.Handle, s.Profile);
            }
        } else if (s.IsValid) {
            m_Audio->UpdateEmitterTransform(s.Handle, tr.Position);
            m_Audio->SetEmitterVolume(s.Handle, s.Volume);
            m_Audio->SetEmitterFocus(s.Handle, s.Focus, 0.05f, 1.0f);
            m_Audio->SetEmitterImmersion(s.Handle, s.Immersion, 0.4f, 0.25f);
            m_Audio->SetEmitterDiffraction(s.Handle, s.Diffraction, 0.15f);
            m_Audio->SetEmitterMotionAdaptation(s.Handle, s.MotionAdaptation);
            m_Audio->SetEmitterAirAbsorption(s.Handle, s.AirAbsorption);
            m_Audio->SetEmitterFlags(s.Handle, s.IsDialogue, s.IsCriticalCue, s.Priority);
        }
    });
}

void AudioSystem::SyncAcousticZones(ECS::Registry& Registry) {
    if (!m_Audio) return;
    std::vector<Core::Audio::AcousticZone> zones;
    Registry.ForEach<ECS::AcousticZoneVolume>([&](ECS::EntityId, ECS::AcousticZoneVolume& z) {
        Core::Audio::AcousticZone zone{};
        zone.Name = z.Name;
        zone.Center = (z.Min + z.Max) * 0.5f;
        zone.Extents = (z.Max - z.Min) * 0.5f;
        zone.Wetness = z.ReverbWetness;
        zone.ObstructionMultiplier = z.ObstructionMultiplier;
        zone.Priority = z.Priority;
        zone.IsSpherical = z.IsSphere;
        zone.Enabled = true;
        zone.MusicPath = z.MusicPath;
        zone.AmbiencePath = z.AmbiencePath;
        const int rp = std::clamp(z.ReverbPreset, static_cast<int>(Core::Audio::ReverbPresetType::None),
                                  static_cast<int>(Core::Audio::ReverbPresetType::Industrial));
        zone.Preset = static_cast<Core::Audio::ReverbPresetType>(rp);
        zones.push_back(zone);
    });
    m_Audio->SetAcousticZones(zones);
}

void AudioSystem::SyncPortals(ECS::Registry& Registry) {
    if (!m_Audio) return;

    Math::Vec3 listenerPos{};
    bool haveListener = false;
    Registry.ForEach<ECS::AudioListener, ECS::Transform>([&](ECS::EntityId, ECS::AudioListener&, ECS::Transform& tr) {
        if (!haveListener) {
            listenerPos = tr.Position;
            haveListener = true;
        }
    });
    if (!haveListener) {
        m_Audio->ClearPortalTransform();
        Render::PortalLightPropagation::ClearActivePortal();
        return;
    }

    float bestDistSq = std::numeric_limits<float>::infinity();
    Math::Matrix4 bestWorldToPartner = Math::Matrix4::Identity();
    bool havePortal = false;
    float bestLightScale = 1.0f;

    Registry.ForEach<ECS::Portal, ECS::Transform>([&](ECS::EntityId, ECS::Portal& portal, ECS::Transform& tr) {
        if (!portal.LinkEnabled) return;

        Math::Matrix4 worldToPartner = Math::Matrix4::Identity();
        if (portal.ManualTopology) {
            worldToPartner = portal.WorldToPartnerWorld;
        } else {
            if (portal.Partner == 0 || !Registry.Valid(portal.Partner)) return;
            if (!Registry.Has<ECS::Transform>(portal.Partner)) return;
            const ECS::Transform& partnerTr = Registry.Get<ECS::Transform>(portal.Partner);
            if (std::abs(tr.Matrix.Determinant()) < 1e-8f) return;
            if (std::abs(partnerTr.Matrix.Determinant()) < 1e-8f) return;
            worldToPartner = partnerTr.Matrix * tr.Matrix.Inverse();
        }

        const float distSq = listenerPos.DistanceSquared(tr.Position);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestWorldToPartner = worldToPartner;
            bestLightScale = std::max(0.f, portal.LightPropagationScale);
            havePortal = true;
        }
    });

    if (havePortal) {
        m_Audio->SetPortalTransform(bestWorldToPartner, true);
        if (bestLightScale > 1e-5f) {
            Render::PortalLightPropagation::ConfigureActivePortal(bestWorldToPartner, 0.32f * bestLightScale);
        } else {
            Render::PortalLightPropagation::ClearActivePortal();
        }
    } else {
        m_Audio->ClearPortalTransform();
        Render::PortalLightPropagation::ClearActivePortal();
    }
}

void AudioSystem::Update(ECS::Registry& Registry, float DeltaTime) {
    if (!m_Initialized) {
        Initialize(Registry);
    }
    SyncListener(Registry);
    SyncPortals(Registry);
    SyncAudioSources(Registry);
    SyncAcousticZones(Registry);
    if (m_Audio) {
        m_Audio->Update(DeltaTime);
    }
}

} // namespace Solstice::Game
