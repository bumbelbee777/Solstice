#include "TestHarness.hxx"

#include <Core/Relic/Bootstrap.hxx>
#include <Core/Relic/PathTable.hxx>
#include <Core/Relic/Reader.hxx>
#include <Core/Relic/Types.hxx>
#include <Core/Relic/Unpack.hxx>
#include <Core/Relic/Writer.hxx>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using Solstice::Core::Relic::AssetTypeTag;
using Solstice::Core::Relic::BootstrapEntry;
using Solstice::Core::Relic::CompressionType;
using Solstice::Core::Relic::ContainerType;
using Solstice::Core::Relic::OpenRelic;
using Solstice::Core::Relic::ParseBootstrap;
using Solstice::Core::Relic::ParsePathTableBlob;
using Solstice::Core::Relic::RelicWriteInput;
using Solstice::Core::Relic::RelicWriteOptions;
using Solstice::Core::Relic::RelicTag;
using Solstice::Core::Relic::StreamingHint;
using Solstice::Core::Relic::UnpackRelic;
using Solstice::Core::Relic::WriteBootstrap;
using Solstice::Core::Relic::WriteRelic;

static std::vector<std::byte> Bytes(const char* s) {
    const size_t n = std::strlen(s);
    std::vector<std::byte> out(n);
    for (size_t i = 0; i < n; ++i) {
        out[i] = static_cast<std::byte>(static_cast<unsigned char>(s[i]));
    }
    return out;
}

static bool RoundTripRelic() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "solstice_relic_test";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    SOLSTICE_TEST_ASSERT(!ec, "create_temp_dir");

    const auto relicPath = dir / "test.relic";
    const auto unpackDir = dir / "out";

    std::vector<RelicWriteInput> inputs;
    RelicWriteInput a{};
    a.Hash = 0xA001ull;
    a.TypeTag = AssetTypeTag::Texture;
    a.ClusterId = 1;
    a.Compression = CompressionType::None;
    a.Uncompressed = Bytes("hello");
    a.LogicalPath = "textures/a.txt";
    inputs.push_back(std::move(a));

    RelicWriteInput b{};
    b.Hash = 0xB002ull;
    b.TypeTag = AssetTypeTag::Audio;
    b.ClusterId = 1;
    b.Compression = CompressionType::LZ4;
    b.Uncompressed = Bytes("lz4 payload");
    b.LogicalPath = "audio/b.raw";
    inputs.push_back(std::move(b));

    RelicWriteOptions opt{};
    opt.Container = ContainerType::Content;
    opt.TagSet = RelicTag::TagBase;

    SOLSTICE_TEST_ASSERT(WriteRelic(relicPath, std::move(inputs), opt), "WriteRelic");

    auto c = OpenRelic(relicPath);
    SOLSTICE_TEST_ASSERT(c.has_value(), "OpenRelic");
    SOLSTICE_TEST_ASSERT(c->Manifest.size() == 2, "manifest count");

    std::vector<std::pair<std::string, uint64_t>> paths;
    SOLSTICE_TEST_ASSERT(
        ParsePathTableBlob(std::span<const std::byte>(c->PathTableBlob.data(), c->PathTableBlob.size()), paths),
        "ParsePathTableBlob");
    SOLSTICE_TEST_ASSERT(paths.size() == 2, "path table size");

    std::filesystem::remove_all(unpackDir, ec);
    SOLSTICE_TEST_ASSERT(UnpackRelic(relicPath, unpackDir, true), "UnpackRelic");

    return true;
}

static bool RoundTripBootstrap() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "solstice_relic_boot_test";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    SOLSTICE_TEST_ASSERT(!ec, "create_boot_temp_dir");

    const auto p = dir / "game.data.relic";
    std::vector<BootstrapEntry> entries;
    BootstrapEntry e{};
    e.Path = "content.relic";
    e.Priority = 10;
    e.Hint = StreamingHint::Stream;
    e.TagSet = RelicTag::TagBase;
    entries.push_back(e);

    SOLSTICE_TEST_ASSERT(WriteBootstrap(p, entries), "WriteBootstrap");
    auto cfg = ParseBootstrap(p);
    SOLSTICE_TEST_ASSERT(cfg.has_value() && cfg->Valid, "ParseBootstrap");
    SOLSTICE_TEST_ASSERT(cfg->Entries.size() == 1, "bootstrap entries");
    SOLSTICE_TEST_ASSERT(cfg->Entries[0].Path == "content.relic", "bootstrap path");

    return true;
}

int main() {
    SOLSTICE_TEST_ASSERT(RoundTripRelic(), "RoundTripRelic");
    SOLSTICE_TEST_ASSERT(RoundTripBootstrap(), "RoundTripBootstrap");
    return SolsticeTestMainResult("RelicFormatTest");
}
