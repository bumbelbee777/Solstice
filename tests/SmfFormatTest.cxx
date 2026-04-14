#include <Smf/SmfBinary.hxx>

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

    const auto tmp = std::filesystem::temp_directory_path() / "solstice_smf_roundtrip_test.smf";
    err = SmfError::None;
    assert(SaveSmfToFile(tmp, map, &err));
    SmfMap fromDisk;
    SmfFileHeader fh2{};
    assert(LoadSmfFromFile(tmp, fromDisk, &fh2, &err));
    assert(fromDisk.Entities.size() == map.Entities.size());
    std::filesystem::remove(tmp);

    return 0;
}
