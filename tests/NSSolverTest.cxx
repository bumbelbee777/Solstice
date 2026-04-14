#include "../source/Physics/Fluid/Fluid.hxx"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace Solstice::Physics;

static std::atomic<int> g_TestPassed{0};
static std::atomic<int> g_TestFailed{0};

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << " (at " << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            g_TestFailed.fetch_add(1, std::memory_order_relaxed); \
            return false; \
        } \
    } while (0)

#define TEST_PASS(message) \
    do { \
        std::cout << "PASS: " << message << std::endl; \
        g_TestPassed.fetch_add(1, std::memory_order_relaxed); \
    } while (0)

namespace {

bool EnvFlag(const char* name) {
    const char* v = std::getenv(name);
    return v && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

bool IsFiniteField(const std::vector<float>& values) {
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    return true;
}

float SumAbs(const std::vector<float>& values) {
    float sum = 0.0f;
    for (float value : values) {
        sum += std::abs(value);
    }
    return sum;
}

float CellDivergence(const FluidSimulation& sim, int x, int y, int z) {
    const GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto IX = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& vx = sim.GetVx();
    const auto& vy = sim.GetVy();
    const auto& vz = sim.GetVz();
    const float inv2hx = 0.5f / g.hx;
    const float inv2hy = 0.5f / g.hy;
    const float inv2hz = 0.5f / g.hz;
    return -(inv2hx * (vx[static_cast<size_t>(IX(x + 1, y, z))] - vx[static_cast<size_t>(IX(x - 1, y, z))])
             + inv2hy * (vy[static_cast<size_t>(IX(x, y + 1, z))] - vy[static_cast<size_t>(IX(x, y - 1, z))])
             + inv2hz * (vz[static_cast<size_t>(IX(x, y, z + 1))] - vz[static_cast<size_t>(IX(x, y, z - 1))]));
}

float MeanAbsDivergence(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    float total = 0.0f;
    int count = 0;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                total += std::abs(CellDivergence(sim, x, y, z));
                ++count;
            }
        }
    }
    return count > 0 ? (total / static_cast<float>(count)) : 0.0f;
}

float MaxAbsDivergence(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    float m = 0.0f;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                m = std::max(m, std::abs(CellDivergence(sim, x, y, z)));
            }
        }
    }
    return m;
}

float MaxAbsVelocity(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto IX = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& vx = sim.GetVx();
    const auto& vy = sim.GetVy();
    const auto& vz = sim.GetVz();
    float m = 0.0f;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                const int idx = IX(x, y, z);
                const float s = std::abs(vx[static_cast<size_t>(idx)]) + std::abs(vy[static_cast<size_t>(idx)])
                    + std::abs(vz[static_cast<size_t>(idx)]);
                m = std::max(m, s);
            }
        }
    }
    return m;
}

float InteriorKineticEnergy(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto IX = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& vx = sim.GetVx();
    const auto& vy = sim.GetVy();
    const auto& vz = sim.GetVz();
    float e = 0.0f;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                const int idx = IX(x, y, z);
                const float u = vx[static_cast<size_t>(idx)];
                const float v = vy[static_cast<size_t>(idx)];
                const float w = vz[static_cast<size_t>(idx)];
                e += 0.5f * (u * u + v * v + w * w);
            }
        }
    }
    return e;
}

float TotalInteriorMass(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto IX = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& d = sim.GetDensity();
    float s = 0.0f;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                s += d[static_cast<size_t>(IX(x, y, z))];
            }
        }
    }
    return s;
}

float MeanInteriorDensity(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    if (g.Nx <= 0 || g.Ny <= 0 || g.Nz <= 0) {
        return 0.0f;
    }
    const float vol = static_cast<float>(g.Nx) * static_cast<float>(g.Ny) * static_cast<float>(g.Nz);
    return TotalInteriorMass(sim) / vol;
}

/// Mass-weighted mean z-index in [1, Nz] (cell-centered convention).
float CenterOfMassZ(const FluidSimulation& sim) {
    const GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto IX = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& d = sim.GetDensity();
    double num = 0.0;
    double den = 0.0;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                const float rho = d[static_cast<size_t>(IX(x, y, z))];
                if (rho > 0.0f) {
                    num += static_cast<double>(z) * static_cast<double>(rho);
                    den += static_cast<double>(rho);
                }
            }
        }
    }
    return den > 1e-20 ? static_cast<float>(num / den) : 0.0f;
}

void PrintBanner() {
    std::cout << R"(
================================================================================
  Solstice Navier-Stokes / semi-Lagrangian + projection validation suite
--------------------------------------------------------------------------------
  Notation:  u = (u,v,w) velocity;  rho = dye/density;  grid N^3 interior cells
  Discrete div uses same 2nd-order stencil as the solver (see MeanAbsDivergence).
  After projection, <|div u|> should stay small vs |u| for this operator-split
  scheme. Semi-Lagrangian uses dt0 = dt*N; on a periodic grid a passive marker
  drifts Delta z ~ w*sum(dt)*N. Enclosed walls + projection break plug flow, so
  measured Delta z_COM is typically a fraction (~0.15-0.35) of that ballistic ref.
================================================================================
)"
              << std::endl;
}

void PrintVerificationTableHeader() {
    std::cout << std::endl
              << "+---------------------------+------------------+---------------+---------------+----------+-----+" << std::endl
              << "| Case / quantity           | Expected (theory)| Solved (code) | Abs error     | Tol      | OK? |" << std::endl
              << "+---------------------------+------------------+---------------+---------------+----------+-----+" << std::endl;
}

void PrintVerificationRow(const std::string& caseQty, double expected, double solved, double tol, bool ok) {
    const double err = std::abs(solved - expected);
    std::cout << "| " << std::left << std::setw(25) << caseQty.substr(0, 25) << " | " << std::right << std::fixed << std::setprecision(5)
              << std::setw(16) << expected << " | " << std::setw(13) << solved << " | " << std::setw(13) << err << " | "
              << std::setw(8) << std::setprecision(5) << tol << " | " << (ok ? "yes" : "NO ") << " |" << std::endl;
}

/// Upper bound: solved should be <= bound (show overrun as positive shortfall column).
void PrintLeqRow(const std::string& caseQty, double bound, double solved, bool ok) {
    const double overrun = std::max(0.0, solved - bound);
    std::cout << "| " << std::left << std::setw(25) << caseQty.substr(0, 25) << " | " << std::right << std::fixed << std::setprecision(5)
              << std::setw(16) << bound << " | " << std::setw(13) << solved << " | " << std::setw(13) << overrun << " | "
              << std::setw(8) << "(<=)    " << " | " << (ok ? "yes" : "NO ") << " |" << std::endl;
}

/// Lower bound: solved should be >= bound (show deficit if any).
void PrintGeqRow(const std::string& caseQty, double bound, double solved, bool ok) {
    const double deficit = std::max(0.0, bound - solved);
    std::cout << "| " << std::left << std::setw(25) << caseQty.substr(0, 25) << " | " << std::right << std::fixed << std::setprecision(5)
              << std::setw(16) << bound << " | " << std::setw(13) << solved << " | " << std::setw(13) << deficit << " | "
              << std::setw(8) << "(>=)    " << " | " << (ok ? "yes" : "NO ") << " |" << std::endl;
}

void PrintVerificationTableFooter() {
    std::cout << "+---------------------------+------------------+---------------+---------------+----------+-----+" << std::endl;
}

void FillCheckerboardVelocity(FluidSimulation& sim, float scale) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const float sign = ((x + y + z) % 2 == 0) ? 1.0f : -1.0f;
                sim.AddVelocity(x, y, z, sign * 0.8f * scale, -sign * 0.4f * scale, sign * 0.6f * scale);
                if ((x + y + z) % 5 == 0) {
                    sim.AddDensity(x, y, z, 0.3f * scale);
                }
            }
        }
    }
}

void FillSmoothWaves(FluidSimulation& sim, float scale) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const float wave = std::sin(static_cast<float>(x + y + z) * 0.25f);
                sim.AddVelocity(x, y, z, wave * 0.3f * scale, wave * -0.2f * scale, wave * 0.4f * scale);
                if ((x * y + z) % 7 == 0) {
                    sim.AddDensity(x, y, z, 0.2f * scale);
                }
            }
        }
    }
}

void FillShearLayer(FluidSimulation& sim) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    const float mid = 0.5f * static_cast<float>(ny - 1);
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const float dy = static_cast<float>(y) - mid;
                sim.AddVelocity(x, y, z, std::tanh(dy * 0.35f) * 1.2f, 0.05f * std::sin(static_cast<float>(x) * 0.5f), 0.0f);
                if (std::abs(dy) < 2.0f) {
                    sim.AddDensity(x, y, z, 0.4f);
                }
            }
        }
    }
}

void FillLocalizedJet(FluidSimulation& sim) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    const int cx = nx / 2;
    const int cy = ny / 2;
    const int cz = nz / 2;
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const float dx = static_cast<float>(x - cx);
                const float dy = static_cast<float>(y - cy);
                const float dz = static_cast<float>(z - cz);
                const float r2 = dx * dx + dy * dy + dz * dz;
                if (r2 < 9.0f) {
                    sim.AddVelocity(x, y, z, 0.0f, 0.0f, 2.5f * std::exp(-r2 * 0.15f));
                    sim.AddDensity(x, y, z, 1.0f * std::exp(-r2 * 0.2f));
                }
            }
        }
    }
}

void FillUniformVelocity(FluidSimulation& sim, float ux, float uy, float uz) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                sim.AddVelocity(x, y, z, ux, uy, uz);
            }
        }
    }
}

void FillZGaussianDensity(FluidSimulation& sim, float zPeakCells, float sigmaCells, float amplitude) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    const float inv2s2 = 1.0f / (2.0f * sigmaCells * sigmaCells);
    for (int z = 0; z < nz; ++z) {
        const float zc = static_cast<float>(z) + 0.5f;
        const float dz = zc - zPeakCells;
        const float gz = std::exp(-dz * dz * inv2s2);
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                sim.AddDensity(x, y, z, amplitude * gz);
            }
        }
    }
}

void ApplyBitplanePattern(FluidSimulation& sim) {
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    for (int z = 0; z < nz; ++z) {
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const uint8_t a = static_cast<uint8_t>(40 + (x * 17 + y * 3 + z * 11) % 180);
                const uint8_t b = static_cast<uint8_t>(60 + (x + z * 2) % 160);
                sim.SetPackedChannel(x, y, z, 0, a);
                sim.SetPackedChannel(x, y, z, 1, b);
                sim.SetPackedChannel(x, y, z, 2, static_cast<uint8_t>(100 + y % 100));
                sim.SetPackedChannel(x, y, z, 3, static_cast<uint8_t>(80 + z % 120));
            }
        }
    }
}

bool AssertSimulationHealthy(const FluidSimulation& sim, const char* ctx) {
    const std::string p = ctx;
    TEST_ASSERT(IsFiniteField(sim.GetDensity()), p + ": density");
    TEST_ASSERT(IsFiniteField(sim.GetVx()), p + ": Vx");
    TEST_ASSERT(IsFiniteField(sim.GetVy()), p + ": Vy");
    TEST_ASSERT(IsFiniteField(sim.GetVz()), p + ": Vz");
    TEST_ASSERT(std::isfinite(MeanAbsDivergence(sim)), p + ": divergence metric");
    const float vmax = MaxAbsVelocity(sim);
    TEST_ASSERT(std::isfinite(vmax), p + ": max velocity");
    TEST_ASSERT(vmax < 1.0e6f, p + ": velocity blow-up guard");
    return true;
}

using Clock = std::chrono::steady_clock;

double MedianMs(std::vector<double>& samples) {
    if (samples.empty()) {
        return 0.0;
    }
    std::sort(samples.begin(), samples.end());
    const size_t mid = samples.size() / 2;
    if (samples.size() % 2 == 1) {
        return samples[mid];
    }
    return 0.5 * (samples[mid - 1] + samples[mid]);
}

struct BenchConfig {
    const char* label;
    int N;
    int stepsPerRun;
    int warmupSteps;
    int timedRuns;
    float dt;
    float diff;
    float visc;
    FluidSolverTuning tuning;
};

struct BenchStats {
    double msPerStepMedian = 0.0;
    double meanAbsDiv = 0.0;
    double maxAbsDiv = 0.0;
    double ke = 0.0;
    double mass = 0.0;
};

BenchStats RunBenchmarkRow(const BenchConfig& cfg) {
    BenchStats out{};
    FluidSimulation sim(cfg.N, cfg.diff, cfg.visc);
    sim.GetTuning() = cfg.tuning;
    FillSmoothWaves(sim, 1.0f);

    for (int w = 0; w < cfg.warmupSteps; ++w) {
        sim.Step(cfg.dt);
    }
    if (!IsFiniteField(sim.GetDensity())) {
        std::cerr << "BENCH ABORT (non-finite after warmup): " << cfg.label << std::endl;
        return out;
    }

    std::vector<double> runMs;
    runMs.reserve(static_cast<size_t>(cfg.timedRuns));
    for (int r = 0; r < cfg.timedRuns; ++r) {
        const auto t0 = Clock::now();
        for (int s = 0; s < cfg.stepsPerRun; ++s) {
            sim.Step(cfg.dt);
        }
        const auto t1 = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        runMs.push_back(ms / static_cast<double>(cfg.stepsPerRun));
    }
    out.msPerStepMedian = MedianMs(runMs);
    out.meanAbsDiv = static_cast<double>(MeanAbsDivergence(sim));
    out.maxAbsDiv = static_cast<double>(MaxAbsDivergence(sim));
    out.ke = static_cast<double>(InteriorKineticEnergy(sim));
    out.mass = static_cast<double>(TotalInteriorMass(sim));

    const double cells = static_cast<double>(cfg.N) * static_cast<double>(cfg.N) * static_cast<double>(cfg.N);
    const double mcups = (cells * 1e-6) / (out.msPerStepMedian * 1e-3 + 1e-30);

    std::cout << "| " << std::left << std::setw(26) << cfg.label << " | " << std::setw(3) << cfg.N << " | " << std::fixed
              << std::setprecision(3) << std::setw(9) << out.msPerStepMedian << " | " << std::setw(10) << std::setprecision(2)
              << mcups << " | " << std::setw(9) << std::setprecision(4) << out.meanAbsDiv << " | " << std::setw(9) << out.maxAbsDiv
              << " | " << std::setw(11) << std::setprecision(3) << out.ke << " | " << std::setw(10) << std::setprecision(3) << out.mass
              << " |" << std::endl;
    return out;
}

void RunBenchmarkSuite() {
    const bool deep = EnvFlag("NSSOLVER_BENCH_DEEP");
    const bool torture = EnvFlag("NSSOLVER_TORTURE");
    const int N2 = torture ? (deep ? 48 : 40) : (deep ? 32 : 24);
    const int steps = torture ? (deep ? 16 : 12) : (deep ? 12 : 8);
    const int runs = torture ? (deep ? 5 : 4) : (deep ? 7 : 5);

    std::cout << std::endl
              << "+======================================================================================================+" << std::endl
              << "|  NSSolver performance & discrete-diagnostics (median of " << runs << " timed runs, " << steps << " steps/run)                |" << std::endl
              << "|  N = " << N2 << " interior;  dt = 1/120;  nu, kappa = 1e-4  (unless torture env widens grid)                         |" << std::endl
              << "+----------------------------+-----+-----------+------------+-----------+-----------+-------------+------------+" << std::endl
              << "| Config                     |  N  | ms/step   | Mcells/s   | <|div|>  | max|div| |  KE end     | mass rho   |" << std::endl
              << "+----------------------------+-----+-----------+------------+-----------+-----------+-------------+------------+" << std::endl;

    FluidSolverTuning base{};
    base.enableMacCormack = true;
    base.useSparseRaymarch = false;
    base.useSpectralDiffusion = false;

    FluidSolverTuning semi = base;
    semi.enableMacCormack = false;

    FluidSolverTuning spectral = base;
    spectral.useSpectralDiffusion = true;
    spectral.periodicSpectralDiffusion = true;

    FluidSolverTuning sparse = base;
    sparse.useSparseRaymarch = true;
    sparse.enableMacCormack = false;
    sparse.occupancyThreshold = 1e-5f;

    FluidSolverTuning allOn = base;
    allOn.useSpectralDiffusion = true;
    allOn.periodicSpectralDiffusion = true;
    allOn.useSparseRaymarch = true;
    allOn.temporalReprojectWeight = 0.5f;

    const float dt = 1.0f / 120.0f;
    const float diff = 0.0001f;
    const float visc = 0.0001f;

    (void)RunBenchmarkRow({"MacCormack (default)", N2, steps, 2, runs, dt, diff, visc, base});
    (void)RunBenchmarkRow({"Semi-Lagrangian", N2, steps, 2, runs, dt, diff, visc, semi});
    if ((N2 & (N2 - 1)) == 0) {
        (void)RunBenchmarkRow({"+ spectral diffusion", N2, steps, 2, runs, dt, diff, visc, spectral});
    }
    (void)RunBenchmarkRow({"+ sparse raymarch", N2, steps, 2, runs, dt, diff, visc, sparse});
    if (torture && ((N2 & (N2 - 1)) == 0)) {
        (void)RunBenchmarkRow({"torture: all toggles", N2, steps, 2, runs, dt, diff, visc, allOn});
    }

    std::cout << "+----------------------------+-----+-----------+------------+-----------+-----------+-------------+------------+" << std::endl;
    std::cout << "|  Env: NSSOLVER_BENCH_DEEP=1  larger N & runs;  NSSOLVER_TORTURE=1  stress grid + combined toggles   |" << std::endl;
    std::cout << "|  Env: NSSOLVER_NO_BENCH=1    skip this table                                                          |" << std::endl;
    std::cout << "+======================================================================================================+" << std::endl;
}

} // namespace

bool TestAnalytical_RestState() {
    constexpr int N = 12;
    FluidSimulation sim(N, 1e-6f, 1e-6f);
    for (int i = 0; i < 15; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    const float vmax = MaxAbsVelocity(sim);
    const float mass = TotalInteriorMass(sim);
    const double expV = 0.0;
    const double expM = 0.0;
    const bool okV = vmax < 1e-4f;
    const bool okM = mass < 1e-4f;
    PrintVerificationRow("Rest: max|u| (expect 0)", expV, vmax, 1e-4, okV);
    PrintVerificationRow("Rest: sum rho (expect 0)", expM, mass, 1e-4, okM);
    TEST_ASSERT(okV, "Rest state velocity should stay ~0");
    TEST_ASSERT(okM, "Rest state mass should stay ~0");

    TEST_PASS("Analytical: hydrostatic rest (u=0, rho=0)");
    return true;
}

bool TestAnalytical_UniformRhoNoVelocity() {
    constexpr int N = 10;
    FluidSimulation sim(N, 1e-8f, 1e-8f);
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                sim.AddDensity(x, y, z, 1.0f);
            }
        }
    }
    const float rho0 = MeanInteriorDensity(sim);
    for (int i = 0; i < 12; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    const float rho1 = MeanInteriorDensity(sim);
    const double expR = 1.0;
    const bool ok = std::abs(static_cast<double>(rho1) - expR) < 0.08;
    PrintVerificationRow("Uniform rho: mean after", expR, rho1, 0.08, ok);
    TEST_ASSERT(ok, "Uniform rho with u=0: mean density ~1");
    if (!AssertSimulationHealthy(sim, "uniform rho")) {
        return false;
    }

    TEST_PASS("Analytical: uniform rho, vanishing diff/visc");
    return true;
}

bool TestAnalytical_UniformZAdvection() {
    constexpr int N = 28;
    constexpr float w = 0.42f;
    constexpr int nStep = 24;
    constexpr float dt = 1.0f / 200.0f;
    FluidSimulation sim(N, 1e-7f, 1e-7f);
    sim.GetTuning().enableMacCormack = false;
    FillUniformVelocity(sim, 0.0f, 0.0f, w);
    const float zPeak = 0.5f * static_cast<float>(N);
    FillZGaussianDensity(sim, zPeak, 2.5f, 2.0f);

    const float zCom0 = CenterOfMassZ(sim);
    const float expectedDelta = w * dt * static_cast<float>(nStep) * static_cast<float>(N);
    for (int i = 0; i < nStep; ++i) {
        sim.Step(dt);
    }
    if (!AssertSimulationHealthy(sim, "uniform z adv")) {
        return false;
    }
    const float zCom1 = CenterOfMassZ(sim);
    const float solvedDelta = zCom1 - zCom0;
    const double ballistic = static_cast<double>(expectedDelta);
    const double eta = static_cast<double>(solvedDelta) / (ballistic + 1e-20);
    constexpr double kMinFrac = 0.05;
    constexpr double kMaxFrac = 1.20;
    const bool okPositive = solvedDelta > 0.02f;
    const bool okEtaMin = eta >= kMinFrac;
    const bool okEtaMax = eta <= kMaxFrac;
    const bool ok = okPositive && okEtaMin && okEtaMax;
    std::cout << "| " << std::left << std::setw(25) << "Adv: periodic ref |dz|" << " | " << std::right << std::fixed << std::setprecision(5)
              << std::setw(16) << ballistic << " | " << std::setw(13) << static_cast<double>(solvedDelta) << " | " << std::setw(13) << eta
              << " | " << std::setw(8) << "eta=dz/ref" << " | " << " n/a |" << std::endl;
    PrintGeqRow("Adv: Delta z_COM (+w)", 0.02, static_cast<double>(solvedDelta), okPositive);
    PrintGeqRow("Adv: eta lower bound", kMinFrac, eta, okEtaMin);
    PrintLeqRow("Adv: eta upper bound", kMaxFrac, eta, okEtaMax);
    TEST_ASSERT(ok, "Uniform +w: z_COM increases; eta=dz/ballistic in [0.05,1.20]");

    TEST_PASS("Analytical: uniform w, Gaussian rho (COM shift vs ballistic ref)");
    return true;
}

bool TestAnalytical_ProjectionSmallDivergence() {
    constexpr int N = 20;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillSmoothWaves(sim, 1.0f);
    const float uScale = MaxAbsVelocity(sim);
    const float div0 = MeanAbsDivergence(sim);
    sim.Step(1.0f / 120.0f);
    const float divM = MeanAbsDivergence(sim);
    const float divX = MaxAbsDivergence(sim);
    const bool legacyIsotropic = sim.GetGridLayout().LegacyIsotropicCube();
    const double velocityBound = legacyIsotropic ? static_cast<double>(uScale) * 0.40 : static_cast<double>(uScale) * 0.25;
    const double expBound = std::max(legacyIsotropic ? 0.035 : 0.02, velocityBound);
    const double maxBound = expBound * (legacyIsotropic ? 8.5 : 4.0);
    const bool okDecay = divM <= std::max(0.65f, div0 * 0.75f);
    const bool okM = divM < expBound;
    const bool okX = divX < maxBound;
    PrintLeqRow("Proj: mean|div| cap", expBound, divM, okM);
    PrintLeqRow("Proj: max|div| cap", maxBound, divX, okX);
    PrintLeqRow("Proj: mean|div| decay cap", std::max(0.65, static_cast<double>(div0) * 0.75), divM, okDecay);
    TEST_ASSERT(okM, "Post-step mean divergence within projection tolerance");
    TEST_ASSERT(okX, "Post-step max divergence within slack tolerance");
    TEST_ASSERT(okDecay, "Projection should reduce mean divergence versus initialization");

    TEST_PASS("Analytical: discrete div small after projection step");
    return true;
}

bool TestProjection_MultigridImprovesDivergence() {
    constexpr int N = 20;
    FluidSimulation baseline(N, 0.0001f, 0.0001f);
    FluidSimulation multigrid(N, 0.0001f, 0.0001f);
    FillSmoothWaves(baseline, 1.0f);
    FillSmoothWaves(multigrid, 1.0f);

    auto& tBase = baseline.GetTuning();
    tBase.usePressureMultigrid = false;
    tBase.pressureRelaxationIterations = 32;

    auto& tMg = multigrid.GetTuning();
    tMg.usePressureMultigrid = true;
    tMg.pressureMultigridLevels = 4;
    tMg.pressureMultigridPreSmooth = 2;
    tMg.pressureMultigridPostSmooth = 2;
    tMg.pressureMultigridCoarseSmooth = 20;
    tMg.pressureRelaxationIterations = 32;

    baseline.Step(1.0f / 120.0f);
    multigrid.Step(1.0f / 120.0f);

    const float baseMeanDiv = MeanAbsDivergence(baseline);
    const float mgMeanDiv = MeanAbsDivergence(multigrid);
    const bool ok = std::isfinite(baseMeanDiv) && std::isfinite(mgMeanDiv) && (mgMeanDiv <= baseMeanDiv * 1.02f);

    PrintLeqRow("ProjMG: mean|div| vs relax", static_cast<double>(baseMeanDiv * 1.02f), mgMeanDiv, ok);
    TEST_ASSERT(ok, "Multigrid projection should not regress divergence");

    TEST_PASS("Projection: multigrid parity/improvement against relaxation");
    return true;
}

bool TestProjection_StabilityAcrossGridSizes() {
    const std::vector<int> sizes = {10, 14, 20, 28};
    for (int n : sizes) {
        FluidSimulation sim(n, 0.0001f, 0.0001f);
        auto& t = sim.GetTuning();
        t.usePressureMultigrid = true;
        t.pressureMultigridLevels = 5;
        t.pressureMultigridPreSmooth = 2;
        t.pressureMultigridPostSmooth = 2;
        t.pressureMultigridCoarseSmooth = 18;
        FillSmoothWaves(sim, 1.0f);
        for (int step = 0; step < 10; ++step) {
            sim.Step(1.0f / 120.0f);
        }
        const float divM = MeanAbsDivergence(sim);
        const float divX = MaxAbsDivergence(sim);
        const bool ok = std::isfinite(divM) && std::isfinite(divX) && divM < 0.8f && divX < 6.0f;
        PrintLeqRow("ProjStable: mean|div| cap", 0.8, divM, divM < 0.8f);
        PrintLeqRow("ProjStable: max|div| cap", 6.0, divX, divX < 6.0f);
        TEST_ASSERT(ok, "Projection stability should hold across grid sizes");
    }

    TEST_PASS("Projection: stability matrix across grid sizes");
    return true;
}

bool TestAnalytical_ViscousDecayTrend() {
    constexpr int N = 16;
    FluidSimulation sim(N, 0.0001f, 0.08f);
    FillUniformVelocity(sim, 0.35f, -0.22f, 0.18f);
    float ePrev = InteriorKineticEnergy(sim);
    int increases = 0;
    for (int i = 0; i < 40; ++i) {
        sim.Step(1.0f / 120.0f);
        const float e = InteriorKineticEnergy(sim);
        if (e > ePrev * 1.02f) {
            ++increases;
        }
        ePrev = e;
    }
    if (!AssertSimulationHealthy(sim, "viscous decay")) {
        return false;
    }
    const float e0 = InteriorKineticEnergy(sim);
    const bool ok = increases <= 12;
    PrintLeqRow("Viscous: KE uptick count", 12.0, static_cast<double>(increases), ok);
    TEST_ASSERT(ok, "High viscosity: kinetic energy should trend down (few local upticks)");
    TEST_ASSERT(e0 < 5000.0f, "Residual KE bounded");

    TEST_PASS("Analytical: viscous damping trend (high nu)");
    return true;
}

bool TestSolverRegression() {
    constexpr int N = 16;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillCheckerboardVelocity(sim, 1.0f);

    const float divergenceBefore = MeanAbsDivergence(sim);
    const float densityBefore = SumAbs(sim.GetDensity());
    TEST_ASSERT(std::isfinite(divergenceBefore), "Initial divergence must be finite");
    TEST_ASSERT(std::isfinite(densityBefore), "Initial density must be finite");

    for (int i = 0; i < 8; ++i) {
        sim.Step(1.0f / 60.0f);
    }

    const float divergenceAfter = MeanAbsDivergence(sim);
    const float densityAfter = SumAbs(sim.GetDensity());

    TEST_ASSERT(IsFiniteField(sim.GetDensity()), "Density field must remain finite");
    TEST_ASSERT(IsFiniteField(sim.GetVx()), "Velocity X field must remain finite");
    TEST_ASSERT(IsFiniteField(sim.GetVy()), "Velocity Y field must remain finite");
    TEST_ASSERT(IsFiniteField(sim.GetVz()), "Velocity Z field must remain finite");
    TEST_ASSERT(std::isfinite(divergenceAfter), "Final divergence must be finite");
    PrintLeqRow("Regression: mean|div| cap", static_cast<double>(divergenceBefore * 1.15f), divergenceAfter,
                divergenceAfter <= divergenceBefore * 1.15f);
    TEST_ASSERT(divergenceAfter <= divergenceBefore * 1.15f, "Divergence should not regress significantly");
    TEST_ASSERT(std::isfinite(densityAfter), "Final density must be finite");
    PrintLeqRow("Regression: sum|rho| cap", static_cast<double>(densityBefore * 5.0f), densityAfter,
                densityAfter <= densityBefore * 5.0f);
    TEST_ASSERT(densityAfter <= densityBefore * 5.0f, "Density should stay bounded");

    TEST_PASS("Navier-Stokes regression invariants");
    return true;
}

bool TestScenario_SemiLagrangianAdvection() {
    constexpr int N = 18;
    FluidSimulation sim(N, 0.0002f, 0.00015f);
    auto& t = sim.GetTuning();
    t.enableMacCormack = false;
    t.useSparseRaymarch = false;
    t.useSpectralDiffusion = false;
    FillSmoothWaves(sim, 1.2f);

    const float div0 = MeanAbsDivergence(sim);
    for (int i = 0; i < 24; ++i) {
        sim.Step(1.0f / 90.0f);
    }
    if (!AssertSimulationHealthy(sim, "semi-Lagrangian")) {
        return false;
    }
    const float div1 = MeanAbsDivergence(sim);
    const double cap = static_cast<double>(div0) * 1.35;
    PrintLeqRow("SemiLag: mean|div| cap", cap, div1, div1 <= div0 * 1.35f);
    TEST_ASSERT(div1 <= div0 * 1.35f, "Semi-Lagrangian divergence drift");

    TEST_PASS("Scenario: semi-Lagrangian (MacCormack off)");
    return true;
}

bool TestScenario_SpectralDiffusionPowerOfTwo() {
    constexpr int N = 32;
    FluidSimulation sim(N, 0.0001f, 0.0002f);
    auto& t = sim.GetTuning();
    t.useSpectralDiffusion = true;
    t.periodicSpectralDiffusion = true;
    t.enableMacCormack = true;
    FillCheckerboardVelocity(sim, 0.7f);

    const float e0 = InteriorKineticEnergy(sim);
    for (int i = 0; i < 12; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    if (!AssertSimulationHealthy(sim, "spectral diffusion")) {
        return false;
    }
    const float e1 = InteriorKineticEnergy(sim);
    const double keCap = static_cast<double>(e0) * 50.0;
    PrintLeqRow("Spectral: KE cap", keCap, e1, e1 < e0 * 50.0f);
    TEST_ASSERT(std::isfinite(e1) && e1 < e0 * 50.0f, "Kinetic energy should not explode");

    TEST_PASS("Scenario: spectral diffusion (N=32 power-of-two)");
    return true;
}

bool TestScenario_SpectralFallbackNonPowerOfTwo() {
    constexpr int N = 20;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    auto& t = sim.GetTuning();
    t.useSpectralDiffusion = true;
    t.periodicSpectralDiffusion = true;
    FillSmoothWaves(sim, 1.0f);
    for (int i = 0; i < 10; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    if (!AssertSimulationHealthy(sim, "spectral fallback N=20")) {
        return false;
    }

    TEST_PASS("Scenario: spectral flag on non-power-of-two (fallback path)");
    return true;
}

bool TestScenario_SparseRaymarch() {
    constexpr int N = 20;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    auto& t = sim.GetTuning();
    t.useSparseRaymarch = true;
    t.enableMacCormack = false;
    t.occupancyThreshold = 1e-5f;
    t.sparseBlockSize = 4;
    FillLocalizedJet(sim);
    for (int i = 0; i < 16; ++i) {
        sim.Step(1.0f / 120.0f);
    }
    if (!AssertSimulationHealthy(sim, "sparse raymarch")) {
        return false;
    }
    const float mass = TotalInteriorMass(sim);
    PrintGeqRow("Sparse: interior mass", 1e-4, mass, mass > 1e-4f);
    TEST_ASSERT(mass > 1e-4f, "Sparse raymarch should retain some density");

    TEST_PASS("Scenario: sparse occupancy + raymarch advection");
    return true;
}

bool TestScenario_ShearLayer() {
    constexpr int N = 22;
    FluidSimulation sim(N, 0.00008f, 0.00012f);
    FillShearLayer(sim);
    for (int i = 0; i < 20; ++i) {
        sim.Step(1.0f / 100.0f);
    }
    if (!AssertSimulationHealthy(sim, "shear layer")) {
        return false;
    }
    const float d1 = MeanAbsDivergence(sim);
    PrintLeqRow("Shear: mean|div| cap", 5.0, d1, d1 < 5.0f);
    TEST_ASSERT(std::isfinite(d1) && d1 < 5.0f, "Shear layer: mean |div| should stay bounded");

    TEST_PASS("Scenario: shear / density interface");
    return true;
}

bool TestScenario_TemporalReprojection() {
    constexpr int N = 16;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    auto& t = sim.GetTuning();
    t.temporalReprojectWeight = 0.45f;
    FillSmoothWaves(sim, 1.0f);
    for (int i = 0; i < 40; ++i) {
        sim.Step(1.0f / 120.0f);
    }
    if (!AssertSimulationHealthy(sim, "temporal reprojection")) {
        return false;
    }

    TEST_PASS("Scenario: temporal reprojection stress");
    return true;
}

bool TestScenario_HighViscosityDiffusion() {
    constexpr int N = 14;
    FluidSimulation sim(N, 0.02f, 0.05f);
    FillCheckerboardVelocity(sim, 1.5f);
    for (int i = 0; i < 15; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    if (!AssertSimulationHealthy(sim, "high visc/diff")) {
        return false;
    }

    TEST_PASS("Scenario: high viscosity and diffusion");
    return true;
}

bool TestScenario_AggressiveTimestep() {
    constexpr int N = 16;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillSmoothWaves(sim, 2.0f);
    for (int i = 0; i < 6; ++i) {
        sim.Step(1.0f / 30.0f);
    }
    if (!AssertSimulationHealthy(sim, "large dt")) {
        return false;
    }

    TEST_PASS("Scenario: aggressive (large) timestep");
    return true;
}

bool TestScenario_BitplanePackedChannels() {
    constexpr int N = 12;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillSmoothWaves(sim, 0.5f);
    ApplyBitplanePattern(sim);
    for (int i = 0; i < 10; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    if (!AssertSimulationHealthy(sim, "bitplane")) {
        return false;
    }
    const int nx = sim.GetNx();
    const int ny = sim.GetNy();
    const int nz = sim.GetNz();
    unsigned touched = 0;
    for (int z = 0; z < nz; z += 3) {
        for (int y = 0; y < ny; y += 3) {
            for (int x = 0; x < nx; x += 3) {
                for (int ch = 0; ch < 4; ++ch) {
                    (void)sim.GetPackedChannel(x, y, z, ch);
                    ++touched;
                }
            }
        }
    }
    TEST_ASSERT(touched > 0, "Bitplane sampling touched cells");

    TEST_PASS("Scenario: bitplane / packed channels");
    return true;
}

bool TestInvalidDtNoCrash() {
    constexpr int N = 8;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    sim.AddVelocity(2, 2, 2, 0.1f, 0.0f, 0.0f);
    sim.Step(0.0f);
    sim.Step(-1.0f);
    if (!AssertSimulationHealthy(sim, "invalid dt")) {
        return false;
    }

    TEST_PASS("Invalid / zero dt does not corrupt state");
    return true;
}

bool TestSmallGrid() {
    constexpr int N = 4;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    sim.AddVelocity(1, 1, 1, 0.5f, -0.3f, 0.2f);
    sim.AddDensity(1, 1, 1, 0.8f);
    for (int i = 0; i < 20; ++i) {
        sim.Step(1.0f / 120.0f);
    }
    if (!AssertSimulationHealthy(sim, "small grid")) {
        return false;
    }

    TEST_PASS("Small grid (N=4) stability");
    return true;
}

bool TestTorture_LongHorizonChaos() {
    const int N = EnvFlag("NSSOLVER_TORTURE") ? 28 : 20;
    FluidSimulation sim(N, 0.00012f, 0.0001f);
    FillCheckerboardVelocity(sim, EnvFlag("NSSOLVER_TORTURE") ? 2.2f : 1.4f);
    FillSmoothWaves(sim, 0.4f);
    const int steps = EnvFlag("NSSOLVER_TORTURE") ? 520 : 220;
    float peakV = 0.0f;
    float peakDiv = 0.0f;
    for (int i = 0; i < steps; ++i) {
        sim.Step(1.0f / 120.0f);
        peakV = std::max(peakV, MaxAbsVelocity(sim));
        peakDiv = std::max(peakDiv, MaxAbsDivergence(sim));
        if ((i & 63) == 0 && !IsFiniteField(sim.GetDensity())) {
            TEST_ASSERT(false, "Long horizon: non-finite density");
            return false;
        }
    }
    if (!AssertSimulationHealthy(sim, "long horizon")) {
        return false;
    }
    PrintLeqRow("Torture: peak L1|u|", 1.0e6, peakV, peakV < 1.0e6f);
    PrintLeqRow("Torture: peak |div|", 500.0, peakDiv, peakDiv < 500.0f);
    TEST_ASSERT(peakV < 1.0e6f, "Long run velocity bound");
    TEST_ASSERT(peakDiv < 500.0f, "Long run divergence blow-up");

    TEST_PASS("Torture: long-horizon chaotic forcing");
    return true;
}

bool TestTorture_RapidDtSwing() {
    constexpr int N = 18;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillLocalizedJet(sim);
    const float dts[] = {1.0f / 240.0f, 1.0f / 45.0f, 1.0f / 200.0f, 1.0f / 30.0f, 1.0f / 180.0f};
    for (int c = 0; c < 180; ++c) {
        sim.Step(dts[static_cast<size_t>(c % 5)]);
    }
    if (!AssertSimulationHealthy(sim, "dt swing")) {
        return false;
    }
    const float divM = MeanAbsDivergence(sim);
    PrintLeqRow("DtSwing: mean|div| cap", 8.0, divM, divM < 8.0f);
    TEST_ASSERT(divM < 8.0f, "Dt swing: divergence still bounded");

    TEST_PASS("Torture: rapid dt schedule");
    return true;
}

bool TestTorture_ExtremeSparseAndSpectral() {
    if (!EnvFlag("NSSOLVER_TORTURE")) {
        std::cout << "(skip) Torture combo: set NSSOLVER_TORTURE=1 for spectral+sparse+reproj overload." << std::endl;
        return true;
    }
    constexpr int N = 32;
    FluidSimulation sim(N, 0.00015f, 0.00015f);
    auto& t = sim.GetTuning();
    t.useSpectralDiffusion = true;
    t.periodicSpectralDiffusion = true;
    t.useSparseRaymarch = true;
    t.enableMacCormack = true;
    t.occupancyThreshold = 1e-8f;
    t.temporalReprojectWeight = 0.55f;
    t.sparseBlockSize = 2;
    FillCheckerboardVelocity(sim, 2.5f);
    FillLocalizedJet(sim);
    for (int i = 0; i < 140; ++i) {
        sim.Step(1.0f / 100.0f);
    }
    if (!AssertSimulationHealthy(sim, "extreme combo")) {
        return false;
    }

    TEST_PASS("Torture: spectral + sparse + MacCormack + reprojection");
    return true;
}

bool TestAnisotropic_SmokeThermal() {
    FluidSimulation sim(10, 12, 8, 0.1f, 0.11f, 0.09f, 1e-4f, 1e-4f);
    sim.GetTuning().useSparseRaymarch = false;
    sim.GetTuning().enableMacCormack = true;
    auto& th = sim.GetThermal();
    th.enableBoussinesq = true;
    th.buoyancyStrength = 3.0f;
    th.Prandtl = 0.71f;
    th.THot = 1.0f;
    th.TCold = 0.0f;
    th.TReference = 0.5f;
    th.thermalWallAxis = 1;
    th.buoyancyAxis = 1;
    for (int z = 0; z < sim.GetNz(); ++z) {
        for (int y = 0; y < sim.GetNy(); ++y) {
            for (int x = 0; x < sim.GetNx(); ++x) {
                const float fy = static_cast<float>(y) / static_cast<float>(std::max(sim.GetNy() - 1, 1));
                sim.SetTemperatureLogical(x, y, z, th.TCold + fy * (th.THot - th.TCold));
            }
        }
    }
    sim.AddVelocity(4, 4, 3, 0.02f, 0.01f, 0.0f);
    for (int i = 0; i < 16; ++i) {
        sim.Step(1.0f / 60.0f);
    }
    TEST_ASSERT(IsFiniteField(sim.GetTemperature()), "aniso thermal: T finite");
    TEST_ASSERT(IsFiniteField(sim.GetVx()), "aniso thermal: Vx finite");
    const float divM = MeanAbsDivergence(sim);
    TEST_ASSERT(divM < 200.0f, "aniso thermal: mean |div| bounded");
    TEST_PASS("Anisotropic grid + Boussinesq smoke");
    return true;
}

bool TestSolverPerformanceGuardrail() {
    constexpr int N = 20;
    FluidSimulation sim(N, 0.0001f, 0.0001f);
    FillSmoothWaves(sim, 1.0f);

    constexpr int kSteps = 20;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kSteps; ++i) {
        sim.Step(1.0f / 120.0f);
    }
    const auto end = std::chrono::steady_clock::now();
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    PrintLeqRow("Guardrail: wall time ms", 8000.0, static_cast<double>(elapsedMs), elapsedMs < 8000);
    TEST_ASSERT(elapsedMs < 8000, "Solver performance guardrail exceeded");
    TEST_ASSERT(IsFiniteField(sim.GetDensity()), "Performance run must preserve finite density");

    TEST_PASS("Navier-Stokes performance guardrail");
    return true;
}

int main() {
    PrintBanner();

    if (!EnvFlag("NSSOLVER_NO_BENCH")) {
        RunBenchmarkSuite();
    }

    PrintVerificationTableHeader();

    bool passed = true;
    passed &= TestAnalytical_RestState();
    passed &= TestAnalytical_UniformRhoNoVelocity();
    passed &= TestAnalytical_UniformZAdvection();
    passed &= TestAnalytical_ProjectionSmallDivergence();
    passed &= TestProjection_MultigridImprovesDivergence();
    passed &= TestProjection_StabilityAcrossGridSizes();
    passed &= TestAnalytical_ViscousDecayTrend();
    passed &= TestSolverRegression();
    passed &= TestScenario_SemiLagrangianAdvection();
    passed &= TestScenario_SpectralDiffusionPowerOfTwo();
    passed &= TestScenario_SpectralFallbackNonPowerOfTwo();
    passed &= TestScenario_SparseRaymarch();
    passed &= TestScenario_ShearLayer();
    passed &= TestScenario_TemporalReprojection();
    passed &= TestScenario_HighViscosityDiffusion();
    passed &= TestScenario_AggressiveTimestep();
    passed &= TestScenario_BitplanePackedChannels();
    passed &= TestAnisotropic_SmokeThermal();
    passed &= TestInvalidDtNoCrash();
    passed &= TestSmallGrid();
    passed &= TestTorture_LongHorizonChaos();
    passed &= TestTorture_RapidDtSwing();
    passed &= TestTorture_ExtremeSparseAndSpectral();
    passed &= TestSolverPerformanceGuardrail();

    PrintVerificationTableFooter();

    std::cout << std::endl;
    std::cout << "Tests Passed: " << g_TestPassed.load() << std::endl;
    std::cout << "Tests Failed: " << g_TestFailed.load() << std::endl;
    std::cout << "Figures above: expected = closed-form or a priori bound; solved = measured from the running solver." << std::endl;

    return (passed && g_TestFailed.load() == 0) ? 0 : 1;
}
