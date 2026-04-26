#include "RelicOps.hxx"

#include <Core/Relic/Decompress.hxx>
#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace {

using Solstice::Core::Relic::AssetTypeTag;
using Solstice::Core::Relic::FlagIsDelta;
using Solstice::Core::Relic::GetCompressionType;
using Solstice::Core::Relic::GetDependencies;
using Solstice::Core::Relic::OpenRelic;
using Solstice::Core::Relic::ParsePathTableBlob;
using Solstice::Core::Relic::RelicManifestEntry;

} // namespace

namespace Refulgent::RelicOps {

uint64_t HashPathFNV1a(const std::string_view path) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : path) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t HashBytesFNV1a(const std::span<const std::byte> data) {
    uint64_t h = 14695981039346656037ull;
    for (std::byte b : data) {
        h ^= static_cast<uint8_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

std::string NormalizeLogicalPath(const std::string_view rel) {
    std::string out;
    out.reserve(rel.size());
    for (char c : rel) {
        out.push_back(c == '\\' ? '/' : c);
    }
    while (out.size() > 0 && (out[0] == '/')) {
        out.erase(out.begin());
    }
    if (out.rfind("./", 0) == 0) {
        out.erase(0, 2);
    }
    return out;
}

const char* CompressionName(const Solstice::Core::Relic::CompressionType c) {
    using CT = Solstice::Core::Relic::CompressionType;
    switch (c) {
    case CT::None:
        return "none";
    case CT::LZ4:
        return "lz4";
    default:
        return "zstd";
    }
}

const char* AssetTypeShortName(const Solstice::Core::Relic::AssetTypeTag t) {
    using AT = Solstice::Core::Relic::AssetTypeTag;
    switch (t) {
    case AT::Mesh:
        return "mesh";
    case AT::Texture:
        return "tex";
    case AT::Material:
        return "mat";
    case AT::Audio:
        return "audio";
    case AT::Script:
        return "script";
    case AT::Lightmap:
        return "lmap";
    case AT::ParallaxScene:
        return "plx";
    default:
        return "?";
    }
}

AssetTypeTag AssetTypeFromExtension(const std::filesystem::path& filePath) {
    std::string ext = filePath.extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".ktx" || ext == ".webp") {
        return AssetTypeTag::Texture;
    }
    if (ext == ".ogg" || ext == ".wav" || ext == ".flac" || ext == ".mp3") {
        return AssetTypeTag::Audio;
    }
    if (ext == ".glb" || ext == ".gltf" || ext == ".mesh") {
        return AssetTypeTag::Mesh;
    }
    if (ext == ".lua" || ext == ".nut" || ext == ".solv") {
        return AssetTypeTag::Script;
    }
    if (ext == ".prlx" || ext == ".parallax") {
        return AssetTypeTag::ParallaxScene;
    }
    return AssetTypeTag::Unknown;
}

RelicWriteOptions OptionsFromHeader(const RelicFileHeader& hdr) {
    RelicWriteOptions opt{};
    opt.Container = static_cast<Solstice::Core::Relic::ContainerType>(hdr.ContainerType);
    opt.TagSet = hdr.TagSet;
    return opt;
}

std::optional<std::vector<RelicWriteInput>> LoadEntriesForRewrite(const std::filesystem::path& relicPath, LoadError* errOut) {
    auto c = OpenRelic(relicPath);
    if (!c) {
        if (errOut) {
            errOut->Message = "Failed to open or parse RELIC: " + relicPath.string();
        }
        return std::nullopt;
    }

    std::unordered_map<uint64_t, std::string> hashToPath;
    if (!c->PathTableBlob.empty()) {
        std::vector<std::pair<std::string, uint64_t>> rows;
        if (ParsePathTableBlob(
                std::span<const std::byte>(c->PathTableBlob.data(), c->PathTableBlob.size()), rows)) {
            for (const auto& [p, h] : rows) {
                hashToPath[h] = p;
            }
        }
    }

    for (const auto& e : c->Manifest) {
        if ((e.Flags & FlagIsDelta) != 0) {
            if (errOut) {
                errOut->Message = "This RELIC contains delta-compressed entries; re-write is not supported yet.";
            }
            return std::nullopt;
        }
    }

    std::ifstream f(relicPath, std::ios::binary);
    if (!f) {
        if (errOut) {
            errOut->Message = "Cannot read data blob: " + relicPath.string();
        }
        return std::nullopt;
    }

    const uint64_t dataBase = c->Header.DataBlobOffset;
    std::vector<RelicWriteInput> out;
    out.reserve(c->Manifest.size());

    for (const auto& entry : c->Manifest) {
        f.seekg(static_cast<std::streamoff>(dataBase + entry.DataOffset));
        std::vector<std::byte> raw(entry.CompressedSize);
        if (entry.CompressedSize > 0) {
            f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(entry.CompressedSize));
            if (!f) {
                if (errOut) {
                    errOut->Message = "Short read in data blob for hash " + std::to_string(entry.AssetHash);
                }
                return std::nullopt;
            }
        }
        std::vector<std::byte> data = Solstice::Core::Relic::DecompressAsset(
            raw, GetCompressionType(entry.Flags), entry.UncompressedSize);
        if (entry.UncompressedSize > 0 && data.size() != entry.UncompressedSize) {
            if (errOut) {
                errOut->Message = "Decompression failed for hash " + std::to_string(entry.AssetHash);
            }
            return std::nullopt;
        }

        RelicWriteInput in{};
        in.Hash = entry.AssetHash;
        in.TypeTag = static_cast<AssetTypeTag>(entry.AssetTypeTag);
        in.ClusterId = entry.ClusterId;
        in.Compression = GetCompressionType(entry.Flags);
        in.Uncompressed = std::move(data);
        in.ExtraFlags = entry.Flags;

        GetDependencies(*c, entry, in.Dependencies);

        auto it = hashToPath.find(entry.AssetHash);
        if (it != hashToPath.end()) {
            in.LogicalPath = it->second;
        }

        out.push_back(std::move(in));
    }

    return out;
}

std::optional<std::vector<RelicWriteInput>> BuildInputsFromDirectory(
    const std::filesystem::path& root, const PackDirOptions& opt, std::string* errOut) {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) {
        if (errOut) {
            *errOut = "Not a directory: " + root.string();
        }
        return std::nullopt;
    }

    const std::filesystem::path rootAbs = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        if (errOut) {
            *errOut = "Cannot canonicalize path: " + root.string();
        }
        return std::nullopt;
    }

    std::vector<RelicWriteInput> inputs;
    for (const auto& dirIt : std::filesystem::recursive_directory_iterator(root, ec)) {
        if (ec) {
            if (errOut) {
                *errOut = "Directory walk error: " + ec.message();
            }
            return std::nullopt;
        }
        if (opt.SkipSymlinks) {
            std::error_code sec;
            if (std::filesystem::is_symlink(dirIt.path(), sec) && !sec) {
                continue;
            }
        }
        {
            std::error_code resec;
            if (!std::filesystem::is_regular_file(dirIt.path(), resec) || resec) {
                continue;
            }
        }
        const std::filesystem::path& p = dirIt.path();
        std::string rel;
        {
            std::error_code e2;
            std::filesystem::path relP = std::filesystem::relative(p, rootAbs, e2);
            if (e2) {
                relP = p.filename();
            }
            rel = NormalizeLogicalPath(relP.generic_string());
        }
        if (rel.empty()) {
            continue;
        }

        std::ifstream infile(p, std::ios::binary);
        if (!infile) {
            if (errOut) {
                *errOut = "Unreadable file: " + p.string();
            }
            return std::nullopt;
        }
        infile.seekg(0, std::ios::end);
        const auto sz = infile.tellg();
        infile.seekg(0);
        std::vector<std::byte> bytes(static_cast<size_t>(sz));
        if (sz > 0) {
            infile.read(reinterpret_cast<char*>(bytes.data()), sz);
        }
        if (!infile) {
            if (errOut) {
                *errOut = "Read failed: " + p.string();
            }
            return std::nullopt;
        }

        RelicWriteInput rw{};
        if (opt.Hash == PackDirOptions::HashMode::Path) {
            rw.Hash = HashPathFNV1a(rel);
        } else {
            rw.Hash = HashBytesFNV1a(std::span<const std::byte>(bytes.data(), bytes.size()));
        }
        rw.TypeTag = AssetTypeFromExtension(p);
        rw.ClusterId = 0;
        rw.Compression = opt.Compression;
        rw.Uncompressed = std::move(bytes);
        rw.LogicalPath = rel;
        inputs.push_back(std::move(rw));
    }

    std::sort(inputs.begin(), inputs.end(), [](const RelicWriteInput& a, const RelicWriteInput& b) {
        if (a.ClusterId != b.ClusterId) {
            return a.ClusterId < b.ClusterId;
        }
        return a.Hash < b.Hash;
    });
    for (size_t i = 1; i < inputs.size(); ++i) {
        if (inputs[i - 1].Hash == inputs[i].Hash) {
            if (errOut) {
                *errOut = "Duplicate hash after packing (use --hash content or fix paths): " + std::to_string(inputs[i].Hash);
            }
            return std::nullopt;
        }
    }

    return inputs;
}

bool WriteRelicFile(const std::filesystem::path& outPath, std::vector<RelicWriteInput> inputs, const RelicWriteOptions& opt,
    std::string* errOut) {
    if (inputs.empty()) {
        if (errOut) {
            *errOut = "No manifest entries; refusing to write an empty RELIC.";
        }
        return false;
    }
    for (const auto& in : inputs) {
        if (in.ExtraFlags & Solstice::Core::Relic::FlagIsDelta) {
            if (errOut) {
                *errOut = "Writer cannot emit delta entries.";
            }
            return false;
        }
    }
    if (!Solstice::Core::Relic::WriteRelic(outPath, std::move(inputs), opt)) {
        if (errOut) {
            *errOut = "WriteRelic failed (disk full, bad path, or duplicate hash in inputs).";
        }
        return false;
    }
    return true;
}

std::optional<std::vector<RelicWriteInput>> RemoveHashes(
    const std::filesystem::path& relicPath, const std::vector<uint64_t>& remove, std::string* errOut) {
    LoadError le;
    auto inputs = LoadEntriesForRewrite(relicPath, &le);
    if (!inputs) {
        if (errOut) {
            *errOut = le.Message;
        }
        return std::nullopt;
    }
    std::unordered_set<uint64_t> r(remove.begin(), remove.end());
    std::vector<RelicWriteInput> kept;
    kept.reserve(inputs->size());
    for (auto& in : *inputs) {
        if (r.find(in.Hash) == r.end()) {
            kept.push_back(std::move(in));
        }
    }
    if (kept.size() == inputs->size()) {
        if (errOut) {
            *errOut = "No matching hashes removed.";
        }
    }
    return kept;
}

std::optional<std::vector<RelicWriteInput>> AddFilesToContainer(const std::filesystem::path& relicPath,
    const std::vector<std::pair<std::filesystem::path, std::string>>& fileAndLogical, const CompressionType compression,
    std::string* errOut) {
    LoadError le;
    auto baseOpt = LoadEntriesForRewrite(relicPath, &le);
    if (!baseOpt) {
        if (errOut) {
            *errOut = le.Message;
        }
        return std::nullopt;
    }
    std::vector<RelicWriteInput> result = std::move(*baseOpt);
    std::unordered_set<uint64_t> used;
    for (const auto& e : result) {
        used.insert(e.Hash);
    }

    for (const auto& [path, logical] : fileAndLogical) {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            if (errOut) {
                *errOut = "Cannot read: " + path.string();
            }
            return std::nullopt;
        }
        f.seekg(0, std::ios::end);
        const auto sz = f.tellg();
        f.seekg(0);
        std::vector<std::byte> bytes(static_cast<size_t>(sz));
        if (sz > 0) {
            f.read(reinterpret_cast<char*>(bytes.data()), sz);
        }
        if (!f) {
            if (errOut) {
                *errOut = "Read failed: " + path.string();
            }
            return std::nullopt;
        }
        RelicWriteInput in{};
        in.Hash = HashPathFNV1a(NormalizeLogicalPath(logical));
        in.TypeTag = AssetTypeFromExtension(path);
        in.ClusterId = 0;
        in.Compression = compression;
        in.Uncompressed = std::move(bytes);
        in.LogicalPath = logical;
        if (used.find(in.Hash) != used.end()) {
            if (errOut) {
                *errOut = "Hash already in archive: " + std::to_string(in.Hash) + " (" + logical + ")";
            }
            return std::nullopt;
        }
        used.insert(in.Hash);
        result.push_back(std::move(in));
    }
    return result;
}

std::optional<std::vector<std::byte>> ExtractUncompressed(
    const std::filesystem::path& relicPath, const uint64_t assetHash, std::string* errOut) {
    auto c = OpenRelic(relicPath);
    if (!c) {
        if (errOut) {
            *errOut = "OpenRelic failed.";
        }
        return std::nullopt;
    }
    const RelicManifestEntry* found = nullptr;
    for (const auto& e : c->Manifest) {
        if (e.AssetHash == assetHash) {
            found = &e;
            break;
        }
    }
    if (!found) {
        if (errOut) {
            *errOut = "Hash not in manifest.";
        }
        return std::nullopt;
    }
    if ((found->Flags & FlagIsDelta) != 0) {
        if (errOut) {
            *errOut = "Entry is delta-compressed; extract not supported.";
        }
        return std::nullopt;
    }
    std::ifstream f(relicPath, std::ios::binary);
    if (!f) {
        if (errOut) {
            *errOut = "File read error.";
        }
        return std::nullopt;
    }
    f.seekg(static_cast<std::streamoff>(c->Header.DataBlobOffset + found->DataOffset));
    std::vector<std::byte> raw(found->CompressedSize);
    if (found->CompressedSize > 0) {
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(found->CompressedSize));
        if (!f) {
            if (errOut) {
                *errOut = "Data blob read failed.";
            }
            return std::nullopt;
        }
    }
    return Solstice::Core::Relic::DecompressAsset(
        raw, GetCompressionType(found->Flags), found->UncompressedSize);
}

bool MergeRelics(const std::filesystem::path& outPath, const std::vector<std::filesystem::path>& paths, std::string* errOut) {
    if (paths.empty()) {
        if (errOut) {
            *errOut = "No input RELIC paths.";
        }
        return false;
    }
    std::unordered_map<uint64_t, RelicWriteInput> byHash;
    RelicWriteOptions options{};
    if (auto c0 = OpenRelic(paths[0])) {
        options = OptionsFromHeader(c0->Header);
    }
    LoadError le;
    for (const auto& p : paths) {
        auto entries = LoadEntriesForRewrite(p, &le);
        if (!entries) {
            if (errOut) {
                *errOut = le.Message;
            }
            return false;
        }
        for (auto& in : *entries) {
            byHash[in.Hash] = std::move(in);
        }
    }
    std::vector<RelicWriteInput> merged;
    merged.reserve(byHash.size());
    for (auto& kv : byHash) {
        merged.push_back(std::move(kv.second));
    }
    return WriteRelicFile(outPath, std::move(merged), options, errOut);
}

const char* AssetTypeLongName(const Solstice::Core::Relic::AssetTypeTag t) {
    using AT = Solstice::Core::Relic::AssetTypeTag;
    switch (t) {
    case AT::Mesh:
        return "mesh";
    case AT::Texture:
        return "texture";
    case AT::Material:
        return "material";
    case AT::Audio:
        return "audio";
    case AT::Script:
        return "script";
    case AT::Lightmap:
        return "lightmap";
    case AT::ParallaxScene:
        return "parallax";
    default:
        return "unknown";
    }
}

std::optional<RelicArchiveStats> GetArchiveStats(const std::filesystem::path& relicPath, std::string* errOut) {
    std::error_code ec;
    const uint64_t fileSize = std::filesystem::file_size(relicPath, ec);
    if (ec) {
        if (errOut) {
            *errOut = "Cannot stat file: " + relicPath.string();
        }
        return std::nullopt;
    }
    auto c = OpenRelic(relicPath);
    if (!c) {
        if (errOut) {
            *errOut = "OpenRelic failed.";
        }
        return std::nullopt;
    }
    RelicArchiveStats s{};
    s.FileSizeBytes = fileSize;
    s.EntryCount = c->Manifest.size();
    s.HasPathTable = c->Header.PathTableSize > 0;
    s.PathTableBytes = c->Header.PathTableSize;
    s.DependencyTableBytes = c->Header.DependencyTableSize;
    for (const auto& e : c->Manifest) {
        s.TotalCompressedPayload += e.CompressedSize;
        s.TotalUncompressed += e.UncompressedSize;
    }
    return s;
}

bool VerifyRelic(const std::filesystem::path& relicPath, std::string* firstError, size_t& okCount, size_t& failCount,
    size_t& skippedDelta, const bool verbose) {
    okCount = 0;
    failCount = 0;
    skippedDelta = 0;
    auto c = OpenRelic(relicPath);
    if (!c) {
        if (firstError) {
            *firstError = "OpenRelic failed.";
        }
        ++failCount;
        return false;
    }
    std::ifstream f(relicPath, std::ios::binary);
    if (!f) {
        if (firstError) {
            *firstError = "Cannot open file for reading.";
        }
        ++failCount;
        return false;
    }
    const uint64_t dataBase = c->Header.DataBlobOffset;
    for (const auto& entry : c->Manifest) {
        if ((entry.Flags & FlagIsDelta) != 0) {
            ++skippedDelta;
            if (verbose) {
                std::cerr << "verify: skipped delta entry hash 0x" << std::hex << entry.AssetHash << std::dec << "\n";
            }
            continue;
        }
        f.seekg(static_cast<std::streamoff>(dataBase + entry.DataOffset));
        std::vector<std::byte> raw(entry.CompressedSize);
        if (entry.CompressedSize > 0) {
            f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(entry.CompressedSize));
            if (!f) {
                if (firstError && firstError->empty()) {
                    *firstError = "Short read for hash " + std::to_string(entry.AssetHash);
                }
                ++failCount;
                continue;
            }
        }
        auto out = Solstice::Core::Relic::DecompressAsset(
            raw, GetCompressionType(entry.Flags), entry.UncompressedSize);
        if (entry.UncompressedSize > 0 && out.size() != entry.UncompressedSize) {
            if (firstError && firstError->empty()) {
                *firstError = "Decompress size mismatch for hash " + std::to_string(entry.AssetHash);
            }
            ++failCount;
            continue;
        }
        ++okCount;
    }
    return failCount == 0;
}

} // namespace Refulgent::RelicOps
