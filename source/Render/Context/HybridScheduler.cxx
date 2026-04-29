#include "Render/Context/HybridScheduler.hxx"
#include <algorithm>

namespace Solstice::Render {

void DrawFeature::Normalize(const DrawFeatureStats& stats) {
    float f[12];
    ToArray(f);
    for (size_t i = 0; i < 12; ++i) {
        const float s = stats.StdDev[i] > 1e-5f ? stats.StdDev[i] : 1.0f;
        f[i] = (f[i] - stats.Mean[i]) / s;
    }
    ScreenArea = f[0]; Depth = f[1]; DepthVariance = f[2]; SilhouetteRatio = f[3];
    MotionMagnitude = f[4]; WasVisibleLastFrame = f[5]; OcclusionProbability = f[6];
    GpuBusy = f[7]; CpuBusy = f[8]; MeshletSize = f[9]; MaterialComplexity = f[10]; Reserved = f[11];
}

void DrawFeature::ToArray(float out[12]) const {
    out[0] = ScreenArea; out[1] = Depth; out[2] = DepthVariance; out[3] = SilhouetteRatio;
    out[4] = MotionMagnitude; out[5] = WasVisibleLastFrame; out[6] = OcclusionProbability;
    out[7] = GpuBusy; out[8] = CpuBusy; out[9] = MeshletSize; out[10] = MaterialComplexity; out[11] = 1.0f;
}

HybridScheduler::HybridScheduler() = default;

void HybridScheduler::Initialize() {
    m_CpuGpuClassifier = std::make_unique<Core::MLP>();
    m_CpuGpuClassifier->AddLayer(12, 16, Core::ActivationType::ReLU);
    m_CpuGpuClassifier->AddLayer(16, 1, Core::ActivationType::Sigmoid);

    m_LodSelector = std::make_unique<Core::MLP>();
    m_LodSelector->AddLayer(12, 16, Core::ActivationType::ReLU);
    m_LodSelector->AddLayer(16, 4, Core::ActivationType::Sigmoid);

    m_FramePredictorCnn = std::make_unique<Core::CNN>();
    m_FramePredictorCnn->AddConvolution(1, 4, 3, 4, 4, 1, 1);
    m_FramePredictorCnn->AddPooling(Core::PoolingType::Average, 2, 2);
}

void HybridScheduler::UpdateHeuristics(float) {
}

void HybridScheduler::ScheduleMeshlet(const Meshlet&, const DrawFeature&) {
}

void HybridScheduler::TrainModels(const std::vector<DrawFeature>& features,
                                  const std::vector<float>& actualGpuTimes,
                                  const std::vector<float>& actualCpuTimes) {
    if (!m_CpuGpuClassifier || features.empty() || actualGpuTimes.size() != features.size() || actualCpuTimes.size() != features.size()) {
        return;
    }
    std::vector<float> inputs(features.size() * 12);
    std::vector<float> labels(features.size());
    for (size_t i = 0; i < features.size(); ++i) {
        features[i].ToArray(&inputs[i * 12]);
        labels[i] = actualGpuTimes[i] < actualCpuTimes[i] ? 1.0f : 0.0f;
    }
    // Current Core::MLP exposes forward APIs; training is reserved for future extension.
}

float HybridScheduler::PredictGpuProbability(const DrawFeature& features) {
    if (!m_CpuGpuClassifier) {
        return 0.5f;
    }
    float input[12];
    features.ToArray(input);
    float output[1]{0.5f};
    m_CpuGpuClassifier->Forward(input, output);
    return std::clamp(output[0], 0.0f, 1.0f);
}

uint32_t HybridScheduler::PredictLod(const DrawFeature& features) {
    if (!m_LodSelector) {
        return 0;
    }
    float input[12];
    float output[4]{};
    features.ToArray(input);
    m_LodSelector->Forward(input, output);
    uint32_t best = 0;
    for (uint32_t i = 1; i < 4; ++i) {
        if (output[i] > output[best]) best = i;
    }
    return best;
}

} // namespace Solstice::Render
