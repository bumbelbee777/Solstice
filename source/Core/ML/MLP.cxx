#include "MLP.hxx"
#include <fstream>
#include <random>

namespace Solstice::Core {

void MLP::InitializeRandomWeights(float Scale) {
    std::mt19937 rng(m_Seed);
    std::uniform_real_distribution<float> dist(-Scale, Scale);
    for (auto& layer : m_Layers) {
        layer.SetFunctionalWeights(false);
        if (layer.Weights.empty()) {
            layer.Weights.resize(layer.InputSize * layer.OutputSize, 0.0f);
        }
        for (float& w : layer.Weights) {
            w = dist(rng);
        }
        for (float& b : layer.Biases) {
            b = dist(rng);
        }
    }
}

bool MLP::SaveWeights(const std::string& Path) const {
    std::ofstream f(Path, std::ios::binary);
    if (!f.good()) {
        return false;
    }
    const uint32_t magic = 0x504C4D53; // SMLP
    const uint32_t version = 1;
    const uint32_t layers = static_cast<uint32_t>(m_Layers.size());
    f.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    f.write(reinterpret_cast<const char*>(&version), sizeof(version));
    f.write(reinterpret_cast<const char*>(&layers), sizeof(layers));
    for (const auto& layer : m_Layers) {
        const uint32_t in = static_cast<uint32_t>(layer.InputSize);
        const uint32_t out = static_cast<uint32_t>(layer.OutputSize);
        const uint32_t act = static_cast<uint32_t>(layer.Activation);
        const uint32_t wf = layer.UseFunctionalWeights ? 1u : 0u;
        f.write(reinterpret_cast<const char*>(&in), sizeof(in));
        f.write(reinterpret_cast<const char*>(&out), sizeof(out));
        f.write(reinterpret_cast<const char*>(&act), sizeof(act));
        f.write(reinterpret_cast<const char*>(&wf), sizeof(wf));
        if (!layer.UseFunctionalWeights) {
            f.write(reinterpret_cast<const char*>(layer.Weights.data()), sizeof(float) * layer.Weights.size());
        }
        f.write(reinterpret_cast<const char*>(layer.Biases.data()), sizeof(float) * layer.Biases.size());
    }
    return f.good();
}

bool MLP::LoadWeights(const std::string& Path) {
    std::ifstream f(Path, std::ios::binary);
    if (!f.good()) {
        return false;
    }
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t layers = 0;
    f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    f.read(reinterpret_cast<char*>(&version), sizeof(version));
    f.read(reinterpret_cast<char*>(&layers), sizeof(layers));
    if (!f.good() || magic != 0x504C4D53 || version != 1 || layers != m_Layers.size()) {
        return false;
    }
    for (auto& layer : m_Layers) {
        uint32_t in = 0, out = 0, act = 0, wf = 0;
        f.read(reinterpret_cast<char*>(&in), sizeof(in));
        f.read(reinterpret_cast<char*>(&out), sizeof(out));
        f.read(reinterpret_cast<char*>(&act), sizeof(act));
        f.read(reinterpret_cast<char*>(&wf), sizeof(wf));
        if (!f.good() || in != layer.InputSize || out != layer.OutputSize) {
            return false;
        }
        layer.Activation = static_cast<ActivationType>(act);
        layer.UseFunctionalWeights = (wf != 0);
        layer.Weights.resize(layer.InputSize * layer.OutputSize, 0.0f);
        if (!layer.UseFunctionalWeights) {
            f.read(reinterpret_cast<char*>(layer.Weights.data()), sizeof(float) * layer.Weights.size());
        }
        layer.Biases.resize(layer.OutputSize, 0.0f);
        f.read(reinterpret_cast<char*>(layer.Biases.data()), sizeof(float) * layer.Biases.size());
        if (!f.good()) {
            return false;
        }
    }
    return true;
}

} // namespace Solstice::Core

