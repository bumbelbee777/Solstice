#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Smm::Timeline {

/// Named tick range for nested sub-timeline / loop work (see `presets/Timeline/*.ini`).
struct RangePreset {
    std::string Id;
    std::string DisplayName;
    std::string Author;
    std::string Description;
    std::string Tags;
    uint64_t StartTick{0};
    /// Exclusive end (same convention as nested **end (exclusive)** in the main timeline UI).
    uint64_t EndTick{1};
};

void ScanRangePresetsFromRoots(const std::vector<std::filesystem::path>& roots, std::vector<RangePreset>& out);

} // namespace Smm::Timeline
