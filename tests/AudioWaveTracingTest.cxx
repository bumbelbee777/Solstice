#include "TestHarness.hxx"
#include <Core/Audio/WaveTracing/WaveTracer.hxx>

using namespace Solstice::Core::Audio;

int main() {
    WaveTracer tracer;
    tracer.Initialize(nullptr);
    tracer.SetMaxRays(128);
    tracer.SetMaxBounces(3);

    std::vector<AudioSource> sources(2);
    sources[0].Position = Math::Vec3(0.0f, 0.0f, -5.0f);
    sources[0].Direction = Math::Vec3(0.0f, 0.0f, 1.0f);
    sources[0].MaxDistance = 60.0f;
    sources[1].Position = Math::Vec3(3.0f, 1.0f, -10.0f);
    sources[1].Direction = Math::Vec3(-1.0f, 0.0f, 0.0f);
    sources[1].MaxDistance = 90.0f;

    std::vector<SoundRayResult> results;
    tracer.Trace(sources, Math::Vec3(0.0f, 0.0f, 0.0f), results);
    TEST_ASSERT(results.size() == sources.size());
    TEST_ASSERT(results[0].Distance > 0.0f);
    WaveTraceReverbParams rp = tracer.ComputeReverb(results);
    TEST_ASSERT(rp.DecayTime >= 0.0f);
    tracer.Shutdown();
    TEST_PASS();
}
