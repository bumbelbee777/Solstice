#include <UI/Motion/MotionGraphicsCompositor.hxx>

#include <Math/Vector.hxx>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>

namespace Solstice::UI::MotionGraphics {

namespace {

ImU32 ScaleColorAlpha(ImU32 c, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    const uint8_t r = static_cast<uint8_t>((c >> IM_COL32_R_SHIFT) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((c >> IM_COL32_G_SHIFT) & 0xFF);
    const uint8_t b = static_cast<uint8_t>((c >> IM_COL32_B_SHIFT) & 0xFF);
    const uint8_t a = static_cast<uint8_t>((c >> IM_COL32_A_SHIFT) & 0xFF);
    const uint8_t na = static_cast<uint8_t>(static_cast<float>(a) * factor);
    return IM_COL32(r, g, b, na);
}

ImU32 ApplyBlendApprox(ImU32 c, Parallax::BlendMode mode) {
    const uint8_t r = static_cast<uint8_t>((c >> IM_COL32_R_SHIFT) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((c >> IM_COL32_G_SHIFT) & 0xFF);
    const uint8_t b = static_cast<uint8_t>((c >> IM_COL32_B_SHIFT) & 0xFF);
    const uint8_t a = static_cast<uint8_t>((c >> IM_COL32_A_SHIFT) & 0xFF);
    switch (mode) {
        case Parallax::BlendMode::Over:
            return c;
        case Parallax::BlendMode::Additive:
            return IM_COL32(std::min(255, r + 55), std::min(255, g + 55), std::min(255, b + 40), a);
        case Parallax::BlendMode::Multiply:
        default:
            return IM_COL32(static_cast<uint8_t>(r * 200 / 255), static_cast<uint8_t>(g * 200 / 255),
                            static_cast<uint8_t>(b * 200 / 255), a);
    }
}

} // namespace

void Compositor::Clear() {
    m_List = Parallax::MGDisplayList{};
}

void Compositor::Submit(const Parallax::MGDisplayList& list) {
    m_List = list;
}

void Compositor::drawEntry(ImDrawList* dl, const Parallax::MGDisplayList::Entry& e, ImVec2 origin, float listGlobalAlpha) const {
    if (!dl) {
        return;
    }
    const float entryFactor = std::clamp(e.Alpha, 0.0f, 1.0f) * std::clamp(listGlobalAlpha, 0.0f, 1.0f);

    if (e.SchemaType == "MGTextElement") {
        std::string text = "Text";
        ImVec2 pos = origin;
        ImU32 col = IM_COL32(255, 255, 255, 255);
        auto itT = e.Attributes.find("Text");
        if (itT != e.Attributes.end()) {
            if (const auto* s = std::get_if<std::string>(&itT->second)) {
                text = *s;
            }
        }
        auto itP = e.Attributes.find("Position");
        if (itP != e.Attributes.end()) {
            if (const auto* v = std::get_if<Math::Vec2>(&itP->second)) {
                pos = ImVec2(origin.x + v->x, origin.y + v->y);
            }
        }
        auto itC = e.Attributes.find("Color");
        if (itC != e.Attributes.end()) {
            if (const auto* v = std::get_if<Math::Vec4>(&itC->second)) {
                col = IM_COL32(static_cast<int>(v->x * 255.f), static_cast<int>(v->y * 255.f),
                               static_cast<int>(v->z * 255.f), static_cast<int>(v->w * 255.f));
            }
        }
        col = ApplyBlendApprox(col, e.Blend);
        col = ScaleColorAlpha(col, entryFactor);
        dl->AddText(pos, col, text.c_str());
    } else if (e.SchemaType == "MGSpriteElement") {
        ImVec2 pos = origin;
        ImVec2 size(64.0f, 64.0f);
        uint64_t assetHash = 0;
        auto itP = e.Attributes.find("Position");
        if (itP != e.Attributes.end()) {
            if (const auto* v = std::get_if<Math::Vec2>(&itP->second)) {
                pos = ImVec2(origin.x + v->x, origin.y + v->y);
            }
        }
        auto itS = e.Attributes.find("Size");
        if (itS != e.Attributes.end()) {
            if (const auto* v = std::get_if<Math::Vec2>(&itS->second)) {
                size = ImVec2(v->x, v->y);
            }
        }
        auto itTex = e.Attributes.find("Texture");
        if (itTex != e.Attributes.end()) {
            if (const auto* h = std::get_if<uint64_t>(&itTex->second)) {
                assetHash = *h;
            }
        }
        const ImVec2 min = pos;
        const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);
        ImU32 col = IM_COL32(255, 255, 255, 255);
        col = ApplyBlendApprox(col, e.Blend);
        col = ScaleColorAlpha(col, entryFactor);

        ImTextureID texId = static_cast<ImTextureID>(0);
        if (m_TextureResolver) {
            texId = m_TextureResolver(assetHash);
        }
        if (texId != static_cast<ImTextureID>(0)) {
            dl->AddImage(texId, min, max, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), col);
        } else {
            dl->AddRectFilled(min, max, ScaleColorAlpha(IM_COL32(200, 80, 220, 200), entryFactor));
            dl->AddRect(min, max, IM_COL32(255, 255, 255, 90), 0.0f, 0, 1.0f);
        }
    }

    ImVec2 childOrigin = origin;
    auto itParentPos = e.Attributes.find("Position");
    if (itParentPos != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec2>(&itParentPos->second)) {
            childOrigin = ImVec2(origin.x + v->x, origin.y + v->y);
        }
    }
    for (const auto& c : e.Children) {
        drawEntry(dl, c, childOrigin, listGlobalAlpha);
    }
}

void Compositor::Render(ImDrawList* drawList) {
    if (!drawList) {
        return;
    }
    const float ga = std::clamp(m_List.GlobalAlpha, 0.0f, 1.0f);
    ImVec2 origin(16.f, 16.f);
    std::vector<size_t> order(m_List.Entries.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [this](size_t ai, size_t bi) {
        return Parallax::MGEntryCompositeDepth(m_List.Entries[ai]) < Parallax::MGEntryCompositeDepth(m_List.Entries[bi]);
    });
    for (size_t idx : order) {
        drawEntry(drawList, m_List.Entries[idx], origin, ga);
    }
}

void SubmitMGDisplayList(const Parallax::MGDisplayList& list, Compositor& compositor) {
    compositor.Submit(list);
}

} // namespace Solstice::UI::MotionGraphics
