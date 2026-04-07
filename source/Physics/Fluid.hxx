#pragma once

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>
#include "../Math/Vector.hxx"
#include "../Solstice.hxx"

namespace Solstice::Physics {

class NSSolver;
class FluidSparseData;

/// 64-byte aligned heap buffer for float scratch (SIMD / cache-line friendly).
class SOLSTICE_API AlignedFloatScratchBuffer {
public:
    AlignedFloatScratchBuffer() = default;
    ~AlignedFloatScratchBuffer();
    AlignedFloatScratchBuffer(const AlignedFloatScratchBuffer&) = delete;
    AlignedFloatScratchBuffer& operator=(const AlignedFloatScratchBuffer&) = delete;
    AlignedFloatScratchBuffer(AlignedFloatScratchBuffer&& o) noexcept;
    AlignedFloatScratchBuffer& operator=(AlignedFloatScratchBuffer&& o) noexcept;

    void Resize(size_t floatCount);
    float* Data() noexcept { return ptr_; }
    const float* Data() const noexcept { return ptr_; }
    size_t Size() const noexcept { return count_; }

private:
    void Release();
    void* block_ = nullptr;
    float* ptr_ = nullptr;
    size_t count_ = 0;
};

/// Padded MAC grid: interior (Nx x Ny x Nz), stride sx = Nx+2 along x, sxy = sx*sy.
struct SOLSTICE_API GridLayout {
    int Nx = 0;
    int Ny = 0;
    int Nz = 0;
    float hx = 1.0f;
    float hy = 1.0f;
    float hz = 1.0f;

    int sx() const { return Nx + 2; }
    int sy() const { return Ny + 2; }
    int sz() const { return Nz + 2; }
    int sxy() const { return sx() * sy(); }

    int IX(int i, int j, int k) const { return i + sx() * j + sxy() * k; }

    size_t CellCountPadded() const {
        return static_cast<size_t>(sx()) * static_cast<size_t>(sy()) * static_cast<size_t>(sz());
    }

    /// Unit cube per axis: hx = 1/Nx, etc. Matches legacy single-N stable-fluids scaling when Nx=Ny=Nz.
    static GridLayout FromUniformCube(int n) {
        GridLayout g;
        if (n <= 0) {
            return g;
        }
        g.Nx = g.Ny = g.Nz = n;
        const float inv = 1.0f / static_cast<float>(n);
        g.hx = g.hy = g.hz = inv;
        return g;
    }

    static GridLayout FromBox(int nx, int ny, int nz, float cellHx, float cellHy, float cellHz) {
        GridLayout g;
        g.Nx = nx;
        g.Ny = ny;
        g.Nz = nz;
        g.hx = cellHx > 1e-20f ? cellHx : 1.0f;
        g.hy = cellHy > 1e-20f ? cellHy : 1.0f;
        g.hz = cellHz > 1e-20f ? cellHz : 1.0f;
        return g;
    }

    /// Legacy cubic grid with same diffusion/projection/advection scaling as pre-anisotropic NSSolver.
    bool LegacyIsotropicCube() const {
        if (Nx <= 0 || Ny <= 0 || Nz <= 0 || Nx != Ny || Ny != Nz) {
            return false;
        }
        const float nxf = static_cast<float>(Nx);
        return std::abs(hx * nxf - 1.0f) < 1e-3f && std::abs(hy * nxf - 1.0f) < 1e-3f && std::abs(hz * nxf - 1.0f) < 1e-3f;
    }
};

/// Tuning for NSSolver: MacCormack, sparse raymarch, spectral diffusion, temporal reprojection, bitplanes.
struct SOLSTICE_API FluidSolverTuning {
    bool enableMacCormack = true;
    bool useSparseRaymarch = false;
    bool useSpectralDiffusion = false;
    bool periodicSpectralDiffusion = false;
    float temporalReprojectWeight = 0.25f;
    float bitplaneCouplingStrength = 0.05f;
    float occupancyThreshold = 1e-6f;
    int sparseBlockSize = 4;
};

/// Boussinesq thermal mode (temperature advected; buoyancy on one axis).
struct SOLSTICE_API FluidThermalTuning {
    bool enableBoussinesq = false;
    /// 0 = x, 1 = y, 2 = z (world +Y plume uses 1).
    int buoyancyAxis = 1;
    float TReference = 0.5f;
    float THot = 1.0f;
    float TCold = 0.0f;
    /// Effective g*beta (Boussinesq scaling); Ra/Pr mapping may overwrite visc/kappa and this each frame.
    float buoyancyStrength = 1.0f;
    float Prandtl = 0.71f;
    /// Sinusoidal modulation amplitude on THot (bottom) when timeVaryingForcing is true.
    float bottomTempOscillationAmplitude = 0.0f;
    float bottomTempOscillationHz = 0.2f;
    bool timeVaryingForcing = false;
    /// Wall normal for Dirichlet faces: 1 = low y is hot bottom, high y is cold top.
    int thermalWallAxis = 1;
    /// If true, only ghost cells on the hot wall whose in-plane world coordinates fall inside the rectangle below get THot;
    /// others on that face get TCold (narrow source vs full-span plate).
    bool useLocalizedHotWall = false;
    /// Inclusive world-space bounds on the hot wall (two axes). For thermalWallAxis 1 (low Y): U = X, V = Z.
    /// For 0 (low X): U = Y, V = Z. For 2 (low Z): U = X, V = Y.
    float hotWallUMin = 0.0f;
    float hotWallUMax = 0.0f;
    float hotWallVMin = 0.0f;
    float hotWallVMax = 0.0f;
};

// Component for entities (Legacy/Simple fluid behavior)
struct Fluid {
    float Density = 1000.0f;
    float Viscosity = 0.5f;
};

// Grid-based Fluid Simulation
class SOLSTICE_API FluidSimulation {
public:
    /// Cubic grid N^3, unit cube per axis (hx = hy = hz = 1/N).
    FluidSimulation(int size, float diffusion, float viscosity);
    FluidSimulation(int nx, int ny, int nz, float hx, float hy, float hz, float diffusion, float viscosity);
    ~FluidSimulation();

    void Step(float dt);

    void AddDensity(int x, int y, int z, float amount);
    void AddVelocity(int x, int y, int z, float amountX, float amountY, float amountZ);
    void SetPackedChannel(int x, int y, int z, int channel, uint8_t value);
    uint8_t GetPackedChannel(int x, int y, int z, int channel) const;

    Math::Vec3 GetVelocityAt(int x, int y, int z) const;
    /// World-space position (meters). Uses gridOrigin + per-axis cell sizes from layout.
    Math::Vec3 SampleVelocity(const Math::Vec3& worldPos) const;

    /// Largest interior extent (for heuristics). Prefer GetNx/GetNy/GetNz for loops.
    int GetSize() const { return (Nx >= Ny && Nx >= Nz) ? Nx : (Ny >= Nz ? Ny : Nz); }
    int GetNx() const { return Nx; }
    int GetNy() const { return Ny; }
    int GetNz() const { return Nz; }
    GridLayout GetGridLayout() const { return GridLayout::FromBox(Nx, Ny, Nz, hx, hy, hz); }

    const std::vector<float>& GetDensity() const { return density; }
    const std::vector<float>& GetVx() const { return Vx; }
    const std::vector<float>& GetVy() const { return Vy; }
    const std::vector<float>& GetVz() const { return Vz; }
    const std::vector<float>& GetTemperature() const { return temperature; }
    const std::vector<uint32_t>& GetPacked() const { return packed; }

    FluidSolverTuning& GetTuning() { return tuning; }
    const FluidSolverTuning& GetTuning() const { return tuning; }
    FluidThermalTuning& GetThermal() { return thermal; }
    const FluidThermalTuning& GetThermal() const { return thermal; }

    float GetReferenceDensity() const { return referenceDensity; }
    void SetReferenceDensity(float kgPerM3) { referenceDensity = kgPerM3; }

    float GetFluidDragCoefficient() const { return fluidDragCoefficient; }
    void SetFluidDragCoefficient(float c) { fluidDragCoefficient = c; }

    Math::Vec3 GetGridOrigin() const { return gridOrigin; }
    void SetGridOrigin(const Math::Vec3& o) { gridOrigin = o; }

    /// World AABB for visualization only (raymarch / isosurface). When enabled, drawing is limited to this box
    /// (e.g. column above a heater mesh). Physics is unchanged.
    void SetVolumeVisualizationClip(bool enable, const Math::Vec3& worldMin, const Math::Vec3& worldMax);
    bool IsVolumeVisualizationClipEnabled() const { return volumeVisClipEnable; }
    Math::Vec3 GetVolumeVisualizationClipMin() const { return volumeVisClipMin; }
    Math::Vec3 GetVolumeVisualizationClipMax() const { return volumeVisClipMax; }

    float GetGridCellSize() const { return gridCellSize; }
    void SetGridCellSize(float metersPerCell) { gridCellSize = metersPerCell > 1e-8f ? metersPerCell : 1.0f; }

    /// Apply Ra / Pr / deltaT / channel height L with fixed buoyancyStrength proxy: sets visc, diff (kappa), keeps buoyancyStrength.
    void ApplyRayleighPrParameters(float rayleigh, float prandtl, float deltaT, float channelHeight);

    /// Reset time used for thermal BC modulation (`FluidThermalTuning::timeVaryingForcing`).
    void ResetSimulationClock() { simulationTime = 0.0; }

    /// Logical interior indices x ∈ [0, Nx-1], etc. Zeros velocity, density, pressure; clears density history flag.
    void ResetFlowFields();
    /// Logical interior indices; writes both `temperature` and `temperature_prev` at that cell.
    void SetTemperatureLogical(int x, int y, int z, float value);

private:
    friend class NSSolver;

    int Nx = 0;
    int Ny = 0;
    int Nz = 0;
    float hx = 1.0f;
    float hy = 1.0f;
    float hz = 1.0f;
    float diff = 0.0f;
    float visc = 0.0f;

    FluidSolverTuning tuning;
    FluidThermalTuning thermal;

    float referenceDensity = 1000.0f;
    float fluidDragCoefficient = 0.5f;
    Math::Vec3 gridOrigin{0.0f, 0.0f, 0.0f};
    float gridCellSize = 1.0f;

    bool volumeVisClipEnable = false;
    Math::Vec3 volumeVisClipMin{0.0f, 0.0f, 0.0f};
    Math::Vec3 volumeVisClipMax{0.0f, 0.0f, 0.0f};

    std::vector<float> density;
    std::vector<float> density_prev;
    std::vector<float> density_history;
    bool densityHistoryValid = false;

    std::vector<float> temperature;
    std::vector<float> temperature_prev;

    std::vector<float> Vx;
    std::vector<float> Vy;
    std::vector<float> Vz;

    std::vector<float> Vx0;
    std::vector<float> Vy0;
    std::vector<float> Vz0;

    std::vector<float> pressure;
    std::vector<float> divergence;

    std::vector<uint32_t> packed;
    std::vector<uint32_t> packed_prev;

    AlignedFloatScratchBuffer bitplaneFieldScratch;
    AlignedFloatScratchBuffer bitplanePrevScratch;

    std::unique_ptr<FluidSparseData> sparse;

    double simulationTime = 0.0;
};

} // namespace Solstice::Physics
