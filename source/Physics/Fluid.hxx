#pragma once

#include <vector>
#include "../Math/Vector.hxx"
#include "../Solstice.hxx"

namespace Solstice::Physics {

class NSSolver;

// Component for entities (Legacy/Simple fluid behavior)
struct Fluid {
    float Density = 1000.0f;
    float Viscosity = 0.5f;
};

// Grid-based Fluid Simulation
class SOLSTICE_API FluidSimulation {
public:
    FluidSimulation(int size, float diffusion, float viscosity);
    ~FluidSimulation() = default;

    void Step(float dt);

    void AddDensity(int x, int y, int z, float amount);
    void AddVelocity(int x, int y, int z, float amountX, float amountY, float amountZ);
    
    // Get velocity at normalized position (0..1) or world position? 
    // We'll assume local grid coordinates for now.
    Math::Vec3 GetVelocityAt(int x, int y, int z) const;
    Math::Vec3 SampleVelocity(const Math::Vec3& pos) const; // World space sample
    
    int GetSize() const { return N; }
    
    const std::vector<float>& GetDensity() const { return density; }
    const std::vector<float>& GetVx() const { return Vx; }
    const std::vector<float>& GetVy() const { return Vy; }
    const std::vector<float>& GetVz() const { return Vz; }
    
private:
    friend class NSSolver;
    
    int N;
    float diff;
    float visc;
    
    std::vector<float> density;
    std::vector<float> density_prev;
    
    std::vector<float> Vx;
    std::vector<float> Vy;
    std::vector<float> Vz;
    
    std::vector<float> Vx0;
    std::vector<float> Vy0;
    std::vector<float> Vz0;
};

}