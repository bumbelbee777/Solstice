#pragma once

#include "Fluid.hxx"
#include "../Solstice.hxx"

namespace Solstice::Physics {

class SOLSTICE_API NSSolver {
public:
    static void Resolve(FluidSimulation& fluid, float dt);

private:
    static void LinearSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c, int N);
    static void Diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt, int N);
    static void Project(std::vector<float>& velocX, std::vector<float>& velocY, std::vector<float>& velocZ, std::vector<float>& p, std::vector<float>& div, int N);
    static void Advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& velocX, const std::vector<float>& velocY, const std::vector<float>& velocZ, float dt, int N);
    static void SetBoundaries(int b, std::vector<float>& x, int N);
};

}