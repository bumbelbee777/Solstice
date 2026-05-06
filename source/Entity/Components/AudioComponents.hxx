#pragma once

#include <Core/Audio/Audio.hxx>
#include <Math/Vector.hxx>
#include <string>

namespace Solstice::ECS {

struct AudioSource {
    std::string SoundPath;
    float Volume{1.0f};
    float Pitch{1.0f};
    float MaxDistance{50.0f};
    float MinDistance{1.0f};
    bool Loop{false};
    bool AutoPlay{true};
    int Profile{0};

    float Focus{0.15f};
    float Immersion{0.35f};
    float Diffraction{0.35f};
    float MotionAdaptation{0.20f};
    float AirAbsorption{0.25f};
    bool IsDialogue{false};
    bool IsCriticalCue{false};
    int Priority{0};

    Solstice::Core::Audio::AudioEmitterHandle Handle{0};
    bool IsValid{false};
};

struct AudioListener {
};

struct AcousticZoneVolume {
    std::string Name;
    Math::Vec3 Min{0.0f, 0.0f, 0.0f};
    Math::Vec3 Max{0.0f, 0.0f, 0.0f};
    float Radius{0.0f};
    bool IsSphere{false};

    float ReverbWetness{0.0f};
    int ReverbPreset{1};
    float ObstructionMultiplier{1.0f};
    int Priority{0};

    std::string MusicPath;
    std::string AmbiencePath;

    Solstice::Core::Audio::AudioEmitterHandle AmbienceHandle{0};
};

} // namespace Solstice::ECS
