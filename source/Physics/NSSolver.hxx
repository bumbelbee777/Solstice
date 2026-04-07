#pragma once

#include "Fluid.hxx"
#include "../Solstice.hxx"

namespace Solstice::Physics {

class SOLSTICE_API NSSolver {
public:
    static void Resolve(FluidSimulation& fluid, float dt);

private:
    static void LinearSolveIsotropic(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c,
                                     const GridLayout& g);
    static void LinearSolveAnisotropicDiffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float ax,
                                              float ay, float az, const GridLayout& g);
    static void LinearSolveAnisotropicPressure(std::vector<float>& p, const std::vector<float>& div, const GridLayout& g);

    static void Diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt,
                        const GridLayout& g);
    static void DiffuseSpectralPeriodic(int b, std::vector<float>& x, const std::vector<float>& x0, float nu, float dt,
                                        const GridLayout& g);

    static void Project(std::vector<float>& velocX, std::vector<float>& velocY, std::vector<float>& velocZ,
                        std::vector<float>& p, std::vector<float>& div, const GridLayout& g);

    static void Advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& velocX,
                       const std::vector<float>& velocY, const std::vector<float>& velocZ, float dt, const GridLayout& g);
    static void AdvectMacCormack(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u,
                                 const std::vector<float>& v, const std::vector<float>& w, float dt, const GridLayout& g);
    static void AdvectRaymarch(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u,
                               const std::vector<float>& v, const std::vector<float>& w, float dt, const GridLayout& g,
                               const class FluidSparseData* sparse);

    static void ApplyTemporalReprojection(std::vector<float>& density, const std::vector<float>& history,
                                          const std::vector<float>& u, const std::vector<float>& v,
                                          const std::vector<float>& w, float dt, const GridLayout& g, float alpha);

    static void BitplaneStep(FluidSimulation& fluid, float dt, const GridLayout& g);

    static void SetBoundaries(int b, std::vector<float>& x, const GridLayout& g);

    static void ApplyBoussinesq(FluidSimulation& fluid, float dt, const GridLayout& g);
    static void SetThermalWallBoundaries(std::vector<float>& T, const GridLayout& g, const Math::Vec3& gridOrigin,
                                         const FluidThermalTuning& th, float tHotEffective, float tColdEffective);
};

} // namespace Solstice::Physics
