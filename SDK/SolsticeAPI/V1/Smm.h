#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SolsticeV1_SmmViewerTab {
    SolsticeV1_SmmViewerTabViewport3D = 0,
    SolsticeV1_SmmViewerTabMotionGraphics2D = 1,
} SolsticeV1_SmmViewerTab;

typedef struct SolsticeV1_SmmSessionState {
    uint64_t PlayheadTick;
    uint64_t TimelineDurationTicks;
    uint32_t TicksPerSecond;
    SolsticeV1_Bool LoopRegionEnabled;
    uint64_t LoopRegionStartTick;
    uint64_t LoopRegionEndTick;
    int32_t PlaybackPermille;
} SolsticeV1_SmmSessionState;

typedef struct SolsticeV1_SmmExportIntent {
    SolsticeV1_Bool Enabled;
    SolsticeV1_Bool HasVideoTarget;
    SolsticeV1_Bool HasSceneTarget;
} SolsticeV1_SmmExportIntent;

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmmClampPlayhead(
    const SolsticeV1_SmmSessionState* SessionIn,
    uint64_t* OutPlayheadTick);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmmSetViewerTab(
    SolsticeV1_SmmViewerTab Tab,
    SolsticeV1_Bool AllowOptionalTabs,
    SolsticeV1_SmmViewerTab* OutAppliedTab);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_SmmCanRunExport(
    const SolsticeV1_SmmExportIntent* Intent,
    SolsticeV1_Bool* OutCanExport);

#ifdef __cplusplus
}
#endif
