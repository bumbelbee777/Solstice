#include <UI/Motion/MotionGraphicsCompositor.hxx>

#include <Math/Vector.hxx>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
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

ImU32 ApplyBlendApproxU32(ImU32 c, Parallax::BlendMode mode) {
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

static float AttrF(const Parallax::MGDisplayList::Entry& e, const char* k, float d) {
    auto it = e.Attributes.find(k);
    if (it == e.Attributes.end()) {
        return d;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return d;
}

static int AttrI(const Parallax::MGDisplayList::Entry& e, const char* k, int d) {
    auto it = e.Attributes.find(k);
    if (it == e.Attributes.end()) {
        return d;
    }
    if (const auto* in = std::get_if<int32_t>(&it->second)) {
        return static_cast<int>(*in);
    }
    return d;
}

static ImU32 ModulateRgbaImGui(ImU32 c, float aoMult, float aoEdge) {
    const float aom = (std::clamp)(aoMult, 0.0f, 4.0f);
    const float aoe = (std::clamp)(aoEdge, 0.0f, 1.0f);
    const float edgeDark = 1.f - 0.28f * aoe; // ImGui has no per-pixel edge AO; nudge whole quad.
    const float m = aom * edgeDark;
    if (m <= 0.0001f) {
        return IM_COL32(0, 0, 0, 0);
    }
    const uint8_t r = static_cast<uint8_t>((c >> IM_COL32_R_SHIFT) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((c >> IM_COL32_G_SHIFT) & 0xFF);
    const uint8_t b = static_cast<uint8_t>((c >> IM_COL32_B_SHIFT) & 0xFF);
    const uint8_t a = static_cast<uint8_t>((c >> IM_COL32_A_SHIFT) & 0xFF);
    return IM_COL32(
        (std::min)(255, static_cast<int>(r * m + 0.5f)), (std::min)(255, static_cast<int>(g * m + 0.5f)),
        (std::min)(255, static_cast<int>(b * m + 0.5f)), a);
}

static void UvRgbaAtCorners(float uSc, float vSc, float wu, float wv, float wph, ImVec2& uva, ImVec2& uvb, ImVec2& uvc, ImVec2& uvd) {
    const auto corner = [&](float u, float v) {
        float su = u + uSc;
        float sv = v + vSc;
        su = su - std::floor(su);
        sv = sv - std::floor(sv);
        if (std::abs(wu) + std::abs(wv) > 1e-6f) {
            su = (std::clamp)(su + wu * std::sin(6.2831853f * sv + wph), 0.0f, 1.0f);
            sv = (std::clamp)(sv + wv * std::sin(6.2831853f * u + wph * 0.5f), 0.0f, 1.0f);
        }
        return ImVec2(su, sv);
    };
    uva = corner(0.f, 0.f);
    uvb = corner(1.f, 0.f);
    uvc = corner(1.f, 1.f);
    uvd = corner(0.f, 1.f);
}

static void RotatedSpriteCorners(float posX, float posY, float sizeX, float sizeY, float rotRad, float offX, float offY, float co,
    float si, ImVec2& p1, ImVec2& p2, ImVec2& p3, ImVec2& p4) {
    const float hw = sizeX * 0.5f;
    const float hh = sizeY * 0.5f;
    const float cx = posX + hw + offX;
    const float cy = posY + hh + offY;
    const float lxs[4] = {-hw, hw, hw, -hw};
    const float lys[4] = {-hh, -hh, hh, hh};
    ImVec2* pts[4] = {&p1, &p2, &p3, &p4};
    for (int i = 0; i < 4; ++i) {
        const float lx = lxs[i];
        const float ly = lys[i];
        pts[i]->x = cx + lx * co - ly * si;
        pts[i]->y = cy + lx * si + ly * co;
    }
}

static void DrawMGSpriteImGui(ImDrawList* dl, const Parallax::MGDisplayList::Entry& e, ImVec2 pos, ImVec2 size, ImU32 colBase,
    const std::function<ImTextureID(uint64_t)>& texResolve) {
    uint64_t assetHash = 0;
    auto itTex = e.Attributes.find("Texture");
    if (itTex != e.Attributes.end()) {
        if (const auto* h = std::get_if<uint64_t>(&itTex->second)) {
            assetHash = *h;
        }
    }
    float rotZ = AttrF(e, "RotationZ", 0.f);
    const int bbm = AttrI(e, "BillboardMode", 0);
    if (bbm != 0) {
        const float ph = AttrF(e, "UvPhase", 0.f);
        rotZ += 0.12f * static_cast<float>(bbm) * std::sin(ph * 0.05f);
    }
    const int smearN = (std::clamp)(AttrI(e, "SmearCount", 0), 0, 8);
    const float smx = AttrF(e, "SmearDx", 0.f);
    const float smy = AttrF(e, "SmearDy", 0.f);
    const float sfall = (std::clamp)(AttrF(e, "SmearFalloff", 0.65f), 0.05f, 0.999f);
    const float uSc = AttrF(e, "UvScrollU", 0.f);
    const float vSc = AttrF(e, "UvScrollV", 0.f);
    const float wu = AttrF(e, "UvWaveU", 0.f);
    const float wv = AttrF(e, "UvWaveV", 0.f);
    const float wph = AttrF(e, "UvPhase", 0.f);
    const float aoM = (std::max)(0.f, AttrF(e, "AOMultiply", 1.f));
    const float aoE = (std::clamp)(AttrF(e, "AOEdge", 0.f), 0.f, 1.f);
    if (aoM <= 1e-4f) {
        return;
    }

    const float co = std::cos(rotZ);
    const float si = std::sin(rotZ);
    ImVec2 uva, uvb, uvc, uvd;
    UvRgbaAtCorners(uSc, vSc, wu, wv, wph, uva, uvb, uvc, uvd);
    const ImU32 colPaint = ModulateRgbaImGui(colBase, aoM, aoE);

    ImTextureID tid = static_cast<ImTextureID>(0);
    if (texResolve) {
        tid = texResolve(assetHash);
    }
    if (tid != static_cast<ImTextureID>(0)) {
        const ImTextureRef tref{tid};
        for (int s = smearN; s >= 1; --s) {
            const float g = static_cast<float>(std::pow(sfall, static_cast<double>(s)));
            const ImU32 csm = ScaleColorAlpha(colPaint, g);
            const float oxf = smx * static_cast<float>(s);
            const float oyf = smy * static_cast<float>(s);
            ImVec2 q1, q2, q3, q4;
            RotatedSpriteCorners(pos.x, pos.y, size.x, size.y, rotZ, oxf, oyf, co, si, q1, q2, q3, q4);
            dl->AddImageQuad(tref, q1, q2, q3, q4, uva, uvb, uvc, uvd, csm);
        }
        ImVec2 p1, p2, p3, p4;
        RotatedSpriteCorners(pos.x, pos.y, size.x, size.y, rotZ, 0.f, 0.f, co, si, p1, p2, p3, p4);
        dl->AddImageQuad(tref, p1, p2, p3, p4, uva, uvb, uvc, uvd, colPaint);
    } else {
        for (int s = smearN; s >= 1; --s) {
            const float g = static_cast<float>(std::pow(sfall, static_cast<double>(s)));
            const ImU32 csm = ScaleColorAlpha(colPaint, g);
            ImVec2 q1, q2, q3, q4;
            RotatedSpriteCorners(pos.x, pos.y, size.x, size.y, rotZ, smx * static_cast<float>(s), smy * static_cast<float>(s), co, si, q1, q2, q3, q4);
            const ImVec2 arr[4] = {q1, q2, q3, q4};
            dl->AddConvexPolyFilled(arr, 4, csm);
        }
        ImVec2 f1, f2, f3, f4;
        RotatedSpriteCorners(pos.x, pos.y, size.x, size.y, rotZ, 0.f, 0.f, co, si, f1, f2, f3, f4);
        const ImVec2 farr[4] = {f1, f2, f3, f4};
        dl->AddConvexPolyFilled(farr, 4, colPaint);
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
        col = ApplyBlendApproxU32(col, e.Blend);
        col = ScaleColorAlpha(col, entryFactor);
        dl->AddText(pos, col, text.c_str());
    } else if (e.SchemaType == "MGSpriteElement") {
        ImVec2 pos = origin;
        ImVec2 size(64.0f, 64.0f);
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
        ImU32 col = IM_COL32(255, 255, 255, 255);
        {
            const auto itC = e.Attributes.find("Color");
            if (itC != e.Attributes.end()) {
                if (const auto* v = std::get_if<Math::Vec4>(&itC->second)) {
                    col = IM_COL32(static_cast<int>(v->x * 255.f), static_cast<int>(v->y * 255.f), static_cast<int>(v->z * 255.f),
                        static_cast<int>(v->w * 255.f));
                }
            }
        }
        col = ApplyBlendApproxU32(col, e.Blend);
        col = ScaleColorAlpha(col, entryFactor);
        DrawMGSpriteImGui(dl, e, pos, size, col, m_TextureResolver);
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
    ImVec2 origin(16.f + m_List.Post.ScreenShakeX, 16.f + m_List.Post.ScreenShakeY);
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
