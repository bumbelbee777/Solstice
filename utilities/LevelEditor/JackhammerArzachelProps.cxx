#include "JackhammerArzachelProps.hxx"

#include <Smf/SmfTypes.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <utility>
#include <vector>

namespace Jackhammer::ArzachelProps {

using Solstice::Smf::SmfVec2;
using Solstice::Smf::SmfVec3;
using Jackhammer::MeshOps::JhTriangleMesh;
using Jackhammer::MeshOps::RecalculateNormals;

static SmfVec3 JhNormalized(const SmfVec3& v) {
    const float s = 1.0f / std::max(1.0e-8f, std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
    return {v.x * s, v.y * s, v.z * s};
}

static void BuildPlaneYUp(JhTriangleMesh& m, float halfW, float halfD) {
    m.positions.clear();
    m.normals.clear();
    m.uvs.clear();
    m.indices.clear();
    const SmfVec3 n{0.f, 1.f, 0.f};
    m.positions = {
        SmfVec3{-halfW, 0, -halfD},
        SmfVec3{halfW, 0, -halfD},
        SmfVec3{halfW, 0, halfD},
        SmfVec3{-halfW, 0, halfD},
    };
    m.normals = {n, n, n, n};
    m.uvs = {SmfVec2{0, 0}, SmfVec2{1, 0}, SmfVec2{1, 1}, SmfVec2{0, 1}};
    m.indices = {0, 1, 2, 0, 2, 3};
}

static void AddFace(JhTriangleMesh& m, const SmfVec3& a, const SmfVec3& b, const SmfVec3& c) {
    const float ax = b.x - a.x, ay = b.y - a.y, az = b.z - a.z;
    const float bx = c.x - a.x, by = c.y - a.y, bz = c.z - a.z;
    SmfVec3 n{
        ay * bz - az * by,
        az * bx - ax * bz,
        ax * by - ay * bx,
    };
    n = JhNormalized(n);
    const uint32_t b0 = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back(a);
    m.positions.push_back(b);
    m.positions.push_back(c);
    m.normals.push_back(n);
    m.normals.push_back(n);
    m.normals.push_back(n);
    m.uvs.push_back(SmfVec2{0, 0});
    m.uvs.push_back(SmfVec2{1, 0});
    m.uvs.push_back(SmfVec2{0.5f, 1.f});
    m.indices.push_back(b0);
    m.indices.push_back(b0 + 1);
    m.indices.push_back(b0 + 2);
}

static void BuildTetrahedron(JhTriangleMesh& m, float size) {
    const float s = std::max(1.0e-4f, size);
    const SmfVec3 v0(s, s, s);
    const SmfVec3 v1(-s, -s, s);
    const SmfVec3 v2(-s, s, -s);
    const SmfVec3 v3(s, -s, -s);
    m.positions.clear();
    m.normals.clear();
    m.uvs.clear();
    m.indices.clear();
    AddFace(m, v0, v2, v1);
    AddFace(m, v0, v1, v3);
    AddFace(m, v0, v3, v2);
    AddFace(m, v1, v2, v3);
}

static void BuildSquarePyramid(JhTriangleMesh& m, float baseWidth, float height) {
    const float half = std::max(1.0e-4f, baseWidth) * 0.5f;
    const float h = std::max(1.0e-4f, height);
    const SmfVec3 base[4] = {
        SmfVec3{-half, 0, -half},
        SmfVec3{half, 0, -half},
        SmfVec3{half, 0, half},
        SmfVec3{-half, 0, half},
    };
    const SmfVec3 apex(0, h, 0);
    m.positions.clear();
    m.normals.clear();
    m.uvs.clear();
    m.indices.clear();
    {
        const SmfVec3 bn{0, -1, 0};
        const uint32_t b0 = static_cast<uint32_t>(m.positions.size());
        for (int i = 0; i < 4; ++i) {
            m.positions.push_back(base[i]);
            m.normals.push_back(bn);
            m.uvs.push_back(SmfVec2{static_cast<float>(i % 2), static_cast<float>(i / 2)});
        }
        m.indices.push_back(b0 + 0);
        m.indices.push_back(b0 + 2);
        m.indices.push_back(b0 + 1);
        m.indices.push_back(b0 + 0);
        m.indices.push_back(b0 + 3);
        m.indices.push_back(b0 + 2);
    }
    auto addSide = [&](const SmfVec3& p0, const SmfVec3& p1) {
        const float ax = p1.x - p0.x, ay = p1.y - p0.y, az = p1.z - p0.z;
        const float bx = apex.x - p0.x, by = apex.y - p0.y, bz = apex.z - p0.z;
        SmfVec3 n{
            ay * bz - az * by,
            az * bx - ax * bz,
            ax * by - ay * bx,
        };
        n = JhNormalized(n);
        const uint32_t t0 = static_cast<uint32_t>(m.positions.size());
        m.positions.push_back(p0);
        m.positions.push_back(p1);
        m.positions.push_back(apex);
        m.normals.push_back(n);
        m.normals.push_back(n);
        m.normals.push_back(n);
        m.uvs.push_back(SmfVec2{0, 0});
        m.uvs.push_back(SmfVec2{1, 0});
        m.uvs.push_back(SmfVec2{0.5f, 1.f});
        m.indices.push_back(t0);
        m.indices.push_back(t0 + 1);
        m.indices.push_back(t0 + 2);
    };
    addSide(base[0], base[1]);
    addSide(base[1], base[2]);
    addSide(base[2], base[3]);
    addSide(base[3], base[0]);
}

/// Same topology as `Solstice::Arzachel::Polyhedra::Icosphere` (icosahedron + edge subdivision, projected to sphere).
static void BuildIcosphere(JhTriangleMesh& m, float radius, int subdivisions) {
    m.positions.clear();
    m.normals.clear();
    m.uvs.clear();
    m.indices.clear();
    const float T = (1.0f + std::sqrt(5.0f)) * 0.5f;
    std::vector<SmfVec3> verts = {JhNormalized(SmfVec3{-1, T, 0}),   JhNormalized(SmfVec3{1, T, 0}),
        JhNormalized(SmfVec3{-1, -T, 0}), JhNormalized(SmfVec3{1, -T, 0}), JhNormalized(SmfVec3{0, -1, T}),
        JhNormalized(SmfVec3{0, 1, T}),   JhNormalized(SmfVec3{0, -1, -T}), JhNormalized(SmfVec3{0, 1, -T}),
        JhNormalized(SmfVec3{T, 0, -1}),  JhNormalized(SmfVec3{T, 0, 1}),
        JhNormalized(SmfVec3{-T, 0, -1}), JhNormalized(SmfVec3{-T, 0, 1})};
    std::vector<std::array<uint32_t, 3>> faces = {
        {0, 11, 5},  {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9},  {5, 11, 4},  {11, 10, 2},
        {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},  {3, 2, 6},  {3, 6, 8},  {3, 8, 9},  {4, 9, 5},
        {2, 4, 11},  {6, 2, 10},  {8, 6, 7},  {9, 8, 1},
    };

    for (int sub = 0; sub < subdivisions; ++sub) {
        std::map<std::pair<uint32_t, uint32_t>, uint32_t> edge;
        std::vector<std::array<uint32_t, 3>> next;
        next.reserve(faces.size() * 4);
        auto getMid = [&](uint32_t v0, uint32_t v1) -> uint32_t {
            if (v0 > v1) {
                std::swap(v0, v1);
            }
            const std::pair<uint32_t, uint32_t> key{v0, v1};
            const auto it = edge.find(key);
            if (it != edge.end()) {
                return it->second;
            }
            const SmfVec3 mid = JhNormalized(
                {verts[v0].x + verts[v1].x, verts[v0].y + verts[v1].y, verts[v0].z + verts[v1].z});
            const uint32_t id = static_cast<uint32_t>(verts.size());
            verts.push_back(mid);
            edge.emplace(key, id);
            return id;
        };
        for (const auto& f : faces) {
            const uint32_t a = getMid(f[0], f[1]);
            const uint32_t b = getMid(f[1], f[2]);
            const uint32_t c = getMid(f[2], f[0]);
            next.push_back({f[0], a, c});
            next.push_back({f[1], b, a});
            next.push_back({f[2], c, b});
            next.push_back({a, b, c});
        }
        faces = std::move(next);
    }

    m.positions.reserve(verts.size());
    m.normals.reserve(verts.size());
    m.uvs.assign(verts.size(), SmfVec2{0, 0});
    for (const auto& u : verts) {
        m.positions.push_back({u.x * radius, u.y * radius, u.z * radius});
        m.normals.push_back(u);
    }
    m.indices.reserve(faces.size() * 3);
    for (const auto& f : faces) {
        m.indices.push_back(f[0]);
        m.indices.push_back(f[1]);
        m.indices.push_back(f[2]);
    }
}

void AddArzachelProp(PropKind kind, const PropBuildParams& p, JhTriangleMesh& out, char* statusLine, size_t statusLineMax) {
    if (!statusLine || statusLineMax == 0) {
        return;
    }
    statusLine[0] = '\0';
    const float u = std::max(0.01f, p.uniformScale);
    const char* label = "Arzachel-style prop";
    using namespace Jackhammer::MeshOps;
    switch (kind) {
    case PropKind::ArzachelCube: {
        label = "Arzachel-style cube (AABB)";
        const float h = u * 0.5f;
        MakeAxisAlignedBox(out, SmfVec3{-h, -h, -h}, SmfVec3{h, h, h});
    } break;
    case PropKind::UvSphere: {
        label = "UV sphere (MeshOps)";
        const int seg = std::max(3, p.i0);
        MakeSphere(out, u, seg, std::max(3, seg * 2));
    } break;
    case PropKind::Icosphere: {
        label = "Icosphere (icosa + subdiv, Arzachel-style)";
        const int sub = std::max(0, p.i2);
        BuildIcosphere(out, u, sub);
    } break;
    case PropKind::Cylinder: {
        label = "Cylinder (MeshOps)";
        const int seg = std::max(3, p.i0);
        MakeCylinder(out, u * 0.5f, u * 2.f, seg, 1);
    } break;
    case PropKind::Torus: {
        label = "Torus (MeshOps)";
        const int maj = std::max(3, p.i0);
        const int min = std::max(3, p.i1);
        const float majorR = u;
        const float minorR = u * 0.28f;
        MakeTorus(out, majorR, minorR, maj, min);
    } break;
    case PropKind::GroundPlane: {
        label = "Ground plane (quad)";
        const float h = u * 2.f;
        BuildPlaneYUp(out, h, h);
    } break;
    case PropKind::Tetrahedron: {
        label = "Tetrahedron";
        BuildTetrahedron(out, u);
    } break;
    case PropKind::SquarePyramid: {
        label = "Square pyramid";
        BuildSquarePyramid(out, u * 2.f, u * 1.5f);
    } break;
    }
    if (out.uvs.size() != out.positions.size() && out.normals.size() == out.positions.size() && !out.positions.empty()) {
        out.uvs.assign(out.positions.size(), SmfVec2{0, 0});
    }
    if (out.normals.size() != out.positions.size() && !out.positions.empty() && (out.indices.size() % 3) == 0) {
        out.normals.clear();
        RecalculateNormals(out);
    } else if (out.normals.size() != out.positions.size()) {
        out.normals.clear();
    }
    std::snprintf(
        statusLine, statusLineMax, "Mesh workshop: %s (normals recalculated if needed).", label);
}

} // namespace Jackhammer::ArzachelProps
