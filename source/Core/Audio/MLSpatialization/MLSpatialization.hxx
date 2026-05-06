#pragma once

#include <Core/ML/MLP.hxx>
#include <Math/Vector.hxx>
#include <memory>
#include <string>
#include <vector>

namespace Solstice::Core::Audio {

class MLSpatializer {
public:
    struct SpatialParameters {
        float Focus{0.15f};
        float Immersion{0.35f};
        float Diffraction{0.35f};
        float MotionAdaptation{0.20f};
        float AirAbsorption{0.25f};
    };

    MLSpatializer();
    ~MLSpatializer();

    void Initialize();
    void Shutdown();

    SpatialParameters Predict(const Math::Vec3& SourcePos,
                              const Math::Vec3& ListenerPos,
                              const Math::Vec3& SourceVel,
                              const Math::Vec3& ListenerVel,
                              float Occlusion,
                              float Distance,
                              float Azimuth);

    void RecordGroundTruth(const SpatialParameters& Params, float QualityScore);
    void Train();
    bool SaveWeights(const std::string& Path) const;
    bool LoadWeights(const std::string& Path);

    void SetEnabled(bool Enabled) { m_Enabled = Enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    std::unique_ptr<Solstice::Core::MLP> m_Model;
    std::vector<float> m_TrainingInputs;
    std::vector<float> m_TrainingOutputs;
    bool m_Enabled{false};

    static void ExtractFeatures(const Math::Vec3& SourcePos, const Math::Vec3& ListenerPos,
                                const Math::Vec3& SourceVel, const Math::Vec3& ListenerVel,
                                float Occlusion, float Distance, float Azimuth,
                                std::vector<float>& OutFeatures);
};

} // namespace Solstice::Core::Audio
