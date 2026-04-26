// Refulgent CLI — RELIC / bootstrap commands (shared binary; no GUI).

#include "RefulgentCli.hxx"
#include "RelicOps.hxx"

#include <Core/Relic/Bootstrap.hxx>
#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Unpack.hxx>

#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// File scope: RunRefulgentCli (in namespace Refulgent) and Cmd* helpers need shared state.
static thread_local const Refulgent::CliGlobalOptions* g_GlobalCliOpts{nullptr};

namespace {

using Solstice::Core::Relic::AssetTypeTag;
using Solstice::Core::Relic::BootstrapEntry;
using Solstice::Core::Relic::CompressionType;
using Solstice::Core::Relic::GetCompressionType;
using Solstice::Core::Relic::GetStreamingPriority;
using Solstice::Core::Relic::OpenRelic;
using Solstice::Core::Relic::ParseBootstrap;
using Solstice::Core::Relic::ParsePathTableBlob;
using Solstice::Core::Relic::RelicTag;
using Solstice::Core::Relic::StreamingHint;
using Solstice::Core::Relic::UnpackRelic;
using Solstice::Core::Relic::WriteBootstrap;

void PrintUsage() {
    std::cout
        << "Refulgent — RELIC archive manager\n"
        << "\nGlobal options (before subcommand):\n"
        << "  -h, -?, --help     Show this help\n"
        << "  -v, --version      Print version\n"
        << "  --verbose          More diagnostic output (verify, etc.)\n"
        << "  -q, --quiet        Suppress non-error stdout\n"
        << "  --json             Default JSON for list (or use per-command --json)\n"
        << "\nSubcommands:\n"
        << "  info <file.relic>\n"
        << "  list <file.relic> [--json]\n"
        << "  stats <file.relic>\n"
        << "  verify <file.relic>\n"
        << "  diff <a.relic> <b.relic>\n"
        << "  export <file.relic> <outDir> [--no-path-names]\n"
        << "  pack <out.relic> <inputDir> [--hash path|content] [--compress none|lz4|zstd]\n"
        << "  add <archive.relic> <file> <logicalPath> [--compress none|lz4|zstd] [--out <path>]\n"
        << "  remove <archive.relic> <hash> [more hashes...] [--out <path>]\n"
        << "  extract <archive.relic> <hash> <outFile>\n"
        << "  merge <out.relic> <a.relic> <b.relic> [more...]\n"
        << "  bootstrap list <game.data.relic>\n"
        << "  bootstrap add <in> <out> <relicPath> <priority> <hint>\n"
        << "  bootstrap remove <in> <out> <index>\n"
        << "\n"
        << "  help               Same as -h\n"
        << "\nWith no subcommand, the graphical UI opens. A lone .relic path opens the UI on that file.\n"
        << "Hash format: 64-bit hex (optional 0x prefix).\n";
}

bool ParseU64(std::string_view s, uint64_t& out) {
    while (!s.empty() && s.front() == ' ') {
        s.remove_prefix(1);
    }
    if (s.size() >= 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')) {
        s.remove_prefix(2);
    }
    if (s.empty()) {
        return false;
    }
    const char* first = s.data();
    const char* last = s.data() + s.size();
    uint64_t v = 0;
    auto r = std::from_chars(first, last, v, 16);
    if (r.ec != std::errc() || r.ptr != last) {
        return false;
    }
    out = v;
    return true;
}

CompressionType ParseCompression(std::string_view s) {
    if (s == "none") {
        return CompressionType::None;
    }
    if (s == "lz4") {
        return CompressionType::LZ4;
    }
    return CompressionType::Zstd;
}

StreamingHint ParseHint(std::string_view s) {
    if (s == "preload") {
        return StreamingHint::Preload;
    }
    if (s == "lazy") {
        return StreamingHint::Lazy;
    }
    return StreamingHint::Stream;
}

const char* StreamingHintName(StreamingHint h) {
    switch (h) {
    case StreamingHint::Preload:
        return "preload";
    case StreamingHint::Lazy:
        return "lazy";
    default:
        return "stream";
    }
}

int CmdInfo(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "usage: info <file.relic>\n";
        return 2;
    }
    auto c = OpenRelic(args[2]);
    if (!c) {
        std::cerr << "Failed to open RELIC.\n";
        return 1;
    }
    const auto& h = c->Header;
    std::cout << "Path: " << args[2] << "\n";
    std::cout << "Format version: " << h.FormatVersion << "\n";
    std::cout << "Container type: " << static_cast<int>(h.ContainerType) << "\n";
    std::cout << "Tag set: " << h.TagSet << "\n";
    std::cout << "Manifest entries: " << c->Manifest.size() << "\n";
    std::cout << "Dependency table: " << h.DependencyTableSize << " bytes\n";
    std::cout << "Path table: " << h.PathTableSize << " bytes\n";
    std::cout << "Data blob offset: " << h.DataBlobOffset << "\n";
    return 0;
}

int CmdList(const std::vector<std::string>& args) {
    bool json = (g_GlobalCliOpts && g_GlobalCliOpts->JsonOutput);
    std::string path;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--json") {
            json = true;
        } else if (!args[i].empty() && args[i][0] != '-') {
            path = args[i];
        }
    }
    if (path.empty()) {
        std::cerr << "usage: list <file.relic> [--json]\n";
        return 2;
    }
    auto c = OpenRelic(path);
    if (!c) {
        std::cerr << "Failed to open RELIC.\n";
        return 1;
    }
    std::unordered_map<uint64_t, std::string> hashToPath;
    if (!c->PathTableBlob.empty()) {
        std::vector<std::pair<std::string, uint64_t>> rows;
        if (ParsePathTableBlob(std::span<const std::byte>(c->PathTableBlob.data(), c->PathTableBlob.size()), rows)) {
            for (const auto& [p, hash] : rows) {
                hashToPath[hash] = p;
            }
        }
    }
    if (json) {
        std::cout << "[\n";
        for (size_t i = 0; i < c->Manifest.size(); ++i) {
            const auto& e = c->Manifest[i];
            if (i > 0) {
                std::cout << ",\n";
            }
            char hb[32];
            std::snprintf(hb, sizeof(hb), "%016llX", static_cast<unsigned long long>(e.AssetHash));
            std::cout << "  {\"hashHex\":\"" << hb << "\"";
            std::cout << ",\"type\":\"" << Refulgent::RelicOps::AssetTypeLongName(static_cast<AssetTypeTag>(e.AssetTypeTag)) << "\"";
            std::cout << ",\"compression\":\"" << Refulgent::RelicOps::CompressionName(GetCompressionType(e.Flags)) << "\"";
            std::cout << ",\"compressedSize\":" << e.CompressedSize;
            std::cout << ",\"uncompressedSize\":" << e.UncompressedSize;
            std::cout << ",\"clusterId\":" << e.ClusterId;
            std::cout << ",\"streamingPriority\":" << static_cast<int>(GetStreamingPriority(e.Flags));
            auto it = hashToPath.find(e.AssetHash);
            if (it != hashToPath.end()) {
                std::cout << ",\"logicalPath\":\"" << it->second << "\"";
            }
            std::cout << "}";
        }
        std::cout << "\n]\n";
        return 0;
    }
    for (const auto& e : c->Manifest) {
        std::cout << std::hex << "0x" << e.AssetHash << std::dec;
        std::cout << '\t' << Refulgent::RelicOps::AssetTypeLongName(static_cast<AssetTypeTag>(e.AssetTypeTag));
        std::cout << '\t' << Refulgent::RelicOps::CompressionName(GetCompressionType(e.Flags));
        std::cout << '\t' << e.CompressedSize << '\t' << e.UncompressedSize;
        std::cout << '\t' << "cluster " << e.ClusterId;
        std::cout << '\t' << "prio " << static_cast<int>(GetStreamingPriority(e.Flags));
        auto it = hashToPath.find(e.AssetHash);
        if (it != hashToPath.end()) {
            std::cout << '\t' << it->second;
        }
        std::cout << '\n';
    }
    return 0;
}

int CmdExport(const std::vector<std::string>& args) {
    bool byName = true;
    std::vector<std::string> pos;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--no-path-names") {
            byName = false;
        } else if (args[i].size() > 0 && args[i][0] != '-') {
            pos.push_back(args[i]);
        }
    }
    if (pos.size() < 2) {
        std::cerr << "usage: export <file.relic> <outDir> [--no-path-names]\n";
        return 2;
    }
    if (!UnpackRelic(pos[0], pos[1], byName)) {
        std::cerr << "Export failed.\n";
        return 1;
    }
    std::cout << "Exported to " << pos[1] << "\n";
    return 0;
}

int CmdPack(const std::vector<std::string>& args) {
    Refulgent::RelicOps::PackDirOptions opt{};
    std::vector<std::string> pos;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--hash" && i + 1 < args.size()) {
            ++i;
            if (args[i] == "content") {
                opt.Hash = Refulgent::RelicOps::PackDirOptions::HashMode::Content;
            } else {
                opt.Hash = Refulgent::RelicOps::PackDirOptions::HashMode::Path;
            }
        } else if (args[i] == "--compress" && i + 1 < args.size()) {
            ++i;
            opt.Compression = ParseCompression(args[i]);
        } else if (args[i].size() > 0 && args[i][0] != '-') {
            pos.push_back(args[i]);
        }
    }
    if (pos.size() < 2) {
        std::cerr << "usage: pack <out.relic> <inputDir> [--hash path|content] [--compress none|lz4|zstd]\n";
        return 2;
    }
    std::string err;
    auto inputs = Refulgent::RelicOps::BuildInputsFromDirectory(pos[1], opt, &err);
    if (!inputs) {
        std::cerr << err << "\n";
        return 1;
    }
    if (inputs->empty()) {
        std::cerr << "No files under " << pos[1] << " to pack.\n";
        return 1;
    }
    Solstice::Core::Relic::RelicWriteOptions wopt{};
    wopt.Container = Solstice::Core::Relic::ContainerType::Content;
    wopt.TagSet = RelicTag::TagBase;
    if (!Refulgent::RelicOps::WriteRelicFile(pos[0], std::move(*inputs), wopt, &err)) {
        std::cerr << err << "\n";
        return 1;
    }
    std::cout << "Wrote " << pos[0] << "\n";
    return 0;
}

int CmdAdd(const std::vector<std::string>& args) {
    CompressionType comp = CompressionType::Zstd;
    std::string outOverride;
    std::vector<std::string> pos;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--compress" && i + 1 < args.size()) {
            ++i;
            comp = ParseCompression(args[i]);
        } else if (args[i] == "--out" && i + 1 < args.size()) {
            ++i;
            outOverride = args[i];
        } else if (args[i].size() > 0 && args[i][0] != '-') {
            pos.push_back(args[i]);
        }
    }
    if (pos.size() < 3) {
        std::cerr << "usage: add <archive.relic> <file> <logicalPath> [--compress none|lz4|zstd] [--out <path>]\n";
        return 2;
    }
    std::vector<std::pair<std::filesystem::path, std::string>> batch;
    batch.push_back({pos[1], pos[2]});
    std::string err;
    auto merged = Refulgent::RelicOps::AddFilesToContainer(pos[0], batch, comp, &err);
    if (!merged) {
        std::cerr << err << "\n";
        return 1;
    }
    auto c = OpenRelic(pos[0]);
    if (!c) {
        std::cerr << "OpenRelic failed after build.\n";
        return 1;
    }
    const std::filesystem::path outPath = outOverride.empty() ? std::filesystem::path(pos[0]) : std::filesystem::path(outOverride);
    if (!Refulgent::RelicOps::WriteRelicFile(outPath, std::move(*merged), Refulgent::RelicOps::OptionsFromHeader(c->Header), &err)) {
        std::cerr << err << "\n";
        return 1;
    }
    std::cout << "Updated " << outPath.string() << "\n";
    return 0;
}

int CmdRemove(const std::vector<std::string>& args) {
    std::string outOverride;
    std::vector<std::string> pos;
    std::vector<uint64_t> hashes;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--out" && i + 1 < args.size()) {
            ++i;
            outOverride = args[i];
        } else if (args[i].size() > 0 && args[i][0] != '-') {
            pos.push_back(args[i]);
        }
    }
    if (pos.size() < 2) {
        std::cerr << "usage: remove <archive.relic> <hash> [more...] [--out <path>]\n";
        return 2;
    }
    for (size_t i = 1; i < pos.size(); ++i) {
        uint64_t h = 0;
        if (!ParseU64(pos[i], h)) {
            std::cerr << "Bad hash: " << pos[i] << "\n";
            return 2;
        }
        hashes.push_back(h);
    }
    std::string err;
    auto kept = Refulgent::RelicOps::RemoveHashes(pos[0], hashes, &err);
    if (!kept) {
        std::cerr << err << "\n";
        return 1;
    }
    if (kept->empty()) {
        std::cerr << "Refusing to write an empty archive.\n";
        return 1;
    }
    auto c = OpenRelic(pos[0]);
    if (!c) {
        std::cerr << "OpenRelic failed.\n";
        return 1;
    }
    const size_t nKept = kept->size();
    const std::filesystem::path outPath = outOverride.empty() ? std::filesystem::path(pos[0]) : std::filesystem::path(outOverride);
    if (!Refulgent::RelicOps::WriteRelicFile(outPath, std::move(*kept), Refulgent::RelicOps::OptionsFromHeader(c->Header), &err)) {
        std::cerr << err << "\n";
        return 1;
    }
    std::cout << "Wrote " << outPath.string() << " (" << nKept << " entries)\n";
    return 0;
}

int CmdExtract(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        std::cerr << "usage: extract <archive.relic> <hash> <outFile>\n";
        return 2;
    }
    uint64_t h = 0;
    if (!ParseU64(args[3], h)) {
        std::cerr << "Bad hash.\n";
        return 2;
    }
    std::string err;
    auto bytes = Refulgent::RelicOps::ExtractUncompressed(args[2], h, &err);
    if (!bytes) {
        std::cerr << err << "\n";
        return 1;
    }
    std::ofstream f(args[4], std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "Cannot open output file.\n";
        return 1;
    }
    if (!bytes->empty()) {
        f.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    }
    if (!f) {
        std::cerr << "Write failed.\n";
        return 1;
    }
    std::cout << "Wrote " << args[4] << " (" << bytes->size() << " bytes)\n";
    return 0;
}

int CmdMerge(const std::vector<std::string>& args) {
    if (args.size() < 5) {
        std::cerr << "usage: merge <out.relic> <a.relic> <b.relic> [more...]\n";
        return 2;
    }
    std::vector<std::filesystem::path> inputs;
    for (size_t i = 3; i < args.size(); ++i) {
        if (args[i].size() > 0 && args[i][0] != '-') {
            inputs.push_back(args[i]);
        }
    }
    std::string err;
    if (!Refulgent::RelicOps::MergeRelics(args[2], inputs, &err)) {
        std::cerr << err << "\n";
        return 1;
    }
    std::cout << "Wrote " << args[2] << "\n";
    return 0;
}

int CmdStats(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "usage: stats <file.relic>\n";
        return 2;
    }
    std::string err;
    auto st = Refulgent::RelicOps::GetArchiveStats(args[2], &err);
    if (!st) {
        std::cerr << err << "\n";
        return 1;
    }
    const bool q = g_GlobalCliOpts && g_GlobalCliOpts->Quiet;
    if (g_GlobalCliOpts && g_GlobalCliOpts->JsonOutput) {
        std::cout << "{"
            << "\"path\":\"" << args[2] << "\""
            << ",\"fileSizeBytes\":" << st->FileSizeBytes
            << ",\"entryCount\":" << st->EntryCount
            << ",\"totalCompressedPayload\":" << st->TotalCompressedPayload
            << ",\"totalUncompressed\":" << st->TotalUncompressed
            << ",\"hasPathTable\":" << (st->HasPathTable ? "true" : "false")
            << ",\"pathTableBytes\":" << st->PathTableBytes
            << ",\"dependencyTableBytes\":" << st->DependencyTableBytes
            << "}"
            << "\n";
        return 0;
    }
    if (!q) {
        std::cout << "Path: " << args[2] << "\n";
        std::cout << "File size: " << st->FileSizeBytes << " bytes\n";
        std::cout << "Entries: " << st->EntryCount << "\n";
        std::cout << "Compressed payload (sum): " << st->TotalCompressedPayload << " bytes\n";
        std::cout << "Uncompressed (sum): " << st->TotalUncompressed << " bytes\n";
        if (st->TotalUncompressed > 0) {
            const double ratio = static_cast<double>(st->TotalCompressedPayload) / static_cast<double>(st->TotalUncompressed);
            std::cout << "Payload / uncompressed: " << ratio << "\n";
        }
        std::cout << "Path table: " << (st->HasPathTable ? "yes" : "no") << " (" << st->PathTableBytes << " bytes)\n";
        std::cout << "Dependency table: " << st->DependencyTableBytes << " bytes\n";
    }
    return 0;
}

int CmdVerify(const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "usage: verify <file.relic>\n";
        return 2;
    }
    const bool verbose = g_GlobalCliOpts && g_GlobalCliOpts->Verbose;
    std::string firstErr;
    size_t ok = 0, fail = 0, skipDelta = 0;
    (void)Refulgent::RelicOps::VerifyRelic(args[2], &firstErr, ok, fail, skipDelta, verbose);
    if (g_GlobalCliOpts && g_GlobalCliOpts->JsonOutput) {
        std::cout << "{"
            << "\"ok\":" << (fail == 0 ? "true" : "false")
            << ",\"verified\":" << ok
            << ",\"failed\":" << fail
            << ",\"skippedDelta\":" << skipDelta;
        if (!firstErr.empty()) {
            std::cout << ",\"error\":\"" << firstErr << "\"";
        }
        std::cout << "}\n";
        return fail == 0 ? 0 : 1;
    }
    if (!(g_GlobalCliOpts && g_GlobalCliOpts->Quiet)) {
        std::cout << "Verified: " << ok << "  Failed: " << fail << "  Skipped (delta): " << skipDelta << "\n";
    }
    if (!firstErr.empty() && (!g_GlobalCliOpts || !g_GlobalCliOpts->Quiet)) {
        std::cout << "First error: " << firstErr << "\n";
    }
    return fail == 0 ? 0 : 1;
}

int CmdDiff(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "usage: diff <a.relic> <b.relic>\n";
        return 2;
    }
    auto a = OpenRelic(args[2]);
    auto b = OpenRelic(args[3]);
    if (!a || !b) {
        std::cerr << "Failed to open one or both RELIC files.\n";
        return 1;
    }
    std::unordered_set<uint64_t> ha, hb;
    for (const auto& e : a->Manifest) {
        ha.insert(e.AssetHash);
    }
    for (const auto& e : b->Manifest) {
        hb.insert(e.AssetHash);
    }
    std::vector<uint64_t> onlyA, onlyB, both;
    for (uint64_t h : ha) {
        if (hb.find(h) == hb.end()) {
            onlyA.push_back(h);
        } else {
            both.push_back(h);
        }
    }
    for (uint64_t h : hb) {
        if (ha.find(h) == ha.end()) {
            onlyB.push_back(h);
        }
    }
    if (g_GlobalCliOpts && g_GlobalCliOpts->JsonOutput) {
        std::cout << "{\"onlyA\":" << onlyA.size() << ",\"onlyB\":" << onlyB.size() << ",\"both\":" << both.size() << "}\n";
        return 0;
    }
    if (!(g_GlobalCliOpts && g_GlobalCliOpts->Quiet)) {
        std::cout << "Only in " << args[2] << ": " << onlyA.size() << "\n";
        for (uint64_t h : onlyA) {
            std::cout << "  0x" << std::hex << h << std::dec << "\n";
        }
        std::cout << "Only in " << args[3] << ": " << onlyB.size() << "\n";
        for (uint64_t h : onlyB) {
            std::cout << "  0x" << std::hex << h << std::dec << "\n";
        }
        std::cout << "In both: " << both.size() << "\n";
    }
    return 0;
}

int CmdBootstrapList(const std::vector<std::string>& args) {
    if (args.size() < 4) {
        std::cerr << "usage: bootstrap list <game.data.relic>\n";
        return 2;
    }
    auto b = ParseBootstrap(args[3]);
    if (!b || !b->Valid) {
        std::cerr << "Invalid bootstrap file.\n";
        return 1;
    }
    for (size_t i = 0; i < b->Entries.size(); ++i) {
        const auto& e = b->Entries[i];
        std::cout << i << "\t" << e.Path << "\t" << e.Priority << "\t" << StreamingHintName(e.Hint) << "\t" << e.TagSet << "\n";
    }
    return 0;
}

int CmdBootstrapAdd(const std::vector<std::string>& args) {
    if (args.size() < 8) {
        std::cerr
            << "usage: bootstrap add <in.game.data.relic> <out.game.data.relic> <relicPath> <priority> <hint>\n"
            << "  hint: preload | stream | lazy\n";
        return 2;
    }
    std::vector<BootstrapEntry> entries;
    {
        auto b = ParseBootstrap(args[3]);
        if (b && b->Valid) {
            entries = b->Entries;
        }
    }
    BootstrapEntry ne{};
    ne.Path = args[5];
    ne.Priority = static_cast<uint32_t>(std::stoul(args[6]));
    ne.Hint = ParseHint(args[7]);
    ne.TagSet = RelicTag::TagBase;
    entries.push_back(std::move(ne));
    if (!WriteBootstrap(args[4], entries)) {
        std::cerr << "WriteBootstrap failed.\n";
        return 1;
    }
    std::cout << "Wrote " << args[4] << "\n";
    return 0;
}

int CmdBootstrapRemove(const std::vector<std::string>& args) {
    if (args.size() < 6) {
        std::cerr << "usage: bootstrap remove <in> <out> <index>\n";
        return 2;
    }
    auto b = ParseBootstrap(args[3]);
    if (!b || !b->Valid) {
        std::cerr << "Invalid bootstrap file.\n";
        return 1;
    }
    const size_t idx = static_cast<size_t>(std::stoull(args[5]));
    if (idx >= b->Entries.size()) {
        std::cerr << "Index out of range.\n";
        return 1;
    }
    b->Entries.erase(b->Entries.begin() + static_cast<std::ptrdiff_t>(idx));
    if (!WriteBootstrap(args[4], b->Entries)) {
        std::cerr << "WriteBootstrap failed.\n";
        return 1;
    }
    std::cout << "Wrote " << args[4] << "\n";
    return 0;
}

static int DispatchCli(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        PrintUsage();
        return 2;
    }
    const std::string& cmd = args[1];
    if (cmd == "help") {
        PrintUsage();
        return 0;
    }
    if (cmd == "info") {
        return CmdInfo(args);
    }
    if (cmd == "list") {
        return CmdList(args);
    }
    if (cmd == "stats") {
        return CmdStats(args);
    }
    if (cmd == "verify") {
        return CmdVerify(args);
    }
    if (cmd == "diff") {
        return CmdDiff(args);
    }
    if (cmd == "export") {
        return CmdExport(args);
    }
    if (cmd == "pack") {
        return CmdPack(args);
    }
    if (cmd == "add") {
        return CmdAdd(args);
    }
    if (cmd == "remove") {
        return CmdRemove(args);
    }
    if (cmd == "extract") {
        return CmdExtract(args);
    }
    if (cmd == "merge") {
        return CmdMerge(args);
    }
    if (cmd == "bootstrap") {
        if (args.size() < 3) {
            std::cerr << "usage: bootstrap list|add|remove ...\n";
            return 2;
        }
        if (args[2] == "list") {
            return CmdBootstrapList(args);
        }
        if (args[2] == "add") {
            return CmdBootstrapAdd(args);
        }
        if (args[2] == "remove") {
            return CmdBootstrapRemove(args);
        }
        std::cerr << "Unknown bootstrap subcommand.\n";
        return 2;
    }

    std::cerr << "Unknown subcommand. Try: Refulgent --help\n";
    return 2;
}

} // namespace

namespace Refulgent {

int RunRefulgentCli(int argc, char** argv, const CliGlobalOptions& opts) {
    g_GlobalCliOpts = &opts;
    if (argc < 2) {
        PrintUsage();
        g_GlobalCliOpts = nullptr;
        return 2;
    }
    std::vector<std::string> args(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args[static_cast<size_t>(i)] = argv[i] ? argv[i] : "";
    }
    const int r = DispatchCli(args);
    g_GlobalCliOpts = nullptr;
    return r;
}

} // namespace Refulgent
