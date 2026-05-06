#pragma once

#include <Physics/Integration/PhysicsBackend.hxx>
#include <Math/Vector.hxx>
#include <array>

namespace Solstice::Physics {

struct VehicleWheel {
    Math::Vec3 LocalOffset{0.0f, 0.0f, 0.0f};
    float Radius{0.34f};
    float SuspensionRestLength{0.35f};
    float SuspensionStiffness{18000.0f};
    float SuspensionDamping{1800.0f};
    bool Steerable{false};
    bool Driven{true};
    bool Braking{true};
};

struct Vehicle {
    VehicleConfig Config{};
    std::array<VehicleWheel, 4> Wheels{};
    std::array<WheelState, 4> WheelStates{};

    float EngineForce{9000.0f};
    float BrakeForce{6000.0f};
    float MaxSteerAngleRadians{0.55f};
    float YawStability{1.8f};
    float LateralGrip{4.5f};
    float RollingResistance{0.35f};
    float LongitudinalGrip{3.0f};
    float MaxSpeed{90.0f};
    float DifferentialBias{0.15f};
    float AckermannStrength{1.0f};

    float ThrottleInput{0.0f};
    float BrakeInput{0.0f};
    float SteeringInput{0.0f};
    bool Handbrake{false};
    bool Enabled{true};

    void BuildDefault(const VehicleConfig& cfg) {
        Config = cfg;
        const float halfTrack = cfg.TrackWidth * 0.5f;
        const float halfWheelBase = cfg.WheelBase * 0.5f;

        Wheels[0].LocalOffset = Math::Vec3(-halfTrack, 0.0f, halfWheelBase);
        Wheels[1].LocalOffset = Math::Vec3(halfTrack, 0.0f, halfWheelBase);
        Wheels[2].LocalOffset = Math::Vec3(-halfTrack, 0.0f, -halfWheelBase);
        Wheels[3].LocalOffset = Math::Vec3(halfTrack, 0.0f, -halfWheelBase);

        Wheels[0].Steerable = true;
        Wheels[1].Steerable = true;
        Wheels[2].Driven = true;
        Wheels[3].Driven = true;

        for (size_t i = 0; i < WheelStates.size(); ++i) {
            WheelStates[i].SuspensionLength = Wheels[i].SuspensionRestLength;
            WheelStates[i].AngularSpeed = 0.0f;
        }
    }
};

} // namespace Solstice::Physics
