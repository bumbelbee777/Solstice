#include "SolsticeAPI/V1/Core.h"
#include "SolsticeAPI/V1/Physics.h"
#include "SolsticeAPI/V1/Networking.h"
#include "Solstice.hxx"
#if defined(_WIN32)
#include <windows.h>
#endif

extern "C" {

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreInitialize(void) {
    return Solstice::Initialize() ? SolsticeV1_True : SolsticeV1_False;
}

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreReinitialize(void) {
    return Solstice::Reinitialize() ? SolsticeV1_True : SolsticeV1_False;
}

SOLSTICE_V1_API void SolsticeV1_CoreShutdown(void) {
    // Centralized shutdown already handles subsystem order and idempotency.
#if defined(_WIN32)
    __try {
        Solstice::Shutdown();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Best-effort containment for teardown AVs in external backends.
    }
#else
    Solstice::Shutdown();
#endif
}

SOLSTICE_V1_API void SolsticeV1_CoreGetVersionString(const char** OutVersion) {
    if (OutVersion) {
        *OutVersion = SOLSTICE_VERSION;
    }
}

SOLSTICE_V1_API void SolsticeV1_CoreGetBuildCommit(const char** OutCommit) {
    if (OutCommit) {
        *OutCommit = SOLSTICE_BUILD_GIT_COMMIT;
    }
}

} // extern "C"
