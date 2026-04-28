#include "WohStarryBackground.hxx"

#include <imgui.h>

#include <cmath>
#include <cstdint>

namespace Solstice::WarsOfHeaven {
namespace {

constexpr int kNumStars = 720;

// xorshift32
inline std::uint32_t StarHash(std::uint32_t i) {
    std::uint32_t x = i * 747796405u + 2891336453u;
    x ^= x >> 16;
    x *= 2246822519u;
    x ^= x >> 13;
    x *= 3266489917u;
    x ^= x >> 16;
    return x ? x : 1u;
}

} // namespace

void DrawWohStarryMenuBackground() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    if (!vp) {
        return;
    }
    const ImVec2 a = vp->Pos;
    const ImVec2 b = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) {
        return;
    }

    const double t = static_cast<double>(ImGui::GetTime());

    // Deep-space wash (gives the glass something to “sit” on)
    const ImU32 colTop = IM_COL32(6, 10, 36, 255);
    const ImU32 colBot = IM_COL32(0, 1, 8, 255);
    dl->AddRectFilledMultiColor(a, b, colTop, colTop, colBot, colBot);

    // Subtle radial fog — reads as haze for “breathtaking” depth without a blur pass
    const ImVec2 center = ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
    const float R = (b.x - a.x) * 0.85f;
    for (int layer = 0; layer < 3; ++layer) {
        const float fade = 0.06f - static_cast<float>(layer) * 0.012f;
        const ImU32 c = ImGui::ColorConvertFloat4ToU32(ImVec4(0.2f, 0.3f, 0.6f, fade));
        dl->AddCircleFilled(center, R * (0.55f + 0.12f * static_cast<float>(layer)), c, 64);
    }

    // Palettes: cool / warm / jewel / white
    const ImVec4 kPal[] = {ImVec4(0.70f, 0.86f, 1.0f, 1.0f), ImVec4(1.0f, 0.82f, 0.63f, 1.0f), ImVec4(0.75f, 1.0f, 0.86f, 1.0f), ImVec4(0.90f, 0.70f, 1.0f, 1.0f),
        ImVec4(0.95f, 0.95f, 1.0f, 1.0f)};

    for (int i = 0; i < kNumStars; ++i) {
        const std::uint32_t h0 = StarHash(static_cast<std::uint32_t>(i + 1));
        const std::uint32_t h1 = StarHash(h0);
        const std::uint32_t h2 = StarHash(h0 ^ 0xA5A5A5A5u);
        const float u = (h0 & 0xffff) / 65535.0f;
        const float v = (h1 & 0xffff) / 65535.0f;
        const ImVec2 p = ImVec2(a.x + u * (b.x - a.x), a.y + v * (b.y - a.y));
        const int pi = static_cast<int>(h2 % (sizeof(kPal) / sizeof(kPal[0])));
        const ImVec4 col = kPal[pi];

        const double phase = static_cast<double>(h0 & 0xff) * 0.17 + static_cast<double>(h1 & 0xff) * 0.01;
        const double tw = 0.5 + 0.5 * std::sin(t * 1.1 + phase);
        const double tw2 = 0.5 + 0.5 * std::sin(t * 2.3 + phase * 0.5 + static_cast<double>(i) * 0.02);
        const float aPulse = static_cast<float>(0.12 + 0.88 * (tw * 0.55 + tw2 * 0.45));
        const float aGlow = aPulse * 0.4f;
        const float aCore = aPulse * 0.85f;

        const float s = 1.0f + static_cast<float>(h1 & 0x3);
        ImVec4 g = col;
        g.w = aGlow;
        ImVec4 c = ImVec4(1, 1, 1, aCore);
        dl->AddCircleFilled(p, s * 2.1f, ImGui::ColorConvertFloat4ToU32(g), 8);
        dl->AddCircleFilled(p, s * 0.6f, ImGui::ColorConvertFloat4ToU32(c), 6);
    }
}

} // namespace Solstice::WarsOfHeaven
