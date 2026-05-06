#pragma once

#include <Math/Vector.hxx>
#include <array>
#include <string>
#include <vector>

namespace Solstice::Core::Audio {

constexpr int HRTF_FIR_LENGTH = 128;

struct HRTFDatabase {
    bool LoadSOFA(const std::string& Path);
    bool LoadDefault();

    void GetFilters(float Azimuth, float Elevation,
                    std::array<float, HRTF_FIR_LENGTH>& Left,
                    std::array<float, HRTF_FIR_LENGTH>& Right) const;

    static void ConvolveSIMD(const float* Input, float* Output, const float* Filter, int Length);

    void Clear();
    bool IsValid() const { return !Filters.empty(); }

private:
    struct FIRFilter {
        alignas(16) float Left[HRTF_FIR_LENGTH];
        alignas(16) float Right[HRTF_FIR_LENGTH];
    };

    std::vector<FIRFilter> Filters;
    int AzimuthSteps{0};
    int ElevationSteps{0};
    float AzimuthStepSize{0.0f};
    float ElevationStepSize{0.0f};

    static float WrapAzimuth(float Azimuth);
    static float ClampElevation(float Elevation);
    void GetNearestIndices(float Azimuth, float Elevation,
                           int& AzIdx0, int& AzIdx1,
                           int& ElIdx0, int& ElIdx1,
                           float& AzFrac, float& ElFrac) const;
    static void BilinearInterpolate(const FIRFilter& F00, const FIRFilter& F01,
                                    const FIRFilter& F10, const FIRFilter& F11,
                                    float AZ, float EL,
                                    std::array<float, HRTF_FIR_LENGTH>& Left,
                                    std::array<float, HRTF_FIR_LENGTH>& Right);
};

} // namespace Solstice::Core::Audio
