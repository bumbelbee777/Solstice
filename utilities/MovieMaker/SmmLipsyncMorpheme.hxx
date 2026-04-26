#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Smm::Authoring {
struct LipsyncLineStub;
struct LipsyncSample;
} // namespace Smm::Authoring

namespace Smm::LipsyncMorpheme {

struct MorphemeStats {
    size_t WordCount{0};
    size_t CharCount{0};
    size_t UniqueTokens{0};
    float VowelDensity{0.f};
};

MorphemeStats ComputeMorphemeStats(const std::string& text);

void BuildLipsyncSamples(
    const Smm::Authoring::LipsyncLineStub& line, uint32_t ticksPerSecond, std::vector<Smm::Authoring::LipsyncSample>& out, std::string& warnOrEmpty);

} // namespace Smm::LipsyncMorpheme
