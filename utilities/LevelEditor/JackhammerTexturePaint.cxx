#include "JackhammerTexturePaint.hxx"

#include "LibUI/Tools/RgbaImageFile.hxx"

#include <algorithm>
#include <cmath>

namespace Jackhammer::TexturePaint {

bool LoadOrCreateCanvas(RgbaCanvas& c, const std::string& pathUtf8, int defaultSize, std::string& errOut) {
    errOut.clear();
    c.rgba.clear();
    c.width = 0;
    c.height = 0;
    const int def = std::clamp(defaultSize, 16, 4096);
    if (!pathUtf8.empty() && LibUI::Tools::LoadImageFileToRgba8(pathUtf8, c.rgba, c.width, c.height)) {
        return true;
    }
    c.width = def;
    c.height = def;
    try {
        c.rgba.assign(static_cast<size_t>(def) * static_cast<size_t>(def) * 4u, std::byte{0});
    } catch (const std::bad_alloc&) {
        errOut = "Face paint: out of memory allocating canvas.";
        c.width = c.height = 0;
        return false;
    }
    for (int y = 0; y < def; ++y) {
        for (int x = 0; x < def; ++x) {
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(def) + static_cast<size_t>(x)) * 4u;
            c.rgba[i + 0] = std::byte{188};
            c.rgba[i + 1] = std::byte{188};
            c.rgba[i + 2] = std::byte{192};
            c.rgba[i + 3] = std::byte{255};
        }
    }
    if (!pathUtf8.empty()) {
        errOut = "Face paint: could not load image (empty or missing); started new canvas.";
    }
    return true;
}

static void BlendOver(std::byte* dst, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa, float strength) {
    auto* d = reinterpret_cast<unsigned char*>(dst);
    const float a = std::clamp(strength, 0.f, 1.f) * (static_cast<float>(sa) / 255.f);
    if (a <= 1e-5f) {
        return;
    }
    const float dr = static_cast<float>(d[0]);
    const float dg = static_cast<float>(d[1]);
    const float db = static_cast<float>(d[2]);
    const float da = static_cast<float>(d[3]);
    const float inv = 1.f - a;
    const float outA = a + da * inv;
    if (outA < 1e-3f) {
        d[0] = d[1] = d[2] = d[3] = 0;
        return;
    }
    const float oR = (static_cast<float>(sr) * a + dr * inv) / outA;
    const float oG = (static_cast<float>(sg) * a + dg * inv) / outA;
    const float oB = (static_cast<float>(sb) * a + db * inv) / outA;
    d[0] = static_cast<unsigned char>(std::clamp(oR, 0.f, 255.f));
    d[1] = static_cast<unsigned char>(std::clamp(oG, 0.f, 255.f));
    d[2] = static_cast<unsigned char>(std::clamp(oB, 0.f, 255.f));
    d[3] = static_cast<unsigned char>(std::clamp(outA, 0.f, 255.f));
}

void PaintDisc(RgbaCanvas& c, float u, float v, float radiusFrac, float hardness, uint8_t r, uint8_t g, uint8_t b, uint8_t a,
    bool erase) {
    if (c.width <= 0 || c.height <= 0 || c.rgba.size() != static_cast<size_t>(c.width) * static_cast<size_t>(c.height) * 4u) {
        return;
    }
    u = std::clamp(u, 0.f, 1.f);
    v = std::clamp(v, 0.f, 1.f);
    const float px = u * static_cast<float>(c.width - 1);
    const float py = v * static_cast<float>(c.height - 1);
    const int cx = static_cast<int>(std::lround(px));
    const int cy = static_cast<int>(std::lround(py));
    const int rad = std::max(
        1, static_cast<int>(std::lround(std::clamp(radiusFrac, 0.001f, 2.f) * static_cast<float>(std::min(c.width, c.height)))));
    const float hard = std::clamp(hardness, 0.f, 1.f);
    const int x0 = std::max(0, cx - rad);
    const int x1 = std::min(c.width - 1, cx + rad);
    const int y0 = std::max(0, cy - rad);
    const int y1 = std::min(c.height - 1, cy + rad);
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float dx = static_cast<float>(x) - px;
            const float dy = static_cast<float>(y) - py;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d > static_cast<float>(rad) + 1e-4f) {
                continue;
            }
            float t = 1.f - d / static_cast<float>(std::max(rad, 1));
            t = std::clamp(t, 0.f, 1.f);
            t = std::pow(t, 0.35f + 1.65f * hard);
            std::byte* p = c.rgba.data() + (static_cast<size_t>(y) * static_cast<size_t>(c.width) + static_cast<size_t>(x)) * 4u;
            if (erase) {
                const float keep = 1.f - t * (static_cast<float>(a) / 255.f);
                auto* pu = reinterpret_cast<unsigned char*>(p);
                const float da = static_cast<float>(pu[3]);
                pu[3] = static_cast<unsigned char>(std::clamp(da * keep, 0.f, 255.f));
            } else {
                BlendOver(p, r, g, b, a, t);
            }
        }
    }
}

void FillSolid(RgbaCanvas& c, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (c.width <= 0 || c.height <= 0) {
        return;
    }
    const size_t n = static_cast<size_t>(c.width) * static_cast<size_t>(c.height);
    auto* p = reinterpret_cast<unsigned char*>(c.rgba.data());
    for (size_t i = 0; i < n; ++i) {
        p[i * 4 + 0] = r;
        p[i * 4 + 1] = g;
        p[i * 4 + 2] = b;
        p[i * 4 + 3] = a;
    }
}

} // namespace Jackhammer::TexturePaint
