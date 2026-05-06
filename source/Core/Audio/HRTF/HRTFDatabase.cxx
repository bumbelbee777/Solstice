#include "HRTFDatabase.hxx"
#include <Core/Debug/Debug.hxx>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace Solstice::Core::Audio {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

float HRTFDatabase::WrapAzimuth(float Azimuth) {
    float a = std::fmod(Azimuth, 360.0f);
    if (a < 0.0f) a += 360.0f;
    return a;
}

float HRTFDatabase::ClampElevation(float Elevation) {
    return std::clamp(Elevation, -90.0f, 90.0f);
}

bool HRTFDatabase::LoadSOFA(const std::string& Path) {
    // Minimal in-house path: validate file exists then seed deterministic generic dataset.
    std::ifstream file(Path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    return LoadDefault();
}

bool HRTFDatabase::LoadDefault() {
    Clear();
    AzimuthSteps = 36;
    ElevationSteps = 19;
    AzimuthStepSize = 360.0f / static_cast<float>(AzimuthSteps);
    ElevationStepSize = 180.0f / static_cast<float>(ElevationSteps - 1);
    Filters.resize(static_cast<size_t>(AzimuthSteps * ElevationSteps));

    for (int el = 0; el < ElevationSteps; ++el) {
        const float elDeg = -90.0f + static_cast<float>(el) * ElevationStepSize;
        const float elRad = elDeg * kPi / 180.0f;
        for (int az = 0; az < AzimuthSteps; ++az) {
            const float azDeg = static_cast<float>(az) * AzimuthStepSize;
            const float azRad = azDeg * kPi / 180.0f;
            FIRFilter& f = Filters[static_cast<size_t>(el * AzimuthSteps + az)];
            for (int i = 0; i < HRTF_FIR_LENGTH; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(HRTF_FIR_LENGTH);
                const float decay = std::exp(-5.0f * t);
                const float phase = 18.0f * t + 0.5f * std::sin(elRad);
                const float leftPan = 0.75f + 0.25f * std::sin(azRad);
                const float rightPan = 0.75f + 0.25f * std::sin(azRad + kPi);
                f.Left[i] = decay * std::sin(phase) * leftPan;
                f.Right[i] = decay * std::sin(phase) * rightPan;
            }
            // Add direct impulse for localization stability.
            f.Left[0] += std::max(0.2f, 1.0f - std::abs(std::sin(azRad)));
            f.Right[0] += std::max(0.2f, 1.0f - std::abs(std::sin(azRad + kPi)));
        }
    }
    return true;
}

void HRTFDatabase::Clear() {
    Filters.clear();
    AzimuthSteps = 0;
    ElevationSteps = 0;
    AzimuthStepSize = 0.0f;
    ElevationStepSize = 0.0f;
}

void HRTFDatabase::GetNearestIndices(float Azimuth, float Elevation,
                                     int& AzIdx0, int& AzIdx1,
                                     int& ElIdx0, int& ElIdx1,
                                     float& AzFrac, float& ElFrac) const {
    const float az = WrapAzimuth(Azimuth) / std::max(0.001f, AzimuthStepSize);
    const float el = (ClampElevation(Elevation) + 90.0f) / std::max(0.001f, ElevationStepSize);
    AzIdx0 = std::clamp(static_cast<int>(std::floor(az)), 0, AzimuthSteps - 1);
    AzIdx1 = (AzIdx0 + 1) % AzimuthSteps;
    ElIdx0 = std::clamp(static_cast<int>(std::floor(el)), 0, ElevationSteps - 1);
    ElIdx1 = std::clamp(ElIdx0 + 1, 0, ElevationSteps - 1);
    AzFrac = az - std::floor(az);
    ElFrac = el - std::floor(el);
}

void HRTFDatabase::BilinearInterpolate(const FIRFilter& F00, const FIRFilter& F01,
                                       const FIRFilter& F10, const FIRFilter& F11,
                                       float AZ, float EL,
                                       std::array<float, HRTF_FIR_LENGTH>& Left,
                                       std::array<float, HRTF_FIR_LENGTH>& Right) {
    for (int i = 0; i < HRTF_FIR_LENGTH; ++i) {
        const float l0 = std::lerp(F00.Left[i], F01.Left[i], AZ);
        const float l1 = std::lerp(F10.Left[i], F11.Left[i], AZ);
        const float r0 = std::lerp(F00.Right[i], F01.Right[i], AZ);
        const float r1 = std::lerp(F10.Right[i], F11.Right[i], AZ);
        Left[i] = std::lerp(l0, l1, EL);
        Right[i] = std::lerp(r0, r1, EL);
    }
}

void HRTFDatabase::GetFilters(float Azimuth, float Elevation,
                              std::array<float, HRTF_FIR_LENGTH>& Left,
                              std::array<float, HRTF_FIR_LENGTH>& Right) const {
    Left.fill(0.0f);
    Right.fill(0.0f);
    if (Filters.empty() || AzimuthSteps <= 0 || ElevationSteps <= 0) {
        Left[0] = 1.0f;
        Right[0] = 1.0f;
        return;
    }
    int az0 = 0, az1 = 0, el0 = 0, el1 = 0;
    float azFrac = 0.0f, elFrac = 0.0f;
    GetNearestIndices(Azimuth, Elevation, az0, az1, el0, el1, azFrac, elFrac);
    const FIRFilter& f00 = Filters[static_cast<size_t>(el0 * AzimuthSteps + az0)];
    const FIRFilter& f01 = Filters[static_cast<size_t>(el0 * AzimuthSteps + az1)];
    const FIRFilter& f10 = Filters[static_cast<size_t>(el1 * AzimuthSteps + az0)];
    const FIRFilter& f11 = Filters[static_cast<size_t>(el1 * AzimuthSteps + az1)];
    BilinearInterpolate(f00, f01, f10, f11, azFrac, elFrac, Left, Right);
}

void HRTFDatabase::ConvolveSIMD(const float* Input, float* Output, const float* Filter, int Length) {
    if (!Input || !Output || !Filter || Length <= 0) {
        return;
    }
    for (int i = 0; i < Length; ++i) {
        float sum = 0.0f;
        const int taps = std::min(HRTF_FIR_LENGTH, i + 1);
        for (int t = 0; t < taps; ++t) {
            sum += Input[i - t] * Filter[t];
        }
        Output[i] = sum;
    }
}

} // namespace Solstice::Core::Audio
