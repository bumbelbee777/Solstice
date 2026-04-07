#include "FluidSparse.hxx"
#include "Fluid.hxx"
#include <algorithm>
#include <cmath>

namespace Solstice::Physics {

void FluidSparseData::Resize(int nx, int ny, int nz, int b) {
    Nx = nx;
    Ny = ny;
    Nz = nz;
    blockSize = std::max(2, std::min(b, 8));
    blocksX = std::max(1, (Nx + blockSize - 1) / blockSize);
    blocksY = std::max(1, (Ny + blockSize - 1) / blockSize);
    blocksZ = std::max(1, (Nz + blockSize - 1) / blockSize);
    const int nb = blocksX * blocksY * blocksZ;
    blockOccupancy.assign(static_cast<size_t>(nb), 0u);
}

int FluidSparseData::BlockIndex(int bx, int by, int bz) const {
    return bx + blocksX * (by + blocksY * bz);
}

bool FluidSparseData::IsBlockEmpty(int bx, int by, int bz) const {
    if (bx < 0 || by < 0 || bz < 0 || bx >= blocksX || by >= blocksY || bz >= blocksZ) {
        return true;
    }
    return blockOccupancy[static_cast<size_t>(BlockIndex(bx, by, bz))] == 0u;
}

void FluidSparseData::BuildFromFields(const std::vector<float>& density, const std::vector<float>& vx,
                                      const std::vector<float>& vy, const std::vector<float>& vz,
                                      float threshold, const GridLayout& g) {
    (void)vy;
    (void)vz;
    for (int bz = 0; bz < blocksZ; ++bz) {
        for (int by = 0; by < blocksY; ++by) {
            for (int bx = 0; bx < blocksX; ++bx) {
                uint64_t bits = 0u;
                int bit = 0;
                for (int dz = 0; dz < blockSize && bit < 64; ++dz) {
                    for (int dy = 0; dy < blockSize && bit < 64; ++dy) {
                        for (int dx = 0; dx < blockSize && bit < 64; ++dx) {
                            const int cx = bx * blockSize + dx + 1;
                            const int cy = by * blockSize + dy + 1;
                            const int cz = bz * blockSize + dz + 1;
                            if (cx <= Nx && cy <= Ny && cz <= Nz) {
                                const int idx = g.IX(cx, cy, cz);
                                const float d = std::abs(density[static_cast<size_t>(idx)]);
                                const float u = std::abs(vx[static_cast<size_t>(idx)]);
                                if (d > threshold || u > threshold) {
                                    bits |= (uint64_t{1} << bit);
                                }
                            }
                            ++bit;
                        }
                    }
                }
                blockOccupancy[static_cast<size_t>(BlockIndex(bx, by, bz))] = bits;
            }
        }
    }
}

uint8_t FluidSparseData::BuildChildMask(int minBx, int minBy, int minBz, int extent) const {
    uint8_t mask = 0;
    if (extent <= 1) {
        return IsBlockEmpty(minBx, minBy, minBz) ? 0u : 0xFFu;
    }
    const int half = extent / 2;
    for (int oz = 0; oz < 2; ++oz) {
        for (int oy = 0; oy < 2; ++oy) {
            for (int ox = 0; ox < 2; ++ox) {
                const int sub = ox + 2 * oy + 4 * oz;
                int nonempty = 0;
                for (int z = 0; z < half; ++z) {
                    for (int y = 0; y < half; ++y) {
                        for (int x = 0; x < half; ++x) {
                            const int bx = minBx + ox * half + x;
                            const int by = minBy + oy * half + y;
                            const int bz = minBz + oz * half + z;
                            if (!IsBlockEmpty(bx, by, bz)) {
                                nonempty = 1;
                            }
                        }
                    }
                }
                if (nonempty) {
                    mask |= static_cast<uint8_t>(1u << sub);
                }
            }
        }
    }
    return mask;
}

} // namespace Solstice::Physics
