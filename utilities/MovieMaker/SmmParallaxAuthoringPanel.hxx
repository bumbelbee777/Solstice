#pragma once

struct SDL_Window;

namespace Solstice::Parallax {
class ParallaxScene;
}

namespace Smm {
/// **Properties** block: `SceneRoot` skybox (Jackhammer-style cubemap face paths) + env toggles, saved in `.prlx`.
void DrawParallaxRootEnvironment(
    Solstice::Parallax::ParallaxScene& scene, SDL_Window* window, bool compressPrlx, bool& sceneDirty);
/// **Properties** block for `ActorElement`: Arzachel rigid damage amount, LOD distances, animation / destruction preset labels.
void DrawActorArzachelFields(
    Solstice::Parallax::ParallaxScene& scene, int elementIndex, bool compressPrlx, bool& sceneDirty);
} // namespace Smm
