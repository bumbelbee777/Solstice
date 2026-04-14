#pragma once

#include "Solstice.hxx"
#include <thread>
#include <atomic>
#include <future>
#include <functional>
#include <stdexcept>
#include <memory>
#include <queue>
#include <deque>
#include <vector>
#include <chrono>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace Solstice::Core {

// Simple spinlock implementation with backoff
struct Spinlock {
    std::atomic_flag Flag = ATOMIC_FLAG_INIT;

    void Lock() {
        while (Flag.test_and_set(std::memory_order_acquire)) {
            #if defined(_MSC_VER)
                _mm_pause();
            #else
                std::this_thread::yield();
            #endif
        }
    }

    void Unlock() {
        Flag.clear(std::memory_order_release);
    }

    void lock() { Lock(); }
    void unlock() { Unlock(); }

    Spinlock() = default;
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;
    Spinlock(Spinlock&&) noexcept = default;
    Spinlock& operator=(Spinlock&&) noexcept = default;
};

// RAII guard for Spinlock
struct LockGuard {
    Spinlock& lock;

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

    explicit LockGuard(Spinlock& l) : lock(l) {
        lock.Lock();
    }

    ~LockGuard() noexcept {
        lock.Unlock();
    }
};

// Execution guard with timeout support for preventing concurrent execution
struct ExecutionGuard {
    std::atomic<bool> m_Executing{false};
    std::atomic<std::chrono::high_resolution_clock::time_point> m_StartTime{};

    ExecutionGuard() = default;
    ExecutionGuard(const ExecutionGuard&) = delete;
    ExecutionGuard& operator=(const ExecutionGuard&) = delete;
    ExecutionGuard(ExecutionGuard&&) noexcept = default;
    ExecutionGuard& operator=(ExecutionGuard&&) noexcept = default;

    bool IsExecuting() const {
        return m_Executing.load(std::memory_order_acquire);
    }

    // Try to acquire execution lock, returns true if acquired
    // TimeoutMs: Maximum execution time in milliseconds (0 = no timeout check)
    bool TryExecute(uint32_t TimeoutMs = 0) {
        bool Expected = false;
        if (m_Executing.compare_exchange_strong(Expected, true, std::memory_order_acq_rel)) {
            m_StartTime.store(std::chrono::high_resolution_clock::now(), std::memory_order_release);
            return true;
        }

        // Check if previous execution timed out
        if (TimeoutMs > 0) {
            auto StartTime = m_StartTime.load(std::memory_order_acquire);
            auto Now = std::chrono::high_resolution_clock::now();
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Now - StartTime).count();
            if (Elapsed > TimeoutMs) {
                // Force release on timeout (previous execution hung)
                m_Executing.store(false, std::memory_order_release);
                m_StartTime.store(Now, std::memory_order_release);
                return true;
            }
        }

        return false;
    }

    void Release() {
        m_Executing.store(false, std::memory_order_release);
    }
};

// Simple job wrapper
template<class F>
struct Job {
    F Task;

    Job() = default;
    explicit Job(F t) : Task(std::move(t)) {}

    void Execute() { Task(); }

    explicit operator bool() const { return static_cast<bool>(Task); }
};

// Asynchronous job
template<typename R>
struct AsyncJob {
    std::function<R()> Task;
    std::shared_ptr<std::promise<R>> Promise;

    explicit AsyncJob(std::function<R()> t) : Task(std::move(t)), Promise(std::make_shared<std::promise<R>>()) {}

    std::future<R> Future() { return Promise->get_future(); }

    void Execute() {
        try {
            if constexpr (std::is_void_v<R>) {
                Task();
                Promise->set_value();
            } else {
                Promise->set_value(Task());
            }
        } catch (...) {
            Promise->set_exception(std::current_exception());
        }
    }
};

// Thread-safe job queue
template<class JobType>
struct ThreadJobQueue {
    std::deque<JobType> Queue;
    Spinlock Lock;

    ThreadJobQueue() = default;
    ThreadJobQueue(const ThreadJobQueue&) = delete;
    ThreadJobQueue& operator=(const ThreadJobQueue&) = delete;
    ThreadJobQueue(ThreadJobQueue&&) noexcept = default;
    ThreadJobQueue& operator=(ThreadJobQueue&&) noexcept = default;

    void PushLocal(JobType&& job) {
        LockGuard l(Lock);
        Queue.push_back(std::move(job));
    }

    bool PopLocal(JobType& outJob) {
        LockGuard l(Lock);
        if (Queue.empty()) return false;
        outJob = std::move(Queue.back());
        Queue.pop_back();
        return true;
    }

    bool Steal(JobType& outJob) {
        LockGuard l(Lock);
        if (Queue.empty()) return false;
        outJob = std::move(Queue.front());
        Queue.pop_front();
        return true;
    }
};

class SOLSTICE_API JobSystem {
public:
    static JobSystem& Instance() {
        static JobSystem instance;
        return instance;
    }

    void Initialize(unsigned int numThreads = std::thread::hardware_concurrency()) {
        if (m_Running.load(std::memory_order_acquire)) {
            return;
        }
        m_Running = true;
        for (unsigned int i = 0; i < numThreads; ++i) {
            m_Queues.push_back(std::make_unique<ThreadJobQueue<Job<std::function<void()>>>>());
            m_Workers.emplace_back([this, i] { WorkerLoop(i); });
        }
    }

    void Shutdown() {
        if (!m_Running.load(std::memory_order_acquire) && m_Workers.empty()) {
            return;
        }
        m_Running = false;
        for (auto& worker : m_Workers) {
            if (worker.joinable()) worker.join();
        }
        m_Workers.clear();
        m_Queues.clear();
    }

    bool IsRunning() const {
        return m_Running.load(std::memory_order_acquire);
    }

    template<typename F>
    void Submit(F&& task) {
        if (!m_Queues.empty()) {
            const auto idx = m_NextQueueIndex.fetch_add(1, std::memory_order_relaxed) % m_Queues.size();
            m_Queues[idx]->PushLocal(Job<std::function<void()>>(std::function<void()>(std::forward<F>(task))));
        } else {
            throw std::runtime_error("JobSystem not initialized");
        }
    }

    template<typename F>
    auto SubmitAsync(F&& task) -> std::future<decltype(task())> {
        using R = decltype(task());
        if (m_Queues.empty()) {
            throw std::runtime_error("JobSystem not initialized");
        }

        AsyncJob<R> aj(std::function<R()>(std::forward<F>(task)));
        auto fut = aj.Future();

        const auto idx = m_NextQueueIndex.fetch_add(1, std::memory_order_relaxed) % m_Queues.size();
        m_Queues[idx]->PushLocal(Job<std::function<void()>>([job = std::move(aj)]() mutable { job.Execute(); }));
        return fut;
    }

private:
    JobSystem() = default;
    ~JobSystem() { Shutdown(); }

    void WorkerLoop(unsigned int threadIndex) {
        const size_t queueIndex = threadIndex % m_Queues.size();
        size_t stealAttempts = 0;
        const size_t maxStealAttempts = m_Queues.size() * 2; // Try all queues twice before yielding

        while (m_Running) {
            Job<std::function<void()>> job;
            bool found = false;

            if (!m_Queues.empty()) {
                // Try pop from own queue first (most common case, no contention)
                if (m_Queues[queueIndex]->PopLocal(job)) {
                    found = true;
                    stealAttempts = 0; // Reset on success
                } else {
                    // Work-stealing: try other queues with round-robin
                    size_t startSteal = (queueIndex + 1) % m_Queues.size();
                    for (size_t i = 0; i < m_Queues.size() - 1; ++i) {
                        size_t stealIndex = (startSteal + i) % m_Queues.size();
                        if (m_Queues[stealIndex]->Steal(job)) {
                            found = true;
                            stealAttempts = 0;
                            break;
                        }
                    }
                }
            }

            if (found && job) {
                job.Execute();
            } else {
                // Exponential backoff to reduce CPU spinning
                stealAttempts++;
                if (stealAttempts < 4) {
                    std::this_thread::yield();
                } else if (stealAttempts < 16) {
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                    stealAttempts = 0; // Reset after long wait
                }
            }
        }
    }

    std::vector<std::thread> m_Workers;
    std::vector<std::unique_ptr<ThreadJobQueue<Job<std::function<void()>>>>> m_Queues;
    std::atomic<size_t> m_NextQueueIndex{0};
    std::atomic<bool> m_Running{false};
};

} // namespace Solstice::Core
