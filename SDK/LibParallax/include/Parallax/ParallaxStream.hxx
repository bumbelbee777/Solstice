#pragma once

#include "ParallaxScene.hxx"
#include <Core/Profiling/Profiler.hxx>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace Solstice::Parallax {

class ParallaxStreamReader {
public:
    ParallaxStreamReader(const std::filesystem::path& path, IAssetResolver* resolver = nullptr)
        : m_Path(path), m_Resolver(resolver) {}

    bool Open(ParallaxError* outError = nullptr) {
        Solstice::Core::Profiler::Instance().BeginScope("Parallax.Open");
        std::ifstream f(m_Path, std::ios::binary);
        if (!f) {
            Solstice::Core::Profiler::Instance().EndScope("Parallax.Open");
            if (outError) {
                *outError = ParallaxError::StreamingError;
            }
            return false;
        }
        f.seekg(0, std::ios::end);
        auto sz = f.tellg();
        f.seekg(0);
        m_Buffer.resize(static_cast<size_t>(sz));
        f.read(reinterpret_cast<char*>(m_Buffer.data()), sz);
        m_Scene = std::make_unique<ParallaxScene>();
        if (!LoadSceneFromBytes(*m_Scene, m_Buffer, outError)) {
            m_Scene.reset();
            Solstice::Core::Profiler::Instance().EndScope("Parallax.Open");
            return false;
        }
        Solstice::Core::Profiler::Instance().EndScope("Parallax.Open");
        return true;
    }

    bool PrefetchWindow(uint64_t startTicks, uint64_t endTicks) {
        (void)startTicks;
        (void)endTicks;
        Solstice::Core::Profiler::Instance().BeginScope("Parallax.PrefetchWindow");
        Solstice::Core::Profiler::Instance().EndScope("Parallax.PrefetchWindow");
        return true;
    }

    void Evaluate(uint64_t timeTicks, SceneEvaluationResult& outResult) {
        Solstice::Core::Profiler::Instance().BeginScope("Parallax.Evaluate");
        if (m_Scene) {
            EvaluateScene(*m_Scene, timeTicks, outResult);
        }
        Solstice::Core::Profiler::Instance().EndScope("Parallax.Evaluate");
    }

    MGDisplayList EvaluateMG(uint64_t timeTicks) {
        Solstice::Core::Profiler::Instance().BeginScope("Parallax.EvaluateMG");
        MGDisplayList r;
        if (m_Scene) {
            r = Solstice::Parallax::EvaluateMG(*m_Scene, timeTicks);
        }
        Solstice::Core::Profiler::Instance().EndScope("Parallax.EvaluateMG");
        return r;
    }

    uint64_t GetDurationTicks() const { return m_Scene ? m_Scene->GetTimelineDurationTicks() : 0; }

    uint32_t GetTicksPerSecond() const { return m_Scene ? m_Scene->GetTicksPerSecond() : 6000; }

    ParallaxScene* GetScene() const { return m_Scene.get(); }

private:
    std::filesystem::path m_Path;
    IAssetResolver* m_Resolver{nullptr};
    std::vector<std::byte> m_Buffer;
    std::unique_ptr<ParallaxScene> m_Scene;
};

} // namespace Solstice::Parallax
