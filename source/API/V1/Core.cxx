#include "SolsticeAPI/V1/Core.h"
#include "SolsticeAPI/V1/Physics.h"
#include "Solstice.hxx"

extern "C" {

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreInitialize(void) {
    return Solstice::Initialize() ? SolsticeV1_True : SolsticeV1_False;
}

SOLSTICE_V1_API SolsticeV1_Bool SolsticeV1_CoreReinitialize(void) {
    return Solstice::Reinitialize() ? SolsticeV1_True : SolsticeV1_False;
}

SOLSTICE_V1_API void SolsticeV1_CoreShutdown(void) {
    SolsticeV1_PhysicsStop();
    Solstice::Shutdown();
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
