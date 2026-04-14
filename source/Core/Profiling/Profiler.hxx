#pragma once

#include "Solstice.hxx"
#include <string>
#include <map>
#include <vector>
#include <chrono>
#include <mutex>
#include <memory>
#include <thread>
#include <unordered_map>

namespace Solstice::Core {

/**
 * Profiler - Singleton performance profiling system
 * Provides frame timing, scope timing, counters, and memory tracking
 */
class SOLSTICE_API Profiler {
public:
    static Profiler& Instance();

    // Frame management
    void BeginFrame();
    void EndFrame();

    // Scope timing
    void BeginScope(const char* name);
    void EndScope(const char* name);

    // Thread-level profiling
    void BeginThreadScope(const char* name);
    void EndThreadScope(const char* name);

    // GPU timing
    void BeginGPUScope(const char* name);
    void EndGPUScope(const char* name);

    // Performance counters
    void IncrementCounter(const char* name, int64_t value = 1);
    void SetCounter(const char* name, int64_t value);
    int64_t GetCounter(const char* name) const;

    // Memory tracking
    void TrackMemoryAlloc(size_t size);
    void TrackMemoryFree(size_t size);
    size_t GetMemoryUsage() const { return m_currentMemory; }
    size_t GetPeakMemory() const { return m_peakMemory; }

    // Statistics
    struct FrameStats {
        float FrameTime = 0.0f;
        float FPS = 0.0f;
        float GPUTime = 0.0f;
        std::map<std::string, float> ScopeTimes;
        std::map<std::string, float> GPUScopeTimes;
        std::map<std::string, int64_t> Counters;
        size_t MemoryUsage = 0;
        size_t PeakMemory = 0;
        size_t GPUMemory = 0;
        std::unordered_map<std::thread::id, std::map<std::string, float>> ThreadScopeTimes;
    };

    FrameStats GetLastFrameStats() const;
    FrameStats GetAverageFrameStats(int frameCount = 60) const;

    // Overlay rendering
    void RenderOverlay(bool* pOpen = nullptr);

    // Export
    void ExportReport(const char* filename, int frameCount = 60);
    void ExportFlameGraph(const char* filename, int frameCount = 60);

    // Configuration
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Performance warnings
    struct WarningThresholds {
        float MaxFrameTime = 33.33f; // 30 FPS threshold
        float MemoryLeakThreshold = 10.0f; // MB per frame growth
        float ScopeTimeWarning = 16.67f; // ms per scope warning
    };

    void SetWarningThresholds(const WarningThresholds& thresholds) { m_warningThresholds = thresholds; }
    const WarningThresholds& GetWarningThresholds() const { return m_warningThresholds; }

    struct Warning {
        std::string Message;
        float Severity = 0.0f; // 0.0 to 1.0
    };

    std::vector<Warning> GetWarnings() const;

private:
    Profiler() = default;
    ~Profiler() = default;
    Profiler(const Profiler&) = delete;
    Profiler& operator=(const Profiler&) = delete;

    struct ScopeData {
        std::string Name;
        std::chrono::high_resolution_clock::time_point StartTime;
        float Duration = 0.0f;
        int CallCount = 0;
        std::vector<std::unique_ptr<ScopeData>> Children;
        ScopeData* Parent = nullptr;
        std::thread::id ThreadId;

        // Call graph data
        std::map<std::string, int> CallFrequencies; // How many times this scope called each child
    };

public:
    // Thread-local scope tracking
    struct ThreadScopeData {
        std::string Name;
        std::chrono::high_resolution_clock::time_point StartTime;
        std::thread::id ThreadId;
        size_t ParentIndex = SIZE_MAX;
    };

private:

    struct FrameData {
        std::chrono::high_resolution_clock::time_point StartTime;
        float FrameTime = 0.0f;
        float FPS = 0.0f;
        float GPUTime = 0.0f;
        std::map<std::string, float> ScopeTimes;
        std::map<std::string, float> GPUScopeTimes;
        std::map<std::string, int64_t> Counters;
        size_t MemoryUsage = 0;
        size_t PeakMemory = 0;
        size_t GPUMemory = 0;
        std::unordered_map<std::thread::id, std::map<std::string, float>> ThreadScopeTimes;
    };

    // GPU scope tracking
    struct GPUScopeData {
        std::string Name;
        std::chrono::high_resolution_clock::time_point StartTime;
        float Duration = 0.0f;
        int CallCount = 0;
        GPUScopeData* Parent = nullptr;
        std::vector<std::unique_ptr<GPUScopeData>> Children;
    };

    mutable std::mutex m_mutex;
    bool m_enabled = false;
    WarningThresholds m_warningThresholds;

    // Current frame tracking
    std::chrono::high_resolution_clock::time_point m_frameStartTime;
    std::thread::id m_frameThreadId{};
    std::unique_ptr<ScopeData> m_rootScope;  // Root scope for the current frame
    ScopeData* m_currentScope = nullptr;     // Raw pointer to current scope (owned by tree)
    std::map<std::string, int64_t> m_counters;
    size_t m_currentMemory = 0;
    size_t m_peakMemory = 0;

    // Thread-local scope tracking
    std::unordered_map<std::thread::id, std::map<std::string, float>> m_threadScopeTimes;
    std::unordered_map<std::thread::id, std::vector<ThreadScopeData>> m_threadScopes;

    // GPU timing tracking
    std::unique_ptr<GPUScopeData> m_rootGPUScope;
    GPUScopeData* m_currentGPUScope = nullptr;
    size_t m_gpuMemory = 0;

    // Helper to flatten thread scope times
    void FlattenThreadScopeTimes(std::unordered_map<std::thread::id, std::map<std::string, float>>& threadTimes);
    void FlattenGPUScopeTree(GPUScopeData* scope, std::map<std::string, float>& scopeTimes);
    void ClearGPUScopeTree(GPUScopeData* scope);

    // Frame history
    std::vector<FrameData> m_frameHistory;
    static constexpr size_t MAX_FRAME_HISTORY = 300;

    // Helper methods
    void FlattenScopeTree(ScopeData* scope, std::map<std::string, float>& scopeTimes);
    void ClearScopeTree(ScopeData* scope);
};

} // namespace Solstice::Core

