#include <Parallax/ParallaxPlayback.hxx>

#include <Core/AuthoringSkyboxBus.hxx>

namespace Solstice::Parallax {

bool ParallaxPlaybackSession::LoadFromPath(const std::filesystem::path& path, ParallaxError* errOut) {
    std::unique_ptr<ParallaxScene> next = LoadScene(path, nullptr, errOut);
    if (!next) {
        return false;
    }
    m_Scene = std::move(next);
    return true;
}

bool ParallaxPlaybackSession::LoadFromBytes(std::span<const std::byte> bytes, ParallaxError* errOut) {
    return LoadSceneFromBytes(*m_Scene, bytes, errOut);
}

void ParallaxPlaybackSession::PublishSkyboxFromEvaluation(const SceneEvaluationResult& ev) {
    if (!ev.EnvironmentSkybox.has_value()) {
        return;
    }
    const SkyboxAuthoringState& sk = *ev.EnvironmentSkybox;
    if (!sk.Enabled) {
        return;
    }
    Core::AuthoringSkyboxState pub{};
    pub.Enabled = true;
    pub.Brightness = sk.Brightness;
    pub.YawDegrees = sk.YawDegrees;
    for (int i = 0; i < 6; ++i) {
        pub.FacePaths[static_cast<size_t>(i)] = sk.FacePaths[static_cast<size_t>(i)];
    }
    Core::PublishAuthoringSkyboxState(pub);
}

void ParallaxPlaybackSession::Evaluate(SceneEvaluationResult& out, bool publishSkyToBus) {
    EvaluateScene(*m_Scene, m_TimeTicks, out);
    if (publishSkyToBus) {
        PublishSkyboxFromEvaluation(out);
    }
}

} // namespace Solstice::Parallax
