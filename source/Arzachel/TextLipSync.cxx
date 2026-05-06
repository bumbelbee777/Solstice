#include "TextLipSync.hxx"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace Solstice::Arzachel {
namespace {

char LowerAscii(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

const std::unordered_map<char, const char*>& CharToViseme() {
    static const std::unordered_map<char, const char*> k = {
        {'a', "aa"}, {'e', "E"}, {'i', "I"}, {'o', "O"}, {'u', "U"}, {'y', "I"},
        {'b', "PP"}, {'m', "PP"}, {'p', "PP"},
        {'f', "FF"}, {'v', "FF"},
        {'t', "DD"}, {'d', "DD"}, {'n', "nn"}, {'l', "DD"},
        {'k', "kk"}, {'g', "kk"}, {'c', "kk"}, {'q', "kk"},
        {'s', "SS"}, {'z', "SS"},
        {'h', "TH"}, {'r', "RR"}, {'w', "U"}, {'j', "CH"}, {'x', "kk"},
    };
    return k;
}

const char* MapCharToViseme(char c) {
    const char lc = LowerAscii(c);
    if (lc == '\0' || std::isspace(static_cast<unsigned char>(lc))) {
        return "sil";
    }
    const auto& tab = CharToViseme();
    auto it = tab.find(lc);
    if (it != tab.end()) {
        return it->second;
    }
    return "DD";
}

bool IsVowel(char c) {
    const char lc = LowerAscii(c);
    return lc == 'a' || lc == 'e' || lc == 'i' || lc == 'o' || lc == 'u' || lc == 'y';
}

const char* MapDigraphToViseme(char a, char b) {
    const char x = LowerAscii(a);
    const char y = LowerAscii(b);
    if (x == 't' && y == 'h') return "TH";
    if (x == 'c' && y == 'h') return "CH";
    if (x == 's' && y == 'h') return "SS";
    if (x == 'p' && y == 'h') return "FF";
    if (x == 'n' && y == 'g') return "nn";
    if (x == 'q' && y == 'u') return "kk";
    if (x == 'o' && y == 'o') return "U";
    if (x == 'e' && y == 'e') return "I";
    return nullptr;
}

} // namespace

std::vector<std::pair<std::string, float>> TextToVisemeStrengthSamples(const std::string& text) {
    std::vector<std::pair<std::string, float>> out;
    out.reserve(std::min<size_t>(text.size(), 4096));
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            continue;
        }
        out.emplace_back(MapCharToViseme(c), 1.f);
    }
    if (out.empty()) {
        out.emplace_back("sil", 0.2f);
    }
    return out;
}

void BuildVisemeKeyframesFromEnglishText(const std::string& text, uint32_t ticksPerSecond, uint64_t startTick,
    uint64_t endTick, uint32_t maxKeyframes, std::vector<VisemeKeyframeTick>& out, std::string& warnOrEmpty) {
    BuildVisemeKeyframesFromEnglishTextEx(
        text,
        ticksPerSecond,
        startTick,
        endTick,
        maxKeyframes,
        LipSyncTextConfig{},
        out,
        warnOrEmpty
    );
}

void BuildVisemeKeyframesFromEnglishTextEx(const std::string& text, uint32_t ticksPerSecond, uint64_t startTick,
    uint64_t endTick, uint32_t maxKeyframes, const LipSyncTextConfig& config, std::vector<VisemeKeyframeTick>& out,
    std::string& warnOrEmpty) {
    warnOrEmpty.clear();
    out.clear();
    if (maxKeyframes == 0 || ticksPerSecond == 0) {
        warnOrEmpty = "Viseme: invalid ticksPerSecond or maxKeyframes.";
        return;
    }
    const uint64_t span = (endTick > startTick) ? (endTick - startTick) : 0;
    if (span == 0) {
        VisemeKeyframeTick k{};
        k.TimeTicks = startTick;
        k.VisemeId = "sil";
        k.Strength = 0.25f;
        out.push_back(k);
        return;
    }

    std::vector<std::pair<std::string, float>> phonemes;
    phonemes.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (config.EnablePunctuationPauses) {
                phonemes.emplace_back("sil", config.PauseStrength);
            }
            continue;
        }
        if (config.EnablePunctuationPauses && (c == ',' || c == ';' || c == ':' || c == '.' || c == '!' || c == '?')) {
            float pause = config.PauseStrength * ((c == '!' || c == '?') ? 1.35f : 1.0f);
            if (config.EnableQuestionRise && c == '?') {
                pause *= 0.85f;
            }
            phonemes.emplace_back("sil", pause);
            continue;
        }
        const char next = (i + 1 < text.size()) ? text[i + 1] : '\0';
        if (config.EnableDigraphs) {
            if (const char* dg = MapDigraphToViseme(c, next)) {
                float s = config.BaseStrength * (IsVowel(c) ? config.VowelBias : config.ConsonantBias);
                phonemes.emplace_back(dg, s);
                ++i;
                continue;
            }
        }
        float s = config.BaseStrength * (IsVowel(c) ? config.VowelBias : config.ConsonantBias);
        phonemes.emplace_back(MapCharToViseme(c), s);
    }
    const size_t n = std::max<size_t>(1, phonemes.size());
    const uint64_t step = span / static_cast<uint64_t>(std::min<uint64_t>(static_cast<uint64_t>(n), static_cast<uint64_t>(maxKeyframes)));

    uint32_t produced = 0;
    for (size_t i = 0; i < n && produced < maxKeyframes; ++i) {
        VisemeKeyframeTick k{};
        k.TimeTicks = startTick + static_cast<uint64_t>(i) * step;
        if (!phonemes.empty()) {
            k.VisemeId = phonemes[i].first;
            k.Strength = phonemes[i].second;
        } else {
            k.VisemeId = "sil";
            k.Strength = config.PauseStrength;
        }

        if (config.Coarticulation > 0.0f && !phonemes.empty()) {
            const float prev = (i > 0) ? phonemes[i - 1].second : k.Strength;
            const float nextS = (i + 1 < phonemes.size()) ? phonemes[i + 1].second : k.Strength;
            const float neighbor = 0.5f * (prev + nextS);
            k.Strength = std::lerp(k.Strength, neighbor, std::clamp(config.Coarticulation, 0.0f, 1.0f));
        }
        k.Strength = std::clamp(k.Strength, 0.0f, 1.5f);
        out.push_back(k);
        ++produced;
    }
    if (!out.empty() && produced < maxKeyframes && endTick > out.back().TimeTicks) {
        VisemeKeyframeTick k{};
        k.TimeTicks = endTick;
        k.VisemeId = out.back().VisemeId;
        k.Strength = 0.35f;
        out.push_back(k);
    }
    if (phonemes.size() > maxKeyframes) {
        warnOrEmpty = "Viseme: many letters vs max keyframes; timeline truncated.";
    }
}

void SampleVisemeAtTick(const std::vector<VisemeKeyframeTick>& keyframes, uint64_t tick,
    std::string& outCurrentViseme, std::string& outNextViseme, float& outBlend, float& outStrength) {
    outCurrentViseme = "sil";
    outNextViseme = "sil";
    outBlend = 0.0f;
    outStrength = 0.0f;
    if (keyframes.empty()) {
        return;
    }
    if (tick <= keyframes.front().TimeTicks) {
        outCurrentViseme = keyframes.front().VisemeId;
        outStrength = keyframes.front().Strength;
        return;
    }
    for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
        const auto& a = keyframes[i];
        const auto& b = keyframes[i + 1];
        if (tick >= a.TimeTicks && tick <= b.TimeTicks) {
            outCurrentViseme = a.VisemeId;
            outNextViseme = b.VisemeId;
            const uint64_t span = std::max<uint64_t>(1, b.TimeTicks - a.TimeTicks);
            outBlend = std::clamp(static_cast<float>(tick - a.TimeTicks) / static_cast<float>(span), 0.0f, 1.0f);
            outStrength = std::lerp(a.Strength, b.Strength, outBlend);
            return;
        }
    }
    outCurrentViseme = keyframes.back().VisemeId;
    outStrength = keyframes.back().Strength;
}

void CompactVisemeKeyframes(std::vector<VisemeKeyframeTick>& ioKeyframes, float minStrengthDelta) {
    if (ioKeyframes.size() < 3) {
        return;
    }
    std::vector<VisemeKeyframeTick> compact;
    compact.reserve(ioKeyframes.size());
    compact.push_back(ioKeyframes.front());
    for (size_t i = 1; i + 1 < ioKeyframes.size(); ++i) {
        const auto& prev = compact.back();
        const auto& cur = ioKeyframes[i];
        const auto& next = ioKeyframes[i + 1];
        const float dsPrev = std::abs(cur.Strength - prev.Strength);
        const float dsNext = std::abs(cur.Strength - next.Strength);
        const bool sameVisemeTriplet = (prev.VisemeId == cur.VisemeId) && (cur.VisemeId == next.VisemeId);
        if (sameVisemeTriplet && dsPrev <= minStrengthDelta && dsNext <= minStrengthDelta) {
            continue;
        }
        compact.push_back(cur);
    }
    compact.push_back(ioKeyframes.back());
    ioKeyframes.swap(compact);
}

} // namespace Solstice::Arzachel
