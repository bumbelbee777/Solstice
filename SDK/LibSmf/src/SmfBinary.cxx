#include <Smf/SmfBinary.hxx>
#include <Smf/SmfWire.hxx>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include <zstd.h>

namespace Solstice::Smf {

bool SaveSmfToBytes(const SmfMap& map, std::vector<std::byte>& out, SmfError* err, bool compressTail) {
    (void)err;
    std::unordered_map<std::string, uint32_t, std::hash<std::string>, std::equal_to<>> strOff;
    std::vector<std::string> poolOrder;

    auto intern = [&](const std::string& s) -> uint32_t {
        auto it = strOff.find(s);
        if (it != strOff.end()) {
            return it->second;
        }
        uint32_t off = 0;
        for (const auto& e : poolOrder) {
            off += static_cast<uint32_t>(e.size()) + 1;
        }
        strOff[s] = off;
        poolOrder.push_back(s);
        return off;
    };

    for (const auto& e : map.Entities) {
        intern(e.Name);
        intern(e.ClassName);
        for (const auto& prop : e.Properties) {
            intern(prop.Key);
        }
    }
    for (const auto& kv : map.PathTable) {
        intern(kv.first);
    }

    std::vector<std::byte> stringTable;
    for (const auto& s : poolOrder) {
        for (char c : s) {
            stringTable.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
        }
        stringTable.push_back(std::byte{0});
    }

    std::vector<std::byte> geometryBlob;
    Wire::AppendU32(geometryBlob, 0); // brushCount

    std::vector<std::byte> entityBlob;
    Wire::AppendU32(entityBlob, static_cast<uint32_t>(map.Entities.size()));
    for (const auto& ent : map.Entities) {
        Wire::AppendU32(entityBlob, intern(ent.Name));
        Wire::AppendU32(entityBlob, intern(ent.ClassName));
        Wire::AppendU32(entityBlob, static_cast<uint32_t>(ent.Properties.size()));
        for (const auto& prop : ent.Properties) {
            SmfAttributeType at = prop.Type;
            Wire::AppendU32(entityBlob, intern(prop.Key));
            entityBlob.push_back(static_cast<std::byte>(static_cast<uint8_t>(at)));
            entityBlob.push_back(std::byte{0});
            entityBlob.push_back(std::byte{0});
            entityBlob.push_back(std::byte{0});
            Wire::WriteAttributeValue(entityBlob, at, prop.Value);
        }
    }

    std::vector<std::byte> sectorBlob;
    Wire::AppendU32(sectorBlob, 0);

    std::vector<std::byte> physicsBlob;
    Wire::AppendU32(physicsBlob, 0);

    std::vector<std::byte> scriptBlob;
    Wire::AppendU32(scriptBlob, 0);

    std::vector<std::byte> triggerBlob;
    Wire::AppendU32(triggerBlob, 0);

    std::vector<std::byte> pathBlob;
    Wire::AppendU32(pathBlob, static_cast<uint32_t>(map.PathTable.size()));
    for (const auto& kv : map.PathTable) {
        SmfPathTableEntryDisk pe{};
        pe.PathOffset = intern(kv.first);
        pe.AssetHash = kv.second;
        pathBlob.insert(pathBlob.end(), reinterpret_cast<const std::byte*>(&pe),
            reinterpret_cast<const std::byte*>(&pe) + sizeof(pe));
    }

    std::vector<std::byte> file;
    file.resize(sizeof(SmfFileHeader));

    auto append = [&](const std::vector<std::byte>& sec) -> uint32_t {
        uint32_t o = static_cast<uint32_t>(file.size());
        file.insert(file.end(), sec.begin(), sec.end());
        return o;
    };

    SmfFileHeader fh{};
    fh.Magic = SMF_MAGIC;
    fh.FormatVersionMajor = SMF_FORMAT_VERSION_MAJOR;
    fh.FormatVersionMinor = SMF_FORMAT_VERSION_MINOR;
    fh.StringTableOffset = append(stringTable);
    fh.StringTableSize = static_cast<uint32_t>(stringTable.size());
    fh.GeometryOffset = append(geometryBlob);
    fh.GeometrySize = static_cast<uint32_t>(geometryBlob.size());
    fh.BspOffset = 0;
    fh.BspSize = 0;
    fh.EntityOffset = append(entityBlob);
    fh.EntitySize = static_cast<uint32_t>(entityBlob.size());
    fh.SectorOffset = append(sectorBlob);
    fh.SectorSize = static_cast<uint32_t>(sectorBlob.size());
    fh.PhysicsOffset = append(physicsBlob);
    fh.PhysicsSize = static_cast<uint32_t>(physicsBlob.size());
    fh.ScriptOffset = append(scriptBlob);
    fh.ScriptSize = static_cast<uint32_t>(scriptBlob.size());
    fh.TriggerOffset = append(triggerBlob);
    fh.TriggerSize = static_cast<uint32_t>(triggerBlob.size());
    fh.PathTableOffset = append(pathBlob);
    fh.PathTableSize = static_cast<uint32_t>(pathBlob.size());
    fh.Flags = compressTail ? 1u : 0u;

    std::memcpy(file.data(), &fh, sizeof(fh));
    if (compressTail) {
        const size_t hdrSize = sizeof(SmfFileHeader);
        const size_t tailSize = file.size() - hdrSize;
        size_t maxDst = ZSTD_compressBound(tailSize);
        std::vector<std::byte> packed(hdrSize + maxDst);
        std::memcpy(packed.data(), file.data(), hdrSize);
        size_t z = ZSTD_compress(packed.data() + hdrSize, maxDst, file.data() + hdrSize, tailSize, 3);
        if (ZSTD_isError(z)) {
            if (err) {
                *err = SmfError::OutOfMemory;
            }
            return false;
        }
        packed.resize(hdrSize + z);
        out = std::move(packed);
    } else {
        out = std::move(file);
    }
    return true;
}

bool LoadSmfFromBytes(SmfMap& map, std::span<const std::byte> data, SmfFileHeader* outHeader, SmfError* err) {
    const std::byte* base = data.data();
    size_t len = data.size();

    if (len < sizeof(SmfFileHeader)) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }

    SmfFileHeader fh{};
    std::memcpy(&fh, base, sizeof(fh));
    if (fh.Magic != SMF_MAGIC) {
        if (err) {
            *err = SmfError::InvalidMagic;
        }
        return false;
    }
    if (fh.FormatVersionMajor != SMF_FORMAT_VERSION_MAJOR) {
        if (err) {
            *err = SmfError::UnsupportedVersion;
        }
        return false;
    }
    if (fh.FormatVersionMinor > SMF_FORMAT_VERSION_MINOR) {
        if (err) {
            *err = SmfError::UnsupportedVersion;
        }
        return false;
    }

    std::vector<std::byte> owned;
    if (fh.Flags & 1u) {
        const size_t hdrSize = sizeof(SmfFileHeader);
        if (len <= hdrSize) {
            if (err) {
                *err = SmfError::CorruptHeader;
            }
            return false;
        }
        const std::byte* comp = base + hdrSize;
        const size_t compLen = len - hdrSize;
        unsigned long long dec = ZSTD_getFrameContentSize(comp, compLen);
        if (dec == ZSTD_CONTENTSIZE_ERROR || dec == ZSTD_CONTENTSIZE_UNKNOWN) {
            if (err) {
                *err = SmfError::CorruptHeader;
            }
            return false;
        }
        std::vector<std::byte> tail(static_cast<size_t>(dec));
        size_t r = ZSTD_decompress(tail.data(), tail.size(), comp, compLen);
        if (ZSTD_isError(r) || r != tail.size()) {
            if (err) {
                *err = SmfError::CorruptHeader;
            }
            return false;
        }
        owned.resize(hdrSize + tail.size());
        std::memcpy(owned.data(), base, hdrSize);
        std::memcpy(owned.data() + hdrSize, tail.data(), tail.size());
        {
            SmfFileHeader* patch = reinterpret_cast<SmfFileHeader*>(owned.data());
            patch->Flags &= ~1u;
        }
        base = owned.data();
        len = owned.size();
        std::memcpy(&fh, base, sizeof(fh));
    }

    if (outHeader) {
        *outHeader = fh;
    }

    auto inRange = [&](uint32_t off, uint32_t sz) -> bool {
        return static_cast<uint64_t>(off) + static_cast<uint64_t>(sz) <= len;
    };

    if (!inRange(fh.StringTableOffset, fh.StringTableSize)) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }

    const char* strBase = reinterpret_cast<const char*>(base + fh.StringTableOffset);
    auto strAt = [&](uint32_t o) -> std::string {
        if (o >= fh.StringTableSize) {
            return {};
        }
        const char* p = strBase + o;
        const char* q = p;
        size_t max = fh.StringTableSize - o;
        size_t n = 0;
        while (n < max && *q) {
            ++q;
            ++n;
        }
        return std::string(p, q);
    };

    map.Clear();

    if (fh.BspSize > 0 && !inRange(fh.BspOffset, fh.BspSize)) {
        if (err) {
            *err = SmfError::CorruptSection;
        }
        return false;
    }

    if (!inRange(fh.EntityOffset, fh.EntitySize) || !inRange(fh.GeometryOffset, fh.GeometrySize) ||
        !inRange(fh.SectorOffset, fh.SectorSize) || !inRange(fh.PhysicsOffset, fh.PhysicsSize) ||
        !inRange(fh.ScriptOffset, fh.ScriptSize) || !inRange(fh.TriggerOffset, fh.TriggerSize)) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }
    if (fh.PathTableSize > 0 && !inRange(fh.PathTableOffset, fh.PathTableSize)) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }

    const std::byte* pEnt = base + fh.EntityOffset;
    const std::byte* endEnt = pEnt + fh.EntitySize;
    if (static_cast<size_t>(endEnt - pEnt) < 4) {
        if (err) {
            *err = SmfError::CorruptEntitySection;
        }
        return false;
    }
    uint32_t entityCount = Wire::ReadU32(pEnt);
    pEnt += 4;
    map.Entities.reserve(entityCount);
    for (uint32_t ei = 0; ei < entityCount; ++ei) {
        if (static_cast<size_t>(endEnt - pEnt) < 12) {
            if (err) {
                *err = SmfError::CorruptEntitySection;
            }
            return false;
        }
        uint32_t nameOff = Wire::ReadU32(pEnt);
        pEnt += 4;
        uint32_t classOff = Wire::ReadU32(pEnt);
        pEnt += 4;
        uint32_t propCount = Wire::ReadU32(pEnt);
        pEnt += 4;
        SmfEntity ent;
        ent.Name = strAt(nameOff);
        ent.ClassName = strAt(classOff);
        for (uint32_t pi = 0; pi < propCount; ++pi) {
            if (static_cast<size_t>(endEnt - pEnt) < 8) {
                if (err) {
                    *err = SmfError::CorruptEntitySection;
                }
                return false;
            }
            uint32_t keyOff = Wire::ReadU32(pEnt);
            pEnt += 4;
            auto typeByte = static_cast<uint8_t>(*pEnt);
            pEnt += 4;
            auto at = static_cast<SmfAttributeType>(typeByte);
            SmfValue val;
            if (!Wire::ReadAttributeValue(pEnt, endEnt, at, val)) {
                if (err) {
                    *err = SmfError::CorruptEntitySection;
                }
                return false;
            }
            ent.Properties.push_back(SmfProperty{strAt(keyOff), at, std::move(val)});
        }
        map.Entities.push_back(std::move(ent));
    }

    if (fh.PathTableOffset != 0 && fh.PathTableSize >= 4) {
        const std::byte* pp = base + fh.PathTableOffset;
        const std::byte* endPt = pp + fh.PathTableSize;
        uint32_t ptCount = Wire::ReadU32(pp);
        pp += 4;
        for (uint32_t i = 0; i < ptCount && pp + sizeof(SmfPathTableEntryDisk) <= endPt; ++i) {
            SmfPathTableEntryDisk pe{};
            std::memcpy(&pe, pp, sizeof(pe));
            pp += sizeof(pe);
            map.PathTable.push_back({strAt(pe.PathOffset), pe.AssetHash});
        }
    }

    if (err) {
        *err = SmfError::None;
    }
    return true;
}

bool SaveSmfToFile(const std::filesystem::path& path, const SmfMap& map, SmfError* err, bool compressTail) {
    std::vector<std::byte> bytes;
    if (!SaveSmfToBytes(map, bytes, err, compressTail)) {
        return false;
    }
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        if (err) {
            *err = SmfError::IoOpenFailed;
        }
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!f) {
        if (err) {
            *err = SmfError::IoWriteFailed;
        }
        return false;
    }
    return true;
}

bool LoadSmfFromFile(const std::filesystem::path& path, SmfMap& map, SmfFileHeader* outHeader, SmfError* err) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        if (err) {
            *err = SmfError::IoOpenFailed;
        }
        return false;
    }
    const auto sz = f.tellg();
    if (sz < 0) {
        if (err) {
            *err = SmfError::IoReadFailed;
        }
        return false;
    }
    f.seekg(0);
    std::vector<std::byte> buf(static_cast<size_t>(sz));
    if (!buf.empty()) {
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (f.gcount() != static_cast<std::streamsize>(buf.size()) || f.fail()) {
            if (err) {
                *err = SmfError::IoReadFailed;
            }
            return false;
        }
    }
    return LoadSmfFromBytes(map, buf, outHeader, err);
}

} // namespace Solstice::Smf
