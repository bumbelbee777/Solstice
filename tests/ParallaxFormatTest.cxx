#include <Parallax/Parallax.hxx>

#include <cassert>
#include <cstring>
#include <vector>

int main() {
    using namespace Solstice::Parallax;

    auto scene = CreateScene(6000);
    assert(scene);
    assert(!scene->GetElements().empty());

    ElementIndex light = AddElement(*scene, "LightElement", "Key", 0);
    assert(light != PARALLAX_INVALID_INDEX);
    SetAttribute(*scene, light, "Intensity", 2.5f);

    std::vector<std::byte> bytes;
    ParallaxError err = ParallaxError::None;
    assert(SaveSceneToBytes(*scene, bytes, false, &err));

    ParallaxScene loaded{};
    assert(LoadSceneFromBytes(loaded, bytes, &err));
    assert(loaded.GetElements().size() == scene->GetElements().size());

    std::vector<std::byte> zbytes;
    assert(SaveSceneToBytes(*scene, zbytes, true, &err));
    ParallaxScene zloaded{};
    assert(LoadSceneFromBytes(zloaded, zbytes, &err));
    assert(zloaded.GetElements().size() == scene->GetElements().size());

    std::vector<std::byte> bad = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    assert(!LoadSceneFromBytes(loaded, bad, &err));
    assert(err == ParallaxError::InvalidMagic || err == ParallaxError::CorruptHeader);

    return 0;
}
