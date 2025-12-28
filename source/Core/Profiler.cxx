#include <Core/Profiler.hxx>
#include <Core/Debug.hxx>
#include <imgui.h>
#include <bgfx/bgfx.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <set>
#include <cstdint>
#include <thread>

namespace Solstice::Core {

// Thread-local scope tracking
thread_local Profiler::ThreadScopeData* g_threadCurrentScope = nullptr;

Profiler& Profiler::Instance() {
    static Profiler instance;
    return instance;
}

void Profiler::BeginFrame() {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // End previous frame if still active
    if (m_rootScope) {
        EndFrame();
    }

    m_frameStartTime = std::chrono::high_resolution_clock::now();
    m_rootScope.reset();
    m_currentScope = nullptr;
    m_rootGPUScope.reset();
    m_currentGPUScope = nullptr;

    // Clear counters for new frame (they accumulate)
    // Actually, keep counters persistent across frames
}

void Profiler::EndFrame() {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto frameEndTime = std::chrono::high_resolution_clock::now();
    float frameTime = std::chrono::duration<float, std::milli>(frameEndTime - m_frameStartTime).count();
    float fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;

    // Flatten scope tree
    std::map<std::string, float> scopeTimes;
    if (m_rootScope) {
        FlattenScopeTree(m_rootScope.get(), scopeTimes);
        ClearScopeTree(m_rootScope.get());
        m_rootScope.reset();
        m_currentScope = nullptr;
    }

    // Flatten thread scope times
    FlattenThreadScopeTimes(m_threadScopeTimes);

    // Flatten GPU scope tree
    std::map<std::string, float> gpuScopeTimes;
    float gpuFrameTime = 0.0f;
    if (m_rootGPUScope) {
        FlattenGPUScopeTree(m_rootGPUScope.get(), gpuScopeTimes);
        // Calculate total GPU time
        for (const auto& pair : gpuScopeTimes) {
            gpuFrameTime += pair.second;
        }
        ClearGPUScopeTree(m_rootGPUScope.get());
        m_rootGPUScope.reset();
        m_currentGPUScope = nullptr;
    }

    // Store frame data
    FrameData frameData;
    frameData.StartTime = m_frameStartTime;
    frameData.FrameTime = frameTime;
    frameData.FPS = fps;
    frameData.GPUTime = gpuFrameTime;
    frameData.ScopeTimes = scopeTimes;
    frameData.GPUScopeTimes = gpuScopeTimes;
    frameData.Counters = m_counters;
    frameData.MemoryUsage = m_currentMemory;
    frameData.PeakMemory = m_peakMemory;
    frameData.GPUMemory = m_gpuMemory;
    frameData.ThreadScopeTimes = m_threadScopeTimes;

    m_frameHistory.push_back(frameData);

    // Clear thread scope times for next frame
    m_threadScopeTimes.clear();
    if (m_frameHistory.size() > MAX_FRAME_HISTORY) {
        m_frameHistory.erase(m_frameHistory.begin());
    }
}

void Profiler::BeginScope(const char* name) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    auto newScope = std::make_unique<ScopeData>();
    newScope->Name = name;
    newScope->StartTime = std::chrono::high_resolution_clock::now();
    newScope->Parent = m_currentScope;
    newScope->ThreadId = std::this_thread::get_id();

    if (m_currentScope) {
        // Add as child of current scope
        ScopeData* newScopePtr = newScope.get();
        m_currentScope->Children.push_back(std::move(newScope));
        m_currentScope = newScopePtr;
    } else {
        // This is the root scope
        m_rootScope = std::move(newScope);
        m_currentScope = m_rootScope.get();
    }
}

void Profiler::EndScope(const char* name) {
    if (!m_enabled || !m_currentScope) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the scope to end (walk up the tree if needed)
    ScopeData* scopeToEnd = m_currentScope;

    // If name doesn't match, try to find it in the tree
    if (scopeToEnd->Name != name) {
        // Walk up to find matching scope
        ScopeData* search = m_currentScope;
        while (search && search->Name != name) {
            search = search->Parent;
        }
        if (search) {
            scopeToEnd = search;
        } else {
            // Scope not found, just end current
            scopeToEnd = m_currentScope;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(endTime - scopeToEnd->StartTime).count();

    scopeToEnd->Duration = duration;
    scopeToEnd->CallCount++;

    // Move current scope to parent
    m_currentScope = scopeToEnd->Parent;
}

void Profiler::IncrementCounter(const char* name, int64_t value) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_counters[name] += value;
}

void Profiler::SetCounter(const char* name, int64_t value) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_counters[name] = value;
}

int64_t Profiler::GetCounter(const char* name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_counters.find(name);
    return it != m_counters.end() ? it->second : 0;
}

void Profiler::TrackMemoryAlloc(size_t size) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentMemory += size;
    if (m_currentMemory > m_peakMemory) {
        m_peakMemory = m_currentMemory;
    }
}

void Profiler::TrackMemoryFree(size_t size) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentMemory >= size) {
        m_currentMemory -= size;
    } else {
        m_currentMemory = 0; // Safety check
    }
}

Profiler::FrameStats Profiler::GetLastFrameStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    FrameStats stats;
    if (!m_frameHistory.empty()) {
        const auto& lastFrame = m_frameHistory.back();
        stats.FrameTime = lastFrame.FrameTime;
        stats.FPS = lastFrame.FPS;
        stats.ScopeTimes = lastFrame.ScopeTimes;
        stats.GPUScopeTimes = lastFrame.GPUScopeTimes;
        stats.Counters = lastFrame.Counters;
        stats.MemoryUsage = lastFrame.MemoryUsage;
        stats.PeakMemory = lastFrame.PeakMemory;
        stats.GPUTime = lastFrame.GPUTime;
        stats.GPUMemory = lastFrame.GPUMemory;
        stats.ThreadScopeTimes = lastFrame.ThreadScopeTimes;
    }
    return stats;
}

Profiler::FrameStats Profiler::GetAverageFrameStats(int frameCount) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    FrameStats stats;
    if (m_frameHistory.empty()) return stats;

    int count = std::min(frameCount, static_cast<int>(m_frameHistory.size()));
    int startIdx = static_cast<int>(m_frameHistory.size()) - count;

    float totalFrameTime = 0.0f;
    float totalFPS = 0.0f;
    std::map<std::string, float> totalScopeTimes;
    std::map<std::string, int64_t> totalCounters;
    size_t totalMemory = 0;
    size_t maxPeakMemory = 0;

    for (int i = startIdx; i < static_cast<int>(m_frameHistory.size()); ++i) {
        const auto& frame = m_frameHistory[i];
        totalFrameTime += frame.FrameTime;
        totalFPS += frame.FPS;
        totalMemory += frame.MemoryUsage;
        if (frame.PeakMemory > maxPeakMemory) {
            maxPeakMemory = frame.PeakMemory;
        }

        for (const auto& pair : frame.ScopeTimes) {
            totalScopeTimes[pair.first] += pair.second;
        }

        for (const auto& pair : frame.Counters) {
            totalCounters[pair.first] += pair.second;
        }
    }

    float totalGPUTime = 0.0f;
    std::map<std::string, float> totalGPUScopeTimes;
    size_t totalGPUMemory = 0;

    for (int i = startIdx; i < static_cast<int>(m_frameHistory.size()); ++i) {
        const auto& frame = m_frameHistory[i];
        totalGPUTime += frame.GPUTime;
        totalGPUMemory += frame.GPUMemory;

        for (const auto& pair : frame.GPUScopeTimes) {
            totalGPUScopeTimes[pair.first] += pair.second;
        }
    }

    float invCount = 1.0f / static_cast<float>(count);
    stats.FrameTime = totalFrameTime * invCount;
    stats.FPS = totalFPS * invCount;
    stats.GPUTime = totalGPUTime * invCount;
    stats.MemoryUsage = totalMemory / count;
    stats.PeakMemory = maxPeakMemory;
    stats.GPUMemory = totalGPUMemory / count;

    for (auto& pair : totalScopeTimes) {
        stats.ScopeTimes[pair.first] = pair.second * invCount;
    }

    for (auto& pair : totalGPUScopeTimes) {
        stats.GPUScopeTimes[pair.first] = pair.second * invCount;
    }

    for (const auto& pair : totalCounters) {
        stats.Counters[pair.first] = pair.second / count;
    }

    return stats;
}

void Profiler::RenderOverlay(bool* pOpen) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Profiler", pOpen)) {
        ImGui::End();
        return;
    }

    if (m_frameHistory.empty()) {
        ImGui::Text("No frame data available");
        ImGui::End();
        return;
    }

    const auto& lastFrame = m_frameHistory.back();
    FrameStats avgStats = GetAverageFrameStats(60);

    // Frame stats
    if (ImGui::CollapsingHeader("Frame Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Frame Time: %.2f ms (Avg: %.2f ms)", lastFrame.FrameTime, avgStats.FrameTime);
        ImGui::Text("FPS: %.1f (Avg: %.1f)", lastFrame.FPS, avgStats.FPS);
        if (lastFrame.GPUTime > 0.0f) {
            ImGui::Text("GPU Time: %.2f ms (Avg: %.2f ms)", lastFrame.GPUTime, avgStats.GPUTime);
            ImGui::Text("CPU/GPU Ratio: %.2f", lastFrame.FrameTime / (lastFrame.GPUTime > 0.001f ? lastFrame.GPUTime : 0.001f));
        }

        float minFrameTime = lastFrame.FrameTime;
        float maxFrameTime = lastFrame.FrameTime;
        for (const auto& frame : m_frameHistory) {
            minFrameTime = std::min(minFrameTime, frame.FrameTime);
            maxFrameTime = std::max(maxFrameTime, frame.FrameTime);
        }
        ImGui::Text("Min: %.2f ms, Max: %.2f ms", minFrameTime, maxFrameTime);
    }

    // Scope times
    if (ImGui::CollapsingHeader("Scope Times", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Scopes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            // Sort by time
            std::vector<std::pair<std::string, float>> sortedScopes;
            for (const auto& pair : avgStats.ScopeTimes) {
                sortedScopes.push_back({pair.first, pair.second});
            }
            std::sort(sortedScopes.begin(), sortedScopes.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            for (const auto& pair : sortedScopes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", pair.first.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", pair.second);
            }

            ImGui::EndTable();
        }
    }

    // Counters
    if (ImGui::CollapsingHeader("Counters", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTable("Counters", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            for (const auto& pair : avgStats.Counters) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", pair.first.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(pair.second));
            }

            ImGui::EndTable();
        }
    }

    // GPU Scope times
    if (ImGui::CollapsingHeader("GPU Scope Times")) {
        if (ImGui::BeginTable("GPUScopes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableHeadersRow();

            std::vector<std::pair<std::string, float>> sortedScopes;
            for (const auto& pair : avgStats.GPUScopeTimes) {
                sortedScopes.push_back({pair.first, pair.second});
            }
            std::sort(sortedScopes.begin(), sortedScopes.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });

            for (const auto& pair : sortedScopes) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", pair.first.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", pair.second);
            }

            ImGui::EndTable();
        }
    }

    // Memory
    if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Current: %.2f MB", m_currentMemory / (1024.0f * 1024.0f));
        ImGui::Text("Peak: %.2f MB", m_peakMemory / (1024.0f * 1024.0f));
        if (m_gpuMemory > 0) {
            ImGui::Text("GPU: %.2f MB", m_gpuMemory / (1024.0f * 1024.0f));
        }
    }

    // Call graph (simplified - shows scope hierarchy from last frame)
    if (ImGui::CollapsingHeader("Call Graph")) {
        if (!m_frameHistory.empty()) {
            // Show scope relationships from aggregated data
            ImGui::Text("Scope Hierarchy (from aggregated data):");
            ImGui::Indent();
            for (const auto& [scopeName, duration] : avgStats.ScopeTimes) {
                if (ImGui::TreeNode(scopeName.c_str())) {
                    ImGui::Text("Average Time: %.3f ms", duration);
                    ImGui::Text("Calls: %d", static_cast<int>(duration / (duration > 0.001f ? duration : 0.001f))); // Simplified
                    ImGui::TreePop();
                }
            }
            ImGui::Unindent();
        } else {
            ImGui::Text("No call graph data available");
        }
    }

    // Thread breakdown
    if (ImGui::CollapsingHeader("Thread Breakdown")) {
        if (!m_frameHistory.empty()) {
            const auto& lastFrame = m_frameHistory.back();
            for (const auto& [threadId, scopeTimes] : lastFrame.ThreadScopeTimes) {
                std::stringstream ss;
                ss << "Thread " << threadId;
                if (ImGui::TreeNode(ss.str().c_str())) {
                    if (ImGui::BeginTable("ThreadScopes", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableSetupColumn("Time (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
                        ImGui::TableHeadersRow();

                        std::vector<std::pair<std::string, float>> sortedScopes;
                        for (const auto& pair : scopeTimes) {
                            sortedScopes.push_back({pair.first, pair.second});
                        }
                        std::sort(sortedScopes.begin(), sortedScopes.end(),
                            [](const auto& a, const auto& b) { return a.second > b.second; });

                        for (const auto& pair : sortedScopes) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", pair.first.c_str());
                            ImGui::TableNextColumn();
                            ImGui::Text("%.3f", pair.second);
                        }

                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    // Performance warnings
    std::vector<Warning> warnings = GetWarnings();
    if (!warnings.empty() && ImGui::CollapsingHeader("Warnings", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& warning : warnings) {
            ImVec4 color = ImVec4(1.0f, 1.0f - warning.Severity, 0.0f, 1.0f); // Yellow to red
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::BulletText("%s", warning.Message.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}

std::vector<Profiler::Warning> Profiler::GetWarnings() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Warning> warnings;

    if (m_frameHistory.empty()) return warnings;

    const auto& lastFrame = m_frameHistory.back();
    FrameStats avgStats = GetAverageFrameStats(60);

    // Frame time spike warning
    if (lastFrame.FrameTime > m_warningThresholds.MaxFrameTime) {
        float severity = std::min(1.0f, (lastFrame.FrameTime - m_warningThresholds.MaxFrameTime) / m_warningThresholds.MaxFrameTime);
        Warning w;
        w.Message = "Frame time spike: " + std::to_string(lastFrame.FrameTime) + " ms (threshold: " +
                   std::to_string(m_warningThresholds.MaxFrameTime) + " ms)";
        w.Severity = severity;
        warnings.push_back(w);
    }

    // Memory leak detection
    if (m_frameHistory.size() >= 60) {
        size_t recentMemory = 0;
        size_t oldMemory = 0;
        int count = std::min(30, static_cast<int>(m_frameHistory.size()));
        for (int i = static_cast<int>(m_frameHistory.size()) - count; i < static_cast<int>(m_frameHistory.size()); ++i) {
            recentMemory += m_frameHistory[i].MemoryUsage;
        }
        for (int i = 0; i < count && i < static_cast<int>(m_frameHistory.size()); ++i) {
            oldMemory += m_frameHistory[i].MemoryUsage;
        }

        float recentAvg = recentMemory / static_cast<float>(count);
        float oldAvg = oldMemory / static_cast<float>(count);
        float growthMB = (recentAvg - oldAvg) / (1024.0f * 1024.0f);

        if (growthMB > m_warningThresholds.MemoryLeakThreshold) {
            Warning w;
            w.Message = "Possible memory leak: " + std::to_string(growthMB) + " MB growth per frame";
            w.Severity = std::min(1.0f, growthMB / (m_warningThresholds.MemoryLeakThreshold * 2.0f));
            warnings.push_back(w);
        }
    }

    // Scope time warnings
    for (const auto& [scopeName, duration] : avgStats.ScopeTimes) {
        if (duration > m_warningThresholds.ScopeTimeWarning) {
            float severity = std::min(1.0f, duration / (m_warningThresholds.ScopeTimeWarning * 2.0f));
            Warning w;
            w.Message = "Slow scope: " + scopeName + " (" + std::to_string(duration) + " ms)";
            w.Severity = severity;
            warnings.push_back(w);
        }
    }

    return warnings;
}

void Profiler::ExportReport(const char* filename, int frameCount) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(filename);
    if (!file.is_open()) {
        SIMPLE_LOG("Profiler: Failed to open file for export: " + std::string(filename));
        return;
    }

    file << "Frame,FrameTime(ms),FPS";

    // Get all unique scope names
    std::set<std::string> scopeNames;
    std::set<std::string> counterNames;
    for (const auto& frame : m_frameHistory) {
        for (const auto& pair : frame.ScopeTimes) {
            scopeNames.insert(pair.first);
        }
        for (const auto& pair : frame.Counters) {
            counterNames.insert(pair.first);
        }
    }

    for (const auto& name : scopeNames) {
        file << "," << name << "(ms)";
    }
    for (const auto& name : counterNames) {
        file << "," << name;
    }
    file << ",Memory(MB),PeakMemory(MB)\n";

    int count = std::min(frameCount, static_cast<int>(m_frameHistory.size()));
    int startIdx = static_cast<int>(m_frameHistory.size()) - count;

    for (int i = startIdx; i < static_cast<int>(m_frameHistory.size()); ++i) {
        const auto& frame = m_frameHistory[i];
        file << (i - startIdx) << "," << frame.FrameTime << "," << frame.FPS;

        for (const auto& name : scopeNames) {
            auto it = frame.ScopeTimes.find(name);
            file << "," << (it != frame.ScopeTimes.end() ? it->second : 0.0f);
        }

        for (const auto& name : counterNames) {
            auto it = frame.Counters.find(name);
            file << "," << (it != frame.Counters.end() ? it->second : 0);
        }

        file << "," << (frame.MemoryUsage / (1024.0f * 1024.0f))
             << "," << (frame.PeakMemory / (1024.0f * 1024.0f)) << "\n";
    }

    file.close();
    SIMPLE_LOG("Profiler: Exported report to " + std::string(filename));
}

void Profiler::ExportFlameGraph(const char* filename, int frameCount) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream file(filename);
    if (!file.is_open()) {
        SIMPLE_LOG("Profiler: Failed to open file for flame graph export: " + std::string(filename));
        return;
    }

    // Export in speedscope JSON format
    file << "{\n";
    file << "  \"version\": \"0.0.0\",\n";
    file << "  \"$schema\": \"https://www.speedscope.app/file-format-schema.json\",\n";
    file << "  \"shared\": {\n";
    file << "    \"frames\": [\n";

    // Collect all unique scope names
    std::set<std::string> allScopes;
    for (const auto& frame : m_frameHistory) {
        for (const auto& pair : frame.ScopeTimes) {
            allScopes.insert(pair.first);
        }
    }

    bool first = true;
    for (const auto& scopeName : allScopes) {
        if (!first) file << ",\n";
        file << "      { \"name\": \"" << scopeName << "\" }";
        first = false;
    }

    file << "\n    ]\n";
    file << "  },\n";
    file << "  \"profiles\": [\n";
    file << "    {\n";
    file << "      \"type\": \"evented\",\n";
    file << "      \"name\": \"Solstice Profiler\",\n";
    file << "      \"unit\": \"milliseconds\",\n";
    file << "      \"startValue\": 0,\n";
    file << "      \"endValue\": " << (frameCount > 0 ? frameCount : m_frameHistory.size()) << ",\n";
    file << "      \"events\": [\n";

    // Export events (simplified - would need full scope tree for accurate flame graph)
    int count = std::min(frameCount, static_cast<int>(m_frameHistory.size()));
    int startIdx = static_cast<int>(m_frameHistory.size()) - count;

    int frameIdx = 0;
    for (int i = startIdx; i < static_cast<int>(m_frameHistory.size()); ++i) {
        const auto& frame = m_frameHistory[i];
        float currentTime = 0.0f;

        for (const auto& [scopeName, duration] : frame.ScopeTimes) {
            // Find frame index for this scope
            int frameIndex = 0;
            for (const auto& scope : allScopes) {
                if (scope == scopeName) break;
                frameIndex++;
            }

            file << "        { \"type\": \"O\", \"frame\": " << frameIndex
                 << ", \"at\": " << currentTime << " },\n";
            currentTime += duration;
            file << "        { \"type\": \"C\", \"frame\": " << frameIndex
                 << ", \"at\": " << currentTime << " },\n";
        }
        frameIdx++;
    }

    file << "      ]\n";
    file << "    }\n";
    file << "  ]\n";
    file << "}\n";

    file.close();
    SIMPLE_LOG("Profiler: Exported flame graph to " + std::string(filename));
}

void Profiler::FlattenScopeTree(ScopeData* scope, std::map<std::string, float>& scopeTimes) {
    if (!scope) return;

    // Aggregate times for scopes with same name
    scopeTimes[scope->Name] += scope->Duration;

    // Track call frequencies
    for (auto& child : scope->Children) {
        scope->CallFrequencies[child->Name]++;
        FlattenScopeTree(child.get(), scopeTimes);
    }
}

void Profiler::ClearScopeTree(ScopeData* scope) {
    if (!scope) return;

    for (auto& child : scope->Children) {
        ClearScopeTree(child.get());
    }
    scope->Children.clear();
}

void Profiler::BeginThreadScope(const char* name) {
    if (!m_enabled) return;

    std::thread::id threadId = std::this_thread::get_id();

    ThreadScopeData newScope;
    newScope.Name = name;
    newScope.StartTime = std::chrono::high_resolution_clock::now();
    newScope.ThreadId = threadId;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_threadScopes.find(threadId) == m_threadScopes.end()) {
            m_threadScopes[threadId] = std::vector<ThreadScopeData>();
        }

        // Find current scope index
        size_t parentIndex = SIZE_MAX;
        if (g_threadCurrentScope) {
            // Find parent in current thread's scopes
            for (size_t i = 0; i < m_threadScopes[threadId].size(); ++i) {
                if (&m_threadScopes[threadId][i] == g_threadCurrentScope) {
                    parentIndex = i;
                    break;
                }
            }
        }

        newScope.ParentIndex = parentIndex;
        m_threadScopes[threadId].push_back(newScope);
        g_threadCurrentScope = &m_threadScopes[threadId].back();
    }
}

void Profiler::EndThreadScope(const char* name) {
    if (!m_enabled || !g_threadCurrentScope) return;

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::milli>(endTime - g_threadCurrentScope->StartTime).count();

    std::thread::id threadId = std::this_thread::get_id();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threadScopeTimes[threadId][g_threadCurrentScope->Name] += duration;

        // Move to parent
        if (g_threadCurrentScope->ParentIndex != SIZE_MAX &&
            g_threadCurrentScope->ParentIndex < m_threadScopes[threadId].size()) {
            g_threadCurrentScope = &m_threadScopes[threadId][g_threadCurrentScope->ParentIndex];
        } else {
            g_threadCurrentScope = nullptr;
        }
    }
}

void Profiler::FlattenThreadScopeTimes(std::unordered_map<std::thread::id, std::map<std::string, float>>& threadTimes) {
    threadTimes = m_threadScopeTimes;
}

void Profiler::BeginGPUScope(const char* name) {
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if BGFX is available
    bgfx::RendererType::Enum rendererType = bgfx::getRendererType();
    if (rendererType == bgfx::RendererType::Noop) {
        return;
    }

    auto newScope = std::make_unique<GPUScopeData>();
    newScope->Name = name;
    newScope->Parent = m_currentGPUScope;
    newScope->StartTime = std::chrono::high_resolution_clock::now(); // Fallback to CPU timing for now

    // Set marker for debugging
    bgfx::setMarker(name);

    if (m_currentGPUScope) {
        GPUScopeData* newScopePtr = newScope.get();
        m_currentGPUScope->Children.push_back(std::move(newScope));
        m_currentGPUScope = newScopePtr;
    } else {
        m_rootGPUScope = std::move(newScope);
        m_currentGPUScope = m_rootGPUScope.get();
    }
}

void Profiler::EndGPUScope(const char* name) {
    if (!m_enabled || !m_currentGPUScope) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the scope to end
    GPUScopeData* scopeToEnd = m_currentGPUScope;

    if (scopeToEnd->Name != name) {
        GPUScopeData* search = m_currentGPUScope;
        while (search && search->Name != name) {
            search = search->Parent;
        }
        if (search) {
            scopeToEnd = search;
        } else {
            scopeToEnd = m_currentGPUScope;
        }
    }

    // Calculate duration (using CPU timing as fallback until proper GPU timestamps are implemented)
    auto endTime = std::chrono::high_resolution_clock::now();
    scopeToEnd->Duration = std::chrono::duration<float, std::milli>(endTime - scopeToEnd->StartTime).count();
    scopeToEnd->CallCount++;

    // Move current scope to parent
    m_currentGPUScope = scopeToEnd->Parent;
}

void Profiler::FlattenGPUScopeTree(GPUScopeData* scope, std::map<std::string, float>& scopeTimes) {
    if (!scope) return;

    // Try to read GPU timing (simplified - actual implementation would use proper timestamp queries)
    // For now, we'll use a placeholder that will be filled when proper GPU timing is available
    scopeTimes[scope->Name] += scope->Duration;

    for (auto& child : scope->Children) {
        FlattenGPUScopeTree(child.get(), scopeTimes);
    }
}

void Profiler::ClearGPUScopeTree(GPUScopeData* scope) {
    if (!scope) return;

    for (auto& child : scope->Children) {
        ClearGPUScopeTree(child.get());
    }
    scope->Children.clear();
}

} // namespace Solstice::Core

