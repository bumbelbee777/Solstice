#pragma once

#include "Types.hxx"
#include "Solstice.hxx"
#include <filesystem>
#include <string>
#include <vector>

namespace Solstice::Core::Relic {

struct RelicWriteInput {
    AssetHash Hash = 0;
    AssetTypeTag TypeTag = AssetTypeTag::Unknown;
    uint32_t ClusterId = 0;
    /// Extra manifest flags (e.g. streaming priority in upper byte); must not request delta payloads here.
    uint16_t ExtraFlags = FlagNone;
    CompressionType Compression = CompressionType::None;
    std::vector<std::byte> Uncompressed;
    std::vector<AssetHash> Dependencies;
    /// If non-empty, adds a path-table row (UTF-8 path → Hash).
    std::string LogicalPath;
};

struct RelicWriteOptions {
    ContainerType Container = ContainerType::Content;
    uint16_t TagSet = TagBase;
};

SOLSTICE_API bool WriteRelic(const std::filesystem::path& path, std::vector<RelicWriteInput> inputs,
    const RelicWriteOptions& options = {});

} // namespace Solstice::Core::Relic
