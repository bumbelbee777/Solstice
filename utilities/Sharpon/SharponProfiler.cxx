#include "SharponProfiler.hxx"

#include <SolsticeAPI/V1/Profiler.h>

#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

typedef SolsticeV1_ResultCode (*ProfSetEnabledFn)(SolsticeV1_Bool);
typedef SolsticeV1_ResultCode (*ProfBeginFrameFn)(void);
typedef SolsticeV1_ResultCode (*ProfEndFrameFn)(void);
typedef SolsticeV1_ResultCode (*ProfGetLastFrameFn)(float*, float*);

ProfSetEnabledFn g_SetEnabled = nullptr;
ProfBeginFrameFn g_BeginFrame = nullptr;
ProfEndFrameFn g_EndFrame = nullptr;
ProfGetLastFrameFn g_GetLastFrame = nullptr;

bool g_ProfilerEnabledUi = false;
bool g_AutoFrameUi = false;

} // namespace

void SharponProfiler_BindFromEngineModule(void* moduleHandle) {
    g_SetEnabled = nullptr;
    g_BeginFrame = nullptr;
    g_EndFrame = nullptr;
    g_GetLastFrame = nullptr;
    if (!moduleHandle) {
        return;
    }
#ifdef _WIN32
    HMODULE m = static_cast<HMODULE>(moduleHandle);
    g_SetEnabled = reinterpret_cast<ProfSetEnabledFn>(GetProcAddress(m, "SolsticeV1_ProfilerSetEnabled"));
    g_BeginFrame = reinterpret_cast<ProfBeginFrameFn>(GetProcAddress(m, "SolsticeV1_ProfilerBeginFrame"));
    g_EndFrame = reinterpret_cast<ProfEndFrameFn>(GetProcAddress(m, "SolsticeV1_ProfilerEndFrame"));
    g_GetLastFrame = reinterpret_cast<ProfGetLastFrameFn>(GetProcAddress(m, "SolsticeV1_ProfilerGetLastFrame"));
#else
    void* m = moduleHandle;
    g_SetEnabled = reinterpret_cast<ProfSetEnabledFn>(dlsym(m, "SolsticeV1_ProfilerSetEnabled"));
    g_BeginFrame = reinterpret_cast<ProfBeginFrameFn>(dlsym(m, "SolsticeV1_ProfilerBeginFrame"));
    g_EndFrame = reinterpret_cast<ProfEndFrameFn>(dlsym(m, "SolsticeV1_ProfilerEndFrame"));
    g_GetLastFrame = reinterpret_cast<ProfGetLastFrameFn>(dlsym(m, "SolsticeV1_ProfilerGetLastFrame"));
#endif
}

void SharponProfiler_DrawPanel(bool* pOpen) {
    if (pOpen && !*pOpen) {
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance (SolsticeV1 Profiler)", pOpen)) {
        if (!g_SetEnabled || !g_GetLastFrame) {
            ImGui::TextUnformatted("Profiler API not exported by the loaded engine DLL.");
            ImGui::TextUnformatted("Future: script breakpoints / stepping when the engine exposes them.");
        } else {
            if (ImGui::Checkbox("Profiler enabled", &g_ProfilerEnabledUi)) {
                g_SetEnabled(g_ProfilerEnabledUi ? SolsticeV1_True : SolsticeV1_False);
            }
            ImGui::Checkbox("Auto Begin/End frame (each Sharpon UI frame)", &g_AutoFrameUi);
            if (!g_AutoFrameUi) {
                if (ImGui::Button("BeginFrame")) {
                    if (g_BeginFrame) {
                        g_BeginFrame();
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("EndFrame")) {
                    if (g_EndFrame) {
                        g_EndFrame();
                    }
                }
            }
            float ms = 0.f;
            float fps = 0.f;
            if (g_GetLastFrame(&ms, &fps) == SolsticeV1_ResultSuccess) {
                ImGui::Text("Last frame: %.3f ms   FPS: %.1f", ms, fps);
            } else {
                ImGui::TextUnformatted("SolsticeV1_ProfilerGetLastFrame: not available or no data yet.");
            }
        }
    }
    ImGui::End();
}

void SharponProfiler_TickAutoFrame() {
    if (!g_AutoFrameUi || !g_BeginFrame || !g_EndFrame) {
        return;
    }
    g_BeginFrame();
    g_EndFrame();
}
