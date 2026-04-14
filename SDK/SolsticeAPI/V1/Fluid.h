#pragma once

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SolsticeV1_FluidOpaque SolsticeV1_FluidOpaque;
typedef SolsticeV1_FluidOpaque* SolsticeV1_FluidHandle;

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidCreate(
    int Nx,
    int Ny,
    int Nz,
    float Hx,
    float Hy,
    float Hz,
    float Diffusion,
    float Viscosity,
    SolsticeV1_FluidHandle* OutHandle);
SOLSTICE_V1_API void SolsticeV1_FluidDestroy(SolsticeV1_FluidHandle Handle);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidStep(SolsticeV1_FluidHandle Handle, float Dt);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidAddDensity(
    SolsticeV1_FluidHandle Handle,
    int X,
    int Y,
    int Z,
    float Amount);
SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidAddVelocity(
    SolsticeV1_FluidHandle Handle,
    int X,
    int Y,
    int Z,
    float Vx,
    float Vy,
    float Vz);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidSetPressureMultigrid(
    SolsticeV1_FluidHandle Handle,
    SolsticeV1_Bool Enable,
    int Levels,
    int PreSmooth,
    int PostSmooth,
    int CoarseSmooth,
    int RelaxIterations);

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_FluidGetDivergenceMetrics(
    SolsticeV1_FluidHandle Handle,
    float* OutMeanAbsDivergence,
    float* OutMaxAbsDivergence);

#ifdef __cplusplus
}
#endif
