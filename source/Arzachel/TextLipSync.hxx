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

/// English grapheme → viseme id (OVR-style subset). For long dialogue, call once per line.
void BuildVisemeKeyframesFromEnglishText(const std::string& text, uint32_t ticksPerSecond, uint64_t startTick,
    uint64_t endTick, uint32_t maxKeyframes, std::vector<VisemeKeyframeTick>& out, std::string& warnOrEmpty);

/// Simple list of (visemeId, strength) covering the utterance — used by `PhonemeDetector`.
std::vector<std::pair<std::string, float>> TextToVisemeStrengthSamples(const std::string& text);

} // namespace Solstice::Arzachel
