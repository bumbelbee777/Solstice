// Regenerates example/JhSmfProject: demo .smf, WAVs, checker PPM, and .smat for Jackhammer.
// Usage: JhSmfProjectGen [path/to/example/JhSmfProject]  (default: ./example/JhSmfProject from cwd)

#include <Material/Material.hxx>
#include <Material/SmatBinary.hxx>
#include <Smf/SmfBinary.hxx>
#include <Smf/SmfMap.hxx>
#include <Smf/SmfSpatial.hxx>
#include <Smf/SmfUtil.hxx>
#include <Smf/SmfValidate.hxx>

#include <Math/Vector.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Solstice::Smf::SmfAcousticZone;
using Solstice::Smf::SmfAttributeType;
using Solstice::Smf::SmfAuthoringBsp;
using Solstice::Smf::SmfAuthoringBspNode;
using Solstice::Smf::SmfAuthoringLight;
using Solstice::Smf::SmfAuthoringLightType;
using Solstice::Smf::SmfAuthoringOctree;
using Solstice::Smf::SmfAuthoringOctreeNode;
using Solstice::Smf::SmfEntity;
using Solstice::Smf::SmfError;
using Solstice::Smf::SmfMap;
using Solstice::Smf::SmfValue;
using Solstice::Smf::SmfVec3;
using Solstice::Smf::SmfMakeEntity;
using Solstice::Smf::SaveSmfToFile;
using Solstice::Smf::ValidateSmfMap;
using Solstice::Smf::SmfValidationReport;

uint64_t HashFnv1a(std::span<const std::byte> data) {
    uint64_t h = 14695981039346656037ull;
    for (std::byte b : data) {
        h ^= static_cast<uint8_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t HashFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return 0;
    }
    in.seekg(0, std::ios::end);
    const auto sz = in.tellg();
    in.seekg(0);
    if (sz < 0) {
        return 0;
    }
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    if (sz > 0) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));
    }
    if (!in) {
        return 0;
    }
    return HashFnv1a(buf);
}

void WriteWavTone(const std::filesystem::path& path, float freqHz, float seconds, int sampleRate) {
    const int nSamples = std::max(1, static_cast<int>(seconds * static_cast<float>(sampleRate)));
    const uint32_t dataBytes = static_cast<uint32_t>(nSamples * 2);
    const uint32_t riffSize = 36 + dataBytes;

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write wav");
    }
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riffSize), 4);
    out.write("WAVEfmt ", 8);
    uint32_t fmtChunkSize = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t byteRate = static_cast<uint32_t>(sampleRate * 2);
    uint16_t blockAlign = 2;
    uint16_t bitsPerSample = 16;
    out.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&numChannels), 2);
    out.write(reinterpret_cast<const char*>(&sampleRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataBytes), 4);

    const float twoPi = 6.28318530718f;
    for (int i = 0; i < nSamples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
        float s = std::sin(twoPi * freqHz * t) * 0.2f;
        if (freqHz < 300.f) {
            s *= 0.35f;
        }
        auto sample = static_cast<int16_t>(std::clamp(s, -1.f, 1.f) * 30000.f);
        out.write(reinterpret_cast<const char*>(&sample), 2);
    }
}

void WriteCheckerPpm8(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write ppm");
    }
    out << "P6\n8 8\n255\n";
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const bool dark = ((x / 2) + (y / 2)) % 2 == 0;
            const unsigned char r = dark ? 38 : 210;
            const unsigned char g = dark ? 42 : 215;
            const unsigned char b = dark ? 52 : 225;
            out.put(static_cast<char>(r));
            out.put(static_cast<char>(g));
            out.put(static_cast<char>(b));
        }
    }
}

void AddPathRow(SmfMap& map, const std::filesystem::path& projectRoot, const std::string& relUtf8) {
    const auto abs = projectRoot / relUtf8;
    const uint64_t h = HashFile(abs);
    map.PathTable.push_back({relUtf8, h});
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path projectRoot = std::filesystem::current_path();
        if (argc > 1) {
            projectRoot = std::filesystem::path(argv[1]);
        } else {
            projectRoot /= "example";
            projectRoot /= "JhSmfProject";
        }
        projectRoot = std::filesystem::weakly_canonical(projectRoot);

        std::filesystem::create_directories(projectRoot / "assets" / "audio");
        std::filesystem::create_directories(projectRoot / "assets" / "textures");
        std::filesystem::create_directories(projectRoot / "assets" / "materials");

        const auto wavHall = projectRoot / "assets" / "audio" / "demo_hall.wav";
        const auto wavRoom = projectRoot / "assets" / "audio" / "demo_room.wav";
        WriteWavTone(wavHall, 220.f, 1.25f, 22050);
        WriteWavTone(wavRoom, 440.f, 0.85f, 22050);

        const auto ppmPath = projectRoot / "assets" / "textures" / "checker_8.ppm";
        WriteCheckerPpm8(ppmPath);

        {
            Solstice::Core::Material m = Solstice::Core::Materials::CreateDefault();
            m.SetAlbedoColor(Solstice::Math::Vec3(0.72f, 0.68f, 0.55f), 0.42f);
            m.Metallic = 40;
            m.SetEmission(Solstice::Math::Vec3(0.02f, 0.03f, 0.04f), 0.15f);
            const auto smatPath = projectRoot / "assets" / "materials" / "demo_ceramic.smat";
            Solstice::Core::SmatError werr = Solstice::Core::SmatError::None;
            if (!Solstice::Core::WriteSmat(smatPath.string(), m, &werr) || werr != Solstice::Core::SmatError::None) {
                std::cerr << "WriteSmat failed\n";
                return 1;
            }
        }

        SmfMap map;

        {
            SmfEntity ws = SmfMakeEntity("worldspawn", "WorldSettings");
            ws.Properties.push_back({"gravity", SmfAttributeType::Float, SmfValue{9.81f}});
            ws.Properties.push_back({"title", SmfAttributeType::String, SmfValue{std::string("Jackhammer demo")}});
            map.Entities.push_back(std::move(ws));
        }
        {
            SmfEntity e = SmfMakeEntity("sun_entity", "Light");
            e.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{0.f, 12.f, 0.f}}});
            e.Properties.push_back({"style", SmfAttributeType::String, SmfValue{std::string("point")}});
            map.Entities.push_back(std::move(e));
        }
        {
            SmfEntity e = SmfMakeEntity("teapot_west", "Mesh");
            e.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{-2.8f, 0.f, 0.6f}}});
            e.Properties.push_back({"modelPath", SmfAttributeType::String, SmfValue{std::string("teapot.gltf")}});
            e.Properties.push_back({"materialPath", SmfAttributeType::String, SmfValue{std::string("assets/materials/demo_ceramic.smat")}});
            e.Properties.push_back({"diffuseTexture", SmfAttributeType::String, SmfValue{std::string("assets/textures/checker_8.ppm")}});
            map.Entities.push_back(std::move(e));
        }
        {
            SmfEntity e = SmfMakeEntity("teapot_east", "Mesh");
            e.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{2.8f, 0.f, -0.4f}}});
            e.Properties.push_back({"modelPath", SmfAttributeType::String, SmfValue{std::string("teapot.gltf")}});
            e.Properties.push_back({"roughnessTexture", SmfAttributeType::String, SmfValue{std::string("assets/textures/checker_8.ppm")}});
            map.Entities.push_back(std::move(e));
        }
        {
            SmfEntity e = SmfMakeEntity("proc_marker", "Prop");
            e.Properties.push_back({"origin", SmfAttributeType::Vec3, SmfValue{SmfVec3{0.f, 0.f, 3.2f}}});
            e.Properties.push_back({"note", SmfAttributeType::String,
                SmfValue{std::string("Arzachel: use Mesh + modelPath or procedural builders in-engine; see docs/Arzachel.md")}});
            map.Entities.push_back(std::move(e));
        }

        {
            SmfAuthoringBsp b;
            SmfAuthoringBspNode floor{};
            floor.PlaneNormal = SmfVec3{0.f, 1.f, 0.f};
            floor.PlaneD = 0.f;
            floor.FrontChild = -1;
            floor.BackChild = -1;
            floor.LeafId = 0xFFFFFFFFu;
            floor.FrontTexturePath = "assets/textures/checker_8.ppm";
            floor.BackTexturePath = "assets/textures/checker_8.ppm";
            floor.SlabValid = true;
            floor.SlabMin = SmfVec3{-9.f, -0.5f, -9.f};
            floor.SlabMax = SmfVec3{9.f, 7.f, 9.f};
            b.Nodes.push_back(floor);
            b.RootIndex = 0;
            map.Bsp = std::move(b);
        }
        {
            SmfAuthoringOctree o;
            SmfAuthoringOctreeNode r{};
            r.Min = SmfVec3{-5.f, -0.25f, -5.f};
            r.Max = SmfVec3{5.f, 5.5f, 5.f};
            r.Children.fill(-1);
            r.LeafId = 0xFFFFFFFFu;
            o.Nodes.push_back(r);
            o.RootIndex = 0;
            map.Octree = std::move(o);
        }

        map.AuthoringLights.push_back([&] {
            SmfAuthoringLight L{};
            L.Name = "key";
            L.Type = SmfAuthoringLightType::Directional;
            L.Position = SmfVec3{0.f, 0.f, 0.f};
            L.Direction = SmfVec3{0.35f, -0.82f, 0.45f};
            L.Color = SmfVec3{1.f, 0.95f, 0.88f};
            L.Intensity = 1.15f;
            return L;
        }());
        map.AuthoringLights.push_back([&] {
            SmfAuthoringLight L{};
            L.Name = "fill";
            L.Type = SmfAuthoringLightType::Point;
            L.Position = SmfVec3{-4.f, 3.2f, 2.f};
            L.Direction = SmfVec3{0.f, -1.f, 0.f};
            L.Color = SmfVec3{0.55f, 0.7f, 1.f};
            L.Intensity = 0.85f;
            L.Range = 18.f;
            return L;
        }());
        map.AuthoringLights.push_back([&] {
            SmfAuthoringLight L{};
            L.Name = "spot_teapots";
            L.Type = SmfAuthoringLightType::Spot;
            L.Position = SmfVec3{0.f, 5.f, 6.f};
            L.Direction = SmfVec3{0.f, -0.35f, -0.94f};
            L.Color = SmfVec3{1.f, 0.82f, 0.55f};
            L.Intensity = 1.4f;
            L.Range = 22.f;
            L.SpotInnerDeg = 18.f;
            L.SpotOuterDeg = 38.f;
            return L;
        }());

        map.AcousticZones.push_back([&] {
            SmfAcousticZone z{};
            z.Name = "hall_main";
            z.Center = SmfVec3{0.f, 2.f, 0.f};
            z.Extents = SmfVec3{8.f, 3.f, 8.f};
            z.ReverbPreset = "Hallway";
            z.Wetness = 0.42f;
            z.Priority = 1;
            z.AmbiencePath = "assets/audio/demo_hall.wav";
            return z;
        }());
        map.AcousticZones.push_back([&] {
            SmfAcousticZone z{};
            z.Name = "alcove";
            z.Center = SmfVec3{-4.f, 1.5f, 3.f};
            z.Extents = SmfVec3{2.2f, 2.f, 2.2f};
            z.ReverbPreset = "Room";
            z.Wetness = 0.28f;
            z.Priority = 2;
            z.MusicPath = "assets/audio/demo_room.wav";
            return z;
        }());

        AddPathRow(map, projectRoot, "assets/audio/demo_hall.wav");
        AddPathRow(map, projectRoot, "assets/audio/demo_room.wav");
        AddPathRow(map, projectRoot, "assets/textures/checker_8.ppm");
        AddPathRow(map, projectRoot, "assets/materials/demo_ceramic.smat");
        AddPathRow(map, projectRoot, "teapot.gltf");

        {
            SmfValidationReport vr;
            if (!ValidateSmfMap(map, vr) || !vr.IsFullyValid()) {
                std::cerr << "ValidateSmfMap failed\n";
                return 1;
            }
        }

        const auto smfOut = projectRoot / "JackhammerDemo.smf";
        SmfError err = SmfError::None;
        if (!SaveSmfToFile(smfOut, map, &err) || err != SmfError::None) {
            std::cerr << "SaveSmfToFile failed: " << Solstice::Smf::SmfErrorMessage(err) << '\n';
            return 1;
        }

        std::cout << "Wrote " << smfOut.string() << " and demo assets under " << projectRoot.string() << '\n';
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
