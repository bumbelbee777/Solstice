#include "NSSolver.hxx"
#include "FluidSparse.hxx"
#include "FluidFFT.hxx"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

#ifdef SOLSTICE_HAS_OPENMP
#include <omp.h>
#endif

#include "../Core/SIMD.hxx"

namespace Solstice::Physics {

using namespace ::Solstice::Core::SIMD;

constexpr int kMaxLinearSolveIterations = 32;
constexpr float kLinearSolveConvergenceEpsilon = 1e-4f;
constexpr float kLinearSolveTinyResidual = 1e-20f;

namespace {

template <typename T>
void CopyPodVector(std::vector<T>& dest, const std::vector<T>& src) {
    assert(dest.size() == src.size());
    if (dest.empty()) {
        return;
    }
    std::memcpy(dest.data(), src.data(), dest.size() * sizeof(T));
}

inline float ClampCellCoord(float value, float lo, float hi) {
    return std::clamp(value, lo, hi);
}

inline float SanitizeFinite(float value) {
    return std::isfinite(value) ? value : 0.0f;
}

inline Vec4 Vec4Splat(float s) {
    return Vec4(s, s, s, s);
}

inline Vec4 Vec4Iota(float base) {
    return Vec4(base, base + 1.0f, base + 2.0f, base + 3.0f);
}

inline void ClampCellCoordArray4(float* v, float lo, float hi) {
    for (int q = 0; q < 4; ++q) {
        v[q] = std::clamp(v[q], lo, hi);
    }
}

inline float SampleTrilinearField(const std::vector<float>& d0, float x, float y, float z, const GridLayout& g) {
    const int i0 = static_cast<int>(std::floor(x));
    const int j0 = static_cast<int>(std::floor(y));
    const int k0 = static_cast<int>(std::floor(z));
    const int i1 = i0 + 1;
    const int j1 = j0 + 1;
    const int k1 = k0 + 1;
    const float s1 = x - static_cast<float>(i0);
    const float s0 = 1.0f - s1;
    const float t1 = y - static_cast<float>(j0);
    const float t0 = 1.0f - t1;
    const float r1 = z - static_cast<float>(k0);
    const float r0 = 1.0f - r1;
    return s0 * (t0 * (r0 * d0[static_cast<size_t>(g.IX(i0, j0, k0))] + r1 * d0[static_cast<size_t>(g.IX(i0, j0, k1))]) +
                 t1 * (r0 * d0[static_cast<size_t>(g.IX(i0, j1, k0))] + r1 * d0[static_cast<size_t>(g.IX(i0, j1, k1))])) +
           s1 * (t0 * (r0 * d0[static_cast<size_t>(g.IX(i1, j0, k0))] + r1 * d0[static_cast<size_t>(g.IX(i1, j0, k1))]) +
                 t1 * (r0 * d0[static_cast<size_t>(g.IX(i1, j1, k0))] + r1 * d0[static_cast<size_t>(g.IX(i1, j1, k1))]));
}

inline float SampleTrilinearPtr(const float* d0, float x, float y, float z, const GridLayout& g) {
    const int i0 = static_cast<int>(std::floor(x));
    const int j0 = static_cast<int>(std::floor(y));
    const int k0 = static_cast<int>(std::floor(z));
    const int i1 = i0 + 1;
    const int j1 = j0 + 1;
    const int k1 = k0 + 1;
    const float s1 = x - static_cast<float>(i0);
    const float s0 = 1.0f - s1;
    const float t1 = y - static_cast<float>(j0);
    const float t0 = 1.0f - t1;
    const float r1 = z - static_cast<float>(k0);
    const float r0 = 1.0f - r1;
    return s0 * (t0 * (r0 * d0[g.IX(i0, j0, k0)] + r1 * d0[g.IX(i0, j0, k1)]) +
                 t1 * (r0 * d0[g.IX(i0, j1, k0)] + r1 * d0[g.IX(i0, j1, k1)])) +
           s1 * (t0 * (r0 * d0[g.IX(i1, j0, k0)] + r1 * d0[g.IX(i1, j0, k1)]) +
                 t1 * (r0 * d0[g.IX(i1, j1, k0)] + r1 * d0[g.IX(i1, j1, k1)]));
}

inline void SampleTrilinearSameCoords4Ptr(const float* d0, const float* u, const float* v, const float* w, float x, float y,
                                          float z, const GridLayout& g, float& outD0, float& outU, float& outV,
                                          float& outW) {
    const int i0 = static_cast<int>(std::floor(x));
    const int j0 = static_cast<int>(std::floor(y));
    const int k0 = static_cast<int>(std::floor(z));
    const int i1 = i0 + 1;
    const int j1 = j0 + 1;
    const int k1 = k0 + 1;
    const float s1 = x - static_cast<float>(i0);
    const float s0 = 1.0f - s1;
    const float t1 = y - static_cast<float>(j0);
    const float t0 = 1.0f - t1;
    const float r1 = z - static_cast<float>(k0);
    const float r0 = 1.0f - r1;
    const int ix000 = g.IX(i0, j0, k0);
    const int ix001 = g.IX(i0, j0, k1);
    const int ix010 = g.IX(i0, j1, k0);
    const int ix011 = g.IX(i0, j1, k1);
    const int ix100 = g.IX(i1, j0, k0);
    const int ix101 = g.IX(i1, j0, k1);
    const int ix110 = g.IX(i1, j1, k0);
    const int ix111 = g.IX(i1, j1, k1);
    auto blend = [&](const float* f) {
        return s0 * (t0 * (r0 * f[ix000] + r1 * f[ix001]) + t1 * (r0 * f[ix010] + r1 * f[ix011])) +
               s1 * (t0 * (r0 * f[ix100] + r1 * f[ix101]) + t1 * (r0 * f[ix110] + r1 * f[ix111]));
    };
    outD0 = blend(d0);
    outU = blend(u);
    outV = blend(v);
    outW = blend(w);
}

inline void NeighborMinMaxField(const std::vector<float>& d0, int i, int j, int k, const GridLayout& g, float& mn,
                                float& mx) {
    mn = d0[static_cast<size_t>(g.IX(i, j, k))];
    mx = mn;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const float v = d0[static_cast<size_t>(g.IX(i + dx, j + dy, k + dz))];
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
    }
}

inline void NeighborMinMaxPtr(const float* d0, int i, int j, int k, const GridLayout& g, float& mn, float& mx) {
    mn = d0[g.IX(i, j, k)];
    mx = mn;
    for (int dz = -1; dz <= 1; ++dz) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const float v = d0[g.IX(i + dx, j + dy, k + dz)];
                mn = std::min(mn, v);
                mx = std::max(mx, v);
            }
        }
    }
}

inline bool SparseCellSkip(const FluidSparseData* sparse, int i, int j, int k) {
    if (!sparse) {
        return false;
    }
    const int B = sparse->blockSize;
    const int bx = (i - 1) / B;
    const int by = (j - 1) / B;
    const int bz = (k - 1) / B;
    if (sparse->IsBlockEmpty(bx, by, bz)) {
        return true;
    }
    return false;
}

void SetBoundariesPtrImpl(int b, float* x, const GridLayout& g) {
    const int Nx = g.Nx;
    const int Ny = g.Ny;
    const int Nz = g.Nz;
    for (int k = 1; k <= Nz; ++k) {
        for (int i = 1; i <= Nx; ++i) {
            x[g.IX(i, 0, k)] = (b == 2) ? -x[g.IX(i, 1, k)] : x[g.IX(i, 1, k)];
            x[g.IX(i, Ny + 1, k)] = (b == 2) ? -x[g.IX(i, Ny, k)] : x[g.IX(i, Ny, k)];
        }
    }
    for (int j = 1; j <= Ny; ++j) {
        for (int i = 1; i <= Nx; ++i) {
            x[g.IX(i, j, 0)] = (b == 3) ? -x[g.IX(i, j, 1)] : x[g.IX(i, j, 1)];
            x[g.IX(i, j, Nz + 1)] = (b == 3) ? -x[g.IX(i, j, Nz)] : x[g.IX(i, j, Nz)];
        }
    }
    for (int k = 1; k <= Nz; ++k) {
        for (int j = 1; j <= Ny; ++j) {
            x[g.IX(0, j, k)] = (b == 1) ? -x[g.IX(1, j, k)] : x[g.IX(1, j, k)];
            x[g.IX(Nx + 1, j, k)] = (b == 1) ? -x[g.IX(Nx, j, k)] : x[g.IX(Nx, j, k)];
        }
    }

    x[g.IX(0, 0, 0)] = 0.33f * (x[g.IX(1, 0, 0)] + x[g.IX(0, 1, 0)] + x[g.IX(0, 0, 1)]);
    x[g.IX(0, Ny + 1, 0)] = 0.33f * (x[g.IX(1, Ny + 1, 0)] + x[g.IX(0, Ny, 0)] + x[g.IX(0, Ny + 1, 1)]);
    x[g.IX(0, 0, Nz + 1)] = 0.33f * (x[g.IX(1, 0, Nz + 1)] + x[g.IX(0, 1, Nz + 1)] + x[g.IX(0, 0, Nz)]);
    x[g.IX(0, Ny + 1, Nz + 1)] =
        0.33f * (x[g.IX(1, Ny + 1, Nz + 1)] + x[g.IX(0, Ny, Nz + 1)] + x[g.IX(0, Ny + 1, Nz)]);
    x[g.IX(Nx + 1, 0, 0)] = 0.33f * (x[g.IX(Nx, 0, 0)] + x[g.IX(Nx + 1, 1, 0)] + x[g.IX(Nx + 1, 0, 1)]);
    x[g.IX(Nx + 1, Ny + 1, 0)] =
        0.33f * (x[g.IX(Nx, Ny + 1, 0)] + x[g.IX(Nx + 1, Ny, 0)] + x[g.IX(Nx + 1, Ny + 1, 1)]);
    x[g.IX(Nx + 1, 0, Nz + 1)] =
        0.33f * (x[g.IX(Nx, 0, Nz + 1)] + x[g.IX(Nx + 1, 1, Nz + 1)] + x[g.IX(Nx + 1, 0, Nz)]);
    x[g.IX(Nx + 1, Ny + 1, Nz + 1)] =
        0.33f * (x[g.IX(Nx, Ny + 1, Nz + 1)] + x[g.IX(Nx + 1, Ny, Nz + 1)] + x[g.IX(Nx + 1, Ny + 1, Nz)]);
}

void AdvectPtrAnisotropic(int b, float* d, const float* d0, const float* u, const float* v, const float* w, float dt,
                          const GridLayout& g) {
    const float dtx = dt / std::max(g.hx, 1e-12f);
    const float dty = dt / std::max(g.hy, 1e-12f);
    const float dtz = dt / std::max(g.hz, 1e-12f);
    const Vec4 vdtx = Vec4Splat(dtx);
    const Vec4 vdty = Vec4Splat(dty);
    const Vec4 vdtz = Vec4Splat(dtz);
    const float lx = 0.5f;
    const float hxb = static_cast<float>(g.Nx) + 0.5f;
    const float ly = 0.5f;
    const float hyb = static_cast<float>(g.Ny) + 0.5f;
    const float lz = 0.5f;
    const float hzb = static_cast<float>(g.Nz) + 0.5f;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= g.Nz; k++) {
        for (int j = 1; j <= g.Ny; j++) {
            int i = 1;
            for (; i <= g.Nx - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float px[4];
                float py[4];
                float pz[4];
                (Vec4Iota(static_cast<float>(i)) - vu * vdtx).Store(px);
                (Vec4Splat(static_cast<float>(j)) - vv * vdty).Store(py);
                (Vec4Splat(static_cast<float>(k)) - vw * vdtz).Store(pz);
                ClampCellCoordArray4(px, lx, hxb);
                ClampCellCoordArray4(py, ly, hyb);
                ClampCellCoordArray4(pz, lz, hzb);
                for (int q = 0; q < 4; ++q) {
                    d[static_cast<size_t>(idx + q)] = SanitizeFinite(SampleTrilinearPtr(d0, px[q], py[q], pz[q], g));
                }
            }
            for (; i <= g.Nx; ++i) {
                const int idx = g.IX(i, j, k);
                const float x = ClampCellCoord(static_cast<float>(i) - u[static_cast<size_t>(idx)] * dtx, lx, hxb);
                const float y = ClampCellCoord(static_cast<float>(j) - v[static_cast<size_t>(idx)] * dty, ly, hyb);
                const float z = ClampCellCoord(static_cast<float>(k) - w[static_cast<size_t>(idx)] * dtz, lz, hzb);
                d[static_cast<size_t>(idx)] = SanitizeFinite(SampleTrilinearPtr(d0, x, y, z, g));
            }
        }
    }
    SetBoundariesPtrImpl(b, d, g);
}

void AdvectPtr(int b, float* d, const float* d0, const float* u, const float* v, const float* w, float dt,
               const GridLayout& g) {
    if (!g.LegacyIsotropicCube()) {
        AdvectPtrAnisotropic(b, d, d0, u, v, w, dt, g);
        return;
    }
    const int N = g.Nx;
    const float dt0 = dt * static_cast<float>(N);
    const Vec4 vdt = Vec4Splat(dt0);
    const float clampLo = 0.5f;
    const float clampHi = static_cast<float>(N) + 0.5f;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= N; k++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float px[4];
                float py[4];
                float pz[4];
                (Vec4Iota(static_cast<float>(i)) - vu * vdt).Store(px);
                (Vec4Splat(static_cast<float>(j)) - vv * vdt).Store(py);
                (Vec4Splat(static_cast<float>(k)) - vw * vdt).Store(pz);
                ClampCellCoordArray4(px, clampLo, clampHi);
                ClampCellCoordArray4(py, clampLo, clampHi);
                ClampCellCoordArray4(pz, clampLo, clampHi);
                for (int q = 0; q < 4; ++q) {
                    d[static_cast<size_t>(idx + q)] = SanitizeFinite(SampleTrilinearPtr(d0, px[q], py[q], pz[q], g));
                }
            }
            for (; i <= N; ++i) {
                const int idx = g.IX(i, j, k);
                const float x = ClampCellCoord(static_cast<float>(i) - dt0 * u[static_cast<size_t>(idx)], clampLo, clampHi);
                const float y = ClampCellCoord(static_cast<float>(j) - dt0 * v[static_cast<size_t>(idx)], clampLo, clampHi);
                const float z = ClampCellCoord(static_cast<float>(k) - dt0 * w[static_cast<size_t>(idx)], clampLo, clampHi);
                d[static_cast<size_t>(idx)] = SanitizeFinite(SampleTrilinearPtr(d0, x, y, z, g));
            }
        }
    }
    SetBoundariesPtrImpl(b, d, g);
}

void AdvectMacCormackAnisotropic(int b, float* d, const float* d0, const float* u, const float* v, const float* w,
                                 float dt, const GridLayout& g) {
    const float dtx = dt / std::max(g.hx, 1e-12f);
    const float dty = dt / std::max(g.hy, 1e-12f);
    const float dtz = dt / std::max(g.hz, 1e-12f);
    const Vec4 vdtx = Vec4Splat(dtx);
    const Vec4 vdty = Vec4Splat(dty);
    const Vec4 vdtz = Vec4Splat(dtz);
    const float lx = 0.5f;
    const float hxb = static_cast<float>(g.Nx) + 0.5f;
    const float ly = 0.5f;
    const float hyb = static_cast<float>(g.Ny) + 0.5f;
    const float lz = 0.5f;
    const float hzb = static_cast<float>(g.Nz) + 0.5f;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= g.Nz; k++) {
        for (int j = 1; j <= g.Ny; j++) {
            int i = 1;
            for (; i <= g.Nx - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float xf[4];
                float yf[4];
                float zf[4];
                (Vec4Iota(static_cast<float>(i)) - vu * vdtx).Store(xf);
                (Vec4Splat(static_cast<float>(j)) - vv * vdty).Store(yf);
                (Vec4Splat(static_cast<float>(k)) - vw * vdtz).Store(zf);
                ClampCellCoordArray4(xf, lx, hxb);
                ClampCellCoordArray4(yf, ly, hyb);
                ClampCellCoordArray4(zf, lz, hzb);
                for (int q = 0; q < 4; ++q) {
                    const int iq = i + q;
                    const int idxq = idx + q;
                    float phiHat = 0.0f;
                    float uu = 0.0f;
                    float vvB = 0.0f;
                    float ww = 0.0f;
                    SampleTrilinearSameCoords4Ptr(d0, u, v, w, xf[q], yf[q], zf[q], g, phiHat, uu, vvB, ww);
                    const float xb = ClampCellCoord(xf[q] + uu * dtx, lx, hxb);
                    const float yb = ClampCellCoord(yf[q] + vvB * dty, ly, hyb);
                    const float zb = ClampCellCoord(zf[q] + ww * dtz, lz, hzb);
                    const float phiTilde = SampleTrilinearPtr(d0, xb, yb, zb, g);
                    float corrected = phiHat + 0.5f * (d0[static_cast<size_t>(idxq)] - phiTilde);
                    float mn = 0.0f;
                    float mx = 0.0f;
                    NeighborMinMaxPtr(d0, iq, j, k, g, mn, mx);
                    corrected = std::clamp(corrected, mn, mx);
                    d[static_cast<size_t>(idxq)] = SanitizeFinite(corrected);
                }
            }
            for (; i <= g.Nx; ++i) {
                const int idx = g.IX(i, j, k);
                const float xf = ClampCellCoord(static_cast<float>(i) - u[static_cast<size_t>(idx)] * dtx, lx, hxb);
                const float yf = ClampCellCoord(static_cast<float>(j) - v[static_cast<size_t>(idx)] * dty, ly, hyb);
                const float zf = ClampCellCoord(static_cast<float>(k) - w[static_cast<size_t>(idx)] * dtz, lz, hzb);
                float phiHat = 0.0f;
                float uu = 0.0f;
                float vvB = 0.0f;
                float ww = 0.0f;
                SampleTrilinearSameCoords4Ptr(d0, u, v, w, xf, yf, zf, g, phiHat, uu, vvB, ww);
                const float xb = ClampCellCoord(xf + uu * dtx, lx, hxb);
                const float yb = ClampCellCoord(yf + vvB * dty, ly, hyb);
                const float zb = ClampCellCoord(zf + ww * dtz, lz, hzb);
                const float phiTilde = SampleTrilinearPtr(d0, xb, yb, zb, g);
                float corrected = phiHat + 0.5f * (d0[static_cast<size_t>(idx)] - phiTilde);
                float mn = 0.0f;
                float mx = 0.0f;
                NeighborMinMaxPtr(d0, i, j, k, g, mn, mx);
                corrected = std::clamp(corrected, mn, mx);
                d[static_cast<size_t>(idx)] = SanitizeFinite(corrected);
            }
        }
    }
    SetBoundariesPtrImpl(b, d, g);
}

void AdvectMacCormackPtr(int b, float* d, const float* d0, const float* u, const float* v, const float* w, float dt,
                         const GridLayout& g) {
    if (!g.LegacyIsotropicCube()) {
        AdvectMacCormackAnisotropic(b, d, d0, u, v, w, dt, g);
        return;
    }
    const int N = g.Nx;
    const float dt0 = dt * static_cast<float>(N);
    const Vec4 vdt = Vec4Splat(dt0);
    const float clampLo = 0.5f;
    const float clampHi = static_cast<float>(N) + 0.5f;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= N; k++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float xf[4];
                float yf[4];
                float zf[4];
                (Vec4Iota(static_cast<float>(i)) - vu * vdt).Store(xf);
                (Vec4Splat(static_cast<float>(j)) - vv * vdt).Store(yf);
                (Vec4Splat(static_cast<float>(k)) - vw * vdt).Store(zf);
                ClampCellCoordArray4(xf, clampLo, clampHi);
                ClampCellCoordArray4(yf, clampLo, clampHi);
                ClampCellCoordArray4(zf, clampLo, clampHi);
                for (int q = 0; q < 4; ++q) {
                    float phiHat = 0.0f;
                    float uu = 0.0f;
                    float vvB = 0.0f;
                    float ww = 0.0f;
                    SampleTrilinearSameCoords4Ptr(d0, u, v, w, xf[q], yf[q], zf[q], g, phiHat, uu, vvB, ww);
                    const float xb = ClampCellCoord(xf[q] + dt0 * uu, clampLo, clampHi);
                    const float yb = ClampCellCoord(yf[q] + dt0 * vvB, clampLo, clampHi);
                    const float zb = ClampCellCoord(zf[q] + dt0 * ww, clampLo, clampHi);
                    const float phiTilde = SampleTrilinearPtr(d0, xb, yb, zb, g);
                    float corrected = phiHat + 0.5f * (d0[static_cast<size_t>(idx + q)] - phiTilde);
                    float mn = 0.0f;
                    float mx = 0.0f;
                    NeighborMinMaxPtr(d0, i + q, j, k, g, mn, mx);
                    corrected = std::clamp(corrected, mn, mx);
                    d[static_cast<size_t>(idx + q)] = SanitizeFinite(corrected);
                }
            }
            for (; i <= N; ++i) {
                const int idx = g.IX(i, j, k);
                const float xf = ClampCellCoord(static_cast<float>(i) - dt0 * u[static_cast<size_t>(idx)], clampLo, clampHi);
                const float yf = ClampCellCoord(static_cast<float>(j) - dt0 * v[static_cast<size_t>(idx)], clampLo, clampHi);
                const float zf = ClampCellCoord(static_cast<float>(k) - dt0 * w[static_cast<size_t>(idx)], clampLo, clampHi);
                float phiHat = 0.0f;
                float uu = 0.0f;
                float vvB = 0.0f;
                float ww = 0.0f;
                SampleTrilinearSameCoords4Ptr(d0, u, v, w, xf, yf, zf, g, phiHat, uu, vvB, ww);
                const float xb = ClampCellCoord(xf + dt0 * uu, clampLo, clampHi);
                const float yb = ClampCellCoord(yf + dt0 * vvB, clampLo, clampHi);
                const float zb = ClampCellCoord(zf + dt0 * ww, clampLo, clampHi);
                const float phiTilde = SampleTrilinearPtr(d0, xb, yb, zb, g);
                float corrected = phiHat + 0.5f * (d0[static_cast<size_t>(idx)] - phiTilde);
                float mn = 0.0f;
                float mx = 0.0f;
                NeighborMinMaxPtr(d0, i, j, k, g, mn, mx);
                corrected = std::clamp(corrected, mn, mx);
                d[static_cast<size_t>(idx)] = SanitizeFinite(corrected);
            }
        }
    }
    SetBoundariesPtrImpl(b, d, g);
}

void SetThermalWallBoundariesPtr(float* T, const GridLayout& g, const Math::Vec3& origin, const FluidThermalTuning& th,
                                 float tHot, float tCold) {
    const int Nx = g.Nx;
    const int Ny = g.Ny;
    const int Nz = g.Nz;
    const int wallAxis = th.thermalWallAxis;
    const bool local = th.useLocalizedHotWall;
    const float u0 = std::min(th.hotWallUMin, th.hotWallUMax);
    const float u1 = std::max(th.hotWallUMin, th.hotWallUMax);
    const float v0 = std::min(th.hotWallVMin, th.hotWallVMax);
    const float v1 = std::max(th.hotWallVMin, th.hotWallVMax);
    auto inPlate = [&](float u, float wv) {
        return u >= u0 && u <= u1 && wv >= v0 && wv <= v1;
    };
    if (wallAxis == 1) {
        for (int k = 1; k <= Nz; ++k) {
            for (int i = 1; i <= Nx; ++i) {
                float ghostT = tHot;
                if (local) {
                    const float wu = origin.x + (static_cast<float>(i) - 0.5f) * g.hx;
                    const float wv = origin.z + (static_cast<float>(k) - 0.5f) * g.hz;
                    ghostT = inPlate(wu, wv) ? tHot : tCold;
                }
                T[g.IX(i, 0, k)] = ghostT;
                T[g.IX(i, Ny + 1, k)] = tCold;
            }
        }
        for (int j = 1; j <= Ny; ++j) {
            for (int k = 1; k <= Nz; ++k) {
                T[g.IX(0, j, k)] = T[g.IX(1, j, k)];
                T[g.IX(Nx + 1, j, k)] = T[g.IX(Nx, j, k)];
            }
        }
        for (int j = 1; j <= Ny; ++j) {
            for (int i = 1; i <= Nx; ++i) {
                T[g.IX(i, j, 0)] = T[g.IX(i, j, 1)];
                T[g.IX(i, j, Nz + 1)] = T[g.IX(i, j, Nz)];
            }
        }
    } else if (wallAxis == 0) {
        for (int j = 1; j <= Ny; ++j) {
            for (int k = 1; k <= Nz; ++k) {
                float ghostT = tHot;
                if (local) {
                    const float wu = origin.y + (static_cast<float>(j) - 0.5f) * g.hy;
                    const float wv = origin.z + (static_cast<float>(k) - 0.5f) * g.hz;
                    ghostT = inPlate(wu, wv) ? tHot : tCold;
                }
                T[g.IX(0, j, k)] = ghostT;
                T[g.IX(Nx + 1, j, k)] = tCold;
            }
        }
        for (int j = 1; j <= Ny; ++j) {
            for (int i = 1; i <= Nx; ++i) {
                T[g.IX(i, j, 0)] = T[g.IX(i, j, 1)];
                T[g.IX(i, j, Nz + 1)] = T[g.IX(i, j, Nz)];
            }
        }
        for (int k = 1; k <= Nz; ++k) {
            for (int i = 1; i <= Nx; ++i) {
                T[g.IX(i, 0, k)] = T[g.IX(i, 1, k)];
                T[g.IX(i, Ny + 1, k)] = T[g.IX(i, Ny, k)];
            }
        }
    } else {
        for (int j = 1; j <= Ny; ++j) {
            for (int i = 1; i <= Nx; ++i) {
                float ghostT = tHot;
                if (local) {
                    const float wu = origin.x + (static_cast<float>(i) - 0.5f) * g.hx;
                    const float wv = origin.y + (static_cast<float>(j) - 0.5f) * g.hy;
                    ghostT = inPlate(wu, wv) ? tHot : tCold;
                }
                T[g.IX(i, j, 0)] = ghostT;
                T[g.IX(i, j, Nz + 1)] = tCold;
            }
        }
        for (int j = 1; j <= Ny; ++j) {
            for (int k = 1; k <= Nz; ++k) {
                T[g.IX(0, j, k)] = T[g.IX(1, j, k)];
                T[g.IX(Nx + 1, j, k)] = T[g.IX(Nx, j, k)];
            }
        }
        for (int k = 1; k <= Nz; ++k) {
            for (int i = 1; i <= Nx; ++i) {
                T[g.IX(i, 0, k)] = T[g.IX(i, 1, k)];
                T[g.IX(i, Ny + 1, k)] = T[g.IX(i, Ny, k)];
            }
        }
    }
}

void LinearSolveIsotropicThermal(std::vector<float>& x, const std::vector<float>& x0, float a, float c, const GridLayout& g,
                                 const Math::Vec3& gridOrigin, const FluidThermalTuning& th, float tHot, float tCold) {
    const int N = g.Nx;
    const int s = g.sx();
    const int s2 = g.sxy();
    const float invC = 1.0f / c;
    for (int k = 0; k < kMaxLinearSolveIterations; k++) {
        float maxDelta = 0.0f;
        for (int m = 1; m <= N; m++) {
            for (int j = 1; j <= N; j++) {
                const int rowStart = g.IX(1, j, m);
                for (int i = 1; i <= N; i++) {
                    const int idx = rowStart + (i - 1);
                    const float oldValue = x[static_cast<size_t>(idx)];
                    float newValue = (x0[static_cast<size_t>(idx)]
                        + a * (x[static_cast<size_t>(idx + 1)] + x[static_cast<size_t>(idx - 1)]
                             + x[static_cast<size_t>(idx + s)] + x[static_cast<size_t>(idx - s)]
                             + x[static_cast<size_t>(idx + s2)] + x[static_cast<size_t>(idx - s2)])) * invC;
                    newValue = SanitizeFinite(newValue);
                    x[static_cast<size_t>(idx)] = newValue;
                    maxDelta = std::max(maxDelta, std::abs(newValue - oldValue));
                }
            }
        }
        SetThermalWallBoundariesPtr(x.data(), g, gridOrigin, th, tHot, tCold);
        if (!std::isfinite(maxDelta) || maxDelta <= kLinearSolveConvergenceEpsilon || maxDelta <= kLinearSolveTinyResidual
            || maxDelta == 0.0f) {
            break;
        }
    }
}

void LinearSolveAnisotropicDiffuseThermal(std::vector<float>& x, const std::vector<float>& x0, float ax, float ay, float az,
                                          const GridLayout& g, const Math::Vec3& gridOrigin, const FluidThermalTuning& th,
                                          float tHot, float tCold) {
    const int sx = g.sx();
    const int sxy = g.sxy();
    const float denom = 1.0f + 2.0f * (ax + ay + az);
    const float invD = 1.0f / denom;
    for (int it = 0; it < kMaxLinearSolveIterations; ++it) {
        float maxDelta = 0.0f;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    const float oldValue = x[static_cast<size_t>(idx)];
                    float newValue =
                        (x0[static_cast<size_t>(idx)]
                         + ax * (x[static_cast<size_t>(idx + 1)] + x[static_cast<size_t>(idx - 1)])
                         + ay * (x[static_cast<size_t>(idx + sx)] + x[static_cast<size_t>(idx - sx)])
                         + az * (x[static_cast<size_t>(idx + sxy)] + x[static_cast<size_t>(idx - sxy)]))
                        * invD;
                    newValue = SanitizeFinite(newValue);
                    x[static_cast<size_t>(idx)] = newValue;
                    maxDelta = std::max(maxDelta, std::abs(newValue - oldValue));
                }
            }
        }
        SetThermalWallBoundariesPtr(x.data(), g, gridOrigin, th, tHot, tCold);
        if (!std::isfinite(maxDelta) || maxDelta <= kLinearSolveConvergenceEpsilon || maxDelta <= kLinearSolveTinyResidual
            || maxDelta == 0.0f) {
            break;
        }
    }
}

} // namespace

void NSSolver::LinearSolveAnisotropicDiffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float ax, float ay,
                                           float az, const GridLayout& g) {
    const int sx = g.sx();
    const int sxy = g.sxy();
    const float denom = 1.0f + 2.0f * (ax + ay + az);
    const float invD = 1.0f / denom;
    for (int it = 0; it < kMaxLinearSolveIterations; ++it) {
        float maxDelta = 0.0f;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    const float oldValue = x[static_cast<size_t>(idx)];
                    float newValue =
                        (x0[static_cast<size_t>(idx)]
                         + ax * (x[static_cast<size_t>(idx + 1)] + x[static_cast<size_t>(idx - 1)])
                         + ay * (x[static_cast<size_t>(idx + sx)] + x[static_cast<size_t>(idx - sx)])
                         + az * (x[static_cast<size_t>(idx + sxy)] + x[static_cast<size_t>(idx - sxy)]))
                        * invD;
                    newValue = SanitizeFinite(newValue);
                    x[static_cast<size_t>(idx)] = newValue;
                    maxDelta = std::max(maxDelta, std::abs(newValue - oldValue));
                }
            }
        }
        SetBoundaries(b, x, g);
        if (!std::isfinite(maxDelta) || maxDelta <= kLinearSolveConvergenceEpsilon || maxDelta <= kLinearSolveTinyResidual
            || maxDelta == 0.0f) {
            break;
        }
    }
}

void NSSolver::LinearSolveAnisotropicPressure(std::vector<float>& p, const std::vector<float>& div, const GridLayout& g) {
    const int sx = g.sx();
    const int sxy = g.sxy();
    const float wx = 1.0f / (g.hx * g.hx);
    const float wy = 1.0f / (g.hy * g.hy);
    const float wz = 1.0f / (g.hz * g.hz);
    const float denom = 2.0f * (wx + wy + wz);
    const float invD = 1.0f / denom;
    for (int it = 0; it < kMaxLinearSolveIterations; ++it) {
        float maxDelta = 0.0f;
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                for (int i = 1; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    const float oldValue = p[static_cast<size_t>(idx)];
                    float newValue =
                        (div[static_cast<size_t>(idx)]
                         + wx * (p[static_cast<size_t>(idx + 1)] + p[static_cast<size_t>(idx - 1)])
                         + wy * (p[static_cast<size_t>(idx + sx)] + p[static_cast<size_t>(idx - sx)])
                         + wz * (p[static_cast<size_t>(idx + sxy)] + p[static_cast<size_t>(idx - sxy)]))
                        * invD;
                    newValue = SanitizeFinite(newValue);
                    p[static_cast<size_t>(idx)] = newValue;
                    maxDelta = std::max(maxDelta, std::abs(newValue - oldValue));
                }
            }
        }
        SetBoundaries(0, p, g);
        if (!std::isfinite(maxDelta) || maxDelta <= kLinearSolveConvergenceEpsilon || maxDelta <= kLinearSolveTinyResidual
            || maxDelta == 0.0f) {
            break;
        }
    }
}

void NSSolver::LinearSolveIsotropic(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c,
                                    const GridLayout& g) {
    const int N = g.Nx;
    const int s = g.sx();
    const int s2 = g.sxy();
    const float invC = 1.0f / c;
    for (int k = 0; k < kMaxLinearSolveIterations; k++) {
        float maxDelta = 0.0f;
        for (int m = 1; m <= N; m++) {
            for (int j = 1; j <= N; j++) {
                const int rowStart = g.IX(1, j, m);
                for (int i = 1; i <= N; i++) {
                    const int idx = rowStart + (i - 1);
                    const float oldValue = x[static_cast<size_t>(idx)];
                    float newValue = (x0[static_cast<size_t>(idx)]
                        + a * (x[static_cast<size_t>(idx + 1)] + x[static_cast<size_t>(idx - 1)]
                             + x[static_cast<size_t>(idx + s)] + x[static_cast<size_t>(idx - s)]
                             + x[static_cast<size_t>(idx + s2)] + x[static_cast<size_t>(idx - s2)])) * invC;
                    newValue = SanitizeFinite(newValue);
                    x[static_cast<size_t>(idx)] = newValue;
                    maxDelta = std::max(maxDelta, std::abs(newValue - oldValue));
                }
            }
        }
        SetBoundaries(b, x, g);
        if (!std::isfinite(maxDelta) || maxDelta <= kLinearSolveConvergenceEpsilon || maxDelta <= kLinearSolveTinyResidual
            || maxDelta == 0.0f) {
            break;
        }
    }
}

void NSSolver::DiffuseSpectralPeriodic(int b, std::vector<float>& x, const std::vector<float>& x0, float nu, float dt,
                                       const GridLayout& g) {
    if (!g.LegacyIsotropicCube()) {
        Diffuse(b, x, x0, nu, dt, g);
        return;
    }
    const int N = g.Nx;
    if (!FluidFFT::IsPowerOf2(N)) {
        Diffuse(b, x, x0, nu, dt, g);
        return;
    }
    const int n3 = N * N * N;
    std::vector<std::complex<float>> work(static_cast<size_t>(n3));
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int iz = 0; iz < N; ++iz) {
        for (int iy = 0; iy < N; ++iy) {
            for (int ix = 0; ix < N; ++ix) {
                const int idx = g.IX(ix + 1, iy + 1, iz + 1);
                work[static_cast<size_t>(ix + N * (iy + N * iz))] = std::complex<float>(x0[static_cast<size_t>(idx)], 0.0f);
            }
        }
    }
    FluidFFT::FFT3D(work, N, false);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int iz = 0; iz < N; ++iz) {
        for (int iy = 0; iy < N; ++iy) {
            for (int ix = 0; ix < N; ++ix) {
                const float lam = FluidFFT::LaplacianEigenvaluePeriodic(ix, iy, iz, N);
                const float m = std::exp(nu * dt * lam);
                const size_t o = static_cast<size_t>(ix + N * (iy + N * iz));
                work[o] *= m;
            }
        }
    }
    FluidFFT::FFT3D(work, N, true);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int iz = 0; iz < N; ++iz) {
        for (int iy = 0; iy < N; ++iy) {
            for (int ix = 0; ix < N; ++ix) {
                const int idx = g.IX(ix + 1, iy + 1, iz + 1);
                x[static_cast<size_t>(idx)] = SanitizeFinite(work[static_cast<size_t>(ix + N * (iy + N * iz))].real());
            }
        }
    }
    SetBoundaries(b, x, g);
}

void NSSolver::Diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt, const GridLayout& g) {
    if (g.LegacyIsotropicCube()) {
        const float a = dt * diff * static_cast<float>(g.Nx * g.Ny * g.Nz);
        LinearSolveIsotropic(b, x, x0, a, 1.0f + 6.0f * a, g);
    } else {
        const float ax = dt * diff / (g.hx * g.hx);
        const float ay = dt * diff / (g.hy * g.hy);
        const float az = dt * diff / (g.hz * g.hz);
        LinearSolveAnisotropicDiffuse(b, x, x0, ax, ay, az, g);
    }
}

void NSSolver::Project(std::vector<float>& velocX, std::vector<float>& velocY, std::vector<float>& velocZ, std::vector<float>& p,
                       std::vector<float>& div, const GridLayout& g) {
    if (!g.LegacyIsotropicCube()) {
        const float i2hx = 0.5f / g.hx;
        const float i2hy = 0.5f / g.hy;
        const float i2hz = 0.5f / g.hz;
        const int sx = g.sx();
        const int sxy = g.sxy();
        const Vec4 vnegHx = Vec4Splat(-i2hx);
        const Vec4 vnegHy = Vec4Splat(-i2hy);
        const Vec4 vnegHz = Vec4Splat(-i2hz);
        const Vec4 vhalfHx = Vec4Splat(i2hx);
        const Vec4 vhalfHy = Vec4Splat(i2hy);
        const Vec4 vhalfHz = Vec4Splat(i2hz);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                int i = 1;
                for (; i <= g.Nx - 3; i += 4) {
                    const int idx = g.IX(i, j, k);
                    const Vec4 u_next = Vec4::Load(&velocX[static_cast<size_t>(idx + 1)]);
                    const Vec4 u_prev = Vec4::Load(&velocX[static_cast<size_t>(idx - 1)]);
                    const Vec4 v_next = Vec4::Load(&velocY[static_cast<size_t>(idx + sx)]);
                    const Vec4 v_prev = Vec4::Load(&velocY[static_cast<size_t>(idx - sx)]);
                    const Vec4 w_next = Vec4::Load(&velocZ[static_cast<size_t>(idx + sxy)]);
                    const Vec4 w_prev = Vec4::Load(&velocZ[static_cast<size_t>(idx - sxy)]);
                    const Vec4 u_diff = u_next - u_prev;
                    const Vec4 v_diff = v_next - v_prev;
                    const Vec4 w_diff = w_next - w_prev;
                    const Vec4 divergence = u_diff * vnegHx + v_diff * vnegHy + w_diff * vnegHz;
                    float dtmp[4];
                    divergence.Store(dtmp);
                    for (int q = 0; q < 4; ++q) {
                        const size_t iq = static_cast<size_t>(idx + q);
                        div[iq] = SanitizeFinite(dtmp[q]);
                        p[iq] = 0.0f;
                    }
                }
                for (; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    div[static_cast<size_t>(idx)] =
                        -((velocX[static_cast<size_t>(g.IX(i + 1, j, k))] - velocX[static_cast<size_t>(g.IX(i - 1, j, k))]) * i2hx
                          + (velocY[static_cast<size_t>(g.IX(i, j + 1, k))] - velocY[static_cast<size_t>(g.IX(i, j - 1, k))]) * i2hy
                          + (velocZ[static_cast<size_t>(g.IX(i, j, k + 1))] - velocZ[static_cast<size_t>(g.IX(i, j, k - 1))]) * i2hz);
                    div[static_cast<size_t>(idx)] = SanitizeFinite(div[static_cast<size_t>(idx)]);
                    p[static_cast<size_t>(idx)] = 0.0f;
                }
            }
        }
        SetBoundaries(0, div, g);
        SetBoundaries(0, p, g);
        LinearSolveAnisotropicPressure(p, div, g);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int k = 1; k <= g.Nz; ++k) {
            for (int j = 1; j <= g.Ny; ++j) {
                int i = 1;
                for (; i <= g.Nx - 3; i += 4) {
                    const int idx = g.IX(i, j, k);
                    Vec4 velX = Vec4::Load(&velocX[static_cast<size_t>(idx)]);
                    Vec4 velY = Vec4::Load(&velocY[static_cast<size_t>(idx)]);
                    Vec4 velZ = Vec4::Load(&velocZ[static_cast<size_t>(idx)]);
                    const Vec4 p_next_x = Vec4::Load(&p[static_cast<size_t>(idx + 1)]);
                    const Vec4 p_prev_x = Vec4::Load(&p[static_cast<size_t>(idx - 1)]);
                    const Vec4 p_next_y = Vec4::Load(&p[static_cast<size_t>(g.IX(i, j + 1, k))]);
                    const Vec4 p_prev_y = Vec4::Load(&p[static_cast<size_t>(g.IX(i, j - 1, k))]);
                    const Vec4 p_next_z = Vec4::Load(&p[static_cast<size_t>(g.IX(i, j, k + 1))]);
                    const Vec4 p_prev_z = Vec4::Load(&p[static_cast<size_t>(g.IX(i, j, k - 1))]);
                    const Vec4 gX = (p_next_x - p_prev_x) * vhalfHx;
                    const Vec4 gY = (p_next_y - p_prev_y) * vhalfHy;
                    const Vec4 gZ = (p_next_z - p_prev_z) * vhalfHz;
                    float sxv[4];
                    float syv[4];
                    float szv[4];
                    (velX - gX).Store(sxv);
                    (velY - gY).Store(syv);
                    (velZ - gZ).Store(szv);
                    for (int q = 0; q < 4; ++q) {
                        const size_t iq = static_cast<size_t>(idx + q);
                        velocX[iq] = SanitizeFinite(sxv[q]);
                        velocY[iq] = SanitizeFinite(syv[q]);
                        velocZ[iq] = SanitizeFinite(szv[q]);
                    }
                }
                for (; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    velocX[static_cast<size_t>(idx)] -=
                        (p[static_cast<size_t>(g.IX(i + 1, j, k))] - p[static_cast<size_t>(g.IX(i - 1, j, k))]) * i2hx;
                    velocY[static_cast<size_t>(idx)] -=
                        (p[static_cast<size_t>(g.IX(i, j + 1, k))] - p[static_cast<size_t>(g.IX(i, j - 1, k))]) * i2hy;
                    velocZ[static_cast<size_t>(idx)] -=
                        (p[static_cast<size_t>(g.IX(i, j, k + 1))] - p[static_cast<size_t>(g.IX(i, j, k - 1))]) * i2hz;
                    velocX[static_cast<size_t>(idx)] = SanitizeFinite(velocX[static_cast<size_t>(idx)]);
                    velocY[static_cast<size_t>(idx)] = SanitizeFinite(velocY[static_cast<size_t>(idx)]);
                    velocZ[static_cast<size_t>(idx)] = SanitizeFinite(velocZ[static_cast<size_t>(idx)]);
                }
            }
        }
        SetBoundaries(1, velocX, g);
        SetBoundaries(2, velocY, g);
        SetBoundaries(3, velocZ, g);
        return;
    }

    const int N = g.Nx;
    const float invN = 1.0f / static_cast<float>(N);
    const float negHalfInvN = -0.5f * invN;
    const float halfN = 0.5f * static_cast<float>(N);

#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int m = 1; m <= N; m++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                const int idx = g.IX(i, j, m);
                Vec4 u_next = Vec4::Load(&velocX[idx + 1]);
                Vec4 u_prev = Vec4::Load(&velocX[idx - 1]);
                Vec4 v_next = Vec4::Load(&velocY[g.IX(i, j + 1, m)]);
                Vec4 v_prev = Vec4::Load(&velocY[g.IX(i, j - 1, m)]);
                Vec4 w_next = Vec4::Load(&velocZ[g.IX(i, j, m + 1)]);
                Vec4 w_prev = Vec4::Load(&velocZ[g.IX(i, j, m - 1)]);
                Vec4 u_diff = u_next - u_prev;
                Vec4 v_diff = v_next - v_prev;
                Vec4 w_diff = w_next - w_prev;
                Vec4 divergence = (u_diff + v_diff + w_diff) * negHalfInvN;
                divergence.Store(&div[idx]);
                Vec4 zero(0, 0, 0, 0);
                zero.Store(&p[idx]);
            }
            for (; i <= N; i++) {
                const int idx = g.IX(i, j, m);
                div[idx] = -0.5f * (
                             velocX[g.IX(i+1, j, m)] - velocX[g.IX(i-1, j, m)]
                           + velocY[g.IX(i, j+1, m)] - velocY[g.IX(i, j-1, m)]
                           + velocZ[g.IX(i, j, m+1)] - velocZ[g.IX(i, j, m-1)]
                        ) * invN;
                div[idx] = SanitizeFinite(div[idx]);
                p[idx] = 0.0f;
            }
        }
    }

    SetBoundaries(0, div, g);
    SetBoundaries(0, p, g);

    LinearSolveIsotropic(0, p, div, 1.0f, 6.0f, g);

#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int m = 1; m <= N; m++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                const int idx = g.IX(i, j, m);
                Vec4 velX = Vec4::Load(&velocX[idx]);
                Vec4 velY = Vec4::Load(&velocY[idx]);
                Vec4 velZ = Vec4::Load(&velocZ[idx]);
                Vec4 p_next_x = Vec4::Load(&p[idx + 1]);
                Vec4 p_prev_x = Vec4::Load(&p[idx - 1]);
                Vec4 p_next_y = Vec4::Load(&p[g.IX(i, j + 1, m)]);
                Vec4 p_prev_y = Vec4::Load(&p[g.IX(i, j - 1, m)]);
                Vec4 p_next_z = Vec4::Load(&p[g.IX(i, j, m + 1)]);
                Vec4 p_prev_z = Vec4::Load(&p[g.IX(i, j, m - 1)]);
                Vec4 factor(halfN, halfN, halfN, halfN);
                Vec4 gX = (p_next_x - p_prev_x) * factor;
                Vec4 gY = (p_next_y - p_prev_y) * factor;
                Vec4 gZ = (p_next_z - p_prev_z) * factor;
                (velX - gX).Store(&velocX[idx]);
                (velY - gY).Store(&velocY[idx]);
                (velZ - gZ).Store(&velocZ[idx]);
            }
            for (; i <= N; i++) {
                const int idx = g.IX(i, j, m);
                velocX[idx] -= (p[g.IX(i+1, j, m)] - p[g.IX(i-1, j, m)]) * halfN;
                velocY[idx] -= (p[g.IX(i, j+1, m)] - p[g.IX(i, j-1, m)]) * halfN;
                velocZ[idx] -= (p[g.IX(i, j, m+1)] - p[g.IX(i, j, m-1)]) * halfN;
                velocX[idx] = SanitizeFinite(velocX[idx]);
                velocY[idx] = SanitizeFinite(velocY[idx]);
                velocZ[idx] = SanitizeFinite(velocZ[idx]);
            }
        }
    }

    SetBoundaries(1, velocX, g);
    SetBoundaries(2, velocY, g);
    SetBoundaries(3, velocZ, g);
}

void NSSolver::Advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v,
                      const std::vector<float>& w, float dt, const GridLayout& g) {
    AdvectPtr(b, d.data(), d0.data(), u.data(), v.data(), w.data(), dt, g);
}

void NSSolver::AdvectMacCormack(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u,
                                const std::vector<float>& v, const std::vector<float>& w, float dt, const GridLayout& g) {
    AdvectMacCormackPtr(b, d.data(), d0.data(), u.data(), v.data(), w.data(), dt, g);
}

void NSSolver::AdvectRaymarch(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u,
                              const std::vector<float>& v, const std::vector<float>& w, float dt, const GridLayout& g,
                              const FluidSparseData* sparse) {
    if (!g.LegacyIsotropicCube()) {
        const float dtx = dt / std::max(g.hx, 1e-12f);
        const float dty = dt / std::max(g.hy, 1e-12f);
        const float dtz = dt / std::max(g.hz, 1e-12f);
        const Vec4 vdtx = Vec4Splat(dtx);
        const Vec4 vdty = Vec4Splat(dty);
        const Vec4 vdtz = Vec4Splat(dtz);
        const float lox = 0.5f;
        const float hix = static_cast<float>(g.Nx) + 0.5f;
        const float loy = 0.5f;
        const float hiy = static_cast<float>(g.Ny) + 0.5f;
        const float loz = 0.5f;
        const float hiz = static_cast<float>(g.Nz) + 0.5f;
        constexpr int kRaySteps = 8;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int k = 1; k <= g.Nz; k++) {
            for (int j = 1; j <= g.Ny; j++) {
                int i = 1;
                for (; i <= g.Nx - 3; i += 4) {
                    const int idx = g.IX(i, j, k);
                    bool skip4[4];
                    for (int q = 0; q < 4; ++q) {
                        skip4[q] = SparseCellSkip(sparse, i + q, j, k);
                    }
                    const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                    const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                    const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                    float rdx[4];
                    float rdy[4];
                    float rdz[4];
                    (Vec4Splat(0.0f) - vu * vdtx).Store(rdx);
                    (Vec4Splat(0.0f) - vv * vdty).Store(rdy);
                    (Vec4Splat(0.0f) - vw * vdtz).Store(rdz);
                    const float x0 = static_cast<float>(i);
                    const float y0 = static_cast<float>(j);
                    const float z0 = static_cast<float>(k);
                    for (int q = 0; q < 4; ++q) {
                        if (skip4[q]) {
                            d[static_cast<size_t>(idx + q)] = d0[static_cast<size_t>(idx + q)];
                            continue;
                        }
                        float acc = 0.0f;
                        for (int s = 0; s < kRaySteps; ++s) {
                            const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(kRaySteps);
                            const float px = ClampCellCoord(x0 + static_cast<float>(q) + rdx[q] * t, lox, hix);
                            const float py = ClampCellCoord(y0 + rdy[q] * t, loy, hiy);
                            const float pz = ClampCellCoord(z0 + rdz[q] * t, loz, hiz);
                            acc += SampleTrilinearField(d0, px, py, pz, g);
                        }
                        d[static_cast<size_t>(idx + q)] = SanitizeFinite(acc / static_cast<float>(kRaySteps));
                    }
                }
                for (; i <= g.Nx; ++i) {
                    const int idx = g.IX(i, j, k);
                    if (SparseCellSkip(sparse, i, j, k)) {
                        d[static_cast<size_t>(idx)] = d0[static_cast<size_t>(idx)];
                        continue;
                    }
                    const float xf = static_cast<float>(i);
                    const float yf = static_cast<float>(j);
                    const float zf = static_cast<float>(k);
                    const float rdx = -u[static_cast<size_t>(idx)] * dtx;
                    const float rdy = -v[static_cast<size_t>(idx)] * dty;
                    const float rdz = -w[static_cast<size_t>(idx)] * dtz;
                    float acc = 0.0f;
                    for (int s = 0; s < kRaySteps; ++s) {
                        const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(kRaySteps);
                        const float px = ClampCellCoord(xf + rdx * t, lox, hix);
                        const float py = ClampCellCoord(yf + rdy * t, loy, hiy);
                        const float pz = ClampCellCoord(zf + rdz * t, loz, hiz);
                        acc += SampleTrilinearField(d0, px, py, pz, g);
                    }
                    d[static_cast<size_t>(idx)] = SanitizeFinite(acc / static_cast<float>(kRaySteps));
                }
            }
        }
        SetBoundaries(b, d, g);
        return;
    }
    const int N = g.Nx;
    const float dt0 = dt * static_cast<float>(N);
    const Vec4 vdt = Vec4Splat(dt0);
    constexpr int kRaySteps = 8;
    const float clampLo = 0.5f;
    const float clampHi = static_cast<float>(N) + 0.5f;
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= N; k++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                bool skip4[4];
                for (int q = 0; q < 4; ++q) {
                    skip4[q] = SparseCellSkip(sparse, i + q, j, k);
                }
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float dx[4];
                float dy[4];
                float dz[4];
                (Vec4Splat(0.0f) - vu * vdt).Store(dx);
                (Vec4Splat(0.0f) - vv * vdt).Store(dy);
                (Vec4Splat(0.0f) - vw * vdt).Store(dz);
                const float x0 = static_cast<float>(i);
                const float y0 = static_cast<float>(j);
                const float z0 = static_cast<float>(k);
                for (int q = 0; q < 4; ++q) {
                    if (skip4[q]) {
                        d[static_cast<size_t>(idx + q)] = d0[static_cast<size_t>(idx + q)];
                        continue;
                    }
                    float acc = 0.0f;
                    for (int s = 0; s < kRaySteps; ++s) {
                        const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(kRaySteps);
                        const float px = ClampCellCoord(x0 + static_cast<float>(q) + dx[q] * t, clampLo, clampHi);
                        const float py = ClampCellCoord(y0 + dy[q] * t, clampLo, clampHi);
                        const float pz = ClampCellCoord(z0 + dz[q] * t, clampLo, clampHi);
                        acc += SampleTrilinearField(d0, px, py, pz, g);
                    }
                    d[static_cast<size_t>(idx + q)] = SanitizeFinite(acc / static_cast<float>(kRaySteps));
                }
            }
            for (; i <= N; ++i) {
                const int idx = g.IX(i, j, k);
                if (SparseCellSkip(sparse, i, j, k)) {
                    d[static_cast<size_t>(idx)] = d0[static_cast<size_t>(idx)];
                    continue;
                }
                const float xf = static_cast<float>(i);
                const float yf = static_cast<float>(j);
                const float zf = static_cast<float>(k);
                const float rdx = -dt0 * u[static_cast<size_t>(idx)];
                const float rdy = -dt0 * v[static_cast<size_t>(idx)];
                const float rdz = -dt0 * w[static_cast<size_t>(idx)];
                float acc = 0.0f;
                for (int s = 0; s < kRaySteps; ++s) {
                    const float t = (static_cast<float>(s) + 0.5f) / static_cast<float>(kRaySteps);
                    const float px = ClampCellCoord(xf + rdx * t, clampLo, clampHi);
                    const float py = ClampCellCoord(yf + rdy * t, clampLo, clampHi);
                    const float pz = ClampCellCoord(zf + rdz * t, clampLo, clampHi);
                    acc += SampleTrilinearField(d0, px, py, pz, g);
                }
                d[static_cast<size_t>(idx)] = SanitizeFinite(acc / static_cast<float>(kRaySteps));
            }
        }
    }
    SetBoundaries(b, d, g);
}

void NSSolver::ApplyTemporalReprojection(std::vector<float>& density, const std::vector<float>& history, const std::vector<float>& u,
                                         const std::vector<float>& v, const std::vector<float>& w, float dt, const GridLayout& g,
                                         float alpha) {
    const float dtx = g.LegacyIsotropicCube() ? dt * static_cast<float>(g.Nx) : dt / std::max(g.hx, 1e-12f);
    const float dty = g.LegacyIsotropicCube() ? dt * static_cast<float>(g.Ny) : dt / std::max(g.hy, 1e-12f);
    const float dtz = g.LegacyIsotropicCube() ? dt * static_cast<float>(g.Nz) : dt / std::max(g.hz, 1e-12f);
    const float lx = 0.5f;
    const float hxb = static_cast<float>(g.Nx) + 0.5f;
    const float ly = 0.5f;
    const float hyb = static_cast<float>(g.Ny) + 0.5f;
    const float lz = 0.5f;
    const float hzb = static_cast<float>(g.Nz) + 0.5f;
    std::vector<float> tmp(density.size());
    const Vec4 vdtx = Vec4Splat(dtx);
    const Vec4 vdty = Vec4Splat(dty);
    const Vec4 vdtz = Vec4Splat(dtz);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= g.Nz; k++) {
        for (int j = 1; j <= g.Ny; j++) {
            int i = 1;
            for (; i <= g.Nx - 3; i += 4) {
                const int idx = g.IX(i, j, k);
                const Vec4 vu = Vec4::Load(&u[static_cast<size_t>(idx)]);
                const Vec4 vv = Vec4::Load(&v[static_cast<size_t>(idx)]);
                const Vec4 vw = Vec4::Load(&w[static_cast<size_t>(idx)]);
                float px[4];
                float py[4];
                float pz[4];
                (Vec4Iota(static_cast<float>(i)) - vu * vdtx).Store(px);
                (Vec4Splat(static_cast<float>(j)) - vv * vdty).Store(py);
                (Vec4Splat(static_cast<float>(k)) - vw * vdtz).Store(pz);
                ClampCellCoordArray4(px, lx, hxb);
                ClampCellCoordArray4(py, ly, hyb);
                ClampCellCoordArray4(pz, lz, hzb);
                for (int q = 0; q < 4; ++q) {
                    tmp[static_cast<size_t>(idx + q)] = SampleTrilinearField(history, px[q], py[q], pz[q], g);
                }
            }
            for (; i <= g.Nx; ++i) {
                const int idx = g.IX(i, j, k);
                const float x = ClampCellCoord(static_cast<float>(i) - u[static_cast<size_t>(idx)] * dtx, lx, hxb);
                const float y = ClampCellCoord(static_cast<float>(j) - v[static_cast<size_t>(idx)] * dty, ly, hyb);
                const float z = ClampCellCoord(static_cast<float>(k) - w[static_cast<size_t>(idx)] * dtz, lz, hzb);
                const float h = SampleTrilinearField(history, x, y, z, g);
                tmp[static_cast<size_t>(idx)] = h;
            }
        }
    }
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= g.Nz; k++) {
        for (int j = 1; j <= g.Ny; j++) {
            for (int i = 1; i <= g.Nx; i++) {
                const int idx = g.IX(i, j, k);
                const float mag = std::abs(u[static_cast<size_t>(idx)]) + std::abs(v[static_cast<size_t>(idx)])
                    + std::abs(w[static_cast<size_t>(idx)]);
                const float wgt = std::clamp(alpha * (1.0f + 0.1f * mag), 0.0f, 0.65f);
                density[static_cast<size_t>(idx)] =
                    SanitizeFinite((1.0f - wgt) * density[static_cast<size_t>(idx)] + wgt * tmp[static_cast<size_t>(idx)]);
            }
        }
    }
}

void NSSolver::BitplaneStep(FluidSimulation& fluid, float dt, const GridLayout& g) {
    if (g.Nx <= 0 || g.Ny <= 0 || g.Nz <= 0) {
        return;
    }
    const int Nx = g.Nx;
    const int Ny = g.Ny;
    const int Nz = g.Nz;
    const float coupling = fluid.tuning.bitplaneCouplingStrength;
    auto& packed = fluid.packed;
    auto& prev = fluid.packed_prev;
    CopyPodVector(prev, packed);

#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= Nz; k++) {
        for (int j = 1; j <= Ny; j++) {
            for (int i = 1; i <= Nx; i++) {
                const int idx = g.IX(i, j, k);
                uint32_t w = prev[static_cast<size_t>(idx)];
                float c[4];
                for (int ch = 0; ch < 4; ++ch) {
                    c[ch] = static_cast<float>((w >> (8 * ch)) & 0xFFu) / 255.0f;
                }
                float mean[4] = {0, 0, 0, 0};
                int cnt = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0 && dz == 0) {
                                continue;
                            }
                            const int o = g.IX(i + dx, j + dy, k + dz);
                            const uint32_t ow = prev[static_cast<size_t>(o)];
                            for (int ch = 0; ch < 4; ++ch) {
                                mean[ch] += static_cast<float>((ow >> (8 * ch)) & 0xFFu) / 255.0f;
                            }
                            ++cnt;
                        }
                    }
                }
                if (cnt > 0) {
                    for (int ch = 0; ch < 4; ++ch) {
                        mean[ch] /= static_cast<float>(cnt);
                        c[ch] = std::clamp(c[ch] + coupling * (mean[ch] - c[ch]), 0.0f, 1.0f);
                    }
                }
                uint32_t out = 0;
                for (int ch = 0; ch < 4; ++ch) {
                    const uint32_t uu = static_cast<uint32_t>(std::lround(c[ch] * 255.0f));
                    out |= (uu & 0xFFu) << (8 * ch);
                }
                packed[static_cast<size_t>(idx)] = out;
            }
        }
    }

    const bool mac = fluid.tuning.enableMacCormack;
    const size_t gridBytes = fluid.Vx.size() * sizeof(float);
    assert(fluid.bitplaneFieldScratch.Size() == fluid.Vx.size());
    assert(fluid.bitplanePrevScratch.Size() == fluid.Vx.size());
    float* chField = fluid.bitplaneFieldScratch.Data();
    float* ch0 = fluid.bitplanePrevScratch.Data();

    for (int ch = 0; ch < 4; ++ch) {
        std::memset(chField, 0, gridBytes);
        std::memset(ch0, 0, gridBytes);
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int k = 1; k <= Nz; k++) {
            for (int j = 1; j <= Ny; j++) {
                for (int i = 1; i <= Nx; i++) {
                    const int idx = g.IX(i, j, k);
                    const uint32_t w = packed[static_cast<size_t>(idx)];
                    const float val = static_cast<float>((w >> (8 * ch)) & 0xFFu) / 255.0f;
                    chField[idx] = val;
                    ch0[idx] = val;
                }
            }
        }
        if (mac) {
            AdvectMacCormackPtr(0, chField, ch0, fluid.Vx.data(), fluid.Vy.data(), fluid.Vz.data(), dt, g);
        } else {
            AdvectPtr(0, chField, ch0, fluid.Vx.data(), fluid.Vy.data(), fluid.Vz.data(), dt, g);
        }
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int k = 1; k <= Nz; k++) {
            for (int j = 1; j <= Ny; j++) {
                for (int i = 1; i <= Nx; i++) {
                    const int idx = g.IX(i, j, k);
                    const float val = std::clamp(chField[idx], 0.0f, 1.0f);
                    uint32_t w = packed[static_cast<size_t>(idx)];
                    const uint32_t uu = static_cast<uint32_t>(std::lround(val * 255.0f));
                    w = (w & ~(0xFFu << (8 * ch))) | ((uu & 0xFFu) << (8 * ch));
                    packed[static_cast<size_t>(idx)] = w;
                }
            }
        }
    }
}

void NSSolver::ApplyBoussinesq(FluidSimulation& fluid, float dt, const GridLayout& g) {
    const FluidThermalTuning& th = fluid.thermal;
    if (!th.enableBoussinesq) {
        return;
    }
    const float B = th.buoyancyStrength;
    const float Tref = th.TReference;
    const int ax = th.buoyancyAxis;
    float* ux = fluid.Vx0.data();
    float* uy = fluid.Vy0.data();
    float* uz = fluid.Vz0.data();
    const float* Tp = fluid.temperature.data();
#ifdef SOLSTICE_HAS_OPENMP
#pragma omp parallel for schedule(static)
#endif
    for (int k = 1; k <= g.Nz; ++k) {
        for (int j = 1; j <= g.Ny; ++j) {
            for (int i = 1; i <= g.Nx; ++i) {
                const int idx = g.IX(i, j, k);
                const float dT = Tp[static_cast<size_t>(idx)] - Tref;
                const float f = dt * B * dT;
                if (ax == 0) {
                    ux[static_cast<size_t>(idx)] += f;
                } else if (ax == 1) {
                    uy[static_cast<size_t>(idx)] += f;
                } else {
                    uz[static_cast<size_t>(idx)] += f;
                }
            }
        }
    }
}

void NSSolver::SetThermalWallBoundaries(std::vector<float>& T, const GridLayout& g, const Math::Vec3& gridOrigin,
                                        const FluidThermalTuning& th, float tHotEffective, float tColdEffective) {
    SetThermalWallBoundariesPtr(T.data(), g, gridOrigin, th, tHotEffective, tColdEffective);
}

void NSSolver::Resolve(FluidSimulation& fluid, float dt) {
    const GridLayout grid = fluid.GetGridLayout();
    if (grid.Nx <= 0 || grid.Ny <= 0 || grid.Nz <= 0 || !std::isfinite(dt) || dt <= 0.0f) {
        return;
    }
    fluid.simulationTime += static_cast<double>(dt);

    const float visc = fluid.visc;
    const float diff = fluid.diff;
    auto& tuning = fluid.tuning;
    auto& thermal = fluid.thermal;

    auto& Vx = fluid.Vx;
    auto& Vy = fluid.Vy;
    auto& Vz = fluid.Vz;
    auto& Vx0 = fluid.Vx0;
    auto& Vy0 = fluid.Vy0;
    auto& Vz0 = fluid.Vz0;
    auto& dens = fluid.density;
    auto& density_prev = fluid.density_prev;
    auto& pressure = fluid.pressure;
    auto& divergence = fluid.divergence;
    auto& density_history = fluid.density_history;
    auto& temp = fluid.temperature;
    auto& temp_prev = fluid.temperature_prev;

    FluidSparseData* sparsePtr = fluid.sparse.get();
    if (sparsePtr && tuning.useSparseRaymarch) {
        sparsePtr->Resize(grid.Nx, grid.Ny, grid.Nz, tuning.sparseBlockSize);
        sparsePtr->BuildFromFields(dens, Vx, Vy, Vz, tuning.occupancyThreshold, grid);
    }

    const bool spectralOk =
        grid.LegacyIsotropicCube() && tuning.useSpectralDiffusion && tuning.periodicSpectralDiffusion && FluidFFT::IsPowerOf2(grid.Nx);
    const bool spectralVel = spectralOk;
    const bool spectralDen = spectralOk;

    if (spectralVel) {
        DiffuseSpectralPeriodic(1, Vx0, Vx, visc, dt, grid);
        DiffuseSpectralPeriodic(2, Vy0, Vy, visc, dt, grid);
        DiffuseSpectralPeriodic(3, Vz0, Vz, visc, dt, grid);
    } else {
        Diffuse(1, Vx0, Vx, visc, dt, grid);
        Diffuse(2, Vy0, Vy, visc, dt, grid);
        Diffuse(3, Vz0, Vz, visc, dt, grid);
    }

    Project(Vx0, Vy0, Vz0, pressure, divergence, grid);
    ApplyBoussinesq(fluid, dt, grid);

    const bool raymarch = tuning.useSparseRaymarch;
    const FluidSparseData* sparseConst = sparsePtr;

    if (raymarch) {
        AdvectRaymarch(1, Vx, Vx0, Vx0, Vy0, Vz0, dt, grid, sparseConst);
        AdvectRaymarch(2, Vy, Vy0, Vx0, Vy0, Vz0, dt, grid, sparseConst);
        AdvectRaymarch(3, Vz, Vz0, Vx0, Vy0, Vz0, dt, grid, sparseConst);
    } else if (tuning.enableMacCormack) {
        AdvectMacCormack(1, Vx, Vx0, Vx0, Vy0, Vz0, dt, grid);
        AdvectMacCormack(2, Vy, Vy0, Vx0, Vy0, Vz0, dt, grid);
        AdvectMacCormack(3, Vz, Vz0, Vx0, Vy0, Vz0, dt, grid);
    } else {
        Advect(1, Vx, Vx0, Vx0, Vy0, Vz0, dt, grid);
        Advect(2, Vy, Vy0, Vx0, Vy0, Vz0, dt, grid);
        Advect(3, Vz, Vz0, Vx0, Vy0, Vz0, dt, grid);
    }

    Project(Vx, Vy, Vz, pressure, divergence, grid);

    if (thermal.enableBoussinesq) {
        const float pr = std::max(thermal.Prandtl, 1e-6f);
        const float kappa = visc / pr;
        float tHotEff = thermal.THot;
        if (thermal.timeVaryingForcing) {
            const double w =
                fluid.simulationTime * 6.283185307179586476925286766559 * static_cast<double>(thermal.bottomTempOscillationHz);
            tHotEff += thermal.bottomTempOscillationAmplitude * static_cast<float>(std::sin(w));
        }
        if (grid.LegacyIsotropicCube()) {
            const float a = dt * kappa * static_cast<float>(grid.Nx * grid.Ny * grid.Nz);
            LinearSolveIsotropicThermal(temp_prev, temp, a, 1.0f + 6.0f * a, grid, fluid.GetGridOrigin(), thermal, tHotEff,
                                        thermal.TCold);
        } else {
            const float ax = dt * kappa / (grid.hx * grid.hx);
            const float ay = dt * kappa / (grid.hy * grid.hy);
            const float az = dt * kappa / (grid.hz * grid.hz);
            LinearSolveAnisotropicDiffuseThermal(temp_prev, temp, ax, ay, az, grid, fluid.GetGridOrigin(), thermal, tHotEff,
                                                 thermal.TCold);
        }
        if (raymarch) {
            AdvectRaymarch(0, temp, temp_prev, Vx, Vy, Vz, dt, grid, sparseConst);
        } else if (tuning.enableMacCormack) {
            AdvectMacCormack(0, temp, temp_prev, Vx, Vy, Vz, dt, grid);
        } else {
            Advect(0, temp, temp_prev, Vx, Vy, Vz, dt, grid);
        }
        SetThermalWallBoundaries(temp, grid, fluid.GetGridOrigin(), thermal, tHotEff, thermal.TCold);
    }

    if (spectralDen) {
        DiffuseSpectralPeriodic(0, density_prev, dens, diff, dt, grid);
    } else {
        Diffuse(0, density_prev, dens, diff, dt, grid);
    }

    if (raymarch) {
        AdvectRaymarch(0, dens, density_prev, Vx, Vy, Vz, dt, grid, sparseConst);
    } else if (tuning.enableMacCormack) {
        AdvectMacCormack(0, dens, density_prev, Vx, Vy, Vz, dt, grid);
    } else {
        Advect(0, dens, density_prev, Vx, Vy, Vz, dt, grid);
    }

    if (fluid.densityHistoryValid && tuning.temporalReprojectWeight > 0.0f) {
        ApplyTemporalReprojection(dens, density_history, Vx, Vy, Vz, dt, grid, tuning.temporalReprojectWeight);
    }

    BitplaneStep(fluid, dt, grid);

    CopyPodVector(density_history, dens);
    fluid.densityHistoryValid = true;

    SetBoundaries(0, dens, grid);
}

void NSSolver::SetBoundaries(int b, std::vector<float>& x, const GridLayout& g) {
    SetBoundariesPtrImpl(b, x.data(), g);
}

} // namespace Solstice::Physics
