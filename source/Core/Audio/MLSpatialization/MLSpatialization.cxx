#include "MLSpatialization.hxx"
#include <algorithm>
#include <fstream>

namespace Solstice::Core::Audio {

MLSpatializer::MLSpatializer() = default;
MLSpatializer::~MLSpatializer() = default;

void MLSpatializer::Initialize() {
    m_Model = std::make_unique<Solstice::Core::MLP>();
    m_Model->AddLayer(10, 16, Solstice::Core::ActivationType::ReLU);
    m_Model->AddLayer(16, 8, Solstice::Core::ActivationType::ReLU);
    m_Model->AddLayer(8, 5, Solstice::Core::ActivationType::Sigmoid);
    for (size_t i = 0; i < 3; ++i) {
        m_Model->SetLayerFunctionalWeights(i, false);
    }
    m_Enabled = true;
}

void MLSpatializer::Shutdown() {
    m_Model.reset();
    m_TrainingInputs.clear();
    m_TrainingOutputs.clear();
    m_Enabled = false;
}

void MLSpatializer::ExtractFeatures(const Math::Vec3& SourcePos, const Math::Vec3& ListenerPos,
                                    const Math::Vec3& SourceVel, const Math::Vec3& ListenerVel,
                                    float Occlusion, float Distance, float Azimuth,
                                    std::vector<float>& OutFeatures) {
    OutFeatures.resize(10);
    const Math::Vec3 rel = SourcePos - ListenerPos;
    OutFeatures[0] = rel.x;
    OutFeatures[1] = rel.y;
    OutFeatures[2] = rel.z;
    OutFeatures[3] = SourceVel.Magnitude();
    OutFeatures[4] = ListenerVel.Magnitude();
    OutFeatures[5] = std::clamp(Occlusion, 0.0f, 1.0f);
    OutFeatures[6] = std::max(0.0f, Distance);
    OutFeatures[7] = Azimuth;
    OutFeatures[8] = SourceVel.Dot(ListenerVel);
    OutFeatures[9] = (Distance > 0.001f) ? (rel.Dot(SourceVel - ListenerVel) / Distance) : 0.0f;
}

MLSpatializer::SpatialParameters MLSpatializer::Predict(const Math::Vec3& SourcePos,
                                                        const Math::Vec3& ListenerPos,
                                                        const Math::Vec3& SourceVel,
                                                        const Math::Vec3& ListenerVel,
                                                        float Occlusion,
                                                        float Distance,
                                                        float Azimuth) {
    SpatialParameters out{};
    if (!m_Enabled || !m_Model) {
        return out;
    }
    std::vector<float> features;
    ExtractFeatures(SourcePos, ListenerPos, SourceVel, ListenerVel, Occlusion, Distance, Azimuth, features);
    float result[5] = {};
    m_Model->Forward(features.data(), result);
    out.Focus = std::clamp(result[0], 0.0f, 1.0f);
    out.Immersion = std::clamp(result[1], 0.0f, 1.0f);
    out.Diffraction = std::clamp(result[2], 0.0f, 1.0f);
    out.MotionAdaptation = std::clamp(result[3], 0.0f, 1.0f);
    out.AirAbsorption = std::clamp(result[4], 0.0f, 1.0f);
    return out;
}

void MLSpatializer::RecordGroundTruth(const SpatialParameters& Params, float QualityScore) {
    (void)QualityScore;
    m_TrainingOutputs.push_back(std::clamp(Params.Focus, 0.0f, 1.0f));
    m_TrainingOutputs.push_back(std::clamp(Params.Immersion, 0.0f, 1.0f));
    m_TrainingOutputs.push_back(std::clamp(Params.Diffraction, 0.0f, 1.0f));
    m_TrainingOutputs.push_back(std::clamp(Params.MotionAdaptation, 0.0f, 1.0f));
    m_TrainingOutputs.push_back(std::clamp(Params.AirAbsorption, 0.0f, 1.0f));
}

void MLSpatializer::Train() {
    // Minimal uplift: training pipeline placeholder keeps deterministic runtime path.
}

bool MLSpatializer::SaveWeights(const std::string& Path) const {
    std::ofstream f(Path, std::ios::binary);
    if (!f.good()) return false;
    const uint32_t magic = 0x534D4C41; // SMLA
    const uint32_t version = 1;
    f.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    f.write(reinterpret_cast<const char*>(&version), sizeof(version));
    return f.good();
}

bool MLSpatializer::LoadWeights(const std::string& Path) {
    std::ifstream f(Path, std::ios::binary);
    if (!f.good()) return false;
    uint32_t magic = 0;
    uint32_t version = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    f.read(reinterpret_cast<char*>(&version), sizeof(version));
    return f.good() && magic == 0x534D4C41 && version == 1;
}

} // namespace Solstice::Core::Audio
