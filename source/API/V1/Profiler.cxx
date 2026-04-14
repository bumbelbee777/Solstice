#include "SolsticeAPI/V1/Profiler.h"
#include "Core/Profiling/Profiler.hxx"

extern "C" {

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerSetEnabled(SolsticeV1_Bool Enabled) {
    Solstice::Core::Profiler::Instance().SetEnabled(Enabled == SolsticeV1_True);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerBeginFrame(void) {
    Solstice::Core::Profiler::Instance().BeginFrame();
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerEndFrame(void) {
    Solstice::Core::Profiler::Instance().EndFrame();
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerBeginScope(const char* Name) {
    if (!Name) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Core::Profiler::Instance().BeginScope(Name);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerEndScope(const char* Name) {
    if (!Name) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Core::Profiler::Instance().EndScope(Name);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerSetCounter(const char* Name, int64_t Value) {
    if (!Name) {
        return SolsticeV1_ResultFailure;
    }
    Solstice::Core::Profiler::Instance().SetCounter(Name, Value);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerGetCounter(const char* Name, int64_t* OutValue) {
    if (!Name || !OutValue) {
        return SolsticeV1_ResultFailure;
    }
    *OutValue = Solstice::Core::Profiler::Instance().GetCounter(Name);
    return SolsticeV1_ResultSuccess;
}

SOLSTICE_V1_API SolsticeV1_ResultCode SolsticeV1_ProfilerGetLastFrame(float* OutFrameMs, float* OutFps) {
    const auto stats = Solstice::Core::Profiler::Instance().GetLastFrameStats();
    if (OutFrameMs) {
        *OutFrameMs = stats.FrameTime;
    }
    if (OutFps) {
        *OutFps = stats.FPS;
    }
    return SolsticeV1_ResultSuccess;
}

} // extern "C"
