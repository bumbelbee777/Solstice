#pragma once

#include <cstdint>
#include <vector>

namespace Solstice::Physics {

struct GridLayout;

/// Block-aligned occupancy bitmap + shallow octree bitmask for skipping empty regions.
struct FluidSparseData {
    int Nx = 0;
    int Ny = 0;
    int Nz = 0;
    int blockSize = 4;
    int blocksX = 0;
    int blocksY = 0;
    int blocksZ = 0;
    /// One bit per cell in each block (blockSize^3 <= 64 for blockSize=4).
    std::vector<uint64_t> blockOccupancy;

    void Resize(int nx, int ny, int nz, int b);
    void BuildFromFields(const std::vector<float>& density, const std::vector<float>& vx,
                         const std::vector<float>& vy, const std::vector<float>& vz, float threshold,
                         const GridLayout& g);

    bool IsBlockEmpty(int bx, int by, int bz) const;
    int BlockIndex(int bx, int by, int bz) const;

    /// Child bitmask for octree node covering [min,max] in block coordinates (recursive build uses blockOccupancy).
    uint8_t BuildChildMask(int minBx, int minBy, int minBz, int extent) const;
};

} // namespace Solstice::Physics
