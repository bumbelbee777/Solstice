#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Smm::Keyframe {

/// One named curve key style (easing / interpolation) from `presets/Keyframe/*.ini` (and subfolders).
struct KeyframeCurvePreset {
    /// Section id after `KeyframeCurvePreset:` in the .ini.
    std::string Id;
    /// Optional display name (defaults to Id).
    std::string DisplayName;
    /// Optional: shown in the curve panel and for filtering; team handoff metadata.
    std::string Author;
    std::string Description;
    /// Comma- or space-separated list for simple filtering; optional.
    std::string Tags;
    /// Ease **into** this key (MinGfx 0 = Linear .. 13 = Bezier).
    uint8_t EaseIn{0};
    /// 0xFF = inherit on segment leaving this key; else same enum as EaseIn.
    uint8_t EaseOut{0xFF};
    /// `KeyframeInterpolation` (0 = Eased, 1 = Hold, 2 = Linear, 3 = Bezier).
    uint8_t Interp{0};
    float TangentIn{1.f / 3.f};
    float TangentOut{1.f / 3.f};
};

/// Merges all `presets/Keyframe/**/*.ini` under each root. Later sources **override** the same `Id` (e.g. project
/// `presets/` overwrites shipped defaults with the same section id).
void ScanCurvePresetsFromRoots(const std::vector<std::filesystem::path>& roots, std::vector<KeyframeCurvePreset>& out);

} // namespace Smm::Keyframe
