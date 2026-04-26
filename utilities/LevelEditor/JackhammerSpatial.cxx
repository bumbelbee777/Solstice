#include "JackhammerSpatial.hxx"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace Jackhammer::Spatial {

SmfVec3 NormalizeSmfVec3(const SmfVec3& v) {
    const float il = 1.f / std::max(std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z), 1e-6f);
    return SmfVec3{v.x * il, v.y * il, v.z * il};
}

void OrthoBasisFromNormal(const SmfVec3& nUnit, SmfVec3& u, SmfVec3& v) {
    SmfVec3 up{0.f, 1.f, 0.f};
    if (std::fabs(nUnit.y) > 0.92f) {
        up = SmfVec3{1.f, 0.f, 0.f};
    }
    u.x = up.y * nUnit.z - up.z * nUnit.y;
    u.y = up.z * nUnit.x - up.x * nUnit.z;
    u.z = up.x * nUnit.y - up.y * nUnit.x;
    const float ul = std::max(std::sqrt(u.x * u.x + u.y * u.y + u.z * u.z), 1e-6f);
    u.x /= ul;
    u.y /= ul;
    u.z /= ul;
    v.x = nUnit.y * u.z - nUnit.z * u.y;
    v.y = nUnit.z * u.x - nUnit.x * u.z;
    v.z = nUnit.x * u.y - nUnit.y * u.x;
}

void SmfAabbCanonical(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a, const SmfVec3& b) {
    outMin.x = std::min(a.x, b.x);
    outMax.x = std::max(a.x, b.x);
    outMin.y = std::min(a.y, b.y);
    outMax.y = std::max(a.y, b.y);
    outMin.z = std::min(a.z, b.z);
    outMax.z = std::max(a.z, b.z);
}

bool SmfAabbIntersect(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1, const SmfVec3& b0,
    const SmfVec3& b1) {
    SmfVec3 amin, amax, bmin, bmax;
    SmfAabbCanonical(amin, amax, a0, a1);
    SmfAabbCanonical(bmin, bmax, b0, b1);
    outMin.x = std::max(amin.x, bmin.x);
    outMin.y = std::max(amin.y, bmin.y);
    outMin.z = std::max(amin.z, bmin.z);
    outMax.x = std::min(amax.x, bmax.x);
    outMax.y = std::min(amax.y, bmax.y);
    outMax.z = std::min(amax.z, bmax.z);
    return outMin.x <= outMax.x && outMin.y <= outMax.y && outMin.z <= outMax.z;
}

void SmfAabbUnion(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1, const SmfVec3& b0,
    const SmfVec3& b1) {
    SmfVec3 amin, amax, bmin, bmax;
    SmfAabbCanonical(amin, amax, a0, a1);
    SmfAabbCanonical(bmin, bmax, b0, b1);
    outMin.x = std::min(amin.x, bmin.x);
    outMin.y = std::min(amin.y, bmin.y);
    outMin.z = std::min(amin.z, bmin.z);
    outMax.x = std::max(amax.x, bmax.x);
    outMax.y = std::max(amax.y, bmax.y);
    outMax.z = std::max(amax.z, bmax.z);
}

float BspPlaneSignedAnchorD(const SmfAuthoringBspNode& pl) {
    const SmfVec3 nraw = pl.PlaneNormal;
    const SmfVec3 p0{nraw.x * pl.PlaneD, nraw.y * pl.PlaneD, nraw.z * pl.PlaneD};
    const SmfVec3 nu = NormalizeSmfVec3(nraw);
    return nu.x * p0.x + nu.y * p0.y + nu.z * p0.z;
}

void ReflectSmfVec3AcrossBspPlane(SmfVec3& o, const SmfVec3& p, const SmfAuthoringBspNode& pl) {
    const SmfVec3 nu = NormalizeSmfVec3(pl.PlaneNormal);
    const float d = BspPlaneSignedAnchorD(pl);
    const float dist = nu.x * p.x + nu.y * p.y + nu.z * p.z - d;
    o.x = p.x - 2.f * dist * nu.x;
    o.y = p.y - 2.f * dist * nu.y;
    o.z = p.z - 2.f * dist * nu.z;
}

void MirrorSmfAabbAcrossBspPlane(SmfVec3& oMin, SmfVec3& oMax, const SmfVec3& iMin, const SmfVec3& iMax,
    const SmfAuthoringBspNode& mirrorPlane) {
    SmfVec3 amin, amax;
    SmfAabbCanonical(amin, amax, iMin, iMax);
    const float bx[2] = {amin.x, amax.x};
    const float by[2] = {amin.y, amax.y};
    const float bz[2] = {amin.z, amax.z};
    bool first = true;
    for (int ix = 0; ix < 2; ++ix) {
        for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
                SmfVec3 r{};
                ReflectSmfVec3AcrossBspPlane(r, SmfVec3{bx[ix], by[iy], bz[iz]}, mirrorPlane);
                if (first) {
                    oMin = r;
                    oMax = r;
                    first = false;
                } else {
                    oMin.x = std::min(oMin.x, r.x);
                    oMin.y = std::min(oMin.y, r.y);
                    oMin.z = std::min(oMin.z, r.z);
                    oMax.x = std::max(oMax.x, r.x);
                    oMax.y = std::max(oMax.y, r.y);
                    oMax.z = std::max(oMax.z, r.z);
                }
            }
        }
    }
}

void ExpandSmfAabb(SmfVec3& ioMin, SmfVec3& ioMax, float margin) {
    SmfVec3 nmin, nmax;
    SmfAabbCanonical(nmin, nmax, ioMin, ioMax);
    const float m = std::max(margin, 0.f);
    nmin.x -= m;
    nmin.y -= m;
    nmin.z -= m;
    nmax.x += m;
    nmax.y += m;
    nmax.z += m;
    ioMin = nmin;
    ioMax = nmax;
}

bool InsetSmfAabb(SmfVec3& ioMin, SmfVec3& ioMax, float margin) {
    SmfVec3 nmin, nmax;
    SmfAabbCanonical(nmin, nmax, ioMin, ioMax);
    const float m = std::max(margin, 0.f);
    nmin.x += m;
    nmin.y += m;
    nmin.z += m;
    nmax.x -= m;
    nmax.y -= m;
    nmax.z -= m;
    if (nmin.x > nmax.x || nmin.y > nmax.y || nmin.z > nmax.z) {
        return false;
    }
    ioMin = nmin;
    ioMax = nmax;
    return true;
}

float SmfAabbVolume(const SmfVec3& a0, const SmfVec3& a1) {
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, a0, a1);
    const float dx = hi.x - lo.x;
    const float dy = hi.y - lo.y;
    const float dz = hi.z - lo.z;
    return dx * dy * dz;
}

void SnapSmfAabbToGrid(SmfVec3& ioMin, SmfVec3& ioMax, float grid) {
    const float g = std::max(grid, 1e-6f);
    auto q = [g](float x) { return std::round(x / g) * g; };
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, ioMin, ioMax);
    lo.x = q(lo.x);
    lo.y = q(lo.y);
    lo.z = q(lo.z);
    hi.x = q(hi.x);
    hi.y = q(hi.y);
    hi.z = q(hi.z);
    ioMin = lo;
    ioMax = hi;
}

bool SmfAabbLargestFaceStripOutsideB(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1) {
    SmfVec3 amin, amax, bmin, bmax;
    SmfAabbCanonical(amin, amax, a0, a1);
    SmfAabbCanonical(bmin, bmax, b0, b1);
    SmfVec3 capMin, capMax;
    if (!SmfAabbIntersect(capMin, capMax, amin, amax, bmin, bmax)) {
        outMin = amin;
        outMax = amax;
        return true;
    }
    struct Cand {
        SmfVec3 mn;
        SmfVec3 mx;
    };
    Cand cands[6]{};
    int n = 0;
    if (amin.x < bmin.x) {
        cands[n].mn = amin;
        cands[n].mx = SmfVec3{std::min(amax.x, bmin.x), amax.y, amax.z};
        ++n;
    }
    if (bmax.x < amax.x) {
        cands[n].mn = SmfVec3{std::max(amin.x, bmax.x), amin.y, amin.z};
        cands[n].mx = amax;
        ++n;
    }
    if (amin.y < bmin.y) {
        cands[n].mn = SmfVec3{amin.x, amin.y, amin.z};
        cands[n].mx = SmfVec3{amax.x, std::min(amax.y, bmin.y), amax.z};
        ++n;
    }
    if (bmax.y < amax.y) {
        cands[n].mn = SmfVec3{amin.x, std::max(amin.y, bmax.y), amin.z};
        cands[n].mx = SmfVec3{amax.x, amax.y, amax.z};
        ++n;
    }
    if (amin.z < bmin.z) {
        cands[n].mn = amin;
        cands[n].mx = SmfVec3{amax.x, amax.y, std::min(amax.z, bmin.z)};
        ++n;
    }
    if (bmax.z < amax.z) {
        cands[n].mn = SmfVec3{amin.x, amin.y, std::max(amin.z, bmax.z)};
        cands[n].mx = amax;
        ++n;
    }
    float bestV = -1.f;
    int bestI = -1;
    for (int i = 0; i < n; ++i) {
        const float v = SmfAabbVolume(cands[i].mn, cands[i].mx);
        if (v > bestV) {
            bestV = v;
            bestI = i;
        }
    }
    if (bestI < 0 || bestV <= 0.f) {
        return false;
    }
    outMin = cands[bestI].mn;
    outMax = cands[bestI].mx;
    return true;
}

SmfAuthoringBspNode MakeJhBspBoxPlaneNode(const SmfVec3& normal, float planeD, const SmfVec3& slabMin,
    const SmfVec3& slabMax, const std::string& frontTexture, const std::string& backTexture) {
    SmfAuthoringBspNode nd{};
    nd.PlaneNormal = normal;
    nd.PlaneD = planeD;
    nd.FrontChild = -1;
    nd.BackChild = -1;
    nd.LeafId = 0xFFFFFFFFu;
    nd.FrontTexturePath = frontTexture;
    nd.BackTexturePath = backTexture;
    nd.SlabValid = true;
    nd.SlabMin = slabMin;
    nd.SlabMax = slabMax;
    return nd;
}

void BuildJhBspBoxBrush(SmfAuthoringBsp& outBsp, const SmfVec3& a, const SmfVec3& b, const std::string& frontTexture,
    const std::string& backTexture) {
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, a, b);
    outBsp.Nodes.clear();
    outBsp.Nodes.reserve(6);
    outBsp.RootIndex = 0;
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{1.f, 0.f, 0.f}, hi.x, lo, hi, frontTexture, backTexture));
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{-1.f, 0.f, 0.f}, -lo.x, lo, hi, frontTexture, backTexture));
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{0.f, 1.f, 0.f}, hi.y, lo, hi, frontTexture, backTexture));
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{0.f, -1.f, 0.f}, -lo.y, lo, hi, frontTexture, backTexture));
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{0.f, 0.f, 1.f}, hi.z, lo, hi, frontTexture, backTexture));
    outBsp.Nodes.push_back(MakeJhBspBoxPlaneNode(SmfVec3{0.f, 0.f, -1.f}, -lo.z, lo, hi, frontTexture, backTexture));
    for (size_t i = 0; i + 1 < outBsp.Nodes.size(); ++i) {
        outBsp.Nodes[i].FrontChild = static_cast<int32_t>(i + 1);
    }
}

bool JhBspIsCanonicalBoxBrush(const SmfAuthoringBsp& b) {
    if (b.Nodes.size() != 6 || b.RootIndex != 0) {
        return false;
    }
    const SmfVec3 expectedNormals[6] = {
        {1.f, 0.f, 0.f},
        {-1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, -1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, -1.f},
    };
    for (size_t i = 0; i < b.Nodes.size(); ++i) {
        const SmfVec3 n = NormalizeSmfVec3(b.Nodes[i].PlaneNormal);
        const SmfVec3 e = expectedNormals[i];
        if (std::fabs(n.x - e.x) > 1e-3f || std::fabs(n.y - e.y) > 1e-3f || std::fabs(n.z - e.z) > 1e-3f) {
            return false;
        }
        const int32_t expectedChild = (i + 1 < b.Nodes.size()) ? static_cast<int32_t>(i + 1) : -1;
        if (b.Nodes[i].FrontChild != expectedChild || b.Nodes[i].BackChild != -1) {
            return false;
        }
    }
    return true;
}

void SyncJhBspBoxBrushFromAabb(SmfAuthoringBsp& b, const SmfVec3& a, const SmfVec3& c) {
    if (b.Nodes.size() != 6) {
        return;
    }
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, a, c);
    const SmfVec3 normals[6] = {
        {1.f, 0.f, 0.f},
        {-1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, -1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, -1.f},
    };
    const float planeD[6] = {hi.x, -lo.x, hi.y, -lo.y, hi.z, -lo.z};
    b.RootIndex = 0;
    for (size_t i = 0; i < b.Nodes.size(); ++i) {
        SmfAuthoringBspNode& nd = b.Nodes[i];
        nd.PlaneNormal = normals[i];
        nd.PlaneD = planeD[i];
        nd.FrontChild = (i + 1 < b.Nodes.size()) ? static_cast<int32_t>(i + 1) : -1;
        nd.BackChild = -1;
        nd.SlabValid = true;
        nd.SlabMin = lo;
        nd.SlabMax = hi;
    }
}

const char* JhBspFaceLabel(const SmfAuthoringBspNode& nd) {
    const SmfVec3 n = NormalizeSmfVec3(nd.PlaneNormal);
    const float ax = std::fabs(n.x);
    const float ay = std::fabs(n.y);
    const float az = std::fabs(n.z);
    if (ax >= ay && ax >= az) {
        return n.x >= 0.f ? "+X face" : "-X face";
    }
    if (ay >= ax && ay >= az) {
        return n.y >= 0.f ? "+Y face" : "-Y face";
    }
    return n.z >= 0.f ? "+Z face" : "-Z face";
}

SmfVec3 JhAabbCorner(const SmfVec3& a, const SmfVec3& b, int cornerIndex) {
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, a, b);
    return SmfVec3{
        (cornerIndex & 1) ? hi.x : lo.x,
        (cornerIndex & 2) ? hi.y : lo.y,
        (cornerIndex & 4) ? hi.z : lo.z,
    };
}

void JhSetAabbCorner(SmfVec3& ioMin, SmfVec3& ioMax, int cornerIndex, const SmfVec3& p) {
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, ioMin, ioMax);
    if (cornerIndex & 1) {
        hi.x = p.x;
    } else {
        lo.x = p.x;
    }
    if (cornerIndex & 2) {
        hi.y = p.y;
    } else {
        lo.y = p.y;
    }
    if (cornerIndex & 4) {
        hi.z = p.z;
    } else {
        lo.z = p.z;
    }
    SmfAabbCanonical(ioMin, ioMax, lo, hi);
}

int FindBspParentIndex(const SmfAuthoringBsp& b, int childIdx) {
    if (childIdx < 0) {
        return -1;
    }
    for (size_t i = 0; i < b.Nodes.size(); ++i) {
        const auto& n = b.Nodes[i];
        if (n.FrontChild == childIdx || n.BackChild == childIdx) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int JhBspSubtreeHeight(const SmfAuthoringBsp& b, int idx, int guard) {
    if (guard <= 0 || idx < 0 || idx >= static_cast<int>(b.Nodes.size())) {
        return 0;
    }
    const auto& n = b.Nodes[static_cast<size_t>(idx)];
    const int hF = JhBspSubtreeHeight(b, n.FrontChild, guard - 1);
    const int hB = JhBspSubtreeHeight(b, n.BackChild, guard - 1);
    return 1 + std::max(hF, hB);
}

static bool JhBspDfsDirectedCycle(const SmfAuthoringBsp& b, int idx, std::vector<uint8_t>& col) {
    if (idx < 0) {
        return false;
    }
    if (idx >= static_cast<int>(b.Nodes.size())) {
        return false;
    }
    const size_t ui = static_cast<size_t>(idx);
    if (col[ui] == 1) {
        return true;
    }
    if (col[ui] == 2) {
        return false;
    }
    col[ui] = 1;
    const auto& n = b.Nodes[ui];
    if (JhBspDfsDirectedCycle(b, n.FrontChild, col)) {
        return true;
    }
    if (JhBspDfsDirectedCycle(b, n.BackChild, col)) {
        return true;
    }
    col[ui] = 2;
    return false;
}

bool JhBspGraphContainsDirectedCycle(const SmfAuthoringBsp& b) {
    const size_t n = b.Nodes.size();
    if (n == 0) {
        return false;
    }
    std::vector<uint8_t> col(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (col[i] != 0) {
            continue;
        }
        if (JhBspDfsDirectedCycle(b, static_cast<int>(i), col)) {
            return true;
        }
    }
    return false;
}

std::string JhExportBspNodeText(const SmfAuthoringBspNode& node, int nodeIndex) {
    char line[768];
    std::string out;
    out += "# Jackhammer BSP node export (Solstice .smf v1, spatial + optional BPEX)\n";
    std::snprintf(line, sizeof(line), "nodeIndex: %d\n", nodeIndex);
    out += line;
    std::snprintf(line, sizeof(line), "planeNormal: %.9g %.9g %.9g\n", static_cast<double>(node.PlaneNormal.x),
        static_cast<double>(node.PlaneNormal.y), static_cast<double>(node.PlaneNormal.z));
    out += line;
    std::snprintf(line, sizeof(line), "planeD: %.9g\n", static_cast<double>(node.PlaneD));
    out += line;
    std::snprintf(line, sizeof(line), "frontChild: %d\nbackChild: %d\n", static_cast<int>(node.FrontChild),
        static_cast<int>(node.BackChild));
    out += line;
    if (node.LeafId == 0xFFFFFFFFu) {
        out += "leafId: none (0xFFFFFFFF)\n";
    } else {
        std::snprintf(line, sizeof(line), "leafId: %u\n", node.LeafId);
        out += line;
    }
    out += "frontTexturePath: ";
    out += node.FrontTexturePath;
    out += "\nbackTexturePath: ";
    out += node.BackTexturePath;
    out += "\n";
    out += node.SlabValid ? "slabValid: true\n" : "slabValid: false\n";
    std::snprintf(line, sizeof(line), "slabMin: %.9g %.9g %.9g\n", static_cast<double>(node.SlabMin.x),
        static_cast<double>(node.SlabMin.y), static_cast<double>(node.SlabMin.z));
    out += line;
    std::snprintf(line, sizeof(line), "slabMax: %.9g %.9g %.9g\n", static_cast<double>(node.SlabMax.x),
        static_cast<double>(node.SlabMax.y), static_cast<double>(node.SlabMax.z));
    out += line;
    const SmfVec3 nraw = node.PlaneNormal;
    std::snprintf(line, sizeof(line), "planeAnchorPoint (n * D): %.9g %.9g %.9g\n", static_cast<double>(nraw.x * node.PlaneD),
        static_cast<double>(nraw.y * node.PlaneD), static_cast<double>(nraw.z * node.PlaneD));
    out += line;
    return out;
}

bool SmfAabbBooleanSubtractLargestPiece(SmfVec3& outMin, SmfVec3& outMax, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1) {
    constexpr float eps = 1e-3f;
    SmfVec3 amin, amax, bmin, bmax;
    SmfAabbCanonical(amin, amax, a0, a1);
    SmfAabbCanonical(bmin, bmax, b0, b1);
    SmfVec3 capMin, capMax;
    if (!SmfAabbIntersect(capMin, capMax, amin, amax, bmin, bmax)) {
        outMin = amin;
        outMax = amax;
        return true;
    }
    struct Cand {
        SmfVec3 mn;
        SmfVec3 mx;
    };
    Cand cands[6]{};
    int n = 0;
    auto addCand = [&](const SmfVec3& mn, const SmfVec3& mx) {
        if (mn.x <= mx.x - eps && mn.y <= mx.y - eps && mn.z <= mx.z - eps) {
            cands[n].mn = mn;
            cands[n].mx = mx;
            ++n;
        }
    };
    if (amin.x < bmin.x - eps) {
        addCand({amin.x, amin.y, amin.z}, {std::min(amax.x, bmin.x - eps), amax.y, amax.z});
    }
    if (bmax.x + eps < amax.x) {
        addCand({std::max(amin.x, bmax.x + eps), amin.y, amin.z}, {amax.x, amax.y, amax.z});
    }
    const float ix0 = std::max(amin.x, bmin.x);
    const float ix1 = std::min(amax.x, bmax.x);
    if (ix0 <= ix1 - eps) {
        if (amin.y < bmin.y - eps) {
            addCand({ix0, amin.y, amin.z}, {ix1, std::min(amax.y, bmin.y - eps), amax.z});
        }
        if (bmax.y + eps < amax.y) {
            addCand({ix0, std::max(amin.y, bmax.y + eps), amin.z}, {ix1, amax.y, amax.z});
        }
        const float iy0 = std::max(amin.y, bmin.y);
        const float iy1 = std::min(amax.y, bmax.y);
        if (iy0 <= iy1 - eps) {
            if (amin.z < bmin.z - eps) {
                addCand({ix0, iy0, amin.z}, {ix1, iy1, std::min(amax.z, bmin.z - eps)});
            }
            if (bmax.z + eps < amax.z) {
                addCand({ix0, iy0, std::max(amin.z, bmax.z + eps)}, {ix1, iy1, amax.z});
            }
        }
    }
    float bestV = -1.f;
    int bestI = -1;
    for (int i = 0; i < n; ++i) {
        const float v = SmfAabbVolume(cands[i].mn, cands[i].mx);
        if (v > bestV) {
            bestV = v;
            bestI = i;
        }
    }
    if (bestI < 0 || bestV <= 0.f) {
        return false;
    }
    outMin = cands[bestI].mn;
    outMax = cands[bestI].mx;
    return true;
}

void SmfAabbBooleanSubtractAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0,
    const SmfVec3& a1, const SmfVec3& b0, const SmfVec3& b1) {
    outPieces.clear();
    constexpr float eps = 1e-3f;
    SmfVec3 amin, amax, bmin, bmax;
    SmfAabbCanonical(amin, amax, a0, a1);
    SmfAabbCanonical(bmin, bmax, b0, b1);
    SmfVec3 capMin, capMax;
    if (!SmfAabbIntersect(capMin, capMax, amin, amax, bmin, bmax)) {
        outPieces.push_back({amin, amax});
        return;
    }
    auto addCand = [&](const SmfVec3& mn, const SmfVec3& mx) {
        if (mn.x <= mx.x - eps && mn.y <= mx.y - eps && mn.z <= mx.z - eps) {
            outPieces.push_back({mn, mx});
        }
    };
    if (amin.x < bmin.x - eps) {
        addCand({amin.x, amin.y, amin.z}, {std::min(amax.x, bmin.x - eps), amax.y, amax.z});
    }
    if (bmax.x + eps < amax.x) {
        addCand({std::max(amin.x, bmax.x + eps), amin.y, amin.z}, {amax.x, amax.y, amax.z});
    }
    const float ix0 = std::max(amin.x, bmin.x);
    const float ix1 = std::min(amax.x, bmax.x);
    if (ix0 <= ix1 - eps) {
        if (amin.y < bmin.y - eps) {
            addCand({ix0, amin.y, amin.z}, {ix1, std::min(amax.y, bmin.y - eps), amax.z});
        }
        if (bmax.y + eps < amax.y) {
            addCand({ix0, std::max(amin.y, bmax.y + eps), amin.z}, {ix1, amax.y, amax.z});
        }
        const float iy0 = std::max(amin.y, bmin.y);
        const float iy1 = std::min(amax.y, bmax.y);
        if (iy0 <= iy1 - eps) {
            if (amin.z < bmin.z - eps) {
                addCand({ix0, iy0, amin.z}, {ix1, iy1, std::min(amax.z, bmin.z - eps)});
            }
            if (bmax.z + eps < amax.z) {
                addCand({ix0, iy0, std::max(amin.z, bmax.z + eps)}, {ix1, iy1, amax.z});
            }
        }
    }
}

void SmfAabbBooleanUnionAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0,
    const SmfVec3& a1, const SmfVec3& b0, const SmfVec3& b1) {
    SmfAabbBooleanSubtractAllPieces(outPieces, a0, a1, b0, b1);
    SmfVec3 blo, bhi;
    SmfAabbCanonical(blo, bhi, b0, b1);
    outPieces.push_back({blo, bhi});
}

void SmfAabbBooleanXorAllPieces(std::vector<std::pair<SmfVec3, SmfVec3>>& outPieces, const SmfVec3& a0, const SmfVec3& a1,
    const SmfVec3& b0, const SmfVec3& b1) {
    outPieces.clear();
    std::vector<std::pair<SmfVec3, SmfVec3>> aNotB, bNotA;
    SmfAabbBooleanSubtractAllPieces(aNotB, a0, a1, b0, b1);
    SmfAabbBooleanSubtractAllPieces(bNotA, b0, b1, a0, a1);
    outPieces.reserve(aNotB.size() + bNotA.size());
    outPieces.insert(outPieces.end(), aNotB.begin(), aNotB.end());
    outPieces.insert(outPieces.end(), bNotA.begin(), bNotA.end());
}

void JhSyncAxisAlignedPlaneDFromSlab(SmfAuthoringBspNode& nd) {
    if (!nd.SlabValid) {
        return;
    }
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, nd.SlabMin, nd.SlabMax);
    const SmfVec3 n = NormalizeSmfVec3(nd.PlaneNormal);
    auto ax = [&](float c) { return std::fabs(c) > 0.92f; };
    if (ax(n.x)) {
        nd.PlaneD = n.x >= 0.f ? hi.x : -lo.x;
        return;
    }
    if (ax(n.y)) {
        nd.PlaneD = n.y >= 0.f ? hi.y : -lo.y;
        return;
    }
    if (ax(n.z)) {
        nd.PlaneD = n.z >= 0.f ? hi.z : -lo.z;
    }
}

bool JhBspSlabFaceWorldSize(const SmfAuthoringBspNode& nd, float& outWidth, float& outHeight) {
    if (!nd.SlabValid) {
        return false;
    }
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, nd.SlabMin, nd.SlabMax);
    const SmfVec3 n = NormalizeSmfVec3(nd.PlaneNormal);
    const float dx = hi.x - lo.x;
    const float dy = hi.y - lo.y;
    const float dz = hi.z - lo.z;
    auto ax = [&](float c) { return std::fabs(c) > 0.92f; };
    if (ax(n.x)) {
        outWidth = dz;
        outHeight = dy;
        return true;
    }
    if (ax(n.y)) {
        outWidth = dx;
        outHeight = dz;
        return true;
    }
    if (ax(n.z)) {
        outWidth = dx;
        outHeight = dy;
        return true;
    }
    return false;
}

bool PushSlabFace(SmfVec3& ioMin, SmfVec3& ioMax, int faceIndex, float delta) {
    SmfVec3 lo, hi;
    SmfAabbCanonical(lo, hi, ioMin, ioMax);
    const float d = delta;
    switch (faceIndex) {
    case 0:
        hi.x += d;
        break;
    case 1:
        lo.x -= d;
        break;
    case 2:
        hi.y += d;
        break;
    case 3:
        lo.y -= d;
        break;
    case 4:
        hi.z += d;
        break;
    case 5:
        lo.z -= d;
        break;
    default:
        return false;
    }
    if (lo.x >= hi.x - 1e-4f || lo.y >= hi.y - 1e-4f || lo.z >= hi.z - 1e-4f) {
        return false;
    }
    ioMin = lo;
    ioMax = hi;
    return true;
}

bool PushCanonicalBoxBrushFace(SmfAuthoringBsp& b, int faceIndex, float delta) {
    if (!JhBspIsCanonicalBoxBrush(b) || b.Nodes.empty()) {
        return false;
    }
    const float d = delta;
    SmfVec3 lo = b.Nodes[0].SlabMin;
    SmfVec3 hi = b.Nodes[0].SlabMax;
    SmfAabbCanonical(lo, hi, lo, hi);
    switch (faceIndex) {
    case 0:
        hi.x += d;
        break;
    case 1:
        lo.x -= d;
        break;
    case 2:
        hi.y += d;
        break;
    case 3:
        lo.y -= d;
        break;
    case 4:
        hi.z += d;
        break;
    case 5:
        lo.z -= d;
        break;
    default:
        return false;
    }
    if (lo.x >= hi.x - 1e-4f || lo.y >= hi.y - 1e-4f || lo.z >= hi.z - 1e-4f) {
        return false;
    }
    SyncJhBspBoxBrushFromAabb(b, lo, hi);
    return true;
}

SmfVec3 SmfDirectionUnit(const SmfVec3& d) {
    return NormalizeSmfVec3(d);
}

} // namespace Jackhammer::Spatial
