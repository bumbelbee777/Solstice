#pragma once

#include "IAssetResolver.hxx"
#include <filesystem>
#include <optional>
#include <unordered_map>

namespace Solstice::Parallax {

// In-memory session resolver for tools: import files/folders, hash → bytes.
class DevSessionAssetResolver final : public IAssetResolver {
public:
    void Register(uint64_t hash, AssetData data, std::string_view logicalPath = {});

    // Load bytes from disk, hash content (FNV-1a), register under logicalPath basename.
    // Returns the content hash on success.
    std::optional<uint64_t> ImportFile(const std::filesystem::path& path);

    bool Resolve(uint64_t assetHash, AssetData& outData) override;
    uint64_t HashFromPath(std::string_view path) override;
    bool IsLoaded(uint64_t assetHash) const override;

    const std::unordered_map<uint64_t, AssetData>& GetStore() const { return m_ByHash; }
    const std::unordered_map<std::string, uint64_t>& GetPathBindings() const { return m_PathToHash; }

private:
    std::unordered_map<uint64_t, AssetData> m_ByHash;
    std::unordered_map<std::string, uint64_t> m_PathToHash;
};

} // namespace Solstice::Parallax
