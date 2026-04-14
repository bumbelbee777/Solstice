#include "../source/Core/System/Async.hxx"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

using namespace Solstice;
using namespace Solstice::Core;

static std::atomic<int> g_TestPassed{0};
static std::atomic<int> g_TestFailed{0};

#define TEST_PASS(message) \
    do { \
        std::cout << "PASS: " << message << std::endl; \
        g_TestPassed.fetch_add(1, std::memory_order_relaxed); \
    } while(0)

#define TEST_FAIL(message) \
    do { \
        std::cerr << "FAIL: " << message << " (at " << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        g_TestFailed.fetch_add(1, std::memory_order_relaxed); \
    } while(0)

// Recursive job to test depth and stack usage
void RecursiveJob(std::atomic<int>& counter, int depth) {
    counter.fetch_add(1, std::memory_order_relaxed);
    if (depth > 0) {
        JobSystem::Instance().Submit([&counter, depth]() {
            RecursiveJob(counter, depth - 1);
        });
        JobSystem::Instance().Submit([&counter, depth]() {
            RecursiveJob(counter, depth - 1);
        });
    }
}

// 1. Massive Submission Test
bool TestMassiveSubmission() {
    std::atomic<int> Counter{0};
    const int NumJobs = 100000;
    
    auto Start = std::chrono::high_resolution_clock::now();
    
    // Batch submission if possible, or just tight loop
    for(int i=0; i<NumJobs; ++i) {
        JobSystem::Instance().Submit([&Counter](){
            Counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    
    // Wait for completion - crude wait for this test since we don't have a single fence
    // In a real usage we might use a Counter/Fence, but here we just wait until value matches
    int WaitCount = 0;
    while(Counter.load() < NumJobs && WaitCount < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }
    
    auto End = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(End - Start).count();
    
    if (Counter.load() == NumJobs) {
        TEST_PASS("Massive 100k job submission completed in " + std::to_string(Duration) + "ms");
        return true;
    } else {
        TEST_FAIL("Massive job submission incomplete: " + std::to_string(Counter.load()) + "/" + std::to_string(NumJobs));
        return false;
    }
}

// 2. Recursive/Tree Job Test
bool TestRecursiveJobs() {
    std::atomic<int> Counter{0};
    int Depth = 10; 
    // Tree of depth 10, branching factor 2. Total nodes = 2^(D+1) - 1.
    // 2^11 - 1 = 2047 jobs.
    
    JobSystem::Instance().Submit([&Counter, Depth]() {
        RecursiveJob(Counter, Depth);
    });
    
    // Expected count
    int Expected = 0;
    for(int i=0; i<=Depth; ++i) {
        Expected += (1 << i); // 1 + 2 + 4 ...
    }
    // Actually our recursive logic is: 
    // Node -> add 1. If depth > 0, spawn 2 children.
    // D=0: 1 node.
    // D=1: 1 root + 2 children = 3 nodes.
    // Yes, 2^(D+1) - 1.
    int ExpectedNodes = (1 << (Depth + 1)) - 1;

    int WaitCount = 0;
    while(Counter.load() < ExpectedNodes && WaitCount < 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        WaitCount++;
    }

    if (Counter.load() == ExpectedNodes) {
        TEST_PASS("Recursive job tree (Depth " + std::to_string(Depth) + ") completed");
        return true;
    } else {
        TEST_FAIL("Recursive jobs incomplete: " + std::to_string(Counter.load()) + "/" + std::to_string(ExpectedNodes));
        return false;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Solstice Job System Stress Test" << std::endl;
    std::cout << "========================================" << std::endl;

    // Ensure JobSystem is initialized
    JobSystem::Instance().Initialize();

    bool passed = true;
    passed &= TestMassiveSubmission();
    passed &= TestRecursiveJobs();

    JobSystem::Instance().Shutdown();

    std::cout << std::endl;
    std::cout << "Tests Passed: " << g_TestPassed.load() << std::endl;
    std::cout << "Tests Failed: " << g_TestFailed.load() << std::endl;

    return g_TestFailed.load() == 0 ? 0 : 1;
}
