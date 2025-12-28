#pragma once

#include "../Solstice.hxx"
#include "SIMD.hxx"
#include "Async.hxx"
#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>
#include <memory>
#include <future>

namespace Solstice::Core {

enum class ActivationType {
    ReLU,
    Tanh,
    Sigmoid,
    SIREN
};

enum class QuantizationType {
    None,
    INT8,
    INT4
};

// Functional weight generator using SIREN (periodic sine functions)
struct FunctionalWeightGenerator {
    float BaseFrequency = 30.0f;
    float PhaseOffset = 0.0f;

    // Generate weight on-demand using periodic function
    float GenerateWeight(size_t LayerIdx, size_t NeuronIdx, size_t InputIdx, float Phase = 0.0f) const {
        // SIREN-style periodic weight generation
        float frequency = BaseFrequency * (1.0f + LayerIdx * 0.1f);
        float phase = PhaseOffset + Phase + static_cast<float>(LayerIdx + NeuronIdx + InputIdx) * 0.1f;
        return std::sin(frequency * phase);
    }

    void SetFrequency(float Freq) { BaseFrequency = Freq; }
    void SetPhaseOffset(float Phase) { PhaseOffset = Phase; }
};

// Quantized layer wrapper
template<QuantizationType QType>
struct QuantizedLayer {
    static constexpr size_t BitsPerWeight = (QType == QuantizationType::INT8) ? 8 : 4;
    static constexpr size_t WeightsPerByte = (QType == QuantizationType::INT8) ? 1 : 2;

    std::vector<uint8_t> QuantizedWeights;
    float Scale;
    float ZeroPoint;

    void Quantize(const float* Weights, size_t Count) {
        // Find min/max for quantization
        float minVal = Weights[0];
        float maxVal = Weights[0];
        for (size_t i = 1; i < Count; ++i) {
            if (Weights[i] < minVal) minVal = Weights[i];
            if (Weights[i] > maxVal) maxVal = Weights[i];
        }

        if constexpr (QType == QuantizationType::INT8) {
            Scale = (maxVal - minVal) / 255.0f;
            ZeroPoint = minVal;
            QuantizedWeights.resize(Count);
            for (size_t i = 0; i < Count; ++i) {
                int32_t quantized = static_cast<int32_t>((Weights[i] - ZeroPoint) / Scale);
                quantized = std::max(0, std::min(255, quantized));
                QuantizedWeights[i] = static_cast<uint8_t>(quantized);
            }
        } else if constexpr (QType == QuantizationType::INT4) {
            Scale = (maxVal - minVal) / 15.0f;
            ZeroPoint = minVal;
            QuantizedWeights.resize((Count + 1) / 2);
            for (size_t i = 0; i < Count; i += 2) {
                int32_t q0 = static_cast<int32_t>((Weights[i] - ZeroPoint) / Scale);
                q0 = std::max(0, std::min(15, q0));
                if (i + 1 < Count) {
                    int32_t q1 = static_cast<int32_t>((Weights[i + 1] - ZeroPoint) / Scale);
                    q1 = std::max(0, std::min(15, q1));
                    QuantizedWeights[i / 2] = static_cast<uint8_t>(q0 | (q1 << 4));
                } else {
                    QuantizedWeights[i / 2] = static_cast<uint8_t>(q0);
                }
            }
        }
    }

    float Dequantize(size_t Index) const {
        if constexpr (QType == QuantizationType::INT8) {
            return static_cast<float>(QuantizedWeights[Index]) * Scale + ZeroPoint;
        } else if constexpr (QType == QuantizationType::INT4) {
            size_t byteIdx = Index / 2;
            uint8_t byte = QuantizedWeights[byteIdx];
            if (Index % 2 == 0) {
                return static_cast<float>(byte & 0x0F) * Scale + ZeroPoint;
            } else {
                return static_cast<float>((byte >> 4) & 0x0F) * Scale + ZeroPoint;
            }
        }
        return 0.0f;
    }
};

// Linear (fully connected) layer
struct LinearLayer {
    size_t InputSize;
    size_t OutputSize;
    bool UseFunctionalWeights;
    FunctionalWeightGenerator WeightGen;
    QuantizationType QuantType;

    // Storage for quantized weights
    std::unique_ptr<QuantizedLayer<QuantizationType::INT8>> QuantizedWeightsINT8;
    std::unique_ptr<QuantizedLayer<QuantizationType::INT4>> QuantizedWeightsINT4;

    // Storage for full-precision weights (if not using functional)
    std::vector<float> Weights;
    std::vector<float> Biases;

    ActivationType Activation;

    LinearLayer(size_t InSize, size_t OutSize, ActivationType Act = ActivationType::ReLU)
        : InputSize(InSize), OutputSize(OutSize), UseFunctionalWeights(true),
          QuantType(QuantizationType::None), Activation(Act) {
        Biases.resize(OutputSize, 0.0f);
    }

    // Move constructor
    LinearLayer(LinearLayer&& Other) noexcept = default;

    // Move assignment
    LinearLayer& operator=(LinearLayer&& Other) noexcept = default;

    // Delete copy constructor and assignment (required because of unique_ptr members)
    LinearLayer(const LinearLayer&) = delete;
    LinearLayer& operator=(const LinearLayer&) = delete;

    void SetQuantization(QuantizationType Type) {
        QuantType = Type;
    }

    void SetFunctionalWeights(bool Use) {
        UseFunctionalWeights = Use;
        if (!Use && Weights.empty()) {
            Weights.resize(InputSize * OutputSize, 0.0f);
        }
    }

    // Forward pass with SIMD acceleration
    void Forward(const float* Input, float* Output) const {
        for (size_t outIdx = 0; outIdx < OutputSize; ++outIdx) {
            float sum = Biases[outIdx];

            // SIMD-accelerated dot product
            size_t simdCount = InputSize / 4;
            Solstice::Core::SIMD::Vec4 simdSum(0.0f, 0.0f, 0.0f, 0.0f);

            for (size_t i = 0; i < simdCount; ++i) {
                Solstice::Core::SIMD::Vec4 inputVec = Solstice::Core::SIMD::Vec4::Load(&Input[i * 4]);
                Solstice::Core::SIMD::Vec4 weightVec;

                if (UseFunctionalWeights) {
                    float w0 = WeightGen.GenerateWeight(0, outIdx, i * 4 + 0);
                    float w1 = WeightGen.GenerateWeight(0, outIdx, i * 4 + 1);
                    float w2 = WeightGen.GenerateWeight(0, outIdx, i * 4 + 2);
                    float w3 = WeightGen.GenerateWeight(0, outIdx, i * 4 + 3);
                    weightVec = Solstice::Core::SIMD::Vec4(w0, w1, w2, w3);
                } else {
                    float w0, w1, w2, w3;
                    if (QuantType == QuantizationType::INT8 && QuantizedWeightsINT8) {
                        w0 = QuantizedWeightsINT8->Dequantize(outIdx * InputSize + i * 4 + 0);
                        w1 = QuantizedWeightsINT8->Dequantize(outIdx * InputSize + i * 4 + 1);
                        w2 = QuantizedWeightsINT8->Dequantize(outIdx * InputSize + i * 4 + 2);
                        w3 = QuantizedWeightsINT8->Dequantize(outIdx * InputSize + i * 4 + 3);
                    } else if (QuantType == QuantizationType::INT4 && QuantizedWeightsINT4) {
                        w0 = QuantizedWeightsINT4->Dequantize(outIdx * InputSize + i * 4 + 0);
                        w1 = QuantizedWeightsINT4->Dequantize(outIdx * InputSize + i * 4 + 1);
                        w2 = QuantizedWeightsINT4->Dequantize(outIdx * InputSize + i * 4 + 2);
                        w3 = QuantizedWeightsINT4->Dequantize(outIdx * InputSize + i * 4 + 3);
                    } else {
                        w0 = Weights[outIdx * InputSize + i * 4 + 0];
                        w1 = Weights[outIdx * InputSize + i * 4 + 1];
                        w2 = Weights[outIdx * InputSize + i * 4 + 2];
                        w3 = Weights[outIdx * InputSize + i * 4 + 3];
                    }
                    weightVec = Solstice::Core::SIMD::Vec4(w0, w1, w2, w3);
                }

                simdSum = simdSum + (inputVec * weightVec);
            }

            // Sum all elements of simdSum
            Solstice::Core::SIMD::Vec4 ones(1.0f, 1.0f, 1.0f, 1.0f);
            sum += simdSum.Dot(ones);

            // Handle remaining elements
            for (size_t i = simdCount * 4; i < InputSize; ++i) {
                float weight;
                if (UseFunctionalWeights) {
                    weight = WeightGen.GenerateWeight(0, outIdx, i);
                } else {
                    if (QuantType == QuantizationType::INT8 && QuantizedWeightsINT8) {
                        weight = QuantizedWeightsINT8->Dequantize(outIdx * InputSize + i);
                    } else if (QuantType == QuantizationType::INT4 && QuantizedWeightsINT4) {
                        weight = QuantizedWeightsINT4->Dequantize(outIdx * InputSize + i);
                    } else {
                        weight = Weights[outIdx * InputSize + i];
                    }
                }
                sum += Input[i] * weight;
            }

            // Apply activation
            Output[outIdx] = ApplyActivation(sum);
        }
    }

    // Batched forward pass (SoA layout)
    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        for (size_t batchIdx = 0; batchIdx < BatchSize; ++batchIdx) {
            Forward(&InputBatch[batchIdx * InputSize], &OutputBatch[batchIdx * OutputSize]);
        }
    }

private:
    float ApplyActivation(float X) const {
        switch (Activation) {
            case ActivationType::ReLU:
                return std::max(0.0f, X);
            case ActivationType::Tanh:
                return std::tanh(X);
            case ActivationType::Sigmoid:
                return 1.0f / (1.0f + std::exp(-X));
            case ActivationType::SIREN:
                return std::sin(30.0f * X);
            default:
                return X;
        }
    }
};

// Multi-Layer Perceptron
class SOLSTICE_API MLP {
public:
    MLP() = default;

    // Move constructor
    MLP(MLP&& Other) noexcept : m_Layers(std::move(Other.m_Layers)) {}

    // Move assignment
    MLP& operator=(MLP&& Other) noexcept {
        if (this != &Other) {
            m_Layers = std::move(Other.m_Layers);
        }
        return *this;
    }

    // Delete copy operations (because LinearLayer is non-copyable)
    MLP(const MLP&) = delete;
    MLP& operator=(const MLP&) = delete;

    void AddLayer(size_t InputSize, size_t OutputSize, ActivationType Act = ActivationType::ReLU) {
        m_Layers.emplace_back(InputSize, OutputSize, Act);
        if (m_Layers.size() > 1) {
            // Verify layer connectivity
            if (m_Layers[m_Layers.size() - 2].OutputSize != InputSize) {
                // Adjust if needed
            }
        }
    }

    void SetLayerQuantization(size_t LayerIdx, QuantizationType Type) {
        if (LayerIdx < m_Layers.size()) {
            m_Layers[LayerIdx].SetQuantization(Type);
        }
    }

    void SetLayerFunctionalWeights(size_t LayerIdx, bool Use) {
        if (LayerIdx < m_Layers.size()) {
            m_Layers[LayerIdx].SetFunctionalWeights(Use);
        }
    }

    void Forward(const float* Input, float* Output) const {
        if (m_Layers.empty()) return;

        std::vector<float> tempInput(Input, Input + m_Layers[0].InputSize);
        std::vector<float> tempOutput;

        for (size_t i = 0; i < m_Layers.size(); ++i) {
            tempOutput.resize(m_Layers[i].OutputSize);
            m_Layers[i].Forward(tempInput.data(), tempOutput.data());

            if (i < m_Layers.size() - 1) {
                tempInput = std::move(tempOutput);
            }
        }

        std::copy(tempOutput.begin(), tempOutput.end(), Output);
    }

    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        if (m_Layers.empty()) return;

        size_t inputSize = m_Layers[0].InputSize;
        size_t outputSize = m_Layers.back().OutputSize;

        std::vector<float> tempInput(InputBatch, InputBatch + inputSize * BatchSize);
        std::vector<float> tempOutput;

        for (size_t i = 0; i < m_Layers.size(); ++i) {
            tempOutput.resize(m_Layers[i].OutputSize * BatchSize);
            m_Layers[i].ForwardBatch(tempInput.data(), BatchSize, tempOutput.data());

            if (i < m_Layers.size() - 1) {
                tempInput = std::move(tempOutput);
            }
        }

        std::copy(tempOutput.begin(), tempOutput.end(), OutputBatch);
    }

    std::future<void> ForwardAsync(const float* Input, float* Output) const {
        return Solstice::Core::JobSystem::Instance().SubmitAsync([this, Input, Output]() {
            Forward(Input, Output);
        });
    }

    std::future<void> ForwardBatchAsync(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        return Solstice::Core::JobSystem::Instance().SubmitAsync([this, InputBatch, BatchSize, OutputBatch]() {
            ForwardBatch(InputBatch, BatchSize, OutputBatch);
        });
    }

    size_t GetInputSize() const {
        return m_Layers.empty() ? 0 : m_Layers[0].InputSize;
    }

    size_t GetOutputSize() const {
        return m_Layers.empty() ? 0 : m_Layers.back().OutputSize;
    }

private:
    std::vector<LinearLayer> m_Layers;
};

} // namespace Solstice::Core
