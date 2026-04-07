/**
 * Smoke test for Solstice V1 C API: link-time resolution and basic call sequence.
 * On Windows, also verifies GetProcAddress can resolve exports from the SolsticeEngine DLL.
 */

#include "SolsticeAPI/V1/SolsticeAPI.h"
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#endif

static int TestLinkedExports() {
    if (SolsticeV1_CoreInitialize() != SolsticeV1_True) {
        return 1;
    }
    const char* Version = nullptr;
    SolsticeV1_CoreGetVersionString(&Version);
    if (!Version || Version[0] == '\0') {
        return 2;
    }
    const char* Commit = nullptr;
    SolsticeV1_CoreGetBuildCommit(&Commit);
    if (!Commit || Commit[0] == '\0') {
        return 8;
    }
    SolsticeV1_PhysicsWorldHandle World = nullptr;
    if (SolsticeV1_PhysicsStart(&World) != SolsticeV1_ResultSuccess) {
        return 3;
    }
    if (!World) {
        return 4;
    }
    SolsticeV1_PhysicsUpdate(0.0f);
    if (SolsticeV1_AudioSetMasterVolume(0.5f) != SolsticeV1_ResultSuccess) {
        return 5;
    }
    if (SolsticeV1_AudioUpdate(0.0f) != SolsticeV1_ResultSuccess) {
        return 6;
    }
    if (SolsticeV1_AudioStopMusic() != SolsticeV1_ResultSuccess) {
        return 7;
    }
    SolsticeV1_PhysicsStop();
    SolsticeV1_CoreShutdown();
    return 0;
}

#if defined(_WIN32)
static int TestDllExportResolution() {
    HMODULE Module = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&SolsticeV1_CoreInitialize),
            &Module)) {
        return 10;
    }
    auto Resolved = reinterpret_cast<SolsticeV1_Bool (*)(void)>(
        GetProcAddress(Module, "SolsticeV1_CoreInitialize"));
    if (!Resolved) {
        return 11;
    }
    if (Resolved != &SolsticeV1_CoreInitialize) {
        // Same process: should resolve to the same function.
    }
    return 0;
}
#endif

int main() {
    int Code = TestLinkedExports();
    if (Code != 0) {
        return Code;
    }
#if defined(_WIN32)
    Code = TestDllExportResolution();
#endif
    return Code;
}
