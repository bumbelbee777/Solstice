#include "FluidAudioCoupling.hxx"
#include <Core/Audio/Audio.hxx>
#include <algorithm>

namespace Solstice::Core::Audio {

float FluidCoupling::GetLowPassCutoff() const {
    const float d = std::clamp(SmokeDensity, 0.0f, 1.0f);
    return std::lerp(18000.0f, 1200.0f, d);
}

float FluidCoupling::GetPitchShift() const {
    const float tempNorm = std::clamp((Temperature - 20.0f) / 100.0f, -1.0f, 1.0f);
    return 1.0f + 0.06f * tempNorm;
}

float FluidCoupling::GetDopplerShift() const {
    return 1.0f + 0.01f * std::clamp(Velocity, -30.0f, 30.0f);
}

void FluidAudioCoupler::UpdateFluidData(const Solstice::Physics::NSSolver*, const Math::Vec3& Min, const Math::Vec3& Max) {
    m_Min = Min;
    m_Max = Max;
    for (int z = 0; z < kGridDim; ++z) {
        for (int y = 0; y < kGridDim; ++y) {
            for (int x = 0; x < kGridDim; ++x) {
                FluidCoupling& c = m_Grid[x][y][z];
                c.SmokeDensity = 0.0f;
                c.Temperature = 20.0f;
                c.Velocity = 0.0f;
            }
        }
    }
}

FluidCoupling FluidAudioCoupler::SampleAtPoint(const Math::Vec3& Position) const {
    const Math::Vec3 span = m_Max - m_Min;
    const Math::Vec3 rel = Position - m_Min;
    const float fx = std::clamp(rel.x / std::max(0.001f, span.x), 0.0f, 0.999f) * static_cast<float>(kGridDim);
    const float fy = std::clamp(rel.y / std::max(0.001f, span.y), 0.0f, 0.999f) * static_cast<float>(kGridDim);
    const float fz = std::clamp(rel.z / std::max(0.001f, span.z), 0.0f, 0.999f) * static_cast<float>(kGridDim);
    const int x = std::clamp(static_cast<int>(fx), 0, kGridDim - 1);
    const int y = std::clamp(static_cast<int>(fy), 0, kGridDim - 1);
    const int z = std::clamp(static_cast<int>(fz), 0, kGridDim - 1);
    return m_Grid[x][y][z];
}

void FluidAudioCoupler::ApplyToEmitter(AudioManager& Audio, std::uint64_t EmitterHandle, const Math::Vec3& Position, const FluidCoupling& Coupling) {
    (void)Position;
    Audio.SetEmitterAirAbsorption(static_cast<AudioEmitterHandle>(EmitterHandle),
        std::clamp(Coupling.SmokeDensity * 0.8f, 0.0f, 1.0f));
    Audio.SetEmitterDoppler(static_cast<AudioEmitterHandle>(EmitterHandle),
        std::clamp(Coupling.GetDopplerShift(), 0.1f, 3.0f), 343.3f);
}

} // namespace Solstice::Core::Audio
