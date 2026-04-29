#pragma once

#include "Core/ML/MLP.hxx"
#include "Core/ML/CNN.hxx"
#include "Render/Assets/MeshletBuilder.hxx"
#include <array>
#include <memory>

namespace Solstice::Render {

struct DrawFeatureStats {
    float Mean[12]{};
    float StdDev[12]{};
};

struct DrawFeature {
    float ScreenArea{0.0f};
    float Depth{0.0f};
    float DepthVariance{0.0f};
    float SilhouetteRatio{0.0f};
    float MotionMagnitude{0.0f};
    float WasVisibleLastFrame{0.0f};
    float OcclusionProbability{0.0f};
    float GpuBusy{0.0f};
    float CpuBusy{0.0f};
    float MeshletSize{0.0f};
    float MaterialComplexity{0.0f};
    float Reserved{0.0f};

    void Normalize(const DrawFeatureStats& stats);
    void ToArray(float out[12]) const;
};

struct RenderCommand {
    uint64_t MeshletId : 20;
    uint64_t Lod : 4;
    uint64_t MaterialId : 20;
    uint64_t TransformId : 16;
    uint32_t TileX : 6;
    uint32_t TileY : 6;
    uint32_t GpuIndirectOffset : 20;
    float DepthSortKey;
    uint32_t NextFree;
};

class HybridScheduler {
public:
    HybridScheduler();
    void Initialize();
    void UpdateHeuristics(float deltaTime);
    void ScheduleMeshlet(const Meshlet& meshlet, const DrawFeature& features);
    void TrainModels(const std::vector<DrawFeature>& features,
                     const std::vector<float>& actualGpuTimes,
                     const std::vector<float>& actualCpuTimes);
    float PredictGpuProbability(const DrawFeature& features);
    uint32_t PredictLod(const DrawFeature& features);

private:
    std::unique_ptr<Core::MLP> m_CpuGpuClassifier;
    std::unique_ptr<Core::MLP> m_LodSelector;
    std::unique_ptr<Core::CNN> m_FramePredictorCnn;
    DrawFeatureStats m_FeatureStats{};
};

} // namespace Solstice::Render
