#include <Parallax/MGRaster.hxx>

#include <Parallax/IAssetResolver.hxx>

#include <Math/Vector.hxx>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <stb_easy_font.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Solstice::Parallax {

bool DecodeImageBytesToRgba(std::span<const std::byte> bytes, std::vector<std::byte>& outRgba, int& outW, int& outH) {
    outRgba.clear();
    outW = 0;
    outH = 0;
    if (bytes.empty()) {
        return false;
    }
    int comp = 0;
    unsigned char* pix = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(bytes.data()), static_cast<int>(bytes.size()), &outW, &outH,
        &comp, 4);
    if (!pix) {
        return false;
    }
    const size_t n = static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4;
    outRgba.resize(n);
    std::memcpy(outRgba.data(), pix, n);
    stbi_image_free(pix);
    return true;
}

void ApplyMGPostProcessRgba(
    const MGPostProcessSettings& post, uint32_t width, uint32_t height, std::span<std::byte> rgbaBuffer);

namespace {

constexpr float kRootOrigin = 16.f;

static void ScaleColorAlpha(uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a, float factor) {
    factor = std::clamp(factor, 0.0f, 1.0f);
    a = static_cast<uint8_t>(static_cast<float>(a) * factor);
}

static void ApplyBlendApprox(uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a, BlendMode mode) {
    switch (mode) {
        case BlendMode::Over:
            break;
        case BlendMode::Additive:
            r = static_cast<uint8_t>(std::min(255, static_cast<int>(r) + 55));
            g = static_cast<uint8_t>(std::min(255, static_cast<int>(g) + 55));
            b = static_cast<uint8_t>(std::min(255, static_cast<int>(b) + 40));
            break;
        case BlendMode::Multiply:
        default:
            r = static_cast<uint8_t>(static_cast<int>(r) * 200 / 255);
            g = static_cast<uint8_t>(static_cast<int>(g) * 200 / 255);
            b = static_cast<uint8_t>(static_cast<int>(b) * 200 / 255);
            break;
    }
}

static void BlendSrcOverBlack(uint8_t* d, uint8_t sr, uint8_t sg, uint8_t sb, uint8_t sa) {
    if (sa == 0) {
        return;
    }
    const float a = static_cast<float>(sa) / 255.f;
    const float ia = 1.f - a;
    d[0] = static_cast<uint8_t>(static_cast<float>(sr) * a + static_cast<float>(d[0]) * ia);
    d[1] = static_cast<uint8_t>(static_cast<float>(sg) * a + static_cast<float>(d[1]) * ia);
    d[2] = static_cast<uint8_t>(static_cast<float>(sb) * a + static_cast<float>(d[2]) * ia);
    d[3] = 255;
}

static void ClearBlackOpaque(std::span<std::byte> rgba, uint32_t w, uint32_t h) {
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    for (size_t i = 0; i < n; i += 4) {
        rgba[i] = std::byte{0};
        rgba[i + 1] = std::byte{0};
        rgba[i + 2] = std::byte{0};
        rgba[i + 3] = std::byte{255};
    }
}

static void FillRectSpan(std::span<std::byte> rgba, uint32_t w, uint32_t h, int x0, int y0, int x1, int y1, uint8_t sr,
    uint8_t sg, uint8_t sb, uint8_t sa) {
    x0 = std::max(0, x0);
    y0 = std::max(0, y0);
    x1 = std::min(static_cast<int>(w), x1);
    y1 = std::min(static_cast<int>(h), y1);
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            uint8_t* d = reinterpret_cast<uint8_t*>(rgba.data()) + (static_cast<size_t>(py) * w + static_cast<size_t>(px)) * 4;
            BlendSrcOverBlack(d, sr, sg, sb, sa);
        }
    }
}

static void RasterizeStbQuad(const unsigned char* vbase, int quadIndex, std::span<std::byte> rgba, uint32_t w, uint32_t h) {
    const unsigned char* q = vbase + quadIndex * 64;
    float minx = 1e30f;
    float miny = 1e30f;
    float maxx = -1e30f;
    float maxy = -1e30f;
    for (int v = 0; v < 4; ++v) {
        const float* vf = reinterpret_cast<const float*>(q + v * 16);
        minx = std::min(minx, vf[0]);
        miny = std::min(miny, vf[1]);
        maxx = std::max(maxx, vf[0]);
        maxy = std::max(maxy, vf[1]);
    }
    const uint8_t r = q[12];
    const uint8_t g = q[13];
    const uint8_t b = q[14];
    const uint8_t a = q[15];
    const int x0 = static_cast<int>(std::floor(minx));
    const int y0 = static_cast<int>(std::floor(miny));
    const int x1 = static_cast<int>(std::ceil(maxx));
    const int y1 = static_cast<int>(std::ceil(maxy));
    FillRectSpan(rgba, w, h, x0, y0, x1, y1, r, g, b, a);
}

static void BlitSpriteScaled(const uint8_t* src, int sw, int sh, float dx0, float dy0, float dx1, float dy1, uint8_t tr,
    uint8_t tg, uint8_t tb, uint8_t ta, std::span<std::byte> rgba, uint32_t w, uint32_t h) {
    const int ix0 = static_cast<int>(std::floor(std::min(dx0, dx1)));
    const int iy0 = static_cast<int>(std::floor(std::min(dy0, dy1)));
    const int ix1 = static_cast<int>(std::ceil(std::max(dx0, dx1)));
    const int iy1 = static_cast<int>(std::ceil(std::max(dy0, dy1)));
    const float rw = dx1 - dx0;
    const float rh = dy1 - dy0;
    if (rw <= 0.001f || rh <= 0.001f || sw <= 0 || sh <= 0) {
        return;
    }
    for (int py = std::max(0, iy0); py < std::min(static_cast<int>(h), iy1); ++py) {
        for (int px = std::max(0, ix0); px < std::min(static_cast<int>(w), ix1); ++px) {
            const float u = (static_cast<float>(px) + 0.5f - dx0) / rw;
            const float v = (static_cast<float>(py) + 0.5f - dy0) / rh;
            if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) {
                continue;
            }
            const float fx = u * static_cast<float>(sw - 1);
            const float fy = v * static_cast<float>(sh - 1);
            const int sx0 = static_cast<int>(std::floor(fx));
            const int sy0 = static_cast<int>(std::floor(fy));
            const int sx1 = std::min(sw - 1, sx0 + 1);
            const int sy1 = std::min(sh - 1, sy0 + 1);
            const float tx = fx - static_cast<float>(sx0);
            const float ty = fy - static_cast<float>(sy0);
            const auto samp = [&](int c) {
                const float p00 = static_cast<float>(src[(static_cast<size_t>(sy0) * static_cast<size_t>(sw) + static_cast<size_t>(sx0)) * 4
                    + static_cast<size_t>(c)]);
                const float p10 = static_cast<float>(src[(static_cast<size_t>(sy0) * static_cast<size_t>(sw) + static_cast<size_t>(sx1)) * 4
                    + static_cast<size_t>(c)]);
                const float p01 = static_cast<float>(src[(static_cast<size_t>(sy1) * static_cast<size_t>(sw) + static_cast<size_t>(sx0)) * 4
                    + static_cast<size_t>(c)]);
                const float p11 = static_cast<float>(src[(static_cast<size_t>(sy1) * static_cast<size_t>(sw) + static_cast<size_t>(sx1)) * 4
                    + static_cast<size_t>(c)]);
                const float p0 = p00 * (1.f - tx) + p10 * tx;
                const float p1 = p01 * (1.f - tx) + p11 * tx;
                return p0 * (1.f - ty) + p1 * ty;
            };
            const float mr = samp(0) * static_cast<float>(tr) / 255.f;
            const float mg = samp(1) * static_cast<float>(tg) / 255.f;
            const float mb = samp(2) * static_cast<float>(tb) / 255.f;
            const float ma = samp(3) * static_cast<float>(ta) / 255.f;
            uint8_t sr = static_cast<uint8_t>(std::clamp(mr, 0.f, 255.f));
            uint8_t sg = static_cast<uint8_t>(std::clamp(mg, 0.f, 255.f));
            uint8_t sb = static_cast<uint8_t>(std::clamp(mb, 0.f, 255.f));
            uint8_t sa = static_cast<uint8_t>(std::clamp(ma, 0.f, 255.f));
            uint8_t* d = reinterpret_cast<uint8_t*>(rgba.data()) + (static_cast<size_t>(py) * w + static_cast<size_t>(px)) * 4;
            BlendSrcOverBlack(d, sr, sg, sb, sa);
        }
    }
}

static float AttrF(const MGDisplayList::Entry& e, const char* k, float d) {
    auto it = e.Attributes.find(k);
    if (it == e.Attributes.end()) {
        return d;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return d;
}

static int AttrI(const MGDisplayList::Entry& e, const char* k, int d) {
    auto it = e.Attributes.find(k);
    if (it == e.Attributes.end()) {
        return d;
    }
    if (const auto* in = std::get_if<int32_t>(&it->second)) {
        return static_cast<int>(*in);
    }
    return d;
}

struct MGSpriteFxParams {
    float ChromaKeyR{0.f};
    float ChromaKeyG{1.f};
    float ChromaKeyB{0.f};
    float ChromaKeyTolerance{0.f};
    float ChromaKeyFeather{0.01f};
    float RotoscopeStrength{0.f};
    float RotoscopeEdgePx{2.f};
    float ZoomBlur{0.f};
    float ZoomBlurCenterU{0.5f};
    float ZoomBlurCenterV{0.5f};
};

static void SampleSpriteUVRgba(const uint8_t* src, int sw, int sh, float u, float v, float uScroll, float vScroll, float wu,
    float wv, float wphase, float& outR, float& outG, float& outB, float& outA) {
    u = (std::clamp)(u, 0.0f, 1.0f);
    v = (std::clamp)(v, 0.0f, 1.0f);
    float su = u + uScroll;
    float sv = v + vScroll;
    su = su - std::floor(su);
    sv = sv - std::floor(sv);
    if (std::abs(wu) + std::abs(wv) > 1e-6f) {
        su = (std::clamp)(su + wu * std::sin(6.2831853f * sv + wphase), 0.0f, 1.0f);
        sv = (std::clamp)(sv + wv * std::sin(6.2831853f * u + wphase * 0.5f), 0.0f, 1.0f);
    }
    const float fx = su * static_cast<float>(sw - 1);
    const float fy = sv * static_cast<float>(sh - 1);
    const int sx0 = static_cast<int>(std::floor(fx));
    const int sy0 = static_cast<int>(std::floor(fy));
    const int sx1 = (std::min)(sw - 1, sx0 + 1);
    const int sy1 = (std::min)(sh - 1, sy0 + 1);
    const float tx = fx - static_cast<float>(sx0);
    const float ty = fy - static_cast<float>(sy0);
    for (int c = 0; c < 4; ++c) {
        const float p00
            = static_cast<float>(src[((static_cast<size_t>(sy0) * static_cast<size_t>(sw) + static_cast<size_t>(sx0)) * 4u
                + static_cast<size_t>(c))]);
        const float p10
            = static_cast<float>(src[((static_cast<size_t>(sy0) * static_cast<size_t>(sw) + static_cast<size_t>(sx1)) * 4u
                + static_cast<size_t>(c))]);
        const float p01
            = static_cast<float>(src[((static_cast<size_t>(sy1) * static_cast<size_t>(sw) + static_cast<size_t>(sx0)) * 4u
                + static_cast<size_t>(c))]);
        const float p11
            = static_cast<float>(src[((static_cast<size_t>(sy1) * static_cast<size_t>(sw) + static_cast<size_t>(sx1)) * 4u
                + static_cast<size_t>(c))]);
        const float p0 = p00 * (1.f - tx) + p10 * tx;
        const float p1 = p01 * (1.f - tx) + p11 * tx;
        const float p = p0 * (1.f - ty) + p1 * ty;
        if (c == 0) {
            outR = p;
        } else if (c == 1) {
            outG = p;
        } else if (c == 2) {
            outB = p;
        } else {
            outA = p;
        }
    }
}

static void BlitMGSpriteRgba(
    const uint8_t* src, int sw, int sh, float x0, float y0, float w, float h, float rotRad, float offX, float offY,
    float uScroll, float vScroll, float wu, float wv, float wphase, float aoMult, float aoEdge, const MGSpriteFxParams& sprFx,
    uint8_t tr, uint8_t tg, uint8_t tb, uint8_t ta, std::span<std::byte> rgba, uint32_t rw, uint32_t rh) {
    if (sw <= 0 || sh <= 0 || w <= 0.01f || h <= 0.01f) {
        return;
    }
    const float co = std::cos(rotRad);
    const float si = std::sin(rotRad);
    const float hw = w * 0.5f;
    const float hh = h * 0.5f;
    const float cx = x0 + hw + offX;
    const float cy = y0 + hh + offY;
    const float aoe = std::clamp(aoEdge, 0.0f, 1.0f);
    const float aom = std::clamp(aoMult, 0.0f, 4.0f);
    if (aom <= 0.0001f) {
        return; // “fully unlit / culled” sprite
    }
    // Precompute AABB of four rotated local corners
    const float lxs[4] = {-hw, hw, hw, -hw};
    const float lys[4] = {-hh, -hh, hh, hh};
    float pminX = 1e30f, pminY = 1e30f, pmaxX = -1e30f, pmaxY = -1e30f;
    for (int i = 0; i < 4; ++i) {
        const float px = cx + lxs[i] * co - lys[i] * si;
        const float py = cy + lxs[i] * si + lys[i] * co;
        pminX = (std::min)(pminX, px);
        pminY = (std::min)(pminY, py);
        pmaxX = (std::max)(pmaxX, px);
        pmaxY = (std::max)(pmaxY, py);
    }
    const int px0 = (std::max)(0, static_cast<int>(std::floor(pminX)));
    const int py0 = (std::max)(0, static_cast<int>(std::floor(pminY)));
    const int px1 = (std::min)(static_cast<int>(rw), static_cast<int>(std::ceil(pmaxX)));
    const int py1 = (std::min)(static_cast<int>(rh), static_cast<int>(std::ceil(pmaxY)));
    const bool fast2d = std::abs(rotRad) < 0.0002f;
    for (int py = py0; py < py1; ++py) {
        for (int px = px0; px < px1; ++px) {
            const float ddx = static_cast<float>(px) + 0.5f - cx;
            const float ddy = static_cast<float>(py) + 0.5f - cy;
            float lx, ly;
            if (fast2d) {
                lx = ddx;
                ly = ddy;
            } else {
                lx = ddx * co + ddy * si;
                ly = -ddx * si + ddy * co;
            }
            if (lx < -hw + 0.0001f || lx > hw - 0.0001f || ly < -hh + 0.0001f || ly > hh - 0.0001f) {
                continue;
            }
            float u = (lx + hw) / (2.f * hw);
            float v = (ly + hh) / (2.f * hh);
            u = (std::clamp)(u, 0.0f, 1.0f);
            v = (std::clamp)(v, 0.0f, 1.0f);
            const float cux = (std::clamp)(sprFx.ZoomBlurCenterU, 0.f, 1.f);
            const float cvz = (std::clamp)(sprFx.ZoomBlurCenterV, 0.f, 1.f);
            const float zbf = (std::clamp)(sprFx.ZoomBlur, 0.f, 1.f);
            const int kSamples = (zbf > 1e-4f) ? 8 : 1;
            float trr = 0.f, tgg = 0.f, tbb = 0.f, taa = 0.f;
            for (int szi = 0; szi < kSamples; ++szi) {
                float t = 1.f;
                if (kSamples > 1) {
                    t = 1.f - zbf * (static_cast<float>(szi) / static_cast<float>(kSamples - 1));
                }
                const float uu = cux + (u - cux) * t;
                const float vv = cvz + (v - cvz) * t;
                float sR, sG, sB, sA;
                SampleSpriteUVRgba(src, sw, sh, uu, vv, uScroll, vScroll, wu, wv, wphase, sR, sG, sB, sA);
                trr += sR;
                tgg += sG;
                tbb += sB;
                taa += sA;
            }
            if (kSamples > 1) {
                const float inv = 1.f / static_cast<float>(kSamples);
                trr *= inv;
                tgg *= inv;
                tbb *= inv;
                taa *= inv;
            }
            if (sprFx.ChromaKeyTolerance > 1e-6f) {
                const float r1 = trr / 255.f;
                const float g1 = tgg / 255.f;
                const float b1 = tbb / 255.f;
                const float d = std::sqrt((r1 - sprFx.ChromaKeyR) * (r1 - sprFx.ChromaKeyR) + (g1 - sprFx.ChromaKeyG) * (g1 - sprFx.ChromaKeyG)
                    + (b1 - sprFx.ChromaKeyB) * (b1 - sprFx.ChromaKeyB));
                const float fe = (std::max)(1e-4f, sprFx.ChromaKeyFeather);
                float am = 1.f;
                if (d < sprFx.ChromaKeyTolerance) {
                    am = 0.f;
                } else if (d < sprFx.ChromaKeyTolerance + fe) {
                    am = (d - sprFx.ChromaKeyTolerance) / fe;
                }
                taa *= (std::clamp)(am, 0.f, 1.f);
            }
            if (sprFx.RotoscopeStrength > 1e-6f && sw > 2 && sh > 2) {
                const float dpu = 1.f / static_cast<float>((std::max)(1, sw - 1));
                const float dpv = 1.f / static_cast<float>((std::max)(1, sh - 1));
                auto sampA = [&](float u0, float v0) {
                    float R, G, B, A;
                    SampleSpriteUVRgba(
                        src, sw, sh, (std::clamp)(u0, 0.f, 1.f), (std::clamp)(v0, 0.f, 1.f), uScroll, vScroll, wu, wv, wphase, R, G, B, A);
                    return A;
                };
                const float aC = sampA(u, v);
                const float aL = sampA(u - dpu, v);
                const float aR = sampA(u + dpu, v);
                const float aU = sampA(u, v - dpv);
                const float aD = sampA(u, v + dpv);
                const float g1 = (std::max)(std::abs(aC - aL), std::abs(aC - aR));
                const float g2 = (std::max)(std::abs(aC - aU), std::abs(aC - aD));
                const float grad = (std::max)(g1, g2) / 255.f;
                const float edgeS = (std::clamp)(grad * (0.25f * sprFx.RotoscopeEdgePx + 0.4f), 0.f, 1.f);
                const float k = 1.f - (std::min)(1.f, sprFx.RotoscopeStrength) * edgeS;
                trr *= k;
                tgg *= k;
                tbb *= k;
            }
            const float dEdge = 2.f
                * (std::min)(u, (std::min)(1.f - u, (std::min)(v, 1.f - v)));
            const float mEdge = 1.f - aoe * (1.f - dEdge);
            float mr = trr * static_cast<float>(tr) / 255.f;
            float mg = tgg * static_cast<float>(tg) / 255.f;
            float mb = tbb * static_cast<float>(tb) / 255.f;
            float ma = taa * static_cast<float>(ta) / 255.f;
            mr = mr * aom * mEdge;
            mg = mg * aom * mEdge;
            mb = mb * aom * mEdge;
            uint8_t sr = static_cast<uint8_t>(std::clamp(mr, 0.f, 255.f));
            uint8_t sg = static_cast<uint8_t>(std::clamp(mg, 0.f, 255.f));
            uint8_t sb = static_cast<uint8_t>(std::clamp(mb, 0.f, 255.f));
            uint8_t sa = static_cast<uint8_t>(std::clamp(ma, 0.f, 255.f));
            uint8_t* d = reinterpret_cast<uint8_t*>(rgba.data()) + (static_cast<size_t>(py) * rw + static_cast<size_t>(px)) * 4;
            BlendSrcOverBlack(d, sr, sg, sb, sa);
        }
    }
}

static void DrawTextCpu(const MGDisplayList::Entry& e, float ox, float oy, float listGlobalAlpha, std::span<std::byte> rgba,
    uint32_t w, uint32_t h) {
    std::string text = "Text";
    auto itT = e.Attributes.find("Text");
    if (itT != e.Attributes.end()) {
        if (const auto* s = std::get_if<std::string>(&itT->second)) {
            text = *s;
        }
    }
    float px = ox;
    float py = oy;
    auto itP = e.Attributes.find("Position");
    if (itP != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec2>(&itP->second)) {
            px = ox + v->x;
            py = oy + v->y;
        }
    }
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
    auto itC = e.Attributes.find("Color");
    if (itC != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec4>(&itC->second)) {
            r = static_cast<uint8_t>(std::clamp(v->x * 255.f, 0.f, 255.f));
            g = static_cast<uint8_t>(std::clamp(v->y * 255.f, 0.f, 255.f));
            b = static_cast<uint8_t>(std::clamp(v->z * 255.f, 0.f, 255.f));
            a = static_cast<uint8_t>(std::clamp(v->w * 255.f, 0.f, 255.f));
        }
    }
    ApplyBlendApprox(r, g, b, a, e.Blend);
    const float entryFactor = std::clamp(e.Alpha, 0.0f, 1.0f) * std::clamp(listGlobalAlpha, 0.0f, 1.0f);
    ScaleColorAlpha(r, g, b, a, entryFactor);

    std::vector<char> mut;
    mut.reserve(text.size() + 1);
    for (char c : text) {
        if (c == '\n' || (static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 128)) {
            mut.push_back(c);
        } else {
            mut.push_back('?');
        }
    }
    mut.push_back(0);

    unsigned char col4[4] = {r, g, b, a};
    alignas(16) unsigned char vbuf[99999];
    const int numQuads = stb_easy_font_print(px, py, reinterpret_cast<char*>(mut.data()), col4, vbuf, static_cast<int>(sizeof(vbuf)));
    for (int qi = 0; qi < numQuads; ++qi) {
        RasterizeStbQuad(vbuf, qi, rgba, w, h);
    }
}

struct DecodedTex {
    int w{0};
    int h{0};
    std::vector<uint8_t> rgba;
};

static const uint8_t* GetCachedTexture(uint64_t hash, IAssetResolver* assets, std::unordered_map<uint64_t, DecodedTex>& cache,
    int& outW, int& outH) {
    auto it = cache.find(hash);
    if (it != cache.end()) {
        outW = it->second.w;
        outH = it->second.h;
        return it->second.rgba.data();
    }
    if (!assets) {
        return nullptr;
    }
    AssetData ad;
    if (!assets->Resolve(hash, ad) || ad.Bytes.empty()) {
        return nullptr;
    }
    std::vector<std::byte> outBytes;
    int iw = 0;
    int ih = 0;
    if (!DecodeImageBytesToRgba(std::span<const std::byte>(ad.Bytes.data(), ad.Bytes.size()), outBytes, iw, ih)) {
        return nullptr;
    }
    DecodedTex dt;
    dt.w = iw;
    dt.h = ih;
    dt.rgba.resize(outBytes.size());
    std::memcpy(dt.rgba.data(), outBytes.data(), outBytes.size());
    cache[hash] = std::move(dt);
    outW = cache[hash].w;
    outH = cache[hash].h;
    return cache[hash].rgba.data();
}

static void DrawSpriteCpu(const MGDisplayList::Entry& e, float ox, float oy, float listGlobalAlpha, IAssetResolver* assets,
    std::unordered_map<uint64_t, DecodedTex>& texCache, std::span<std::byte> rgba, uint32_t w, uint32_t h) {
    float posX = ox;
    float posY = oy;
    float sizeX = 64.f;
    float sizeY = 64.f;
    uint64_t assetHash = 0;
    auto itP = e.Attributes.find("Position");
    if (itP != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec2>(&itP->second)) {
            posX = ox + v->x;
            posY = oy + v->y;
        }
    }
    auto itS = e.Attributes.find("Size");
    if (itS != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec2>(&itS->second)) {
            sizeX = v->x;
            sizeY = v->y;
        }
    }
    auto itTex = e.Attributes.find("Texture");
    if (itTex != e.Attributes.end()) {
        if (const auto* hh = std::get_if<uint64_t>(&itTex->second)) {
            assetHash = *hh;
        }
    }
    uint8_t tr = 255;
    uint8_t tg = 255;
    uint8_t tb = 255;
    uint8_t ta = 255;
    {
        auto itC = e.Attributes.find("Color");
        if (itC != e.Attributes.end()) {
            if (const auto* c = std::get_if<Math::Vec4>(&itC->second)) {
                tr = static_cast<uint8_t>(std::clamp(c->x * 255.f, 0.f, 255.f));
                tg = static_cast<uint8_t>(std::clamp(c->y * 255.f, 0.f, 255.f));
                tb = static_cast<uint8_t>(std::clamp(c->z * 255.f, 0.f, 255.f));
                ta = static_cast<uint8_t>(std::clamp(c->w * 255.f, 0.f, 255.f));
            }
        }
    }
    ApplyBlendApprox(tr, tg, tb, ta, e.Blend);
    const float entryFactor = std::clamp(e.Alpha, 0.0f, 1.0f) * std::clamp(listGlobalAlpha, 0.0f, 1.0f);
    ScaleColorAlpha(tr, tg, tb, ta, entryFactor);

    float rotZ = AttrF(e, "RotationZ", 0.f);
    const int bbm = AttrI(e, "BillboardMode", 0);
    if (bbm != 0) {
        const float ph = AttrF(e, "UvPhase", 0.f);
        rotZ += 0.12f * static_cast<float>(bbm) * std::sin(ph * 0.05f);
    }
    const int smearN = (std::clamp)(AttrI(e, "SmearCount", 0), 0, 8);
    const float smx = AttrF(e, "SmearDx", 0.f);
    const float smy = AttrF(e, "SmearDy", 0.f);
    const float sfall = std::clamp(AttrF(e, "SmearFalloff", 0.65f), 0.05f, 0.999f);
    const float uSc = AttrF(e, "UvScrollU", 0.f);
    const float vSc = AttrF(e, "UvScrollV", 0.f);
    const float wu = AttrF(e, "UvWaveU", 0.f);
    const float wv = AttrF(e, "UvWaveV", 0.f);
    const float wph = AttrF(e, "UvPhase", 0.f);
    const float aoM = std::max(0.f, AttrF(e, "AOMultiply", 1.f));
    const float aoE = (std::clamp)(AttrF(e, "AOEdge", 0.f), 0.f, 1.f);
    MGSpriteFxParams spr;
    {
        const auto itCk = e.Attributes.find("ChromaKeyColor");
        if (itCk != e.Attributes.end() && std::get_if<Math::Vec3>(&itCk->second)) {
            const auto& kv = *std::get_if<Math::Vec3>(&itCk->second);
            spr.ChromaKeyR = kv.x;
            spr.ChromaKeyG = kv.y;
            spr.ChromaKeyB = kv.z;
        } else {
            spr.ChromaKeyR = 0.f;
            spr.ChromaKeyG = 1.f;
            spr.ChromaKeyB = 0.f;
        }
        spr.ChromaKeyTolerance = AttrF(e, "ChromaKeyTolerance", 0.f);
        spr.ChromaKeyFeather = (std::max)(1e-4f, AttrF(e, "ChromaKeyFeather", 0.05f));
        spr.RotoscopeStrength = (std::max)(0.f, AttrF(e, "RotoscopeStrength", 0.f));
        spr.RotoscopeEdgePx = (std::max)(0.01f, AttrF(e, "RotoscopeEdgePx", 2.f));
        spr.ZoomBlur = (std::clamp)(AttrF(e, "ZoomBlur", 0.f), 0.f, 1.f);
        spr.ZoomBlurCenterU = (std::clamp)(AttrF(e, "ZoomBlurCenterU", 0.5f), 0.f, 1.f);
        spr.ZoomBlurCenterV = (std::clamp)(AttrF(e, "ZoomBlurCenterV", 0.5f), 0.f, 1.f);
    }

    int tw = 0;
    int th = 0;
    const uint8_t* tex = GetCachedTexture(assetHash, assets, texCache, tw, th);
    if (tex && tw > 0 && th > 0) {
        for (int s = smearN; s >= 1; --s) {
            const float g = static_cast<float>(std::pow(sfall, static_cast<double>(s)));
            uint8_t ttr = tr;
            uint8_t ttg = tg;
            uint8_t ttb = tb;
            uint8_t tta = static_cast<uint8_t>(std::clamp(static_cast<float>(ta) * g, 0.f, 255.f));
            const float oxf = smx * static_cast<float>(s);
            const float oyf = smy * static_cast<float>(s);
            BlitMGSpriteRgba(
                tex, tw, th, posX, posY, sizeX, sizeY, rotZ, oxf, oyf, uSc, vSc, wu, wv, wph, aoM, aoE, spr, ttr, ttg, ttb, tta, rgba, w, h);
        }
        BlitMGSpriteRgba(
            tex, tw, th, posX, posY, sizeX, sizeY, rotZ, 0.f, 0.f, uSc, vSc, wu, wv, wph, aoM, aoE, spr, tr, tg, tb, ta, rgba, w, h);
    } else {
        const float minX = posX;
        const float minY = posY;
        const float maxX = posX + sizeX;
        const float maxY = posY + sizeY;
        uint8_t pr = static_cast<uint8_t>(200.f * static_cast<float>(tr) / 255.f);
        uint8_t pg = static_cast<uint8_t>(80.f * static_cast<float>(tg) / 255.f);
        uint8_t pb = static_cast<uint8_t>(220.f * static_cast<float>(tb) / 255.f);
        uint8_t pa = static_cast<uint8_t>(std::clamp(200.f * static_cast<float>(ta) / 255.f, 0.f, 255.f));
        const int x0 = static_cast<int>(std::floor(minX));
        const int y0 = static_cast<int>(std::floor(minY));
        const int x1 = static_cast<int>(std::ceil(maxX));
        const int y1 = static_cast<int>(std::ceil(maxY));
        FillRectSpan(rgba, w, h, x0, y0, x1, y1, pr, pg, pb, pa);
        const uint8_t wr = 255;
        const uint8_t wg = 255;
        const uint8_t wb = 255;
        const uint8_t wa = 90;
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                if (px < 0 || py < 0 || px >= static_cast<int>(w) || py >= static_cast<int>(h)) {
                    continue;
                }
                const bool edge = (px == x0 || py == y0 || px == x1 - 1 || py == y1 - 1);
                if (edge) {
                    uint8_t* d = reinterpret_cast<uint8_t*>(rgba.data()) + (static_cast<size_t>(py) * w + static_cast<size_t>(px)) * 4;
                    BlendSrcOverBlack(d, wr, wg, wb, wa);
                }
            }
        }
    }
}

static void DrawEntry(const MGDisplayList::Entry& e, float originX, float originY, float listGlobalAlpha, IAssetResolver* assets,
    std::unordered_map<uint64_t, DecodedTex>& texCache, std::span<std::byte> rgba, uint32_t w, uint32_t h) {
    if (e.SchemaType == "MGTextElement") {
        DrawTextCpu(e, originX, originY, listGlobalAlpha, rgba, w, h);
    } else if (e.SchemaType == "MGSpriteElement") {
        DrawSpriteCpu(e, originX, originY, listGlobalAlpha, assets, texCache, rgba, w, h);
    }
    float childOriginX = originX;
    float childOriginY = originY;
    auto itParentPos = e.Attributes.find("Position");
    if (itParentPos != e.Attributes.end()) {
        if (const auto* v = std::get_if<Math::Vec2>(&itParentPos->second)) {
            childOriginX = originX + v->x;
            childOriginY = originY + v->y;
        }
    }
    for (const auto& c : e.Children) {
        DrawEntry(c, childOriginX, childOriginY, listGlobalAlpha, assets, texCache, rgba, w, h);
    }
}

static thread_local std::unordered_map<uint64_t, DecodedTex> s_GlobalMgDecodedTexCache;

static void RasterizeMGDisplayListInner(const MGDisplayList& list, IAssetResolver* assets, uint32_t width, uint32_t height,
    std::span<std::byte> rgbaBuffer) {
    ClearBlackOpaque(rgbaBuffer, width, height);
    if (s_GlobalMgDecodedTexCache.size() > 200) {
        s_GlobalMgDecodedTexCache.clear();
    }
    const float ga = std::clamp(list.GlobalAlpha, 0.0f, 1.0f);
    std::vector<size_t> order(list.Entries.size());
    for (size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t ai, size_t bi) {
        return MGEntryCompositeDepth(list.Entries[ai]) < MGEntryCompositeDepth(list.Entries[bi]);
    });
    const float ox = kRootOrigin + list.Post.ScreenShakeX;
    const float oy = kRootOrigin + list.Post.ScreenShakeY;
    for (size_t idx : order) {
        DrawEntry(list.Entries[idx], ox, oy, ga, assets, s_GlobalMgDecodedTexCache, rgbaBuffer, width, height);
    }
    ApplyMGPostProcessRgba(list.Post, width, height, rgbaBuffer);
}

static void Downsample2x2Box(const std::span<const std::byte> src, uint32_t sw, uint32_t sh, std::span<std::byte> dst, uint32_t dw,
    uint32_t dh) {
    if (sw != dw * 2u || sh != dh * 2u) {
        return;
    }
    const auto* s = reinterpret_cast<const uint8_t*>(src.data());
    auto* d = reinterpret_cast<uint8_t*>(dst.data());
    for (uint32_t y = 0; y < dh; ++y) {
        for (uint32_t x = 0; x < dw; ++x) {
            const size_t s0 = (static_cast<size_t>(y * 2u) * sw + static_cast<size_t>(x * 2u)) * 4u;
            const size_t s1 = s0 + 4u;
            const size_t s2 = s0 + static_cast<size_t>(sw) * 4u;
            const size_t s3 = s2 + 4u;
            for (int c = 0; c < 4; ++c) {
                const unsigned sum = static_cast<unsigned>(s[s0 + c]) + static_cast<unsigned>(s[s1 + c]) + static_cast<unsigned>(s[s2 + c])
                    + static_cast<unsigned>(s[s3 + c]);
                d[(static_cast<size_t>(y) * dw + static_cast<size_t>(x)) * 4u + static_cast<size_t>(c)] =
                    static_cast<uint8_t>((sum + 2u) / 4u);
            }
        }
    }
}

} // namespace

void ApplyMGPostProcessRgba(
    const MGPostProcessSettings& post, uint32_t width, uint32_t height, std::span<std::byte> rgbaBuffer) {
    if (width == 0u || height == 0u) {
        return;
    }
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    if (rgbaBuffer.size() < need) {
        return;
    }
    const int wi = static_cast<int>(width);
    const int hi = static_cast<int>(height);
    const float dpx = (std::max)(0.f, post.ChromaticAberrationPx);
    const int off = static_cast<int>(std::lround(dpx));
    const bool needCa = off > 0;
    const float expG = (std::max)(0.f, post.GradeExposure);
    const float sat = (std::max)(0.f, post.GradeSaturation);
    const float con = (std::max)(0.01f, post.GradeContrast);
    const float lift = post.GradeLift;
    const bool needGrade
        = std::abs(expG - 1.f) > 1e-4f || std::abs(sat - 1.f) > 1e-4f || std::abs(con - 1.f) > 1e-4f || std::abs(lift) > 1e-5f;
    if (!needCa && !needGrade) {
        return;
    }
    std::vector<std::byte> copy;
    if (needCa) {
        copy.assign(rgbaBuffer.begin(), rgbaBuffer.end());
    }
    const uint8_t* pre = needCa ? reinterpret_cast<const uint8_t*>(copy.data()) : nullptr;
    uint8_t* dst = reinterpret_cast<uint8_t*>(rgbaBuffer.data());
    for (int y = 0; y < hi; ++y) {
        for (int x = 0; x < wi; ++x) {
            const size_t o = (static_cast<size_t>(y) * static_cast<size_t>(wi) + static_cast<size_t>(x)) * 4u;
            const int xr = (std::max)(0, (std::min)(wi - 1, x - off));
            const int xb = (std::max)(0, (std::min)(wi - 1, x + off));
            float r, g, b, a;
            if (needCa) {
                r = static_cast<float>(pre[(static_cast<size_t>(y) * static_cast<size_t>(wi) + static_cast<size_t>(xr)) * 4u]);
                g = static_cast<float>(pre[(static_cast<size_t>(y) * static_cast<size_t>(wi) + static_cast<size_t>(x)) * 4u + 1u]);
                b = static_cast<float>(pre[(static_cast<size_t>(y) * static_cast<size_t>(wi) + static_cast<size_t>(xb)) * 4u + 2u]);
                a = static_cast<float>(pre[(static_cast<size_t>(y) * static_cast<size_t>(wi) + static_cast<size_t>(x)) * 4u + 3u]);
            } else {
                r = static_cast<float>(dst[o]);
                g = static_cast<float>(dst[o + 1]);
                b = static_cast<float>(dst[o + 2]);
                a = static_cast<float>(dst[o + 3]);
            }
            if (needGrade) {
                r *= expG;
                g *= expG;
                b *= expG;
                const float yl = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = yl + sat * (r - yl);
                g = yl + sat * (g - yl);
                b = yl + sat * (b - yl);
                const float inv = 1.f / 255.f;
                r = (r * inv - 0.5f) * con + 0.5f + lift;
                g = (g * inv - 0.5f) * con + 0.5f + lift;
                b = (b * inv - 0.5f) * con + 0.5f + lift;
                r = (std::clamp)(r * 255.f, 0.f, 255.f);
                g = (std::clamp)(g * 255.f, 0.f, 255.f);
                b = (std::clamp)(b * 255.f, 0.f, 255.f);
            }
            dst[o] = static_cast<uint8_t>(r + 0.5f);
            dst[o + 1] = static_cast<uint8_t>(g + 0.5f);
            dst[o + 2] = static_cast<uint8_t>(b + 0.5f);
            dst[o + 3] = static_cast<uint8_t>(a + 0.5f);
        }
    }
}

void RasterizeMGDisplayList(const MGDisplayList& list, IAssetResolver* assets, uint32_t width, uint32_t height,
    std::span<std::byte> rgbaBuffer) {
    const size_t need = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (rgbaBuffer.size() < need) {
        return;
    }
    const bool use2x = width >= 2u && height >= 2u && width <= 2048u && height <= 2048u
        && static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * 16u
            < (uint64_t{1} << 29); // cap ~537M for 4x buffer
    if (use2x) {
        const uint32_t w2 = width * 2u;
        const uint32_t h2 = height * 2u;
        const size_t n2 = static_cast<size_t>(w2) * static_cast<size_t>(h2) * 4u;
        std::vector<std::byte> high(n2);
        RasterizeMGDisplayListInner(list, assets, w2, h2, std::span<std::byte>(high.data(), high.size()));
        Downsample2x2Box(std::span<const std::byte>(high.data(), high.size()), w2, h2, rgbaBuffer, width, height);
        return;
    }
    RasterizeMGDisplayListInner(list, assets, width, height, rgbaBuffer);
}

} // namespace Solstice::Parallax
