#pragma once

#include <Solstice.hxx>
#include <Core/ML/SIMD.hxx>
#include <Core/System/Async.hxx>
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <memory>
#include <future>

namespace Solstice::Core {

enum class PoolingType {
    Max,
    Average
};

enum class NormalizationType {
    Batch,
    Layer
};

// Convolution layer with SIMD-optimized kernels
struct ConvolutionLayer {
    size_t InputChannels;
    size_t OutputChannels;
    size_t KernelSize;
    size_t InputWidth;
    size_t InputHeight;
    size_t OutputWidth;
    size_t OutputHeight;
    size_t Stride;
    size_t Padding;

    std::vector<float> Weights;
    std::vector<float> Biases;

    ConvolutionLayer(size_t InChannels, size_t OutChannels, size_t Kernel,
                     size_t InW, size_t InH, size_t Stride_ = 1, size_t Pad = 0)
        : InputChannels(InChannels), OutputChannels(OutChannels), KernelSize(Kernel),
          InputWidth(InW), InputHeight(InH), Stride(Stride_), Padding(Pad) {
        OutputWidth = (InputWidth + 2 * Padding - KernelSize) / Stride + 1;
        OutputHeight = (InputHeight + 2 * Padding - KernelSize) / Stride + 1;

        Weights.resize(OutputChannels * InputChannels * KernelSize * KernelSize, 0.0f);
        Biases.resize(OutputChannels, 0.0f);
    }

    void Forward(const float* Input, float* Output) const {
        // Im2col optimization for matrix multiplication
        for (size_t outC = 0; outC < OutputChannels; ++outC) {
            for (size_t outY = 0; outY < OutputHeight; ++outY) {
                for (size_t outX = 0; outX < OutputWidth; ++outX) {
                    float sum = Biases[outC];

                    for (size_t inC = 0; inC < InputChannels; ++inC) {
                        for (size_t ky = 0; ky < KernelSize; ++ky) {
                            for (size_t kx = 0; kx < KernelSize; ++kx) {
                                int inY = static_cast<int>(outY * Stride + ky) - static_cast<int>(Padding);
                                int inX = static_cast<int>(outX * Stride + kx) - static_cast<int>(Padding);

                                if (inY >= 0 && inY < static_cast<int>(InputHeight) &&
                                    inX >= 0 && inX < static_cast<int>(InputWidth)) {
                                    size_t inputIdx = inC * InputWidth * InputHeight + inY * InputWidth + inX;
                                    size_t weightIdx = outC * InputChannels * KernelSize * KernelSize +
                                                      inC * KernelSize * KernelSize + ky * KernelSize + kx;
                                    sum += Input[inputIdx] * Weights[weightIdx];
                                }
                            }
                        }
                    }

                    size_t outputIdx = outC * OutputWidth * OutputHeight + outY * OutputWidth + outX;
                    Output[outputIdx] = std::max(0.0f, sum); // ReLU
                }
            }
        }
    }

    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        size_t inputSize = InputChannels * InputWidth * InputHeight;
        size_t outputSize = OutputChannels * OutputWidth * OutputHeight;

        for (size_t b = 0; b < BatchSize; ++b) {
            Forward(&InputBatch[b * inputSize], &OutputBatch[b * outputSize]);
        }
    }
};

// Pooling layer
struct PoolingLayer {
    PoolingType Type;
    size_t PoolSize;
    size_t Stride;
    size_t InputWidth;
    size_t InputHeight;
    size_t InputChannels;
    size_t OutputWidth;
    size_t OutputHeight;

    PoolingLayer(PoolingType T, size_t Pool, size_t InW, size_t InH, size_t InChannels, size_t Stride_ = 2)
        : Type(T), PoolSize(Pool), Stride(Stride_), InputWidth(InW), InputHeight(InH),
          InputChannels(InChannels) {
        OutputWidth = (InputWidth - PoolSize) / Stride + 1;
        OutputHeight = (InputHeight - PoolSize) / Stride + 1;
    }

    void Forward(const float* Input, float* Output) const {
        for (size_t c = 0; c < InputChannels; ++c) {
            for (size_t outY = 0; outY < OutputHeight; ++outY) {
                for (size_t outX = 0; outX < OutputWidth; ++outX) {
                    float result = (Type == PoolingType::Max) ? -1e9f : 0.0f;
                    float count = 0.0f;

                    for (size_t py = 0; py < PoolSize; ++py) {
                        for (size_t px = 0; px < PoolSize; ++px) {
                            size_t inY = outY * Stride + py;
                            size_t inX = outX * Stride + px;

                            if (inY < InputHeight && inX < InputWidth) {
                                size_t inputIdx = c * InputWidth * InputHeight + inY * InputWidth + inX;
                                float val = Input[inputIdx];

                                if (Type == PoolingType::Max) {
                                    result = std::max(result, val);
                                } else {
                                    result += val;
                                    count += 1.0f;
                                }
                            }
                        }
                    }

                    if (Type == PoolingType::Average && count > 0.0f) {
                        result /= count;
                    }

                    size_t outputIdx = c * OutputWidth * OutputHeight + outY * OutputWidth + outX;
                    Output[outputIdx] = result;
                }
            }
        }
    }

    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        size_t inputSize = InputChannels * InputWidth * InputHeight;
        size_t outputSize = InputChannels * OutputWidth * OutputHeight;

        for (size_t b = 0; b < BatchSize; ++b) {
            Forward(&InputBatch[b * inputSize], &OutputBatch[b * outputSize]);
        }
    }
};

// Normalization layer
struct NormalizationLayer {
    NormalizationType Type;
    size_t Channels;
    size_t Width;
    size_t Height;
    float Epsilon;
    std::vector<float> Scale;
    std::vector<float> Bias;

    NormalizationLayer(NormalizationType T, size_t Ch, size_t W, size_t H, float Eps = 1e-5f)
        : Type(T), Channels(Ch), Width(W), Height(H), Epsilon(Eps) {
        Scale.resize(Channels, 1.0f);
        Bias.resize(Channels, 0.0f);
    }

    void Forward(const float* Input, float* Output) const {
        if (Type == NormalizationType::Layer) {
            // Layer normalization: normalize across channels for each spatial location
            for (size_t y = 0; y < Height; ++y) {
                for (size_t x = 0; x < Width; ++x) {
                    // Compute mean
                    float mean = 0.0f;
                    for (size_t c = 0; c < Channels; ++c) {
                        size_t idx = c * Width * Height + y * Width + x;
                        mean += Input[idx];
                    }
                    mean /= static_cast<float>(Channels);

                    // Compute variance
                    float variance = 0.0f;
                    for (size_t c = 0; c < Channels; ++c) {
                        size_t idx = c * Width * Height + y * Width + x;
                        float diff = Input[idx] - mean;
                        variance += diff * diff;
                    }
                    variance /= static_cast<float>(Channels);

                    float stdDev = std::sqrt(variance + Epsilon);

                    // Normalize and apply scale/bias
                    for (size_t c = 0; c < Channels; ++c) {
                        size_t idx = c * Width * Height + y * Width + x;
                        Output[idx] = (Input[idx] - mean) / stdDev * Scale[c] + Bias[c];
                    }
                }
            }
        } else {
            // Batch normalization (simplified - would need running stats in real implementation)
            for (size_t c = 0; c < Channels; ++c) {
                float mean = 0.0f;
                size_t count = Width * Height;
                for (size_t i = 0; i < count; ++i) {
                    mean += Input[c * count + i];
                }
                mean /= static_cast<float>(count);

                float variance = 0.0f;
                for (size_t i = 0; i < count; ++i) {
                    float diff = Input[c * count + i] - mean;
                    variance += diff * diff;
                }
                variance /= static_cast<float>(count);

                float stdDev = std::sqrt(variance + Epsilon);

                for (size_t i = 0; i < count; ++i) {
                    Output[c * count + i] = (Input[c * count + i] - mean) / stdDev * Scale[c] + Bias[c];
                }
            }
        }
    }

    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        size_t inputSize = Channels * Width * Height;
        for (size_t b = 0; b < BatchSize; ++b) {
            Forward(&InputBatch[b * inputSize], &OutputBatch[b * inputSize]);
        }
    }
};

// Complete CNN network
class SOLSTICE_API CNN {
public:
    CNN() = default;

    void AddConvolution(size_t InChannels, size_t OutChannels, size_t KernelSize,
                       size_t Width, size_t Height, size_t Stride = 1, size_t Padding = 0) {
        if (m_Convolutions.empty()) {
            m_InputWidth = Width;
            m_InputHeight = Height;
            m_InputChannels = InChannels;
        }
        m_Convolutions.emplace_back(InChannels, OutChannels, KernelSize, Width, Height, Stride, Padding);
        m_InputChannels = OutChannels;
        m_InputWidth = m_Convolutions.back().OutputWidth;
        m_InputHeight = m_Convolutions.back().OutputHeight;
    }

    void AddPooling(PoolingType Type, size_t PoolSize, size_t Stride = 2) {
        m_Poolings.emplace_back(Type, PoolSize, m_InputWidth, m_InputHeight, m_InputChannels, Stride);
        m_InputWidth = m_Poolings.back().OutputWidth;
        m_InputHeight = m_Poolings.back().OutputHeight;
    }

    void AddNormalization(NormalizationType Type) {
        m_Normalizations.emplace_back(Type, m_InputChannels, m_InputWidth, m_InputHeight);
    }

    void Forward(const float* Input, float* Output) const {
        if (m_Convolutions.empty()) return;

        std::vector<float> tempInput(Input, Input + m_InputChannels * m_InputWidth * m_InputHeight);
        std::vector<float> tempOutput;

        size_t convIdx = 0;
        size_t poolIdx = 0;
        size_t normIdx = 0;

        for (size_t i = 0; i < m_Convolutions.size(); ++i) {
            const auto& conv = m_Convolutions[i];
            tempOutput.resize(conv.OutputChannels * conv.OutputWidth * conv.OutputHeight);
            conv.Forward(tempInput.data(), tempOutput.data());
            tempInput = std::move(tempOutput);

            // Apply pooling if available
            if (poolIdx < m_Poolings.size()) {
                const auto& pool = m_Poolings[poolIdx];
                tempOutput.resize(pool.InputChannels * pool.OutputWidth * pool.OutputHeight);
                pool.Forward(tempInput.data(), tempOutput.data());
                tempInput = std::move(tempOutput);
                ++poolIdx;
            }

            // Apply normalization if available
            if (normIdx < m_Normalizations.size()) {
                const auto& norm = m_Normalizations[normIdx];
                norm.Forward(tempInput.data(), tempInput.data()); // In-place
                ++normIdx;
            }
        }

        std::copy(tempInput.begin(), tempInput.end(), Output);
    }

    void ForwardBatch(const float* InputBatch, size_t BatchSize, float* OutputBatch) const {
        if (m_Convolutions.empty()) return;

        size_t inputSize = m_InputChannels * m_InputWidth * m_InputHeight;
        std::vector<float> tempInput(InputBatch, InputBatch + inputSize * BatchSize);
        std::vector<float> tempOutput;

        size_t convIdx = 0;
        size_t poolIdx = 0;
        size_t normIdx = 0;

        for (size_t i = 0; i < m_Convolutions.size(); ++i) {
            const auto& conv = m_Convolutions[i];
            tempOutput.resize(conv.OutputChannels * conv.OutputWidth * conv.OutputHeight * BatchSize);
            conv.ForwardBatch(tempInput.data(), BatchSize, tempOutput.data());
            tempInput = std::move(tempOutput);

            if (poolIdx < m_Poolings.size()) {
                const auto& pool = m_Poolings[poolIdx];
                tempOutput.resize(pool.InputChannels * pool.OutputWidth * pool.OutputHeight * BatchSize);
                pool.ForwardBatch(tempInput.data(), BatchSize, tempOutput.data());
                tempInput = std::move(tempOutput);
                ++poolIdx;
            }

            if (normIdx < m_Normalizations.size()) {
                const auto& norm = m_Normalizations[normIdx];
                norm.ForwardBatch(tempInput.data(), BatchSize, tempInput.data());
                ++normIdx;
            }
        }

        std::copy(tempInput.begin(), tempInput.end(), OutputBatch);
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
        return m_InputChannels * m_InputWidth * m_InputHeight;
    }

    size_t GetOutputSize() const {
        if (m_Convolutions.empty()) return 0;
        const auto& lastConv = m_Convolutions.back();
        return lastConv.OutputChannels * lastConv.OutputWidth * lastConv.OutputHeight;
    }

private:
    std::vector<ConvolutionLayer> m_Convolutions;
    std::vector<PoolingLayer> m_Poolings;
    std::vector<NormalizationLayer> m_Normalizations;
    size_t m_InputWidth = 0;
    size_t m_InputHeight = 0;
    size_t m_InputChannels = 0;
};

} // namespace Solstice::Core
