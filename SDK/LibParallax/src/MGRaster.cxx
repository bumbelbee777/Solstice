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
    ApplyBlendApprox(tr, tg, tb, ta, e.Blend);
    const float entryFactor = std::clamp(e.Alpha, 0.0f, 1.0f) * std::clamp(listGlobalAlpha, 0.0f, 1.0f);
    ScaleColorAlpha(tr, tg, tb, ta, entryFactor);

    int tw = 0;
    int th = 0;
    const uint8_t* tex = GetCachedTexture(assetHash, assets, texCache, tw, th);
    const float minX = posX;
    const float minY = posY;
    const float maxX = posX + sizeX;
    const float maxY = posY + sizeY;
    if (tex && tw > 0 && th > 0) {
        BlitSpriteScaled(tex, tw, th, minX, minY, maxX, maxY, tr, tg, tb, ta, rgba, w, h);
    } else {
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
    for (size_t idx : order) {
        DrawEntry(list.Entries[idx], kRootOrigin, kRootOrigin, ga, assets, s_GlobalMgDecodedTexCache, rgbaBuffer, width, height);
    }
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
