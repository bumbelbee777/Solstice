#include "../source/Solstice.hxx"
#include "../source/Math/Vector.hxx"
#include "../source/Math/Matrix.hxx"
#include "../source/Math/Quaternion.hxx"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <random>
#include <cmath>

using namespace Solstice;
using namespace Solstice::Math;

static std::atomic<int> g_TestPassed{0};
static std::atomic<int> g_TestFailed{0};

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "FAIL: " << message << " (at " << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
            g_TestFailed.fetch_add(1, std::memory_order_relaxed); \
            return false; \
        } \
    } while(0)

#define TEST_PASS(message) \
    do { \
        std::cout << "PASS: " << message << std::endl; \
        g_TestPassed.fetch_add(1, std::memory_order_relaxed); \
    } while(0)

// Check if two floats are approximately equal
bool AlmostEqual(float a, float b, float epsilon = 0.001f) {
    return std::abs(a - b) < epsilon;
}

// Test Vector3 operations in parallel
bool TestVectorOps() {
    const int NumThreads = 8;
    const int Iterations = 100000;
    
    std::vector<std::thread> Threads;
    std::atomic<bool> AllCorrect{true};

    auto Worker = [&]() {
        for (int i = 0; i < Iterations; ++i) {
            float f = static_cast<float>(i);
            Vec3 a(f, f*2.0f, f*3.0f);
            Vec3 b(1.0f, 1.0f, 1.0f);
            
            // SIMD Addition
            Vec3 c = a + b;
            if (!AlmostEqual(c.x, f + 1.0f) || !AlmostEqual(c.y, f*2.0f + 1.0f)) {
                AllCorrect.store(false, std::memory_order_relaxed);
                return;
            }

            // SIMD Dot Product
            float dot = a.Dot(b); // (f*1) + (f*2*1) + (f*3*1) = 6f
            if (!AlmostEqual(dot, 6.0f * f)) {
                AllCorrect.store(false, std::memory_order_relaxed);
            }

            // Cross Product
            Vec3 x(1, 0, 0);
            Vec3 y(0, 1, 0);
            Vec3 z = x.Cross(y); // Should be (0, 0, 1)
            if (!AlmostEqual(z.z, 1.0f)) {
                AllCorrect.store(false, std::memory_order_relaxed);
            }
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(Worker);
    }

    for (auto& T : Threads) {
        T.join();
    }

    TEST_ASSERT(AllCorrect.load(), "Vector SIMD operations check failed");
    TEST_PASS("Vector SIMD operations thread safety");
    return true;
}

// Test Matrix4 multiplication stress
bool TestMatrixOps() {
    const int NumThreads = 8;
    const int Iterations = 50000;
    
    std::vector<std::thread> Threads;
    std::atomic<bool> AllCorrect{true};

    auto Worker = [&]() {
        Matrix4 Identity; // Identity by default
        Matrix4 Scale = Matrix4::Scale(Vec3(2.0f, 2.0f, 2.0f));
        
        for (int i = 0; i < Iterations; ++i) {
            // Identity * Scale = Scale
            Matrix4 Result = Identity * Scale;
            
            if (!AlmostEqual(Result[0][0], 2.0f) || !AlmostEqual(Result[3][3], 1.0f)) {
                AllCorrect.store(false, std::memory_order_relaxed); 
                return;
            }

            // Manual check of a few elements
            if (!AlmostEqual(Result[1][1], 2.0f) || !AlmostEqual(Result[2][2], 2.0f)) {
                AllCorrect.store(false, std::memory_order_relaxed);
            }
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(Worker);
    }

    for (auto& T : Threads) {
        T.join();
    }

    TEST_ASSERT(AllCorrect.load(), "Matrix SIMD operations check failed");
    TEST_PASS("Matrix SIMD operations thread safety");
    return true;
}

// Test Quaternion rotation
bool TestQuaternionOps() {
    const int NumThreads = 8;
    const int Iterations = 50000;
    
    std::vector<std::thread> Threads;
    std::atomic<bool> AllCorrect{true};

    auto Worker = [&]() {
        Vec3 Up(0, 1, 0);
        // Rotate 90 degrees around X
        Quaternion Q = Quaternion::AngleAxis(90.0f * 3.14159f / 180.0f, Vec3(1, 0, 0));
        
        for (int i = 0; i < Iterations; ++i) {
            // Rotate the Up vector. Should become (0, 0, 1) roughly
            Vec3 Rotated = Q.Rotate(Up);
            
            if (std::abs(Rotated.y) > 0.01f || std::abs(Rotated.z - 1.0f) > 0.01f) {
                // Allow some epsilon for float math
                 AllCorrect.store(false, std::memory_order_relaxed);
            }
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(Worker);
    }

    for (auto& T : Threads) {
        T.join();
    }

    TEST_ASSERT(AllCorrect.load(), "Quaternion operations check failed");
    TEST_PASS("Quaternion operations thread safety");
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Solstice Math Safety Test" << std::endl;
    std::cout << "========================================" << std::endl;

    bool passed = true;
    passed &= TestVectorOps();
    passed &= TestMatrixOps();
    passed &= TestQuaternionOps();

    std::cout << std::endl;
    std::cout << "Tests Passed: " << g_TestPassed.load() << std::endl;
    std::cout << "Tests Failed: " << g_TestFailed.load() << std::endl;

    return g_TestFailed.load() == 0 ? 0 : 1;
}
