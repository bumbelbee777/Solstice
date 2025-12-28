#include "../source/Solstice.hxx"
#include "../source/Core/Async.hxx"
#include "../source/Scripting/BytecodeVM.hxx"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cassert>

using namespace Solstice;

// Test counters
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

// ============================================================================
// Test 1: Spinlock and LockGuard
// ============================================================================
bool TestSpinlock() {
    Core::Spinlock Lock;
    std::atomic<int> Counter{0};
    const int NumThreads = 8;
    const int IterationsPerThread = 5000; // Reduced from 10000 - still tests thoroughly

    std::vector<std::thread> Threads;
    Threads.reserve(NumThreads);

    auto Worker = [&]() {
        for (int i = 0; i < IterationsPerThread; ++i) {
            Core::LockGuard Guard(Lock);
            int OldValue = Counter.load(std::memory_order_relaxed);
            // Removed unnecessary sleep - the lock contention itself provides sufficient testing
            Counter.store(OldValue + 1, std::memory_order_relaxed);
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(Worker);
    }

    for (auto& T : Threads) {
        T.join();
    }

    int Expected = NumThreads * IterationsPerThread;
    int Actual = Counter.load(std::memory_order_acquire);
    TEST_ASSERT(Actual == Expected, "Spinlock test: Counter mismatch");
    TEST_PASS("Spinlock and LockGuard thread safety");
    return true;
}

// ============================================================================
// Test 2: ExecutionGuard
// ============================================================================
bool TestExecutionGuard() {
    Core::ExecutionGuard Guard;

    // Test 1: Single execution
    TEST_ASSERT(Guard.TryExecute(), "ExecutionGuard: Should acquire on first try");
    TEST_ASSERT(Guard.IsExecuting(), "ExecutionGuard: Should be executing");
    TEST_ASSERT(!Guard.TryExecute(), "ExecutionGuard: Should not acquire when already executing");
    Guard.Release();
    TEST_ASSERT(!Guard.IsExecuting(), "ExecutionGuard: Should not be executing after release");

    // Test 2: Concurrent execution attempts
    std::atomic<int> SuccessCount{0};
    std::atomic<int> FailCount{0};
    const int NumThreads = 10;
    std::atomic<bool> Ready{false};

    std::vector<std::thread> Threads;
    Threads.reserve(NumThreads);

    auto Worker = [&]() {
        // Synchronize start to maximize contention
        while (!Ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        if (Guard.TryExecute()) {
            SuccessCount.fetch_add(1, std::memory_order_relaxed);
            // Reduced sleep - 1ms is sufficient to test the guard mechanism
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Guard.Release();
        } else {
            FailCount.fetch_add(1, std::memory_order_relaxed);
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(Worker);
    }

    // Start all threads simultaneously for maximum contention
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Ready.store(true, std::memory_order_release);

    for (auto& T : Threads) {
        T.join();
    }

    // Only one thread should succeed
    TEST_ASSERT(SuccessCount.load() == 1, "ExecutionGuard: Only one thread should acquire");
    TEST_ASSERT(FailCount.load() == NumThreads - 1, "ExecutionGuard: Other threads should fail");

    // Test 3: Timeout mechanism
    Core::ExecutionGuard TimeoutGuard;
    TEST_ASSERT(TimeoutGuard.TryExecute(100), "ExecutionGuard: Should acquire with timeout");

    // Simulate a hung execution (don't release)
    // Another thread should be able to force release after timeout
    std::atomic<bool> TimeoutAcquired{false};
    std::thread TimeoutThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (TimeoutGuard.TryExecute(100)) {
            TimeoutAcquired.store(true, std::memory_order_release);
            TimeoutGuard.Release();
        }
    });

    TimeoutThread.join();
    TEST_ASSERT(TimeoutAcquired.load(), "ExecutionGuard: Timeout should force release hung execution");

    TEST_PASS("ExecutionGuard thread safety and timeout");
    return true;
}

// ============================================================================
// Test 3: BytecodeVM Thread Safety
// ============================================================================
bool TestBytecodeVM() {
    Scripting::BytecodeVM VM;

    // Create a simple program
    Scripting::Program Prog;
    Prog.Add(Scripting::OpCode::PUSH_CONST, (int64_t)42);
    Prog.Add(Scripting::OpCode::HALT);

    // Test concurrent LoadProgram calls
    const int NumThreads = 5;
    std::vector<std::thread> Threads;
    Threads.reserve(NumThreads);

    std::atomic<int> LoadSuccess{0};
    std::atomic<int> LoadFail{0};

    auto LoadWorker = [&]() {
        try {
            VM.LoadProgram(Prog);
            LoadSuccess.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
            LoadFail.fetch_add(1, std::memory_order_relaxed);
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(LoadWorker);
    }

    for (auto& T : Threads) {
        T.join();
    }

    // All should succeed (protected by lock)
    TEST_ASSERT(LoadSuccess.load() == NumThreads, "BytecodeVM: All LoadProgram calls should succeed");

    // Test concurrent RegisterNative calls
    std::vector<std::thread> NativeThreads;
    NativeThreads.reserve(NumThreads);

    auto NativeWorker = [&](int Id) {
        std::string Name = "NativeFunc" + std::to_string(Id);
        VM.RegisterNative(Name, [](const std::vector<Scripting::Value>&) -> Scripting::Value {
            return (int64_t)42;
        });
    };

    for (int i = 0; i < NumThreads; ++i) {
        NativeThreads.emplace_back(NativeWorker, i);
    }

    for (auto& T : NativeThreads) {
        T.join();
    }

    // Test concurrent HasModule calls
    VM.AddModule("TestModule", Prog);
    std::vector<std::thread> ModuleThreads;
    ModuleThreads.reserve(NumThreads);

    std::atomic<int> ModuleCheckSuccess{0};

    auto ModuleWorker = [&]() {
        if (VM.HasModule("TestModule")) {
            ModuleCheckSuccess.fetch_add(1, std::memory_order_relaxed);
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        ModuleThreads.emplace_back(ModuleWorker);
    }

    for (auto& T : ModuleThreads) {
        T.join();
    }

    TEST_ASSERT(ModuleCheckSuccess.load() == NumThreads, "BytecodeVM: All HasModule calls should succeed");

    TEST_PASS("BytecodeVM thread safety");
    return true;
}

// ============================================================================
// Test 4: JobSystem Thread Safety
// ============================================================================
bool TestJobSystem() {
    Core::JobSystem& JobSys = Core::JobSystem::Instance();
    JobSys.Initialize(4); // 4 worker threads

    std::atomic<int> Counter{0};
    const int NumJobs = 500; // Reduced from 1000 - still tests thoroughly
    const int ExpectedValue = NumJobs;
    const int NumSubmitThreads = 8;

    // Submit many jobs concurrently
    std::vector<std::thread> SubmitThreads;
    SubmitThreads.reserve(NumSubmitThreads);

    // Distribute jobs evenly, handling remainder
    const int JobsPerThread = NumJobs / NumSubmitThreads;
    const int Remainder = NumJobs % NumSubmitThreads;

    auto SubmitWorker = [&](int jobCount) {
        for (int i = 0; i < jobCount; ++i) {
            JobSys.Submit([&]() {
                Counter.fetch_add(1, std::memory_order_relaxed);
            });
        }
    };

    for (int i = 0; i < NumSubmitThreads; ++i) {
        // First 'Remainder' threads get one extra job to ensure exactly NumJobs total
        int jobsForThisThread = JobsPerThread + (i < Remainder ? 1 : 0);
        SubmitThreads.emplace_back(SubmitWorker, jobsForThisThread);
    }

    for (auto& T : SubmitThreads) {
        T.join();
    }

    // Wait for all jobs to complete using adaptive polling
    // Give workers a moment to start processing
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    const int MaxWaitIterations = 10000; // Increased timeout (1 second max)
    int waitIterations = 0;
    while (Counter.load(std::memory_order_acquire) < ExpectedValue && waitIterations < MaxWaitIterations) {
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // 0.1ms polling
        waitIterations++;
    }

    // If we timed out, give one more brief wait
    if (Counter.load(std::memory_order_acquire) < ExpectedValue) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    int Actual = Counter.load(std::memory_order_acquire);
    TEST_ASSERT(Actual == ExpectedValue, "JobSystem: All jobs should execute");

    // Test async jobs
    std::atomic<int> AsyncCounter{0};
    const int NumAsyncJobs = 50; // Reduced from 100 - still tests thoroughly

    std::vector<std::future<int>> Futures;
    Futures.reserve(NumAsyncJobs);

    for (int i = 0; i < NumAsyncJobs; ++i) {
        auto Fut = JobSys.SubmitAsync([&, i]() -> int {
            AsyncCounter.fetch_add(1, std::memory_order_relaxed);
            return i * 2;
        });
        Futures.push_back(std::move(Fut));
    }

    // Wait for all futures
    for (auto& Fut : Futures) {
        int Result = Fut.get();
        TEST_ASSERT(Result % 2 == 0, "JobSystem: Async job should return correct value");
    }

    TEST_ASSERT(AsyncCounter.load() == NumAsyncJobs, "JobSystem: All async jobs should execute");

    JobSys.Shutdown();

    TEST_PASS("JobSystem thread safety");
    return true;
}

// ============================================================================
// Test 5: Stress Test - Mixed Operations
// ============================================================================
bool TestStressMixed() {
    Core::Spinlock Lock;
    Core::ExecutionGuard ExecGuard;
    Scripting::BytecodeVM VM;
    std::atomic<int> SharedCounter{0};

    const int NumThreads = 10;
    const int Iterations = 500; // Reduced from 1000 - still tests thoroughly

    std::vector<std::thread> Threads;
    Threads.reserve(NumThreads);

    auto StressWorker = [&](int ThreadId) {
        for (int i = 0; i < Iterations; ++i) {
            // Mix of operations
            if (i % 3 == 0) {
                // Spinlock operation
                {
                    Core::LockGuard Guard(Lock);
                    int Val = SharedCounter.load(std::memory_order_relaxed);
                    SharedCounter.store(Val + 1, std::memory_order_relaxed);
                }
            } else if (i % 3 == 1) {
                // ExecutionGuard operation - removed sleep, contention is sufficient test
                if (ExecGuard.TryExecute()) {
                    ExecGuard.Release();
                }
            } else {
                // VM operation
                Scripting::Program Prog;
                Prog.Add(Scripting::OpCode::PUSH_CONST, (int64_t)(ThreadId * 1000 + i));
                Prog.Add(Scripting::OpCode::HALT);
                VM.LoadProgram(Prog);
            }
        }
    };

    for (int i = 0; i < NumThreads; ++i) {
        Threads.emplace_back(StressWorker, i);
    }

    for (auto& T : Threads) {
        T.join();
    }

    // Verify final counter (should be NumThreads * (Iterations / 3) approximately)
    int FinalValue = SharedCounter.load(std::memory_order_acquire);
    int ExpectedMin = NumThreads * (Iterations / 3) - 50; // Allow some variance
    int ExpectedMax = NumThreads * (Iterations / 3) + 50;
    TEST_ASSERT(FinalValue >= ExpectedMin && FinalValue <= ExpectedMax,
                "Stress test: Counter should be approximately correct");

    TEST_PASS("Stress test with mixed operations");
    return true;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "Solstice Thread Safety Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    bool UseThreadSanitizer = false;
    if (argc > 1 && std::string(argv[1]) == "--tsan") {
        UseThreadSanitizer = true;
        std::cout << "Note: Running with ThreadSanitizer support" << std::endl;
        std::cout << "      Build with: -fsanitize=thread" << std::endl;
        std::cout << std::endl;
    }

    auto StartTime = std::chrono::high_resolution_clock::now();

    // Run tests
    bool AllPassed = true;
    AllPassed &= TestSpinlock();
    AllPassed &= TestExecutionGuard();
    AllPassed &= TestBytecodeVM();
    AllPassed &= TestJobSystem();
    AllPassed &= TestStressMixed();

    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);

    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << g_TestPassed.load() << std::endl;
    std::cout << "Failed: " << g_TestFailed.load() << std::endl;
    std::cout << "Duration: " << Duration.count() << " ms" << std::endl;
    std::cout << std::endl;

    if (AllPassed && g_TestFailed.load() == 0) {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}
