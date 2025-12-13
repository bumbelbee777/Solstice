#include "Fluid.hxx"
#include "NSSolver.hxx"
#include <algorithm>

namespace Solstice::Physics {

FluidSimulation::FluidSimulation(int size, float diffusion, float viscosity)
    : N(size), diff(diffusion), visc(viscosity) {
    int total_size = (N + 2) * (N + 2) * (N + 2);
    density.resize(total_size, 0.0f);
    density_prev.resize(total_size, 0.0f);
    Vx.resize(total_size, 0.0f);
    Vy.resize(total_size, 0.0f);
    Vz.resize(total_size, 0.0f);
    Vx0.resize(total_size, 0.0f);
    Vy0.resize(total_size, 0.0f);
    Vz0.resize(total_size, 0.0f);
}

void FluidSimulation::AddDensity(int x, int y, int z, float amount) {
    int index = (x + 1) + (y + 1) * (N + 2) + (z + 1) * (N + 2) * (N + 2);
    if (index >= 0 && index < density.size()) {
        density[index] += amount;
    }
}

void FluidSimulation::AddVelocity(int x, int y, int z, float amountX, float amountY, float amountZ) {
    int index = (x + 1) + (y + 1) * (N + 2) + (z + 1) * (N + 2) * (N + 2);
    if (index >= 0 && index < Vx.size()) {
        Vx[index] += amountX;
        Vy[index] += amountY;
        Vz[index] += amountZ;
    }
}

void FluidSimulation::Step(float dt) {
    NSSolver::Resolve(*this, dt);
}

Math::Vec3 FluidSimulation::GetVelocityAt(int x, int y, int z) const
{
    int index = (x + 1) + (y + 1) * (N + 2) + (z + 1) * (N + 2) * (N + 2);
    if (index >= 0 && index < Vx.size()) {
        return Math::Vec3(Vx[index], Vy[index], Vz[index]);
    }
    return Math::Vec3(0, 0, 0);
}

Math::Vec3 FluidSimulation::SampleVelocity(const Math::Vec3& pos) const
{
    // Simple nearest neighbor or trilinear
    // Assuming pos is in grid coords (0..N)
    
    int x = (int)pos.x;
    int y = (int)pos.y;
    int z = (int)pos.z;
    
    if (x < 0) x = 0; if (x >= N) x = N - 1;
    if (y < 0) y = 0; if (y >= N) y = N - 1;
    if (z < 0) z = 0; if (z >= N) z = N - 1;
    
    return GetVelocityAt(x, y, z);
}

}
