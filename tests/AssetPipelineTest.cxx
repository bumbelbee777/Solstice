#include "TestHarness.hxx"
#include "Core/Serialization/JSON.hxx"
#include "Core/Serialization/Base64.hxx"
#include "Asset/IO/WavefrontParser.hxx"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifndef SOLSTICE_TEST_FIXTURES_DIR
#define SOLSTICE_TEST_FIXTURES_DIR "."
#endif

namespace {

bool RunAssetPipeline() {
    const std::filesystem::path fixtureDir(SOLSTICE_TEST_FIXTURES_DIR);
    const auto objPath = fixtureDir / "minimal.obj";

    Solstice::Core::WavefrontData mesh = Solstice::Core::WavefrontParser::Parse(objPath);
    {
        const std::string err = std::string("Wavefront parse: ") + mesh.Error;
        SOLSTICE_TEST_ASSERT(mesh.Success, err.c_str());
    }
    SOLSTICE_TEST_ASSERT(!mesh.Positions.empty(), "Expected positions");

    Solstice::Core::JSONValue root;
    root["pi"] = Solstice::Core::JSONValue(3.25);
    root["name"] = Solstice::Core::JSONValue(std::string("solstice"));
    Solstice::Core::JSONArray arr;
    arr.push_back(Solstice::Core::JSONValue(1.0));
    arr.push_back(Solstice::Core::JSONValue(std::string("two")));
    root["arr"] = Solstice::Core::JSONValue(std::move(arr));

    const std::string dumped = root.Stringify(true);
    const Solstice::Core::JSONValue parsed = Solstice::Core::JSONParser::Parse(dumped);
    SOLSTICE_TEST_ASSERT(parsed.HasKey("pi"), "Round-trip object key pi");
    SOLSTICE_TEST_ASSERT(parsed["name"].AsString() == "solstice", "Round-trip string field");

    std::vector<uint8_t> raw = {0, 1, 2, 255, 128, 64};
    const std::string b64 = Solstice::Core::Base64::Encode(raw.data(), raw.size());
    const std::vector<uint8_t> decoded = Solstice::Core::Base64::Decode(b64);
    SOLSTICE_TEST_ASSERT(decoded == raw, "Base64 round-trip");

    SOLSTICE_TEST_PASS("Asset pipeline checks");
    return true;
}

} // namespace

int main() {
    if (!RunAssetPipeline()) {
        return 1;
    }
    return SolsticeTestMainResult("AssetPipelineTest");
}
