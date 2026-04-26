#include <Smf/SmfBinary.hxx>
#include <Smf/SmfValidate.hxx>

#include <cassert>
#include <cstring>
#include <filesystem>
#include <vector>

using namespace Solstice::Smf;

static bool ValueEqual(const SmfValue& a, const SmfValue& b) {
    if (a.index() != b.index()) {
        return false;
    }
    if (std::holds_alternative<float>(a)) {
        return std::get<float>(a) == std::get<float>(b);
    }
    if (std::holds_alternative<std::string>(a)) {
        return std::get<std::string>(a) == std::get<std::string>(b);
    }
    if (std::holds_alternative<SmfVec3>(a)) {
        auto u = std::get<SmfVec3>(a);
        auto v = std::get<SmfVec3>(b);
        return u.x == v.x && u.y == v.y && u.z == v.z;
    }
    return false;
}

static bool EntityEqual(const SmfEntity& a, const SmfEntity& b) {
    if (a.Name != b.Name || a.ClassName != b.ClassName || a.Properties.size() != b.Properties.size()) {
        return false;
    }
    for (size_t i = 0; i < a.Properties.size(); ++i) {
        if (a.Properties[i].Key != b.Properties[i].Key || a.Properties[i].Type != b.Properties[i].Type) {
            return false;
        }
        if (!ValueEqual(a.Properties[i].Value, b.Properties[i].Value)) {
            return false;
        }
    }
    return true;
}

int main() {
    SmfMap map;
    SmfEntity e1;
    e1.Name = "lamp_0";
    e1.ClassName = "Light";
    e1.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{1.f, 2.f, 3.f}}});
    e1.Properties.push_back({"style", SmfAttributeType::String, SmfValue{std::string("point")}});
    map.Entities.push_back(std::move(e1));

    SmfEntity e2;
    e2.Name = "worldspawn";
    e2.ClassName = "WorldSettings";
    e2.Properties.push_back({"gravity", SmfAttributeType::Float, SmfValue{9.81f}});
    map.Entities.push_back(std::move(e2));

    map.PathTable.push_back({"meshes/foo.gltf", 0xDEADBEEFCAFEBABEull});

    std::vector<std::byte> bytes;
    SmfError err = SmfError::None;
    assert(SaveSmfToBytes(map, bytes, &err));

    SmfMap loaded;
    SmfFileHeader hdr{};
    assert(LoadSmfFromBytes(loaded, bytes, &hdr, &err) && err == SmfError::None);
    assert(hdr.Magic == SMF_MAGIC);
    assert(hdr.BspSize == 0);

    {
        SmfValidationReport vr;
        assert(ValidateSmfMap(map, vr));
        assert(vr.loadOk && vr.roundTripOk);
        assert(vr.IsFullyValid());
    }
    {
        SmfValidationReport vr;
        assert(ValidateSmfBytes(bytes, vr));
        assert(vr.loadOk && vr.roundTripOk);
    }

    assert(loaded.Entities.size() == map.Entities.size());
    assert(loaded.PathTable == map.PathTable);
    for (size_t i = 0; i < map.Entities.size(); ++i) {
        assert(EntityEqual(map.Entities[i], loaded.Entities[i]));
    }

    std::vector<std::byte> zbytes;
    err = SmfError::None;
    assert(SaveSmfToBytes(map, zbytes, &err, true));
    SmfMap zloaded;
    assert(LoadSmfFromBytes(zloaded, zbytes, nullptr, &err) && err == SmfError::None);
    assert(zloaded.Entities.size() == map.Entities.size());
    assert(zloaded.PathTable == map.PathTable);

    std::vector<std::byte> bad = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    err = SmfError::None;
    assert(!LoadSmfFromBytes(loaded, bad, nullptr, &err));
    assert(err == SmfError::InvalidMagic || err == SmfError::CorruptHeader);
    {
        SmfValidationReport vr;
        assert(!ValidateSmfBytes(bad, vr));
        assert(!vr.loadOk);
        assert(vr.loadError != SmfError::None);
    }

    const auto tmp = std::filesystem::temp_directory_path() / "solstice_smf_roundtrip_test.smf";
    err = SmfError::None;
    assert(SaveSmfToFile(tmp, map, &err));
    SmfMap fromDisk;
    SmfFileHeader fh2{};
    assert(LoadSmfFromFile(tmp, fromDisk, &fh2, &err));
    assert(fromDisk.Entities.size() == map.Entities.size());
    {
        SmfValidationReport vr;
        assert(ValidateSmfFile(tmp, vr));
        assert(vr.loadOk && vr.roundTripOk);
    }
    std::filesystem::remove(tmp);

    {
        SmfMap spatialMap;
        SmfEntity e;
        e.Name = "x";
        e.ClassName = "y";
        spatialMap.Entities.push_back(std::move(e));

        SmfAuthoringBsp bsp;
        bsp.RootIndex = 0;
        SmfAuthoringBspNode root{};
        root.PlaneNormal = SmfVec3{0.f, 1.f, 0.f};
        root.PlaneD = 0.f;
        root.FrontChild = -1;
        root.BackChild = -1;
        root.LeafId = 1u;
        root.FrontTexturePath = "textures/bsp_front.png";
        root.BackTexturePath = "";
        root.SlabValid = true;
        root.SlabMin = SmfVec3{-1.f, -2.f, -3.f};
        root.SlabMax = SmfVec3{4.f, 5.f, 6.f};
        bsp.Nodes.push_back(root);
        spatialMap.Bsp = std::move(bsp);

        SmfAuthoringOctree oct;
        oct.RootIndex = 0;
        SmfAuthoringOctreeNode on;
        on.Min = SmfVec3{0.f, 0.f, 0.f};
        on.Max = SmfVec3{1.f, 1.f, 1.f};
        on.Children.fill(-1);
        on.LeafId = 2u;
        oct.Nodes.push_back(on);
        spatialMap.Octree = std::move(oct);

        std::vector<std::byte> sb;
        err = SmfError::None;
        assert(SaveSmfToBytes(spatialMap, sb, &err));
        SmfMap sloaded;
        SmfFileHeader sh{};
        assert(LoadSmfFromBytes(sloaded, sb, &sh, &err) && err == SmfError::None);
        assert(sh.BspSize > 0);
        assert(sloaded.Bsp.has_value());
        assert(sloaded.Octree.has_value());
        assert(sloaded.Bsp->Nodes.size() == spatialMap.Bsp->Nodes.size());
        assert(sloaded.Bsp->Nodes[0].FrontTexturePath == spatialMap.Bsp->Nodes[0].FrontTexturePath);
        assert(sloaded.Bsp->Nodes[0].SlabValid == spatialMap.Bsp->Nodes[0].SlabValid);
        assert(sloaded.Bsp->Nodes[0].SlabMax.z == spatialMap.Bsp->Nodes[0].SlabMax.z);
        assert(sloaded.Octree->Nodes.size() == spatialMap.Octree->Nodes.size());
    }

    {
        SmfMap g;
        SmfAcousticZone az;
        az.Name = "cave_main";
        az.Center = SmfVec3{10.f, 2.f, -3.f};
        az.Extents = SmfVec3{20.f, 8.f, 12.f};
        az.ReverbPreset = "Cave";
        az.Wetness = 0.5f;
        az.ObstructionMultiplier = 0.9f;
        az.Priority = 2;
        az.IsSpherical = false;
        az.Enabled = true;
        az.MusicPath = "assets/audio/room.wav";
        az.AmbiencePath = "assets/audio/wind.wav";
        g.AcousticZones.push_back(std::move(az));

        SmfAuthoringLight lt;
        lt.Name = "key_fill";
        lt.Type = SmfAuthoringLightType::Spot;
        lt.Position = SmfVec3{0.f, 5.f, 0.f};
        lt.Direction = SmfVec3{0.f, -1.f, 0.f};
        lt.Color = SmfVec3{1.f, 0.95f, 0.9f};
        lt.Intensity = 2.5f;
        lt.Hue = 0.f;
        lt.Attenuation = 1.f;
        lt.Range = 30.f;
        lt.SpotInnerDeg = 25.f;
        lt.SpotOuterDeg = 40.f;
        g.AuthoringLights.push_back(std::move(lt));

        std::vector<std::byte> gb;
        err = SmfError::None;
        assert(SaveSmfToBytes(g, gb, &err));
        SmfMap g2;
        SmfFileHeader gh{};
        assert(LoadSmfFromBytes(g2, gb, &gh, &err) && err == SmfError::None);
        assert(gh.FormatVersionMinor == SMF_FORMAT_VERSION_MINOR);
        assert(g2.AcousticZones.size() == 1);
        assert(g2.AuthoringLights.size() == 1);
        assert(g2.AcousticZones[0].Name == "cave_main");
        assert(g2.AcousticZones[0].ReverbPreset == "Cave");
        assert(g2.AcousticZones[0].MusicPath == "assets/audio/room.wav");
        assert(g2.AcousticZones[0].AmbiencePath == "assets/audio/wind.wav");
        assert(g2.AuthoringLights[0].Type == SmfAuthoringLightType::Spot);
        assert(g2.AuthoringLights[0].Range == 30.f);
    }

    {
        SmfMap skyMap;
        skyMap.Skybox = SmfSkybox{};
        skyMap.Skybox->Enabled = true;
        skyMap.Skybox->Brightness = 1.25f;
        skyMap.Skybox->YawDegrees = 15.f;
        skyMap.Skybox->FacePaths[0] = "sky/px.png";
        skyMap.Skybox->FacePaths[1] = "sky/nx.png";
        skyMap.Skybox->FacePaths[2] = "sky/py.png";
        skyMap.Skybox->FacePaths[3] = "sky/ny.png";
        skyMap.Skybox->FacePaths[4] = "sky/pz.png";
        skyMap.Skybox->FacePaths[5] = "sky/nz.png";
        std::vector<std::byte> sb;
        err = SmfError::None;
        assert(SaveSmfToBytes(skyMap, sb, &err));
        SmfMap s2;
        assert(LoadSmfFromBytes(s2, sb, nullptr, &err) && err == SmfError::None);
        assert(s2.Skybox.has_value());
        assert(s2.Skybox->Enabled);
        assert(s2.Skybox->FacePaths[0] == "sky/px.png");
        assert(s2.Skybox->FacePaths[5] == "sky/nz.png");
    }

    {
        SmfMap hookMap;
        hookMap.WorldAuthoringHooks.ScriptPath = "scripts/entry.mw";
        hookMap.WorldAuthoringHooks.CutscenePath = "narrative/opening.json";
        hookMap.WorldAuthoringHooks.WorldSpaceUiPath = "ui/world_hud.json";
        std::vector<std::byte> hb;
        err = SmfError::None;
        assert(SaveSmfToBytes(hookMap, hb, &err));
        SmfMap h2;
        assert(LoadSmfFromBytes(h2, hb, nullptr, &err) && err == SmfError::None);
        assert(h2.WorldAuthoringHooks.ScriptPath == hookMap.WorldAuthoringHooks.ScriptPath);
        assert(h2.WorldAuthoringHooks.CutscenePath == hookMap.WorldAuthoringHooks.CutscenePath);
        assert(h2.WorldAuthoringHooks.WorldSpaceUiPath == hookMap.WorldAuthoringHooks.WorldSpaceUiPath);
    }

    {
        SmfMap fluidMap;
        SmfFluidVolume fv;
        fv.Name = "room_air";
        fv.BoundsMin = SmfVec3{-1.f, 0.f, -1.f};
        fv.BoundsMax = SmfVec3{2.f, 3.f, 1.5f};
        fv.ResolutionX = 24;
        fv.ResolutionY = 16;
        fv.ResolutionZ = 20;
        fv.Viscosity = 0.0002f;
        fv.Diffusion = 0.00015f;
        fv.EnableBoussinesq = true;
        fluidMap.FluidVolumes.push_back(std::move(fv));
        std::vector<std::byte> fb;
        err = SmfError::None;
        assert(SaveSmfToBytes(fluidMap, fb, &err));
        SmfMap f2;
        assert(LoadSmfFromBytes(f2, fb, nullptr, &err) && err == SmfError::None);
        assert(f2.FluidVolumes.size() == 1);
        assert(f2.FluidVolumes[0].Name == "room_air");
        assert(f2.FluidVolumes[0].ResolutionX == 24);
        assert(f2.FluidVolumes[0].EnableBoussinesq);
    }

    {
        SmfMap bx;
        SmfAuthoringBsp bsp;
        SmfAuthoringBspNode n{};
        n.PlaneNormal = SmfVec3{0.f, 1.f, 0.f};
        n.PlaneD = 0.f;
        n.FrontChild = -1;
        n.BackChild = -1;
        n.LeafId = 0xFFFFFFFFu;
        n.FrontTexturePath = "t.png";
        n.SlabValid = true;
        n.SlabMin = SmfVec3{0.f, 0.f, 0.f};
        n.SlabMax = SmfVec3{1.f, 2.f, 3.f};
        n.HasFrontTextureXform = true;
        n.FrontTextureXform.ShiftU = 0.1f;
        n.FrontTextureXform.ScaleU = 2.f;
        n.FrontTextureXform.RotateDeg = 45.f;
        bsp.Nodes.push_back(n);
        bsp.RootIndex = 0;
        bx.Bsp = std::move(bsp);
        std::vector<std::byte> bb;
        err = SmfError::None;
        assert(SaveSmfToBytes(bx, bb, &err));
        SmfMap b2;
        assert(LoadSmfFromBytes(b2, bb, nullptr, &err) && err == SmfError::None);
        assert(b2.Bsp.has_value());
        assert(b2.Bsp->Nodes[0].HasFrontTextureXform);
        assert(b2.Bsp->Nodes[0].FrontTextureXform.RotateDeg == 45.f);
    }

    return 0;
}
