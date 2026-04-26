#include "JackhammerMeshOps.hxx"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Jackhammer::MeshOps {

namespace {

SmfVec3 Vec3Sub(const SmfVec3& a, const SmfVec3& b) {
    return SmfVec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

SmfVec3 Vec3Add(const SmfVec3& a, const SmfVec3& b) {
    return SmfVec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

SmfVec3 Vec3Scale(const SmfVec3& a, float s) {
    return SmfVec3{a.x * s, a.y * s, a.z * s};
}

float Vec3LenSq(const SmfVec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

SmfVec3 Vec3Cross(const SmfVec3& a, const SmfVec3& b) {
    return SmfVec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float Vec3Dot(const SmfVec3& a, const SmfVec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

void AddUndirectedEdge(std::map<std::pair<uint32_t, uint32_t>, int>& edgeCount, uint32_t u, uint32_t v) {
    if (u == v) {
        return;
    }
    const uint32_t a = u < v ? u : v;
    const uint32_t b = u < v ? v : u;
    edgeCount[{a, b}] += 1;
}

bool IsBoundaryEdge(const std::map<std::pair<uint32_t, uint32_t>, int>& edgeCount, uint32_t u, uint32_t v) {
    const uint32_t a = u < v ? u : v;
    const uint32_t b = u < v ? v : u;
    const auto it = edgeCount.find({a, b});
    return it != edgeCount.end() && it->second == 1;
}

void BuildEdgeIncidence(const JhTriangleMesh& m, std::map<std::pair<uint32_t, uint32_t>, int>& edgeCount) {
    edgeCount.clear();
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        AddUndirectedEdge(edgeCount, i0, i1);
        AddUndirectedEdge(edgeCount, i1, i2);
        AddUndirectedEdge(edgeCount, i2, i0);
    }
}

} // namespace

bool IsValidTriangleMesh(const JhTriangleMesh& m) {
    if (m.indices.size() % 3 != 0) {
        return false;
    }
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    if (n == 0) {
        return m.indices.empty();
    }
    for (uint32_t ix : m.indices) {
        if (ix >= n) {
            return false;
        }
    }
    if (!m.normals.empty() && m.normals.size() != m.positions.size()) {
        return false;
    }
    if (!m.uvs.empty() && m.uvs.size() != m.positions.size()) {
        return false;
    }
    return true;
}

void MeshCounts(const JhTriangleMesh& m, std::uint32_t& outVerts, std::uint32_t& outTris) {
    outVerts = static_cast<std::uint32_t>(m.positions.size());
    outTris = static_cast<std::uint32_t>(m.indices.size() / 3);
}

void MakeUnitCube(JhTriangleMesh& out) {
    out.positions = {
        {-1.f, -1.f, -1.f},
        {1.f, -1.f, -1.f},
        {1.f, 1.f, -1.f},
        {-1.f, 1.f, -1.f},
        {-1.f, -1.f, 1.f},
        {1.f, -1.f, 1.f},
        {1.f, 1.f, 1.f},
        {-1.f, 1.f, 1.f},
    };
    // CCW when viewed from outside for each face.
    out.indices = {
        0, 1, 2, 0, 2, 3, // -Z
        4, 6, 5, 4, 7, 6, // +Z
        0, 4, 5, 0, 5, 1, // -Y
        3, 2, 6, 3, 6, 7, // +Y
        0, 3, 7, 0, 7, 4, // -X
        1, 5, 6, 1, 6, 2  // +X
    };
    out.normals.clear();
    out.uvs.clear();
}

void MakeTwoStackedSquaresDemo(JhTriangleMesh& out) {
    // Bottom quad Y=0: (0,0,0)-(1,0,0)-(1,0,1)-(0,0,1); top Y=1 same XZ.
    out.positions = {
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {0.f, 1.f, 0.f},
        {1.f, 1.f, 0.f},
        {1.f, 1.f, 1.f},
        {0.f, 1.f, 1.f},
    };
    out.indices = {
        0, 1, 2, 0, 2, 3, // bottom
        4, 6, 5, 4, 7, 6  // top
    };
    out.normals.clear();
    out.uvs.clear();
}

/// Returns **old vertex index → new vertex index** after compaction (size = vertex count before merge).
/// Dropped vertex maps to the same new slot as ``keep``.
static std::vector<uint32_t> MergeVertexPairWithRemap(JhTriangleMesh& m, uint32_t keep, uint32_t drop) {
    std::vector<uint32_t> empty;
    if (keep == drop) {
        return empty;
    }
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    if (n == 0 || keep >= n || drop >= n) {
        return empty;
    }
    for (uint32_t& ix : m.indices) {
        if (ix == drop) {
            ix = keep;
        }
    }
    std::vector<uint32_t> o2n(n);
    uint32_t w = 0;
    uint32_t survivorNew = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (i == drop) {
            continue;
        }
        if (i == keep) {
            survivorNew = w;
        }
        o2n[i] = w++;
    }
    for (uint32_t i = 0; i < n; ++i) {
        if (i == drop) {
            o2n[i] = survivorNew;
        }
    }
    {
        std::vector<SmfVec3> newPos;
        newPos.reserve(w);
        for (uint32_t i = 0; i < n; ++i) {
            if (i != drop) {
                newPos.push_back(m.positions[i]);
            }
        }
        m.positions = std::move(newPos);
    }
    if (m.normals.size() == static_cast<size_t>(n)) {
        std::vector<SmfVec3> nn;
        nn.reserve(w);
        for (uint32_t i = 0; i < n; ++i) {
            if (i != drop) {
                nn.push_back(m.normals[i]);
            }
        }
        m.normals = std::move(nn);
    }
    if (m.uvs.size() == static_cast<size_t>(n)) {
        std::vector<SmfVec2> nu;
        nu.reserve(w);
        for (uint32_t i = 0; i < n; ++i) {
            if (i != drop) {
                nu.push_back(m.uvs[i]);
            }
        }
        m.uvs = std::move(nu);
    }
    for (uint32_t& ix : m.indices) {
        ix = o2n[ix];
    }
    return o2n;
}

void MergeVertexPair(JhTriangleMesh& m, uint32_t keep, uint32_t drop) {
    static_cast<void>(MergeVertexPairWithRemap(m, keep, drop));
}

bool SewBorderChains(JhTriangleMesh& m, const std::vector<uint32_t>& chainA, const std::vector<uint32_t>& chainB,
    JhSewPositionPolicy policy, std::string* errOut) {
    if (chainA.size() != chainB.size()) {
        if (errOut) {
            *errOut = "Sew: chains must have the same length.";
        }
        return false;
    }
    if (chainA.size() < 2) {
        if (errOut) {
            *errOut = "Sew: each chain needs at least two vertices.";
        }
        return false;
    }
    if (!IsValidTriangleMesh(m)) {
        if (errOut) {
            *errOut = "Sew: mesh is not a valid indexed triangle list.";
        }
        return false;
    }
    std::map<std::pair<uint32_t, uint32_t>, int> edgeCount;
    BuildEdgeIncidence(m, edgeCount);

    auto validateChain = [&](const std::vector<uint32_t>& ch, const char* label) -> bool {
        const uint32_t nv = static_cast<uint32_t>(m.positions.size());
        for (uint32_t v : ch) {
            if (v >= nv) {
                if (errOut) {
                    *errOut = std::string("Sew: ") + label + " index out of range.";
                }
                return false;
            }
        }
        for (size_t i = 0; i + 1 < ch.size(); ++i) {
            if (!IsBoundaryEdge(edgeCount, ch[i], ch[i + 1])) {
                if (errOut) {
                    *errOut = std::string("Sew: ") + label + " step is not a single boundary edge.";
                }
                return false;
            }
        }
        return true;
    };
    if (!validateChain(chainA, "chain A") || !validateChain(chainB, "chain B")) {
        return false;
    }

    std::vector<uint32_t> curA = chainA;
    std::vector<uint32_t> curB = chainB;
    const size_t pairs = chainA.size();
    for (size_t i = 0; i < pairs; ++i) {
        uint32_t ia = curA[i];
        uint32_t ib = curB[i];
        if (ia == ib) {
            continue;
        }
        SmfVec3 pa = m.positions[ia];
        SmfVec3 pb = m.positions[ib];
        SmfVec3 chosen = pa;
        if (policy == JhSewPositionPolicy::KeepB) {
            chosen = pb;
        } else if (policy == JhSewPositionPolicy::Midpoint) {
            chosen = Vec3Scale(Vec3Add(pa, pb), 0.5f);
        }
        uint32_t survivor = ia;
        uint32_t victim = ib;
        if (policy == JhSewPositionPolicy::KeepB) {
            survivor = ib;
            victim = ia;
        } else if (policy == JhSewPositionPolicy::KeepA) {
            survivor = ia;
            victim = ib;
        } else { // Midpoint: collapse to min index keeps stable ordering
            survivor = ia < ib ? ia : ib;
            victim = ia < ib ? ib : ia;
        }
        m.positions[survivor] = chosen;
        if (survivor != victim) {
            const std::vector<uint32_t> o2n = MergeVertexPairWithRemap(m, survivor, victim);
            if (!o2n.empty()) {
                for (uint32_t& x : curA) {
                    if (x < o2n.size()) {
                        x = o2n[x];
                    }
                }
                for (uint32_t& x : curB) {
                    if (x < o2n.size()) {
                        x = o2n[x];
                    }
                }
            }
        }
        if (!IsValidTriangleMesh(m)) {
            if (errOut) {
                *errOut = "Sew: mesh became invalid after merge.";
            }
            return false;
        }
        BuildEdgeIncidence(m, edgeCount);
    }
    return true;
}

std::uint32_t WeldVerticesByDistance(JhTriangleMesh& m, float epsilon) {
    if (epsilon <= 0.f || m.positions.empty()) {
        return 0;
    }
    const float epsSq = epsilon * epsilon;
    std::uint32_t rounds = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        const uint32_t n = static_cast<uint32_t>(m.positions.size());
        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t j = i + 1; j < n; ++j) {
                const float d2 = Vec3LenSq(Vec3Sub(m.positions[i], m.positions[j]));
                if (d2 <= epsSq) {
                    MergeVertexPair(m, i, j);
                    changed = true;
                    ++rounds;
                    goto next_round;
                }
            }
        }
    next_round:;
    }
    return rounds;
}

void FlipTriangleWinding(JhTriangleMesh& m) {
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        std::swap(m.indices[t * 3 + 1], m.indices[t * 3 + 2]);
    }
}

void RemoveDegenerateTriangles(JhTriangleMesh& m) {
    std::vector<uint32_t> keep;
    keep.reserve(m.indices.size());
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t a = m.indices[t * 3];
        const uint32_t b = m.indices[t * 3 + 1];
        const uint32_t c = m.indices[t * 3 + 2];
        if (a == b || b == c || c == a) {
            continue;
        }
        const SmfVec3& p0 = m.positions[a];
        const SmfVec3& p1 = m.positions[b];
        const SmfVec3& p2 = m.positions[c];
        const SmfVec3 e0 = Vec3Sub(p1, p0);
        const SmfVec3 e1 = Vec3Sub(p2, p0);
        const SmfVec3 cr = Vec3Cross(e0, e1);
        if (Vec3LenSq(cr) <= 1.0e-20f) {
            continue;
        }
        keep.push_back(a);
        keep.push_back(b);
        keep.push_back(c);
    }
    m.indices = std::move(keep);
}

void RecalculateNormals(JhTriangleMesh& m) {
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    m.normals.assign(static_cast<size_t>(n), SmfVec3{0.f, 0.f, 0.f});
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        const SmfVec3& p0 = m.positions[i0];
        const SmfVec3& p1 = m.positions[i1];
        const SmfVec3& p2 = m.positions[i2];
        const SmfVec3 e0 = Vec3Sub(p1, p0);
        const SmfVec3 e1 = Vec3Sub(p2, p0);
        SmfVec3 fn = Vec3Cross(e0, e1);
        m.normals[i0].x += fn.x;
        m.normals[i0].y += fn.y;
        m.normals[i0].z += fn.z;
        m.normals[i1].x += fn.x;
        m.normals[i1].y += fn.y;
        m.normals[i1].z += fn.z;
        m.normals[i2].x += fn.x;
        m.normals[i2].y += fn.y;
        m.normals[i2].z += fn.z;
    }
    for (uint32_t i = 0; i < n; ++i) {
        SmfVec3& v = m.normals[i];
        const float len = std::sqrt(Vec3LenSq(v));
        if (len > 1.0e-20f) {
            v.x /= len;
            v.y /= len;
            v.z /= len;
        } else {
            v = SmfVec3{0.f, 1.f, 0.f};
        }
    }
}

void SubdivideTrianglesMidpoint(JhTriangleMesh& m) {
    if (m.indices.empty()) {
        return;
    }
    // edge (min,max) -> midpoint vertex index
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> midOf;
    std::vector<SmfVec3> newPos = m.positions;
    std::vector<SmfVec3> newN;
    std::vector<SmfVec2> newUv;
    const bool hadN = m.normals.size() == m.positions.size();
    const bool hadUv = m.uvs.size() == m.positions.size();
    if (hadN) {
        newN = m.normals;
    }
    if (hadUv) {
        newUv = m.uvs;
    }

    auto getMid = [&](uint32_t a, uint32_t b) -> uint32_t {
        const uint32_t u = a < b ? a : b;
        const uint32_t v = a < b ? b : a;
        const auto key = std::make_pair(u, v);
        const auto it = midOf.find(key);
        if (it != midOf.end()) {
            return it->second;
        }
        const SmfVec3 pa = newPos[u];
        const SmfVec3 pb = newPos[v];
        const SmfVec3 mid = Vec3Scale(Vec3Add(pa, pb), 0.5f);
        const uint32_t idx = static_cast<uint32_t>(newPos.size());
        newPos.push_back(mid);
        if (hadN) {
            const SmfVec3 na = newN[u];
            const SmfVec3 nb = newN[v];
            newN.push_back(Vec3Scale(Vec3Add(na, nb), 0.5f));
        }
        if (hadUv) {
            const SmfVec2 ta = newUv[u];
            const SmfVec2 tb = newUv[v];
            newUv.push_back(SmfVec2{(ta.x + tb.x) * 0.5f, (ta.y + tb.y) * 0.5f});
        }
        midOf[key] = idx;
        return idx;
    };

    std::vector<uint32_t> outIx;
    outIx.reserve(m.indices.size() * 4);
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        const uint32_t m01 = getMid(i0, i1);
        const uint32_t m12 = getMid(i1, i2);
        const uint32_t m20 = getMid(i2, i0);
        outIx.insert(outIx.end(), {i0, m01, m20, i1, m12, m01, i2, m20, m12, m01, m12, m20});
    }
    m.positions = std::move(newPos);
    m.indices = std::move(outIx);
    if (hadN) {
        m.normals = std::move(newN);
        RecalculateNormals(m);
    } else {
        m.normals.clear();
    }
    if (hadUv) {
        m.uvs = std::move(newUv);
    }
}

void ScaleMeshAboutCentroid(JhTriangleMesh& m, float uniformScale) {
    if (m.positions.empty() || std::abs(uniformScale - 1.f) < 1.0e-8f) {
        return;
    }
    SmfVec3 c{0.f, 0.f, 0.f};
    for (const SmfVec3& p : m.positions) {
        c = Vec3Add(c, p);
    }
    const float inv = 1.f / static_cast<float>(m.positions.size());
    c = Vec3Scale(c, inv);
    for (SmfVec3& p : m.positions) {
        SmfVec3 d = Vec3Sub(p, c);
        d = Vec3Scale(d, uniformScale);
        p = Vec3Add(c, d);
    }
}

void LaplacianSmoothUniform(JhTriangleMesh& m, float factor, int iterations) {
    if (m.positions.empty() || iterations <= 0) {
        return;
    }
    const float f = std::clamp(factor, 0.f, 1.f);
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    std::vector<std::set<uint32_t>> adj(static_cast<size_t>(n));
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        adj[i0].insert(i1);
        adj[i0].insert(i2);
        adj[i1].insert(i0);
        adj[i1].insert(i2);
        adj[i2].insert(i0);
        adj[i2].insert(i1);
    }
    std::vector<SmfVec3> next = m.positions;
    for (int it = 0; it < iterations; ++it) {
        for (uint32_t i = 0; i < n; ++i) {
            if (adj[i].empty()) {
                next[i] = m.positions[i];
                continue;
            }
            SmfVec3 sum{0.f, 0.f, 0.f};
            for (uint32_t j : adj[i]) {
                sum = Vec3Add(sum, m.positions[j]);
            }
            const float inv = 1.f / static_cast<float>(adj[i].size());
            sum = Vec3Scale(sum, inv);
            const SmfVec3& p = m.positions[i];
            next[i] = Vec3Add(Vec3Scale(p, 1.f - f), Vec3Scale(sum, f));
        }
        m.positions.swap(next);
    }
}

void MakeAxisAlignedBox(JhTriangleMesh& out, const SmfVec3& aabbMin, const SmfVec3& aabbMax) {
    out.positions = {
        {aabbMin.x, aabbMin.y, aabbMin.z},
        {aabbMax.x, aabbMin.y, aabbMin.z},
        {aabbMax.x, aabbMax.y, aabbMin.z},
        {aabbMin.x, aabbMax.y, aabbMin.z},
        {aabbMin.x, aabbMin.y, aabbMax.z},
        {aabbMax.x, aabbMin.y, aabbMax.z},
        {aabbMax.x, aabbMax.y, aabbMax.z},
        {aabbMin.x, aabbMax.y, aabbMax.z},
    };
    out.indices = {
        0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1, 3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2
    };
    out.normals.clear();
    out.uvs.clear();
}

void MakeCylinder(JhTriangleMesh& out, float radius, float height, int radialSegments, int heightSegments) {
    out.positions.clear();
    out.indices.clear();
    out.normals.clear();
    out.uvs.clear();
    const int rs = std::max(3, radialSegments);
    const int hs = std::max(1, heightSegments);
    const float r = std::max(1.0e-4f, radius);
    const float h = std::max(1.0e-4f, height);
    for (int j = 0; j <= hs; ++j) {
        const float y = h * (static_cast<float>(j) / static_cast<float>(hs));
        for (int i = 0; i < rs; ++i) {
            const float t = (static_cast<float>(i) / static_cast<float>(rs)) * 6.2831853f;
            out.positions.push_back(SmfVec3{std::cos(t) * r, y, std::sin(t) * r});
        }
    }
    const auto ringOff = [rs, hs](int j, int i) { return j * rs + (i % rs); };
    for (int j = 0; j < hs; ++j) {
        for (int i = 0; i < rs; ++i) {
            const int i1 = (i + 1) % rs;
            const uint32_t a = ringOff(j, i);
            const uint32_t b = ringOff(j, i1);
            const uint32_t c = ringOff(j + 1, i1);
            const uint32_t d = ringOff(j + 1, i);
            out.indices.push_back(a);
            out.indices.push_back(d);
            out.indices.push_back(b);
            out.indices.push_back(b);
            out.indices.push_back(d);
            out.indices.push_back(c);
        }
    }
    // Caps (tri fans from center)
    const uint32_t baseCenter = static_cast<uint32_t>(out.positions.size());
    out.positions.push_back(SmfVec3{0.f, 0.f, 0.f});
    const uint32_t topCenter = static_cast<uint32_t>(out.positions.size());
    out.positions.push_back(SmfVec3{0.f, h, 0.f});
    for (int i = 0; i < rs; ++i) {
        const int i1 = (i + 1) % rs;
        out.indices.push_back(baseCenter);
        out.indices.push_back(ringOff(0, i1));
        out.indices.push_back(ringOff(0, i));
    }
    for (int i = 0; i < rs; ++i) {
        const int i1 = (i + 1) % rs;
        out.indices.push_back(topCenter);
        out.indices.push_back(ringOff(hs, i));
        out.indices.push_back(ringOff(hs, i1));
    }
    out.uvs.assign(out.positions.size(), SmfVec2{0.f, 0.f});
    for (int j = 0; j <= hs; ++j) {
        for (int i = 0; i < rs; ++i) {
            const size_t k = static_cast<size_t>(j * rs + i);
            out.uvs[k] = SmfVec2{static_cast<float>(i) / static_cast<float>(rs), static_cast<float>(j) / static_cast<float>(hs)};
        }
    }
    for (int i = 0; i < rs; ++i) {
        const float t = (static_cast<float>(i) / static_cast<float>(rs)) * 6.2831853f;
        const float cu = 0.5f + 0.48f * std::cos(t);
        const float cv = 0.5f + 0.48f * std::sin(t);
        out.uvs[static_cast<size_t>(ringOff(0, i))] = SmfVec2{cu, cv * 0.15f};
        out.uvs[static_cast<size_t>(ringOff(hs, i))] = SmfVec2{cu, 0.85f + cv * 0.15f};
    }
    out.uvs[static_cast<size_t>(baseCenter)] = SmfVec2{0.5f, 0.02f};
    out.uvs[static_cast<size_t>(topCenter)] = SmfVec2{0.5f, 0.98f};
    RecalculateNormals(out);
}

void MakeSphere(JhTriangleMesh& out, float radius, int latSegments, int longSegments) {
    out.positions.clear();
    out.indices.clear();
    out.normals.clear();
    out.uvs.clear();
    const int la = std::max(2, latSegments);
    const int lo = std::max(3, longSegments);
    const float R = std::max(1.0e-4f, radius);
    for (int i = 0; i <= la; ++i) {
        const float v = static_cast<float>(i) / static_cast<float>(la);
        const float ph = 3.14159265f * (v - 0.5f);
        const float cph = std::cos(ph);
        const float sph = std::sin(ph);
        for (int j = 0; j < lo; ++j) {
            const float u = static_cast<float>(j) / static_cast<float>(lo);
            const float th = u * 6.2831853f;
            const float cth = std::cos(th);
            const float sth = std::sin(th);
            out.positions.push_back(SmfVec3{R * cph * cth, R * sph, R * cph * sth});
        }
    }
    for (int i = 0; i < la; ++i) {
        for (int j = 0; j < lo; ++j) {
            const int j1 = (j + 1) % lo;
            const uint32_t a = static_cast<uint32_t>(i * lo + j);
            const uint32_t b = static_cast<uint32_t>(i * lo + j1);
            const uint32_t c = static_cast<uint32_t>((i + 1) * lo + j1);
            const uint32_t d = static_cast<uint32_t>((i + 1) * lo + j);
            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(c);
            out.indices.push_back(a);
            out.indices.push_back(c);
            out.indices.push_back(d);
        }
    }
    out.uvs.assign(out.positions.size(), SmfVec2{0.f, 0.f});
    for (int i = 0; i <= la; ++i) {
        for (int j = 0; j < lo; ++j) {
            const size_t k = static_cast<size_t>(i * lo + j);
            out.uvs[k] = SmfVec2{static_cast<float>(j) / static_cast<float>(lo), static_cast<float>(i) / static_cast<float>(la)};
        }
    }
    for (uint32_t vi = 0; vi < static_cast<uint32_t>(out.positions.size()); ++vi) {
        const SmfVec3& p = out.positions[vi];
        const float invR = 1.0f / R;
        out.normals.push_back(SmfVec3{p.x * invR, p.y * invR, p.z * invR});
    }
}

void MakeTorus(JhTriangleMesh& out, float majorRadius, float minorRadius, int majorSegments, int minorSegments) {
    out.positions.clear();
    out.indices.clear();
    out.normals.clear();
    out.uvs.clear();
    const int ms = std::max(3, majorSegments);
    const int ns = std::max(3, minorSegments);
    const float R = std::max(1.0e-4f, majorRadius);
    const float r = std::max(1.0e-4f, minorRadius);
    for (int j = 0; j < ns; ++j) {
        const float v = static_cast<float>(j) / static_cast<float>(ns) * 6.2831853f;
        const float cv = std::cos(v);
        const float sv = std::sin(v);
        for (int i = 0; i < ms; ++i) {
            const float u = static_cast<float>(i) / static_cast<float>(ms) * 6.2831853f;
            const float cu = std::cos(u);
            const float su = std::sin(u);
            const float sc = (R + r * cv) * cu;
            const float sr = (R + r * cv) * su;
            out.positions.push_back(SmfVec3{sc, r * sv, sr});
        }
    }
    for (int j = 0; j < ns; ++j) {
        for (int i = 0; i < ms; ++i) {
            const int i1 = (i + 1) % ms;
            const int j1 = (j + 1) % ns;
            const uint32_t a = static_cast<uint32_t>(j * ms + i);
            const uint32_t b = static_cast<uint32_t>(j * ms + i1);
            const uint32_t c = static_cast<uint32_t>(j1 * ms + i1);
            const uint32_t d = static_cast<uint32_t>(j1 * ms + i);
            out.indices.push_back(a);
            out.indices.push_back(c);
            out.indices.push_back(b);
            out.indices.push_back(a);
            out.indices.push_back(d);
            out.indices.push_back(c);
        }
    }
    out.uvs.assign(out.positions.size(), SmfVec2{0.f, 0.f});
    for (int j = 0; j < ns; ++j) {
        for (int i = 0; i < ms; ++i) {
            out.uvs[static_cast<size_t>(j * ms + i)] =
                SmfVec2{static_cast<float>(i) / static_cast<float>(ms), static_cast<float>(j) / static_cast<float>(ns)};
        }
    }
    RecalculateNormals(out);
}

void MakeArch(JhTriangleMesh& out, float width, float height, float depth, int alongArcSegments, float curveDeg) {
    out.positions.clear();
    out.indices.clear();
    out.normals.clear();
    out.uvs.clear();
    const float W = std::max(1.0e-3f, width);
    const float d = std::max(0.f, depth);
    const float sUser = std::max(1.0e-3f, height);
    const float th = std::min(179.0f, std::max(5.0f, curveDeg)) * 0.0174532925f;
    const float R = (W * 0.5f) / std::max(1.0e-5f, std::sin(th * 0.5f));
    const float sNat = R * (1.0f - std::cos(th * 0.5f));
    const float yScale = sNat > 1.0e-5f ? (sUser / sNat) : 1.0f;
    const float Cx = W * 0.5f;
    const float Cy = -R * yScale * std::cos(th * 0.5f);
    const float RY = R * yScale;
    const float a0 = std::atan2(0.0f - Cy, 0.0f - Cx);
    const float a1 = std::atan2(0.0f - Cy, W - Cx);
    const int n = std::max(3, alongArcSegments);
    std::vector<SmfVec2> p2;
    p2.resize(static_cast<size_t>(n + 1));
    p2[0] = SmfVec2{0.f, 0.f};
    p2[static_cast<size_t>(n)] = SmfVec2{W, 0.f};
    for (int i = 1; i < n; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(n);
        const float ang = a0 + t * (a1 - a0);
        p2[static_cast<size_t>(i)] = SmfVec2{Cx + RY * std::cos(ang), Cy + RY * std::sin(ang)};
    }
    for (int i = 0; i <= n; ++i) {
        out.positions.push_back(SmfVec3{p2[static_cast<size_t>(i)].x, p2[static_cast<size_t>(i)].y, 0.f});
        out.positions.push_back(SmfVec3{p2[static_cast<size_t>(i)].x, p2[static_cast<size_t>(i)].y, d});
    }
    for (int i = 0; i < n; ++i) {
        const uint32_t t0 = i * 2u;
        const uint32_t t1 = t0 + 1u;
        const uint32_t u0 = t0 + 2u;
        const uint32_t u1 = t0 + 3u;
        out.indices.push_back(t0);
        out.indices.push_back(u0);
        out.indices.push_back(u1);
        out.indices.push_back(t0);
        out.indices.push_back(u1);
        out.indices.push_back(t1);
    }
    out.uvs.assign(out.positions.size(), SmfVec2{0.f, 0.f});
    for (int i = 0; i <= n; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(n);
        out.uvs[static_cast<size_t>(i * 2u)] = SmfVec2{u, 0.f};
        out.uvs[static_cast<size_t>(i * 2u + 1u)] = SmfVec2{u, 1.f};
    }
    RecalculateNormals(out);
}

void SetVertexPosition(JhTriangleMesh& m, std::uint32_t vertex, const SmfVec3& p) {
    if (vertex < m.positions.size()) {
        m.positions[vertex] = p;
    }
}

bool DeleteTriangle(JhTriangleMesh& m, std::uint32_t triIndex) {
    const size_t nt = m.indices.size() / 3;
    if (triIndex >= nt) {
        return false;
    }
    m.indices.erase(m.indices.begin() + static_cast<ptrdiff_t>(triIndex * 3),
        m.indices.begin() + static_cast<ptrdiff_t>(triIndex * 3 + 3));
    return true;
}

bool CollapseUndirectedEdge(JhTriangleMesh& m, std::uint32_t a, std::uint32_t b, std::string* errOut) {
    if (a == b) {
        if (errOut) {
            *errOut = "Collapse: need two distinct vertex indices.";
        }
        return false;
    }
    if (!IsValidTriangleMesh(m)) {
        if (errOut) {
            *errOut = "Collapse: invalid mesh.";
        }
        return false;
    }
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    if (a >= n || b >= n) {
        if (errOut) {
            *errOut = "Collapse: index out of range.";
        }
        return false;
    }
    bool has = false;
    const size_t nt = m.indices.size() / 3;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        if ((i0 == a && i1 == b) || (i0 == b && i1 == a) || (i1 == a && i2 == b) || (i1 == b && i2 == a)
            || (i2 == a && i0 == b) || (i2 == b && i0 == a)) {
            has = true;
            break;
        }
    }
    if (!has) {
        if (errOut) {
            *errOut = "Collapse: not a mesh edge (no triangle uses both indices).";
        }
        return false;
    }
    const SmfVec3 mid = Vec3Scale(Vec3Add(m.positions[a], m.positions[b]), 0.5f);
    const uint32_t keep = a < b ? a : b;
    const uint32_t drop = a < b ? b : a;
    m.positions[keep] = mid;
    MergeVertexPair(m, keep, drop);
    RemoveDegenerateTriangles(m);
    return true;
}

bool SplitUndirectedEdge(JhTriangleMesh& m, std::uint32_t a, std::uint32_t b, std::string* errOut) {
    if (a == b) {
        if (errOut) {
            *errOut = "Split: need two distinct vertex indices.";
        }
        return false;
    }
    if (!IsValidTriangleMesh(m) || m.positions.empty()) {
        if (errOut) {
            *errOut = "Split: invalid mesh.";
        }
        return false;
    }
    const uint32_t n = static_cast<uint32_t>(m.positions.size());
    if (a >= n || b >= n) {
        if (errOut) {
            *errOut = "Split: index out of range.";
        }
        return false;
    }
    const uint32_t u = std::min(a, b);
    const uint32_t v = std::max(a, b);
    const auto classifies = [&](uint32_t i0, uint32_t i1, uint32_t i2, uint32_t& w, bool& forwardUv) -> bool {
        if (i0 == u && i1 == v) {
            w = i2;
            forwardUv = true;
            return true;
        }
        if (i1 == u && i2 == v) {
            w = i0;
            forwardUv = true;
            return true;
        }
        if (i2 == u && i0 == v) {
            w = i1;
            forwardUv = true;
            return true;
        }
        if (i0 == v && i1 == u) {
            w = i2;
            forwardUv = false;
            return true;
        }
        if (i1 == v && i2 == u) {
            w = i0;
            forwardUv = false;
            return true;
        }
        if (i2 == v && i0 == u) {
            w = i1;
            forwardUv = false;
            return true;
        }
        return false;
    };
    const size_t nt = m.indices.size() / 3;
    bool haveEdge = false;
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        uint32_t w = 0;
        bool forwardUv = true;
        if (classifies(i0, i1, i2, w, forwardUv)) {
            if (w == u || w == v) {
                if (errOut) {
                    *errOut = "Split: degenerate triangle.";
                }
                return false;
            }
            haveEdge = true;
        }
    }
    if (!haveEdge) {
        if (errOut) {
            *errOut = "Split: not a mesh edge.";
        }
        return false;
    }
    const SmfVec3 midp = Vec3Scale(Vec3Add(m.positions[u], m.positions[v]), 0.5f);
    const bool hadN = m.normals.size() == m.positions.size();
    const bool hadU = m.uvs.size() == m.positions.size();
    const uint32_t mid = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back(midp);
    if (hadN) {
        m.normals.push_back(SmfVec3{0.f, 1.f, 0.f});
    }
    if (hadU) {
        m.uvs.push_back(SmfVec2{0.f, 0.f});
    }
    std::vector<uint32_t> newIx;
    newIx.reserve(m.indices.size() * 2);
    for (size_t t = 0; t < nt; ++t) {
        const uint32_t i0 = m.indices[t * 3];
        const uint32_t i1 = m.indices[t * 3 + 1];
        const uint32_t i2 = m.indices[t * 3 + 2];
        uint32_t w = 0;
        bool forwardUv = true;
        if (classifies(i0, i1, i2, w, forwardUv)) {
            if (forwardUv) {
                newIx.push_back(u);
                newIx.push_back(mid);
                newIx.push_back(w);
                newIx.push_back(mid);
                newIx.push_back(v);
                newIx.push_back(w);
            } else {
                newIx.push_back(v);
                newIx.push_back(mid);
                newIx.push_back(w);
                newIx.push_back(mid);
                newIx.push_back(u);
                newIx.push_back(w);
            }
        } else {
            newIx.push_back(i0);
            newIx.push_back(i1);
            newIx.push_back(i2);
        }
    }
    m.indices = std::move(newIx);
    if (hadN) {
        RecalculateNormals(m);
    }
    return true;
}

bool ComputeAxisAlignedBounds(const JhTriangleMesh& m, SmfVec3& outMin, SmfVec3& outMax) {
    if (m.positions.empty()) {
        return false;
    }
    outMin = m.positions.front();
    outMax = m.positions.front();
    for (const SmfVec3& p : m.positions) {
        outMin.x = (std::min)(outMin.x, p.x);
        outMin.y = (std::min)(outMin.y, p.y);
        outMin.z = (std::min)(outMin.z, p.z);
        outMax.x = (std::max)(outMax.x, p.x);
        outMax.y = (std::max)(outMax.y, p.y);
        outMax.z = (std::max)(outMax.z, p.z);
    }
    return true;
}

void MakeHeightfieldGrid(JhTriangleMesh& out, float originX, float originZ, float sizeX, float sizeZ, int cellCountX, int cellCountZ,
    float baseY) {
    out = {};
    cellCountX = (std::max)(1, cellCountX);
    cellCountZ = (std::max)(1, cellCountZ);
    if (sizeX <= 0.f || sizeZ <= 0.f) {
        return;
    }
    const int vx = cellCountX + 1;
    const int vz = cellCountZ + 1;
    out.positions.resize(static_cast<size_t>(vx * vz));
    const float stepX = sizeX / static_cast<float>(cellCountX);
    const float stepZ = sizeZ / static_cast<float>(cellCountZ);
    for (int j = 0; j < vz; ++j) {
        for (int i = 0; i < vx; ++i) {
            const size_t k = static_cast<size_t>(i + j * vx);
            out.positions[k] = SmfVec3{originX + static_cast<float>(i) * stepX, baseY, originZ + static_cast<float>(j) * stepZ};
        }
    }
    out.indices.reserve(static_cast<size_t>(cellCountX * cellCountZ * 6));
    for (int j = 0; j < cellCountZ; ++j) {
        for (int i = 0; i < cellCountX; ++i) {
            const uint32_t i0 = static_cast<uint32_t>(i + j * vx);
            const uint32_t i1 = i0 + 1u;
            const uint32_t i2 = static_cast<uint32_t>(i + (j + 1) * vx);
            const uint32_t i3 = i2 + 1u;
            out.indices.push_back(i0);
            out.indices.push_back(i2);
            out.indices.push_back(i1);
            out.indices.push_back(i1);
            out.indices.push_back(i2);
            out.indices.push_back(i3);
        }
    }
    RecalculateNormals(out);
    out.uvs.clear();
    out.uvs.resize(out.positions.size(), SmfVec2{0.f, 0.f});
    for (int j = 0; j < vz; ++j) {
        for (int i = 0; i < vx; ++i) {
            const size_t k = static_cast<size_t>(i + j * vx);
            const float u = static_cast<float>(i) / static_cast<float>(cellCountX);
            const float v = static_cast<float>(j) / static_cast<float>(cellCountZ);
            out.uvs[k] = SmfVec2{u, v};
        }
    }
}

void MakeHammerDisplacementPatch(
    JhTriangleMesh& out, float originX, float originZ, float sizeX, float sizeZ, float baseY, int power) {
    power = std::clamp(power, 1, 4);
    if (sizeX <= 0.f || sizeZ <= 0.f) {
        return;
    }
    const int segs = 1 << power;
    MakeHeightfieldGrid(out, originX, originZ, sizeX, sizeZ, segs, segs, baseY);
}

void SnapAllVerticesToGrid(JhTriangleMesh& m, float grid) {
    if (!(grid > 0.f) || m.positions.empty()) {
        return;
    }
    const float inv = 1.f / grid;
    for (SmfVec3& p : m.positions) {
        p.x = std::round(p.x * inv) * grid;
        p.y = std::round(p.y * inv) * grid;
        p.z = std::round(p.z * inv) * grid;
    }
    if (!m.normals.empty() && m.normals.size() == m.positions.size()) {
        RecalculateNormals(m);
    }
}

} // namespace Jackhammer::MeshOps
