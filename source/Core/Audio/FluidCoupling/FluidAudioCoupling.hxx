#pragma once

#include <Math/Vector.hxx>
#include <cstdint>

namespace Solstice::Physics { class NSSolver; }

namespace Solstice::Core::Audio {

class AudioManager;

struct FluidCoupling {
    float SmokeDensity{0.0f};
    float Temperature{20.0f};
    float Velocity{0.0f};

    float GetLowPassCutoff() const;
    float GetPitchShift() const;
    float GetDopplerShift() const;
};

class FluidAudioCoupler {
public:
    void UpdateFluidData(const Solstice::Physics::NSSolver* Solver, const Math::Vec3& Min, const Math::Vec3& Max);
    FluidCoupling SampleAtPoint(const Math::Vec3& Position) const;
    void ApplyToEmitter(AudioManager& Audio, std::uint64_t EmitterHandle, const Math::Vec3& Position, const FluidCoupling& Coupling);

private:
    static constexpr int kGridDim = 32;
    FluidCoupling m_Grid[kGridDim][kGridDim][kGridDim]{};
    Math::Vec3 m_Min{0.0f, 0.0f, 0.0f};
    Math::Vec3 m_Max{1.0f, 1.0f, 1.0f};
};

} // namespace Solstice::Core::Audio
