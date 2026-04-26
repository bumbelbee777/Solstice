#include "SmmLipsyncMorpheme.hxx"
#include "SmmSessionAuthoring.hxx"

using Smm::Authoring::LipsyncLineStub;
using Smm::Authoring::LipsyncSample;

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string_view>

namespace Smm::LipsyncMorpheme {
namespace {

static bool IsVowelEn(unsigned char c) {
    c = static_cast<unsigned char>(std::toupper(c));
    return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U' || c == 'Y';
}

static size_t VowelIndexInToken(std::string_view w) {
    for (size_t i = 0; i < w.size(); ++i) {
        if (IsVowelEn(static_cast<unsigned char>(w[i]))) {
            return i;
        }
    }
    return w.size() / 2;
}

static void BuildLegacyStepSamples(
    const Smm::Authoring::LipsyncLineStub& line, const std::string& textIn, std::vector<Smm::Authoring::LipsyncSample>& out,
    uint32_t maxK) {
    const std::vector<std::string> toks = Smm::Authoring::ExtractMorphemeLikeTokens(textIn);
    const uint64_t span = (line.EndTick > line.StartTick) ? (line.EndTick - line.StartTick) : 0;
    if (line.StartTick < line.EndTick && out.size() < maxK) {
        LipsyncSample a;
        a.Tick = line.StartTick;
        a.MouthOpen = 0.15f;
        out.push_back(a);
    }
    if (span == 0) {
        if (out.empty() && maxK > 0) {
            LipsyncSample a;
            a.Tick = line.StartTick;
            a.MouthOpen = 0.14f;
            out.push_back(a);
        }
        return;
    }
    const size_t n = (std::max)(size_t{1}, toks.size());
    const uint64_t step = span / static_cast<uint64_t>(n);
    for (size_t i = 0; i < n && out.size() < maxK; ++i) {
        LipsyncSample s;
        s.Tick = line.StartTick + static_cast<uint64_t>(i) * step;
        const std::string_view w = toks.empty() ? std::string_view("mm") : std::string_view(toks[(std::min)(i, toks.size() - 1)]);
        s.MouthOpen = Smm::Authoring::MouthOpenHeuristic2002(w);
        if (i == n - 1) {
            s.Tick = line.EndTick;
        }
        out.push_back(s);
    }
}

} // namespace

MorphemeStats ComputeMorphemeStats(const std::string& text) {
    MorphemeStats s{};
    const std::vector<std::string> toks = Smm::Authoring::ExtractMorphemeLikeTokens(text);
    s.WordCount = toks.size();
    std::set<std::string> uni(toks.begin(), toks.end());
    s.UniqueTokens = uni.size();
    for (char c : text) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            ++s.CharCount;
        }
    }
    int vcount = 0;
    for (const auto& t : toks) {
        for (unsigned char c : t) {
            if (IsVowelEn(c)) {
                ++vcount;
            }
        }
    }
    const int denom = (std::max)(static_cast<int>(s.CharCount), 1);
    s.VowelDensity = (std::clamp)(static_cast<float>(vcount) / static_cast<float>(denom), 0.f, 1.f);
    return s;
}

void BuildLipsyncSamples(
    const LipsyncLineStub& line, uint32_t ticksPerSecond, std::vector<LipsyncSample>& out, std::string& warnOrEmpty) {
    (void)ticksPerSecond;
    warnOrEmpty.clear();
    out.clear();
    if (line.PhoneticMode == 2) {
        // Actor viseme channels — use `TryApplyLipsyncVisemeToActor` / Arzachel TextLipSync, not MG Depth samples.
        return;
    }
    const size_t kMaxText = 65535u;
    if (line.Text.size() > kMaxText) {
        warnOrEmpty = "Lipsync: text over cap; trimmed for preview.";
    }
    const std::string textIn = (line.Text.size() > kMaxText) ? line.Text.substr(0, kMaxText) : line.Text;
    const uint32_t maxK = (std::max)(1u, line.MaxKeyframes);
    if (line.PhoneticMode == 0) {
        BuildLegacyStepSamples(line, textIn, out, maxK);
    } else {
        const std::vector<std::string> toks = Smm::Authoring::ExtractMorphemeLikeTokens(textIn);
        if (line.StartTick >= line.EndTick) {
            if (out.size() < maxK) {
                LipsyncSample a;
                a.Tick = line.StartTick;
                a.MouthOpen = 0.12f;
                out.push_back(a);
            }
        } else {
            const uint64_t span = line.EndTick - line.StartTick;
            const size_t n = (std::max)(size_t{1}, toks.size());
            if (n + 2 > static_cast<size_t>(maxK)) {
                warnOrEmpty = "Lipsync: many words vs max keyframes; some edges dropped.";
            }
            if (out.size() < maxK) {
                LipsyncSample a0;
                a0.Tick = line.StartTick;
                a0.MouthOpen = 0.14f;
                out.push_back(a0);
            }
            for (size_t i = 0; i < n && out.size() < maxK; ++i) {
                const std::string_view w = toks.empty() ? std::string_view("m") : std::string_view(toks[(std::min)(i, toks.size() - 1)]);
                const size_t vpos = VowelIndexInToken(w);
                const double frac = (w.size() > 0) ? (static_cast<double>(vpos) + 0.5) / static_cast<double>(w.size()) : 0.4;
                const uint64_t tLocal = static_cast<uint64_t>(static_cast<double>(span) * (static_cast<double>(i) + frac) / static_cast<double>(n + 1));
                uint64_t tick = line.StartTick + tLocal;
                if (tick > line.EndTick) {
                    tick = line.EndTick;
                }
                LipsyncSample s;
                s.Tick = tick;
                s.MouthOpen = Smm::Authoring::MouthOpenHeuristic2002(w);
                out.push_back(s);
            }
            if (out.size() < maxK && (out.empty() || out.back().Tick != line.EndTick)) {
                LipsyncSample endS;
                endS.Tick = line.EndTick;
                endS.MouthOpen = 0.12f;
                out.push_back(endS);
            }
        }
    }
    for (LipsyncSample& s : out) {
        s.MouthOpen = (std::clamp)(s.MouthOpen * line.Strength, 0.02f, 1.2f);
    }
    if (out.size() >= maxK) {
        if (warnOrEmpty.empty()) {
            warnOrEmpty = "Lipsync: hit max keyframe cap; curve truncated.";
        }
    }
}

} // namespace Smm::LipsyncMorpheme
