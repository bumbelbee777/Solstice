#pragma once

#include "ParallaxScene.hxx"
#include "ParallaxTypes.hxx"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace Solstice::Parallax {

struct ParallaxSceneSummary {
    std::size_t ElementCount{0};
    std::size_t ChannelCount{0};
    std::size_t MGElementCount{0};
    std::size_t MGTrackCount{0};
    std::uint32_t TicksPerSecond{0};
    std::uint64_t TimelineDurationTicks{0};
};

inline void GetParallaxSceneSummary(const ParallaxScene& scene, ParallaxSceneSummary& out) {
    out.ElementCount = scene.GetElements().size();
    out.ChannelCount = scene.GetChannels().size();
    out.MGElementCount = scene.GetMGElements().size();
    out.MGTrackCount = scene.GetMGTracks().size();
    out.TicksPerSecond = scene.GetTicksPerSecond();
    out.TimelineDurationTicks = scene.GetTimelineDurationTicks();
}

struct ParallaxValidationMessage {
    std::string Text;
};

/// Lightweight checks for authoring tools (does not replace full evaluation).
inline void ValidateParallaxSceneEditing(const ParallaxScene& scene, std::vector<ParallaxValidationMessage>& outMessages) {
    outMessages.clear();

    if (scene.GetTicksPerSecond() == 0) {
        ParallaxValidationMessage m;
        m.Text = "Ticks per second is zero; set a positive value.";
        outMessages.push_back(std::move(m));
    }

    const auto& els = scene.GetElements();
    const auto elCount = els.size();
    for (std::size_t i = 0; i < els.size(); ++i) {
        const auto& e = els[i];
        {
            const std::string_view st = GetElementSchema(scene, static_cast<ElementIndex>(i));
            if (st == "SmmFluidVolumeElement") {
                int32_t nx = 32, ny = 32, nz = 32;
                const ElementIndex eidx = static_cast<ElementIndex>(i);
                {
                    const AttributeValue avx = GetAttribute(scene, eidx, "ResolutionX");
                    if (const auto* v = std::get_if<int32_t>(&avx)) {
                        nx = *v;
                    }
                }
                {
                    const AttributeValue avy = GetAttribute(scene, eidx, "ResolutionY");
                    if (const auto* v = std::get_if<int32_t>(&avy)) {
                        ny = *v;
                    }
                }
                {
                    const AttributeValue avz = GetAttribute(scene, eidx, "ResolutionZ");
                    if (const auto* v = std::get_if<int32_t>(&avz)) {
                        nz = *v;
                    }
                }
                const int64_t cells = static_cast<int64_t>(nx) * static_cast<int64_t>(ny) * static_cast<int64_t>(nz);
                if (cells > kParallaxFluidInteriorCellBudget) {
                    ParallaxValidationMessage m;
                    m.Text = "Fluid volume '" + e.Name + "': Nx*Ny*Nz (" + std::to_string(nx) + "×" + std::to_string(ny) + "×"
                        + std::to_string(nz) + " = " + std::to_string(cells) + ") exceeds budget (" + std::to_string(
                                                                                           kParallaxFluidInteriorCellBudget)
                        + ").";
                    outMessages.push_back(std::move(m));
                }
            }
        }
        if (e.Parent != PARALLAX_INVALID_INDEX && static_cast<std::size_t>(e.Parent) >= elCount) {
            ParallaxValidationMessage m;
            m.Text = "Element '" + e.Name + "': Parent index out of range.";
            outMessages.push_back(std::move(m));
        }
        if (e.FirstChild != PARALLAX_INVALID_INDEX && static_cast<std::size_t>(e.FirstChild) >= elCount) {
            ParallaxValidationMessage m;
            m.Text = "Element '" + e.Name + "': FirstChild index out of range.";
            outMessages.push_back(std::move(m));
        }
        if (e.NextSibling != PARALLAX_INVALID_INDEX && static_cast<std::size_t>(e.NextSibling) >= elCount) {
            ParallaxValidationMessage m;
            m.Text = "Element '" + e.Name + "': NextSibling index out of range.";
            outMessages.push_back(std::move(m));
        }
    }

    const auto& chans = scene.GetChannels();
    const uint64_t dur = scene.GetTimelineDurationTicks();
    bool keyframePastDuration = false;
    for (const auto& c : chans) {
        if (c.Element != PARALLAX_INVALID_INDEX && static_cast<std::size_t>(c.Element) >= elCount) {
            ParallaxValidationMessage m;
            m.Text = "Channel references invalid element index " + std::to_string(c.Element) + ".";
            outMessages.push_back(std::move(m));
        }
        if (dur > 0) {
            for (const auto& kf : c.Keyframes) {
                if (kf.TimeTicks > dur) {
                    keyframePastDuration = true;
                    break;
                }
            }
        }
        if (keyframePastDuration) {
            break;
        }
    }
    if (keyframePastDuration) {
        ParallaxValidationMessage m;
        m.Text = "At least one keyframe time exceeds timeline duration (" + std::to_string(dur) + " ticks).";
        outMessages.push_back(std::move(m));
    }

    const auto& mgs = scene.GetMGElements();
    const auto mgCount = static_cast<MGIndex>(mgs.size());
    for (MGIndex i = 0; i < mgCount; ++i) {
        const auto& g = mgs[i];
        if (g.Parent != PARALLAX_INVALID_INDEX && g.Parent >= mgCount) {
            ParallaxValidationMessage m;
            m.Text = "MG element '" + g.Name + "': Parent index out of range.";
            outMessages.push_back(std::move(m));
        }
        if (g.FirstChild != PARALLAX_INVALID_INDEX && g.FirstChild >= mgCount) {
            ParallaxValidationMessage m;
            m.Text = "MG element '" + g.Name + "': FirstChild index out of range.";
            outMessages.push_back(std::move(m));
        }
        if (g.NextSibling != PARALLAX_INVALID_INDEX && g.NextSibling >= mgCount) {
            ParallaxValidationMessage m;
            m.Text = "MG element '" + g.Name + "': NextSibling index out of range.";
            outMessages.push_back(std::move(m));
        }
    }
}

} // namespace Solstice::Parallax
