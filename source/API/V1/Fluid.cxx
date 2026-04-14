#include "SolsticeAPI/V1/Fluid.h"
#include "Solstice.hxx"
#include "Physics/Fluid/Fluid.hxx"
#include <algorithm>
#include <cmath>
#include <memory>

namespace {

float CellDivergence(const Solstice::Physics::FluidSimulation& sim, int x, int y, int z) {
    const Solstice::Physics::GridLayout g = sim.GetGridLayout();
    const int sx = g.sx();
    const int sxy = g.sxy();
    auto ix = [=](int i, int j, int k) { return i + sx * j + sxy * k; };
    const auto& vx = sim.GetVx();
    const auto& vy = sim.GetVy();
    const auto& vz = sim.GetVz();
    const float inv2hx = 0.5f / g.hx;
    const float inv2hy = 0.5f / g.hy;
    const float inv2hz = 0.5f / g.hz;
    return -(inv2hx * (vx[static_cast<size_t>(ix(x + 1, y, z))] - vx[static_cast<size_t>(ix(x - 1, y, z))])
             + inv2hy * (vy[static_cast<size_t>(ix(x, y + 1, z))] - vy[static_cast<size_t>(ix(x, y - 1, z))])
             + inv2hz * (vz[static_cast<size_t>(ix(x, y, z + 1))] - vz[static_cast<size_t>(ix(x, y, z - 1))]));
}

struct FluidHandleImpl {
    explicit FluidHandleImpl(std::unique_ptr<Solstice::Physics::FluidSimulation>&& inSim) : Sim(std::move(inSim)) {}
    std::unique_ptr<Solstice::Physics::FluidSimulation> Sim;
};

} // namespace

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidCreate(
    int Nx,
    int Ny,
    int Nz,
    float Hx,
    float Hy,
    float Hz,
    float Diffusion,
    float Viscosity,
    SolsticeV1_FluidHandle* OutHandle) {
    if (OutHandle) {
        *OutHandle = nullptr;
    }
    if (!OutHandle || Nx <= 0 || Ny <= 0 || Nz <= 0 || !std::isfinite(Diffusion) || !std::isfinite(Viscosity)
        || !Solstice::Initialized.load(std::memory_order_acquire)) {
        return SolsticeV1_ResultFailure;
    }
    try {
        auto sim = std::make_unique<Solstice::Physics::FluidSimulation>(Nx, Ny, Nz, Hx, Hy, Hz, Diffusion, Viscosity);
        auto* impl = new FluidHandleImpl(std::move(sim));
        *OutHandle = reinterpret_cast<SolsticeV1_FluidHandle>(impl);
        return SolsticeV1_ResultSuccess;
    } catch (...) {
        return SolsticeV1_ResultFailure;
    }
}

SOLSTICE_V1_API void SolsticeV1_FluidDestroy(SolsticeV1_FluidHandle Handle) {
    if (!Handle) {
        return;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    delete impl;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidStep(SolsticeV1_FluidHandle Handle, float Dt) {
    if (!Handle || !std::isfinite(Dt) || Dt <= 0.0f) {
        return SolsticeV1_ResultFailure;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    impl->Sim->Step(Dt);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidAddDensity(
    SolsticeV1_FluidHandle Handle,
    int X,
    int Y,
    int Z,
    float Amount) {
    if (!Handle || !std::isfinite(Amount)) {
        return SolsticeV1_ResultFailure;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    impl->Sim->AddDensity(X, Y, Z, Amount);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidAddVelocity(
    SolsticeV1_FluidHandle Handle,
    int X,
    int Y,
    int Z,
    float Vx,
    float Vy,
    float Vz) {
    if (!Handle || !std::isfinite(Vx) || !std::isfinite(Vy) || !std::isfinite(Vz)) {
        return SolsticeV1_ResultFailure;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    impl->Sim->AddVelocity(X, Y, Z, Vx, Vy, Vz);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidSetPressureMultigrid(
    SolsticeV1_FluidHandle Handle,
    SolsticeV1_Bool Enable,
    int Levels,
    int PreSmooth,
    int PostSmooth,
    int CoarseSmooth,
    int RelaxIterations) {
    if (!Handle) {
        return SolsticeV1_ResultFailure;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    auto& tuning = impl->Sim->GetTuning();
    tuning.usePressureMultigrid = (Enable == SolsticeV1_True);
    tuning.pressureMultigridLevels = std::max(2, Levels);
    tuning.pressureMultigridPreSmooth = std::max(1, PreSmooth);
    tuning.pressureMultigridPostSmooth = std::max(1, PostSmooth);
    tuning.pressureMultigridCoarseSmooth = std::max(2, CoarseSmooth);
    tuning.pressureRelaxationIterations = std::max(2, RelaxIterations);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidGetDivergenceMetrics(
    SolsticeV1_FluidHandle Handle,
    float* OutMeanAbsDivergence,
    float* OutMaxAbsDivergence) {
    if (!Handle) {
        return SolsticeV1_ResultFailure;
    }
    auto* impl = reinterpret_cast<FluidHandleImpl*>(Handle);
    const Solstice::Physics::GridLayout g = impl->Sim->GetGridLayout();
    if (g.Nx <= 0 || g.Ny <= 0 || g.Nz <= 0) {
        return SolsticeV1_ResultFailure;
    }
    float total = 0.0f;
    float maxAbs = 0.0f;
    int count = 0;
    for (int z = 1; z <= g.Nz; ++z) {
        for (int y = 1; y <= g.Ny; ++y) {
            for (int x = 1; x <= g.Nx; ++x) {
                const float div = std::abs(CellDivergence(*impl->Sim, x, y, z));
                total += div;
                maxAbs = std::max(maxAbs, div);
                ++count;
            }
        }
    }
    if (OutMeanAbsDivergence) {
        *OutMeanAbsDivergence = count > 0 ? total / static_cast<float>(count) : 0.0f;
    }
    if (OutMaxAbsDivergence) {
        *OutMaxAbsDivergence = maxAbs;
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
