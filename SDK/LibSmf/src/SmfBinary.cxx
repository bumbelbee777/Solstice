#include <Smf/SmfBinary.hxx>
#include <Smf/SmfWire.hxx>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <zstd.h>

namespace {

constexpr uint32_t kSpatialMagic = 0x31435053u; // 'SPC1'
/// Spatial section stays at **1** with the rest of SMF v1 tooling; BSP node extras use **kBspNodeExtMagic** (optional tail).
constexpr uint32_t kSpatialVersion = 1u;
/// After a node's 28-byte core, if this tag is present the reader consumes UTF-8 paths + slab; otherwise the next bytes are the next node (legacy) or **hasOctree**.
constexpr uint32_t kBspNodeExtMagic = 0x42504558u; // 'BPEX'
/// Optional per-node BSP texture alignment (shift/scale/rotate) after BPEX slab (see `SmfAuthoringBspNode`).
constexpr uint32_t kBspNodeTexXMagic = 0x31545842u; // 'BXT1'
/// Optional per-node **locked** flag for BSP texture xform (after **BXT1** for that node, if any).
constexpr uint32_t kBspNodeTexLockMagic = 0x31584C54u; // 'TXL1' (LE)
/// Optional path to a baked **lightmap** image (after the three world hook string offsets, before **FLD1** if present).
constexpr uint32_t kSmalBakedLightmapMagic = 0x314D4B42u; // 'BKM1' (LE)

constexpr uint32_t kGameplayMagic = 0x4C414D53u; // 'SMAL'
/// **SMAL is versioned as v1** with Solstice Map Format v1. Writers always emit **1**. Earlier TP builds
/// wrote legacy numeric tags **2–4** (same payload layout as today); readers still load those files.
constexpr uint32_t kGameplayVersion = 1u;
constexpr uint32_t kGameplayLegacy2 = 2u;
constexpr uint32_t kGameplayLegacy3 = 3u;
constexpr uint32_t kGameplayLegacy4 = 4u;

/// Optional **SMAL v1** tail: authored **`SmfFluidVolume`** list for **`NSSolver`** (magic `FLD1`).
constexpr uint32_t kSmalFluidMagic = 0x31444446u; // 'F'|'L'<<8|'D'<<16|'1'<<24
constexpr uint32_t kSmalFluidChunkVersion = 1u;

constexpr size_t kSmfHeaderV12Bytes = offsetof(Solstice::Smf::SmfFileHeader, ExtrasOffset);
static_assert(kSmfHeaderV12Bytes == 88u, "SMF v1.2 header ends before ExtrasOffset");

void AppendI32(std::vector<std::byte>& b, int32_t v) {
    b.insert(b.end(), 4, std::byte{0});
    std::memcpy(b.data() + b.size() - 4, &v, 4);
}

int32_t ReadI32(const std::byte*& p, const std::byte* end) {
    if (p + 4 > end) {
        return 0;
    }
    int32_t v;
    std::memcpy(&v, p, 4);
    p += 4;
    return v;
}

void AppendF32(std::vector<std::byte>& b, float f) {
    b.insert(b.end(), 4, std::byte{0});
    std::memcpy(b.data() + b.size() - 4, &f, 4);
}

void AppendVec3(std::vector<std::byte>& b, const Solstice::Smf::SmfVec3& v) {
    AppendF32(b, v.x);
    AppendF32(b, v.y);
    AppendF32(b, v.z);
}

void AppendUtf8String(std::vector<std::byte>& b, const std::string& s) {
    using Solstice::Smf::Wire::AppendU32;
    AppendU32(b, static_cast<uint32_t>(s.size()));
    for (unsigned char ch : s) {
        b.push_back(static_cast<std::byte>(ch));
    }
}

bool ReadUtf8String(const std::byte*& p, const std::byte* end, std::string& out) {
    using Solstice::Smf::Wire::ReadU32;
    if (static_cast<size_t>(end - p) < 4) {
        return false;
    }
    const uint32_t len = ReadU32(p);
    p += 4;
    if (static_cast<size_t>(end - p) < len) {
        return false;
    }
    out.assign(reinterpret_cast<const char*>(p), len);
    p += len;
    return true;
}

std::vector<std::byte> BuildSpatialBlob(const Solstice::Smf::SmfMap& map) {
    using Solstice::Smf::Wire::AppendU32;
    const bool hasBsp = map.Bsp.has_value();
    const bool hasOct = map.Octree.has_value();
    if (!hasBsp && !hasOct) {
        return {};
    }
    std::vector<std::byte> blob;
    AppendU32(blob, kSpatialMagic);
    AppendU32(blob, kSpatialVersion);
    AppendU32(blob, hasBsp ? 1u : 0u);
    if (hasBsp) {
        const auto& bsp = *map.Bsp;
        AppendU32(blob, bsp.RootIndex);
        AppendU32(blob, static_cast<uint32_t>(bsp.Nodes.size()));
        for (const auto& n : bsp.Nodes) {
            AppendVec3(blob, n.PlaneNormal);
            AppendF32(blob, n.PlaneD);
            AppendU32(blob, static_cast<uint32_t>(n.FrontChild));
            AppendU32(blob, static_cast<uint32_t>(n.BackChild));
            AppendU32(blob, n.LeafId);
            AppendU32(blob, kBspNodeExtMagic);
            AppendUtf8String(blob, n.FrontTexturePath);
            AppendUtf8String(blob, n.BackTexturePath);
            AppendU32(blob, n.SlabValid ? 1u : 0u);
            AppendVec3(blob, n.SlabMin);
            AppendVec3(blob, n.SlabMax);
            const uint32_t txf = (n.HasFrontTextureXform ? 1u : 0u) | (n.HasBackTextureXform ? 2u : 0u);
            if (txf != 0u) {
                AppendU32(blob, kBspNodeTexXMagic);
                AppendU32(blob, txf);
                if (n.HasFrontTextureXform) {
                    AppendF32(blob, n.FrontTextureXform.ShiftU);
                    AppendF32(blob, n.FrontTextureXform.ShiftV);
                    AppendF32(blob, n.FrontTextureXform.ScaleU);
                    AppendF32(blob, n.FrontTextureXform.ScaleV);
                    AppendF32(blob, n.FrontTextureXform.RotateDeg);
                }
                if (n.HasBackTextureXform) {
                    AppendF32(blob, n.BackTextureXform.ShiftU);
                    AppendF32(blob, n.BackTextureXform.ShiftV);
                    AppendF32(blob, n.BackTextureXform.ScaleU);
                    AppendF32(blob, n.BackTextureXform.ScaleV);
                    AppendF32(blob, n.BackTextureXform.RotateDeg);
                }
            }
            if (n.FrontTextureXformLocked || n.BackTextureXformLocked) {
                AppendU32(blob, kBspNodeTexLockMagic);
                const uint32_t lk = (n.FrontTextureXformLocked ? 1u : 0u) | (n.BackTextureXformLocked ? 2u : 0u);
                AppendU32(blob, lk);
            }
        }
    }
    AppendU32(blob, hasOct ? 1u : 0u);
    if (hasOct) {
        const auto& oc = *map.Octree;
        AppendU32(blob, oc.RootIndex);
        AppendU32(blob, static_cast<uint32_t>(oc.Nodes.size()));
        for (const auto& n : oc.Nodes) {
            AppendVec3(blob, n.Min);
            AppendVec3(blob, n.Max);
            for (int i = 0; i < 8; ++i) {
                AppendU32(blob, static_cast<uint32_t>(n.Children[static_cast<size_t>(i)]));
            }
            AppendU32(blob, n.LeafId);
        }
    }
    return blob;
}

float ReadF32(const std::byte*& p, const std::byte* end) {
    if (p + 4 > end) {
        return 0.f;
    }
    float f;
    std::memcpy(&f, p, 4);
    p += 4;
    return f;
}

void ReadVec3(const std::byte*& p, const std::byte* end, Solstice::Smf::SmfVec3& v) {
    v.x = ReadF32(p, end);
    v.y = ReadF32(p, end);
    v.z = ReadF32(p, end);
}

bool ParseSpatialBlob(std::span<const std::byte> data, Solstice::Smf::SmfMap& map) {
    using Solstice::Smf::Wire::ReadU32;
    const std::byte* p = data.data();
    const std::byte* end = data.data() + data.size();
    auto readU32 = [&]() -> uint32_t {
        if (p + 4 > end) {
            return 0;
        }
        uint32_t v = ReadU32(p);
        p += 4;
        return v;
    };
    if (static_cast<size_t>(end - p) < 12) {
        return false;
    }
    if (readU32() != kSpatialMagic) {
        return false;
    }
    const uint32_t ver = readU32();
    if (ver != 1u) {
        return false;
    }
    const uint32_t hasBsp = readU32();
    if (hasBsp > 1u) {
        return false;
    }
    if (hasBsp != 0u) {
        Solstice::Smf::SmfAuthoringBsp bsp;
        bsp.RootIndex = readU32();
        const uint32_t nNodes = readU32();
        bsp.Nodes.resize(nNodes);
        for (uint32_t i = 0; i < nNodes; ++i) {
            if (static_cast<size_t>(end - p) < 28) {
                return false;
            }
            auto& nd = bsp.Nodes[i];
            ReadVec3(p, end, nd.PlaneNormal);
            nd.PlaneD = ReadF32(p, end);
            nd.FrontChild = static_cast<int32_t>(readU32());
            nd.BackChild = static_cast<int32_t>(readU32());
            nd.LeafId = readU32();
            nd.FrontTexturePath.clear();
            nd.BackTexturePath.clear();
            nd.SlabValid = false;
            if (static_cast<size_t>(end - p) >= 4) {
                const uint32_t extTag = Solstice::Smf::Wire::ReadU32(p);
                if (extTag == kBspNodeExtMagic) {
                    p += 4;
                    if (!ReadUtf8String(p, end, nd.FrontTexturePath) || !ReadUtf8String(p, end, nd.BackTexturePath)) {
                        return false;
                    }
                    if (static_cast<size_t>(end - p) < 4u + 12u + 12u) {
                        return false;
                    }
                    nd.SlabValid = readU32() != 0u;
                    ReadVec3(p, end, nd.SlabMin);
                    ReadVec3(p, end, nd.SlabMax);
                    nd.HasFrontTextureXform = false;
                    nd.HasBackTextureXform = false;
                    nd.FrontTextureXformLocked = false;
                    nd.BackTextureXformLocked = false;
                    if (static_cast<size_t>(end - p) >= 4) {
                        const uint32_t maybeTx = Solstice::Smf::Wire::ReadU32(p);
                        if (maybeTx == kBspNodeTexXMagic) {
                            p += 4;
                            if (static_cast<size_t>(end - p) < 4) {
                                return false;
                            }
                            const uint32_t txf = readU32();
                            if (txf & 1u) {
                                if (static_cast<size_t>(end - p) < 20) {
                                    return false;
                                }
                                nd.HasFrontTextureXform = true;
                                nd.FrontTextureXform.ShiftU = ReadF32(p, end);
                                nd.FrontTextureXform.ShiftV = ReadF32(p, end);
                                nd.FrontTextureXform.ScaleU = ReadF32(p, end);
                                nd.FrontTextureXform.ScaleV = ReadF32(p, end);
                                nd.FrontTextureXform.RotateDeg = ReadF32(p, end);
                            }
                            if (txf & 2u) {
                                if (static_cast<size_t>(end - p) < 20) {
                                    return false;
                                }
                                nd.HasBackTextureXform = true;
                                nd.BackTextureXform.ShiftU = ReadF32(p, end);
                                nd.BackTextureXform.ShiftV = ReadF32(p, end);
                                nd.BackTextureXform.ScaleU = ReadF32(p, end);
                                nd.BackTextureXform.ScaleV = ReadF32(p, end);
                                nd.BackTextureXform.RotateDeg = ReadF32(p, end);
                            }
                        }
                    }
                    if (static_cast<size_t>(end - p) >= 8) {
                        uint32_t tagLk = 0;
                        std::memcpy(&tagLk, p, 4);
                        if (tagLk == kBspNodeTexLockMagic) {
                            p += 4;
                            if (static_cast<size_t>(end - p) < 4) {
                                return false;
                            }
                            const uint32_t lk = readU32();
                            nd.FrontTextureXformLocked = (lk & 1u) != 0;
                            nd.BackTextureXformLocked = (lk & 2u) != 0;
                        }
                    }
                }
            }
        }
        map.Bsp = std::move(bsp);
    }
    if (static_cast<size_t>(end - p) < 4) {
        return true;
    }
    const uint32_t hasOct = readU32();
    if (hasOct > 1u) {
        return false;
    }
    if (hasOct != 0u) {
        Solstice::Smf::SmfAuthoringOctree oc;
        oc.RootIndex = readU32();
        const uint32_t nNodes = readU32();
        oc.Nodes.resize(nNodes);
        for (uint32_t i = 0; i < nNodes; ++i) {
            if (static_cast<size_t>(end - p) < 4u * 6u + 8u * 4u + 4u) {
                return false;
            }
            auto& nd = oc.Nodes[i];
            ReadVec3(p, end, nd.Min);
            ReadVec3(p, end, nd.Max);
            for (int c = 0; c < 8; ++c) {
                nd.Children[static_cast<size_t>(c)] = static_cast<int32_t>(readU32());
            }
            nd.LeafId = readU32();
        }
        map.Octree = std::move(oc);
    }
    return true;
}

template <typename InternFn>
std::vector<std::byte> BuildGameplayExtrasBlob(const Solstice::Smf::SmfMap& map, InternFn&& intern) {
    using Solstice::Smf::Wire::AppendU32;
    std::vector<std::byte> blob;
    AppendU32(blob, kGameplayMagic);
    AppendU32(blob, kGameplayVersion);
    AppendU32(blob, static_cast<uint32_t>(map.AcousticZones.size()));
    for (const auto& z : map.AcousticZones) {
        AppendU32(blob, intern(z.Name));
        AppendVec3(blob, z.Center);
        AppendVec3(blob, z.Extents);
        AppendU32(blob, intern(z.ReverbPreset));
        AppendF32(blob, z.Wetness);
        AppendF32(blob, z.ObstructionMultiplier);
        AppendI32(blob, z.Priority);
        const uint32_t fl = (z.Enabled ? 1u : 0u) | (z.IsSpherical ? 2u : 0u);
        AppendU32(blob, fl);
        AppendU32(blob, intern(z.MusicPath));
        AppendU32(blob, intern(z.AmbiencePath));
    }
    AppendU32(blob, static_cast<uint32_t>(map.AuthoringLights.size()));
    for (const auto& L : map.AuthoringLights) {
        AppendU32(blob, intern(L.Name));
        blob.push_back(static_cast<std::byte>(static_cast<uint8_t>(L.Type)));
        blob.push_back(std::byte{0});
        blob.push_back(std::byte{0});
        blob.push_back(std::byte{0});
        AppendVec3(blob, L.Position);
        AppendVec3(blob, L.Direction);
        AppendVec3(blob, L.Color);
        AppendF32(blob, L.Intensity);
        AppendF32(blob, L.Hue);
        AppendF32(blob, L.Attenuation);
        AppendF32(blob, L.Range);
        AppendF32(blob, L.SpotInnerDeg);
        AppendF32(blob, L.SpotOuterDeg);
    }
    AppendU32(blob, map.Skybox.has_value() ? 1u : 0u);
    if (map.Skybox.has_value()) {
        const auto& s = *map.Skybox;
        AppendU32(blob, s.Enabled ? 1u : 0u);
        AppendF32(blob, s.Brightness);
        AppendF32(blob, s.YawDegrees);
        for (int k = 0; k < 6; ++k) {
            AppendU32(blob, intern(s.FacePaths[static_cast<size_t>(k)]));
        }
    }
    AppendU32(blob, intern(map.WorldAuthoringHooks.ScriptPath));
    AppendU32(blob, intern(map.WorldAuthoringHooks.CutscenePath));
    AppendU32(blob, intern(map.WorldAuthoringHooks.WorldSpaceUiPath));
    if (!map.BakedLightmapPath.empty()) {
        AppendU32(blob, kSmalBakedLightmapMagic);
        AppendU32(blob, intern(map.BakedLightmapPath));
    }
    if (!map.FluidVolumes.empty()) {
        AppendU32(blob, kSmalFluidMagic);
        AppendU32(blob, kSmalFluidChunkVersion);
        AppendU32(blob, static_cast<uint32_t>(map.FluidVolumes.size()));
        for (const auto& f : map.FluidVolumes) {
            const uint32_t fl = (f.Enabled ? 1u : 0u) | (f.EnableMacCormack ? 2u : 0u) | (f.EnableBoussinesq ? 4u : 0u)
                | (f.VolumeVisualizationClip ? 8u : 0u);
            AppendU32(blob, intern(f.Name));
            AppendU32(blob, fl);
            AppendVec3(blob, f.BoundsMin);
            AppendVec3(blob, f.BoundsMax);
            AppendI32(blob, f.ResolutionX);
            AppendI32(blob, f.ResolutionY);
            AppendI32(blob, f.ResolutionZ);
            AppendF32(blob, f.Diffusion);
            AppendF32(blob, f.Viscosity);
            AppendF32(blob, f.ReferenceDensity);
            AppendI32(blob, f.PressureRelaxationIterations);
            AppendF32(blob, f.BuoyancyStrength);
            AppendF32(blob, f.Prandtl);
        }
    }
    return blob;
}

template <typename StrAtFn>
bool ParseGameplayExtrasBlob(std::span<const std::byte> data, Solstice::Smf::SmfMap& map, StrAtFn&& strAt) {
    using Solstice::Smf::Wire::ReadU32;
    const std::byte* p = data.data();
    const std::byte* end = data.data() + data.size();
    auto need = [&](size_t n) -> bool { return static_cast<size_t>(end - p) >= n; };
    if (!need(12)) {
        return false;
    }
    if (ReadU32(p) != kGameplayMagic) {
        return false;
    }
    p += 4;
    const uint32_t ver = ReadU32(p);
    p += 4;
    if (ver != kGameplayVersion && ver != kGameplayLegacy2 && ver != kGameplayLegacy3 && ver != kGameplayLegacy4) {
        return false;
    }
    if (!need(4)) {
        return false;
    }
    const uint32_t nAz = ReadU32(p);
    p += 4;
    size_t perZoneTail = 8u;
    if (ver == kGameplayVersion) {
        // Canonical **SMAL v1** zone rows include MusicPath/AmbiencePath (8-byte tail). A few early blobs used the
        // same numeric **1** with 48-byte zone rows and ended exactly after lights — detect by total span.
        if (nAz == 0u) {
            perZoneTail = 8u;
        } else if (static_cast<size_t>(end - p) < 48u * static_cast<size_t>(nAz) + 4u) {
            return false;
        } else {
            const std::byte* candLtPtr = p + 48u * static_cast<size_t>(nAz);
            uint32_t nLtGuess = 0;
            std::memcpy(&nLtGuess, candLtPtr, 4);
            const auto* legacyEnd = candLtPtr + 4ull + 68ull * static_cast<uint64_t>(nLtGuess);
            if (legacyEnd <= end && legacyEnd == end) {
                perZoneTail = 0u;
            }
        }
    }
    map.AcousticZones.clear();
    map.AcousticZones.reserve(nAz);
    for (uint32_t i = 0; i < nAz; ++i) {
        if (!need(48u + perZoneTail)) {
            return false;
        }
        const uint32_t nameOff = ReadU32(p);
        p += 4;
        Solstice::Smf::SmfAcousticZone z;
        z.Name = strAt(nameOff);
        ReadVec3(p, end, z.Center);
        ReadVec3(p, end, z.Extents);
        const uint32_t presetOff = ReadU32(p);
        p += 4;
        z.ReverbPreset = strAt(presetOff);
        z.Wetness = ReadF32(p, end);
        z.ObstructionMultiplier = ReadF32(p, end);
        z.Priority = ReadI32(p, end);
        if (!need(4)) {
            return false;
        }
        const uint32_t fl = ReadU32(p);
        p += 4;
        z.Enabled = (fl & 1u) != 0;
        z.IsSpherical = (fl & 2u) != 0;
        if (perZoneTail >= 8u) {
            const uint32_t musicOff = ReadU32(p);
            p += 4;
            const uint32_t ambOff = ReadU32(p);
            p += 4;
            z.MusicPath = strAt(musicOff);
            z.AmbiencePath = strAt(ambOff);
        }
        map.AcousticZones.push_back(std::move(z));
    }
    if (!need(4)) {
        return false;
    }
    const uint32_t nLt = ReadU32(p);
    p += 4;
    map.AuthoringLights.clear();
    map.AuthoringLights.reserve(nLt);
    for (uint32_t i = 0; i < nLt; ++i) {
        if (!need(68)) {
            return false;
        }
        const uint32_t nameOff = ReadU32(p);
        p += 4;
        Solstice::Smf::SmfAuthoringLight L{};
        L.Name = strAt(nameOff);
        const auto ty = static_cast<uint8_t>(*p);
        p += 4;
        if (ty > 2u) {
            return false;
        }
        L.Type = static_cast<Solstice::Smf::SmfAuthoringLightType>(ty);
        ReadVec3(p, end, L.Position);
        ReadVec3(p, end, L.Direction);
        ReadVec3(p, end, L.Color);
        L.Intensity = ReadF32(p, end);
        L.Hue = ReadF32(p, end);
        L.Attenuation = ReadF32(p, end);
        L.Range = ReadF32(p, end);
        L.SpotInnerDeg = ReadF32(p, end);
        L.SpotOuterDeg = ReadF32(p, end);
        map.AuthoringLights.push_back(std::move(L));
    }
    map.Skybox.reset();
    map.WorldAuthoringHooks = {};
    if (p == end) {
        return true;
    }
    if (!need(4)) {
        return false;
    }
    const uint32_t hasSky = ReadU32(p);
    p += 4;
    if (hasSky > 1u) {
        return false;
    }
    if (hasSky == 1u) {
        if (!need(4u + 4u + 4u + 6u * 4u)) {
            return false;
        }
        Solstice::Smf::SmfSkybox sky{};
        const uint32_t sfl = ReadU32(p);
        p += 4;
        sky.Enabled = (sfl & 1u) != 0;
        sky.Brightness = ReadF32(p, end);
        sky.YawDegrees = ReadF32(p, end);
        for (int k = 0; k < 6; ++k) {
            if (!need(4)) {
                return false;
            }
            const uint32_t pathOff = ReadU32(p);
            p += 4;
            sky.FacePaths[static_cast<size_t>(k)] = strAt(pathOff);
        }
        map.Skybox = std::move(sky);
    }
    if (p == end) {
        return true;
    }
    if (!need(12)) {
        return false;
    }
    const uint32_t scriptOff = ReadU32(p);
    p += 4;
    const uint32_t cutsceneOff = ReadU32(p);
    p += 4;
    const uint32_t worldUiOff = ReadU32(p);
    p += 4;
    map.WorldAuthoringHooks.ScriptPath = strAt(scriptOff);
    map.WorldAuthoringHooks.CutscenePath = strAt(cutsceneOff);
    map.WorldAuthoringHooks.WorldSpaceUiPath = strAt(worldUiOff);
    map.BakedLightmapPath.clear();
    if (p < end && static_cast<size_t>(end - p) >= 8) {
        uint32_t bkm = 0;
        std::memcpy(&bkm, p, 4);
        if (bkm == kSmalBakedLightmapMagic) {
            p += 4;
            if (!need(4)) {
                return false;
            }
            const uint32_t bkmPathOff = ReadU32(p);
            p += 4;
            map.BakedLightmapPath = strAt(bkmPathOff);
        }
    }
    if (p == end) {
        return true;
    }
    if (!need(8)) {
        return false;
    }
    if (ReadU32(p) != kSmalFluidMagic) {
        return false;
    }
    p += 4;
    const uint32_t fluidChunkVer = ReadU32(p);
    p += 4;
    if (fluidChunkVer != kSmalFluidChunkVersion) {
        return false;
    }
    if (!need(4)) {
        return false;
    }
    const uint32_t nFluid = ReadU32(p);
    p += 4;
    map.FluidVolumes.clear();
    map.FluidVolumes.reserve(nFluid);
    for (uint32_t fi = 0; fi < nFluid; ++fi) {
        if (!need(4u + 4u + 12u + 12u + 12u + 12u + 4u + 8u)) {
            return false;
        }
        const uint32_t nameOff = ReadU32(p);
        p += 4;
        const uint32_t fl = ReadU32(p);
        p += 4;
        Solstice::Smf::SmfFluidVolume fv{};
        fv.Name = strAt(nameOff);
        fv.Enabled = (fl & 1u) != 0;
        fv.EnableMacCormack = (fl & 2u) != 0;
        fv.EnableBoussinesq = (fl & 4u) != 0;
        fv.VolumeVisualizationClip = (fl & 8u) != 0;
        ReadVec3(p, end, fv.BoundsMin);
        ReadVec3(p, end, fv.BoundsMax);
        fv.ResolutionX = ReadI32(p, end);
        fv.ResolutionY = ReadI32(p, end);
        fv.ResolutionZ = ReadI32(p, end);
        fv.Diffusion = ReadF32(p, end);
        fv.Viscosity = ReadF32(p, end);
        fv.ReferenceDensity = ReadF32(p, end);
        fv.PressureRelaxationIterations = ReadI32(p, end);
        fv.BuoyancyStrength = ReadF32(p, end);
        fv.Prandtl = ReadF32(p, end);
        map.FluidVolumes.push_back(std::move(fv));
    }
    return p == end;
}

bool SmfFileUsesExtendedHeaderOnDisk(const Solstice::Smf::SmfFileHeader& fh) {
    // v1.2-style minors 1–2: 88-byte on-disk header only. Current 1.0 writes minor 0 with 96-byte header.
    // Older dev milestones used minor >= 3 for the same extended layout.
    return fh.FormatVersionMinor == 0 || fh.FormatVersionMinor >= 3;
}

bool ReadSmfFileHeaderPrefix(const std::byte* base, size_t len, Solstice::Smf::SmfFileHeader& fh) {
    if (len < kSmfHeaderV12Bytes) {
        return false;
    }
    fh = {};
    std::memcpy(&fh, base, kSmfHeaderV12Bytes);
    if (len >= sizeof(Solstice::Smf::SmfFileHeader) && SmfFileUsesExtendedHeaderOnDisk(fh)) {
        std::memcpy(reinterpret_cast<std::byte*>(&fh) + kSmfHeaderV12Bytes, base + kSmfHeaderV12Bytes,
            sizeof(Solstice::Smf::SmfFileHeader) - kSmfHeaderV12Bytes);
    }
    return true;
}

size_t SmfHeaderBytesOnDisk(const Solstice::Smf::SmfFileHeader& fh) {
    return SmfFileUsesExtendedHeaderOnDisk(fh) ? sizeof(Solstice::Smf::SmfFileHeader) : kSmfHeaderV12Bytes;
}

} // namespace

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
    for (const auto& z : map.AcousticZones) {
        intern(z.Name);
        intern(z.ReverbPreset);
        intern(z.MusicPath);
        intern(z.AmbiencePath);
    }
    for (const auto& L : map.AuthoringLights) {
        intern(L.Name);
    }
    if (map.Skybox.has_value()) {
        for (const auto& fp : map.Skybox->FacePaths) {
            intern(fp);
        }
    }
    intern(map.WorldAuthoringHooks.ScriptPath);
    intern(map.WorldAuthoringHooks.CutscenePath);
    intern(map.WorldAuthoringHooks.WorldSpaceUiPath);
    intern(map.BakedLightmapPath);
    for (const auto& f : map.FluidVolumes) {
        intern(f.Name);
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

    const std::vector<std::byte> spatialBlob = BuildSpatialBlob(map);

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
    if (!spatialBlob.empty()) {
        fh.BspOffset = append(spatialBlob);
        fh.BspSize = static_cast<uint32_t>(spatialBlob.size());
    } else {
        fh.BspOffset = 0;
        fh.BspSize = 0;
    }
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
    const std::vector<std::byte> gameplayBlob = BuildGameplayExtrasBlob(map, intern);
    fh.ExtrasOffset = append(gameplayBlob);
    fh.ExtrasSize = static_cast<uint32_t>(gameplayBlob.size());
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

    if (len < kSmfHeaderV12Bytes) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }

    SmfFileHeader fh{};
    if (!ReadSmfFileHeaderPrefix(base, len, fh)) {
        if (err) {
            *err = SmfError::CorruptHeader;
        }
        return false;
    }
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
    if (fh.FormatVersionMinor > SMF_MAX_SUPPORTED_LOAD_FORMAT_MINOR) {
        if (err) {
            *err = SmfError::UnsupportedVersion;
        }
        return false;
    }

    std::vector<std::byte> owned;
    if (fh.Flags & 1u) {
        const size_t hdrDisk = SmfHeaderBytesOnDisk(fh);
        if (len <= hdrDisk) {
            if (err) {
                *err = SmfError::CorruptHeader;
            }
            return false;
        }
        const std::byte* comp = base + hdrDisk;
        const size_t compLen = len - hdrDisk;
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
        owned.resize(hdrDisk + tail.size());
        std::memcpy(owned.data(), base, hdrDisk);
        std::memcpy(owned.data() + hdrDisk, tail.data(), tail.size());
        {
            SmfFileHeader* patch = reinterpret_cast<SmfFileHeader*>(owned.data());
            patch->Flags &= ~1u;
        }
        base = owned.data();
        len = owned.size();
        if (!ReadSmfFileHeaderPrefix(base, len, fh)) {
            if (err) {
                *err = SmfError::CorruptHeader;
            }
            return false;
        }
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
    if (fh.ExtrasSize > 0 && !inRange(fh.ExtrasOffset, fh.ExtrasSize)) {
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

    if (fh.ExtrasSize > 0) {
        const std::byte* ex = base + fh.ExtrasOffset;
        if (!ParseGameplayExtrasBlob(std::span<const std::byte>(ex, static_cast<size_t>(fh.ExtrasSize)), map, strAt)) {
            if (err) {
                *err = SmfError::CorruptSection;
            }
            return false;
        }
    }

    if (fh.BspSize > 0) {
        const std::byte* sp = base + fh.BspOffset;
        if (!ParseSpatialBlob(std::span<const std::byte>(sp, static_cast<size_t>(fh.BspSize)), map)) {
            if (err) {
                *err = SmfError::CorruptSection;
            }
            return false;
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
