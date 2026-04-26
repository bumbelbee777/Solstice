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

    std::vector<char> letters;
    letters.reserve(text.size());
    for (char c : text) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            letters.push_back(c);
        }
    }
    const size_t n = std::max<size_t>(1, letters.size());
    const uint64_t step = span / static_cast<uint64_t>(std::min<uint64_t>(static_cast<uint64_t>(n), static_cast<uint64_t>(maxKeyframes)));

    uint32_t produced = 0;
    for (size_t i = 0; i < n && produced < maxKeyframes; ++i) {
        VisemeKeyframeTick k{};
        k.TimeTicks = startTick + static_cast<uint64_t>(i) * step;
        k.VisemeId = MapCharToViseme(letters[i]);
        k.Strength = 1.f;
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
    if (letters.size() > maxKeyframes) {
        warnOrEmpty = "Viseme: many letters vs max keyframes; timeline truncated.";
    }
}

} // namespace Solstice::Arzachel
