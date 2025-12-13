#include "NSSolver.hxx"
#include <cmath>
#include <algorithm>
#include "../Core/SIMD.hxx"

namespace Solstice::Physics {

using namespace ::Solstice::Core::SIMD;

// Helper macros for indexing
#define IX(x, y, z) ((x) + (N + 2) * (y) + (N + 2) * (N + 2) * (z))

void NSSolver::Resolve(FluidSimulation& fluid, float dt) {
    int N = fluid.GetSize();
    float visc = fluid.visc;
    float diff = fluid.diff;
    
    auto& Vx = fluid.Vx;
    auto& Vy = fluid.Vy;
    auto& Vz = fluid.Vz;
    auto& Vx0 = fluid.Vx0;
    auto& Vy0 = fluid.Vy0;
    auto& Vz0 = fluid.Vz0;
    auto& s = fluid.density;
    auto& density_prev = fluid.density_prev;
    
    // Velocity Step
    Diffuse(1, Vx0, Vx, visc, dt, N);
    Diffuse(2, Vy0, Vy, visc, dt, N);
    Diffuse(3, Vz0, Vz, visc, dt, N);
    
    Project(Vx0, Vy0, Vz0, Vx, Vy, N);
    
    Advect(1, Vx, Vx0, Vx0, Vy0, Vz0, dt, N);
    Advect(2, Vy, Vy0, Vx0, Vy0, Vz0, dt, N);
    Advect(3, Vz, Vz0, Vx0, Vy0, Vz0, dt, N);
    
    Project(Vx, Vy, Vz, Vx0, Vy0, N);
    
    // Density Step
    Diffuse(0, density_prev, s, diff, dt, N);
    Advect(0, s, density_prev, Vx, Vy, Vz, dt, N);
}

void NSSolver::LinearSolve(int b, std::vector<float>& x, const std::vector<float>& x0, float a, float c, int N) {
    float invC = 1.0f / c;
    // Standard Gauss-Seidel for stability (SIMD tricky here without huge effort, sticking to scalar for correctness)
    for (int k = 0; k < 20; k++) {
        for (int m = 1; m <= N; m++) {
            for (int j = 1; j <= N; j++) {
                for (int i = 1; i <= N; i++) {
                    x[IX(i, j, m)] = (x0[IX(i, j, m)]
                        + a * (x[IX(i+1, j, m)] + x[IX(i-1, j, m)]
                             + x[IX(i, j+1, m)] + x[IX(i, j-1, m)]
                             + x[IX(i, j, m+1)] + x[IX(i, j, m-1)])) * invC;
                }
            }
        }
        SetBoundaries(b, x, N);
    }
}

void NSSolver::Diffuse(int b, std::vector<float>& x, const std::vector<float>& x0, float diff, float dt, int N) {
    float a = dt * diff * (N * N * N);
    LinearSolve(b, x, x0, a, 1 + 6 * a, N);
}

void NSSolver::Project(std::vector<float>& velocX, std::vector<float>& velocY, std::vector<float>& velocZ, std::vector<float>& p, std::vector<float>& div, int N) {
    // SIMD Optimized Divergence Calculation
    for (int m = 1; m <= N; m++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            // Process 4 elements at a time
            for (; i <= N - 3; i += 4) {
                 int idx = IX(i, j, m);
                 
                 // Load neighbors
                 // u[i+1], u[i-1]
                 Vec4 u_next = Vec4::Load(&velocX[idx + 1]);
                 Vec4 u_prev = Vec4::Load(&velocX[idx - 1]);
                 
                 // v[j+1], v[j-1]
                 Vec4 v_next = Vec4::Load(&velocY[IX(i, j + 1, m)]);
                 Vec4 v_prev = Vec4::Load(&velocY[IX(i, j - 1, m)]);
                 
                 // w[k+1], w[k-1]
                 Vec4 w_next = Vec4::Load(&velocZ[IX(i, j, m + 1)]);
                 Vec4 w_prev = Vec4::Load(&velocZ[IX(i, j, m - 1)]);
                 
                 Vec4 u_diff = u_next - u_prev;
                 Vec4 v_diff = v_next - v_prev;
                 Vec4 w_diff = w_next - w_prev;
                 
                 Vec4 divergence = (u_diff + v_diff + w_diff) * -0.5f * (1.0f / N);
                 divergence.Store(&div[idx]);
                 
                 Vec4 zero(0,0,0,0);
                 zero.Store(&p[idx]);
            }
            // Tail
            for (; i <= N; i++) {
                div[IX(i, j, m)] = -0.5f * (
                         velocX[IX(i+1, j, m)] - velocX[IX(i-1, j, m)]
                       + velocY[IX(i, j+1, m)] - velocY[IX(i, j-1, m)]
                       + velocZ[IX(i, j, m+1)] - velocZ[IX(i, j, m-1)]
                    ) / N;
                p[IX(i, j, m)] = 0;
            }
        }
    }
    
    SetBoundaries(0, div, N);
    SetBoundaries(0, p, N);
    
    LinearSolve(0, p, div, 1, 6, N);
    
    // SIMD Optimized Gradient Subtraction
    for (int m = 1; m <= N; m++) {
        for (int j = 1; j <= N; j++) {
            int i = 1;
            for (; i <= N - 3; i += 4) {
                int idx = IX(i, j, m);
                
                Vec4 velX = Vec4::Load(&velocX[idx]);
                Vec4 velY = Vec4::Load(&velocY[idx]);
                Vec4 velZ = Vec4::Load(&velocZ[idx]);
                
                Vec4 p_next_x = Vec4::Load(&p[idx + 1]);
                Vec4 p_prev_x = Vec4::Load(&p[idx - 1]);
                
                Vec4 p_next_y = Vec4::Load(&p[IX(i, j + 1, m)]);
                Vec4 p_prev_y = Vec4::Load(&p[IX(i, j - 1, m)]);
                
                Vec4 p_next_z = Vec4::Load(&p[IX(i, j, m + 1)]);
                Vec4 p_prev_z = Vec4::Load(&p[IX(i, j, m - 1)]);
                
                float factor = 0.5f * N;
                
                velocX[idx] -= 0.5f * (p[idx+1] - p[idx-1]) * N; // Oops, scalar logic mixed. 
                // Let's do pure SIMD
                
                Vec4 gX = (p_next_x - p_prev_x) * factor;
                Vec4 gY = (p_next_y - p_prev_y) * factor;
                Vec4 gZ = (p_next_z - p_prev_z) * factor;
                
                (velX - gX).Store(&velocX[idx]);
                (velY - gY).Store(&velocY[idx]);
                (velZ - gZ).Store(&velocZ[idx]);
            }
            
            for (; i <= N; i++) {
                velocX[IX(i, j, m)] -= 0.5f * (p[IX(i+1, j, m)] - p[IX(i-1, j, m)]) * N;
                velocY[IX(i, j, m)] -= 0.5f * (p[IX(i, j+1, m)] - p[IX(i, j-1, m)]) * N;
                velocZ[IX(i, j, m)] -= 0.5f * (p[IX(i, j, m+1)] - p[IX(i, j, m-1)]) * N;
            }
        }
    }
    
    SetBoundaries(1, velocX, N);
    SetBoundaries(2, velocY, N);
    SetBoundaries(3, velocZ, N);
}

void NSSolver::Advect(int b, std::vector<float>& d, const std::vector<float>& d0, const std::vector<float>& u, const std::vector<float>& v, const std::vector<float>& w, float dt, int N) {
    float i0, j0, k0, i1, j1, k1;
    float x, y, z, s0, s1, t0, t1, r0, r1;
    float dt0 = dt * N;
    
    // Process scalar for Advect due to complexity of random access
    for (int k = 1; k <= N; k++) {
        for (int j = 1; j <= N; j++) {
            for (int i = 1; i <= N; i++) {
                x = i - dt0 * u[IX(i, j, k)];
                y = j - dt0 * v[IX(i, j, k)];
                z = k - dt0 * w[IX(i, j, k)];
                
                if (x < 0.5f) x = 0.5f; 
                if (x > N + 0.5f) x = N + 0.5f; 
                i0 = floorf(x); 
                i1 = i0 + 1.0f;
                
                if (y < 0.5f) y = 0.5f; 
                if (y > N + 0.5f) y = N + 0.5f; 
                j0 = floorf(y); 
                j1 = j0 + 1.0f;
                
                if (z < 0.5f) z = 0.5f; 
                if (z > N + 0.5f) z = N + 0.5f; 
                k0 = floorf(z); 
                k1 = k0 + 1.0f;
                
                s1 = x - i0; 
                s0 = 1.0f - s1; 
                t1 = y - j0; 
                t0 = 1.0f - t1; 
                r1 = z - k0; 
                r0 = 1.0f - r1;
                
                int i0i = (int)i0;
                int i1i = (int)i1;
                int j0i = (int)j0;
                int j1i = (int)j1;
                int k0i = (int)k0;
                int k1i = (int)k1;
                
                d[IX(i, j, k)] = 
                    s0 * (t0 * (r0 * d0[IX(i0i, j0i, k0i)] + r1 * d0[IX(i0i, j0i, k1i)]) +
                          t1 * (r0 * d0[IX(i0i, j1i, k0i)] + r1 * d0[IX(i0i, j1i, k1i)])) +
                    s1 * (t0 * (r0 * d0[IX(i1i, j0i, k0i)] + r1 * d0[IX(i1i, j0i, k1i)]) +
                          t1 * (r0 * d0[IX(i1i, j1i, k0i)] + r1 * d0[IX(i1i, j1i, k1i)]));
            }
        }
    }
    SetBoundaries(b, d, N);
}

void NSSolver::SetBoundaries(int b, std::vector<float>& x, int N) {
    // Faces
    for (int m = 1; m <= N; m++) {
        for (int i = 1; i <= N; i++) {
            x[IX(i, 0, m)]   = (b == 2) ? -x[IX(i, 1, m)] : x[IX(i, 1, m)];
            x[IX(i, N+1, m)] = (b == 2) ? -x[IX(i, N, m)] : x[IX(i, N, m)];
            
            x[IX(i, m, 0)]   = (b == 3) ? -x[IX(i, m, 1)] : x[IX(i, m, 1)];
            x[IX(i, m, N+1)] = (b == 3) ? -x[IX(i, m, N)] : x[IX(i, m, N)];
            
            x[IX(0, i, m)]   = (b == 1) ? -x[IX(1, i, m)] : x[IX(1, i, m)];
            x[IX(N+1, i, m)] = (b == 1) ? -x[IX(N, i, m)] : x[IX(N, i, m)];
        }
    }
    
    // Corners
    x[IX(0, 0, 0)]       = 0.33f * (x[IX(1, 0, 0)] + x[IX(0, 1, 0)] + x[IX(0, 0, 1)]);
    x[IX(0, N+1, 0)]     = 0.33f * (x[IX(1, N+1, 0)] + x[IX(0, N, 0)] + x[IX(0, N+1, 1)]);
    x[IX(0, 0, N+1)]     = 0.33f * (x[IX(1, 0, N+1)] + x[IX(0, 1, N+1)] + x[IX(0, 0, N)]);
    x[IX(0, N+1, N+1)]   = 0.33f * (x[IX(1, N+1, N+1)] + x[IX(0, N, N+1)] + x[IX(0, N+1, N)]);
    x[IX(N+1, 0, 0)]     = 0.33f * (x[IX(N, 0, 0)] + x[IX(N+1, 1, 0)] + x[IX(N+1, 0, 1)]);
    x[IX(N+1, N+1, 0)]   = 0.33f * (x[IX(N, N+1, 0)] + x[IX(N+1, N, 0)] + x[IX(N+1, N+1, 1)]);
    x[IX(N+1, 0, N+1)]   = 0.33f * (x[IX(N, 0, N+1)] + x[IX(N+1, 1, N+1)] + x[IX(N+1, 0, N)]);
    x[IX(N+1, N+1, N+1)] = 0.33f * (x[IX(N, N+1, N+1)] + x[IX(N+1, N, N+1)] + x[IX(N+1, N+1, N)]);
}

}
