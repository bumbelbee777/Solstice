#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace Solstice::Physics::FluidFFT {

inline bool IsPowerOf2(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

inline void FFT1D(std::vector<std::complex<float>>& a, bool inverse) {
    const int n = static_cast<int>(a.size());
    if (n <= 1) {
        return;
    }
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[static_cast<size_t>(i)], a[static_cast<size_t>(j)]);
        }
    }
    const float sign = inverse ? 1.0f : -1.0f;
    for (int len = 2; len <= n; len <<= 1) {
        constexpr float kPi = 3.14159265358979323846f;
        const float ang = sign * 2.0f * kPi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (int k = 0; k < len / 2; ++k) {
                const std::complex<float> u = a[static_cast<size_t>(i + k)];
                const std::complex<float> v = a[static_cast<size_t>(i + k + len / 2)] * w;
                a[static_cast<size_t>(i + k)] = u + v;
                a[static_cast<size_t>(i + k + len / 2)] = u - v;
                w *= wlen;
            }
        }
    }
    if (inverse) {
        const float inv = 1.0f / static_cast<float>(n);
        for (int i = 0; i < n; ++i) {
            a[static_cast<size_t>(i)] *= inv;
        }
    }
}

/// Layout: index(ix,iy,iz) = ix + N * (iy + N * iz), each in [0, N).
inline void FFT3D(std::vector<std::complex<float>>& data, int N, bool inverse) {
    const int n3 = N * N * N;
    if (static_cast<int>(data.size()) < n3) {
        data.resize(static_cast<size_t>(n3));
    }
    std::vector<std::complex<float>> line(static_cast<size_t>(N));
    if (!inverse) {
        for (int iz = 0; iz < N; ++iz) {
            for (int iy = 0; iy < N; ++iy) {
                for (int ix = 0; ix < N; ++ix) {
                    line[static_cast<size_t>(ix)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, false);
                for (int ix = 0; ix < N; ++ix) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(ix)];
                }
            }
        }
        for (int iz = 0; iz < N; ++iz) {
            for (int ix = 0; ix < N; ++ix) {
                for (int iy = 0; iy < N; ++iy) {
                    line[static_cast<size_t>(iy)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, false);
                for (int iy = 0; iy < N; ++iy) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(iy)];
                }
            }
        }
        for (int iy = 0; iy < N; ++iy) {
            for (int ix = 0; ix < N; ++ix) {
                for (int iz = 0; iz < N; ++iz) {
                    line[static_cast<size_t>(iz)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, false);
                for (int iz = 0; iz < N; ++iz) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(iz)];
                }
            }
        }
    } else {
        for (int iy = 0; iy < N; ++iy) {
            for (int ix = 0; ix < N; ++ix) {
                for (int iz = 0; iz < N; ++iz) {
                    line[static_cast<size_t>(iz)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, true);
                for (int iz = 0; iz < N; ++iz) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(iz)];
                }
            }
        }
        for (int iz = 0; iz < N; ++iz) {
            for (int ix = 0; ix < N; ++ix) {
                for (int iy = 0; iy < N; ++iy) {
                    line[static_cast<size_t>(iy)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, true);
                for (int iy = 0; iy < N; ++iy) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(iy)];
                }
            }
        }
        for (int iz = 0; iz < N; ++iz) {
            for (int iy = 0; iy < N; ++iy) {
                for (int ix = 0; ix < N; ++ix) {
                    line[static_cast<size_t>(ix)] = data[static_cast<size_t>(ix + N * (iy + N * iz))];
                }
                FFT1D(line, true);
                for (int ix = 0; ix < N; ++ix) {
                    data[static_cast<size_t>(ix + N * (iy + N * iz))] = line[static_cast<size_t>(ix)];
                }
            }
        }
    }
}

/// Periodic Laplacian eigenvalues (discrete, same scaling as classic 7-point stencil on torus).
inline float LaplacianEigenvaluePeriodic(int kx, int ky, int kz, int N) {
    const float pi = 3.14159265358979323846f;
    const float sx = std::sin(pi * static_cast<float>(kx) / static_cast<float>(N));
    const float sy = std::sin(pi * static_cast<float>(ky) / static_cast<float>(N));
    const float sz = std::sin(pi * static_cast<float>(kz) / static_cast<float>(N));
    const float n2 = static_cast<float>(N * N);
    return -4.0f * n2 * (sx * sx + sy * sy + sz * sz);
}

} // namespace Solstice::Physics::FluidFFT
