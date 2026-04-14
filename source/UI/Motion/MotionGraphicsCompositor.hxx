#pragma once

#include "../../Solstice.hxx"
#include <Parallax/ParallaxTypes.hxx>
#include <cstdint>
#include <functional>
#include <imgui.h>

namespace Solstice::UI::MotionGraphics {

// Consumes PARALLAX MGDisplayList and draws via ImGui (MGTextElement, MGSpriteElement, hierarchy).
class SOLSTICE_API Compositor {
public:
    void Clear();
    void Submit(const Parallax::MGDisplayList& list);

    void SetViewportSize(float width, float height) {
        m_Width = width;
        m_Height = height;
    }

    /** Optional: map Relic/Parallax asset hash to an ImGui texture id (e.g. GL texture cast). If unset, sprites use a placeholder. */
    void SetTextureResolver(std::function<ImTextureID(uint64_t assetHash)> resolver) {
        m_TextureResolver = std::move(resolver);
    }
    void ClearTextureResolver() { m_TextureResolver = nullptr; }

    void Render(ImDrawList* drawList);

private:
    void drawEntry(ImDrawList* dl, const Parallax::MGDisplayList::Entry& e, ImVec2 origin, float listGlobalAlpha) const;

    float m_Width{1280.f};
    float m_Height{720.f};
    Parallax::MGDisplayList m_List;
    std::function<ImTextureID(uint64_t)> m_TextureResolver;
};

SOLSTICE_API void SubmitMGDisplayList(const Parallax::MGDisplayList& list, Compositor& compositor);

} // namespace Solstice::UI::MotionGraphics
