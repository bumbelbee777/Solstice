#pragma once

#include "../Solstice.hxx"
#include <thread>
#include <atomic>
#include <future>
#include <functional>
#include <stdexcept>
#include <memory>
#include <queue>
#include <deque>
#include <vector>

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
template<class F>
struct AsyncJob {
    F Task;
    std::shared_ptr<std::promise<void>> Promise;

    explicit AsyncJob(F t) : Task(std::move(t)), Promise(std::make_shared<std::promise<void>>()) {}

    std::future<void> Future() { return Promise->get_future(); }

    void Execute() {
        try {
            Task();
            Promise->set_value();
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
        m_Running = true;
        for (unsigned int i = 0; i < numThreads; ++i) {
            m_Queues.push_back(std::make_unique<ThreadJobQueue<Job<std::function<void()>>>>());
            m_Workers.emplace_back([this, i] { WorkerLoop(i); });
        }
    }

    void Shutdown() {
        m_Running = false;
        for (auto& worker : m_Workers) {
            if (worker.joinable()) worker.join();
        }
        m_Workers.clear();
        m_Queues.clear();
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
    std::future<void> SubmitAsync(F&& task) {
        if (m_Queues.empty()) {
            throw std::runtime_error("JobSystem not initialized");
        }

        AsyncJob<std::function<void()>> aj(std::function<void()>(std::forward<F>(task)));
        auto fut = aj.Future();

        const auto idx = m_NextQueueIndex.fetch_add(1, std::memory_order_relaxed) % m_Queues.size();
        m_Queues[idx]->PushLocal(Job<std::function<void()>>([job = std::move(aj)]() mutable { job.Execute(); }));
        return fut;
    }

private:
    JobSystem() = default;
    ~JobSystem() { Shutdown(); }

    void WorkerLoop(unsigned int threadIndex) {
        (void)threadIndex;
        while (m_Running) {
            Job<std::function<void()>> job;

            // Try pop from own queue first
            bool found = false;
            if (!m_Queues.empty()) {
                if (m_Queues[threadIndex % m_Queues.size()]->PopLocal(job)) {
                    found = true;
                } else {
                    // Steal from other queues
                    for (size_t i = 0; i < m_Queues.size(); ++i) {
                        if (i == (threadIndex % m_Queues.size())) continue;
                        if (m_Queues[i]->Steal(job)) { found = true; break; }
                    }
                }
            }

            if (found && job) {
                job.Execute();
            } else {
                std::this_thread::yield();
            }
        }
    }

    std::vector<std::thread> m_Workers;
    std::vector<std::unique_ptr<ThreadJobQueue<Job<std::function<void()>>>>> m_Queues;
    std::atomic<size_t> m_NextQueueIndex{0};
    std::atomic<bool> m_Running{false};
};

} // namespace Solstice::Core