#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Jackhammer::TexturePaint {

struct RgbaCanvas {
    std::vector<std::byte> rgba;
    int width = 0;
    int height = 0;
};

/// Load existing image or allocate `defaultSize`×`defaultSize` neutral gray RGBA.
bool LoadOrCreateCanvas(RgbaCanvas& c, const std::string& pathUtf8, int defaultSize, std::string& errOut);

/// Normalized UV: (0,0) top-left, (1,1) bottom-right. `radiusFrac` is fraction of min(w,h). `hardness` 0 = soft falloff, 1 = hard.
void PaintDisc(RgbaCanvas& c, float u, float v, float radiusFrac, float hardness, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    bool erase);

void FillSolid(RgbaCanvas& c, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

} // namespace Jackhammer::TexturePaint
