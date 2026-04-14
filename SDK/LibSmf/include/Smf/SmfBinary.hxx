#pragma once

#include "SmfMap.hxx"
#include "SmfTypes.hxx"

#include <filesystem>
#include <span>

namespace Solstice::Smf {

bool SaveSmfToBytes(const SmfMap& map, std::vector<std::byte>& out, SmfError* err = nullptr, bool compressTail = false);

/// If `outHeader` is non-null, fills it from the file (after validation).
bool LoadSmfFromBytes(SmfMap& map, std::span<const std::byte> data, SmfFileHeader* outHeader = nullptr,
    SmfError* err = nullptr);

bool SaveSmfToFile(const std::filesystem::path& path, const SmfMap& map, SmfError* err = nullptr,
    bool compressTail = false);

bool LoadSmfFromFile(const std::filesystem::path& path, SmfMap& map, SmfFileHeader* outHeader = nullptr,
    SmfError* err = nullptr);

} // namespace Solstice::Smf
