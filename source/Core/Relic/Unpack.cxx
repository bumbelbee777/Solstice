#include "Unpack.hxx"
#include "Decompress.hxx"
#include "PathTable.hxx"
#include "Reader.hxx"
#include <fstream>
#include <unordered_map>

namespace Solstice::Core::Relic {

namespace {

std::string SanitizeRelPath(std::string_view p) {
    std::string out;
    out.reserve(p.size());
    for (char c : p) {
        if (c == '\\' || c == '/') {
            out.push_back('_');
        } else if (c == '.' && (out.empty() || out == ".")) {
            out.push_back('_');
        } else {
            out.push_back(c);
        }
    }
    while (!out.empty() && (out.front() == '.' || out.front() == '/')) {
        out.erase(out.begin());
    }
    if (out.empty()) {
        out = "asset";
    }
    return out;
}

} // namespace

bool UnpackRelic(const std::filesystem::path& relicPath, const std::filesystem::path& outDir,
    bool preferPathTableNames) {
    auto container = OpenRelic(relicPath);
    if (!container) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    if (ec) {
        return false;
    }

    std::unordered_map<AssetHash, std::string> hashToPath;
    if (preferPathTableNames && !container->PathTableBlob.empty()) {
        std::vector<std::pair<std::string, AssetHash>> rows;
        if (ParsePathTableBlob(std::span<const std::byte>(container->PathTableBlob.data(),
                    container->PathTableBlob.size()),
                rows)) {
            for (const auto& [p, h] : rows) {
                hashToPath[h] = SanitizeRelPath(p);
            }
        }
    }

    std::ifstream f(relicPath, std::ios::binary);
    if (!f) {
        return false;
    }

    const uint64_t dataBase = container->Header.DataBlobOffset;

    for (const auto& entry : container->Manifest) {
        f.seekg(static_cast<std::streamoff>(dataBase + entry.DataOffset));
        std::vector<std::byte> raw(entry.CompressedSize);
        if (entry.CompressedSize > 0) {
            f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(entry.CompressedSize));
            if (!f) {
                return false;
            }
        }
        std::vector<std::byte> data = DecompressAsset(raw, GetCompressionType(entry.Flags), entry.UncompressedSize);
        if (entry.UncompressedSize > 0 && data.size() != entry.UncompressedSize) {
            return false;
        }

        std::filesystem::path outFile = outDir;
        auto it = hashToPath.find(entry.AssetHash);
        if (it != hashToPath.end()) {
            outFile /= it->second;
        } else {
            char name[32];
            std::snprintf(name, sizeof(name), "%016llX.bin", static_cast<unsigned long long>(entry.AssetHash));
            outFile /= name;
        }
        if (outFile.has_parent_path()) {
            std::filesystem::create_directories(outFile.parent_path(), ec);
        }
        std::ofstream wf(outFile, std::ios::binary | std::ios::trunc);
        if (!wf) {
            return false;
        }
        if (!data.empty()) {
            wf.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
        }
        if (!wf) {
            return false;
        }
    }

    return true;
}

} // namespace Solstice::Core::Relic
