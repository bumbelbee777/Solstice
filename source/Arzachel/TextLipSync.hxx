#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Solstice::Arzachel {

/// One viseme hold/sample on the timeline (ticks = Parallax scene ticks).
struct VisemeKeyframeTick {
    uint64_t TimeTicks{0};
    std::string VisemeId;
    float Strength{1.f};
};

struct LipSyncTextConfig {
    float BaseStrength{1.0f};
    float VowelBias{1.1f};
    float ConsonantBias{0.95f};
    float Coarticulation{0.35f};
    float PauseStrength{0.25f};
    bool EnableDigraphs{true};     // th, ch, sh, ph, ng, qu, oo, ee
    bool EnablePunctuationPauses{true};
    bool EnableQuestionRise{true};
};

/// English grapheme → viseme id (OVR-style subset). For long dialogue, call once per line.
void BuildVisemeKeyframesFromEnglishText(const std::string& text, uint32_t ticksPerSecond, uint64_t startTick,
    uint64_t endTick, uint32_t maxKeyframes, std::vector<VisemeKeyframeTick>& out, std::string& warnOrEmpty);

void BuildVisemeKeyframesFromEnglishTextEx(const std::string& text, uint32_t ticksPerSecond, uint64_t startTick,
    uint64_t endTick, uint32_t maxKeyframes, const LipSyncTextConfig& config,
    std::vector<VisemeKeyframeTick>& out, std::string& warnOrEmpty);

/// Simple list of (visemeId, strength) covering the utterance — used by `PhonemeDetector`.
std::vector<std::pair<std::string, float>> TextToVisemeStrengthSamples(const std::string& text);

void SampleVisemeAtTick(const std::vector<VisemeKeyframeTick>& keyframes, uint64_t tick,
    std::string& outCurrentViseme, std::string& outNextViseme, float& outBlend, float& outStrength);

void CompactVisemeKeyframes(std::vector<VisemeKeyframeTick>& ioKeyframes, float minStrengthDelta);

} // namespace Solstice::Arzachel
