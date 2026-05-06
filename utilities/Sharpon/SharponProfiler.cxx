#include "SharponProfiler.hxx"
#include "LibUI/Tools/PropertyGrid.hxx"
#include "LibUI/Widgets/Widgets.hxx"

#include <SolsticeAPI/V1/Profiler.h>

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
    LibUI::Widgets::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    if (LibUI::Widgets::BeginWindow("Performance (SolsticeV1 Profiler)", pOpen)) {
        if (!g_SetEnabled || !g_GetLastFrame) {
            LibUI::Widgets::Text("Profiler API not exported by the loaded engine DLL.");
            LibUI::Widgets::Text("Future: script breakpoints / stepping when the engine exposes them.");
        } else {
            if (LibUI::Tools::BeginPropertyGrid("sharpon_profiler_grid", 240.0f)) {
                if (LibUI::Tools::PropertyBool("Profiler enabled", &g_ProfilerEnabledUi,
                        "Toggles SolsticeV1 profiler data collection.")) {
                    g_SetEnabled(g_ProfilerEnabledUi ? SolsticeV1_True : SolsticeV1_False);
                }
                LibUI::Tools::PropertyBool("Auto Begin/End frame", &g_AutoFrameUi,
                    "Run begin/end frame around each Sharpon UI frame.");
                LibUI::Tools::EndPropertyGrid();
            }
            if (!g_AutoFrameUi) {
                if (LibUI::Widgets::Button("BeginFrame")) {
                    if (g_BeginFrame) {
                        g_BeginFrame();
                    }
                }
                LibUI::Widgets::SameLine();
                if (LibUI::Widgets::Button("EndFrame")) {
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
                LibUI::Widgets::Text("SolsticeV1_ProfilerGetLastFrame: not available or no data yet.");
            }
        }
    }
    LibUI::Widgets::EndWindow();
}

void SharponProfiler_TickAutoFrame() {
    if (!g_AutoFrameUi || !g_BeginFrame || !g_EndFrame) {
        return;
    }
    g_BeginFrame();
    g_EndFrame();
}
