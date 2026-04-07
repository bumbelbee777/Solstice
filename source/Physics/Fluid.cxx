#include "Fluid.hxx"
#include "FluidSparse.hxx"
#include "NSSolver.hxx"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace Solstice::Physics {

namespace {
constexpr size_t kScratchAlign = 64;
inline size_t RoundUpAligned(size_t n, size_t a) { return (n + a - 1) / a * a; }
} // namespace

AlignedFloatScratchBuffer::~AlignedFloatScratchBuffer() {
    Release();
}

AlignedFloatScratchBuffer::AlignedFloatScratchBuffer(AlignedFloatScratchBuffer&& o) noexcept
    : block_(o.block_), ptr_(o.ptr_), count_(o.count_) {
    o.block_ = nullptr;
    o.ptr_ = nullptr;
    o.count_ = 0;
}

AlignedFloatScratchBuffer& AlignedFloatScratchBuffer::operator=(AlignedFloatScratchBuffer&& o) noexcept {
    if (this != &o) {
        Release();
        block_ = o.block_;
        ptr_ = o.ptr_;
        count_ = o.count_;
        o.block_ = nullptr;
        o.ptr_ = nullptr;
        o.count_ = 0;
    }
    return *this;
}

void AlignedFloatScratchBuffer::Release() {
#if defined(_WIN32)
    if (block_) {
        _aligned_free(block_);
    }
#else
    if (block_) {
        std::free(block_);
    }
#endif
    block_ = nullptr;
    ptr_ = nullptr;
    count_ = 0;
}

void AlignedFloatScratchBuffer::Resize(size_t floatCount) {
    Release();
    count_ = floatCount;
    if (count_ == 0) {
        return;
    }
    const size_t bytes = count_ * sizeof(float);
#if defined(_WIN32)
    block_ = _aligned_malloc(bytes, kScratchAlign);
    ptr_ = block_ ? static_cast<float*>(block_) : nullptr;
#else
    const size_t padded = RoundUpAligned(bytes, kScratchAlign);
    block_ = std::aligned_alloc(kScratchAlign, padded);
    ptr_ = block_ ? static_cast<float*>(block_) : nullptr;
#endif
    if (ptr_) {
        std::memset(ptr_, 0, bytes);
    }
}

FluidSimulation::FluidSimulation(int size, float diffusion, float viscosity)
    : FluidSimulation(size, size, size, 1.0f / static_cast<float>(size > 0 ? size : 1),
                      1.0f / static_cast<float>(size > 0 ? size : 1), 1.0f / static_cast<float>(size > 0 ? size : 1),
                      diffusion, viscosity) {}

FluidSimulation::FluidSimulation(int nx, int ny, int nz, float cellHx, float cellHy, float cellHz, float diffusion,
                                 float viscosity)
    : Nx(nx), Ny(ny), Nz(nz), hx(cellHx), hy(cellHy), hz(cellHz), diff(diffusion), visc(viscosity) {
    if (Nx <= 0 || Ny <= 0 || Nz <= 0) {
        Nx = Ny = Nz = 0;
        return;
    }
    const GridLayout lay = GetGridLayout();
    const size_t total = lay.CellCountPadded();
    if (Nx >= 64 || Ny >= 64 || Nz >= 64) {
        tuning.useSparseRaymarch = true;
        tuning.sparseBlockSize = 8;
        tuning.occupancyThreshold = 1e-5f;
    }
    density.resize(total, 0.0f);
    density_prev.resize(total, 0.0f);
    density_history.resize(total, 0.0f);
    temperature.resize(total, 0.0f);
    temperature_prev.resize(total, 0.0f);
    Vx.resize(total, 0.0f);
    Vy.resize(total, 0.0f);
    Vz.resize(total, 0.0f);
    Vx0.resize(total, 0.0f);
    Vy0.resize(total, 0.0f);
    Vz0.resize(total, 0.0f);
    pressure.resize(total, 0.0f);
    divergence.resize(total, 0.0f);
    packed.assign(total, 0u);
    packed_prev.assign(total, 0u);
    sparse = std::make_unique<FluidSparseData>();
    sparse->Resize(Nx, Ny, Nz, tuning.sparseBlockSize);
    bitplaneFieldScratch.Resize(total);
    bitplanePrevScratch.Resize(total);
}

FluidSimulation::~FluidSimulation() = default;

void FluidSimulation::ApplyRayleighPrParameters(float rayleigh, float prandtl, float deltaT, float channelHeight) {
    if (!std::isfinite(rayleigh) || !std::isfinite(prandtl) || !std::isfinite(deltaT) || !std::isfinite(channelHeight)) {
        return;
    }
    const float Ra = std::max(rayleigh, 1e-6f);
    const float Pr = std::max(prandtl, 1e-6f);
    const float dT = std::max(std::abs(deltaT), 1e-8f);
    const float L = std::max(channelHeight, 1e-8f);
    const float B = std::max(std::abs(thermal.buoyancyStrength), 1e-12f);
    const float L3 = L * L * L;
    const float nuSq = Pr * B * dT * L3 / Ra;
    visc = std::sqrt(std::max(nuSq, 1e-12f));
    thermal.Prandtl = Pr;
}

void FluidSimulation::AddDensity(int x, int y, int z, float amount) {
    if (Nx <= 0) {
        return;
    }
    const GridLayout g = GetGridLayout();
    const int index = g.IX(x + 1, y + 1, z + 1);
    if (index >= 0 && index < static_cast<int>(density.size())) {
        density[static_cast<size_t>(index)] += amount;
    }
}

void FluidSimulation::AddVelocity(int x, int y, int z, float amountX, float amountY, float amountZ) {
    if (Nx <= 0) {
        return;
    }
    const GridLayout g = GetGridLayout();
    const int index = g.IX(x + 1, y + 1, z + 1);
    if (index >= 0 && index < static_cast<int>(Vx.size())) {
        Vx[static_cast<size_t>(index)] += amountX;
        Vy[static_cast<size_t>(index)] += amountY;
        Vz[static_cast<size_t>(index)] += amountZ;
    }
}

void FluidSimulation::SetPackedChannel(int x, int y, int z, int channel, uint8_t value) {
    if (channel < 0 || channel > 3 || Nx <= 0) {
        return;
    }
    const GridLayout g = GetGridLayout();
    const int index = g.IX(x + 1, y + 1, z + 1);
    if (index < 0 || index >= static_cast<int>(packed.size())) {
        return;
    }
    uint32_t& w = packed[static_cast<size_t>(index)];
    const uint32_t mask = ~(0xFFu << (8 * channel));
    w = (w & mask) | (static_cast<uint32_t>(value) << (8 * channel));
}

uint8_t FluidSimulation::GetPackedChannel(int x, int y, int z, int channel) const {
    if (channel < 0 || channel > 3 || Nx <= 0) {
        return 0;
    }
    const GridLayout g = GetGridLayout();
    const int index = g.IX(x + 1, y + 1, z + 1);
    if (index < 0 || index >= static_cast<int>(packed.size())) {
        return 0;
    }
    const uint32_t w = packed[static_cast<size_t>(index)];
    return static_cast<uint8_t>((w >> (8 * channel)) & 0xFFu);
}

void FluidSimulation::SetVolumeVisualizationClip(bool enable, const Math::Vec3& worldMin, const Math::Vec3& worldMax) {
    volumeVisClipEnable = enable;
    volumeVisClipMin.x = std::min(worldMin.x, worldMax.x);
    volumeVisClipMax.x = std::max(worldMin.x, worldMax.x);
    volumeVisClipMin.y = std::min(worldMin.y, worldMax.y);
    volumeVisClipMax.y = std::max(worldMin.y, worldMax.y);
    volumeVisClipMin.z = std::min(worldMin.z, worldMax.z);
    volumeVisClipMax.z = std::max(worldMin.z, worldMax.z);
}

void FluidSimulation::Step(float dt) {
    NSSolver::Resolve(*this, dt);
}

void FluidSimulation::ResetFlowFields() {
    if (Nx <= 0) {
        return;
    }
    std::fill(Vx.begin(), Vx.end(), 0.0f);
    std::fill(Vy.begin(), Vy.end(), 0.0f);
    std::fill(Vz.begin(), Vz.end(), 0.0f);
    std::fill(Vx0.begin(), Vx0.end(), 0.0f);
    std::fill(Vy0.begin(), Vy0.end(), 0.0f);
    std::fill(Vz0.begin(), Vz0.end(), 0.0f);
    std::fill(density.begin(), density.end(), 0.0f);
    std::fill(density_prev.begin(), density_prev.end(), 0.0f);
    std::fill(pressure.begin(), pressure.end(), 0.0f);
    std::fill(divergence.begin(), divergence.end(), 0.0f);
    densityHistoryValid = false;
}

void FluidSimulation::SetTemperatureLogical(int x, int y, int z, float value) {
    if (Nx <= 0 || x < 0 || y < 0 || z < 0 || x >= Nx || y >= Ny || z >= Nz) {
        return;
    }
    const GridLayout g = GetGridLayout();
    const int idx = g.IX(x + 1, y + 1, z + 1);
    if (idx < 0 || idx >= static_cast<int>(temperature.size())) {
        return;
    }
    const size_t u = static_cast<size_t>(idx);
    temperature[u] = value;
    temperature_prev[u] = value;
}

Math::Vec3 FluidSimulation::GetVelocityAt(int x, int y, int z) const {
    if (Nx <= 0) {
        return Math::Vec3(0, 0, 0);
    }
    const GridLayout g = GetGridLayout();
    const int index = g.IX(x + 1, y + 1, z + 1);
    if (index >= 0 && index < static_cast<int>(Vx.size())) {
        return Math::Vec3(Vx[static_cast<size_t>(index)], Vy[static_cast<size_t>(index)], Vz[static_cast<size_t>(index)]);
    }
    return Math::Vec3(0, 0, 0);
}

namespace {
inline float TrilinearSample(const std::vector<float>& field, float x, float y, float z, const GridLayout& g) {
    const int s = g.sx();
    const int sxy = g.sxy();
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
    auto ix = [s, sxy](int xi, int yi, int zi) { return xi + s * yi + sxy * zi; };
    return s0 * (t0 * (r0 * field[static_cast<size_t>(ix(i0, j0, k0))] + r1 * field[static_cast<size_t>(ix(i0, j0, k1))]) +
                 t1 * (r0 * field[static_cast<size_t>(ix(i0, j1, k0))] + r1 * field[static_cast<size_t>(ix(i0, j1, k1))])) +
           s1 * (t0 * (r0 * field[static_cast<size_t>(ix(i1, j0, k0))] + r1 * field[static_cast<size_t>(ix(i1, j0, k1))]) +
                 t1 * (r0 * field[static_cast<size_t>(ix(i1, j1, k0))] + r1 * field[static_cast<size_t>(ix(i1, j1, k1))]));
}
} // namespace

Math::Vec3 FluidSimulation::SampleVelocity(const Math::Vec3& worldPos) const {
    if (Nx <= 0) {
        return Math::Vec3(0, 0, 0);
    }
    const GridLayout g = GetGridLayout();
    const float invx = 1.0f / std::max(g.hx, 1e-8f);
    const float invy = 1.0f / std::max(g.hy, 1e-8f);
    const float invz = 1.0f / std::max(g.hz, 1e-8f);
    const float xf = (worldPos.x - gridOrigin.x) * invx + 0.5f;
    const float yf = (worldPos.y - gridOrigin.y) * invy + 0.5f;
    const float zf = (worldPos.z - gridOrigin.z) * invz + 0.5f;
    const float x = std::clamp(xf, 0.5f, static_cast<float>(Nx) + 0.5f);
    const float y = std::clamp(yf, 0.5f, static_cast<float>(Ny) + 0.5f);
    const float z = std::clamp(zf, 0.5f, static_cast<float>(Nz) + 0.5f);
    const float vx = TrilinearSample(Vx, x, y, z, g);
    const float vy = TrilinearSample(Vy, x, y, z, g);
    const float vz = TrilinearSample(Vz, x, y, z, g);
    return Math::Vec3(vx, vy, vz);
}

} // namespace Solstice::Physics
