#include <Parallax/MGRaster.hxx>
#include <Parallax/Parallax.hxx>
#include <Parallax/ParallaxScene.hxx>
#include <Parallax/ParallaxTypes.hxx>

#include <Math/Vector.hxx>

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
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

    MGDisplayList::Entry ed{};
    assert(MGEntryCompositeDepth(ed) == 0.f);
    ed.Attributes["Depth"] = 3.5f;
    assert(MGEntryCompositeDepth(ed) == 3.5f);

    ElementIndex fluid = AddElement(*scene, "SmmFluidVolumeElement", "TestFluid", 0);
    assert(fluid != PARALLAX_INVALID_INDEX);
    using namespace Solstice;
    SetAttribute(*scene, fluid, "Enabled", Parallax::AttributeValue{true});
    SetAttribute(*scene, fluid, "BoundsMin", Parallax::AttributeValue{Math::Vec3{0.f, 0.f, 0.f}});
    SetAttribute(*scene, fluid, "BoundsMax", Parallax::AttributeValue{Math::Vec3{2.f, 2.f, 2.f}});
    SetAttribute(*scene, fluid, "ResolutionX", Parallax::AttributeValue{int32_t{16}});
    SetAttribute(*scene, fluid, "ResolutionY", Parallax::AttributeValue{int32_t{16}});
    SetAttribute(*scene, fluid, "ResolutionZ", Parallax::AttributeValue{int32_t{16}});

    bytes.clear();
    assert(SaveSceneToBytes(*scene, bytes, false, &err));
    Parallax::ParallaxScene fluidLoad{};
    assert(LoadSceneFromBytes(fluidLoad, bytes, &err));
    Parallax::SceneEvaluationResult fev{};
    Parallax::EvaluateScene(fluidLoad, 0, fev);
    assert(fev.FluidVolumes.size() == 1u);
    assert(std::abs(fev.FluidVolumes[0].BoundsMax.x - 2.f) < 1e-3f);

    {
        Parallax::SetAttribute(
            *scene, 0, "SkyboxEnabled", Parallax::AttributeValue{true});
        Parallax::SetAttribute(
            *scene, 0, "SkyboxBrightness", Parallax::AttributeValue{1.5f});
        Parallax::SetAttribute(
            *scene, 0, "SkyboxYawDegrees", Parallax::AttributeValue{12.f});
        Parallax::SetAttribute(
            *scene, 0, "SkyboxFacePosX", Parallax::AttributeValue{std::string("a/px.png")});
        const Parallax::ElementIndex act =
            Parallax::AddElement(*scene, "ActorElement", "PropA", 0);
        assert(act != Parallax::PARALLAX_INVALID_INDEX);
        Parallax::SetAttribute(
            *scene, act, "ArzachelRigidBodyDamage", Parallax::AttributeValue{0.4f});
        Parallax::SetAttribute(
            *scene, act, "LodDistanceHigh", Parallax::AttributeValue{80.f});
        Parallax::SetAttribute(
            *scene, act, "LodDistanceLow", Parallax::AttributeValue{400.f});
        Parallax::SetAttribute(
            *scene, act, "ArzachelAnimationClipPreset", Parallax::AttributeValue{std::string("Walk")});
        Parallax::SetAttribute(
            *scene, act, "ArzachelDestructionAnimPreset", Parallax::AttributeValue{std::string("BreakA")});
        std::vector<std::byte> abytes;
        assert(SaveSceneToBytes(*scene, abytes, false, &err));
        Parallax::ParallaxScene reloaded{};
        assert(LoadSceneFromBytes(reloaded, abytes, &err));
        Parallax::SceneEvaluationResult sev{};
        Parallax::EvaluateScene(reloaded, 0, sev);
        assert(sev.EnvironmentSkybox.has_value());
        assert(sev.EnvironmentSkybox->Enabled);
        assert(std::abs(sev.EnvironmentSkybox->Brightness - 1.5f) < 1e-3f);
        assert(sev.EnvironmentSkybox->FacePaths[0] == "a/px.png");
        assert(!sev.ActorArzachelAuthoring.empty());
        const auto& a0 = sev.ActorArzachelAuthoring[0];
        assert(std::abs(a0.RigidBodyDamage - 0.4f) < 1e-3f);
        assert(a0.AnimationClipPreset == "Walk");
        assert(a0.DestructionAnimPreset == "BreakA");
        assert(std::abs(a0.LodDistanceHigh - 80.f) < 1e-3f);
        assert(sev.ActorFacialPoses.size() == sev.ActorArzachelAuthoring.size());
        assert(sev.ActorFacialPoses.size() == 1u);
        assert(sev.ActorFacialPoses[0].Element == a0.Element);

        const Parallax::ElementIndex act2 = Parallax::FindElement(reloaded, "PropA");
        assert(act2 != Parallax::PARALLAX_INVALID_INDEX);
        const Parallax::ChannelIndex chV = Parallax::AddChannel(
            reloaded, act2, Parallax::kChannelFacialVisemeId, Parallax::AttributeType::String);
        const Parallax::ChannelIndex chW = Parallax::AddChannel(
            reloaded, act2, Parallax::kChannelFacialVisemeWeight, Parallax::AttributeType::Float);
        assert(chV != Parallax::PARALLAX_INVALID_INDEX && chW != Parallax::PARALLAX_INVALID_INDEX);
        Parallax::AddKeyframe(reloaded, chV, 0, Parallax::AttributeValue{std::string("aa")}, Parallax::EasingType::Linear);
        Parallax::AddKeyframe(reloaded, chW, 0, Parallax::AttributeValue{1.0f}, Parallax::EasingType::Linear);
        Parallax::SceneEvaluationResult sevFace{};
        Parallax::EvaluateScene(reloaded, 0, sevFace);
        assert(sevFace.ActorFacialPoses.size() == 1u);
        assert(!sevFace.ActorFacialPoses[0].BoneDeltasByName.empty());
    }

    {
        // MG raster: root `Depth` sort is stable and order-invariant (regression for compositing).
        using Solstice::Parallax::MGDisplayList;
        auto makeSprite = [](float depth) {
            MGDisplayList::Entry e{};
            e.SchemaType = "MGSpriteElement";
            e.Attributes["Depth"] = depth;
            e.Attributes["Position"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{-16.f, -16.f}};
            e.Attributes["Size"] = Solstice::Parallax::AttributeValue{Solstice::Math::Vec2{24.f, 24.f}};
            return e;
        };
        MGDisplayList a{};
        a.Entries.push_back(makeSprite(0.f));
        a.Entries.push_back(makeSprite(1.f));
        MGDisplayList b{};
        b.Entries.push_back(makeSprite(1.f));
        b.Entries.push_back(makeSprite(0.f));
        constexpr uint32_t kW = 32;
        constexpr uint32_t kH = 32;
        std::vector<std::byte> ra(static_cast<size_t>(kW) * static_cast<size_t>(kH) * 4u);
        std::vector<std::byte> rb(static_cast<size_t>(kW) * static_cast<size_t>(kH) * 4u);
        Solstice::Parallax::RasterizeMGDisplayList(
            a, nullptr, kW, kH, std::span<std::byte>(ra.data(), ra.size()));
        Solstice::Parallax::RasterizeMGDisplayList(
            b, nullptr, kW, kH, std::span<std::byte>(rb.data(), rb.size()));
        assert(ra.size() == rb.size());
        assert(std::memcmp(ra.data(), rb.data(), ra.size()) == 0);
        // Same list rasterized twice is deterministic.
        std::vector<std::byte> ra2 = ra;
        Solstice::Parallax::RasterizeMGDisplayList(
            a, nullptr, kW, kH, std::span<std::byte>(ra2.data(), ra2.size()));
        assert(ra == ra2);
    }

    {
        // Parallax format v2: float Bezier tangents + Hold on end key survive save/load.
        auto scB = CreateScene(6000);
        const ElementIndex li = AddElement(*scB, "LightElement", "V2Bezier", 0);
        assert(li != PARALLAX_INVALID_INDEX);
        const ChannelIndex chB = AddChannel(*scB, li, "Intensity", AttributeType::Float);
        assert(chB < scB->GetChannels().size());
        AddKeyframe(*scB, chB, 0, AttributeValue{0.5f}, EasingType::Linear);
        AddKeyframe(*scB, chB, 1000, AttributeValue{2.0f}, EasingType::Linear);
        SetKeyframeInterpolation(*scB, chB, 1000, KeyframeInterpolation::Bezier);
        SetKeyframeBezierTangents(*scB, chB, 1000, 0.25f, 0.5f);
        std::vector<std::byte> bBytes;
        assert(SaveSceneToBytes(*scB, bBytes, false, &err));
        ParallaxScene loadedB{};
        assert(LoadSceneFromBytes(loadedB, bBytes, &err));
        const auto& lch = loadedB.GetChannels()[chB];
        const KeyframeRecord* kEnd = nullptr;
        for (const auto& k : lch.Keyframes) {
            if (k.TimeTicks == 1000) {
                kEnd = &k;
            }
        }
        assert(kEnd);
        assert(static_cast<KeyframeInterpolation>(kEnd->Interp) == KeyframeInterpolation::Bezier);
        assert(std::abs(kEnd->TangentOut - 0.25f) < 1e-4f);
        assert(std::abs(kEnd->TangentIn - 0.5f) < 1e-4f);

        auto scH = CreateScene(6000);
        const ElementIndex li2 = AddElement(*scH, "LightElement", "V2Hold", 0);
        const ChannelIndex chH = AddChannel(*scH, li2, "Intensity", AttributeType::Float);
        AddKeyframe(*scH, chH, 0, AttributeValue{0.0f}, EasingType::Linear);
        AddKeyframe(*scH, chH, 1000, AttributeValue{1.0f}, EasingType::Linear);
        AddKeyframe(*scH, chH, 2000, AttributeValue{1.0f}, EasingType::Linear);
        SetKeyframeInterpolation(*scH, chH, 1000, KeyframeInterpolation::Hold);
        std::vector<std::byte> hBytes;
        assert(SaveSceneToBytes(*scH, hBytes, false, &err));
        ParallaxScene loadedH{};
        assert(LoadSceneFromBytes(loadedH, hBytes, &err));
        const AttributeValue ev = EvaluateChannel(loadedH, chH, 500);
        const float* fp = std::get_if<float>(&ev);
        assert(fp && std::abs(*fp) < 1e-5f);
    }

    {
        ParallaxPlaybackSession play;
        assert(play.LoadFromBytes(bytes, &err));
        assert(play.GetScene().GetElements().size() == scene->GetElements().size());
        SceneEvaluationResult pev{};
        play.Evaluate(pev, false);
        (void)pev;
    }

    return 0;
}
