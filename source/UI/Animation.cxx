#include <UI/Animation.hxx>
#include <UI/VisualEffects.hxx>
#include <imgui.h>

namespace Solstice::UI {

// ShadowParams Lerp implementation (UI-specific; declared in Animation.hxx)
ShadowParams Animation::AnimationTrack<ShadowParams>::Lerp(const ShadowParams& A, const ShadowParams& B, float T) const {
    ShadowParams result;
    result.Offset = ImVec2(A.Offset.x + (B.Offset.x - A.Offset.x) * T, A.Offset.y + (B.Offset.y - A.Offset.y) * T);
    result.BlurRadius = A.BlurRadius + (B.BlurRadius - A.BlurRadius) * T;
    result.Spread = A.Spread + (B.Spread - A.Spread) * T;

    uint8_t rA = static_cast<uint8_t>((A.Color >> IM_COL32_R_SHIFT) & 0xFF);
    uint8_t gA = static_cast<uint8_t>((A.Color >> IM_COL32_G_SHIFT) & 0xFF);
    uint8_t bA = static_cast<uint8_t>((A.Color >> IM_COL32_B_SHIFT) & 0xFF);
    uint8_t aA = static_cast<uint8_t>((A.Color >> IM_COL32_A_SHIFT) & 0xFF);
    uint8_t rB = static_cast<uint8_t>((B.Color >> IM_COL32_R_SHIFT) & 0xFF);
    uint8_t gB = static_cast<uint8_t>((B.Color >> IM_COL32_G_SHIFT) & 0xFF);
    uint8_t bB = static_cast<uint8_t>((B.Color >> IM_COL32_B_SHIFT) & 0xFF);
    uint8_t aB = static_cast<uint8_t>((B.Color >> IM_COL32_A_SHIFT) & 0xFF);

    result.Color = IM_COL32(
        static_cast<uint8_t>(rA + (rB - rA) * T),
        static_cast<uint8_t>(gA + (gB - gA) * T),
        static_cast<uint8_t>(bA + (bB - bA) * T),
        static_cast<uint8_t>(aA + (aB - aA) * T)
    );

    result.Type = (T < 0.5f) ? A.Type : B.Type;
    result.Inset = (T < 0.5f) ? A.Inset : B.Inset;
    return result;
}

} // namespace Solstice::UI
