#pragma once

#include "ParallaxScene.hxx"
#include "ParallaxTypes.hxx"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>

namespace Solstice::Parallax {

/// Minimal runtime session: load `.prlx`, evaluate by tick, publish authoring sky to [`Core::AuthoringSkyboxBus`].
class ParallaxPlaybackSession {
public:
    ParallaxPlaybackSession() = default;

    bool LoadFromPath(const std::filesystem::path& path, ParallaxError* errOut = nullptr);
    bool LoadFromBytes(std::span<const std::byte> bytes, ParallaxError* errOut = nullptr);

    [[nodiscard]] const ParallaxScene& GetScene() const { return *m_Scene; }
    [[nodiscard]] uint64_t GetTimeTicks() const { return m_TimeTicks; }

    void SetTimeTicks(uint64_t ticks) { m_TimeTicks = ticks; }

    /// Fills `out` via `EvaluateScene` and optionally publishes sky when `publishSkyToBus` is true.
    void Evaluate(SceneEvaluationResult& out, bool publishSkyToBus = true);

    static void PublishSkyboxFromEvaluation(const SceneEvaluationResult& ev);

private:
    std::unique_ptr<ParallaxScene> m_Scene = std::make_unique<ParallaxScene>();
    uint64_t m_TimeTicks{0};
};

} // namespace Solstice::Parallax
