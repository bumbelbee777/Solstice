#pragma once

#include <cmath>
#include <cstdint>

// Platform detection
#if defined(__SSE4_1__) || defined(__AVX__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
    #define SOLSTICE_SIMD_SSE
    #include <immintrin.h>
#elif defined(__ARM_NEON__)
    #define SOLSTICE_SIMD_NEON
    #include <arm_neon.h>
#else
    #define SOLSTICE_SIMD_SCALAR
#endif

namespace Solstice::Core::SIMD {

// Vec4 - Cross-platform SIMD vector
struct Vec4 {
#ifdef SOLSTICE_SIMD_SSE
    __m128 data;
    Vec4() : data(_mm_setzero_ps()) {}
    Vec4(__m128 v) : data(v) {}
    Vec4(float x, float y, float z, float w) : data(_mm_set_ps(w, z, y, x)) {}
    
    static Vec4 Load(const float* ptr) { return Vec4(_mm_loadu_ps(ptr)); }
    void Store(float* ptr) const { _mm_storeu_ps(ptr, data); }
    
    Vec4 operator+(const Vec4& o) const { return Vec4(_mm_add_ps(data, o.data)); }
    Vec4 operator-(const Vec4& o) const { return Vec4(_mm_sub_ps(data, o.data)); }
    Vec4 operator*(const Vec4& o) const { return Vec4(_mm_mul_ps(data, o.data)); }
    Vec4 operator*(float s) const { return Vec4(_mm_mul_ps(data, _mm_set1_ps(s))); }
    
    float Dot(const Vec4& o) const {
        __m128 mul = _mm_mul_ps(data, o.data);
        __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
        __m128 sums = _mm_add_ps(mul, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        return _mm_cvtss_f32(sums);
    }
    
    float X() const { return _mm_cvtss_f32(data); }
    float Y() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, 1)); }
    float Z() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, 2)); }
    float W() const { return _mm_cvtss_f32(_mm_shuffle_ps(data, data, 3)); }
    
    // Vec3 operations (treating W as 0)
    float Magnitude3() const {
        __m128 sq = _mm_mul_ps(data, data);
        __m128 shuf = _mm_shuffle_ps(sq, sq, _MM_SHUFFLE(2, 1, 0, 3)); // x,y,z,w -> z,y,x,w
        __m128 sum = _mm_add_ss(sq, shuf); // x+z
        shuf = _mm_shuffle_ps(sq, sq, 1); // get y
        sum = _mm_add_ss(sum, shuf); // x+z+y
        return std::sqrt(_mm_cvtss_f32(sum));
    }
    
    Vec4 Normalized3() const {
        float mag = Magnitude3();
        return (mag > 1e-6f) ? (*this * (1.0f / mag)) : Vec4(0,0,0,0);
    }
    
    // Cross product (3D, ignores W)
    Vec4 Cross(const Vec4& o) const {
        // result.x = y*o.z - z*o.y
        // result.y = z*o.x - x*o.z
        // result.z = x*o.y - y*o.x
        __m128 a_yzx = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 b_yzx = _mm_shuffle_ps(o.data, o.data, _MM_SHUFFLE(3, 0, 2, 1));
        __m128 a_zxy = _mm_shuffle_ps(data, data, _MM_SHUFFLE(3, 1, 0, 2));
        __m128 b_zxy = _mm_shuffle_ps(o.data, o.data, _MM_SHUFFLE(3, 1, 0, 2));
        __m128 prod1 = _mm_mul_ps(a_yzx, b_zxy);
        __m128 prod2 = _mm_mul_ps(a_zxy, b_yzx);
        return Vec4(_mm_sub_ps(prod1, prod2));
    }
#else
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    
    static Vec4 Load(const float* p) { return Vec4(p[0], p[1], p[2], p[3]); }
    void Store(float* p) const { p[0] = x; p[1] = y; p[2] = z; p[3] = w; }
    
    Vec4 operator+(const Vec4& o) const { return Vec4(x+o.x, y+o.y, z+o.z, w+o.w); }
    Vec4 operator-(const Vec4& o) const { return Vec4(x-o.x, y-o.y, z-o.z, w-o.w); }
    Vec4 operator*(const Vec4& o) const { return Vec4(x*o.x, y*o.y, z*o.z, w*o.w); }
    Vec4 operator*(float s) const { return Vec4(x*s, y*s, z*s, w*s); }
    
    float Dot(const Vec4& o) const { return x*o.x + y*o.y + z*o.z + w*o.w; }
    
    float X() const { return x; }
    float Y() const { return y; }
    float Z() const { return z; }
    float W() const { return w; }
    
    // Vec3 operations (treating W as 0)
    float Magnitude3() const { return std::sqrt(x*x + y*y + z*z); }
    Vec4 Normalized3() const {
        float mag = Magnitude3();
        return (mag > 1e-6f) ? (*this * (1.0f / mag)) : Vec4(0,0,0,0);
    }
    Vec4 Cross(const Vec4& o) const {
        return Vec4(y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x, 0);
    }
#endif
};

} // namespace Solstice::Core::SIMD