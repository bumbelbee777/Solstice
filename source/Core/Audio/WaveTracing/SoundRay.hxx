#pragma once

#include <Math/Vector.hxx>
#include <cstdint>
#include <vector>

namespace Solstice::Core::Audio {

struct SoundRayPacket4 {
    alignas(16) Math::Vec3 Origins[4];
    alignas(16) Math::Vec3 Directions[4];
    alignas(16) float Energies[4];
    alignas(16) float HitDistances[4];
    alignas(16) int BouncesRemaining[4];
    alignas(16) uint32_t SourceIds[4];

    void Clear() {
        for (int i = 0; i < 4; ++i) {
            Origins[i] = Math::Vec3(0.0f, 0.0f, 0.0f);
            Directions[i] = Math::Vec3(0.0f, 0.0f, 0.0f);
            Energies[i] = 0.0f;
            HitDistances[i] = 0.0f;
            BouncesRemaining[i] = 0;
            SourceIds[i] = 0;
        }
    }
    bool AnyActive() const {
        return Energies[0] > 0.0f || Energies[1] > 0.0f || Energies[2] > 0.0f || Energies[3] > 0.0f;
    }
};

struct SoundRayResult {
    float Energy{0.0f};
    float Distance{0.0f};
    float IncidentAngle{0.0f};
    uint32_t SourceId{0};
    bool HitOccluder{false};
};

struct WaveTraceReverbParams {
    float DecayTime{0.0f};
    float LateReverbGain{0.0f};
    float EarlyGain{0.0f};
    float LateDelay{0.0f};
};

} // namespace Solstice::Core::Audio
