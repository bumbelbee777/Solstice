#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace Smm {

inline constexpr uint32_t kSmmMaxRasterEdgePx = 4096;
inline constexpr uint64_t kSmmMaxRasterPixels = 16ull * 1024ull * 1024ull;
inline constexpr size_t kSmmMaxEnginePreviewEntities = 768;

inline void ClampMgRasterSize(int& iw, int& ih) {
    iw = (std::max)(2, (std::min)(iw, static_cast<int>(kSmmMaxRasterEdgePx)));
    ih = (std::max)(2, (std::min)(ih, static_cast<int>(kSmmMaxRasterEdgePx)));
    while (static_cast<uint64_t>(iw) * static_cast<uint64_t>(ih) > kSmmMaxRasterPixels) {
        iw = (std::max)(32, iw - (std::max)(1, iw / 8));
        ih = (std::max)(32, ih - (std::max)(1, ih / 8));
    }
}

inline void ClampEnginePreviewFramebuffer(int& w, int& h) {
    w = (std::max)(2, (std::min)(w, static_cast<int>(kSmmMaxRasterEdgePx)));
    h = (std::max)(2, (std::min)(h, static_cast<int>(kSmmMaxRasterEdgePx)));
    while (static_cast<uint64_t>(w) * static_cast<uint64_t>(h) > kSmmMaxRasterPixels) {
        w = (std::max)(32, w - (std::max)(1, w / 8));
        h = (std::max)(32, h - (std::max)(1, h / 8));
    }
}

} // namespace Smm
