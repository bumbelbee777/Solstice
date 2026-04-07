#include "FluidVolumeVisualizer.hxx"
#include <Render/Assets/ShaderLoader.hxx>
#include <Render/Scene/Camera.hxx>
#include <Math/Matrix.hxx>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Solstice::Render {

namespace {

struct DebugVtx {
    float x, y, z;
    uint8_t r, g, b, a;
};

inline Math::Vec3 EdgeInterp(const Math::Vec3& pa, const Math::Vec3& pb, float sa, float sb, float iso) {
    const float da = sa - iso;
    const float db = sb - iso;
    const float denom = da - db;
    float t = (std::abs(denom) > 1e-20f) ? da / denom : 0.5f;
    t = std::clamp(t, 0.0f, 1.0f);
    return pa * (1.0f - t) + pb * t;
}

void EmitTetIso(const Math::Vec3 p[4], const float s[4], float iso, std::vector<DebugVtx>& vtx, std::vector<uint16_t>& idx, uint32_t abgr,
                int& triBudget) {
    if (triBudget <= 0) {
        return;
    }
    const bool in0 = s[0] >= iso;
    const bool in1 = s[1] >= iso;
    const bool in2 = s[2] >= iso;
    const bool in3 = s[3] >= iso;
    const int nIn = (in0 ? 1 : 0) + (in1 ? 1 : 0) + (in2 ? 1 : 0) + (in3 ? 1 : 0);
    if (nIn == 0 || nIn == 4) {
        return;
    }

    auto emitTri = [&](const Math::Vec3& a, const Math::Vec3& b, const Math::Vec3& c) {
        if (triBudget <= 0) {
            return;
        }
        const uint16_t base = static_cast<uint16_t>(vtx.size());
        vtx.push_back({a.x, a.y, a.z, static_cast<uint8_t>((abgr >> 16) & 0xFF), static_cast<uint8_t>((abgr >> 8) & 0xFF),
                       static_cast<uint8_t>(abgr & 0xFF), 255});
        vtx.push_back({b.x, b.y, b.z, static_cast<uint8_t>((abgr >> 16) & 0xFF), static_cast<uint8_t>((abgr >> 8) & 0xFF),
                       static_cast<uint8_t>(abgr & 0xFF), 255});
        vtx.push_back({c.x, c.y, c.z, static_cast<uint8_t>((abgr >> 16) & 0xFF), static_cast<uint8_t>((abgr >> 8) & 0xFF),
                       static_cast<uint8_t>(abgr & 0xFF), 255});
        idx.push_back(base);
        idx.push_back(static_cast<uint16_t>(base + 1));
        idx.push_back(static_cast<uint16_t>(base + 2));
        --triBudget;
    };

    auto ei = [&](int a, int b) { return EdgeInterp(p[a], p[b], s[a], s[b], iso); };

    if (nIn == 1) {
        const int i = in0 ? 0 : (in1 ? 1 : (in2 ? 2 : 3));
        int others[3];
        int q = 0;
        for (int j = 0; j < 4; ++j) {
            if (j != i) {
                others[q++] = j;
            }
        }
        emitTri(ei(i, others[0]), ei(i, others[1]), ei(i, others[2]));
        return;
    }
    if (nIn == 3) {
        const int o = !in0 ? 0 : (!in1 ? 1 : (!in2 ? 2 : 3));
        int ins3[3];
        int q = 0;
        for (int j = 0; j < 4; ++j) {
            if (j != o) {
                ins3[q++] = j;
            }
        }
        emitTri(ei(o, ins3[0]), ei(o, ins3[1]), ei(o, ins3[2]));
        return;
    }
    int ins[2];
    int outs[2];
    int ni = 0;
    int no = 0;
    const bool insd[4] = {in0, in1, in2, in3};
    for (int i = 0; i < 4; ++i) {
        if (insd[i]) {
            ins[ni++] = i;
        } else {
            outs[no++] = i;
        }
    }
    const int ia = ins[0];
    const int ib = ins[1];
    const int oc = outs[0];
    const int od = outs[1];
    const Math::Vec3 ac = ei(ia, oc);
    const Math::Vec3 ad = ei(ia, od);
    const Math::Vec3 bc = ei(ib, oc);
    const Math::Vec3 bd = ei(ib, od);
    emitTri(ac, ad, bd);
    emitTri(ac, bd, bc);
}

inline bool WorldPointInFluidClip(const Math::Vec3& p, bool clipOn, const Math::Vec3& cmn, const Math::Vec3& cmx) {
    if (!clipOn) {
        return true;
    }
    return p.x >= cmn.x && p.x <= cmx.x && p.y >= cmn.y && p.y <= cmx.y && p.z >= cmn.z && p.z <= cmx.z;
}

inline Math::Vec3 MacInteriorCenterWorld(const Math::Vec3& origin, const Physics::GridLayout& g, int i, int j, int k) {
    return origin + Math::Vec3((static_cast<float>(i) - 0.5f) * g.hx, (static_cast<float>(j) - 0.5f) * g.hy,
                               (static_cast<float>(k) - 0.5f) * g.hz);
}

/// Center of the marching-cube cell whose low corner is MAC indices (i+1,j+1,k+1) with i,j,k in [0,N-2].
inline Math::Vec3 IsoMacroCellCenterWorld(const Math::Vec3& origin, const Physics::GridLayout& g, int i, int j, int k) {
    return origin + Math::Vec3((static_cast<float>(i) + 1.0f) * g.hx, (static_cast<float>(j) + 1.0f) * g.hy,
                               (static_cast<float>(k) + 1.0f) * g.hz);
}

} // namespace

FluidVolumeVisualizer::~FluidVolumeVisualizer() {
    Shutdown();
}

void FluidVolumeVisualizer::Shutdown() {
    if (bgfx::isValid(m_RayProgram)) {
        bgfx::destroy(m_RayProgram);
        m_RayProgram = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_DebugProgram)) {
        bgfx::destroy(m_DebugProgram);
        m_DebugProgram = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_VolumeTex)) {
        bgfx::destroy(m_VolumeTex);
        m_VolumeTex = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidInvViewProj)) {
        bgfx::destroy(m_uFluidInvViewProj);
        m_uFluidInvViewProj = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidCameraPos)) {
        bgfx::destroy(m_uFluidCameraPos);
        m_uFluidCameraPos = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidAabbMin)) {
        bgfx::destroy(m_uFluidAabbMin);
        m_uFluidAabbMin = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidAabbMax)) {
        bgfx::destroy(m_uFluidAabbMax);
        m_uFluidAabbMax = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidVisParams)) {
        bgfx::destroy(m_uFluidVisParams);
        m_uFluidVisParams = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidViewport)) {
        bgfx::destroy(m_uFluidViewport);
        m_uFluidViewport = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidClipMin)) {
        bgfx::destroy(m_uFluidClipMin);
        m_uFluidClipMin = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidClipMax)) {
        bgfx::destroy(m_uFluidClipMax);
        m_uFluidClipMax = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_uFluidClipParams)) {
        bgfx::destroy(m_uFluidClipParams);
        m_uFluidClipParams = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_sTexVolume)) {
        bgfx::destroy(m_sTexVolume);
        m_sTexVolume = BGFX_INVALID_HANDLE;
    }
    m_TexW = m_TexH = m_TexD = 0;
}

void FluidVolumeVisualizer::EnsureRayResources() {
    if (!bgfx::isValid(m_RayProgram)) {
        const bgfx::ShaderHandle vsh = ShaderLoader::LoadShader("vs_fluid_volume_ray.bin");
        const bgfx::ShaderHandle fsh = ShaderLoader::LoadShader("fs_fluid_volume_ray.bin");
        if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
            m_RayProgram = bgfx::createProgram(vsh, fsh, true);
        }
        m_RayLayout.begin(bgfx::getRendererType()).add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
    }

    auto ensureVec4 = [](bgfx::UniformHandle& h, const char* name) {
        if (!bgfx::isValid(h)) {
            h = bgfx::createUniform(name, bgfx::UniformType::Vec4);
        }
    };
    auto ensureMat4 = [](bgfx::UniformHandle& h, const char* name) {
        if (!bgfx::isValid(h)) {
            h = bgfx::createUniform(name, bgfx::UniformType::Mat4);
        }
    };
    ensureMat4(m_uFluidInvViewProj, "u_fluidInvViewProj");
    ensureVec4(m_uFluidCameraPos, "u_fluidCameraPos");
    ensureVec4(m_uFluidAabbMin, "u_fluidAabbMin");
    ensureVec4(m_uFluidAabbMax, "u_fluidAabbMax");
    ensureVec4(m_uFluidVisParams, "u_fluidVisParams");
    ensureVec4(m_uFluidViewport, "u_fluidViewport");
    ensureVec4(m_uFluidClipMin, "u_fluidClipMin");
    ensureVec4(m_uFluidClipMax, "u_fluidClipMax");
    ensureVec4(m_uFluidClipParams, "u_fluidClipParams");
    if (!bgfx::isValid(m_sTexVolume)) {
        m_sTexVolume = bgfx::createUniform("s_texVolume", bgfx::UniformType::Sampler);
    }
}

void FluidVolumeVisualizer::EnsureDebugResources() {
    if (bgfx::isValid(m_DebugProgram)) {
        return;
    }
    const bgfx::ShaderHandle vsh = ShaderLoader::LoadShader("vs_debug.bin");
    const bgfx::ShaderHandle fsh = ShaderLoader::LoadShader("fs_debug.bin");
    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        m_DebugProgram = bgfx::createProgram(vsh, fsh, true);
    }
    m_DebugLayout.begin(bgfx::getRendererType())
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .end();
}

void FluidVolumeVisualizer::EnsureTexture3D(uint16_t nx, uint16_t ny, uint16_t nz) {
    if (nx == 0 || ny == 0 || nz == 0) {
        return;
    }
    if (bgfx::isValid(m_VolumeTex) && m_TexW == nx && m_TexH == ny && m_TexD == nz) {
        return;
    }
    if (bgfx::isValid(m_VolumeTex)) {
        bgfx::destroy(m_VolumeTex);
        m_VolumeTex = BGFX_INVALID_HANDLE;
    }
    m_VolumeTex = bgfx::createTexture3D(nx, ny, nz, false, bgfx::TextureFormat::R8,
                                        BGFX_TEXTURE_NONE | BGFX_SAMPLER_UVW_CLAMP);
    m_TexW = nx;
    m_TexH = ny;
    m_TexD = nz;
}

void FluidVolumeVisualizer::UploadFromSimulation(const Physics::FluidSimulation& fluid, FieldMode mode) {
    const Physics::GridLayout g = fluid.GetGridLayout();
    if (g.Nx <= 0 || g.Ny <= 0 || g.Nz <= 0) {
        return;
    }
    const size_t vox = static_cast<size_t>(g.Nx) * static_cast<size_t>(g.Ny) * static_cast<size_t>(g.Nz);
    m_UploadScratch.resize(vox);

    const auto& T = fluid.GetTemperature();
    const int sx = g.sx();
    const int sxy = g.sxy();
    const Math::Vec3 origin = fluid.GetGridOrigin();
    const bool clipOn = fluid.IsVolumeVisualizationClipEnabled();
    const Math::Vec3 cmn = fluid.GetVolumeVisualizationClipMin();
    const Math::Vec3 cmx = fluid.GetVolumeVisualizationClipMax();

    if (mode == FieldMode::Temperature) {
        // Map to fixed THot/TCold band so colors stay meaningful (blue=cold, red=hot). Adaptive min/max on a
        // thin clipped column often collapses span → grey R8 mush after smoothstep.
        const auto& th = fluid.GetThermal();
        const float tLo = std::min(th.TCold, th.THot);
        const float tHi = std::max(th.TCold, th.THot);
        const float invPhys = 1.0f / std::max(tHi - tLo, 1e-4f);
        size_t w = 0;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const int ix = i + sx * j + sxy * k;
                    const float t = T[static_cast<size_t>(ix)];
                    const Math::Vec3 cc = MacInteriorCenterWorld(origin, g, i, j, k);
                    float u = 0.0f;
                    if (WorldPointInFluidClip(cc, clipOn, cmn, cmx)) {
                        u = std::clamp((t - tLo) * invPhys, 0.0f, 1.0f);
                    }
                    m_UploadScratch[w++] = static_cast<uint8_t>(u * 255.0f);
                }
            }
        }
    } else {
        m_SchlierenScratch.assign(vox, 0.0f);
        auto lin = [&](int i, int j, int k) -> size_t {
            return static_cast<size_t>(i - 1) + static_cast<size_t>(g.Nx) * static_cast<size_t>(j - 1)
                + static_cast<size_t>(g.Nx) * static_cast<size_t>(g.Ny) * static_cast<size_t>(k - 1);
        };
        auto tAt = [&](int i, int j, int k) -> float {
            const int ix = i + sx * j + sxy * k;
            return T[static_cast<size_t>(ix)];
        };
        float gmax = 1e-8f;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const float gx = 0.5f * (tAt(i + 1, j, k) - tAt(i - 1, j, k)) / std::max(g.hx, 1e-8f);
                    const float gy = 0.5f * (tAt(i, j + 1, k) - tAt(i, j - 1, k)) / std::max(g.hy, 1e-8f);
                    const float gz = 0.5f * (tAt(i, j, k + 1) - tAt(i, j, k - 1)) / std::max(g.hz, 1e-8f);
                    const float mag = std::sqrt(gx * gx + gy * gy + gz * gz);
                    const size_t id = lin(i, j, k);
                    m_SchlierenScratch[id] = mag;
                    const Math::Vec3 cc = MacInteriorCenterWorld(origin, g, i, j, k);
                    if (WorldPointInFluidClip(cc, clipOn, cmn, cmx)) {
                        gmax = std::max(gmax, mag);
                    }
                }
            }
        }
        const float invG = 1.0f / gmax;
        size_t w = 0;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const size_t id = lin(i, j, k);
                    const Math::Vec3 cc = MacInteriorCenterWorld(origin, g, i, j, k);
                    float u = 0.0f;
                    if (WorldPointInFluidClip(cc, clipOn, cmn, cmx)) {
                        u = std::clamp(m_SchlierenScratch[id] * invG, 0.0f, 1.0f);
                    }
                    m_UploadScratch[w++] = static_cast<uint8_t>(u * 255.0f);
                }
            }
        }
    }

    EnsureTexture3D(static_cast<uint16_t>(g.Nx), static_cast<uint16_t>(g.Ny), static_cast<uint16_t>(g.Nz));
    if (!bgfx::isValid(m_VolumeTex)) {
        return;
    }

    const bgfx::Memory* mem = bgfx::copy(m_UploadScratch.data(), static_cast<uint32_t>(m_UploadScratch.size()));
    bgfx::updateTexture3D(m_VolumeTex, 0, 0, 0, 0, static_cast<uint16_t>(g.Nx), static_cast<uint16_t>(g.Ny),
                         static_cast<uint16_t>(g.Nz), mem);
}

void FluidVolumeVisualizer::DrawOverlay(const Camera& cam, int viewportW, int viewportH, const Physics::FluidSimulation& fluid,
                                        DrawMode drawMode, FieldMode fieldMode, float isoLevel01, int velocityStride, float velocityScale,
                                        bgfx::ViewId viewId) {
    if (viewportW <= 0 || viewportH <= 0) {
        return;
    }
    const Physics::GridLayout g = fluid.GetGridLayout();
    if (g.Nx <= 0) {
        return;
    }

    const Math::Vec3 origin = fluid.GetGridOrigin();
    const bool clipVis = fluid.IsVolumeVisualizationClipEnabled();
    const Math::Vec3 clipMin = fluid.GetVolumeVisualizationClipMin();
    const Math::Vec3 clipMax = fluid.GetVolumeVisualizationClipMax();
    const Math::Vec3 aabbMin = origin;
    const Math::Vec3 aabbMax = origin + Math::Vec3(static_cast<float>(g.Nx) * g.hx, static_cast<float>(g.Ny) * g.hy,
                                                   static_cast<float>(g.Nz) * g.hz);

    const float aspect = static_cast<float>(viewportW) / static_cast<float>(viewportH);
    const Math::Matrix4 view = cam.GetViewMatrix();
    const Math::Matrix4 proj = cam.GetProjectionMatrix(aspect, 0.1f, 2000.0f);
    const Math::Matrix4 viewProj = proj * view;
    const Math::Matrix4 invViewProj = viewProj.Inverse();
    const Math::Matrix4 invViewProjT = invViewProj.Transposed();
    const Math::Matrix4 viewT = view.Transposed();
    const Math::Matrix4 projT = proj.Transposed();

    bgfx::setViewRect(viewId, 0, 0, static_cast<uint16_t>(viewportW), static_cast<uint16_t>(viewportH));
    bgfx::setViewTransform(viewId, &viewT.M[0][0], &projT.M[0][0]);

    if (drawMode == DrawMode::Raymarch) {
        EnsureRayResources();
        UploadFromSimulation(fluid, fieldMode);
        if (!bgfx::isValid(m_RayProgram) || !bgfx::isValid(m_VolumeTex)) {
            return;
        }

        const float camPos[4] = {cam.GetPosition().x, cam.GetPosition().y, cam.GetPosition().z, 0.0f};
        const float bmin[4] = {aabbMin.x, aabbMin.y, aabbMin.z, 0.0f};
        const float bmax[4] = {aabbMax.x, aabbMax.y, aabbMax.z, 0.0f};
        const float vis[4] = {fieldMode == FieldMode::Temperature ? 0.0f : 1.0f, 72.0f, isoLevel01, 0.0f};
        const float invW = 1.0f / static_cast<float>(std::max(viewportW, 1));
        const float invH = 1.0f / static_cast<float>(std::max(viewportH, 1));
        const float vp[4] = {static_cast<float>(viewportW), static_cast<float>(viewportH), invW, invH};

        bgfx::setUniform(m_uFluidInvViewProj, &invViewProjT.M[0][0]);
        bgfx::setUniform(m_uFluidCameraPos, camPos);
        bgfx::setUniform(m_uFluidAabbMin, bmin);
        bgfx::setUniform(m_uFluidAabbMax, bmax);
        bgfx::setUniform(m_uFluidVisParams, vis);
        bgfx::setUniform(m_uFluidViewport, vp);
        const float clipEn = clipVis ? 1.0f : 0.0f;
        const float cminU[4] = {clipMin.x, clipMin.y, clipMin.z, 0.0f};
        const float cmaxU[4] = {clipMax.x, clipMax.y, clipMax.z, 0.0f};
        const float cparU[4] = {clipEn, 0.0f, 0.0f, 0.0f};
        bgfx::setUniform(m_uFluidClipMin, cminU);
        bgfx::setUniform(m_uFluidClipMax, cmaxU);
        bgfx::setUniform(m_uFluidClipParams, cparU);
        bgfx::setTexture(0, m_sTexVolume, m_VolumeTex);

        struct RayVtx {
            float x, y, z;
        };
        const RayVtx tri[3] = {{-1.0f, -1.0f, 0.0f}, {3.0f, -1.0f, 0.0f}, {-1.0f, 3.0f, 0.0f}};
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, 3, m_RayLayout);
        if (tvb.data != nullptr) {
            std::memcpy(tvb.data, tri, sizeof(tri));
            bgfx::setVertexBuffer(0, &tvb);
            const float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            bgfx::setTransform(ident);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, m_RayProgram);
        }
        return;
    }

    EnsureDebugResources();
    if (!bgfx::isValid(m_DebugProgram)) {
        return;
    }

    if (drawMode == DrawMode::IsoSurface) {
        const auto& th = fluid.GetThermal();
        const float tLo = std::min(th.TCold, th.THot);
        const float tHi = std::max(th.TCold, th.THot);
        const float isoWorld = tLo + std::clamp(isoLevel01, 0.0f, 1.0f) * std::max(tHi - tLo, 1e-6f);

        static const int kTet[6][4] = {
            {0, 1, 2, 6}, {0, 2, 3, 6}, {0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}, {0, 5, 1, 6},
        };

        const auto& T = fluid.GetTemperature();
        const int sx = g.sx();
        const int sxy = g.sxy();
        std::vector<DebugVtx> vtx;
        std::vector<uint16_t> idx;
        vtx.reserve(4096);
        idx.reserve(6144);
        int triBudget = 12000;

        Math::Vec3 cp[8];
        float sv[8];

        for (int k = 0; k < g.Nz - 1 && triBudget > 0; ++k) {
            for (int j = 0; j < g.Ny - 1 && triBudget > 0; ++j) {
                for (int i = 0; i < g.Nx - 1 && triBudget > 0; ++i) {
                    const Math::Vec3 isoCenter = IsoMacroCellCenterWorld(origin, g, i, j, k);
                    if (clipVis && !WorldPointInFluidClip(isoCenter, true, clipMin, clipMax)) {
                        continue;
                    }
                    for (int mask = 0; mask < 8; ++mask) {
                        const int dx = mask & 1;
                        const int dy = (mask >> 1) & 1;
                        const int dz = (mask >> 2) & 1;
                        const int pi = i + 1 + dx;
                        const int pj = j + 1 + dy;
                        const int pk = k + 1 + dz;
                        const int li = g.IX(pi, pj, pk);
                        cp[mask] = origin
                            + Math::Vec3((static_cast<float>(i + dx) + 0.5f) * g.hx, (static_cast<float>(j + dy) + 0.5f) * g.hy,
                                         (static_cast<float>(k + dz) + 0.5f) * g.hz);
                        sv[mask] = T[static_cast<size_t>(li)];
                    }
                    for (int t = 0; t < 6 && triBudget > 0; ++t) {
                        Math::Vec3 tp[4];
                        float ts[4];
                        for (int u = 0; u < 4; ++u) {
                            const int vi = kTet[t][u];
                            tp[u] = cp[vi];
                            ts[u] = sv[vi];
                        }
                        EmitTetIso(tp, ts, isoWorld, vtx, idx, 0xFF88CCFFu, triBudget);
                    }
                }
            }
        }

        if (!idx.empty() && bgfx::getAvailTransientVertexBuffer(static_cast<uint32_t>(vtx.size()), m_DebugLayout) >= vtx.size()
            && bgfx::getAvailTransientIndexBuffer(static_cast<uint32_t>(idx.size())) >= idx.size()) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, static_cast<uint32_t>(vtx.size()), m_DebugLayout);
            std::memcpy(tvb.data, vtx.data(), vtx.size() * sizeof(DebugVtx));
            bgfx::TransientIndexBuffer tib;
            bgfx::allocTransientIndexBuffer(&tib, static_cast<uint32_t>(idx.size()));
            std::memcpy(tib.data, idx.data(), idx.size() * sizeof(uint16_t));
            bgfx::setVertexBuffer(0, &tvb);
            bgfx::setIndexBuffer(&tib);
            const float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            bgfx::setTransform(ident);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_BLEND_ALPHA);
            bgfx::submit(viewId, m_DebugProgram);
        }
        return;
    }

    // VelocityLines
    const int stride = std::clamp(velocityStride, 1, 8);
    const int sx = g.sx();
    const int sxy = g.sxy();
    const auto& Vx = fluid.GetVx();
    const auto& Vy = fluid.GetVy();
    const auto& Vz = fluid.GetVz();
    std::vector<DebugVtx> lv;
    lv.reserve(static_cast<size_t>(g.Nx * g.Ny * g.Nz / stride / stride / stride * 2));

    for (int k = 1; k <= g.Nz; k += stride) {
        for (int j = 1; j <= g.Ny; j += stride) {
            for (int i = 1; i <= g.Nx; i += stride) {
                const int ix = i + sx * j + sxy * k;
                const size_t u = static_cast<size_t>(ix);
                const Math::Vec3 p = MacInteriorCenterWorld(origin, g, i, j, k);
                if (clipVis && !WorldPointInFluidClip(p, true, clipMin, clipMax)) {
                    continue;
                }
                const Math::Vec3 v = Math::Vec3(Vx[u], Vy[u], Vz[u]);
                const Math::Vec3 q = p + v * velocityScale;
                lv.push_back({p.x, p.y, p.z, 50, 200, 255, 255});
                lv.push_back({q.x, q.y, q.z, 50, 200, 255, 255});
            }
        }
    }

    if (!lv.empty() && bgfx::getAvailTransientVertexBuffer(static_cast<uint32_t>(lv.size()), m_DebugLayout) >= lv.size()) {
        bgfx::TransientVertexBuffer tvb;
        bgfx::allocTransientVertexBuffer(&tvb, static_cast<uint32_t>(lv.size()), m_DebugLayout);
        std::memcpy(tvb.data, lv.data(), lv.size() * sizeof(DebugVtx));
        bgfx::setVertexBuffer(0, &tvb);
        const float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        bgfx::setTransform(ident);
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_ALWAYS | BGFX_STATE_PT_LINES);
        bgfx::submit(viewId, m_DebugProgram);
    }
}

} // namespace Solstice::Render
