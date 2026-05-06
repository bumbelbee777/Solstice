#pragma once

#include "../../Solstice.hxx"
#include <Core/Audio/Audio.hxx>
#include <Core/Audio/HRTF/HRTFRenderer.hxx>
#include <Core/Audio/WaveTracing/WaveTracer.hxx>
#include <Core/Audio/Ambisonics/AmbisonicCodec.hxx>
#include <Core/Audio/FluidCoupling/FluidAudioCoupling.hxx>
#include <Core/Audio/PortalAudio/PortalAudio.hxx>
#include <Core/Audio/MLSpatialization/MLSpatialization.hxx>
#include <Entity/System.hxx>

namespace Solstice::ECS { class Registry; }

namespace Solstice::Game {

class SOLSTICE_API AudioSystem : public ECS::ISystem {
public:
    void Initialize(ECS::Registry& Registry);
    void Update(ECS::Registry& Registry, float DeltaTime) override;
    void Shutdown();

    void SyncAudioSources(ECS::Registry& Registry);
    void SyncListener(ECS::Registry& Registry);
    void SyncAcousticZones(ECS::Registry& Registry);
    void SyncPortals(ECS::Registry& Registry);

    void SetHRTFEnabled(bool Enabled);
    void SetWaveTracingEnabled(bool Enabled);
    void SetAmbisonicOrder(int Order);
    void SetMLSpatializationEnabled(bool Enabled);

private:
    Core::Audio::AudioManager* m_Audio{nullptr};
    bool m_Initialized{false};
};

} // namespace Solstice::Game
